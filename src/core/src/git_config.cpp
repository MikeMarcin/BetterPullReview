#include "bpr/core/git_config.hpp"

#include <array>
#include <limits>

namespace bpr::core {
	namespace {
		/// Git's own character classes (`sane_ctype`): ASCII only, and white
		/// space is space, tab, line feed and carriage return.
		auto is_space(const int aCharacter) -> bool
		{
			return aCharacter == ' ' || aCharacter == '\t' || aCharacter == '\n' || aCharacter == '\r';
		}

		auto is_alpha(const int aCharacter) -> bool
		{
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z');
		}

		auto is_key_character(const int aCharacter) -> bool
		{
			return is_alpha(aCharacter) || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-';
		}

		auto lower(const int aCharacter) -> char
		{
			return static_cast<char>(aCharacter >= 'A' && aCharacter <= 'Z' ? aCharacter - 'A' + 'a' : aCharacter);
		}

		/// The byte stream as Git's `get_next_char` sees it: CR LF reads as LF,
		/// and the end of the text reads as a final LF with `End` set.
		class config_stream
		{
		public:
			explicit config_stream(const std::string_view aText)
				: Text{ aText }
			{
			}

			auto next() -> int
			{
				if (Offset >= Text.size()) {
					End = true;
					return '\n';
				}
				const auto character = static_cast<unsigned char>(Text[Offset++]);
				if (character == '\r' && Offset < Text.size() && Text[Offset] == '\n') {
					++Offset;
					return '\n';
				}
				return character;
			}

			[[nodiscard]] auto at_end() const noexcept -> bool
			{
				return End;
			}

		private:
			std::string_view Text;
			std::size_t      Offset = 0;
			bool             End    = false;
		};

		/// `[section]`, `[section.sub]` or `[section "sub"]` after the `[`. The
		/// result is Git's dotted stem, without a trailing dot.
		auto section_header(config_stream& aStream) -> std::optional<std::string>
		{
			auto name = std::string{ };
			for (;;) {
				auto character = aStream.next();
				if (aStream.at_end()) {
					return std::nullopt;
				}
				if (character == ']') {
					return name;
				}
				if (is_space(character)) {
					// The extended form: white space, then a quoted subsection.
					do {
						if (character == '\n') {
							return std::nullopt;
						}
						character = aStream.next();
					} while (is_space(character));
					if (character != '"') {
						return std::nullopt;
					}
					name += '.';
					for (;;) {
						character = aStream.next();
						if (character == '\n') {
							return std::nullopt;
						}
						if (character == '"') {
							break;
						}
						if (character == '\\') {
							character = aStream.next();
							if (character == '\n') {
								return std::nullopt;
							}
						}
						name += static_cast<char>(character);
					}
					if (aStream.next() != ']') {
						return std::nullopt;
					}
					return name;
				}
				if (!is_key_character(character) && character != '.') {
					return std::nullopt;
				}
				name += lower(character);
			}
		}

		/// Everything after `=`, up to the end of the logical line.
		auto value_text(config_stream& aStream) -> std::optional<std::string>
		{
			auto value   = std::string{ };
			auto quoted  = false;
			auto comment = false;
			auto spaces  = std::size_t{ 0 };
			for (;;) {
				auto character = aStream.next();
				if (character == '\n') {
					if (quoted) {
						return std::nullopt;
					}
					return value;
				}
				if (comment) {
					continue;
				}
				if (is_space(character) && !quoted) {
					if (!value.empty()) {
						++spaces;
					}
					continue;
				}
				if (!quoted && (character == ';' || character == '#')) {
					comment = true;
					continue;
				}
				value.append(spaces, ' ');
				spaces = 0;
				if (character == '\\') {
					character = aStream.next();
					switch (character) {
					case '\n':
						continue;
					case 't':
						character = '\t';
						break;
					case 'b':
						character = '\b';
						break;
					case 'n':
						character = '\n';
						break;
					case '\\':
					case '"':
						break;
					default:
						return std::nullopt;
					}
					value += static_cast<char>(character);
					continue;
				}
				if (character == '"') {
					quoted = !quoted;
					continue;
				}
				value += static_cast<char>(character);
			}
		}

		auto split_stem(const std::string& aStem, git_config_entry& aEntry) -> void
		{
			const auto dot = aStem.find('.');
			aEntry.Section = aStem.substr(0, dot);
			if (dot != std::string::npos) {
				aEntry.Subsection = aStem.substr(dot + 1);
			}
		}
	}

