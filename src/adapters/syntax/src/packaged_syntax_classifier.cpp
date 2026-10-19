#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"

#include "bpr/core/cpp_identifier_context.hpp"
#include "cpp_template_arguments.hpp"

#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Theme>

#include <QByteArray>
#include <QString>
#include <QStringConverter>
#include <QStringView>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::syntax {
	namespace {
		using text_style = KSyntaxHighlighting::Theme::TextStyle;

		// A separate cache identity keeps resource keywords out of ordinary C.
		constexpr std::string_view ResourceDefinitionName = "Windows Resource";

		/// Lines between two cancellation checks. A blob is classified in one
		/// pass, so cancellation is observed at line granularity rather than
		/// after the whole file.
		constexpr std::uint32_t CancellationLineInterval = 512U;

		/// \brief Maps one packaged text style onto the core role vocabulary.
		///
		/// \details The switch is total and carries no default label, so adding a
		/// text style upstream is a compile error rather than a silent fallback.
		/// Six packaged styles have no distinct core role and map onto their
		/// nearest neighbour: `Extension` onto builtin; `VerbatimString` and
		/// `SpecialString` onto string; `BaseN` and `Float` onto number; and
		/// `CommentVar` onto annotation. The core vocabulary is closed and
		/// authoritative, so the collapse happens here rather than in the core.
		[[nodiscard]] constexpr auto role_of(const text_style aStyle) noexcept
			-> core::appearance_role
		{
			switch (aStyle) {
			case text_style::Normal:
				return core::appearance_role::syntax_normal;
			case text_style::Keyword:
				return core::appearance_role::syntax_keyword;
			case text_style::Function:
				return core::appearance_role::syntax_function;
			case text_style::Variable:
				return core::appearance_role::syntax_variable;
			case text_style::ControlFlow:
				return core::appearance_role::syntax_control_flow;
			case text_style::Operator:
				return core::appearance_role::syntax_operator;
			// `Extension` has no core role of its own; builtin is the nearest.
			case text_style::BuiltIn:
			case text_style::Extension:
				return core::appearance_role::syntax_builtin;
			case text_style::Preprocessor:
				return core::appearance_role::syntax_preprocessor;
			case text_style::Attribute:
				return core::appearance_role::syntax_attribute;
			case text_style::Char:
				return core::appearance_role::syntax_character;
			case text_style::SpecialChar:
				return core::appearance_role::syntax_escape_sequence;
			// The core names one string role, so the verbatim and special string
			// distinctions collapse into it.
			case text_style::String:
			case text_style::VerbatimString:
			case text_style::SpecialString:
				return core::appearance_role::syntax_string;
			case text_style::Import:
				return core::appearance_role::syntax_import;
			case text_style::DataType:
				return core::appearance_role::syntax_type;
			// The core names one number role, so the radix and float distinctions
			// collapse into it.
			case text_style::DecVal:
			case text_style::BaseN:
			case text_style::Float:
				return core::appearance_role::syntax_number;
			case text_style::Constant:
				return core::appearance_role::syntax_constant;
			case text_style::Comment:
				return core::appearance_role::syntax_comment;
			case text_style::Documentation:
				return core::appearance_role::syntax_documentation;
			// A comment variable is the name a documentation annotation binds, and
			// annotation is the nearest core role for it.
			case text_style::Annotation:
			case text_style::CommentVar:
				return core::appearance_role::syntax_annotation;
			case text_style::RegionMarker:
				return core::appearance_role::syntax_region_marker;
			case text_style::Information:
				return core::appearance_role::syntax_information;
			case text_style::Warning:
				return core::appearance_role::syntax_warning;
			case text_style::Alert:
				return core::appearance_role::syntax_alert;
			case text_style::Error:
				return core::appearance_role::syntax_error;
			case text_style::Others:
				return core::appearance_role::syntax_other;
			}
			return core::appearance_role::syntax_other;
		}

		/// \brief Returns the plain-text definition record.
		[[nodiscard]] auto plain_text_definition()
			-> application::syntax_definition_version
		{
			return {
				.Name    = std::string{ plain_text_definition_name },
				.Version = 0U,
			};
		}

		/// \brief Reports whether a definition receives the C-family post-pass.
		///
		/// \details The packaged C and C++ grammars declare user identifiers,
		/// PascalCase types and ALL_CAPS macros as normal text, so the function,
		/// type and preprocessor roles never reach a C++ file without help
		/// (BPR-0726). The same gate carries the three corrections BPR-0744
		/// added: an ALL_CAPS argument of a directive is a macro rather than more
		/// directive grey, the header name of an `#include` is a string rather
		/// than an import, and an ALL_CAPS predefined macro is a macro rather
		/// than an unclassified other. All three are conventions of these two
		/// grammars alone, so no other packaged definition sees them. The same
		/// gate carries the comment after a null directive, which both grammars
		/// paint as an error (BPR-0747). Windows Resource uses that C lexical
		/// behavior with its own normal-identifier refinement (BPR-0930).
		[[nodiscard]] auto uses_identifier_heuristics(const std::string_view aName) noexcept
			-> bool
		{
			return aName == "C++" || aName == "C" || aName == ResourceDefinitionName;
		}

		[[nodiscard]] auto grammar_name(const std::string_view aName) -> QString
		{
			return aName == ResourceDefinitionName
				? QStringLiteral("C")
				: QString::fromUtf8(aName.data(), static_cast<qsizetype>(aName.size()));
		}

		/// \brief Returns the record of one packaged definition.
		///
		/// \details A definition that receives the identifier post-pass carries
		/// the post-pass revision in the upper half of its version, so a span
		/// set cached under the previous heuristics is keyed away rather than
		/// served stale. The upstream grammar revision keeps the lower half.
		[[nodiscard]] auto record_of(const KSyntaxHighlighting::Definition& aDefinition)
			-> application::syntax_definition_version
		{
			const auto revision = aDefinition.version();
			auto       record   = application::syntax_definition_version{
				.Name    = aDefinition.name().toStdString(),
				.Version = revision > 0 ? static_cast<std::uint32_t>(revision) : 0U,
			};
			if (uses_identifier_heuristics(record.Name)) {
				record.Version += (record.Name == "C++" ? cpp_identifier_heuristic_revision : identifier_heuristic_revision) << 16U;
			}
			return record;
		}

		/// \brief Splits the decoded blob into its lines.
		///
		/// \details A trailing newline closes the last line rather than opening an
		/// empty one, which is the convention a blob's line count follows.
		[[nodiscard]] auto split_lines(const QString& aText) -> std::vector<QStringView>
		{
			auto       lines = std::vector<QStringView>{ };
			const auto view  = QStringView{ aText };
			auto       begin = qsizetype{ 0 };
			while (begin < view.size()) {
				const auto end = view.indexOf(u'\n', begin);
				if (end < 0) {
					lines.push_back(view.mid(begin));
					break;
				}
				lines.push_back(view.mid(begin, end - begin));
				begin = end + 1;
			}
			return lines;
		}

		/// \brief Returns the UTF-8 byte length of one UTF-16 code unit.
		///
		/// \details A high surrogate carries the whole four-byte sequence and its
		/// trailing low surrogate carries none, so a prefix sum over the units
		/// lands on exact byte boundaries.
		[[nodiscard]] constexpr auto encoded_length(const char16_t aUnit) noexcept
			-> std::uint32_t
		{
			if (aUnit < 0x80U) {
				return 1U;
			}
			if (aUnit < 0x800U) {
				return 2U;
			}
			if (aUnit >= 0xDC00U && aUnit <= 0xDFFFU) {
				return 0U;
			}
			if (aUnit >= 0xD800U && aUnit <= 0xDBFFU) {
				return 4U;
			}
			return 3U;
		}

		/// \brief Maps every UTF-16 offset of one line onto its byte offset.
		///
		/// \details The published span offsets are byte offsets because the host
		/// never decodes a blob, while the highlighter reports UTF-16 offsets. An
		/// all-ASCII line needs no map, which is the common case.
		[[nodiscard]] auto byte_offsets_of(const QStringView aLine)
			-> std::vector<std::uint32_t>
		{
			auto offsets = std::vector<std::uint32_t>{ };
			offsets.reserve(static_cast<std::size_t>(aLine.size()) + 1U);
			auto total = std::uint32_t{ 0U };
			for (const auto unit : aLine) {
				offsets.push_back(total);
				total += encoded_length(unit.unicode());
			}
			offsets.push_back(total);
			return offsets;
		}

		[[nodiscard]] constexpr auto is_identifier_start(const char16_t aUnit) noexcept
			-> bool
		{
			return (aUnit >= u'a' && aUnit <= u'z') || (aUnit >= u'A' && aUnit <= u'Z')
				|| aUnit == u'_';
		}

		[[nodiscard]] constexpr auto is_identifier_unit(const char16_t aUnit) noexcept
			-> bool
		{
			return is_identifier_start(aUnit) || (aUnit >= u'0' && aUnit <= u'9');
		}

		[[nodiscard]] constexpr auto is_upper(const char16_t aUnit) noexcept -> bool
		{
			return aUnit >= u'A' && aUnit <= u'Z';
		}

		[[nodiscard]] constexpr auto is_lower(const char16_t aUnit) noexcept -> bool
		{
			return aUnit >= u'a' && aUnit <= u'z';
		}

		/// \brief Reports whether the word is written ALL_CAPS in the macro
		/// convention: two or more units, only capitals, digits and underscores,
		/// and at least one capital.
		[[nodiscard]] auto is_macro_case(const QStringView aWord) noexcept -> bool
		{
			if (aWord.size() < 2) {
				return false;
			}
			auto capital = false;
			for (const auto unit : aWord) {
				const auto c = unit.unicode();
				if (is_upper(c)) {
					capital = true;
				} else if (!(c == u'_' || (c >= u'0' && c <= u'9'))) {
					return false;
				}
			}
			return capital;
		}

		/// \brief Reports whether the word is PascalCase: a leading capital and
		/// at least one lower-case letter after it.
		[[nodiscard]] auto is_pascal_case(const QStringView aWord) noexcept -> bool
		{
			if (aWord.isEmpty() || !is_upper(aWord.front().unicode())) {
				return false;
			}
			return std::any_of(aWord.begin(), aWord.end(), [](const QChar aUnit) {
				return is_lower(aUnit.unicode());
			});
		}

		/// \brief Reports whether the word is a string or character literal
		/// encoding prefix (`L`, `u8`, `u`, `U`, optionally with `R`).
		[[nodiscard]] auto is_literal_prefix(const QStringView aWord) noexcept -> bool
		{
			constexpr auto prefixes = std::array<std::u16string_view, 9U>{
				u"L",
				u"u8",
				u"u",
				u"U",
				u"R",
				u"LR",
				u"u8R",
				u"uR",
				u"UR",
			};
			return std::any_of(prefixes.begin(), prefixes.end(), [&](const auto aPrefix) {
				return aWord == QStringView{ aPrefix.data(), static_cast<qsizetype>(aPrefix.size()) };
			});
		}

		/// \brief Reports whether the word introduces a type name.
		[[nodiscard]] auto introduces_type(const QStringView aWord) noexcept -> bool
		{
			constexpr auto words = std::array<std::u16string_view, 6U>{
				u"class",
				u"struct",
				u"enum",
				u"union",
				u"typename",
				u"new",
			};
			return std::any_of(words.begin(), words.end(), [&](const auto aWordText) {
				return aWord
					== QStringView{ aWordText.data(), static_cast<qsizetype>(aWordText.size()) };
			});
		}

		/// \brief Returns the index of the first non-blank unit at or after the
		/// supplied position.
		[[nodiscard]] auto skip_blanks(const QStringView aLine, qsizetype aIndex) noexcept
			-> qsizetype
		{
			while (aIndex < aLine.size()
				   && (aLine[aIndex] == u' ' || aLine[aIndex] == u'\t')) {
				++aIndex;
			}
			return aIndex;
		}

		/// \brief Returns the word that ends just before the supplied position,
		/// skipping blanks; empty when none does.
		[[nodiscard]] auto word_before(const QStringView aLine, qsizetype aIndex) noexcept
			-> QStringView
		{
			while (aIndex > 0 && (aLine[aIndex - 1] == u' ' || aLine[aIndex - 1] == u'\t')) {
				--aIndex;
			}
			auto begin = aIndex;
			while (begin > 0 && is_identifier_unit(aLine[begin - 1].unicode())) {
				--begin;
			}
			return aLine.mid(begin, aIndex - begin);
		}

		/// \brief Chooses the role of one identifier from its line context.
		///
		/// \details These are the C and C++ conventions the packaged grammar does
		/// not express: a call or declaration is an identifier before `(`; a
		/// scope or a declared name is a type; an ALL_CAPS word is a macro, with its own role,
		/// even when invoked; a PascalCase word that introduces another word, a
		/// reference, a pointer or a template argument list is a type;
		/// and an encoding prefix belongs to the literal it opens. Anything else
		/// stays normal. BPR-0726.
		///
		/// C++ alone, which passes its line's template argument lists, also reads
		/// template context after the macro convention: a name whose argument
		/// list is followed by `(` is a function, and a template argument or a
		/// template name used as a type is a type. A single capital, the usual
		/// template parameter, is a type where a PascalCase word would be.
		/// BPR-1559.
		[[nodiscard]] auto identifier_role(
			const QStringView             aLine,
			const qsizetype               aBegin,
			const qsizetype               aEnd,
			const cpp_template_arguments* aCppTemplates) noexcept -> core::appearance_role
		{
			const auto word = aLine.mid(aBegin, aEnd - aBegin);
			if (aEnd < aLine.size() && is_literal_prefix(word)) {
				if (aLine[aEnd] == u'"') {
					return core::appearance_role::syntax_string;
				}
				if (aLine[aEnd] == u'\'') {
					return core::appearance_role::syntax_character;
				}
			}
			if (introduces_type(word_before(aLine, aBegin))) {
				return core::appearance_role::syntax_type;
			}
			// An ALL_CAPS word is a macro whether or not it is invoked, which is
			// how `VERIFY_ARE_EQUAL(...)` reads in the owner's file.
			if (is_macro_case(word)) {
				return core::appearance_role::syntax_macro;
			}
			if (aCppTemplates != nullptr) {
				if (const auto role = aCppTemplates->role_of(aBegin, aEnd)) {
					return *role;
				}
			}
			const auto next = skip_blanks(aLine, aEnd);
			const auto unit = next < aLine.size() ? aLine[next].unicode() : u'\0';
			if (unit == u'(') {
				return core::appearance_role::syntax_function;
			}
			if (unit == u':' && next + 1 < aLine.size() && aLine[next + 1] == u':') {
				return core::appearance_role::syntax_type;
			}
			const auto parameter = aCppTemplates != nullptr && word.size() == 1
				&& is_upper(word.front().unicode());
			if ((is_pascal_case(word) || parameter)
				&& (is_identifier_start(unit) || unit == u'&' || unit == u'*'
					|| unit == u'<')) {
				return core::appearance_role::syntax_type;
			}
			return core::appearance_role::syntax_normal;
		}

		/// \brief Distinguishes language type tokens from typedef-style DataType runs.
		[[nodiscard]] auto is_language_type(const QStringView aWord) noexcept -> bool
		{
			constexpr auto words = std::array{
				u"void", u"bool", u"char", u"char8_t", u"char16_t", u"char32_t", u"wchar_t",
				u"short", u"int", u"long", u"signed", u"unsigned", u"float", u"double",
				u"_Bool", u"_Complex", u"_Imaginary", u"_BitInt", u"_Decimal32", u"_Decimal64", u"_Decimal128"
			};
			return std::any_of(words.begin(), words.end(), [&](const auto aType) {
				return aWord == QStringView{ aType };
			});
		}

		/// \brief Refines normal resource tokens without C++ type-name inference.
		[[nodiscard]] auto resource_identifier_role(const QStringView aWord) noexcept
			-> core::appearance_role
		{
			constexpr auto words = std::array{
				u"LANGUAGE", u"ICON", u"MENU", u"MENUEX", u"BEGIN", u"END",
				u"POPUP", u"MENUITEM", u"SEPARATOR", u"ACCELERATORS", u"ASCII",
				u"VIRTKEY", u"ALT", u"CONTROL", u"SHIFT", u"NOINVERT",
				u"DIALOG", u"DIALOGEX", u"CAPTION", u"CLASS", u"STYLE", u"EXSTYLE",
				u"FONT", u"LTEXT", u"CTEXT", u"RTEXT", u"PUSHBUTTON", u"DEFPUSHBUTTON",
				u"EDITTEXT", u"GROUPBOX", u"CHECKBOX", u"AUTOCHECKBOX", u"RADIOBUTTON",
				u"AUTORADIOBUTTON", u"COMBOBOX", u"LISTBOX", u"SCROLLBAR",
				u"STRINGTABLE", u"VERSIONINFO", u"FILEVERSION", u"PRODUCTVERSION",
				u"FILEFLAGSMASK", u"FILEFLAGS", u"FILEOS", u"FILETYPE", u"FILESUBTYPE",
				u"BLOCK", u"VALUE", u"BITMAP", u"CURSOR", u"RCDATA", u"TEXTINCLUDE"
			};
			const auto keyword = std::any_of(words.begin(), words.end(), [&](const auto aKeyword) {
				return aWord == QStringView{ aKeyword };
			});
			if (keyword) {
				return core::appearance_role::syntax_keyword;
			}
			return is_macro_case(aWord) ? core::appearance_role::syntax_macro : core::appearance_role::syntax_normal;
		}

		/// \brief Collects semantic spans from one complete-blob pass.
		///
		/// \details The collector owns no theme. It reads the text style of each
		/// reported format and nothing else, so the spans it produces are the same
		/// spans under every appearance.
		class span_collector final : public KSyntaxHighlighting::AbstractHighlighter
		{
		public:
			span_collector(const std::size_t aMaximumSpans, const bool aIdentifierHeuristics,
						   const bool aResourceKeywords, const bool aCppTemplates)
				: MaximumSpans{ aMaximumSpans }
				, IdentifierHeuristics{ aIdentifierHeuristics }
				, ResourceKeywords{ aResourceKeywords }
				, CppTemplates{ aCppTemplates }
			{
			}

			void begin_line(
				const std::uint32_t               aLine,
				const QStringView                 aText,
				const std::vector<std::uint32_t>* aByteOffsets)
			{
				if (CppTemplates) {
					Templates.scan(aText);
				}
				const auto continued = ConditionalStart >= 0
					&& (Text.endsWith(u"\\") || Text.endsWith(u"\\\r"));
				Line             = aLine;
				Text             = aText;
				ByteOffsets      = aByteOffsets;
				OpenSpan         = false;
				ConditionalStart = continued ? 0 : -1;
				OperandEnd       = 0;
				if (!continued) {
					OperandPending = false;
					OperandDepth   = 0;
					OperandComment = false;
					OperandQuote   = u'\0';
				} else if (OperandPending || OperandDepth > 0) {
					scan_test_operand(0);
				}
			}

			[[nodiscard]] auto advance(
				const QStringView                 aText,
				const KSyntaxHighlighting::State& aState) -> KSyntaxHighlighting::State
			{
				return highlightLine(aText, aState);
			}

			[[nodiscard]] auto spans() noexcept -> application::token_span_list&
			{
				return Spans;
			}

			[[nodiscard]] auto overflowed() const noexcept -> bool
			{
				return Overflowed;
			}

		protected:
			void applyFormat(
				const int                          aOffset,
				const int                          aLength,
				const KSyntaxHighlighting::Format& aFormat) override
			{
				if (Overflowed || aOffset < 0 || aLength <= 0) {
					return;
				}
				if (IdentifierHeuristics && aFormat.textStyle() == text_style::Preprocessor) {
					begin_directive(aOffset, aOffset + aLength);
				}
				const auto role = c_family_role(
					role_of(aFormat.textStyle()),
					aOffset,
					aOffset + aLength,
					aFormat.name() == QStringLiteral("Standard Macros")
						|| aFormat.name() == QStringLiteral("GNU Macros"));
				if (role == core::appearance_role::syntax_normal) {
					// An unclassified region carries the default role, so naming
					// it would only make the span list larger. The identifier
					// post-pass reads such a region for the conventions the
					// grammar leaves normal. It enters no other styled region, so
					// comments and strings are never re-read; the directive pass
					// below is the one deliberate exception and reads only a
					// preprocessor run.
					OpenSpan = false;
					if (IdentifierHeuristics) {
						classify_identifiers(aOffset, aOffset + aLength);
					}
					OpenSpan = false;
					return;
				}
				if (IdentifierHeuristics
					&& role == core::appearance_role::syntax_preprocessor) {
					classify_directive(aOffset, aOffset + aLength);
					return;
				}
				append_units(aOffset, aOffset + aLength, role);
			}

		private:
			/// \brief Records a macro directive only when the grammar identifies its opening.
			void begin_directive(const int aBegin, const int aEnd) noexcept
			{
				auto at = skip_blanks(Text, 0);
				if (at < aBegin || at >= aEnd || Text[at] != u'#') {
					return;
				}
				at               = skip_blanks(Text, at + 1);
				const auto first = at;
				while (at < aEnd && is_identifier_unit(Text[at].unicode())) {
					++at;
				}
				const auto keyword = Text.mid(first, at - first);
				if (keyword == u"if" || keyword == u"ifdef" || keyword == u"ifndef"
					|| keyword == u"elif" || keyword == u"undef") {
					ConditionalStart = at;
				}
			}

			/// \brief Keeps language operators and boolean literals out of macro ranges.
			[[nodiscard]] static auto is_condition_operator(const QStringView aWord) noexcept -> bool
			{
				return aWord == u"defined" || aWord == u"true" || aWord == u"false"
					|| aWord == u"and" || aWord == u"or" || aWord == u"not"
					|| aWord == u"bitand" || aWord == u"bitor" || aWord == u"xor"
					|| aWord == u"compl" || aWord == u"not_eq"
					|| aWord == u"and_eq" || aWord == u"or_eq" || aWord == u"xor_eq";
			}

			/// \brief Names builtin tests whose arguments are not macro references.
			[[nodiscard]] static auto is_builtin_test(const QStringView aWord) noexcept -> bool
			{
				return aWord == u"__has_include" || aWord == u"__has_include_next"
					|| aWord == u"__has_cpp_attribute" || aWord == u"__has_c_attribute"
					|| aWord == u"__has_builtin" || aWord == u"__has_feature"
					|| aWord == u"__has_extension" || aWord == u"__has_attribute"
					|| aWord == u"__has_declspec_attribute" || aWord == u"__has_warning";
			}

			/// \brief Excludes a builtin operand, including trivia before its opening.
			void protect_test_operand(const qsizetype aEnd) noexcept
			{
				OperandPending = true;
				scan_test_operand(aEnd);
			}

			/// \brief Resumes operand scanning across escaped physical newlines.
			/// Comments are whitespace; parentheses inside comments and header names
			/// cannot end the operand's protected range.
			void scan_test_operand(qsizetype aAt) noexcept
			{
				while (aAt < Text.size()) {
					if (OperandComment) {
						const auto close = Text.indexOf(u"*/", aAt);
						if (close < 0) {
							break;
						}
						OperandComment = false;
						aAt            = close + 2;
						continue;
					}
					const auto unit = Text[aAt].unicode();
					if (OperandQuote != u'\0') {
						if (unit == OperandQuote) {
							OperandQuote = u'\0';
						} else if (unit == u'\\' && aAt + 1 < Text.size()) {
							++aAt;
						}
						++aAt;
						continue;
					}
					if (Text.mid(aAt, 2) == u"/*") {
						OperandComment = true;
						aAt += 2;
						continue;
					}
					if (Text.mid(aAt, 2) == u"//") {
						break;
					}
					if (unit == u' ' || unit == u'\t' || unit == u'\r'
						|| (unit == u'\\' && (aAt + 1 == Text.size() || Text.mid(aAt + 1) == u"\r"))) {
						++aAt;
						continue;
					}
					if (OperandPending) {
						OperandPending = false;
						if (unit != u'(') {
							OperandEnd = aAt;
							return;
						}
					}
					if (unit == u'(') {
						++OperandDepth;
					} else if (unit == u')') {
						--OperandDepth;
						if (OperandDepth == 0) {
							OperandEnd = aAt + 1;
							return;
						}
					} else if (unit == u'"' || unit == u'\'' || unit == u'<') {
						OperandQuote = unit == u'<' ? u'>' : unit;
					}
					++aAt;
				}
				OperandEnd = Text.size();
			}

			[[nodiscard]] auto is_conditional_macro(const qsizetype aBegin, const qsizetype aEnd) const noexcept -> bool
			{
				const auto word = Text.mid(aBegin, aEnd - aBegin);
				return ConditionalStart >= 0 && aBegin >= ConditionalStart && aBegin >= OperandEnd
					&& !word.isEmpty() && is_identifier_start(word.front().unicode())
					&& std::all_of(word.begin(), word.end(), [](const QChar aUnit) {
						   return is_identifier_unit(aUnit.unicode());
					   })
					&& !is_condition_operator(word) && !is_builtin_test(word);
			}

			/// \brief Reports whether the run opens the `//` comment of a null
			/// directive: nothing but blanks, one `#` and blanks precede it.
			///
			/// \details Both grammars hand the rest of a line to `LineError` when
			/// no directive keyword follows its `#`, so `#        // …`, which Qt
			/// writes to keep a comment aligned with the directives around it,
			/// painted as an error. Comments are removed before directives are
			/// read, so that line is a null directive and a comment. The other
			/// ways into `LineError` stay errors: an unknown directive,
			/// `#endif junk` and a stray `#` open their error run before any `//`,
			/// and `#define // …`, which lacks its macro name, has a keyword
			/// between the `#` and the `//`. BPR-0747.
			[[nodiscard]] auto opens_null_directive_comment(const qsizetype aBegin) const noexcept
				-> bool
			{
				const auto hash = skip_blanks(Text, 0);
				return hash < aBegin && Text[hash] == u'#'
					&& skip_blanks(Text, hash + 1) == aBegin && Text.mid(aBegin, 2) == u"//";
			}

			/// \brief Applies the C and C++ corrections the packaged grammar does
			/// not make to a whole styled run.
			///
			/// \details Four styles mean something narrower in C and C++ than the
			/// role vocabulary gives them, so the definitions that already run
			/// the post-pass correct them at the range the grammar reported.
			/// DataType includes both language built-ins and typedef-style names;
			/// only explicit language type tokens receive the built-in role.
			/// Inferred user types retain their type role (BPR-0930).
			///
			/// The import style is the `<…>` or `"…"` header name of an
			/// `#include`, which the reference paints as a string rather than the
			/// directive grey. No other packaged grammar is touched, and in those
			/// the import role keeps naming `import`, `from` and `@import`.
			///
			/// The other style is a predefined macro: the C++ grammar gives it to
			/// `__GNUC__` and `__FILE__`, and the GNU extension grammar it
			/// includes gives it to `__GNUC_MINOR__`. Which predefined names are
			/// listed upstream is an accident, so `__APPLE__` painting as a macro
			/// while `__GNUC__` painted near-grey was a distinction nobody chose.
			/// The correction is deliberately gated on the same ALL_CAPS test the
			/// directive pass uses rather than applied to the style as a whole,
			/// because the Doxygen and modeline grammars this definition also
			/// includes give that same style to an HTML entity, an HTML attribute
			/// name and a modeline value — all of them inside a comment. The
			/// ALL_CAPS test separates the two populations exactly: `__GNUC__`
			/// passes; `&amp;`, `on` and `off` do not, so no role is named inside
			/// a comment. BPR-0744. Conditional lowercase names additionally
			/// require the grammar's named macro format so comment substyles
			/// remain untouched (BPR-0748).
			///
			/// The third is the error style, which is a comment only where it
			/// opens the `//` of a null directive (BPR-0747).
			[[nodiscard]] auto c_family_role(
				const core::appearance_role aRole,
				const int                   aBegin,
				const int                   aEnd,
				const bool                  aPredefinedMacro) noexcept -> core::appearance_role
			{
				if (!IdentifierHeuristics) {
					return aRole;
				}
				if (aRole == core::appearance_role::syntax_type
					&& is_language_type(Text.mid(aBegin, aEnd - aBegin))) {
					return core::appearance_role::syntax_builtin;
				}
				if (ConditionalStart >= 0 && aBegin >= ConditionalStart && aBegin >= OperandEnd
					&& aPredefinedMacro
					&& is_builtin_test(Text.mid(aBegin, aEnd - aBegin))) {
					protect_test_operand(aEnd);
					return core::appearance_role::syntax_preprocessor;
				}
				if (aRole == core::appearance_role::syntax_import) {
					return core::appearance_role::syntax_string;
				}
				if (aRole == core::appearance_role::syntax_other
					&& aBegin >= OperandEnd
					&& (is_macro_case(Text.mid(aBegin, aEnd - aBegin))
						|| (aPredefinedMacro && is_conditional_macro(aBegin, aEnd)))) {
					return core::appearance_role::syntax_macro;
				}
				if (aRole == core::appearance_role::syntax_error
					&& opens_null_directive_comment(aBegin)) {
					return core::appearance_role::syntax_comment;
				}
				return aRole;
			}

			/// \brief Names the identifiers of one normal-styled run.
			void classify_identifiers(const int aBegin, const int aEnd)
			{
				const auto end = std::min(static_cast<qsizetype>(aEnd), Text.size());
				auto       at  = static_cast<qsizetype>(aBegin);
				while (at < end) {
					if (!is_identifier_start(Text[at].unicode())
						|| (at > 0 && is_identifier_unit(Text[at - 1].unicode()))) {
						++at;
						continue;
					}
					auto stop = at + 1;
					while (stop < Text.size() && is_identifier_unit(Text[stop].unicode())) {
						++stop;
					}
					if (stop <= end) {
						const auto role = ResourceKeywords
							? resource_identifier_role(Text.mid(at, stop - at))
							: identifier_role(Text, at, stop, CppTemplates ? &Templates : nullptr);
						if (role != core::appearance_role::syntax_normal) {
							OpenSpan = false;
							append_units(at, stop, role);
						}
					}
					at = stop;
				}
			}

			/// \brief Names the macros of one preprocessor run and leaves every
			/// other part of it the directive grey.
			///
			/// \details The directive keyword is grey and its arguments do not
			/// inherit that grey: an ALL_CAPS word anywhere in the run is a macro
			/// at its own exact range, which is how `#ifndef MACRO`,
			/// `#define MACRO` and `#if MACRO()` read in the reference. Everything
			/// the run still covers — the keyword itself, `once` after `#pragma`,
			/// `defined`, the operators and the parentheses — stays grey, and so
			/// does a quoted region, so the words of an `#error` message are not
			/// mistaken for macros. A part the grammar already styled for itself
			/// (the `()` after a macro name, a numeric replacement, a standard
			/// macro, a trailing comment, an `#include` header name) is a run of
			/// its own and never reaches here. BPR-0744. Conditional directives
			/// also name lowercase identifiers as macros, except language operators
			/// and the operands of builtin tests (BPR-0748).
			void classify_directive(const int aBegin, const int aEnd)
			{
				const auto end  = std::min(static_cast<qsizetype>(aEnd), Text.size());
				auto       at   = static_cast<qsizetype>(aBegin);
				auto       grey = at;
				while (at < end) {
					if (Text[at] == u'"' || Text[at] == u'\'') {
						at = end_of_quotation(at, end);
						continue;
					}
					if (!is_identifier_start(Text[at].unicode())
						|| (at > 0 && is_identifier_unit(Text[at - 1].unicode()))) {
						++at;
						continue;
					}
					auto stop = at + 1;
					while (stop < Text.size() && is_identifier_unit(Text[stop].unicode())) {
						++stop;
					}
					if (ConditionalStart >= 0 && stop < end
						&& is_literal_prefix(Text.mid(at, stop - at))
						&& (Text[stop] == u'"' || Text[stop] == u'\'')) {
						at = end_of_quotation(stop, end);
						continue;
					}
					if (ConditionalStart >= 0 && at >= ConditionalStart && at >= OperandEnd
						&& is_builtin_test(Text.mid(at, stop - at))) {
						protect_test_operand(stop);
					}
					if (stop <= end && at >= OperandEnd
						&& (is_macro_case(Text.mid(at, stop - at)) || is_conditional_macro(at, stop))) {
						append_units(grey, at, core::appearance_role::syntax_preprocessor);
						append_units(at, stop, core::appearance_role::syntax_macro);
						grey = stop;
					}
					at = stop;
				}
				append_units(grey, end, core::appearance_role::syntax_preprocessor);
			}

			/// \brief Returns the index just past the quotation that opens at the
			/// supplied position, or the end of the run when none closes it.
			[[nodiscard]] auto end_of_quotation(
				const qsizetype aOpen,
				const qsizetype aEnd) const noexcept -> qsizetype
			{
				auto at = aOpen + 1;
				while (at < aEnd && Text[at] != Text[aOpen]) {
					at += Text[at] == u'\\' ? 2 : 1;
				}
				return std::min(at + 1, aEnd);
			}

			/// \brief Appends one span named by its UTF-16 unit range.
			void append_units(
				const qsizetype             aBegin,
				const qsizetype             aEnd,
				const core::appearance_role aRole)
			{
				const auto first = byte_offset(static_cast<int>(aBegin));
				const auto last  = byte_offset(static_cast<int>(aEnd));
				if (last <= first) {
					return;
				}
				append(first, last, aRole);
			}

			[[nodiscard]] auto byte_offset(const int aUnitOffset) const noexcept
				-> std::uint32_t
			{
				if (ByteOffsets == nullptr || ByteOffsets->empty()) {
					return 0U;
				}
				const auto index = std::min(
					static_cast<std::size_t>(aUnitOffset),
					ByteOffsets->size() - 1U);
				return (*ByteOffsets)[index];
			}

			void append(
				const std::uint32_t         aFirst,
				const std::uint32_t         aLast,
				const core::appearance_role aRole)
			{
				if (OpenSpan && !Spans.empty()) {
					auto&      previous = Spans.back();
					const auto end      = previous.Column + previous.Length;
					if (previous.Line == Line && previous.Role == aRole
						&& end == aFirst) {
						previous.Length = aLast - previous.Column;
						return;
					}
				}
				if (Spans.size() >= MaximumSpans) {
					Overflowed = true;
					return;
				}
				Spans.push_back(application::token_span{
					.Line   = Line,
					.Column = aFirst,
					.Length = aLast - aFirst,
					.Role   = aRole,
				});
				OpenSpan = true;
			}

			application::token_span_list      Spans;
			std::size_t                       MaximumSpans         = 0U;
			bool                              IdentifierHeuristics = false;
			bool                              ResourceKeywords     = false;
			bool                              CppTemplates         = false;
			cpp_template_arguments            Templates;
			QStringView                       Text;
			const std::vector<std::uint32_t>* ByteOffsets      = nullptr;
			std::uint32_t                     Line             = 0U;
			bool                              OpenSpan         = false;
			bool                              Overflowed       = false;
			qsizetype                         ConditionalStart = -1;
			qsizetype                         OperandEnd       = 0;
			bool                              OperandPending   = false;
			int                               OperandDepth     = 0;
			bool                              OperandComment   = false;
			char16_t                          OperandQuote     = u'\0';
		};

		/// \brief Returns the spans that fall inside the supplied line range.
		[[nodiscard]] auto spans_within(
			const application::token_span_list& aSpans,
			const blob_line_range               aLines) -> application::token_span_list
		{
			auto       selected = application::token_span_list{ };
			const auto last     = aLines.First + aLines.Count;
			for (const auto& span : aSpans) {
				if (span.Line >= aLines.First && span.Line < last) {
					selected.push_back(span);
				}
			}
			return selected;
		}
	} // namespace

	struct packaged_definition_catalog::implementation
	{
		KSyntaxHighlighting::Repository Definitions;
		std::size_t                     Packaged   = 0U;
		std::size_t                     Filesystem = 0U;
	};

	packaged_definition_catalog::packaged_definition_catalog()
		: Implementation{ std::make_unique<implementation>() }
	{
		// A packaged definition lives in the library's compiled-in resource. A
		// definition resolved anywhere else came from the host filesystem, which
		// this build must never consult. The repository always carries one
		// invalid placeholder standing for "no highlighting"; it names no file
		// and is not a loaded definition.
		const auto definitions = Implementation->Definitions.definitions();
		for (const auto& definition : definitions) {
			if (!definition.isValid()) {
				continue;
			}
			if (definition.filePath().startsWith(QStringLiteral(":/"))) {
				++Implementation->Packaged;
			} else {
				++Implementation->Filesystem;
			}
		}
	}

	packaged_definition_catalog::~packaged_definition_catalog() = default;

	auto packaged_definition_catalog::definition_for_path(
		const std::string_view aPath) const -> application::syntax_definition_version
	{
		const auto path = QString::fromUtf8(
			aPath.data(),
			static_cast<qsizetype>(aPath.size()));
		if (path.endsWith(QStringLiteral(".rc"), Qt::CaseInsensitive)) {
			// Upstream TOML claims *.rc. Resource scripts instead use C lexical
			// state plus a bounded keyword refinement under their own identity.
			return definition_named(ResourceDefinitionName).value_or(plain_text_definition());
		}
		const auto definition = Implementation->Definitions.definitionForFileName(path);
		if (!definition.isValid()) {
			return plain_text_definition();
		}
		return record_of(definition);
	}

	auto packaged_definition_catalog::definition_named(
		const std::string_view aName) const
		-> std::optional<application::syntax_definition_version>
	{
		if (aName == plain_text_definition_name) {
			return plain_text_definition();
		}
		const auto name       = grammar_name(aName);
		const auto definition = Implementation->Definitions.definitionForName(name);
		if (!definition.isValid()) {
			return std::nullopt;
		}
		auto record = record_of(definition);
		if (aName == ResourceDefinitionName) {
			record.Name = ResourceDefinitionName;
		}
		return record;
	}

	auto packaged_definition_catalog::packaged_definitions() const noexcept
		-> std::size_t
	{
		return Implementation->Packaged;
	}

	auto packaged_definition_catalog::filesystem_definitions() const noexcept
		-> std::size_t
	{
		return Implementation->Filesystem;
	}

	auto packaged_definition_catalog::custom_search_paths() const noexcept
		-> std::size_t
	{
		return static_cast<std::size_t>(
			Implementation->Definitions.customSearchPaths().size());
	}

	struct packaged_syntax_classifier::implementation
	{
		const packaged_definition_catalog& Catalog;
		blob_source&                       Blobs;
		std::size_t                        LexicalPasses = 0U;

		/// \brief Runs one complete-blob pass.
		///
		/// \details Every line is highlighted in order and the state each line
		/// ends in opens the next, so a construct that spans lines keeps its
		/// classification for the whole of its extent.
		[[nodiscard]] auto classify_blob(
			const application::syntax_classification_request& aRequest,
			const std::stop_token&                            aStop)
			-> application::syntax_classification
		{
			auto answer = application::syntax_classification{
				.Context = aRequest.Context,
				.Key     = aRequest.Key,
				.Status  = application::syntax_classification_status::unavailable,
				.Spans   = { },
			};
			if (!valid(aRequest.Key)) {
				return answer;
			}
			const auto definition = Catalog.definition_named(aRequest.Key.Definition.Name);
			if (!definition.has_value() || *definition != aRequest.Key.Definition) {
				// A key naming an unknown definition, or a revision this build no
				// longer packages, must fail rather than be answered under a label
				// the spans do not match.
				return answer;
			}
			auto bytes = Blobs.blob_bytes(aRequest.Key.Blob);
			if (!bytes.has_value()) {
				return answer;
			}
			if (bytes->size() > maximum_classified_blob_bytes) {
				answer.Status = application::syntax_classification_status::limit_exceeded;
				return answer;
			}
			auto decoder = QStringDecoder{ QStringDecoder::Utf8 };
			auto text    = QString{ decoder(QByteArrayView{
				bytes->data(),
				static_cast<qsizetype>(bytes->size()) }) };
			if (decoder.hasError()) {
				// A blob that is not valid UTF-8 has no line or column the host
				// could address, so it is unavailable rather than plain text.
				return answer;
			}
			++LexicalPasses;
			auto result = collect(aRequest, definition->Name, text, aStop, std::move(answer));
			if (definition->Name == "C++" && result.Status == application::syntax_classification_status::available) {
				result.Status = core::refine_cpp_identifiers(*bytes, result.Spans, aRequest.MaximumSpans, aStop);
			}
			return result;
		}

		[[nodiscard]] auto collect(
			const application::syntax_classification_request& aRequest,
			const std::string&                                aDefinitionName,
			const QString&                                    aText,
			const std::stop_token&                            aStop,
			application::syntax_classification&&              aAnswer)
			-> application::syntax_classification
		{
			auto collector = span_collector{
				aRequest.MaximumSpans,
				uses_identifier_heuristics(aDefinitionName),
				aDefinitionName == ResourceDefinitionName,
				aDefinitionName == "C++",
			};
			if (aDefinitionName != plain_text_definition_name) {
				collector.setDefinition(
					Catalog.Implementation->Definitions.definitionForName(
						grammar_name(aDefinitionName)));
			}
			const auto lines = split_lines(aText);
			auto       state = KSyntaxHighlighting::State{ };
			auto       line  = std::uint32_t{ 0U };
			for (const auto& text : lines) {
				if (line % CancellationLineInterval == 0U && aStop.stop_requested()) {
					aAnswer.Status = application::syntax_classification_status::cancelled;
					return aAnswer;
				}
				const auto offsets = byte_offsets_of(text);
				collector.begin_line(line, text, &offsets);
				state = collector.advance(text, state);
				if (collector.overflowed()) {
					aAnswer.Status
						= application::syntax_classification_status::limit_exceeded;
					return aAnswer;
				}
				++line;
			}
			aAnswer.Status = application::syntax_classification_status::available;
			aAnswer.Spans  = std::move(collector.spans());
			return aAnswer;
		}
	};

	packaged_syntax_classifier::packaged_syntax_classifier(
		const packaged_definition_catalog& aCatalog,
		blob_source&                       aBlobs)
		: Implementation{ std::make_unique<implementation>(aCatalog, aBlobs) }
	{
	}

	packaged_syntax_classifier::~packaged_syntax_classifier() = default;

	auto packaged_syntax_classifier::classify(
		const application::syntax_classification_request& aRequest,
		const std::stop_token                             aStop)
		-> application::syntax_classification
	{
		return Implementation->classify_blob(aRequest, aStop);
	}

	auto packaged_syntax_classifier::classify_hunk(
		const application::syntax_classification_request& aRequest,
		const blob_line_range                             aLines,
		const std::stop_token                             aStop)
		-> application::syntax_classification
	{
		// The hunk is a projection of the complete-blob pass. Highlighting the
		// range on its own would lose the state that opened a multiline comment
		// or string on an earlier line.
		auto classification = Implementation->classify_blob(aRequest, aStop);
		if (classification.Status
			!= application::syntax_classification_status::available) {
			return classification;
		}
		classification.Spans = spans_within(classification.Spans, aLines);
		return classification;
	}

	auto packaged_syntax_classifier::lexical_passes() const noexcept -> std::size_t
	{
		return Implementation->LexicalPasses;
	}
} // namespace bpr::adapters::syntax
