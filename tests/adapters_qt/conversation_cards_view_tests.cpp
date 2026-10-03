#include "bpr/qt/conversation_cards_view.hpp"

#include "bpr/core/diff_model.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFile>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QToolButton>
#include <QWheelEvent>

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
		// Glaze discovers these options by their required member names.
		// NOLINTBEGIN(readability-identifier-naming)
		bool validate_skipped             = true;
		bool validate_trailing_whitespace = true;
		bool escape_control_characters    = true;
		// NOLINTEND(readability-identifier-naming)
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
				   && card.Actions.front().first == qt::card_action::reply,
			   "the card is as tall as its posts and offers Reply to its own thread ([[BPR-1133]])");
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
			   "a thread with unread posts offers Mark read after Reply");
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

	// [[BPR-0649]]. Discuss's feed: file order, a header per post with the
	// role and whom it answers, the code excerpt where the feed can read it,
	// Reply / Mark read / Open in code, and the Unread and Unresolved filters.
	// AC-08: the twenty-four-post, three-branch conversation reads in the
	// feed as one card in the projection's order with every reply's answer named.
	auto test_the_feed_heads_posts_excerpts_code_and_filters() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		QApplication::processEvents();
		auto fixture = branching_fixture();
		fixture.push_back(thread("t-resolved", "src/a.cpp", 3U, true, true));
		fixture.push_back(reply("c-resolved", "t-resolved", "someone", "settled"));
		fixture.push_back(thread("t-open", "src/z.cpp", 9U));
		fixture.push_back(reply("c-open", "t-open", "someone", "open"));
		view.set_viewer_login(QStringLiteral("reviewer-one"));
		view.set_pull_request_author(QStringLiteral("reviewer-one"));
		view.set_excerpt_locator([](const QString& aPath, core::diff_side, const std::uint32_t aLine) -> std::optional<QString> {
			if (aPath != QStringLiteral("src/z.cpp")) {
				return std::nullopt;
			}
			return QStringLiteral("line %1\nline %2\nline %3").arg(aLine - 1).arg(aLine).arg(aLine + 1);
		});
		view.adopt(fixture);
		view.set_read_state({ qt::thread_read_state{ .Thread = "t-open", .UnreadPosts = { "c-open" } } });
		view.set_feed(true);
		QApplication::processEvents();
		expect(view.feed() && view.cards().size() == 3U
				   && view.cards()[0].Id == "t-resolved" && view.cards()[2].Id == "t-open",
			   "the feed lists every thread in file order, never placed at a line");
		const auto& branching = view.cards()[1];
		expect(branching.Posts.size() == 24U && branching.Posts.front().Role == QStringLiteral("author")
				   && branching.Posts[1].Role == QStringLiteral("reviewer"),
			   "the pull request's author is headed author and everyone else reviewer");
		auto answered = std::size_t{ 0U };
		for (const auto& post : branching.Posts) {
			answered += post.ReplyingTo.isEmpty() ? 0U : 1U;
		}
		expect(branching.Posts.front().ReplyingTo.isEmpty() && answered == 23U,
			   "every reply but the first names whom it answers");
		expect(branching.Excerpt.isEmpty() && view.cards()[2].Excerpt == QStringLiteral("line 8\nline 9\nline 10"),
			   "a thread on the compared file carries its excerpt; another file's carries none");
		expect(view.cards()[2].Rect.height() > view.cards()[2].Posts.front().Rect.height() + 3 * 18,
			   "the excerpt takes its own room above the posts");
		const auto actionsOf = [](const qt::thread_card& aCard) {
			auto found = std::vector<qt::card_action>{ };
			for (const auto& [action, rect] : aCard.Actions) {
				found.push_back(action);
			}
			return found;
		};
		expect(actionsOf(view.cards()[2]) == std::vector{ qt::card_action::reply, qt::card_action::mark_read, qt::card_action::open_in_code }
				   && actionsOf(view.cards()[0]) == std::vector{ qt::card_action::reply, qt::card_action::open_in_code },
			   "the feed offers Reply, Mark read while unread, and Open in code; Resolve only once offered");
		// [[BPR-0657]]. Once the provider and the server both resolve threads,
		// every thread card offers Resolve, or Unresolve on a resolved one.
		view.set_resolution_offered(true);
		expect(view.resolution_offered() && view.cards()[0].Resolved && !view.cards()[2].Resolved
				   && actionsOf(view.cards()[0]) == std::vector{ qt::card_action::reply, qt::card_action::unresolve, qt::card_action::open_in_code }
				   && actionsOf(view.cards()[2]) == std::vector{ qt::card_action::reply, qt::card_action::mark_read, qt::card_action::resolve, qt::card_action::open_in_code },
			   "offered, a resolved thread's card gains Unresolve and an open one Resolve, before Open in code");
		view.set_resolution_offered(false);
		expect(actionsOf(view.cards()[0]) == std::vector{ qt::card_action::reply, qt::card_action::open_in_code },
			   "withdrawn, the cards offer neither");
		expect(view.thread_count() == 3U && view.unresolved_count() == 2U && view.unread_thread_count() == 1U,
			   "the counts the summary restates");

		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		const auto open  = view.cards()[2].Actions.back().second.center() - QPoint{ 0, view.verticalScrollBar()->value() };
		auto       press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ open }, QPointF{ open },
										Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(view.viewport(), &press);
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::open_in_code, "t-open" } },
			   "a press on Open in code reports it for its thread");

		view.set_filters(qt::discuss_filters{ .UnreadOnly = true });
		QApplication::processEvents();
		expect(view.cards().size() == 1U && view.cards().front().Id == "t-open",
			   "Unread keeps only the threads with a post newer than what I read");
		view.set_filters(qt::discuss_filters{ .UnresolvedOnly = true });
		QApplication::processEvents();
		expect(view.cards().size() == 2U && view.cards().front().Id != "t-resolved",
			   "Unresolved drops the resolved thread");
		view.set_filters(qt::discuss_filters{ });
		view.set_feed(false);
		QApplication::processEvents();
		const auto readActions = view.cards().empty() ? std::vector<qt::card_action>{ } : actionsOf(view.cards()[0]);
		expect(!view.feed() && view.cards().size() == 3U && view.cards()[0].Posts.front().Role.isEmpty()
				   && !readActions.empty() && readActions.front() == qt::card_action::reply
				   && std::ranges::find(readActions, qt::card_action::open_in_code) == readActions.end(),
			   "leaving the feed restores the Read cards: Reply beside the code, and no Open in code");
		view.hide();
	}

	auto test_cached_geometry_matches_a_fresh_view() -> void
	{
		auto projection = std::vector<conversation>{
			thread("first", "src/a.cpp", 1U),
			reply("p1", "first", "author", "Original post"),
			thread("second", "src/a.cpp", 2U),
			reply("p2", "second", "reviewer", "Another post"),
		};
		auto                        state      = std::vector<qt::thread_read_state>{ };
		auto                        draft      = std::optional<core::review_comment_draft>{ };
		const auto                  comparison = core::file_comparison_digest{ };
		auto                        width      = 420;
		qt::conversation_cards_view cached;
		auto                        font      = cached.font();
		const auto                  configure = [&](qt::conversation_cards_view& aView) {
			aView.resize(width, 900);
			aView.setFont(font);
			aView.set_resolution_offered(true);
			aView.set_read_state(state);
			aView.set_draft(draft, comparison);
			aView.adopt(projection);
			aView.show();
		};
		const auto verify = [&] {
			configure(cached);
			qt::conversation_cards_view fresh;
			configure(fresh);
			QApplication::processEvents();
			expect(cached.cards().size() == fresh.cards().size(), "cached and fresh views have the same card count");
			for (std::size_t index = 0U; index < fresh.cards().size(); ++index) {
				const auto& actual   = cached.cards()[index];
				const auto& expected = fresh.cards()[index];
				expect(actual.Id == expected.Id && actual.Rect == expected.Rect && actual.Actions == expected.Actions
						   && actual.Posts.size() == expected.Posts.size(),
					   "cached card geometry and actions match an independently constructed view");
				for (std::size_t post = 0U; post < expected.Posts.size(); ++post) {
					expect(actual.Posts[post].Id == expected.Posts[post].Id && actual.Posts[post].Body == expected.Posts[post].Body
							   && actual.Posts[post].Rect == expected.Posts[post].Rect,
						   "cached post content and hit geometry match fresh measurement");
				}
			}
		};
		verify();
		projection[1].Body = std::string(350U, 'W');
		verify();
		projection.push_back(reply("new-reply", "first", "reviewer", "A new reply takes another row."));
		verify();
		state = { qt::thread_read_state{ .Thread = "first", .UnreadPosts = { "p1" } } };
		verify();
		projection.front().Resolved = true;
		verify();
		draft = core::review_comment_draft{ .Anchor = core::source_line_ref{ .Comparison = comparison }, .Body = "Draft text" };
		verify();
		draft->Body = std::string(200U, 'M');
		verify();
		width = 300;
		verify();
		font.setPointSizeF(font.pointSizeF() + 2.0);
		verify();
		auto styleChange = QEvent{ QEvent::StyleChange };
		QApplication::sendEvent(&cached, &styleChange);
		verify();
		std::erase_if(projection, [](const conversation& aNode) { return aNode.Id == "second" || aNode.Id == "p2"; });
		verify();
		draft.reset();
		state.clear();
		verify();
	}

	auto test_feed_geometry_refresh_updates_excerpts() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		auto excerpt = QStringLiteral("old code");
		view.set_excerpt_locator([&excerpt](const QString&, core::diff_side, std::uint32_t) -> std::optional<QString> {
			return excerpt.isEmpty() ? std::nullopt : std::optional{ excerpt };
		});
		view.set_feed(true);
		view.adopt({ thread("t", "src/a.cpp", 1U), reply("p", "t", "author", "post") });
		view.set_current_thread("t");
		excerpt = QStringLiteral("new code\nsecond line\nthird line");
		view.refresh_geometry();
		const auto changed = view.cards().front();
		view.relayout();
		expect(changed.Excerpt == excerpt && changed.Rect == view.cards().front().Rect
				   && changed.Posts.front().Rect == view.cards().front().Posts.front().Rect
				   && changed.Actions == view.cards().front().Actions && view.current_thread() == "t",
			   "changed feed excerpts preserve selection and match full text and action geometry");
		excerpt.clear();
		view.refresh_geometry();
		expect(view.cards().front().Excerpt.isEmpty() && view.cards().front().Rect.height() < changed.Rect.height(),
			   "unavailable comparison removes the previous excerpt and its height");
	}

	// [[BPR-1270]]. The excerpt is measured and set in the code font the view
	// was handed, not in a family the view names.
	auto test_the_excerpt_follows_the_supplied_code_font() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.set_excerpt_locator([](const QString&, core::diff_side, std::uint32_t) -> std::optional<QString> {
			return QStringLiteral("first line\nsecond line\nthird line");
		});
		view.set_feed(true);
		view.adopt({ thread("t", "src/a.cpp", 2U), reply("p", "t", "author", "post") });
		const auto before = view.cards().front().Rect.height();
		auto       larger = view.code_font();
		larger.setPointSizeF(larger.pointSizeF() * 2.0);
		view.set_code_font(larger);
		expect(view.code_font() == larger, "the view keeps the code font it was handed");
		expect(view.cards().front().Rect.height() > before, "a larger code font lays the excerpt out taller at once");
		const auto measured = view.measurements();
		view.set_code_font(larger);
		expect(view.measurements() == measured, "the same code font again measures nothing");
	}

	void test_compact_labels_keep_independent_geometry()
	{
		qt::conversation_cards_view view;
		view.resize(280, 600);
		view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		view.set_compact(true);
		view.set_feed(true);
		view.set_resolution_offered(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		view.show();
		QApplication::processEvents();
		const auto projection = std::vector<conversation>{
			thread("first", "src/a.cpp", 1U),
			reply("one", "first", "same_author", "first body"),
			thread("second", "src/a.cpp", 2U),
			reply("two", "second", "same_author", "different body"),
			thread("third", "src/a.cpp", 3U, true, true),
			reply("three", "third", "a_much_longer_author_that_wraps", "third body"),
		};
		view.adopt(projection);
		for (const auto width : { 280, 190, 340 }) {
			view.resize(width, 600);
			QApplication::processEvents();
			for (std::size_t index = 0; index < 3U; ++index) {
				qt::conversation_cards_view single;
				single.resize(width, 600);
				single.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
				single.set_compact(true);
				single.set_feed(true);
				single.set_resolution_offered(true);
				single.set_scope(qt::cards_scope::whole_pull_request);
				single.show();
				QApplication::processEvents();
				single.adopt({ projection[index * 2U], projection[index * 2U + 1U] });
				const auto& together = view.cards()[index];
				const auto& alone    = single.cards().front();
				expect(together.Id == alone.Id && together.Rect.size() == alone.Rect.size()
						   && together.Actions.size() == alone.Actions.size(),
					   "repeated compact labels retain each card's independent geometry");
				for (std::size_t action = 0; action < alone.Actions.size(); ++action) {
					expect(together.Actions[action].first == alone.Actions[action].first
							   && together.Actions[action].second.translated(-together.Rect.topLeft())
								   == alone.Actions[action].second.translated(-alone.Rect.topLeft()),
						   "compact card actions keep exact local rectangles");
				}
			}
		}
	}

	auto test_unchanged_refresh_retains_measured_cards() -> void
	{
		qt::conversation_cards_view view;
		view.resize(400, 600);
		view.show();
		QApplication::processEvents();
		auto projection = std::vector<conversation>{ };
		for (auto index = 0; index < 1000; ++index) {
			const auto id = std::to_string(index);
			projection.push_back(thread("t" + id, "src/a.cpp", static_cast<std::uint32_t>(index + 1)));
			projection.push_back(reply("p" + id, "t" + id, "author", "A retained post whose text measurement must survive refresh."));
		}
		view.adopt(projection, core::file_identity_of("src/a.cpp"));
		view.set_current_thread("p10");
		QApplication::processEvents();
		const auto  layouts = view.layouts();
		const auto* posts   = view.cards().front().Posts.data();
		for (auto refresh = 0; refresh < 10; ++refresh) {
			view.adopt(projection);
			view.refresh_geometry();
		}
		expect(view.layouts() == layouts && view.cards().front().Posts.data() == posts
				   && view.post_count() == 1000U && view.current_thread() == "t10",
			   "unchanged thousand-thread refreshes retain measurements, post storage and selection");
		const auto measurements = view.measurements();
		for (auto refresh = 0; refresh < 10; ++refresh) {
			view.set_anchor_file(std::nullopt);
			view.set_anchor_file(core::file_identity_of("src/a.cpp"));
		}
		expect(view.measurements() == measurements && view.post_count() == 1000U && view.current_thread() == "t10",
			   "temporary anchor clear and restore reuse all thousand retained card measurements");
		projection.front().OriginalLine = 99U;
		view.adopt(projection);
		expect(view.measurements() == measurements, "a changed snapshot with identical measurement inputs shapes no text");
		const auto layoutsBeforeContent = view.layouts();
		projection[1].Body              = "Changed post";
		view.adopt(projection);
		expect(view.layouts() == layoutsBeforeContent + 1U && view.cards().front().Posts.front().Body == QStringLiteral("Changed post"),
			   "a changed post rebuilds its visible content");
		expect(view.measurements() == measurements + 1U, "one changed post remeasures exactly its one card");
		const auto removedPost = projection.back();
		projection.pop_back();
		const auto removedRoot = projection.back();
		projection.pop_back();
		view.adopt(projection);
		const auto beforeRestore = view.measurements();
		projection.push_back(removedRoot);
		projection.push_back(removedPost);
		view.adopt(projection);
		expect(view.measurements() == beforeRestore + 1U, "a removed root releases its cache entry before the same identity returns");
		view.set_viewer_login(QStringLiteral("author"));
		view.adopt(projection);
		expect(view.cards().front().Posts.front().Mine, "viewer changes survive an unchanged projection");
		const auto beforeClear = view.measurements();
		view.clear();
		view.adopt(projection);
		expect(view.measurements() > beforeClear, "authority clear releases the previous measurement cache");
		const auto beforeFont = view.measurements();
		auto       larger     = view.font();
		larger.setPointSizeF(larger.pointSizeF() + 2.0);
		view.setFont(larger);
		expect(view.measurements() > beforeFont, "font changes invalidate cached text measurements");
	}

	auto test_alternating_widths_keep_both_widths_measured() -> void
	{
		// [[BPR-0673]]. Widening a large review's drawer shows the scroll bar
		// and narrowing it to one file hides it again, so the content width
		// alternates between two values. Each width keeps its measurements.
		qt::conversation_cards_view view;
		view.resize(400, 600);
		view.show();
		QApplication::processEvents();
		auto projection = std::vector<conversation>{ };
		for (auto index = 0; index < 1000; ++index) {
			const auto id = std::to_string(index);
			projection.push_back(thread("t" + id, "src/a.cpp", static_cast<std::uint32_t>(index + 1)));
			projection.push_back(reply("p" + id, "t" + id, "author", "A retained post whose text measurement must survive a width change."));
		}
		view.adopt(projection, std::nullopt);
		QApplication::processEvents();
		const auto wide = view.cards();
		view.resize(320, 600);
		QApplication::processEvents();
		const auto narrow       = view.cards();
		const auto measurements = view.measurements();
		for (auto cycle = 0; cycle < 10; ++cycle) {
			view.resize(400, 600);
			view.set_anchor_file(core::file_identity_of("src/a.cpp"));
			view.set_anchor_file(std::nullopt);
			view.resize(320, 600);
			view.set_anchor_file(core::file_identity_of("src/a.cpp"));
			view.set_anchor_file(std::nullopt);
		}
		QApplication::processEvents();
		expect(view.measurements() == measurements && view.cards().size() == 1000U,
			   "alternating between two widths re-measures no card at either width");
		const auto sameRects = [](const std::vector<qt::thread_card>& aLeft, const std::vector<qt::thread_card>& aRight) {
			return std::ranges::equal(aLeft, aRight, [](const qt::thread_card& aA, const qt::thread_card& aB) {
				return aA.Id == aB.Id && aA.Rect == aB.Rect && aA.Actions == aB.Actions
					&& std::ranges::equal(aA.Posts, aB.Posts, { }, &qt::card_post::Rect, &qt::card_post::Rect);
			});
		};
		expect(sameRects(view.cards(), narrow), "the narrow width's retained measurements lay the cards out as before");
		view.resize(400, 600);
		QApplication::processEvents();
		expect(view.measurements() == measurements && sameRects(view.cards(), wide),
			   "the wide width's retained measurements lay the cards out as before");
		view.resize(360, 600);
		QApplication::processEvents();
		expect(view.measurements() == measurements + 1000U, "a third width measures its cards afresh");
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
		lineY                          = { { 10U, 40 }, { 12U, 70 }, { 40U, 440 } };
		const auto layoutsBeforeScroll = view.layouts();
		view.refresh_geometry();
		expect(view.layouts() == layoutsBeforeScroll, "scrolling retains the forest and measured text");
		expect(view.cards()[0].Rect.top() == 40 && view.cards()[2].Rect.top() == 440 && !overlapping(view.cards()),
			   "relaying against moved lines moves the cards with them");

		const auto movedPosts   = view.cards()[0].Posts;
		const auto movedActions = view.cards()[0].Actions;
		view.relayout();
		expect(view.cards()[0].Posts.front().Rect == movedPosts.front().Rect
				   && view.cards()[0].Actions == movedActions,
			   "cached placement has the same post and action hit targets as a full layout");
		lineY.clear();
		view.refresh_geometry();
		expect(std::ranges::none_of(view.cards(), [](const qt::thread_card& aCard) { return aCard.Placed; }),
			   "invalidated comparison geometry withdraws every placed card");
		lineY = { { 10U, 40 }, { 12U, 70 }, { 40U, 440 } };
		view.refresh_geometry();

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
		view.set_thread_dispatch([&chosen](const std::string& aId, const bool aOpenDetail) {
			if (aOpenDetail) {
				chosen.push_back(aId);
			}
		});
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		const auto nearCentre = view.cards()[1].Rect.center();
		auto       press      = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ nearCentre }, QPointF{ nearCentre },
											 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(view.viewport(), &press);
		expect(chosen == std::vector<std::string>{ "t-near" } && view.current_thread() == "t-near",
			   "a press on a card makes its thread current and reports it");
		auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QApplication::sendEvent(&view, &down);
		const auto afterArrow = view.current_thread();
		expect(chosen == std::vector<std::string>{ "t-near" }
				   && !afterArrow.empty() && afterArrow != "t-near",
			   "an arrow changes index selection without explicitly opening detail");
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(&view, &enter);
		expect(chosen == std::vector<std::string>{ "t-near", afterArrow },
			   "Return explicitly opens the currently selected index entry");
		const auto action      = view.cards()[0].Actions.front().second.center();
		auto       pressAction = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ action }, QPointF{ action },
											  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(view.viewport(), &pressAction);
		expect(actions.size() == 1U && actions.front().first == qt::card_action::reply
				   && actions.front().second == "t-early",
			   "a press on a card's Reply reports the action for that card's own thread");
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
		lineY = { { 10U, 200 }, { 12U, 200 }, { 40U, 200 } };
		view.refresh_geometry();
		const auto cached = view.cards();
		view.relayout();
		for (std::size_t index = 0U; index < cached.size(); ++index) {
			expect(cached[index].Id == view.cards()[index].Id && cached[index].Draft == view.cards()[index].Draft
					   && cached[index].Rect == view.cards()[index].Rect && cached[index].Actions == view.cards()[index].Actions,
				   "coincident line positions keep the original projection order and draft action geometry");
		}
		view.set_draft(std::nullopt, std::nullopt);
		expect(std::ranges::none_of(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; }),
			   "without a draft there is no draft card");
		view.clear();
		expect(view.cards().empty(), "clearing empties the cards");
		view.hide();
	}

	// [[BPR-1029]]. The window's dispatch relays the cards out, or makes
	// another thread current, before it reads the identity it was handed. That
	// identity must still name the card that was pressed, not whichever card
	// the relayout rebuilt in its slot.
	auto test_a_dispatch_that_relays_out_still_names_the_pressed_card() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.show();
		QApplication::processEvents();
		view.set_feed(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		const auto first    = std::string{ "review-thread:pressed-first-v1" };
		const auto second   = std::string{ "review-thread:pressed-second-v1" };
		const auto arranged = [&view, &first, &second](const std::string& aFirstPath, const std::string& aSecondPath) {
			view.adopt({ thread(first, aFirstPath, 1U), reply("review-comment:first-v1", first, "someone", "first"),
						 thread(second, aSecondPath, 1U), reply("review-comment:second-v1", second, "someone", "second") });
		};
		arranged("src/a.cpp", "src/z.cpp");
		QApplication::processEvents();
		expect(view.cards().size() == 2U && view.cards()[0].Id == first && !view.cards()[0].Actions.empty(),
			   "the thread to press comes first in file order and offers an action");
		const auto pressAt = [&view](const QPoint aContent) {
			const auto point = aContent - QPoint{ 0, view.verticalScrollBar()->value() };
			auto       press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ point }, QPointF{ point },
											Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(view.viewport(), &press);
		};

		auto reported = std::vector<std::string>{ };
		view.set_action_dispatch([&reported, &arranged](const qt::card_action, const std::string& aId) {
			arranged("src/z.cpp", "src/a.cpp");
			reported.push_back(aId);
		});
		pressAt(view.cards()[0].Actions.front().second.center());
		expect(view.cards().size() == 2U && view.cards()[0].Id == second
				   && reported == std::vector<std::string>{ first },
			   "an action whose dispatch relays the cards out still names the pressed thread");

		view.set_thread_dispatch([&reported, &view, &first](const std::string& aId, const bool) {
			view.set_current_thread(first);
			reported.push_back(aId);
		});
		pressAt(view.cards()[0].Rect.topLeft() + QPoint{ 2, 2 });
		expect(reported == std::vector<std::string>{ first, second },
			   "a press whose dispatch makes another thread current still names the pressed thread");
		view.hide();
	}
} // namespace

