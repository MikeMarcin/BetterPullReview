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

		/// Where an escape sequence ends, and whether the text ended first.
		struct sequence_extent
		{
			std::size_t End        = 0U;
			bool        Unfinished = false;
		};

		/// The end of a control sequence whose parameters start at `aAt`: past
		/// its final byte, at a byte no control sequence holds, or the end.
		[[nodiscard]] auto control_sequence_end(const std::string_view aText, std::size_t aAt) noexcept -> sequence_extent
		{
			while (aAt < aText.size() && in(aText[aAt], 0x30U, 0x3FU)) {
				++aAt;
			}
			while (aAt < aText.size() && in(aText[aAt], 0x20U, 0x2FU)) {
				++aAt;
			}
			if (aAt == aText.size()) {
				return { .End = aAt, .Unfinished = true };
			}
			return { .End = in(aText[aAt], 0x40U, 0x7EU) ? aAt + 1U : aAt };
		}

		/// The end of a string whose body starts at `aAt`: past BEL or
		/// `ESC \`, or the end.
		[[nodiscard]] auto string_end(const std::string_view aText, std::size_t aAt) noexcept -> sequence_extent
		{
			for (; aAt < aText.size(); ++aAt) {
				if (aText[aAt] == bell) {
					return { .End = aAt + 1U };
				}
				if (aText[aAt] == escape && aAt + 1U < aText.size() && aText[aAt + 1U] == '\\') {
					return { .End = aAt + 2U };
				}
			}
			return { .End = aAt, .Unfinished = true };
		}

		/// The end of the escape sequence at `aAt`, which holds ESC.
		[[nodiscard]] auto sequence_end(const std::string_view aText, const std::size_t aAt) noexcept -> sequence_extent
		{
			const auto next = aAt + 1U;
			if (next == aText.size()) {
				return { .End = next, .Unfinished = true };
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
			if (end == aText.size()) {
				return { .End = end, .Unfinished = true };
			}
			// A lone ESC, or one before a byte no sequence holds: only the
			// ESC and any intermediates it had go.
			return { .End = in(aText[end], 0x30U, 0x7EU) ? end + 1U : end };
		}

		/// The text without its escape sequences, and where an unfinished
		/// one at its end starts, or the text's size.
		[[nodiscard]] auto plain(const std::string_view aText, std::string& aPlain) -> std::size_t
		{
			aPlain.reserve(aPlain.size() + aText.size());
			auto at = std::size_t{ 0U };
			while (at < aText.size()) {
				const auto found = aText.find(escape, at);
				if (found == std::string_view::npos) {
					aPlain.append(aText.substr(at));
					break;
				}
				aPlain.append(aText.substr(at, found - at));
				const auto extent = sequence_end(aText, found);
				if (extent.Unfinished) {
					return found;
				}
				at = extent.End;
			}
			return aText.size();
		}
	} // namespace

	auto plain_terminal_text(const std::string_view aText) -> std::string
	{
		auto text = std::string{ };
		static_cast<void>(plain(aText, text));
		return text;
	}

	auto plain_terminal_chunk(const std::string_view aCarry, const std::string_view aChunk) -> plain_terminal_piece
	{
		auto joined = std::string{ aCarry };
		joined.append(aChunk);
		auto       piece = plain_terminal_piece{ };
		const auto held  = plain(joined, piece.Text);
		if (joined.size() - held <= maximum_terminal_carry_bytes) {
			piece.Carry = joined.substr(held);
		}
		return piece;
	}
} // namespace bpr::core
