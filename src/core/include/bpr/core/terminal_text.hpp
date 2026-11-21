#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_TERMINAL_TEXT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_TERMINAL_TEXT_HPP

#include <cstddef>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-2215]] (owner, 0.5.0 build 75: the details box showed
	/// `␛[0;31m…␛[0m`). Process output as plain text, for showing it.
	///
	/// \details Removes the terminal escape sequences a program writes for a
	/// terminal: control sequences (`ESC [`, parameters, intermediates and a
	/// final byte, colours and cursor moves among them); operating system
	/// commands (`ESC ]`, links among them) and the device control, start of
	/// string, privacy and application program strings (`ESC P`, `ESC X`,
	/// `ESC ^`, `ESC _`), each up to BEL or `ESC \`; and the other escape
	/// sequences (`ESC`, intermediates, a final byte). Every other byte is
	/// kept as it is: text, UTF-8, tabs, line ends. A control sequence broken
	/// by a byte that cannot be in one ends before that byte, which is kept.
	/// A sequence the text ends inside, as a bounded capture can, is removed
	/// to the end. Bytes 0x80 to 0x9F are never read as controls, since they
	/// are UTF-8 continuation bytes. Only for showing: logs keep the bytes.
	[[nodiscard]] auto plain_terminal_text(std::string_view aText) -> std::string;

	/// \brief The most bytes of an unfinished sequence held for the next
	/// chunk; a longer one is removed, as `plain_terminal_text` removes it.
	inline constexpr std::size_t maximum_terminal_carry_bytes = 4096U;

	/// \brief One chunk of a stream as plain text, and what it held back.
	struct plain_terminal_piece
	{
		/// \brief The chunk's text without its finished escape sequences.
		std::string Text;
		/// \brief An escape sequence the chunk ended inside, to be read again
		/// before the next chunk. Empty when there is none.
		std::string Carry;

		auto operator==(const plain_terminal_piece&) const -> bool = default;
	};

	/// \brief [[BPR-2215]]. A chunk of streamed process output as plain text,
	/// for a sequence a stream splits between two chunks.
	///
	/// \details Reads `aCarry`, the previous piece's `Carry`, then `aChunk`,
	/// as `plain_terminal_text` reads text, except that an escape sequence
	/// the two end inside is held back as `Carry` rather than removed. The
	/// stream's next chunk is read with it; at the end of the stream it is
	/// dropped, as `plain_terminal_text` drops a sequence cut off at the end.
	/// A sequence longer than `maximum_terminal_carry_bytes` is removed and
	/// not held. Splitting a stream anywhere gives the text
	/// `plain_terminal_text` gives for the whole.
	[[nodiscard]] auto plain_terminal_chunk(std::string_view aCarry, std::string_view aChunk) -> plain_terminal_piece;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_TERMINAL_TEXT_HPP
