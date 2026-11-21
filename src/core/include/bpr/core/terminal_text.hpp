#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_TERMINAL_TEXT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_TERMINAL_TEXT_HPP

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
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_TERMINAL_TEXT_HPP