	auto parse_git_config(const std::string_view aText) -> std::optional<std::vector<git_config_entry>>
	{
		constexpr auto byteOrderMark = std::array<unsigned char, 3>{ 0xEFU, 0xBBU, 0xBFU };
		auto           stream        = config_stream{ aText };
		auto           entries       = std::vector<git_config_entry>{ };
		auto           stem          = std::string{ };
		auto           comment       = false;
		auto           mark          = std::size_t{ 0 };
		auto           atStart       = true;
		for (;;) {
			auto character = stream.next();
			if (atStart) {
				// A UTF-8 byte order mark is skipped; a partial one is an error.
				if (mark < byteOrderMark.size() && character == byteOrderMark.at(mark) && !stream.at_end()) {
					++mark;
					continue;
				}
				if (mark != 0 && mark < byteOrderMark.size()) {
					return std::nullopt;
				}
				atStart = false;
			}
			if (character == '\n') {
				if (stream.at_end()) {
					return entries;
				}
				comment = false;
				continue;
			}
			if (comment || is_space(character)) {
				continue;
			}
			if (character == '#' || character == ';') {
				comment = true;
				continue;
			}
			if (character == '[') {
				auto header = section_header(stream);
				if (!header || header->empty()) {
					return std::nullopt;
				}
				stem = std::move(*header);
				continue;
			}
			if (!is_alpha(character)) {
				return std::nullopt;
			}
			auto entry = git_config_entry{ };
			split_stem(stem, entry);
			entry.Key += lower(character);
			for (;;) {
				character = stream.next();
				if (stream.at_end() || !is_key_character(character)) {
					break;
				}
				entry.Key += lower(character);
			}
			while (character == ' ' || character == '\t') {
				character = stream.next();
			}
			if (character != '\n') {
				if (character != '=') {
					return std::nullopt;
				}
				entry.Value = value_text(stream);
				if (!entry.Value) {
					return std::nullopt;
				}
			}
			if (entries.size() >= maximum_git_config_entries) {
				return std::nullopt;
			}
			entries.push_back(std::move(entry));
			if (stream.at_end()) {
				return entries;
			}
		}
	}

	auto git_config_int(const std::optional<std::string>& aValue) -> std::optional<std::int32_t>
	{
		if (!aValue) {
			return std::nullopt;
		}
		auto text = std::string_view{ *aValue };
		while (!text.empty() && is_space(text.front())) {
			text.remove_prefix(1);
		}
		auto negative = false;
		if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
			negative = text.front() == '-';
			text.remove_prefix(1);
		}
		// strtoimax with base 0: a 0x prefix is hexadecimal and a leading 0
		// octal.
		auto base = 10U;
		if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
			base = 16U;
			text.remove_prefix(2);
		} else if (!text.empty() && text[0] == '0') {
			base = 8U;
		}
		constexpr auto limit     = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1U;
		auto           magnitude = std::uint64_t{ 0 };
		auto           digits    = std::size_t{ 0 };
		while (digits < text.size()) {
			const auto character = text[digits];
			auto       digit     = 99U;
			if (character >= '0' && character <= '9') {
				digit = static_cast<unsigned>(character - '0');
			} else if (character >= 'a' && character <= 'f') {
				digit = static_cast<unsigned>(character - 'a' + 10);
			} else if (character >= 'A' && character <= 'F') {
				digit = static_cast<unsigned>(character - 'A' + 10);
			}
			if (digit >= base) {
				break;
			}
			magnitude = (magnitude * base) + digit;
			if (magnitude > limit) {
				return std::nullopt;
			}
			++digits;
		}
		if (digits == 0) {
			return std::nullopt;
		}
		text.remove_prefix(digits);
		auto factor = std::uint64_t{ 1 };
		if (text.size() == 1) {
			switch (lower(text.front())) {
			case 'k':
				factor = 1024U;
				break;
			case 'm':
				factor = 1024U * 1024U;
				break;
			case 'g':
				factor = 1024U * 1024U * 1024U;
				break;
			default:
				return std::nullopt;
			}
		} else if (!text.empty()) {
			return std::nullopt;
		}
		if (magnitude > limit / factor) {
			return std::nullopt;
		}
		magnitude *= factor;
		if (negative) {
			return magnitude > limit ? std::nullopt : std::optional{ static_cast<std::int32_t>(-static_cast<std::int64_t>(magnitude)) };
		}
		return magnitude >= limit ? std::nullopt : std::optional{ static_cast<std::int32_t>(magnitude) };
	}

	auto git_config_bool(const std::optional<std::string>& aValue) -> std::optional<bool>
	{
		if (!aValue) {
			return true;
		}
		const auto equals = [&aValue](const std::string_view aWord) {
			if (aValue->size() != aWord.size()) {
				return false;
			}
			for (std::size_t index = 0; index < aWord.size(); ++index) {
				if (lower(static_cast<unsigned char>((*aValue)[index])) != aWord[index]) {
					return false;
				}
			}
			return true;
		};
		if (equals("true") || equals("yes") || equals("on")) {
			return true;
		}
		if (aValue->empty() || equals("false") || equals("no") || equals("off")) {
			return false;
		}
		if (const auto number = git_config_int(aValue)) {
			return *number != 0;
		}
		return std::nullopt;
	}
}
