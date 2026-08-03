#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"

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
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::syntax {
	namespace {
		using text_style = KSyntaxHighlighting::Theme::TextStyle;

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

		/// \brief Returns the record of one packaged definition.
		[[nodiscard]] auto record_of(const KSyntaxHighlighting::Definition& aDefinition)
			-> application::syntax_definition_version
		{
			const auto revision = aDefinition.version();
			return {
				.Name    = aDefinition.name().toStdString(),
				.Version = revision > 0 ? static_cast<std::uint32_t>(revision) : 0U,
			};
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

		/// \brief Collects semantic spans from one complete-blob pass.
		///
		/// \details The collector owns no theme. It reads the text style of each
		/// reported format and nothing else, so the spans it produces are the same
		/// spans under every appearance.
		class span_collector final : public KSyntaxHighlighting::AbstractHighlighter
		{
		public:
			explicit span_collector(const std::size_t aMaximumSpans)
				: MaximumSpans{ aMaximumSpans }
			{
			}

			void begin_line(
				const std::uint32_t               aLine,
				const std::vector<std::uint32_t>* aByteOffsets) noexcept
			{
				Line        = aLine;
				ByteOffsets = aByteOffsets;
				OpenSpan    = false;
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
				const auto role = role_of(aFormat.textStyle());
				if (role == core::appearance_role::syntax_normal) {
					// An unclassified region carries the default role, so naming
					// it would only make the span list larger.
					OpenSpan = false;
					return;
				}
				const auto first = byte_offset(aOffset);
				const auto last  = byte_offset(aOffset + aLength);
				if (last <= first) {
					return;
				}
				append(first, last, role);
			}

		private:
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
			std::size_t                       MaximumSpans = 0U;
			const std::vector<std::uint32_t>* ByteOffsets  = nullptr;
			std::uint32_t                     Line         = 0U;
			bool                              OpenSpan     = false;
			bool                              Overflowed   = false;
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
		const auto name = QString::fromUtf8(
			aName.data(),
			static_cast<qsizetype>(aName.size()));
		const auto definition = Implementation->Definitions.definitionForName(name);
		if (!definition.isValid()) {
			return std::nullopt;
		}
		return record_of(definition);
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
			return collect(aRequest, definition->Name, text, aStop, std::move(answer));
		}

		[[nodiscard]] auto collect(
			const application::syntax_classification_request& aRequest,
			const std::string&                                aDefinitionName,
			const QString&                                    aText,
			const std::stop_token&                            aStop,
			application::syntax_classification&&              aAnswer)
			-> application::syntax_classification
		{
			auto collector = span_collector{ aRequest.MaximumSpans };
			if (aDefinitionName != plain_text_definition_name) {
				collector.setDefinition(
					Catalog.Implementation->Definitions.definitionForName(
						QString::fromStdString(aDefinitionName)));
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
				collector.begin_line(line, &offsets);
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