namespace {
	// [[BPR-0972]]. One thread's read state reaches the cards as that thread
	// alone: a thread whose card is not shown lays nothing out, a shown one
	// lays out its file's cards, and both agree with the complete state.
	void test_one_thread_read_state_matches_the_complete_state()
	{
		for (const auto threads : { 32U, 4096U }) {
			auto conversations = std::vector<conversation>{ };
			auto state         = std::vector<qt::thread_read_state>{ };
			for (unsigned index = 0; index < threads; ++index) {
				const auto id = "t" + std::to_string(index);
				conversations.push_back(thread(id, "src/file" + std::to_string(index % (threads / 4U)) + ".cpp", index + 1U));
				conversations.push_back(reply(id + "-c", id, "someone", "a remark"));
				state.push_back(qt::thread_read_state{ .Thread = id, .UnreadPosts = { id + "-c" } });
			}
			qt::conversation_cards_view view;
			view.resize(360, 900);
			view.adopt(conversations, core::file_identity_of("src/file0.cpp"));
			view.set_read_state(state);
			static_cast<void>(view.unread_threads_in_file_order());
			const auto layouts = view.layouts();
			const auto visited = view.layout_nodes_visited();
			// A thread in another file: read, with nothing shown to lay out.
			const auto hidden = view.set_thread_read_state("t1", { });
			expect(hidden == std::vector<std::string>{ "t1-c" } && view.layouts() == layouts,
				   "a hidden thread's read state lays nothing out and returns its previous posts");
			// A thread of the shown file: read, and only that file laid out.
			static_cast<void>(view.set_thread_read_state("t0", { }));
			std::cout << "cards-read-one threads=" << threads << " layouts=" << view.layouts() - layouts
					  << " nodes_visited=" << view.layout_nodes_visited() - visited << '\n';
			expect(view.layouts() == layouts + 1U && view.layout_nodes_visited() - visited == 8U,
				   "a shown thread's read state lays out its own file's conversations only");
			state[0].UnreadPosts.clear();
			state[1].UnreadPosts.clear();
			auto complete = qt::conversation_cards_view{ };
			complete.resize(360, 900);
			complete.adopt(conversations, core::file_identity_of("src/file0.cpp"));
			complete.set_read_state(state);
			expect(view.unread_threads_in_file_order() == complete.unread_threads_in_file_order()
					   && view.unread_post_count() == complete.unread_post_count()
					   && view.unread_marked_lines(core::diff_side::right) == complete.unread_marked_lines(core::diff_side::right),
				   "one thread at a time agrees with the complete read state");
		}
	}
	void test_card_wheel_moves_readable_lines_without_changing_selection()
	{
		qt::conversation_cards_view view;
		view.resize(360, 180);
		view.set_compact(true);
		auto data = std::vector<conversation>{ };
		for (auto i = 0; i < 12; ++i) {
			data.push_back(thread("t" + std::to_string(i), "src/file.cpp", static_cast<std::uint32_t>(i + 1)));
		}
		view.adopt(data);
		view.set_current_thread("t0");
		view.show();
		QApplication::processEvents();
		for (const auto points : { 13, 20 }) {
			auto font = view.font();
			font.setPointSize(points);
			view.setFont(font);
			QApplication::processEvents();
			auto* bar = view.verticalScrollBar();
			bar->setValue(0);
			QWheelEvent wheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
			QApplication::sendEvent(view.viewport(), &wheel);
			expect(wheel.isAccepted() && bar->value() >= view.fontMetrics().lineSpacing()
					   && view.current_thread() == "t0",
				   "one wheel detent moves readable card lines while retaining the selected thread, including after font changes");
		}
	}

