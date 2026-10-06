#include "bpr/qt/conversation_cards_view.hpp"

#include "bpr/core/diff_model.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextEdit>
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

	/// \brief A press at a viewport point of the notes.
	void press_at(qt::conversation_cards_view& aView, const QPoint aPoint)
	{
		auto press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aPoint }, aView.viewport()->mapToGlobal(QPointF{ aPoint }),
								  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aView.viewport(), &press);
		QApplication::processEvents();
	}

	void key(QWidget& aTarget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto event = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(&aTarget, &event);
		QApplication::processEvents();
	}

	[[nodiscard]] auto button(const qt::conversation_cards_view& aView, const char* aName) -> QAbstractButton*
	{
		return aView.open_card() == nullptr ? nullptr : aView.open_card()->findChild<QAbstractButton*>(QString::fromLatin1(aName));
	}

	/// \brief AC-08 and BPR-1290:depth. The twenty-four-post, three-branch
	/// conversation is one note that opens in place to every post in posting
	/// order, each reply to anything but the first post naming whom it answers.
	auto test_branching_conversation_reads_in_place() -> void
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
			   "the twenty-four posts of the three branches are one note");
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
				   && depths.at("review-comment:branching-thread-v1-cc03-aside") == 5U,
			   "every branch's depth is counted from the root");
		auto answering = std::size_t{ 0U };
		for (const auto& post : card.Posts) {
			answering += post.ReplyingTo.isEmpty() ? 0U : 1U;
		}
		expect(card.Posts.front().ReplyingTo.isEmpty() && card.Posts[1].ReplyingTo.isEmpty() && card.Posts[2].ReplyingTo == QStringLiteral("author-two")
				   && answering == 20U,
			   "a reply that answers anything but the first post names whom it answers; the first post's direct replies do not");
		expect(card.Posts.front().Mine && !card.Posts[1].Mine, "my posts are marked mine by login");
		const auto closedHeight = card.Rect.height();
		view.open_thread(card.Id);
		QApplication::processEvents();
		auto* const open = view.open_card();
		expect(open != nullptr && open->findChildren<QWidget*>(QStringLiteral("threadPost")).size() == 24
				   && view.cards().front().Rect.height() > 24 * 20 && view.cards().front().Rect.height() > closedHeight * 5,
			   "the open thread shows all twenty-four posts in place, as tall as they are");
		auto namedAnswer = false;
		for (auto* const post : open->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
			namedAnswer = namedAnswer || post->accessibleName().contains(QStringLiteral("reviewer"));
		}
		expect(namedAnswer, "each post is named for screen readers by its author and mark");
		expect(view.marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 41U }
				   && view.marked_lines(core::diff_side::left).empty(),
			   "the thread's line is marked, zero-based, on its side");
		view.close_thread();
		expect(view.open_card() == nullptr || !view.open_card()->isVisible(), "closing the thread folds it back to its note");
		view.hide();
	}

	// [[BPR-0656]]. The read state the ledger host decided: unread posts carry
	// the dot and count, the open thread offers Mark read, the first post
	// unread makes the thread new for the gutter, and the strip walks the
	// unread threads of the whole pull request in file order.
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
		view.open_thread(root);
		QApplication::processEvents();
		expect(view.unread_post_count() == 0U && view.unread_threads_in_file_order().empty()
				   && button(view, "threadMarkRead") != nullptr && !button(view, "threadMarkRead")->isEnabled(),
			   "before any read state nothing is unread and Mark read has nothing to mark");

		view.set_read_state({
			qt::thread_read_state{ .Thread = root, .UnreadPosts = { "review-comment:branching-thread-v1-c01", "review-comment:branching-thread-v1-a03" } },
			qt::thread_read_state{ .Thread = "t-after", .UnreadPosts = { "c-after" } },
		});
		QApplication::processEvents();
		const auto rootCard = std::ranges::find(view.cards(), root, &qt::thread_card::Id);
		expect(rootCard != view.cards().end() && rootCard->UnreadCount == 2U && rootCard->RootUnread
				   && rootCard->Posts.front().Unread && !rootCard->Posts[1].Unread,
			   "the unread posts are the ones the state names, and the first post unread makes the thread new");
		expect(view.unread_post_count() == 3U, "the count is over every note");
		expect(view.open_card()->findChild<QWidget*>(QStringLiteral("threadNewRule")) != nullptr,
			   "the open thread rules off where its new posts start");
		expect(view.unread_threads_in_file_order() == std::vector<std::string>{ root, "t-after" },
			   "the unread threads of the whole pull request come in file order, the read one absent");
		expect(view.unread_marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 4U, 41U },
			   "the lines of the new threads are the gutter's blue marks");
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		auto* const markRead = button(view, "threadMarkRead");
		expect(markRead != nullptr && markRead->isEnabled() && markRead->toolTip() == QStringLiteral("Mark 2 new as read"),
			   "with unread posts the open thread offers Mark read and says how many");
		markRead->click();
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::mark_read, root } },
			   "Mark read reports the action for its thread");
		view.set_read_state({ });
		QApplication::processEvents();
		expect(view.unread_post_count() == 0U && !button(view, "threadMarkRead")->isEnabled()
				   && view.open_card()->findChild<QWidget*>(QStringLiteral("threadNewRule")) == nullptr,
			   "an empty state reads everything, withdraws Mark read and the New rule");
		view.hide();
	}

	// [[BPR-0649]], [[BPR-1290]]. Discuss: every thread in file order under its
	// path, each opening in place with its code excerpt and Open in code, the
	// Unread and Unresolved filters, and Resolve once offered.
	auto test_discuss_lists_threads_that_open_in_place() -> void
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
		view.set_scope(qt::cards_scope::whole_pull_request);
		QApplication::processEvents();
		expect(view.feed() && view.cards().size() == 3U
				   && view.cards()[0].Id == "t-resolved" && view.cards()[2].Id == "t-open"
				   && std::ranges::none_of(view.cards(), &qt::thread_card::Placed),
			   "Discuss lists every thread in file order, never placed at a line");
		expect(view.cards()[1].Posts.front().Role == QStringLiteral("author") && view.cards()[1].Posts[1].Role == QStringLiteral("reviewer"),
			   "the pull request's author is headed author and everyone else reviewer");
		expect(view.cards()[2].Excerpt == QStringLiteral("line 8\nline 9\nline 10") && view.cards()[1].Excerpt.isEmpty(),
			   "a thread on the compared file carries its excerpt; another file's carries none");
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&actions](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
		});
		press_at(view, view.note_rect("t-open").center());
		expect(view.open_thread_id() == "t-open" && view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt")) != nullptr,
			   "a press opens the thread in place with its code excerpt above its posts");
		expect(button(view, "threadResolve") == nullptr, "Resolve is not offered before the provider and server both resolve threads");
		button(view, "threadShowLine")->click();
		expect(actions == std::vector<std::pair<qt::card_action, std::string>>{ { qt::card_action::open_in_code, "t-open" } },
			   "in Discuss the line button opens the thread in the code");
		view.set_resolution_offered(true);
		QApplication::processEvents();
		expect(button(view, "threadResolve") != nullptr && button(view, "threadResolve")->toolTip() == QStringLiteral("Resolve"),
			   "offered, an open thread gains Resolve");
		view.open_thread("t-resolved");
		QApplication::processEvents();
		expect(button(view, "threadResolve") != nullptr && button(view, "threadResolve")->toolTip() == QStringLiteral("Unresolve")
				   && view.open_card()->findChild<QWidget*>(QStringLiteral("threadState")) != nullptr,
			   "a resolved thread offers Unresolve and says it is resolved");
		expect(view.thread_count() == 3U && view.unresolved_count() == 2U && view.unread_thread_count() == 1U,
			   "the counts the summary restates");
		view.set_filters(qt::discuss_filters{ .UnreadOnly = true });
		QApplication::processEvents();
		expect(view.cards().size() == 2U && view.cards()[1].Id == "t-open" && view.cards()[0].Id == "t-resolved",
			   "Unread keeps only the threads with new posts, and never the open one away");
		view.close_thread();
		expect(view.cards().size() == 1U && view.cards().front().Id == "t-open",
			   "closed, a thread without new posts is filtered out like any other");
		view.set_filters(qt::discuss_filters{ .UnresolvedOnly = true });
		QApplication::processEvents();
		expect(view.cards().size() == 2U && std::ranges::none_of(view.cards(), &qt::thread_card::Resolved),
			   "Unresolved drops the resolved thread");
		view.set_filters(qt::discuss_filters{ });
		view.set_feed(false);
		view.set_scope(qt::cards_scope::this_file);
		QApplication::processEvents();
		expect(!view.feed() && view.cards().size() == 3U && view.cards()[0].Excerpt.isEmpty(),
			   "beside the code the notes carry no excerpt");
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
			expect(cached.cards().size() == fresh.cards().size(), "cached and fresh views have the same note count");
			for (std::size_t index = 0U; index < fresh.cards().size(); ++index) {
				const auto& actual   = cached.cards()[index];
				const auto& expected = fresh.cards()[index];
				expect(actual.Id == expected.Id && actual.Rect == expected.Rect && actual.Posts.size() == expected.Posts.size(),
					   "cached note geometry matches an independently constructed view");
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

	auto test_discuss_geometry_refresh_updates_excerpts() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		auto excerpt = QStringLiteral("old code");
		view.set_excerpt_locator([&excerpt](const QString&, core::diff_side, std::uint32_t) -> std::optional<QString> {
			return excerpt.isEmpty() ? std::nullopt : std::optional{ excerpt };
		});
		view.set_feed(true);
		view.adopt({ thread("t", "src/a.cpp", 1U), reply("p", "t", "author", "post") });
		view.open_thread("t");
		excerpt = QStringLiteral("new code\nsecond line\nthird line");
		view.refresh_geometry();
		auto* label = view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt"));
		expect(view.cards().front().Excerpt == excerpt && label != nullptr && label->text() == excerpt && view.current_thread() == "t",
			   "a comparison frame refreshes the open thread's excerpt and keeps it current");
		excerpt.clear();
		view.refresh_geometry();
		expect(view.cards().front().Excerpt.isEmpty() && view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt")) == nullptr,
			   "an unavailable comparison removes the excerpt");
	}

	// [[BPR-1270]]. The excerpt is set in the code font the view was handed.
	auto test_the_excerpt_follows_the_supplied_code_font() -> void
	{
		qt::conversation_cards_view view;
		view.resize(420, 900);
		view.set_excerpt_locator([](const QString&, core::diff_side, std::uint32_t) -> std::optional<QString> {
			return QStringLiteral("first line\nsecond line\nthird line");
		});
		view.set_feed(true);
		view.adopt({ thread("t", "src/a.cpp", 2U), reply("p", "t", "author", "post") });
		view.open_thread("t");
		const auto before = view.cards().front().Rect.height();
		auto       larger = view.code_font();
		larger.setPointSizeF(larger.pointSizeF() * 2.0);
		view.set_code_font(larger);
		expect(view.code_font() == larger, "the view keeps the code font it was handed");
		expect(view.open_card()->findChild<QLabel*>(QStringLiteral("threadExcerpt"))->font().pointSizeF() == larger.pointSizeF()
				   && view.cards().front().Rect.height() > before,
			   "a larger code font lays the excerpt out taller at once");
	}

	auto test_unchanged_refresh_retains_measured_notes() -> void
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
			   "temporary anchor clear and restore reuse all thousand retained note measurements");
		projection.front().OriginalLine = 99U;
		view.adopt(projection);
		expect(view.measurements() == measurements, "a changed snapshot with identical measurement inputs shapes no text");
		const auto layoutsBeforeContent = view.layouts();
		projection[1].Body              = "Changed post";
		view.adopt(projection);
		expect(view.layouts() == layoutsBeforeContent + 1U && view.cards().front().Posts.front().Body == QStringLiteral("Changed post"),
			   "a changed post rebuilds its visible content");
		expect(view.measurements() == measurements + 1U, "one changed post remeasures exactly its one note");
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
		// [[BPR-0673]]. Each of two alternating widths keeps its measurements.
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
		view.resize(320, 600);
		QApplication::processEvents();
		const auto measurements = view.measurements();
		for (auto cycle = 0; cycle < 10; ++cycle) {
			view.resize(400, 600);
			view.resize(320, 600);
		}
		QApplication::processEvents();
		expect(view.measurements() == measurements && view.cards().size() == 1000U,
			   "alternating between two widths re-measures no note at either width");
		view.resize(360, 600);
		QApplication::processEvents();
		expect(view.measurements() == measurements + 1000U, "a third width measures its notes afresh");
	}

	/// \brief [[BPR-1290]]. Notes hang from their lines; one open thread is
	/// pinned exactly at its line and the others make room above and below.
	auto test_notes_hang_from_their_lines_and_one_opens_pinned() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 600);
		view.show();
		QApplication::processEvents();
		const auto conversations = std::vector<conversation>{
			thread("t-late", "src/a.cpp", 40U),
			reply("c-late", "t-late", "author", "Late thread at line 40."),
			thread("t-early", "src/a.cpp", 10U),
			reply("c-early-1", "t-early", "author", "issue (blocking): Early thread at line 10."),
			reply("c-early-2", "t-early", "reviewer", "A reply that makes the thread taller when it is open."),
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
		const auto top = [&view](const std::string& aId) { return view.note_rect(aId).top(); };

		const auto& cards = view.cards();
		expect(cards.size() == 4U, "this file's threads are shown, the other file's is not");
		expect(cards[0].Id == "t-early" && cards[0].Placed && top("t-early") == 96
				   && cards[1].Id == "t-near" && cards[1].Placed
				   && cards[2].Id == "t-late" && cards[2].Placed && top("t-late") == 496
				   && cards[3].Id == "t-outdated" && !cards[3].Placed && cards[3].Outdated,
			   "notes hang from their lines, nudged up four pixels, in line order, the unplaceable one last");
		expect(top("t-near") > 126 && top("t-near") >= view.note_rect("t-early").bottom() && !overlapping(cards),
			   "a note whose line is under the note above sits beneath it instead");
		expect(top("t-outdated") >= view.note_rect("t-late").bottom(), "the unplaced note follows the last placed one");
		expect(view.line_marks(core::diff_side::right).size() == 3U
				   && view.line_marks(core::diff_side::right).front().Label == core::feedback_label::issue,
			   "each placed thread has a gutter marker with its first post's mark; the outdated one has none");

		// The comparison scrolls: the lines move, the notes follow.
		lineY                          = { { 10U, 40 }, { 12U, 70 }, { 40U, 440 } };
		const auto layoutsBeforeScroll = view.layouts();
		view.refresh_geometry();
		expect(view.layouts() == layoutsBeforeScroll, "following the code retains the forest and measured text");
		expect(top("t-early") == 36 && top("t-late") == 436 && !overlapping(view.cards()),
			   "relaying against moved lines moves the notes with them");

		// Opening t-near pins it at its line; t-early makes room above it.
		auto chosen = std::vector<std::pair<std::string, bool>>{ };
		view.set_thread_dispatch([&chosen](const std::string& aId, const bool aOpen) { chosen.emplace_back(aId, aOpen); });
		press_at(view, view.note_rect("t-near").center());
		expect(chosen == std::vector<std::pair<std::string, bool>>{ { "t-near", true } } && view.open_thread_id() == "t-near"
				   && view.current_thread() == "t-near",
			   "a press on a note opens its thread, makes it current and reports it");
		expect(top("t-near") == 66 && view.open_card()->geometry().top() == 66, "the open thread is pinned exactly at its line");
		expect(view.note_rect("t-early").bottom() < top("t-near") && top("t-early") < 36,
			   "the note above makes room by moving up");
		expect(top("t-late") >= view.note_rect("t-near").bottom() && !overlapping(view.cards()),
			   "the notes below make room by moving down");
		const auto marks = view.line_marks(core::diff_side::right);
		expect(std::ranges::count_if(marks, &qt::thread_line_mark::Open) == 1 && std::ranges::find_if(marks, &qt::thread_line_mark::Open)->Line == 11U,
			   "the open thread's gutter marker says it is open");
		// Negative control: with the thread closed, nothing is pinned and the
		// early note returns to its own line.
		view.close_thread();
		expect(top("t-early") == 36 && view.open_thread_id().empty()
				   && std::ranges::none_of(view.line_marks(core::diff_side::right), &qt::thread_line_mark::Open),
			   "negative control: closed, the early note hangs from its own line again and nothing is open");

		// Keys: Down moves without opening, Return opens, Esc closes.
		view.setFocus(Qt::OtherFocusReason);
		view.set_current_thread("t-early");
		key(view, Qt::Key_Down);
		expect(view.current_thread() == "t-near" && view.open_thread_id().empty(), "Down moves to the next thread without opening it");
		key(view, Qt::Key_Return);
		expect(view.open_thread_id() == "t-near", "Return opens the current thread");
		key(view, Qt::Key_Escape);
		expect(view.open_thread_id().empty() && view.current_thread() == "t-near", "Esc closes the open thread and keeps it current");
		expect(view.hit_test(view.note_rect("t-late").center()) == std::optional<std::size_t>{ 2U }, "hit testing names the note");
		view.set_current_thread("c-late");
		expect(view.current_thread() == "t-late", "a reply's identity names its thread");

		// My draft on this comparison is a dashed note at its line that opens
		// to Publish, Edit and Delete.
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{
						   .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 39U },
						   .Body   = "Same question; check the default before publishing.",
					   },
					   comparison);
		QApplication::processEvents();
		const auto draft = std::ranges::find_if(view.cards(), &qt::thread_card::Draft);
		expect(draft != view.cards().end() && draft->Placed && draft->Line == std::optional<std::uint32_t>{ 40U } && !overlapping(view.cards()),
			   "the draft is a note at its one-based line");
		const auto draftId = draft->Id;
		view.open_thread(draftId);
		QApplication::processEvents();
		expect(button(view, "threadPublishDraft") != nullptr && button(view, "threadEditDraft") != nullptr
				   && button(view, "threadDeleteDraft") != nullptr && button(view, "threadReplyField") == nullptr,
			   "the open draft offers Publish, Edit and Delete and no Reply");
		expect(view.post_count() == 5U, "the draft is not counted as a post");
		// A reply draft is written inside its thread and draws no note.
		view.set_draft(core::review_comment_draft{
						   .Anchor        = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 9U },
						   .Body          = "A reply",
						   .ReplyToThread = std::string{ "early" },
					   },
					   comparison);
		expect(std::ranges::none_of(view.cards(), &qt::thread_card::Draft), "a reply draft draws no note of its own");
		view.set_draft(std::nullopt, std::nullopt);
		lineY.clear();
		view.refresh_geometry();
		expect(std::ranges::none_of(view.cards(), &qt::thread_card::Placed),
			   "invalidated comparison geometry withdraws every placed note");
		view.clear();
		expect(view.cards().empty(), "clearing empties the notes");
		view.hide();
	}

	/// \brief [[BPR-1290]]. Threads beyond the drawer's edges are chips, nearest
	/// first; a wheel scrolls the code; only what the code cannot take scrolls
	/// the notes.
	auto test_chips_wheel_and_overflow() -> void
	{
		qt::conversation_cards_view view;
		view.resize(320, 400);
		view.show();
		QApplication::processEvents();
		auto conversations = std::vector<conversation>{ };
		for (const auto line : { 5U, 30U, 60U, 90U, 120U, 150U, 180U, 210U }) {
			const auto id = "t" + std::to_string(line);
			conversations.push_back(thread(id, "src/a.cpp", line));
			conversations.push_back(reply(id + "-c", id, "author", "suggestion: thread at line " + std::to_string(line)));
		}
		auto offset = 0;
		view.set_line_locator([&offset](core::diff_side, const std::uint32_t aLine) -> std::optional<int> {
			return static_cast<int>(aLine) * 20 - offset;
		});
		auto codeMoves = true;
		auto wheels    = 0;
		view.set_scroll_forward([&](QWheelEvent& aEvent) {
			++wheels;
			if (codeMoves) {
				offset -= aEvent.angleDelta().y();
			}
			return codeMoves;
		});
		view.adopt(conversations, core::file_identity_of("src/a.cpp"));
		view.refresh_geometry();
		QApplication::processEvents();
		auto chips = view.edge_chips();
		expect(chips.size() == 4U && chips[0]->text() == QStringLiteral("30") && chips[1]->text() == QStringLiteral("60")
				   && chips[2]->text() == QStringLiteral("90") && chips[3]->text() == QStringLiteral("+4"),
			   "threads below the drawer are chips, nearest first, three and then a count");
		offset = 2000;
		view.refresh_geometry();
		chips = view.edge_chips();
		expect(!chips.empty() && chips.front()->text() == QStringLiteral("90"),
			   "scrolled down, the threads above are chips, nearest first");
		offset = 0;
		view.refresh_geometry();
		view.edge_chips().front()->click();
		expect(view.open_thread_id() == "t30", "a chip opens its thread");

		const auto centre = QPointF{ view.viewport()->rect().center() };
		auto       down   = QWheelEvent{ centre, view.viewport()->mapToGlobal(centre), QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view.viewport(), &down);
		expect(wheels == 1 && view.overflow() == 0, "a wheel over the notes is the code's");
		codeMoves = false;
		view.refresh_geometry();
		QApplication::sendEvent(view.viewport(), &down);
		expect(wheels == 2 && view.overflow() > 0, "when the code cannot move, the notes take the overflow");
		auto up = QWheelEvent{ centre, view.viewport()->mapToGlobal(centre), QPoint{ }, QPoint{ 0, 120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view.viewport(), &up);
		expect(view.overflow() == 0 && wheels == 2, "scrolling back gives the overflow up before the code moves");
		view.hide();
	}

	/// \brief [[BPR-1290]]. R, N and the reply written in place.
	auto test_reply_in_place_and_next_new() -> void
	{
		qt::conversation_cards_view view;
		view.resize(360, 700);
		view.show();
		QApplication::processEvents();
		view.set_line_locator([](core::diff_side, const std::uint32_t aLine) -> std::optional<int> { return static_cast<int>(aLine) * 6; });
		view.adopt({ thread("t10", "src/a.cpp", 10U), reply("c10", "t10", "a", "first"),
					 thread("t30", "src/a.cpp", 30U), reply("c30", "t30", "b", "second"),
					 thread("t50", "src/a.cpp", 50U), reply("c50", "t50", "c", "third") },
				   core::file_identity_of("src/a.cpp"));
		view.set_read_state({ { .Thread = "t30", .UnreadPosts = { "c30" } }, { .Thread = "t50", .UnreadPosts = { "c50" } } });
		auto actions = std::vector<std::pair<qt::card_action, std::string>>{ };
		view.set_action_dispatch([&](const qt::card_action aAction, const std::string& aId) {
			actions.emplace_back(aAction, aId);
			if (aAction == qt::card_action::reply) {
				view.begin_reply(aId);
			}
		});
		auto typed = std::vector<std::pair<std::string, QString>>{ };
		view.set_reply_text_dispatch([&typed](const std::string& aThread, const QString& aText) { typed.emplace_back(aThread, aText); });
		view.setFocus(Qt::OtherFocusReason);
		key(view, Qt::Key_N);
		expect(view.open_thread_id() == "t30", "N opens the first thread with new posts");
		key(view, Qt::Key_N);
		expect(view.open_thread_id() == "t50", "N again opens the next");
		key(view, Qt::Key_N);
		expect(view.open_thread_id() == "t30", "after the last it wraps to the first");
		key(view, Qt::Key_N, Qt::ShiftModifier);
		expect(view.open_thread_id() == "t50", "Shift+N goes back");
		key(view, Qt::Key_R);
		auto* const editor = view.reply_editor();
		expect(!actions.empty() && actions.back() == std::pair{ qt::card_action::reply, std::string{ "t50" } } && editor != nullptr && editor->hasFocus(),
			   "R asks to reply to the open thread, and the editor opens in place with focus");
		auto* const publish = button(view, "threadReplyPublish");
		expect(publish != nullptr && !publish->isEnabled(), "Publish waits for text and a draft that can publish");
		editor->setPlainText(QStringLiteral("question: Is this still needed?"));
		expect(!typed.empty() && typed.back() == std::pair{ std::string{ "t50" }, QStringLiteral("question: Is this still needed?") },
			   "each edit of the reply goes to the draft editor");
		auto* const chip = view.open_card()->findChild<QWidget*>(QStringLiteral("threadReplyChip"));
		expect(chip != nullptr && chip->accessibleName() == QStringLiteral("question"), "the reply's label follows its first words");
		view.present_reply(qt::thread_reply_presentation{ .Thread = "t50", .Body = QStringLiteral("question: Is this still needed?"), .Publishable = true });
		expect(publish->isEnabled(), "a draft that can publish enables Publish");
		auto submit = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier };
		QApplication::sendEvent(editor, &submit);
		QApplication::processEvents();
		expect(actions.back() == std::pair{ qt::card_action::publish_reply, std::string{ "t50" } } && view.reply_editor() == nullptr,
			   "Command+Return publishes the reply in place and folds the editor");
		view.present_reply(qt::thread_reply_presentation{ .Thread = "t50", .Body = QStringLiteral("kept"), .Publishable = true });
		view.close_thread();
		view.open_thread("t50");
		QApplication::processEvents();
		expect(view.reply_editor() != nullptr && view.reply_editor()->toPlainText() == QStringLiteral("kept"),
			   "a kept reply draft reopens in its thread with its text");
		view.present_reply(std::nullopt);
		auto escape = QKeyEvent{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(view.reply_editor(), &escape);
		QApplication::processEvents();
		expect(view.reply_editor() != nullptr && view.hasFocus(), "Esc in a reply with text keeps it and returns to the notes");
		view.hide();
	}
} // namespace

