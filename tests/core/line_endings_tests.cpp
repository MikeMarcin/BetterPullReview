#include "bpr/core/line_endings.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-1298]]. A body's line endings survive an editor that reads
/// every break back as LF.

namespace {
	using bpr::core::restore_line_endings;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "line endings failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// What a Qt plain-text editor reads back: every CRLF as LF.
	[[nodiscard]] auto as_editor_reads(std::string aBody) -> std::string
	{
		for (auto at = aBody.find("\r\n"); at != std::string::npos; at = aBody.find("\r\n", at)) {
			aBody.erase(at, 1U);
		}
		return aBody;
	}

	void test_an_unedited_body_is_byte_for_byte()
	{
		for (const std::string_view body : { "", "one line", "lf\nbody\n", "crlf\r\nbody\r\n", "mixed\r\nends\nhere\r\nlast",
											 "issue (blocking): Bound the copy.\r\n\r\n```suggestion\r\n\tcopy(a, b);\r\n```\r\n",
											 "trailing break\r\n", "\r\n\r\n" }) {
			expect(restore_line_endings(as_editor_reads(std::string{ body }), body) == body, "an unedited body returns exactly");
		}
	}

	void test_a_header_edit_keeps_every_other_ending()
	{
		const auto body   = std::string{ "question: Why copy here?\r\n\r\n```suggestion\r\n\tcopy(a, b);\r\n```\r\ntail\nmixed" };
		auto       edited = as_editor_reads(body);
		edited.replace(0U, std::string_view{ "question:" }.size(), "issue (blocking):");
		const auto restored = restore_line_endings(edited, body);
		expect(restored == "issue (blocking): Why copy here?\r\n\r\n```suggestion\r\n\tcopy(a, b);\r\n```\r\ntail\nmixed",
			   "a label edit on the first line keeps every later CR, mixed endings included");
	}

	void test_changed_lines_take_the_body_style()
	{
		const auto crlf = std::string{ "first\r\nsecond\r\nthird" };
		expect(restore_line_endings("first\nsecond, edited\nthird", crlf) == "first\r\nsecond, edited\r\nthird", "an edited middle line keeps CRLF");
		expect(restore_line_endings("first\nnew\nlines\nsecond\nthird", crlf) == "first\r\nnew\r\nlines\r\nsecond\r\nthird", "inserted lines take CRLF");
		expect(restore_line_endings("first\nthird", crlf) == "first\r\nthird", "a removed line leaves CRLF around it");
		expect(restore_line_endings("first\nsecond\nthird\nmore", crlf) == "first\r\nsecond\r\nthird\r\nmore", "an appended line takes CRLF");
		expect(restore_line_endings("a\nb", "lf\nonly") == "a\nb", "an LF body stays LF");
		expect(restore_line_endings("typed\nfresh", "") == "typed\nfresh", "a new body is LF");
		expect(restore_line_endings("one\ntwo\nthree", "x\r\ny\nz\r\n") == "one\r\ntwo\r\nthree", "CRLF dominates when at least as many");
		expect(restore_line_endings("one\ntwo\nthree", "x\ny\nz\r\n") == "one\ntwo\nthree", "LF dominates when more lone LF");
	}
} // namespace

auto main() -> int
{
	test_an_unedited_body_is_byte_for_byte();
	test_a_header_edit_keeps_every_other_ending();
	test_changed_lines_take_the_body_style();
	std::cout << "line-endings-tests:pass\n";
	return EXIT_SUCCESS;
}
