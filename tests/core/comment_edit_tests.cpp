// [[BPR-1775]]. A published comment's edit decisions, without Qt or a provider.
#include "bpr/core/comment_edit.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using core::comment_edit_kind;
	using core::comment_edit_refusal;
	using core::description_save_outcome;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_posts_name_their_kind_and_node()
	{
		const auto review = core::comment_edit_subject_of("review-comment:PRRC_kw1");
		expect(review.has_value() && review->Kind == comment_edit_kind::review_comment && review->Node == "PRRC_kw1", "a review-thread comment");
		const auto issue = core::comment_edit_subject_of("issue-comment:IC_kw2");
		expect(issue.has_value() && issue->Kind == comment_edit_kind::pull_request_comment && issue->Node == "IC_kw2", "a pull request comment");
		const auto body = core::comment_edit_subject_of("review:PRR_kw3");
		expect(body.has_value() && body->Kind == comment_edit_kind::review_body && body->Node == "PRR_kw3", "a review's body");
		expect(!core::comment_edit_subject_of("review:").has_value(), "an empty node is no subject");
		expect(!core::comment_edit_subject_of("draft:unpublished").has_value(), "a draft is not a published comment");
		expect(!core::comment_edit_subject_of("thread:PRRT_kw4").has_value(), "a thread is not a comment");
		expect(!core::comment_edit_subject_of("").has_value(), "nothing is nothing");
		expect(core::name(comment_edit_kind::review_body) == "review_body", "kinds are named");
	}

	void test_edit_is_offered_on_my_posts_alone()
	{
		expect(core::comment_edit_offered("issue-comment:IC_1", true, false), "my pull request comment");
		expect(!core::comment_edit_offered("issue-comment:IC_1", false, false), "someone else's");
		expect(!core::comment_edit_offered("issue-comment:IC_1", true, true), "never in an anonymous read");
		expect(!core::comment_edit_offered("draft:unpublished", true, false), "not a draft, which has its own editor");
	}

	void test_a_save_gesture_is_decided_in_order()
	{
		const auto decide = [](const bool aMine, const std::string_view aEdited) {
			return core::decide_comment_edit({ .Mine = aMine, .Original = "Before.\r\n", .Edited = aEdited });
		};
		expect(decide(true, "After.\r\n") == comment_edit_refusal::none, "my change is saved");
		expect(decide(false, "After.\r\n") == comment_edit_refusal::not_mine, "someone else's is refused first");
		expect(decide(false, "Before.\r\n") == comment_edit_refusal::not_mine, "before unchanged");
		expect(core::decide_comment_edit({ .Mine = true, .Anonymous = true, .Original = "a", .Edited = "b" }) == comment_edit_refusal::not_mine, "an anonymous read edits nothing");
		expect(decide(true, "Before.\r\n") == comment_edit_refusal::unchanged, "the same bytes are unchanged");
		expect(decide(true, "Before.\n") == comment_edit_refusal::none, "a CRLF to LF change is a change");
		expect(decide(true, " \r\n\t") == comment_edit_refusal::empty, "blank text is refused");
		expect(decide(true, "") == comment_edit_refusal::empty, "empty text is refused");
		auto longest = std::string(core::maximum_comment_characters, 'x');
		expect(decide(true, longest) == comment_edit_refusal::none, "65,536 characters are kept");
		longest.push_back('x');
		expect(decide(true, longest) == comment_edit_refusal::too_long, "one more is refused");
		auto wide = std::string{ };
		for (std::size_t index = 0U; index < core::maximum_comment_characters; ++index) {
			wide += "\xF0\x9F\x98\x80";
		}
		expect(decide(true, wide) == comment_edit_refusal::none, "a four-byte character counts as one");
		expect(core::name(comment_edit_refusal::empty) == "empty", "refusals are named");
	}

	void test_a_save_is_decided_on_githubs_answer()
	{
		const auto decide = [](const std::string_view aCurrent, const bool aCan) {
			return core::decide_comment_save({ .Original = "Before.", .Edited = "After.", .Current = aCurrent, .ViewerCanUpdate = aCan });
		};
		expect(decide("Before.", true) == description_save_outcome::write, "GitHub has the original: write once");
		expect(decide("After.", true) == description_save_outcome::already_saved, "GitHub already has the text");
		expect(decide("After.", false) == description_save_outcome::already_saved, "already saved before permission");
		expect(decide("Before.", false) == description_save_outcome::not_permitted, "viewerCanUpdate false is refused");
		expect(decide("Changed elsewhere.", false) == description_save_outcome::not_permitted, "not permitted before conflict");
		expect(decide("Changed elsewhere.", true) == description_save_outcome::conflict, "a changed body is never overwritten");
	}
}

auto main() -> int
{
	test_posts_name_their_kind_and_node();
	test_edit_is_offered_on_my_posts_alone();
	test_a_save_gesture_is_decided_in_order();
	test_a_save_is_decided_on_githubs_answer();
	std::cout << "COMMENT_EDIT_OK\n";
	return EXIT_SUCCESS;
}
