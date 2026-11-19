// [[BPR-1774]]. The description edit's decisions, without Qt or a provider.
#include "bpr/core/description_draft.hpp"
#include "bpr/core/description_edit.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using core::description_edit_refusal;
	using core::description_save_outcome;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_edit_is_offered_to_the_author_alone()
	{
		expect(core::description_edit_offered("octo-author", "octo-author"), "the author is offered Edit");
		expect(core::description_edit_offered("MikeMarcin", "mikemarcin"), "logins compare without case, as GitHub's do");
		expect(!core::description_edit_offered("octo-author", "octo-reviewer"), "another account is not");
		expect(!core::description_edit_offered("octo-author", ""), "an anonymous read is not");
		expect(!core::description_edit_offered("", ""), "an unknown author is nobody's");
		expect(!core::description_edit_offered("octo-author", "octo-autho"), "a prefix is another login");
		expect(!core::description_edit_offered("\xC3\x89", "\xC3\xA9"), "only ASCII folds");
	}

	void test_a_save_gesture_is_decided_in_order()
	{
		const auto decide = [](const std::string_view aViewer, const std::string_view aEdited) {
			return core::decide_description_edit({ .Author = "octo-author", .Viewer = aViewer, .Original = "Before.\r\n", .Edited = aEdited });
		};
		expect(decide("octo-author", "After.\r\n") == description_edit_refusal::none, "a change by the author is saved");
		expect(decide("octo-reviewer", "After.\r\n") == description_edit_refusal::not_author, "another account is refused first");
		expect(decide("octo-reviewer", "Before.\r\n") == description_edit_refusal::not_author, "before unchanged");
		expect(decide("octo-author", "Before.\r\n") == description_edit_refusal::unchanged, "the same bytes are unchanged");
		expect(decide("octo-author", "Before.\n") == description_edit_refusal::none, "a line ending is a change: bytes are compared");
		expect(decide("octo-author", "") == description_edit_refusal::none, "an emptied description is a change GitHub accepts");

		const auto limit = std::string(core::maximum_description_characters, 'a');
		expect(decide("octo-author", limit) == description_edit_refusal::none, "65536 characters is GitHub's limit, kept");
		expect(decide("octo-author", limit + "a") == description_edit_refusal::too_long, "one more is refused");
		auto wide = std::string{ };
		for (std::size_t index = 0; index < core::maximum_description_characters; ++index) {
			wide += "\xE2\x9C\x93";
		}
		expect(core::description_characters(wide) == core::maximum_description_characters, "characters are code points, not bytes");
		expect(decide("octo-author", wide) == description_edit_refusal::none, "65536 three-byte characters are within the limit");
		expect(core::description_characters("\xF0\x9F\x91\x8D") == 1U, "a four-byte emoji is one character");
	}

	void test_a_save_is_decided_on_githubs_answer()
	{
		const auto decide = [](const std::string_view aCurrent, const bool aCan) {
			return core::decide_description_save({ .Original = "Before.", .Edited = "After.", .Current = aCurrent, .ViewerCanUpdate = aCan });
		};
		expect(decide("Before.", true) == description_save_outcome::write, "GitHub still has the original: write once");
		expect(decide("Before.", false) == description_save_outcome::not_permitted, "GitHub refuses the viewer");
		expect(decide("Someone else's.", true) == description_save_outcome::conflict, "a newer body is a conflict, never overwritten");
		expect(decide("Someone else's.", false) == description_save_outcome::not_permitted, "permission is said before the conflict");
		expect(decide("After.", true) == description_save_outcome::already_saved, "GitHub already has the text: nothing is sent");
		expect(decide("After.", false) == description_save_outcome::already_saved, "even when the viewer can no longer update");
	}

	void test_names_are_stable()
	{
		expect(core::name(description_edit_refusal::not_author) == "not_author" && core::name(description_edit_refusal::too_long) == "too_long"
				   && core::name(description_edit_refusal::conflict) == "conflict" && core::name(description_edit_refusal::unchanged) == "unchanged",
			   "refusal names");
		expect(core::name(description_save_outcome::already_saved) == "already_saved" && core::name(description_save_outcome::write) == "write", "outcome names");
	}

	/// [[BPR-2281]]. Draft with agent's material and the answer it reads.
	void test_a_description_draft_is_composed_and_read()
	{
		using namespace bpr::core;
		auto material = description_draft_material{
			.Title = "  Clamp the resize  ",
			.Body  = "",
			.Files = { { .Path = "src/a.cpp", .Change = "modified", .Patch = std::string{ "@@ -1 +1 @@\n-old\n+new" } }, { .Path = "logo.png", .Change = "added", .Patch = std::nullopt } },
		};
		const auto plain = compose_description_draft(material);
		expect(plain.starts_with(description_draft_marker) && plain.find("Title: Clamp the resize\n") != std::string::npos && plain.find("has no description yet") != std::string::npos,
			   "the marker, the title and an empty description");
		expect(plain.find("src/a.cpp (modified)\n```diff\n@@ -1 +1 @@\n-old\n+new\n```") != std::string::npos && plain.find("logo.png (added)\n(no patch") != std::string::npos,
			   "each file with its patch, or that it has none");
		expect(compose_description_draft(material) == plain, "equal material, equal message");
		material.Body = "## Why\n\nKeep me.";
		expect(compose_description_draft(material).find("```markdown\n## Why\n\nKeep me.\n```") != std::string::npos, "the current description, to rewrite");
		material.Files.push_back({ .Path = "big.cpp", .Change = "modified", .Patch = std::string(maximum_description_draft_diff_bytes, 'x') });
		material.Files.push_back({ .Path = "after.cpp", .Change = "modified", .Patch = std::string{ "+x" } });
		const auto bounded = compose_description_draft(material);
		expect(bounded.find("Left out, past what one request carries: big.cpp after.cpp") != std::string::npos && bounded.find("big.cpp (modified)") == std::string::npos,
			   "past the diff bound, the rest of the files are named and left out");

		expect(read_description_draft("  ## Summary\n\nIt clamps.\n ") == std::optional<std::string>{ "## Summary\n\nIt clamps." }, "an answer is trimmed");
		expect(read_description_draft("```markdown\n## Summary\n\nIt clamps.\n```") == std::optional<std::string>{ "## Summary\n\nIt clamps." }, "one fence around the whole answer is unwrapped");
		const auto withCode = std::string{ "```cpp\nint a;\n```\n\nThen text.\n\n```cpp\nint b;\n```" };
		expect(read_description_draft(withCode) == std::optional{ withCode }, "fences inside the description stay");
		expect(!read_description_draft(" \n\t") && !read_description_draft(std::string(maximum_description_characters + 1U, 'a')), "empty or too long reads as nothing");
	}
} // namespace

auto main() -> int
{
	test_edit_is_offered_to_the_author_alone();
	test_a_save_gesture_is_decided_in_order();
	test_a_save_is_decided_on_githubs_answer();
	test_names_are_stable();
	test_a_description_draft_is_composed_and_read();
	std::cout << "description edit core tests passed\n";
	return EXIT_SUCCESS;
}