namespace {
	// [[BPR-0972]]. One thread's read state reaches the notes as that thread
	// alone: a thread whose note is not shown lays nothing out, a shown one
	// lays out its file's notes, and both agree with the complete state.
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
			const auto hidden  = view.set_thread_read_state("t1", { });
			expect(hidden == std::vector<std::string>{ "t1-c" } && view.layouts() == layouts,
				   "a hidden thread's read state lays nothing out and returns its previous posts");
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

	void test_discuss_wheel_moves_readable_lines_without_changing_selection()
	{
		qt::conversation_cards_view view;
		view.resize(360, 180);
		view.set_feed(true);
		view.set_scope(qt::cards_scope::whole_pull_request);
		auto data = std::vector<conversation>{ };
		for (auto i = 0; i < 12; ++i) {
			const auto id = "t" + std::to_string(i);
			data.push_back(thread(id, "src/file.cpp", static_cast<std::uint32_t>(i + 1)));
			data.push_back(reply(id + "-c", id, "someone", "a remark"));
		}
		view.adopt(data);
		view.set_current_thread("t0");
		view.show();
		QApplication::processEvents();
		auto* bar = view.verticalScrollBar();
		bar->setValue(0);
		QWheelEvent wheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		QApplication::sendEvent(view.viewport(), &wheel);
		expect(bar->value() > 0 && view.current_thread() == "t0", "in Discuss a wheel scrolls the list and keeps the current thread");
	}