	void test_thread_menus_and_native_defect_badges()
	{
		qt::conversation_cards_view view;
		view.resize(430, 420);
		view.show();
		view.set_action_dispatch([](auto, const auto&) { });
		view.set_context_menu_builder([](QMenu& aMenu, const std::string&) { aMenu.addAction(QStringLiteral("Classify as defect…")); });
		view.adopt({ thread("a", "src/a.cpp", 10), reply("a-1", "a", "reviewer", "One thread"), thread("b", "src/a.cpp", 20) });
		view.set_defect_badges({ { "a", { QStringLiteral("[open] minor · correctness"), QStringLiteral("Nobody has addressed this yet. Shared context available.") } } });
		QApplication::processEvents();
		const auto buttons = view.findChildren<QToolButton*>(QStringLiteral("threadDefectBadge"));
		expect(buttons.size() == 1U && buttons.front()->isVisible() && buttons.front()->property("threadIdentity") == QStringLiteral("a"), "only the classified visible thread has a native badge button");
		auto* accessible = QAccessible::queryAccessibleInterface(buttons.front());
		expect(accessible && accessible->role() == QAccessible::Button
				   && accessible->text(QAccessible::Name).contains(QStringLiteral("correctness"))
				   && accessible->text(QAccessible::Description).contains(QStringLiteral("Shared context")),
			   "badge accessibility carries the classification and explained context");
		for (const auto& identity : { std::string{ "a" }, std::string{ "b" } }) {
			auto* menu = view.thread_menu(identity);
			expect(menu && menu->actions().size() == 6 && menu->actions().front()->text() == QStringLiteral("Reply")
					   && menu->actions().back()->text() == QStringLiteral("Classify as defect…"),
				   "every thread offers existing actions and the host-supplied defect actions");
			expect(!menu->actions()[1]->isEnabled() && !menu->actions()[1]->toolTip().isEmpty(), "unavailable existing actions explain their refusal");
			delete menu;
		}
		view.set_current_thread("a");
		for (const auto key : { Qt::Key_Menu, Qt::Key_F10 }) {
			QKeyEvent event{ QEvent::KeyPress, key, key == Qt::Key_F10 ? Qt::ShiftModifier : Qt::NoModifier };
			QApplication::sendEvent(&view, &event);
			QApplication::processEvents();
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			expect(menu && menu->isVisible(), "context-menu key and Shift+F10 open the selected thread actions");
			menu->close();
			QApplication::processEvents();
		}
		const auto        point = view.cards().front().Rect.center();
		QContextMenuEvent rightClick{ QContextMenuEvent::Mouse, point, view.viewport()->mapToGlobal(point) };
		QApplication::sendEvent(view.viewport(), &rightClick);
		QApplication::processEvents();
		auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		expect(menu && menu->isVisible(), "right-click opens the same thread menu");
		menu->close();
		QApplication::processEvents();
		const auto measured = view.measurements();
		view.set_defect_badges({ { "a", { QStringLiteral("[open] minor · correctness"), QStringLiteral("Nobody has addressed this yet. Shared context available.") } } });
		expect(view.measurements() == measured, "unchanged badges reuse retained measurements");
		view.set_defect_badges({ });
		QApplication::processEvents();
		expect(buttons.front()->isHidden(), "withdrawn badge projection leaves no stale native action");
	}

} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_card_wheel_moves_readable_lines_without_changing_selection();
	test_thread_menus_and_native_defect_badges();
	test_one_thread_read_state_matches_the_complete_state();
	test_branching_conversation_reads_as_one_card_in_order();
	test_read_state_marks_unread_posts_and_offers_mark_read();
	test_the_feed_heads_posts_excerpts_code_and_filters();
	test_cards_sit_at_their_lines_and_never_overlap();
	test_unchanged_refresh_retains_measured_cards();
	test_compact_labels_keep_independent_geometry();
	test_alternating_widths_keep_both_widths_measured();
	test_feed_geometry_refresh_updates_excerpts();
	test_the_excerpt_follows_the_supplied_code_font();
	test_cached_geometry_matches_a_fresh_view();
	test_a_dispatch_that_relays_out_still_names_the_pressed_card();
	std::cout << "conversation-cards-view-tests:pass\n";
	return EXIT_SUCCESS;
}
