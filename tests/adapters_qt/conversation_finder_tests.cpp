#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/conversation_finder.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFile>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyleFactory>

#include <glaze/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1293]]. Threads, the pull request's finder, as prototype
/// v2 draws it: a row per conversation grouped by file, branch rows only where
/// a post has two or more replies that have replies, oldest read posts folded
/// past nine, search over words, people, files and lines, the three filters,
/// the keys, the pick, and the page rendered light and dark.

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

	struct node_json
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
			static constexpr auto value = glz::object("Id", &node_json::Id, "ParentId", &node_json::ParentId, "Kind", &node_json::Kind, "AuthorLogin", &node_json::AuthorLogin, "Body", &node_json::Body, "Path", &node_json::Path, "Line", &node_json::Line);
		};
	};

	struct case_json
	{
		std::vector<node_json> Projection;
		struct glaze
		{
			static constexpr auto value = glz::object("projection", &case_json::Projection);
		};
	};

	struct fixture_json
	{
		std::vector<case_json> Cases;
		struct glaze
		{
			static constexpr auto value = glz::object("cases", &fixture_json::Cases);
		};
	};

	/// \brief `branching-thread-v1`: one thread on `src/core/thread_map.cpp`
	/// line 42, a first reply and three branches off it, the third with an
	/// aside off its third reply that nobody answered.
	[[nodiscard]] auto branching_fixture() -> std::vector<conversation>
	{
		auto file = QFile{ QStringLiteral(BPR_BRANCHING_THREAD_FIXTURE) };
		expect(file.open(QIODevice::ReadOnly), "the branching-thread fixture opens");
		const auto bytes   = file.readAll();
		auto       fixture = fixture_json{ };
		expect(!glz::read<fixture_json_options{ }>(fixture, std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }),
			   "the fixture has its typed fields");
		auto found = std::vector<conversation>{ };
		for (const auto& entry : fixture.Cases) {
			for (const auto& node : entry.Projection) {
				auto made        = conversation{ };
				made.Id          = node.Id;
				made.ParentId    = node.ParentId;
				made.Kind        = node.Kind == "review_thread" ? application::github_conversation_kind::review_thread : application::github_conversation_kind::review_comment;
				made.AuthorLogin = node.AuthorLogin;
				made.Body        = node.Body;
				made.Path        = node.Path;
				made.Line        = node.Line;
				found.push_back(std::move(made));
			}
		}
		expect(found.size() == 25U, "one root and twenty-four posts");
		return found;
	}

	[[nodiscard]] auto thread(const std::string& aId, const std::string& aPath, const std::uint32_t aLine, const bool aResolved = false) -> conversation
	{
		auto made     = conversation{ };
		made.Id       = "review-thread:" + aId;
		made.Kind     = application::github_conversation_kind::review_thread;
		made.Path     = aPath;
		made.Line     = aLine;
		made.Resolved = aResolved;
		return made;
	}

	[[nodiscard]] auto post(const std::string& aId, const std::string& aParent, const std::string& aAuthor, const std::string& aBody, const std::string& aPath) -> conversation
	{
		auto made        = conversation{ };
		made.Id          = "review-comment:" + aId;
		made.ParentId    = aParent;
		made.Kind        = application::github_conversation_kind::review_comment;
		made.AuthorLogin = aAuthor;
		made.Body        = aBody;
		made.Path        = aPath;
		return made;
	}

	/// \brief A GitHub thread: every reply answers the first comment.
	void add_flat(std::vector<conversation>& aForest, const std::string& aId, const std::string& aPath, const std::uint32_t aLine,
				  const std::vector<std::pair<std::string, std::string>>& aPosts, const bool aResolved = false)
	{
		aForest.push_back(thread(aId, aPath, aLine, aResolved));
		for (std::size_t index = 0U; index < aPosts.size(); ++index) {
			const auto parent = index == 0U ? "review-thread:" + aId : "review-comment:" + aId + "-1";
			aForest.push_back(post(aId + "-" + std::to_string(index + 1U), parent, aPosts[index].first, aPosts[index].second, aPath));
		}
	}

	/// \brief The pull request the tests read: five files, a long flat thread,
	/// a resolved one, the branching one, a pull request comment and a
	/// review summary in Not on a file.
	[[nodiscard]] auto forest() -> std::vector<conversation>
	{
		auto made = std::vector<conversation>{ };
		add_flat(made, "t656", "src/ui/conversation_cards_view.cpp", 656,
				 { { "tbutler", "issue (blocking): `set_current_thread()` keeps a reply's id when no card holds it." },
				   { "fixture-author", "question: Should it clear `Current` instead?" },
				   { "jiayin", "praise: Good catch, this explains the lost highlight." },
				   { "tbutler", "Clear it. I'll push the fix with a test." } });
		add_flat(made, "t38", "src/ui/conversation_cards_view.cpp", 38,
				 { { "jiayin", "nitpick (non-blocking): `card_gap` and `card_margin` are both 8." }, { "fixture-author", "They drift apart at Discuss widths." } }, true);
		auto longPosts = std::vector<std::pair<std::string, std::string>>{ };
		for (auto index = 1; index <= 17; ++index) {
			longPosts.emplace_back(index % 3 == 0 ? "tbutler" : index % 3 == 1 ? "jiayin"
																			   : "fixture-author",
								   index == 1 ? "question: Why an explicit stack and not recursion?" : "Reply " + std::to_string(index) + " about the stack.");
		}
		add_flat(made, "g282", "src/adapters/qt/src/github_thread_map.cpp", 282, longPosts);
		add_flat(made, "r6", "src/engine/renderer.cpp", 6, { { "tbutler", "issue (blocking): draw_scene() runs before the clear finishes." } });
		add_flat(made, "r12", "src/engine/renderer.cpp", 12, { { "jiayin", "suggestion (non-blocking, if-minor): Clamp the width to at least one pixel." } });
		for (auto& node : branching_fixture()) {
			made.push_back(std::move(node));
		}
		auto issue        = conversation{ };
		issue.Id          = "issue-comment:c1";
		issue.Kind        = application::github_conversation_kind::issue_comment;
		issue.AuthorLogin = "tbutler";
		issue.Body        = "note: see the design doc";
		made.push_back(issue);
		auto review        = conversation{ };
		review.Id          = "review:r1";
		review.Kind        = application::github_conversation_kind::review;
		review.AuthorLogin = "jiayin";
		review.Body        = "thought: the shape is right";
		made.push_back(review);
		return made;
	}

	[[nodiscard]] auto read_state() -> std::vector<qt::thread_read_state>
	{
		// An edited early post is new again, among old read ones.
		auto unreadLong = std::vector<std::string>{ "review-comment:g282-3" };
		for (auto index = 13; index <= 17; ++index) {
			unreadLong.push_back("review-comment:g282-" + std::to_string(index));
		}
		return {
			{ .Thread = "review-thread:t656", .UnreadPosts = { "review-comment:t656-3", "review-comment:t656-4" } },
			{ .Thread = "review-thread:g282", .UnreadPosts = unreadLong },
			{ .Thread = "review-thread:branching-thread-v1", .UnreadPosts = { "review-comment:branching-thread-v1-b05" } },
		};
	}

	[[nodiscard]] auto finder_post(const std::string& aId, std::optional<std::size_t> aParent) -> qt::finder_post
	{
		auto made   = qt::finder_post{ };
		made.Id     = aId;
		made.Parent = aParent;
		return made;
	}

	void key(QWidget& aTarget, const int aKey, const QString& aText = { })
	{
		auto event = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier, aText };
		QApplication::sendEvent(&aTarget, &event);
		QApplication::processEvents();
	}

	void press(QListView& aList, const QPoint aPoint)
	{
		auto event = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aPoint }, aList.viewport()->mapToGlobal(QPointF{ aPoint }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aList.viewport(), &event);
		QApplication::processEvents();
	}

	/// [[BPR-1582]]. A double click as Qt delivers it: a press, then the
	/// double-click event.
	void double_click(QListView& aList, const QPoint aPoint, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto first = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ aPoint }, aList.viewport()->mapToGlobal(QPointF{ aPoint }), Qt::LeftButton, Qt::LeftButton, aModifiers };
		QApplication::sendEvent(aList.viewport(), &first);
		auto second = QMouseEvent{ QEvent::MouseButtonDblClick, QPointF{ aPoint }, aList.viewport()->mapToGlobal(QPointF{ aPoint }), Qt::LeftButton, Qt::LeftButton, aModifiers };
		QApplication::sendEvent(aList.viewport(), &second);
		QApplication::processEvents();
	}

	void hover(QListView& aList, const QPoint aPoint)
	{
		auto event = QMouseEvent{ QEvent::MouseMove, QPointF{ aPoint }, aList.viewport()->mapToGlobal(QPointF{ aPoint }), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aList.viewport(), &event);
		QApplication::processEvents();
	}

	[[nodiscard]] auto shown_rows(const qt::conversation_finder& aFinder) -> std::vector<std::string>
	{
		auto rows = std::vector<std::string>{ };
		for (const auto& entry : aFinder.shown_keys()) {
			if (entry.starts_with("row:")) {
				rows.push_back(entry);
			}
		}
		return rows;
	}

	[[nodiscard]] auto has_row_of(const qt::conversation_finder& aFinder, const std::string& aThread) -> bool
	{
		const auto rows = shown_rows(aFinder);
		return std::ranges::any_of(rows, [&aThread](const std::string& aRow) { return aRow.starts_with("row:review-thread:" + aThread + "/"); });
	}

	/// \brief The row rule: a GitHub thread is one row; branches only where a
	/// post has two or more replies that have replies; an unexpected shape
	/// costs nesting, never a post.
	void test_rows_branch_only_where_replies_have_replies()
	{
		// A flat thread: every reply answers the first.
		auto flat = std::vector<qt::finder_post>{ finder_post("p0", std::nullopt) };
		for (std::size_t index = 1U; index < 6U; ++index) {
			flat.push_back(finder_post("p" + std::to_string(index), 0U));
		}
		const auto flatRows = qt::finder_rows_of(flat);
		expect(flatRows.size() == 1U && flatRows.front().Posts.size() == 6U && !flatRows.front().Forks, "a GitHub thread is one row of every post");

		// The prototype's deep thread: three branches off the first comment,
		// and the third forks after its third reply into two that each have
		// replies.
		auto       deep  = std::vector<qt::finder_post>{ finder_post("r", std::nullopt) };
		const auto chain = [&deep](const std::string& aName, const std::size_t aFrom, const std::size_t aLength) {
			auto parent = aFrom;
			for (std::size_t index = 0U; index < aLength; ++index) {
				deep.push_back(finder_post(aName + std::to_string(index + 1U), parent));
				parent = deep.size() - 1U;
			}
		};
		chain("a", 0U, 8U);
		chain("b", 0U, 7U);
		chain("c", 0U, 3U);
		const auto c3 = deep.size() - 1U;
		chain("d", c3, 4U);
		chain("x", c3, 3U);
		const auto rows = qt::finder_rows_of(deep);
		expect(rows.size() == 6U, "the first comment, three branches, and the third's continuation and aside: six rows");
		const auto firstOf = [&deep](const qt::finder_row& aRow) { return deep[aRow.Posts.front()].Id; };
		expect(firstOf(rows[0]) == "r" && rows[0].Posts.size() == 1U && rows[0].Forks && rows[0].Level == 0U, "the first comment alone on its row, with rows under it");
		expect(firstOf(rows[1]) == "a1" && rows[1].Posts.size() == 8U && rows[1].Level == 1U
				   && rows[1].Connectors == std::vector{ qt::finder_connector::tee },
			   "branch A is one row of eight, a tee");
		expect(firstOf(rows[2]) == "b1" && rows[2].Posts.size() == 7U && rows[2].Connectors == std::vector{ qt::finder_connector::tee }, "branch B");
		expect(firstOf(rows[3]) == "c1" && rows[3].Posts.size() == 3U && rows[3].Forks && rows[3].Connectors == std::vector{ qt::finder_connector::end },
			   "branch C to its fork, the last branch");
		expect(firstOf(rows[4]) == "d1" && rows[4].Level == 2U
				   && rows[4].Connectors == (std::vector{ qt::finder_connector::blank, qt::finder_connector::tee }),
			   "C's continuation, one step in, under a finished outline");
		expect(firstOf(rows[5]) == "x1" && rows[5].Connectors == (std::vector{ qt::finder_connector::blank, qt::finder_connector::end }), "and the aside last");
		auto counted = std::size_t{ 0U };
		for (const auto& row : rows) {
			counted += row.Posts.size();
			expect(std::ranges::is_sorted(row.Posts), "a row's posts are in posting order");
		}
		expect(counted == deep.size(), "every post is on exactly one row");

		// A reply nobody answered stays on its parent's row.
		auto leaf = deep;
		leaf.push_back(finder_post("aside", c3));
		const auto leafRows = qt::finder_rows_of(leaf);
		expect(leafRows.size() == 6U && std::ranges::find(leafRows[3].Posts, leaf.size() - 1U) != leafRows[3].Posts.end(),
			   "an unanswered reply at a fork stays on its parent's row");

		// An unexpected shape: a parent that is later, itself or absent.
		auto       hostile     = std::vector<qt::finder_post>{ finder_post("h0", std::nullopt), finder_post("h1", 5U), finder_post("h2", 2U), finder_post("h3", std::nullopt) };
		const auto hostileRows = qt::finder_rows_of(hostile);
		expect(hostileRows.size() == 1U && hostileRows.front().Posts.size() == 4U, "posts that name no earlier parent answer the first, and none is lost");

		// A hostile depth costs memory, never the call stack.
		auto tall = std::vector<qt::finder_post>{ finder_post("t0", std::nullopt) };
		for (std::size_t index = 1U; index < 20'000U; ++index) {
			tall.push_back(finder_post("t" + std::to_string(index), index - 1U));
			if (index % 2U == 0U) {
				tall.push_back(finder_post("u" + std::to_string(index), index - 1U));
				tall.push_back(finder_post("v" + std::to_string(index), tall.size() - 1U));
			}
		}
		expect(!qt::finder_rows_of(tall).empty(), "a ten-thousand-deep forking chain lays out");

		// Negative control: a rule that put every reply on its own row would
		// have given the flat thread six rows.
		expect(flatRows.size() != flat.size(), "the control: one row is not one row per post");
	}

	/// \brief Only conversations on a file, read as the drawer reads them.
	void test_conversations_are_the_ones_on_a_file()
	{
		const auto listed = qt::finder_conversations_of(forest(), read_state(), true, QStringLiteral("fixture-author"));
		expect(listed.size() == 8U && listed[6].Path.isEmpty() && listed[7].Path.isEmpty(), "six file conversations followed by the PR comment and review summary");
		expect(listed[0].Path == QStringLiteral("src/adapters/qt/src/github_thread_map.cpp") && listed[1].Path == QStringLiteral("src/core/thread_map.cpp")
				   && listed[2].Line == 6U && listed[3].Line == 12U && listed[4].Line == 38U && listed[5].Line == 656U,
			   "grouped by path, then in line order");
		const auto& t656 = listed[5];
		expect(t656.Posts.size() == 4U && t656.Posts.front().Id == "review-comment:t656-1", "a thread's root holds the thread, not a post");
		expect(t656.Unread == 2U && !t656.Posts[1].Unread && t656.Posts[2].Unread, "the ledger's unread posts are new");
		expect(t656.Posts[0].Label == core::feedback_label::issue && t656.Posts[0].Words.startsWith(QStringLiteral("`set_current_thread()`")),
			   "the mark is the recognized label, and the words follow its header");
		expect(t656.Posts[1].Mine && !t656.Posts[0].Mine, "my posts are mine");
		expect(listed[4].Resolved, "a resolved thread says so");
		const auto unknown = qt::finder_conversations_of(forest(), read_state(), false, QStringLiteral("fixture-author"));
		expect(std::ranges::none_of(unknown, [](const qt::finder_conversation& aConversation) { return aConversation.Unread > 0U; }),
			   "until the read state is known nothing is new");
		const auto& branching = listed[1];
		expect(branching.Rows.size() == 4U && branching.Rows[3].Posts.size() == 8U,
			   "branching-thread-v1: the first comment and three branches, C's unanswered aside on C's row");
	}

	struct page
	{
		qt::conversation_finder                         Finder;
		std::vector<qt::finder_location>                Picks;
		std::vector<core::workbench_thread_destination> Destinations;
		int                                             Leaves = 0;

		page()
		{
			Finder.resize(1000, 640);
			Finder.set_viewer_login(QStringLiteral("fixture-author"));
			Finder.adopt(forest());
			Finder.set_read_state(read_state());
			Finder.set_pick_dispatch([this](const qt::finder_location& aLocation, const core::workbench_thread_destination aDestination) {
				Picks.push_back(aLocation);
				Destinations.push_back(aDestination);
			});
			Finder.set_leave_dispatch([this] { ++Leaves; });
			Finder.show();
			QApplication::processEvents();
			Finder.enter();
			QApplication::processEvents();
		}
	};

	/// \brief Search, the three filters and the label lists.
	void test_search_and_filters()
	{
		auto  shown  = page{ };
		auto& finder = shown.Finder;
		expect(finder.search_field()->hasFocus(), "entering focuses the search");
		expect(shown_rows(finder).size() == 11U, "every row: four of the branching thread and seven single rows");
		expect(finder.shown_keys().front() == "file:src/adapters/qt/src/github_thread_map.cpp", "a file's header comes before its rows");
		expect(finder.cursor_key().starts_with("row:review-thread:g282/"), "with nothing open the cursor starts on the first row");

		finder.search_field()->setText(QStringLiteral("656"));
		expect(shown_rows(finder) == std::vector<std::string>{ "row:review-thread:t656/review-comment:t656-1" }, "a line number finds its conversation");
		finder.search_field()->setText(QStringLiteral("renderer"));
		expect(has_row_of(finder, "r6") && has_row_of(finder, "r12") && shown_rows(finder).size() == 2U, "a file's name finds its conversations");
		finder.search_field()->setText(QStringLiteral("highlight"));
		expect(shown_rows(finder).size() == 1U && has_row_of(finder, "t656"), "words find a conversation by any of its posts");
		finder.search_field()->setText(QStringLiteral("you"));
		expect(has_row_of(finder, "t656") && has_row_of(finder, "t38") && has_row_of(finder, "g282") && !has_row_of(finder, "r6"), "\"you\" finds my posts");
		finder.search_field()->setText(QStringLiteral("observer-three"));
		expect(shown_rows(finder).size() == 4U && shown_rows(finder).front().starts_with("row:review-thread:branching-thread-v1/"),
			   "a person finds the conversations they posted in");
		finder.search_field()->setText(QStringLiteral("nothing anybody wrote"));
		expect(shown_rows(finder).empty(), "no match shows no row");
		key(*finder.search_field(), Qt::Key_Escape);
		expect(finder.search_field()->text().isEmpty() && shown_rows(finder).size() == 11U && shown.Leaves == 0, "Esc clears the search first");
		key(*finder.search_field(), Qt::Key_Escape);
		expect(shown.Leaves == 1, "and then leaves");

		finder.only_new_button()->click();
		expect(finder.filters().OnlyNew && shown_rows(finder).size() == 6U && has_row_of(finder, "t656") && has_row_of(finder, "g282") && !has_row_of(finder, "r6"),
			   "Only new keeps conversations with new posts");
		expect(finder.only_new_button()->accessibleName() == QStringLiteral("Only threads with new replies (3)"), "its count names the new threads");
		finder.only_new_button()->click();
		finder.hide_resolved_button()->click();
		expect(!has_row_of(finder, "t38") && shown_rows(finder).size() == 10U, "Hide resolved");
		finder.hide_resolved_button()->click();

		// [[BPR-1087]]'s lists: a conversation is its root's label.
		auto labels = application::conversation_labels_projection{ .Generation = 3U };
		labels.Labels.push_back({ .Label = core::feedback_label::issue, .Conversations = { "review-thread:r6", "review-thread:t656", "review:r9" } });
		labels.Labels.push_back({ .Label = core::feedback_label::question, .Conversations = { "review-thread:g282" } });
		finder.set_labels(labels);
		const auto counts = finder.label_counts();
		expect(counts.at(core::feedback_label::issue) == 2U && counts.at(core::feedback_label::question) == 1U && counts.size() == 2U,
			   "the label menu counts only the conversations on a file");
		auto label  = finder.filters();
		label.Label = core::feedback_label::issue;
		finder.set_filters(label);
		expect(shown_rows(finder) == std::vector<std::string>{ "row:review-thread:r6/review-comment:r6-1", "row:review-thread:t656/review-comment:t656-1" },
			   "one label keeps the conversations it counts");
		expect(finder.label_button()->accessibleName() == QStringLiteral("Only threads with a issue"), "the chip names the label");
		finder.set_filters({ });
		expect(shown_rows(finder).size() == 11U, "Show all clears everything");
	}

	/// \brief Folding: a file's header, and a row's oldest read posts past nine.
	void test_folding()
	{
		auto       shown   = page{ };
		auto&      finder  = shown.Finder;
		const auto longRow = std::string{ "row:review-thread:g282/review-comment:g282-1" };
		const auto beads   = finder.shown_beads(longRow);
		expect(beads.size() == 9U && finder.folded_posts(longRow) == 8U, "seventeen posts show nine and fold eight into +8");
		for (const auto index : { 3, 13, 14, 15, 16, 17 }) {
			expect(std::ranges::find(beads, "review-comment:g282-" + std::to_string(index)) != beads.end(), "new posts never fold, an early one among read ones included");
		}
		expect(std::ranges::find(beads, std::string{ "review-comment:g282-1" }) == beads.end() && std::ranges::find(beads, std::string{ "review-comment:g282-12" }) != beads.end(),
			   "the oldest read posts fold; the latest read ones stay");
		// The picked post never folds either.
		finder.set_reading({ .Thread = "review-thread:g282", .Post = "review-comment:g282-2" });
		const auto picked = finder.shown_beads(longRow);
		expect(std::ranges::find(picked, std::string{ "review-comment:g282-2" }) != picked.end() && std::ranges::find(picked, std::string{ "review-comment:g282-1" }) != picked.end(),
			   "the picked post and the path to it stay");
		finder.set_reading({ });
		// +n unfolds the row.
		const auto rect = finder.row_rect(longRow);
		const auto pill = finder.bead_rect(longRow, beads.front()).translated(-24, 0);
		press(*finder.list(), QPoint{ pill.center().x(), rect.center().y() });
		expect(finder.shown_beads(longRow).size() == 17U && finder.folded_posts(longRow) == 0U && shown.Picks.empty(), "+n shows every post and picks nothing");
		// A file's header folds its rows.
		const auto header = std::string{ "file:src/engine/renderer.cpp" };
		press(*finder.list(), finder.row_rect(header).center());
		const auto keys = finder.shown_keys();
		expect(!has_row_of(finder, "r6") && std::ranges::find(keys, header) != keys.end(), "a header folds its file");
		key(*finder.list(), Qt::Key_Right);
		expect(has_row_of(finder, "r6"), "Right unfolds it");
		key(*finder.list(), Qt::Key_Left);
		expect(!has_row_of(finder, "r6"), "Left folds it");
		finder.search_field()->setText(QStringLiteral("draw_scene"));
		expect(has_row_of(finder, "r6"), "a search opens folded files");
	}

	/// \brief The keys and the pick.
	void test_keys_and_pick()
	{
		auto        shown  = page{ };
		auto&       finder = shown.Finder;
		auto* const search = finder.search_field();
		key(*search, Qt::Key_Down);
		expect(finder.list()->hasFocus() && finder.cursor_key() == finder.shown_keys().front(), "Down from the search goes to the first row");
		key(*finder.list(), Qt::Key_Down);
		expect(finder.cursor_key().starts_with("row:review-thread:g282/"), "Down moves between rows");
		key(*finder.list(), Qt::Key_Right);
		key(*finder.list(), Qt::Key_Right);
		const auto second = finder.shown_beads(finder.cursor_key())[1];
		expect(finder.cursor_post() == second, "Right moves along a row's posts");
		expect(second == "review-comment:g282-10" && finder.preview() != nullptr && finder.preview()->accessibleName() == QStringLiteral("jiayin: Reply 10 about the stack."),
			   "and previews the post: the second shown bead, past the eight folded");
		key(*finder.list(), Qt::Key_Return);
		expect(shown.Picks.size() == 1U && shown.Picks.back().Post == second && shown.Picks.back().Thread == "review-thread:g282"
				   && shown.Picks.back().Path == QStringLiteral("src/adapters/qt/src/github_thread_map.cpp"),
			   "Return picks the post under the cursor");
		key(*finder.list(), Qt::Key_Left);
		key(*finder.list(), Qt::Key_Left);
		key(*finder.list(), Qt::Key_Return);
		expect(shown.Picks.back().Post == "review-comment:g282-1", "with no post chosen, Return picks the row's first post");
		key(*finder.list(), Qt::Key_Up);
		key(*finder.list(), Qt::Key_Up);
		expect(search->hasFocus() && finder.cursor_key().empty(), "Up past the first row goes back to the search");
		key(*finder.list(), Qt::Key_D, QStringLiteral("d"));
		key(*finder.list(), Qt::Key_Down);
		key(*finder.list(), Qt::Key_R, QStringLiteral("r"));
		expect(search->hasFocus() && search->text() == QStringLiteral("dr"), "typing in the list goes to the search");
		search->clear();

		// [[BPR-1582]]. The pointer: a click selects, as everywhere else,
		// and opens nothing; a double click on a bead picks its post, on the
		// rest of a row its first post; Shift with it asks for Discuss.
		const auto row    = std::string{ "row:review-thread:t656/review-comment:t656-1" };
		const auto rowBox = finder.row_rect(row);
		finder.list()->scrollTo(finder.list()->model()->index(0, 0));
		const auto third = finder.bead_rect(row, "review-comment:t656-3");
		expect(!third.isEmpty() && rowBox.contains(third), "the third post's bead is on its row");
		hover(*finder.list(), third.center());
		expect(finder.preview() != nullptr && finder.preview()->accessibleName().startsWith(QStringLiteral("jiayin: praise")), "hovering a bead previews its post");
		const auto picksBefore = shown.Picks.size();
		press(*finder.list(), third.center());
		expect(shown.Picks.size() == picksBefore && finder.cursor_key() == row && finder.cursor_post() == "review-comment:t656-3",
			   "a click on a bead selects its row and post and picks nothing");
		press(*finder.list(), QPoint{ finder.row_rect(row).left() + 120, finder.row_rect(row).center().y() });
		expect(shown.Picks.size() == picksBefore && finder.cursor_key() == row && finder.cursor_post().empty(), "a click on the row selects the row alone and picks nothing");
		double_click(*finder.list(), third.center());
		expect(shown.Picks.size() == picksBefore + 1U && shown.Picks.back().Post == "review-comment:t656-3", "a double click on a bead picks that post");
		double_click(*finder.list(), QPoint{ finder.row_rect(row).left() + 120, finder.row_rect(row).center().y() });
		expect(shown.Picks.back().Post == "review-comment:t656-1" && shown.Picks.back().Thread == "review-thread:t656", "a double click on a row picks its first post");
		// A branch row picks its own first post.
		const auto branchC = std::string{ "row:review-thread:branching-thread-v1/review-comment:branching-thread-v1-cc01" };
		finder.list()->scrollTo(finder.list()->model()->index(1, 0));
		QApplication::processEvents();
		double_click(*finder.list(), QPoint{ finder.row_rect(branchC).left() + 140, finder.row_rect(branchC).center().y() });
		expect(shown.Picks.back().Post == "review-comment:branching-thread-v1-cc01", "a branch row picks the branch's first post");
		expect(std::ranges::all_of(shown.Destinations, [](const core::workbench_thread_destination aDestination) { return aDestination == core::workbench_thread_destination::review; }),
			   "a double click and Return ask for Review");
		double_click(*finder.list(), QPoint{ finder.row_rect(branchC).left() + 140, finder.row_rect(branchC).center().y() }, Qt::ShiftModifier);
		expect(shown.Destinations.back() == core::workbench_thread_destination::discuss && shown.Picks.back().Post == "review-comment:branching-thread-v1-cc01",
			   "Shift and a double click ask for Discuss");

		// Reopening starts on the conversation open in Review.
		finder.set_reading({ .Thread = "review-thread:t656", .Post = "review-comment:t656-3" });
		finder.enter();
		expect(finder.cursor_key() == row && search->hasFocus(), "the cursor starts on the open conversation, the search focused");
		expect(finder.row_text(row).startsWith(QStringLiteral("Line 656: tbutler, issue, ")) && finder.row_text(row).endsWith(QStringLiteral(", 4 posts, 2 new")),
			   "a row reads its line, author, label, words, posts and new count");
		expect(finder.row_text("row:review-thread:t38/review-comment:t38-1").endsWith(QStringLiteral(", resolved")), "and resolved");
		expect(finder.row_text(branchC).startsWith(QStringLiteral("Branch: reviewer-four")), "a branch row says so");
	}

	/// \brief [[BPR-1356]]. Discuss from Threads: the row's Discuss button on
	/// the row under the pointer and the cursor's row, Shift+Return, and the
	/// row menu with both destinations; my draft is never offered Discuss.
	void test_discuss_button_shift_return_and_menu()
	{
		auto        shown  = page{ };
		auto&       finder = shown.Finder;
		const auto  t656   = std::string{ "row:review-thread:t656/review-comment:t656-1" };
		const auto& first  = finder.cursor_key();
		expect(!finder.discuss_button_rect(first).isEmpty() && finder.discuss_button_rect(t656).isEmpty(),
			   "the cursor's row shows the Discuss button and a row that is neither hovered nor the cursor's does not");
		for (const auto& entry : finder.shown_keys()) {
			if (entry.starts_with("file:")) {
				expect(finder.discuss_button_rect(entry).isEmpty(), "a file's header has no Discuss button");
			}
		}
		finder.list()->scrollTo(finder.list()->model()->index(0, 0));
		const auto box = finder.row_rect(t656);
		hover(*finder.list(), QPoint{ box.left() + 80, box.center().y() });
		const auto button = finder.discuss_button_rect(t656);
		expect(!button.isEmpty() && box.contains(button), "hovering a row shows its button inside the row");
		for (const auto& bead : finder.shown_beads(t656)) {
			expect(finder.bead_rect(t656, bead).left() > button.right(), "the button sits left of the people, so they never move");
		}
		press(*finder.list(), button.center());
		expect(shown.Picks.size() == 1U && shown.Picks.back().Thread == "review-thread:t656" && shown.Picks.back().Post == "review-comment:t656-1"
				   && shown.Destinations.back() == core::workbench_thread_destination::discuss,
			   "the button asks for Discuss at the row's first post");

		// Shift+Return, from the search and from the list, with a post chosen.
		auto shiftReturn = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ShiftModifier };
		finder.enter();
		QApplication::sendEvent(finder.search_field(), &shiftReturn);
		expect(shown.Destinations.back() == core::workbench_thread_destination::discuss && shown.Picks.back().Thread == "review-thread:t656",
			   "Shift+Return in the search asks for Discuss for the cursor's row, kept from the press");
		key(*finder.search_field(), Qt::Key_Down);
		key(*finder.list(), Qt::Key_Down);
		key(*finder.list(), Qt::Key_Right);
		const auto post = finder.cursor_post();
		QApplication::sendEvent(finder.list(), &shiftReturn);
		expect(shown.Destinations.back() == core::workbench_thread_destination::discuss && shown.Picks.back().Post == post,
			   "Shift+Return in the list asks for Discuss at the post under the cursor");

		// The row menu: a right click on a bead, then each destination.
		const auto bead = finder.bead_rect(t656, "review-comment:t656-3");
		finder.popup_row_menu(t656, bead.center());
		auto* menu = finder.row_menu();
		expect(menu != nullptr && menu->isVisible() && menu->defaultAction() == finder.open_in_review_action() && finder.open_in_discuss_action()->isEnabled(),
			   "the row menu opens with Open in Review as its default");
		menu->hide();
		finder.open_in_discuss_action()->trigger();
		expect(shown.Destinations.back() == core::workbench_thread_destination::discuss && shown.Picks.back().Post == "review-comment:t656-3",
			   "Open in Discuss asks for Discuss at the post the right click was on");
		auto right = QContextMenuEvent{ QContextMenuEvent::Mouse, QPoint{ box.left() + 80, box.center().y() }, finder.list()->viewport()->mapToGlobal(QPoint{ box.left() + 80, box.center().y() }) };
		QApplication::sendEvent(finder.list()->viewport(), &right);
		menu = finder.row_menu();
		expect(menu != nullptr && menu->isVisible() && finder.cursor_key() == t656, "a right click on a row opens its menu and makes it the cursor's");
		menu->hide();
		finder.open_in_review_action()->trigger();
		expect(shown.Destinations.back() == core::workbench_thread_destination::review && shown.Picks.back().Post == "review-comment:t656-1",
			   "Open in Review asks for Review at the row's first post");
		const auto picks   = shown.Picks.size();
		auto       menuKey = QKeyEvent{ QEvent::KeyPress, Qt::Key_Menu, Qt::NoModifier };
		finder.list()->setFocus(Qt::OtherFocusReason);
		QApplication::sendEvent(finder.list(), &menuKey);
		menu = finder.row_menu();
		expect(menu != nullptr && menu->isVisible() && shown.Picks.size() == picks, "the menu key opens the cursor's row menu and picks nothing");
		menu->hide();

		// My draft: written beside its line, so never offered Discuss.
		finder.set_draft(qt::finder_draft{ .File = core::file_identity_of("src/engine/renderer.cpp"), .Line = 20U, .Body = QStringLiteral("draft words"), .Author = QStringLiteral("fixture-author") });
		finder.refresh();
		const auto draft = std::string{ "row:draft:unpublished/draft:unpublished" };
		const auto keys  = finder.shown_keys();
		expect(std::ranges::find(keys, draft) != keys.end(), "my draft has its row");
		finder.list()->scrollTo(finder.list()->model()->index(static_cast<int>(std::ranges::find(keys, draft) - keys.begin()), 0));
		QApplication::processEvents();
		const auto draftBox = finder.row_rect(draft);
		hover(*finder.list(), QPoint{ draftBox.left() + 80, draftBox.center().y() });
		expect(finder.discuss_button_rect(draft).isEmpty(), "my draft's row has no Discuss button");
		finder.popup_row_menu(draft, QPoint{ draftBox.left() + 80, draftBox.center().y() });
		menu = finder.row_menu();
		expect(menu != nullptr && menu->isVisible() && !finder.open_in_discuss_action()->isEnabled() && finder.open_in_review_action()->isEnabled(), "and its menu offers Review alone");
		menu->hide();
	}

	/// \brief The page as drawn: light and dark, the high-contrast palettes,
	/// larger text and the narrowest window, nothing clipped.
	void test_renders()
	{
		const auto expected = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		const auto prefix   = qEnvironmentVariable("BPR_QT_FINDER_RENDER_PATH");
		struct variant
		{
			const char* Name;
			QColor      Base;
			QColor      Text;
			int         Width;
			qreal       Font;
		};
		const auto variants = std::array{
			variant{ "light", QColor{ 0xFF, 0xFF, 0xFF }, QColor{ 0x1F, 0x23, 0x28 }, 1000, 1.0 },
			variant{ "dark", QColor{ 0x22, 0x27, 0x2E }, QColor{ 0xE6, 0xED, 0xF3 }, 1000, 1.0 },
			variant{ "contrast-light", QColor{ 0xFF, 0xFF, 0xFF }, QColor{ 0x00, 0x00, 0x00 }, 640, 1.3 },
			variant{ "contrast-dark", QColor{ 0x00, 0x00, 0x00 }, QColor{ 0xFF, 0xFF, 0xFF }, 640, 1.3 },
		};
		for (const auto& entry : variants) {
			auto  shown   = page{ };
			auto& finder  = shown.Finder;
			auto  palette = QApplication::palette();
			palette.setColor(QPalette::Base, entry.Base);
			palette.setColor(QPalette::Window, entry.Base);
			palette.setColor(QPalette::Text, entry.Text);
			palette.setColor(QPalette::WindowText, entry.Text);
			palette.setColor(QPalette::ButtonText, entry.Text);
			palette.setColor(QPalette::PlaceholderText, entry.Base.lightness() < 128 ? QColor{ 0x9B, 0xA4, 0xAE } : QColor{ 0x6E, 0x77, 0x81 });
			finder.setPalette(palette);
			auto font = finder.font();
			font.setPointSizeF(font.pointSizeF() * entry.Font);
			finder.setFont(font);
			finder.resize(entry.Width, 700);
			finder.set_reading({ .Thread = "review-thread:t656", .Post = "review-comment:t656-3" });
			QApplication::processEvents();
			if (!expected.isEmpty()) {
				expect(std::abs(finder.devicePixelRatioF() - expected.toDouble()) < 0.01, "the page renders at the scale the row asked for");
			}
			// Nothing clipped: every bead and state sits inside its row, and
			// the words keep room at the narrowest width.
			for (const auto& key : shown_rows(finder)) {
				const auto rect = finder.row_rect(key);
				if (rect.bottom() < 0 || rect.top() > finder.list()->viewport()->height()) {
					continue;
				}
				for (const auto& bead : finder.shown_beads(key)) {
					const auto box = finder.bead_rect(key, bead);
					expect(box.left() > rect.left() + 120 && box.right() < rect.right() && box.top() >= rect.top() && box.bottom() <= rect.bottom(),
						   std::string{ "a bead sits inside its row, clear of the words: " } + entry.Name);
				}
			}
			// [[BPR-1356]]. The cursor's row shows its Discuss button, inside the
			// row and clear of the people, at every width.
			const auto cursor = finder.cursor_key();
			const auto button = finder.discuss_button_rect(cursor);
			expect(!button.isEmpty() && finder.row_rect(cursor).contains(button), std::string{ "the Discuss button sits inside its row: " } + entry.Name);
			for (const auto& bead : finder.shown_beads(cursor)) {
				expect(finder.bead_rect(cursor, bead).left() > button.right(), std::string{ "and left of the people: " } + entry.Name);
			}
			const auto image = finder.grab().toImage();
			expect(!image.isNull(), "the page renders");
			// The open conversation's row is washed amber, unlike the page.
			const auto open  = finder.row_rect("row:review-thread:t656/review-comment:t656-1");
			const auto at    = finder.list()->viewport()->mapTo(&finder, QPoint{ open.left() + 30, open.top() + 4 }) * finder.devicePixelRatioF();
			const auto plain = finder.list()->viewport()->mapTo(&finder, QPoint{ open.right() - 2, open.bottom() + 40 }) * finder.devicePixelRatioF();
			if (open.isValid() && image.rect().contains(at)) {
				expect(image.pixelColor(at) != image.pixelColor(plain), std::string{ "the open conversation's row is washed: " } + entry.Name);
			}
			if (!prefix.isEmpty()) {
				expect(image.save(prefix + QStringLiteral("-%1.png").arg(QString::fromLatin1(entry.Name))), "the render is saved");
			}
		}
	}

	/// \brief [[BPR-1582]]. The Review button beside the Discuss button, and
	/// the row menu's Mark as read or Mark as unread.
	void test_review_button_and_read_menu()
	{
		auto        shown  = page{ };
		auto&       finder = shown.Finder;
		auto        reads  = std::vector<std::pair<std::string, bool>>{ };
		const auto  t656   = std::string{ "row:review-thread:t656/review-comment:t656-1" };
		const auto  t38    = std::string{ "row:review-thread:t38/review-comment:t38-1" };
		const auto& first  = finder.cursor_key();
		finder.set_read_dispatch([&reads](const std::string& aThread, const bool aRead) { reads.emplace_back(aThread, aRead); });
		expect(!finder.review_button_rect(first).isEmpty() && finder.review_button_rect(t656).isEmpty(), "the cursor's row shows its Review button; another row does not");
		for (const auto& entry : finder.shown_keys()) {
			if (entry.starts_with("file:")) {
				expect(finder.review_button_rect(entry).isEmpty(), "a file's header has no Review button");
			}
		}
		finder.list()->scrollTo(finder.list()->model()->index(0, 0));
		const auto box = finder.row_rect(t656);
		hover(*finder.list(), QPoint{ box.left() + 80, box.center().y() });
		const auto review  = finder.review_button_rect(t656);
		const auto discuss = finder.discuss_button_rect(t656);
		expect(!review.isEmpty() && box.contains(review) && review.right() < discuss.left() && discuss.left() - review.right() <= 8 && review.top() == discuss.top()
				   && review.height() == discuss.height(),
			   "hovering a row shows Review then Discuss, one pair inside the row");
		for (const auto& bead : finder.shown_beads(t656)) {
			expect(finder.bead_rect(t656, bead).left() > discuss.right(), "the pair sits left of the people");
		}
		qt::hide_anchored_tooltip();
		QHelpEvent help{ QEvent::ToolTip, review.center(), finder.list()->viewport()->mapToGlobal(review.center()) };
		QApplication::sendEvent(finder.list()->viewport(), &help);
		const auto* const tip = qobject_cast<const QLabel*>(qt::anchored_tooltip());
		expect(tip != nullptr && tip->text() == QStringLiteral("Open in Review (%1)").arg(QKeySequence{ Qt::Key_Return }.toString(QKeySequence::NativeText)),
			   "the Review button's tooltip names it and its key");
		qt::hide_anchored_tooltip();
		const auto picks = shown.Picks.size();
		press(*finder.list(), review.center());
		expect(shown.Picks.size() == picks + 1U && shown.Destinations.back() == core::workbench_thread_destination::review && shown.Picks.back().Thread == "review-thread:t656",
			   "a press on the Review button opens the row in Review at once");
		press(*finder.list(), finder.discuss_button_rect(t656).center());
		expect(shown.Destinations.back() == core::workbench_thread_destination::discuss, "a press on the Discuss button opens it in Discuss");

		// The row menu: Open in Review, Open in Discuss, then the read command.
		finder.popup_row_menu(t656, QPoint{ box.left() + 80, box.center().y() });
		auto* menu = finder.row_menu();
		expect(menu != nullptr && finder.read_action() != nullptr && finder.read_action()->text() == QStringLiteral("Mark as Read") && finder.read_action()->isEnabled()
				   && menu->actions().indexOf(finder.read_action()) > menu->actions().indexOf(finder.open_in_discuss_action()),
			   "a thread with new posts offers Mark as Read after the two destinations");
		menu->hide();
		finder.read_action()->trigger();
		expect(reads.size() == 1U && reads.back() == std::pair{ std::string{ "review-thread:t656" }, true }, "Mark as Read asks for the thread to be read");
		const auto boxRead = finder.row_rect(t38);
		finder.popup_row_menu(t38, QPoint{ boxRead.left() + 80, boxRead.center().y() });
		menu = finder.row_menu();
		expect(menu != nullptr && finder.read_action()->text() == QStringLiteral("Mark as Unread") && finder.read_action()->isEnabled(),
			   "a read thread someone else wrote in offers Mark as Unread");
		menu->hide();
		finder.read_action()->trigger();
		expect(reads.size() == 2U && reads.back() == std::pair{ std::string{ "review-thread:t38" }, false }, "Mark as Unread asks for it to be new again");
		// By keyboard: the menu key on the cursor's row.
		auto menuKey = QKeyEvent{ QEvent::KeyPress, Qt::Key_Menu, Qt::NoModifier };
		finder.list()->setFocus(Qt::OtherFocusReason);
		QApplication::sendEvent(finder.list(), &menuKey);
		menu = finder.row_menu();
		expect(menu != nullptr && menu->isVisible() && finder.read_action() != nullptr, "the menu key opens the row menu with its read command");
		menu->hide();
		// Negative control: with nobody to hear it, the command is offered
		// disabled rather than doing nothing.
		finder.set_read_dispatch({ });
		finder.popup_row_menu(t38, QPoint{ boxRead.left() + 80, boxRead.center().y() });
		expect(finder.read_action() != nullptr && !finder.read_action()->isEnabled(), "without a read path the command is disabled");
		finder.row_menu()->hide();
	}

	/// \brief [[BPR-1584]]. The rows fill the page up to the reading width;
	/// the people, counts and state stay at the row's right end.
	void test_rows_fill_the_reading_width()
	{
		auto       shown  = page{ };
		auto&      finder = shown.Finder;
		const auto t656   = std::string{ "row:review-thread:t656/review-comment:t656-1" };
		const auto prefix = qEnvironmentVariable("BPR_QT_FINDER_RENDER_PATH");
		const auto page   = [&finder] { return finder.list()->viewport()->width(); };
		struct width_case
		{
			int                 Window;
			core::reading_width Width;
		};
		for (const auto& [window, width] : { width_case{ 1'280, core::reading_width::wide }, width_case{ 1'900, core::reading_width::wide }, width_case{ 3'440, core::reading_width::wide },
											 width_case{ 1'900, core::reading_width::narrow }, width_case{ 3'440, core::reading_width::medium },
											 width_case{ 3'440, core::reading_width::full } }) {
			finder.set_reading_width(width);
			finder.resize(window, 700);
			QApplication::processEvents();
			finder.list()->scrollTo(finder.list()->model()->index(0, 0));
			QApplication::processEvents();
			const auto row      = finder.row_rect(t656);
			const auto column   = static_cast<int>(core::reading_column_width(width, static_cast<std::uint32_t>(page() - 14)));
			const auto trailing = finder.trailing_rect(t656);
			expect(row.width() == page(), "the row's rectangle is the page's width");
			expect(trailing.right() == row.left() + 6 + column - 1 && trailing.right() <= row.right(),
				   "the people, counts and state end at the reading column's right edge: " + std::to_string(window) + " " + std::to_string(static_cast<int>(width)));
			if (width == core::reading_width::wide && window <= 2'560) {
				expect(trailing.right() >= page() - 8 - 1, "an ordinary window's rows reach the page's right edge");
			}
			if (width != core::reading_width::full && window == 3'440) {
				expect(trailing.right() < page() - 400, "an ultra-wide window's rows stop at the reading width");
			}
		}
		// Negative control: the 880-pixel cap the rows had would fail here.
		finder.set_reading_width(core::reading_width::wide);
		finder.resize(1'900, 700);
		QApplication::processEvents();
		expect(finder.trailing_rect(t656).right() > 880 + 200, "a 1900-pixel window's rows run past the old 880-pixel cap");
		// [[BPR-1584]]. Each reading width at an ordinary and an ultra-wide
		// window, for the owner's look.
		for (const auto& [option, label] : { std::pair{ core::reading_width::narrow, "narrow" }, std::pair{ core::reading_width::medium, "medium" },
											 std::pair{ core::reading_width::wide, "wide" }, std::pair{ core::reading_width::full, "full" } }) {
			finder.set_reading_width(option);
			for (const auto window : { 1'900, 3'440 }) {
				finder.resize(window, 520);
				QApplication::processEvents();
				const auto image = finder.grab().toImage();
				if (!prefix.isEmpty()) {
					expect(image.save(prefix + QStringLiteral("-width-%1-%2.png").arg(QString::fromLatin1(label)).arg(window)), "the width render is saved");
				}
			}
		}
		finder.set_reading_width(core::reading_width::wide);
		// Renders, hover and selected rows, Light and Dark, ordinary and
		// ultra-wide.
		for (const auto& [name, base, text] : { std::tuple{ "light", QColor{ 0xFF, 0xFF, 0xFF }, QColor{ 0x1F, 0x23, 0x28 } }, std::tuple{ "dark", QColor{ 0x22, 0x27, 0x2E }, QColor{ 0xE6, 0xED, 0xF3 } } }) {
			auto palette = QApplication::palette();
			palette.setColor(QPalette::Base, base);
			palette.setColor(QPalette::Window, base);
			palette.setColor(QPalette::Text, text);
			palette.setColor(QPalette::WindowText, text);
			palette.setColor(QPalette::ButtonText, text);
			finder.setPalette(palette);
			for (const auto window : { 1'900, 3'440 }) {
				finder.resize(window, 520);
				finder.set_reading({ .Thread = "review-thread:t656", .Post = "review-comment:t656-3" });
				finder.enter();
				QApplication::processEvents();
				finder.list()->scrollTo(finder.list()->model()->index(0, 0));
				const auto other = std::string{ "row:review-thread:t38/review-comment:t38-1" };
				const auto box   = finder.row_rect(other);
				hover(*finder.list(), QPoint{ box.left() + 90, box.center().y() });
				const auto image = finder.grab().toImage();
				expect(!image.isNull(), "the rows render");
				if (!prefix.isEmpty()) {
					expect(image.save(prefix + QStringLiteral("-rows-%1-%2.png").arg(QString::fromLatin1(name)).arg(window)), "the rows render is saved");
				}
			}
		}
	}

	/// \brief Nothing on a file: the page says so and where the rest is read.
	void test_nothing_on_a_file()
	{
		auto finder = qt::conversation_finder{ };
		auto only   = std::vector<conversation>{ };
		auto issue  = conversation{ };
		issue.Id    = "issue-comment:c1";
		issue.Kind  = application::github_conversation_kind::issue_comment;
		issue.Body  = "A pull request comment";
		only.push_back(issue);
		finder.adopt(only);
		finder.show();
		QApplication::processEvents();
		expect(finder.conversations().size() == 1U && finder.shown_keys().size() == 2U && finder.shown_keys().front() == "file:", "a PR comment has a row under Not on a file");
		finder.set_read_state({ { .Thread = issue.Id, .UnreadPosts = { issue.Id } } });
		finder.refresh();
		expect(finder.conversations().front().Unread == 1U && finder.list()->model()->index(0, 0).data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("1 new")), "the pathless header carries the new count accessibly");
		const auto row = finder.shown_keys().back();
		finder.popup_row_menu(row, finder.row_rect(row).center());
		expect(!finder.open_in_review_action()->isEnabled() && finder.open_in_discuss_action()->isEnabled(), "pathless comments offer Discuss, with Review unavailable");
		finder.row_menu()->close();
		// Rebuilt only when shown: a hidden finder adopting a forest reads nothing.
		finder.hide();
		const auto before = finder.rebuilds();
		finder.adopt(forest());
		expect(finder.rebuilds() == before, "a hidden finder rebuilds nothing");
		finder.show();
		QApplication::processEvents();
		expect(finder.rebuilds() == before + 1U && finder.conversations().size() == 8U, "and rebuilds once when shown");
	}
} // namespace

int main(int aArgc, char** aArgv)
{
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Windows")));
	QApplication application{ aArgc, aArgv };
	test_rows_branch_only_where_replies_have_replies();
	test_conversations_are_the_ones_on_a_file();
	test_search_and_filters();
	test_folding();
	test_keys_and_pick();
	test_discuss_button_shift_return_and_menu();
	test_review_button_and_read_menu();
	test_rows_fill_the_reading_width();
	test_renders();
	test_nothing_on_a_file();
	std::cout << "conversation-finder-tests:pass\n";
	return EXIT_SUCCESS;
}