	void test_thread_menus_and_defect_badges()
	{
		qt::conversation_cards_view view;
		view.resize(430, 420);
		view.show();
		view.set_action_dispatch([](auto, const auto&) { });
		view.set_context_menu_builder([](QMenu& aMenu, const std::string&) { aMenu.addAction(QStringLiteral("Classify as defect…")); });
		view.adopt({ thread("a", "src/a.cpp", 10), reply("a-1", "a", "reviewer", "One thread"), thread("b", "src/a.cpp", 20), reply("b-1", "b", "reviewer", "Another") });
		view.set_defect_badges({ { "a", { QStringLiteral("[open] minor · correctness"), QStringLiteral("Nobody has addressed this yet. Shared context available.") } } });
		QApplication::processEvents();
		view.open_thread("a");
		QApplication::processEvents();
		auto* const badge = view.open_card()->findChild<QToolButton*>(QStringLiteral("threadDefectBadge"));
		expect(badge != nullptr && badge->isVisible(), "the classified open thread shows its badge");
		auto* accessible = QAccessible::queryAccessibleInterface(badge);
		expect(accessible && accessible->role() == QAccessible::Button
				   && accessible->text(QAccessible::Name).contains(QStringLiteral("correctness"))
				   && accessible->text(QAccessible::Description).contains(QStringLiteral("Shared context")),
			   "badge accessibility carries the classification and explained context");
		for (const auto& identity : { std::string{ "a" }, std::string{ "b" } }) {
			auto* menu = view.thread_menu(identity);
			expect(menu && menu->actions().front()->text() == QStringLiteral("Reply")
					   && menu->actions().back()->text() == QStringLiteral("Classify as defect…"),
				   "every thread offers existing actions and the host-supplied defect actions");
			expect(!menu->actions()[1]->isEnabled() && !menu->actions()[1]->toolTip().isEmpty(), "unavailable existing actions explain their refusal");
			delete menu;
		}
		view.set_current_thread("a");
		for (const auto pressed : { Qt::Key_Menu, Qt::Key_F10 }) {
			QKeyEvent event{ QEvent::KeyPress, pressed, pressed == Qt::Key_F10 ? Qt::ShiftModifier : Qt::NoModifier };
			QApplication::sendEvent(&view, &event);
			QApplication::processEvents();
			auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			expect(menu && menu->isVisible(), "context-menu key and Shift+F10 open the selected thread actions");
			menu->close();
			QApplication::processEvents();
		}
		const auto        point = view.note_rect("b").center();
		QContextMenuEvent rightClick{ QContextMenuEvent::Mouse, point, view.viewport()->mapToGlobal(point) };
		QApplication::sendEvent(view.viewport(), &rightClick);
		QApplication::processEvents();
		auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		expect(menu && menu->isVisible(), "right-click opens the same thread menu");
		menu->close();
		QApplication::processEvents();
		view.set_defect_badges({ });
		QApplication::processEvents();
		expect(view.open_card()->findChild<QToolButton*>(QStringLiteral("threadDefectBadge")) == nullptr,
			   "a withdrawn badge leaves no stale action");
	}

