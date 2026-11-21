/// \file
/// \brief [[BPR-2215]] (owner, 0.5.0 build 75: the details box showed
/// `␛[0;31m…␛[0m`). Process output loses its terminal escape sequences, and
/// nothing else, before it is shown.

#include "bpr/core/terminal_text.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using bpr::core::plain_terminal_text;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void expect_plain(const std::string_view aText, const std::string_view aPlain, const std::string_view aMessage)
	{
		const auto plain = plain_terminal_text(aText);
		if (plain != aPlain) {
			std::cerr << "FAILED: " << aMessage << ": got [" << plain << "]\n";
			std::exit(EXIT_FAILURE);
		}
	}
} // namespace

auto main() -> int
{
	expect_plain("", "", "nothing stays nothing");
	expect_plain("error: no such file\n\tat line 3\r\n", "error: no such file\n\tat line 3\r\n", "text, tabs and line ends are kept as they are");
	expect_plain("\x1b[0;31merror:\x1b[0m bad\n", "error: bad\n", "the owner's colour codes go");
	expect_plain("\x1b[1m\x1b[4mbold\x1b[22m\x1b[24m", "bold", "attribute on and off codes go");
	expect_plain("\x1b[38;5;196mred\x1b[39m \x1b[48;5;21mblue\x1b[49m", "red blue", "256-colour codes go");
	expect_plain("\x1b[38;2;255;128;0morange\x1b[0m", "orange", "true colour codes go");
	expect_plain("\x1b[38:2::255:128:0morange\x1b[m", "orange", "colon-separated true colour and a bare reset go");
	expect_plain("50%\x1b[2K\x1b[1G100%\x1b[3A\x1b[?25l\x1b[?25h done", "50%100% done", "cursor moves, line erases and private modes go");
	expect_plain("\x1b]8;;https://example.com/a\x1b\\the link\x1b]8;;\x1b\\ after", "the link after", "a link keeps its words and loses its address");
	expect_plain("\x1b]0;window title\x07text", "text", "an operating system command ended by BEL goes");
	expect_plain("\x1bP1$r0m\x1b\\a\x1b_application\x1b\\b", "ab", "device control and application strings go");
	expect_plain("\x1b(Bplain\x1b)0"
				 "\x1b"
				 "7\x1b"
				 "8\x1b=\x1b>",
				 "plain", "character set and other two-byte sequences go");
	expect_plain("caf\xc3\xa9 \x1b[32m\xe2\x9c\x93\x1b[0m na\xc3\xafve \xf0\x9f\x9a\x80", "caf\xc3\xa9 \xe2\x9c\x93 na\xc3\xafve \xf0\x9f\x9a\x80", "UTF-8 around the codes is kept byte for byte");
	expect_plain("\xe2\x9b\x9b\xc2\x9b"
				 "31m",
				 "\xe2\x9b\x9b\xc2\x9b"
				 "31m",
				 "0x9B in UTF-8 is never read as a control sequence");
	expect_plain("kept\x1b[0;3", "kept", "a control sequence cut off at the end goes to the end");
	expect_plain("kept\x1b]8;;https://exam", "kept", "a link cut off at the end goes to the end");
	expect_plain("kept\x1b", "kept", "a lone ESC at the end goes");
	expect_plain("kept\x1b]0;title\x1b", "kept", "a string cut off inside its terminator goes");
	expect_plain("a\x1b[12\nb", "a\nb", "a control sequence broken by a line end ends there and the line end is kept");
	expect_plain("a\x1b\x1b[31mb", "ab", "an ESC before another ESC goes alone");
	expect_plain("a\x1b\xc3\xa9"
				 "b",
				 "a\xc3\xa9"
				 "b",
				 "an ESC before a UTF-8 character goes alone");
	expect(plain_terminal_text(std::string(100000U, 'x') + "\x1b[0m") == std::string(100000U, 'x'), "a long capture keeps its text");
	std::cout << "terminal-text-tests:pass\n";
	return EXIT_SUCCESS;
}
