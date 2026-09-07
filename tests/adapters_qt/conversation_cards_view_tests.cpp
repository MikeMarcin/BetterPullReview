#include "bpr/qt/conversation_cards_view.hpp"

#include "bpr/core/diff_model.hpp"

#include <QApplication>
#include <QFile>
#include <QMouseEvent>
#include <QScrollBar>

#include <glaze/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using conversation = application::github_conversation_projection;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	struct fixture_json_options : glz::opts
	{
		bool validate_skipped             = true;
		bool validate_trailing_whitespace = true;
		bool escape_control_characters    = true;
		constexpr fixture_json_options()
		{
			null_terminated       = false;
			error_on_missing_keys = true;
			error_on_unknown_keys = false;
		}
	};

	struct thread_node_json
	{
		std::string                  Id;
		std::optional<std::string>   ParentId;
		std::string                  Kind;
		std::string                  AuthorLogin;
		std::string                  Body;
		std::string                  Path;
		std::optional<std::uint32_t> Line;
		struct glaze
		{
			static constexpr auto value = glz::object("Id", &thread_node_json::Id, "ParentId", &thread_node_json::ParentId, "Kind", &thread_node_json::Kind, "AuthorLogin", &thread_node_json::AuthorLogin, "Body", &thread_node_json::Body, "Path", &thread_node_json::Path, "Line", &thread_node_json::Line);
		};
	};

	struct thread_case_json
	{
		std::string                   Id;
		std::vector<thread_node_json> Projection;
		struct glaze
		{
			static constexpr auto value = glz::object("id", &thread_case_json::Id, "projection", &thread_case_json::Projection);
		};
	};

	struct thread_provenance_json
	{
		bool RecordedEvidence;
		struct glaze
		{
			static constexpr auto value = glz::object("recorded_evidence", &thread_provenance_json::RecordedEvidence);
		};
	};

	struct thread_fixture_json
	{
		thread_provenance_json        Provenance;
		std::vector<thread_case_json> Cases;
		struct glaze
		{
			static constexpr auto value = glz::object("provenance", &thread_fixture_json::Provenance, "cases", &thread_fixture_json::Cases);
		};
	};

	/// \brief The AC-08 conversation: one thread, twenty-four posts, three branches.
	[[nodiscard]] auto branching_fixture() -> std::vector<conversation>
	{
		auto file = QFile{ QStringLiteral(BPR_BRANCHING_THREAD_FIXTURE) };
		expect(file.open(QIODevice::ReadOnly), "the branching-thread fixture opens");
		const auto          bytes = file.readAll();
		thread_fixture_json fixture;
		expect(!glz::read<fixture_json_options{ }>(fixture, std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }),
			   "the fixture has its required typed JSON fields");
		expect(!fixture.Provenance.RecordedEvidence, "the fixture declares itself constructed");
		auto found = std::vector<conversation>{ };
		for (const auto& entry : fixture.Cases) {
			for (const auto& node : entry.Projection) {
				found.push_back(conversation{
					.Id                 = node.Id,
					.ParentId           = node.ParentId,
					.Kind               = node.Kind == "review_thread" ? application::github_conversation_kind::review_thread : application::github_conversation_kind::review_comment,
					.AuthorLogin        = node.AuthorLogin,
					.Body               = node.Body,
					.Path               = node.Path,
					.Line               = node.Line,
					.DiffSide           = application::github_conversation_diff_side::right,
					.AnchorAvailability = application::github_conversation_anchor_availability::current,
				});
			}
		}
		expect(found.size() == 25U, "the fixture holds one root and twenty-four posts");
		return found;
	}

	[[nodiscard]] auto thread(
		const std::string&  aId,
		const std::string&  aPath,
		const std::uint32_t aLine,
		const bool          aCurrent  = true,
		const bool          aResolved = false) -> conversation
	{
		return conversation{
			.Id                 = aId,
			.Kind               = application::github_conversation_kind::review_thread,
			.Path               = aPath,
			.Line               = aLine,
			.DiffSide           = application::github_conversation_diff_side::right,
			.AnchorAvailability = aCurrent
				? application::github_conversation_anchor_availability::current
				: application::github_conversation_anchor_availability::missing_current_line,
			.Resolved           = aResolved,
		};
	}

	[[nodiscard]] auto reply(const std::string& aId, const std::string& aParent, const std::string& aAuthor, const std::string& aBody)
		-> conversation
	{
		return conversation{
			.Id          = aId,
			.ParentId    = aParent,
			.Kind        = application::github_conversation_kind::review_comment,
			.AuthorLogin = aAuthor,
			.Body        = aBody,
			.Path        = "src/a.cpp",
			.Line        = 10U,
		};
	}

	[[nodiscard]] auto overlapping(const std::vector<qt::thread_card>& aCards) -> bool
	{
		for (std::size_t first = 0U; first < aCards.size(); ++first) {
			for (std::size_t second = first + 1U; second < aCards.size(); ++second) {
				if (aCards[first].Rect.intersects(aCards[second].Rect)) {
					return true;
				}
			}
		}
		return false;
	}

	auto test_branching_conversation_reads_as_one_card_in_order() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 900);
		view.show();
		QApplication::processEvents();
		const auto fixture = branching_fixture();
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.adopt(fixture);
		QApplication::processEvents();
		expect(view.cards().size() == 1U && view.post_count() == 24U,
			   "the twenty-four posts of the three branches are one card");
		const auto& card  = view.cards().front();
		auto        order = std::vector<std::string>{ };
		for (const auto& post : card.Posts) {
			order.push_back(post.Id);
		}
		auto expected = std::vector<std::string>{ };
		for (const auto& node : fixture) {
			if (node.Kind == application::github_conversation_kind::review_comment) {
				expected.push_back(node.Id);
			}
		}
		expect(order == expected, "posts keep the projection's parent-before-child order");
		auto depths = std::map<std::string, std::size_t>{ };
		for (const auto& post : card.Posts) {
			depths.emplace(post.Id, post.Depth);
		}
		expect(depths.at("review-comment:branching-thread-v1-c01") == 1U
				   && depths.at("review-comment:branching-thread-v1-a08") == 9U
				   && depths.at("review-comment:branching-thread-v1-b07") == 8U
				   && depths.at("review-comment:branching-thread-v1-cc03-aside") == 5U,
			   "every branch's depth is counted from the root");
		for (std::size_t index = 1U; index < card.Posts.size(); ++index) {
			expect(card.Posts[index].Rect.top() >= card.Posts[index - 1U].Rect.bottom(),
				   "posts stack beneath each other on the rail");
		}
		expect(card.Posts.front().Mine && !card.Posts[1].Mine,
			   "my posts are marked mine by login");
		expect(card.Rect.height() > 24 * 20 && card.Actions.size() == 1U
				   && card.Actions.front().first == qt::card_action::comment_here,
			   "the card is as tall as its posts and offers a comment on its line");
		expect(view.verticalScrollBar()->maximum() > 0,
			   "a card taller than the view scrolls within it");
		expect(view.marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 41U }
				   && view.marked_lines(core::diff_side::left).empty(),
			   "the thread's line is marked, zero-based, on its side");
		view.hide();
	}

	// [[BPR-0656]]. The read state the ledger host decided: unread posts carry
	// the dot and count, the thread offers Mark read, the first post unread
	// makes the thread new for the gutter, and the strip walks the unread
	// threads of the whole pull request in file order.
	auto test_read_state_marks_unread_posts_and_offers_mark_read() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 900);
		view.show();
		QApplication::processEvents();
		auto fixture = branching_fixture();
		fixture.push_back(thread("t-after", "src/z.cpp", 5U));
		fixture.push_back(reply("c-after", "t-after", "someone", "later file"));
		fixture.push_back(thread("t-before", "src/a.cpp", 1U));
		fixture.push_back(reply("c-before", "t-before", "someone", "earlier file"));
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.adopt(fixture);
		QApplication::processEvents();
		const auto root = fixture.front().Id;
		expect(view.unread_post_count() == 0U && view.unread_threads_in_file_order().empty()
				   && view.cards().front().Actions.size() == 1U,
			   "before any read state nothing is unread and there is nothing to mark");

		view.set_read_state({
			qt::thread_read_state{ .Thread = root, .UnreadPosts = { "review-comment:branching-thread-v1-c01", "review-comment:branching-thread-v1-a03" } },
			qt::thread_read_state{ .Thread = "t-after", .UnreadPosts = { "c-after" } },
		});
		QApplication::processEvents();
		const auto rootCard = std::ranges::find(view.cards(), root, &qt::thread_card::Id);
		expect(rootCard != view.cards().end(), "the branching thread is one of the cards");
		const auto& card = *rootCard;
		expect(card.UnreadCount == 2U && card.RootUnread
				   && card.Posts.front().Unread && !card.Posts[1].Unread,
			   "the unread posts are the ones the state names, and the first post unread makes the thread new");
		expect(view.unread_post_count() == 3U, "the count is over every card");
		expect(card.Actions.size() == 2U && card.Actions.back().first == qt::card_action::mark_read,
			   "a thread with unread posts offers Mark read after Comment here");
		expect(view.unread_threads_in_file_order() == std::vector<std::string>{ root, "t-after" },
			   "the unread threads of the whole pull request come in file order, the read one absent");
		expect(view.unread_marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 4U, 41U },
			   "the lines of the new threads are the gutter's blue marks");

		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		const auto markRead = card.Actions.back().second.center();
		auto       press    = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ markRead }, QPointF{ markRead },
										   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(view.viewport(), &press);
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::mark_read, root } },
			   "a press on Mark read reports the action for its thread");

		view.set_read_state({ qt::thread_read_state{ .Thread = root, .UnreadPosts = { "review-comment:branching-thread-v1-a03" } } });
		QApplication::processEvents();
		const auto later = std::ranges::find(view.cards(), root, &qt::thread_card::Id);
		expect(later != view.cards().end() && !later->RootUnread && later->UnreadCount == 1U
				   && view.unread_marked_lines(core::diff_side::right).empty(),
			   "a thread whose first post is read is no longer new, though a later post is unread");
		view.set_read_state({ });
		QApplication::processEvents();
		const auto readCard = std::ranges::find(view.cards(), root, &qt::thread_card::Id);
		expect(view.unread_post_count() == 0U && readCard != view.cards().end()
				   && readCard->Actions.size() == 1U,
			   "an empty state reads everything and withdraws Mark read");
		view.hide();
	}

	auto test_cards_sit_at_their_lines_and_never_overlap() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 600);
		view.show();
		QApplication::processEvents();
		auto conversations = std::vector<conversation>{
			thread("t-late", "src/a.cpp", 40U),
			reply("c-late", "t-late", "author", "Late thread at line 40."),
			thread("t-early", "src/a.cpp", 10U),
			reply("c-early-1", "t-early", "author", "Early thread at line 10."),
			reply("c-early-2", "t-early", "reviewer", "A reply that makes the card tall enough to push the next one down when the lines are close."),
			thread("t-near", "src/a.cpp", 12U),
			reply("c-near", "t-near", "observer", "Two lines beneath the early thread."),
			thread("t-other", "src/b.cpp", 5U),
			reply("c-other", "t-other", "author", "Another file's thread."),
			thread("t-outdated", "src/a.cpp", 20U, false),
			reply("c-outdated", "t-outdated", "author", "Not on this comparison."),
		};
		auto lineY = std::map<std::uint32_t, int>{ { 10U, 100 }, { 12U, 130 }, { 40U, 500 } };
		view.set_line_locator([&lineY](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			const auto found = lineY.find(aLine);
			return found == lineY.end() ? std::nullopt : std::optional{ found->second };
		});
		view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		view.adopt(conversations);
		QApplication::processEvents();

		const auto& cards = view.cards();
		expect(cards.size() == 4U, "this file's threads are shown, the other file's is not");
		expect(cards[0].Id == "t-early" && cards[0].Placed && cards[0].Rect.top() == 100
				   && cards[1].Id == "t-near" && cards[1].Placed
				   && cards[2].Id == "t-late" && cards[2].Placed && cards[2].Rect.top() == 500
				   && cards[3].Id == "t-outdated" && !cards[3].Placed && cards[3].Outdated,
			   "cards sit at their lines in line order, the unplaceable one last");
		expect(cards[1].Rect.top() > 130 && cards[1].Rect.top() >= cards[0].Rect.bottom()
				   && !overlapping(cards),
			   "a card whose line is under the card above sits beneath it instead");
		expect(cards[3].Rect.top() >= cards[2].Rect.bottom(), "the unplaced card follows the last placed one");

		// The comparison scrolls: the lines move, the cards follow.
		lineY = { { 10U, 40 }, { 12U, 70 }, { 40U, 440 } };
		view.relayout();
		expect(view.cards()[0].Rect.top() == 40 && view.cards()[2].Rect.top() == 440 && !overlapping(view.cards()),
			   "relaying against moved lines moves the cards with them");

		// Whole PR: every thread in file order, under its path.
		view.set_scope(qt::cards_scope::whole_pull_request);
		QApplication::processEvents();
		const auto& all = view.cards();
		expect(all.size() == 5U && all[0].Id == "t-early" && all[1].Id == "t-near"
				   && all[2].Id == "t-outdated" && all[3].Id == "t-late" && all[4].Id == "t-other"
				   && !all[0].Placed && !overlapping(all),
			   "whole PR lists every thread in file then line order without placing them");
		view.set_scope(qt::cards_scope::this_file);

		// Gestures: a press makes a thread current and reports it; a press on
		// the action reports the action.
		auto chosen  = std::vector<std::string>{ };
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_thread_dispatch([&chosen](const std::string& aId) { chosen.push_back(aId); });
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		const auto nearCentre = view.cards()[1].Rect.center();
		auto       press      = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ nearCentre }, QPointF{ nearCentre },
											 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(view.viewport(), &press);
		expect(chosen == std::vector<std::string>{ "t-near" } && view.current_thread() == "t-near",
			   "a press on a card makes its thread current and reports it");
		const auto action      = view.cards()[0].Actions.front().second.center();
		auto       pressAction = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ action }, QPointF{ action },
											  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(view.viewport(), &pressAction);
		expect(actions.size() == 1U && actions.front().first == qt::card_action::comment_here
				   && actions.front().second == "t-early",
			   "a press on Comment here reports the action for that thread");
		const auto hit = view.hit_test(view.cards()[2].Rect.center());
		expect(hit.has_value() && hit->first == 2U && !hit->second.has_value(), "hit testing names the card");
		view.set_current_thread("c-late");
		expect(view.current_thread() == "t-late", "a reply's identity names its card");

		// My draft on this comparison is a dashed card at its line.
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{
						   .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 39U },
						   .Body   = "Same question; check the CVar default before publishing.",
					   },
					   comparison);
		QApplication::processEvents();
		const auto draft = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; });
		expect(draft != view.cards().end() && draft->Placed && draft->Line == std::optional<std::uint32_t>{ 40U }
				   && draft->Actions.size() == 3U && draft->Actions[0].first == qt::card_action::publish_draft
				   && draft->Actions[2].first == qt::card_action::delete_draft && !overlapping(view.cards()),
			   "the draft is a card at its one-based line with Publish, Edit and Delete");
		expect(view.post_count() == 5U, "the draft is not counted as a post");
		view.set_draft(std::nullopt, std::nullopt);
		expect(std::ranges::none_of(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; }),
			   "without a draft there is no draft card");
		view.clear();
		expect(view.cards().empty(), "clearing empties the cards");
		view.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_branching_conversation_reads_as_one_card_in_order();
	test_read_state_marks_unread_posts_and_offers_mark_read();
	test_cards_sit_at_their_lines_and_never_overlap();
	std::cout << "conversation-cards-view-tests:pass\n";
	return EXIT_SUCCESS;
}
