// [[BPR-1864]]. A workbench window's title, without Qt.
#include "bpr/core/window_title.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto expect_equal(const std::string& aActual, const std::string_view aExpected, const std::string_view aMessage) -> void
	{
		if (aActual != aExpected) {
			std::cerr << "FAILED: " << aMessage << "\n  actual:   " << aActual << "\n  expected: " << aExpected << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto characters(const std::string_view aText) -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto byte : aText) {
			count += (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U ? 1U : 0U;
		}
		return count;
	}

	auto pull(const std::string_view aTitle) -> core::window_title_pull_request
	{
		return { .Owner = "MikeMarcin", .Repository = "BPR-Test", .Number = 188875U, .Title = aTitle };
	}

	void test_nothing_open_keeps_the_application_and_profile()
	{
		expect_equal(core::window_title({ }), "BetterPullReview", "a window with no profile yet is the application");
		expect_equal(core::window_title({ .Profile = "Default" }), "BetterPullReview \xE2\x80\x94 Default profile", "Home names the profile");
		expect_equal(core::window_title({ .Profile = "Work", .Profiles = 3U }), "BetterPullReview \xE2\x80\x94 Work profile", "with several profiles too");
		expect_equal(
			core::window_title({ .Profile = "Default", .Mode = "Reading without signing in" }),
			"BetterPullReview \xE2\x80\x94 Reading without signing in \xE2\x80\x94 Default profile",
			"a signed-out Home says its mode, as before");
		expect_equal(
			core::window_title({ .PullRequest = core::window_title_pull_request{ .Owner = "o", .Repository = "r", .Number = 0U, .Title = "t" }, .Profile = "Default" }),
			"BetterPullReview \xE2\x80\x94 Default profile",
			"a pull request without a number names nothing");
	}

	void test_a_pull_request_is_named_number_first()
	{
		expect_equal(
			core::window_title({ .PullRequest = pull("Add the Flow Control panel"), .Profile = "Default", .Profiles = 1U }),
			"#188875 Add the Flow Control panel \xE2\x80\x94 MikeMarcin/BPR-Test",
			"number, title, repository; one profile is not named");
		expect_equal(
			core::window_title({ .PullRequest = pull("Add the Flow Control panel"), .Profile = "Work", .Profiles = 2U }),
			"#188875 Add the Flow Control panel \xE2\x80\x94 MikeMarcin/BPR-Test \xE2\x80\x94 Work profile",
			"with more than one profile, the profile follows last");
		expect_equal(
			core::window_title({ .PullRequest = pull("Fix"), .Profile = "", .Profiles = 2U }),
			"#188875 Fix \xE2\x80\x94 MikeMarcin/BPR-Test",
			"an unlabelled profile adds nothing");
		expect_equal(
			core::window_title({ .PullRequest = pull("Docs"), .Profile = "Default", .Mode = "Reading without signing in" }),
			"#188875 Docs \xE2\x80\x94 MikeMarcin/BPR-Test",
			"the mode is Home's to say, not a pull request's");
		expect_equal(core::window_title({ .PullRequest = pull("") }), "#188875 \xE2\x80\x94 MikeMarcin/BPR-Test", "an empty title leaves the number");
		expect_equal(
			core::window_title({ .PullRequest = core::window_title_pull_request{ .Number = 7U, .Title = "Seven" } }),
			"#7 Seven",
			"no repository, no separator");
	}

	void test_a_long_title_is_shortened_at_a_word()
	{
		const auto long_title = std::string_view{ "Add the Flow Control panel to the review workbench and keep its state per window" };
		const auto shortened  = core::shorten_window_title_text(long_title, core::maximum_window_title_pull_request_characters);
		expect_equal(shortened, "Add the Flow Control panel to the review\xE2\x80\xA6", "cut at the last word that fits, with an ellipsis");
		expect(characters(shortened) <= core::maximum_window_title_pull_request_characters, "never longer than the limit");
		expect_equal(
			core::window_title({ .PullRequest = pull(long_title), .Profile = "Default", .Profiles = 1U }),
			"#188875 Add the Flow Control panel to the review\xE2\x80\xA6 \xE2\x80\x94 MikeMarcin/BPR-Test",
			"the repository survives a long title");

		const auto exact = std::string(core::maximum_window_title_pull_request_characters, 'a');
		expect_equal(core::shorten_window_title_text(exact, core::maximum_window_title_pull_request_characters), exact, "a title at the limit is whole");
		const auto word = std::string(core::maximum_window_title_pull_request_characters + 10U, 'a');
		const auto cut  = core::shorten_window_title_text(word, core::maximum_window_title_pull_request_characters);
		expect(characters(cut) == core::maximum_window_title_pull_request_characters && cut.ends_with("\xE2\x80\xA6"), "one long word is cut within it");
		expect_equal(
			core::shorten_window_title_text("Short words then averyveryveryverylongwordthatgoesonandon", 40U),
			"Short words then averyveryveryverylongw\xE2\x80\xA6",
			"a space too far back is not cut at");
		expect_equal(core::shorten_window_title_text("anything", 0U), "", "no room, no text");
		expect_equal(
			core::shorten_window_title_text("Add XMMatrixInverseTranspose for optimal normal vector transformation", core::maximum_window_title_pull_request_characters),
			"Add XMMatrixInverseTranspose for optimal normal\xE2\x80\xA6",
			"a word that ends exactly at the cut is kept");
	}

	void test_text_is_one_line_and_whole_characters()
	{
		expect_equal(core::shorten_window_title_text("  Fix\r\nthe\tbuild  ", 48U), "Fix the build", "controls read as spaces, collapsed and trimmed");
		auto wide = std::string{ };
		for (int index = 0; index < 60; ++index) {
			wide += "\xE2\x9C\x93";
		}
		const auto cut = core::shorten_window_title_text(wide, 10U);
		expect(characters(cut) == 10U && cut.size() == 30U, "multi-byte characters are counted and cut whole");
		expect_equal(core::shorten_window_title_text("\xC3\xA9t\xC3\xA9 fix", 48U), "\xC3\xA9t\xC3\xA9 fix", "short UTF-8 is literal");
	}

	void test_equal_inputs_give_equal_titles()
	{
		const auto input = core::window_title_input{ .PullRequest = pull("Same"), .Profile = "Work", .Profiles = 2U };
		expect(core::window_title(input) == core::window_title(input), "deterministic");
	}
} // namespace

auto main() -> int
{
	test_nothing_open_keeps_the_application_and_profile();
	test_a_pull_request_is_named_number_first();
	test_a_long_title_is_shortened_at_a_word();
	test_text_is_one_line_and_whole_characters();
	test_equal_inputs_give_equal_titles();
	std::cout << "window_title: all checks passed\n";
	return EXIT_SUCCESS;
}