	// [[BPR-1029]]. A dispatch that relays the notes out still names the
	// thread whose button was pressed.
	auto test_a_dispatch_that_relays_out_still_names_the_pressed_thread() -> void
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
		view.open_thread(first);
		QApplication::processEvents();
		auto reported = std::vector<std::string>{ };
		view.set_action_dispatch([&reported, &arranged](const qt::card_action, const std::string& aId) {
			arranged("src/z.cpp", "src/a.cpp");
			reported.push_back(aId);
		});
		button(view, "threadShowLine")->click();
		expect(view.cards().size() == 2U && view.cards()[0].Id == second && reported == std::vector<std::string>{ first },
			   "an action whose dispatch relays the notes out still names the pressed thread");
		view.close_thread();
		view.set_thread_dispatch([&reported, &view, &first](const std::string& aId, const bool) {
			view.set_current_thread(first);
			reported.push_back(aId);
		});
		press_at(view, view.note_rect(second).topLeft() + QPoint{ 2, 2 });
		expect(reported == std::vector<std::string>{ first, second },
			   "a press whose dispatch makes another thread current still names the pressed thread");
		view.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_discuss_wheel_moves_readable_lines_without_changing_selection();
	test_thread_menus_and_defect_badges();
	test_one_thread_read_state_matches_the_complete_state();
	test_branching_conversation_reads_in_place();
	test_read_state_marks_unread_posts_and_offers_mark_read();
	test_discuss_lists_threads_that_open_in_place();
	test_notes_hang_from_their_lines_and_one_opens_pinned();
	test_chips_wheel_and_overflow();
	test_reply_in_place_and_next_new();
	test_unchanged_refresh_retains_measured_notes();
	test_alternating_widths_keep_both_widths_measured();
	test_discuss_geometry_refresh_updates_excerpts();
	test_the_excerpt_follows_the_supplied_code_font();
	test_cached_geometry_matches_a_fresh_view();
	test_a_dispatch_that_relays_out_still_names_the_pressed_thread();
	std::cout << "conversation-cards-view-tests:pass\n";
	return EXIT_SUCCESS;
}
