#include "bpr/core/terminal_text.hpp"

#include <cstddef>

namespace bpr::core {
	namespace {
		constexpr auto escape = '\x1b';
		constexpr auto bell   = '\x07';

		[[nodiscard]] constexpr auto in(const char aByte, const unsigned char aLow, const unsigned char aHigh) noexcept -> bool
		{
			const auto byte = static_cast<unsigned char>(aByte);
			return byte >= aLow && byte <= aHigh;
		}

		/// The end of a control sequence whose parameters start at `aAt`: past
		/// its final byte, at a byte no control sequence holds, or the end.
		[[nodiscard]] auto control_sequence_end(const std::string_view aText, std::size_t aAt) noexcept -> std::size_t
		{
			while (aAt < aText.size() && in(aText[aAt], 0x30U, 0x3FU)) {
				++aAt;
			}
			while (aAt < aText.size() && in(aText[aAt], 0x20U, 0x2FU)) {
				++aAt;
			}
			return aAt < aText.size() && in(aText[aAt], 0x40U, 0x7EU) ? aAt + 1U : aAt;
		}

		/// The end of a string whose body starts at `aAt`: past BEL or
		/// `ESC \`, or the end.
		[[nodiscard]] auto string_end(const std::string_view aText, std::size_t aAt) noexcept -> std::size_t
		{
			for (; aAt < aText.size(); ++aAt) {
				if (aText[aAt] == bell) {
					return aAt + 1U;
				}
				if (aText[aAt] == escape && aAt + 1U < aText.size() && aText[aAt + 1U] == '\\') {
					return aAt + 2U;
				}
			}
			return aAt;
		}

		/// The end of the escape sequence at `aAt`, which holds ESC.
		[[nodiscard]] auto sequence_end(const std::string_view aText, const std::size_t aAt) noexcept -> std::size_t
		{
			const auto next = aAt + 1U;
			if (next == aText.size()) {
				return next;
			}
			switch (aText[next]) {
			case '[':
				return control_sequence_end(aText, next + 1U);
			case ']':
			case 'P':
			case 'X':
			case '^':
			case '_':
				return string_end(aText, next + 1U);
			default:
				break;
			}
			auto end = next;
			while (end < aText.size() && in(aText[end], 0x20U, 0x2FU)) {
				++end;
			}
			if (end < aText.size() && in(aText[end], 0x30U, 0x7EU)) {
				return end + 1U;
			}
			// A lone ESC, or one before a byte no sequence holds: only the
			// ESC and any intermediates it had go.
			return end;
		}
	} // namespace

	auto plain_terminal_text(const std::string_view aText) -> std::string
	{
		auto plain = std::string{ };
		plain.reserve(aText.size());
		auto at = std::size_t{ 0U };
		while (at < aText.size()) {
			const auto found = aText.find(escape, at);
			if (found == std::string_view::npos) {
				plain.append(aText.substr(at));
				break;
			}
			plain.append(aText.substr(at, found - at));
			at = sequence_end(aText, found);
		}
		return plain;
	}
} // namespace bpr::core
