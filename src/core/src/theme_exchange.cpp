#include "bpr/core/theme_exchange.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::core {
	namespace {
		/// \brief Separates a reference's scheme from its target.
		constexpr auto scheme_separator = std::string_view{ "://" };

		/// \brief Names the directives that pull in another document.
		///
		/// \details Matching is on a whole token, which is why a role such as
		/// `syntax_import` is untouched: the token is `syntax_import`, not
		/// `import`. Without that rule the closed vocabulary would collide with
		/// its own guard and no theme could be imported at all.
		constexpr auto include_words = std::array<std::string_view, 8>{
			"extends",
			"include",
			"includes",
			"import",
			"imports",
			"inherits",
			"require",
			"requires",
		};

		/// \brief Names the tokens that only executable content uses.
		constexpr auto executable_words = std::array<std::string_view, 7>{
			"eval",
			"exec",
			"expression",
			"function",
			"onerror",
			"onload",
			"script",
		};

		/// \brief Names the tokens that only style-sheet content uses.
		constexpr auto style_words = std::array<std::string_view, 7>{
			"css",
			"qproperty",
			"qss",
			"setstylesheet",
			"style",
			"stylesheet",
			"styles",
		};

		/// \brief Names the schemes that address content outside the document.
		constexpr auto reference_schemes = std::array<std::string_view, 13>{
			"about",
			"blob",
			"data",
			"file",
			"ftp",
			"ftps",
			"http",
			"https",
			"javascript",
			"mailto",
			"qrc",
			"res",
			"vbscript",
		};

		[[nodiscard]] constexpr auto ascii_lower(const char aCharacter) noexcept -> char
		{
			return (aCharacter >= 'A' && aCharacter <= 'Z')
				? static_cast<char>(aCharacter + ('a' - 'A'))
				: aCharacter;
		}

		[[nodiscard]] auto lowered(const std::string_view aText) -> std::string
		{
			auto text = std::string{ aText };
			std::ranges::transform(text, text.begin(), ascii_lower);
			return text;
		}

		/// \brief Reports whether the character continues a word-shaped token.
		[[nodiscard]] constexpr auto token_character(const char aCharacter) noexcept -> bool
		{
			return (aCharacter >= '0' && aCharacter <= '9')
				|| (aCharacter >= 'a' && aCharacter <= 'z')
				|| (aCharacter >= 'A' && aCharacter <= 'Z')
				|| aCharacter == '_';
		}

		[[nodiscard]] constexpr auto printable(const char aCharacter) noexcept -> bool
		{
			const auto value = static_cast<unsigned char>(aCharacter);
			return value >= 0x20U && value < 0x7FU;
		}

		[[nodiscard]] constexpr auto layout_character(const char aCharacter) noexcept -> bool
		{
			return aCharacter == '\n' || aCharacter == '\r' || aCharacter == '\t';
		}

		[[nodiscard]] constexpr auto blank_character(const char aCharacter) noexcept -> bool
		{
			return aCharacter == ' ' || aCharacter == '\t' || aCharacter == '\r';
		}

		[[nodiscard]] constexpr auto role_at(const std::size_t aIndex) noexcept -> appearance_role
		{
			return static_cast<appearance_role>(static_cast<std::uint8_t>(aIndex));
		}

		[[nodiscard]] constexpr auto hex_nibble(const char aCharacter) noexcept
			-> std::optional<std::uint8_t>
		{
			if (aCharacter >= '0' && aCharacter <= '9') {
				return static_cast<std::uint8_t>(aCharacter - '0');
			}
			if (aCharacter >= 'a' && aCharacter <= 'f') {
				return static_cast<std::uint8_t>(aCharacter - 'a' + 10);
			}
			if (aCharacter >= 'A' && aCharacter <= 'F') {
				return static_cast<std::uint8_t>(aCharacter - 'A' + 10);
			}
			return std::nullopt;
		}

		/// \brief Quotes untrusted text safely inside a rejection.
		///
		/// \details The text stops at the end of its line, is bounded by
		/// `theme_import_subject_limit`, and keeps only printable ASCII. A
		/// diagnostic quoting an attacker's bytes verbatim would move the payload
		/// rather than refuse it.
		[[nodiscard]] auto subject_of(const std::string_view aText) -> std::string
		{
			auto subject = std::string{ };
			for (const auto character : aText.substr(0U, std::min(aText.size(), theme_import_subject_limit))) {
				if (character == '\n' || character == '\r') {
					break;
				}
				subject.push_back(printable(character) ? character : '?');
			}
			return subject;
		}

		[[nodiscard]] auto rejection_of(
			const theme_import_reason aReason,
			const std::string_view    aSubject,
			const std::size_t         aLine) -> theme_import_rejection
		{
			return theme_import_rejection{
				.Reason  = aReason,
				.Subject = subject_of(aSubject),
				.Line    = aLine,
			};
		}

		[[nodiscard]] auto refusal(theme_import_rejection aRejection) -> theme_import_result
		{
			auto rejections = std::vector<theme_import_rejection>{ };
			rejections.push_back(std::move(aRejection));
			return theme_import_result{ .Palette = std::nullopt, .Rejections = std::move(rejections) };
		}

		/// \brief Collects the first occurrence of each class of foreign content.
		///
		/// \details One rejection per class keeps the report bounded no matter
		/// what the document contains, while still telling a caller that a
		/// document carried both a reference and a payload rather than only the
		/// earlier one.
		struct hazard_collector
		{
			std::vector<theme_import_rejection> Rejections;

			[[nodiscard]] auto recorded(const theme_import_reason aReason) const -> bool
			{
				return std::ranges::any_of(
					Rejections,
					[aReason](const theme_import_rejection& aRejection) {
						return aRejection.Reason == aReason;
					});
			}

			auto record(
				const theme_import_reason aReason,
				const std::string_view    aSubject,
				const std::size_t         aLine) -> void
			{
				if (!recorded(aReason)) {
					Rejections.push_back(rejection_of(aReason, aSubject, aLine));
				}
			}
		};

		[[nodiscard]] auto word_hazard(const std::string_view aLoweredToken) noexcept
			-> std::optional<theme_import_reason>
		{
			if (std::ranges::find(include_words, aLoweredToken) != include_words.end()) {
				return theme_import_reason::include_directive;
			}
			if (std::ranges::find(executable_words, aLoweredToken) != executable_words.end()) {
				return theme_import_reason::executable_payload;
			}
			if (std::ranges::find(style_words, aLoweredToken) != style_words.end()) {
				return theme_import_reason::style_payload;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto token_end(
			const std::string_view aDocument,
			const std::size_t      aBegin) noexcept -> std::size_t
		{
			auto end = aBegin;
			while (end < aDocument.size() && token_character(aDocument[end])) {
				++end;
			}
			return end;
		}

		[[nodiscard]] auto reference_begin(
			const std::string_view aDocument,
			const std::size_t      aIndex) noexcept -> std::size_t
		{
			auto begin = aIndex;
			while (begin > 0U && token_character(aDocument[begin - 1U])) {
				--begin;
			}
			return begin;
		}

		[[nodiscard]] auto opens_markup(
			const std::string_view aDocument,
			const std::size_t      aIndex) noexcept -> bool
		{
			if (aIndex + 1U >= aDocument.size()) {
				return false;
			}
			const auto next = ascii_lower(aDocument[aIndex + 1U]);
			return next == '/' || next == '!' || next == '?' || (next >= 'a' && next <= 'z');
		}

		auto classify_token(
			hazard_collector&      aCollector,
			const std::string_view aDocument,
			const std::size_t      aBegin,
			const std::size_t      aEnd,
			const std::size_t      aLine) -> void
		{
			const auto token = aDocument.substr(aBegin, aEnd - aBegin);
			const auto lower = lowered(token);
			if (const auto hazard = word_hazard(lower); hazard.has_value()) {
				aCollector.record(*hazard, token, aLine);
				return;
			}

			const auto scheme = aEnd < aDocument.size() && aDocument[aEnd] == ':'
				&& std::ranges::find(reference_schemes, std::string_view{ lower })
					!= reference_schemes.end();
			if (scheme) {
				aCollector.record(
					theme_import_reason::external_reference, aDocument.substr(aBegin), aLine);
			}
		}

		auto classify_symbol(
			hazard_collector&      aCollector,
			const std::string_view aDocument,
			const std::size_t      aIndex,
			const std::size_t      aLine) -> void
		{
			const auto character = aDocument[aIndex];
			if (aDocument.compare(aIndex, scheme_separator.size(), scheme_separator) == 0) {
				aCollector.record(
					theme_import_reason::external_reference,
					aDocument.substr(reference_begin(aDocument, aIndex)),
					aLine);
				return;
			}
			if (character == '{' || character == '}') {
				// The format has no block structure, so a brace can only be
				// syntax borrowed from a style sheet.
				aCollector.record(
					theme_import_reason::style_payload, aDocument.substr(aIndex), aLine);
				return;
			}
			if (character == '<' && opens_markup(aDocument, aIndex)) {
				aCollector.record(
					theme_import_reason::executable_payload, aDocument.substr(aIndex), aLine);
				return;
			}
			if (character == '#' && aIndex + 1U < aDocument.size() && aDocument[aIndex + 1U] == '!') {
				aCollector.record(
					theme_import_reason::executable_payload, aDocument.substr(aIndex), aLine);
				return;
			}
			if (!printable(character) && !layout_character(character)) {
				// The alphabet is closed, so a byte outside it is not part of a
				// theme. Refusing it here also keeps a truncating byte out of
				// every identifier a host later handles.
				aCollector.record(
					theme_import_reason::malformed_document, std::string_view{ }, aLine);
			}
		}

		/// \brief Scans the whole document for content no theme document carries.
		///
		/// \details The scan runs over the raw text rather than over parsed
		/// values, which is what makes the refusals hold "wherever it appears":
		/// a reference hidden in a key or in trailing text is refused exactly
		/// like one hidden in a value.
		[[nodiscard]] auto scan_for_foreign_content(const std::string_view aDocument)
			-> std::vector<theme_import_rejection>
		{
			auto collector = hazard_collector{ };
			auto line      = std::size_t{ 1U };
			auto index     = std::size_t{ 0U };
			while (index < aDocument.size()) {
				if (token_character(aDocument[index])) {
					const auto end = token_end(aDocument, index);
					classify_token(collector, aDocument, index, end, line);
					index = end;
					continue;
				}
				if (aDocument[index] == '\n') {
					++line;
				} else {
					classify_symbol(collector, aDocument, index, line);
				}
				++index;
			}
			return std::move(collector.Rejections);
		}

		[[nodiscard]] constexpr auto trimmed(std::string_view aText) noexcept -> std::string_view
		{
			while (!aText.empty() && blank_character(aText.front())) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && blank_character(aText.back())) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		/// \brief Describes one trimmed non-empty line and where it came from.
		struct document_line
		{
			std::string_view Text;
			std::size_t      Number = 0U;
		};

		[[nodiscard]] auto significant_lines(const std::string_view aDocument)
			-> std::vector<document_line>
		{
			auto lines  = std::vector<document_line>{ };
			auto number = std::size_t{ 1U };
			auto begin  = std::size_t{ 0U };
			while (true) {
				const auto end  = std::min(aDocument.find('\n', begin), aDocument.size());
				const auto text = trimmed(aDocument.substr(begin, end - begin));
				if (!text.empty()) {
					lines.push_back(document_line{ .Text = text, .Number = number });
				}
				if (end == aDocument.size()) {
					return lines;
				}
				begin = end + 1U;
				++number;
			}
		}

		[[nodiscard]] auto line_tokens(const std::string_view aLine)
			-> std::vector<std::string_view>
		{
			auto tokens = std::vector<std::string_view>{ };
			auto index  = std::size_t{ 0U };
			while (index < aLine.size()) {
				while (index < aLine.size() && blank_character(aLine[index])) {
					++index;
				}
				const auto begin = index;
				while (index < aLine.size() && !blank_character(aLine[index])) {
					++index;
				}
				if (index > begin) {
					tokens.push_back(aLine.substr(begin, index - begin));
				}
			}
			return tokens;
		}

		[[nodiscard]] auto parse_version(const std::string_view aText) noexcept
			-> std::optional<std::uint32_t>
		{
			// Nine digits cannot overflow the accumulator, and no honest version
			// needs more.
			if (aText.empty() || aText.size() > 9U) {
				return std::nullopt;
			}

			auto value = std::uint32_t{ 0U };
			for (const auto character : aText) {
				if (character < '0' || character > '9') {
					return std::nullopt;
				}
				value = value * 10U + static_cast<std::uint32_t>(character - '0');
			}
			return value;
		}

		[[nodiscard]] auto parse_color(const std::string_view aText) noexcept
			-> std::optional<appearance_color>
		{
			if ((aText.size() != 7U && aText.size() != 9U) || aText.front() != '#') {
				return std::nullopt;
			}

			const auto digits = aText.substr(1U);
			// A six-digit value is opaque. That is a stated convention rather
			// than a guessed default, and it is the only omission the format
			// allows.
			auto channels = std::array<std::uint8_t, 4U>{ 0U, 0U, 0U, 255U };
			for (std::size_t index = 0U; index * 2U < digits.size(); ++index) {
				const auto high = hex_nibble(digits[index * 2U]);
				const auto low  = hex_nibble(digits[(index * 2U) + 1U]);
				if (!high.has_value() || !low.has_value()) {
					return std::nullopt;
				}
				channels[index] = static_cast<std::uint8_t>(
					static_cast<std::uint8_t>(*high << 4U) | *low);
			}

			return appearance_color{
				.Red   = channels[0U],
				.Green = channels[1U],
				.Blue  = channels[2U],
				.Alpha = channels[3U],
			};
		}

		[[nodiscard]] auto check_header(const document_line& aLine)
			-> std::optional<theme_import_rejection>
		{
			const auto tokens = line_tokens(aLine.Text);
			if (tokens.size() != 2U || tokens.front() != theme_document_signature) {
				return rejection_of(
					theme_import_reason::malformed_document, aLine.Text, aLine.Number);
			}

			const auto version = parse_version(tokens[1U]);
			if (!version.has_value()) {
				return rejection_of(
					theme_import_reason::malformed_document, aLine.Text, aLine.Number);
			}
			if (*version != theme_document_version) {
				return rejection_of(
					theme_import_reason::unsupported_version, tokens[1U], aLine.Number);
			}
			return std::nullopt;
		}

		/// \brief Accumulates the palette a document assigns and why it failed.
		struct body_state
		{
			appearance_palette                      Palette{ };
			std::array<bool, appearance_role_count> Assigned{ };
			std::vector<theme_import_rejection>     Rejections;
		};

		auto read_entry(body_state& aState, const document_line& aLine) -> void
		{
			const auto tokens = line_tokens(aLine.Text);
			if (tokens.size() != 2U) {
				aState.Rejections.push_back(rejection_of(
					theme_import_reason::malformed_document, aLine.Text, aLine.Number));
				return;
			}

			const auto role = find_appearance_role(tokens.front());
			if (!role.has_value()) {
				aState.Rejections.push_back(rejection_of(
					theme_import_reason::unknown_field, tokens.front(), aLine.Number));
				return;
			}
			if (aState.Assigned[index_of(*role)]) {
				// Last-one-wins is how a document hides the value it really
				// sets, so a repeat is a refusal rather than a precedence rule.
				aState.Rejections.push_back(rejection_of(
					theme_import_reason::duplicate_field, tokens.front(), aLine.Number));
				return;
			}

			const auto color = parse_color(tokens[1U]);
			if (!color.has_value()) {
				aState.Rejections.push_back(rejection_of(
					theme_import_reason::malformed_color, tokens[1U], aLine.Number));
				return;
			}

			aState.Assigned[index_of(*role)] = true;
			aState.Palette[index_of(*role)]  = *color;
		}

		auto append_missing_roles(body_state& aState) -> void
		{
			for (std::size_t index = 0U; index < appearance_role_count; ++index) {
				if (aState.Assigned[index]) {
					continue;
				}
				// [[BPR-1083]]. A label tint the document predates reads as the
				// built-in tint, which presentation owns; nothing else changes.
				if (defaults_on_import(role_at(index))) {
					aState.Palette[index] = unassigned_appearance_color;
					continue;
				}
				aState.Rejections.push_back(rejection_of(
					theme_import_reason::missing_field, stable_name(role_at(index)), 0U));
			}
		}

		auto append_decimal(std::string& aText, const std::uint32_t aValue) -> void
		{
			auto digits    = std::string{ };
			auto remaining = aValue;
			do {
				digits.push_back(static_cast<char>('0' + static_cast<int>(remaining % 10U)));
				remaining /= 10U;
			} while (remaining != 0U);
			aText.append(digits.rbegin(), digits.rend());
		}

		auto append_color(std::string& aText, const appearance_color& aColor) -> void
		{
			constexpr auto digits   = std::string_view{ "0123456789abcdef" };
			const auto     channels = std::array<std::uint8_t, 4U>{
				aColor.Red,
				aColor.Green,
				aColor.Blue,
				aColor.Alpha,
			};

			aText.push_back('#');
			for (const auto channel : channels) {
				aText.push_back(digits[static_cast<std::size_t>(channel >> 4U)]);
				aText.push_back(digits[static_cast<std::size_t>(channel & 0x0FU)]);
			}
		}
	} // namespace

	auto export_theme(const appearance_palette& aPalette) -> std::string
	{
		auto document = std::string{ theme_document_signature };
		document.push_back(' ');
		append_decimal(document, theme_document_version);
		document.push_back('\n');

		for (std::size_t index = 0U; index < appearance_role_count; ++index) {
			document.append(stable_name(role_at(index)));
			document.push_back(' ');
			append_color(document, aPalette[index]);
			document.push_back('\n');
		}
		return document;
	}

	auto import_theme(const std::string_view aDocument) -> theme_import_result
	{
		if (aDocument.size() > theme_document_byte_limit) {
			// Measured, not discovered: nothing else is examined, so an oversize
			// document costs one comparison however hostile its contents are.
			return refusal(rejection_of(
				theme_import_reason::document_too_large, std::string_view{ }, 0U));
		}

		auto foreign = scan_for_foreign_content(aDocument);
		if (!foreign.empty()) {
			return theme_import_result{ .Palette = std::nullopt, .Rejections = std::move(foreign) };
		}

		const auto lines = significant_lines(aDocument);
		if (lines.empty()) {
			return refusal(rejection_of(
				theme_import_reason::malformed_document, std::string_view{ }, 0U));
		}
		if (const auto header = check_header(lines.front()); header.has_value()) {
			return refusal(*header);
		}

		auto state = body_state{ };
		for (auto line = std::next(lines.begin()); line != lines.end(); ++line) {
			read_entry(state, *line);
		}
		if (state.Rejections.empty()) {
			// A missing role only means something once every present line was
			// legal; otherwise the list is noise from an already-failed parse.
			append_missing_roles(state);
		}
		if (!state.Rejections.empty()) {
			return theme_import_result{
				.Palette    = std::nullopt,
				.Rejections = std::move(state.Rejections),
			};
		}

		return theme_import_result{ .Palette = state.Palette, .Rejections = { } };
	}
} // namespace bpr::core
