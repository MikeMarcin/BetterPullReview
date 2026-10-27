#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include <QAccessible>
#include <QApplication>
#include <QComboBox>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QTreeView>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
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

	[[nodiscard]] auto root(std::string aId, std::string aPath) -> conversation
	{
		return conversation{
			.Id       = std::move(aId),
			.Kind     = application::github_conversation_kind::review_thread,
			.Body     = std::string{ "a remark" },
			.Path     = std::move(aPath),
			.Line     = 12U,
			.DiffSide = application::github_conversation_diff_side::right,
			.AnchorAvailability
			= application::github_conversation_anchor_availability::current,
		};
	}

	[[nodiscard]] auto reply(std::string aId, std::string aParent) -> conversation
	{
		return conversation{
			.Id       = std::move(aId),
			.ParentId = std::move(aParent),
			.Kind     = application::github_conversation_kind::review_comment,
			.Body     = std::string{ "a reply" },
		};
	}

	/// \brief Every node the panel is showing, by identity.
	[[nodiscard]] auto shown(const qt::github_conversation_panel& aPanel)
		-> std::vector<std::string>
	{
		auto* const model = aPanel.model();
		auto        found = std::vector<std::string>{ };
		const auto  walk  = [&](const QModelIndex& aParent, auto& aSelf) -> void {
			for (auto row = 0; row < model->rowCount(aParent); ++row) {
				const auto index = model->index(row, 0, aParent);
				found.push_back(model->id_for_index(index));
				aSelf(index, aSelf);
			}
		};
		walk(QModelIndex{ }, walk);
		return found;
	}

	[[nodiscard]] auto contains(
		const std::vector<std::string>& aIds, const std::string_view aId) -> bool
	{
		return std::ranges::find(aIds, aId) != aIds.end();
	}

	/// \brief One review's conversation, spread across two files.
	[[nodiscard]] auto two_files() -> std::vector<conversation>
	{
		return {
			root("left-root", "src/left.cpp"),
			reply("left-reply", "left-root"),
			reply("left-deep", "left-reply"),
			root("right-root", "src/right.cpp"),
			reply("right-reply", "right-root"),
		};
	}

	/// \brief Anchoring keeps one file's threads and drops the other's.
	auto process_events() -> void
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	/// \brief [[BPR-1290]]. Beside the code the drawer is the notes, with Only
	/// new and Hide resolved as two icons; Discuss lists the same notes in file
	/// order under its scope, file and filter row.
	auto test_discuss_lists_the_notes_with_its_controls() -> void
	{
		auto panel = qt::github_conversation_panel{ };
		panel.resize(1400, 900);
		panel.show();
		process_events();
		panel.adopt({ root("thread", "src/file.cpp"), reply("c1", "thread") },
					QStringLiteral("1 review thread available."));
		process_events();
		expect(!panel.discuss() && !panel.cards()->feed() && !panel.discuss_controls()->isVisibleTo(&panel)
				   && !panel.view()->isVisibleTo(&panel) && panel.cards()->isVisibleTo(&panel)
				   && panel.read_tools()->isAncestorOf(panel.filter_unread_button())
				   && panel.filter_unread_button()->toolButtonStyle() == Qt::ToolButtonIconOnly,
			   "beside the code the drawer is the notes, its filters two icons, and the selection tree is never shown");
		expect(panel.findChild<QWidget*>(QStringLiteral("githubThreadMap")) == nullptr
				   && panel.findChild<QWidget*>(QStringLiteral("threadMapBranch")) == nullptr
				   && panel.findChild<QWidget*>(QStringLiteral("conversationMarkdownBody")) == nullptr,
			   "no thread map, path line or body pane is composed");
		expect(panel.filter_unread_button()->isCheckable() && !panel.filter_unread_button()->isChecked()
				   && panel.filter_unresolved_button()->accessibleName() == QStringLiteral("Hide resolved"),
			   "Only new and Hide resolved are checkable and off");

		panel.set_discuss(true);
		process_events();
		expect(panel.discuss() && panel.cards()->feed() && panel.discuss_controls()->isVisibleTo(&panel)
				   && panel.discuss_controls()->isAncestorOf(panel.filter_unread_button())
				   && panel.filter_unread_button()->toolButtonStyle() == Qt::ToolButtonIconOnly
				   && panel.cards()->isVisibleTo(&panel) && !panel.view()->isVisibleTo(&panel),
			   "Discuss shows its scope, file and filter band above the same notes");
		expect(panel.discuss_summary()->isVisibleTo(&panel) && panel.discuss_controls()->isAncestorOf(panel.discuss_summary())
				   && panel.discuss_summary()->text() == QStringLiteral("1 thread, 1 unresolved, 0 new"),
			   "the band restates the counts, one conversation in the singular");
		panel.filter_unread_button()->setChecked(true);
		process_events();
		expect(panel.cards()->filters().UnreadOnly && panel.cards()->cards().empty(),
			   "Unread filters the list to what has a post newer than what I read");
		panel.filter_unread_button()->setChecked(false);
		panel.set_discuss(false);
		process_events();
		expect(!panel.discuss() && !panel.cards()->feed() && !panel.discuss_summary()->isVisibleTo(&panel)
				   && panel.read_tools()->isAncestorOf(panel.filter_unread_button()) && panel.cards()->isVisibleTo(&panel),
			   "leaving Discuss gives the filters back to the tab row");
	}

	auto test_anchoring_keeps_one_file() -> void
	{
		qt::github_conversation_panel panel;
		panel.adopt(two_files(), QStringLiteral("five conversations"));
		const auto everything = shown(panel);
		expect(everything.size() == 5U, "unanchored, the panel shows every node");
		const auto rootIndex  = panel.model()->index_for_id("left-root");
		const auto replyIndex = panel.model()->index_for_id("left-reply");
		const auto deepIndex  = panel.model()->index_for_id("left-deep");
		expect(replyIndex.parent() == rootIndex && deepIndex.parent() == replyIndex,
			   "reply rows nest under the root and preserve reply-to-reply depth");

		panel.set_anchor_file(core::file_identity_of("src/left.cpp"));
		const auto anchored = shown(panel);
		expect(
			anchored.size() == 3U,
			"anchoring to one file keeps that file's thread and no other");
		expect(
			contains(anchored, "left-root") && contains(anchored, "left-reply")
				&& contains(anchored, "left-deep"),
			"the whole thread survives, root and every reply under it");
		expect(
			!contains(anchored, "right-root")
				&& !contains(anchored, "right-reply"),
			"the other file's thread is gone entirely");
	}

	/// \brief A reply inherits its root's anchor rather than carrying one.
	///
	/// \details This is the case a per-node filter gets wrong. Replies have no
	/// `Path`, so a filter that asked each node for its own anchor would keep
	/// the root and drop every reply — leaving a thread that looks answered by
	/// nobody — or, matching the other way, keep a reply whose root was dropped
	/// and present it as a top-level remark.
	auto test_replies_have_no_anchor_of_their_own() -> void
	{
		const auto conversations = two_files();
		const auto replies       = std::ranges::count_if(
			conversations,
			[](const conversation& aValue) { return aValue.Path.empty(); });
		expect(
			replies == 3U,
			"the fixture's replies really do carry no path, so inheritance is "
			"what this test measures");

		qt::github_conversation_panel panel;
		panel.adopt(conversations, QStringLiteral("five conversations"));
		panel.set_anchor_file(core::file_identity_of("src/left.cpp"));
		const auto anchored = shown(panel);
		expect(
			contains(anchored, "left-deep"),
			"a reply two levels below the root is kept by its root's anchor");
	}

	/// \brief Changing the anchor re-filters without another projection.
	auto test_anchor_round_trip_preserves_identical_projection() -> void
	{
		qt::github_conversation_panel panel;
		const auto                    projection = std::vector{ root("t", "src/a.cpp"), reply("p", "t") };
		panel.adopt(projection, QStringLiteral("available"));
		panel.view()->setCurrentIndex(panel.model()->index_for_id("p"));
		panel.view()->collapse(panel.model()->index_for_id("t"));
		const auto measurements = panel.cards()->measurements();
		for (auto refresh = 0; refresh < 4; ++refresh) {
			panel.set_anchor_file(core::file_identity_of("src/a.cpp"));
			panel.set_anchor_file(std::nullopt);
		}
		expect(panel.cards()->measurements() == measurements
				   && panel.model()->id_for_index(panel.view()->currentIndex()) == "p"
				   && !panel.view()->isExpanded(panel.model()->index_for_id("t"))
				   && panel.cards()->current_thread() == "t",
			   "equivalent anchor projections retain measurements, selected reply and expansion");
		panel.clear(QStringLiteral("stale"));
		panel.set_anchor_file(core::file_identity_of("src/a.cpp"));
		expect(panel.model()->rowCount() == 0 && panel.cards()->cards().empty(),
			   "anchor changes after authority clear cannot restore retained conversations");
	}

	auto test_anchor_and_unchanged_refresh_measure_cards_once() -> void
	{
		qt::github_conversation_panel panel;
		const auto                    projection = two_files();
		panel.adopt(projection, QStringLiteral("available"));
		const auto layouts      = panel.cards()->layouts();
		const auto measurements = panel.cards()->measurements();
		panel.set_anchor_file(core::file_identity_of("src/left.cpp"));
		expect(panel.cards()->layouts() == layouts + 1U && panel.cards()->thread_count() == 1U
				   && panel.cards()->measurements() == measurements,
			   "changing the file anchor measures the final card forest only once");
		panel.view()->setCurrentIndex(panel.model()->index_for_id("left-reply"));
		panel.adopt(projection, QStringLiteral("updated availability"));
		expect(panel.cards()->layouts() == layouts + 1U
				   && panel.model()->id_for_index(panel.view()->currentIndex()) == "left-reply"
				   && panel.cards()->current_thread() == "left-root",
			   "unchanged refresh retains card measurements and exact selected reply");
	}

	auto test_changing_the_anchor_follows_immediately() -> void
	{
		qt::github_conversation_panel panel;
		panel.adopt(two_files(), QStringLiteral("five conversations"));
		panel.set_anchor_file(core::file_identity_of("src/left.cpp"));
		expect(shown(panel).size() == 3U, "anchored to the first file");

		// No second adopt. A reviewer moving between files expects the drawer
		// to follow them, not to wait for the network.
		panel.set_anchor_file(core::file_identity_of("src/right.cpp"));
		const auto moved = shown(panel);
		expect(
			moved.size() == 2U && contains(moved, "right-root")
				&& contains(moved, "right-reply"),
			"moving the anchor shows the other file's thread with no new "
			"projection");

		panel.set_anchor_file(std::nullopt);
		expect(
			shown(panel).size() == 5U,
			"clearing the anchor returns the review-wide conversation");
	}

	/// \brief A file nobody commented on shows nothing, not everything.
	auto test_a_file_without_threads_is_empty() -> void
	{
		qt::github_conversation_panel panel;
		panel.adopt(two_files(), QStringLiteral("five conversations"));
		panel.set_anchor_file(core::file_identity_of("src/untouched.cpp"));
		expect(
			shown(panel).empty(),
			"a file with no threads shows an empty drawer rather than falling "
			"back to the whole review");
		expect(
			!panel.availability_label()->text().isEmpty(),
			"the availability wording survives an empty anchor, so the reviewer "
			"is told why the drawer is empty");
	}

	/// \brief An orphaned reply is shown rather than silently dropped.
	auto test_an_orphan_is_kept_by_its_own_anchor() -> void
	{
		auto conversations = std::vector<conversation>{
			reply("orphan", "a-root-that-is-not-here"),
		};
		conversations.front().Path = "src/left.cpp";

		qt::github_conversation_panel panel;
		panel.adopt(conversations, QStringLiteral("one conversation"));
		panel.set_anchor_file(core::file_identity_of("src/left.cpp"));
		expect(
			contains(shown(panel), "orphan"),
			"a node whose parent is missing from the projection is judged on "
			"its own anchor, because discarding a comment for having a missing "
			"ancestor loses it silently");
	}

	auto test_exact_source_anchor_filters_file_side_and_line() -> void
	{
		auto conversations = std::vector<conversation>{
			root("right-12", "src/file.cpp"),
			root("left-12", "src/file.cpp"),
			root("right-13", "src/file.cpp"),
		};
		conversations[1].DiffSide = application::github_conversation_diff_side::left;
		conversations[2].Line     = 13U;
		qt::github_conversation_panel panel;
		panel.adopt(conversations, QStringLiteral("three conversations"));
		panel.set_anchor_file(core::file_identity_of("src/file.cpp"));
		const auto rightAccessible = panel.model()
										 ->index_for_id("right-12")
										 .data(Qt::AccessibleTextRole)
										 .toString();
		const auto leftAccessible  = panel.model()
										 ->index_for_id("left-12")
										 .data(Qt::AccessibleTextRole)
										 .toString();
		expect(rightAccessible.contains(QStringLiteral("head/right side"))
				   && leftAccessible.contains(QStringLiteral("base/left side"))
				   && rightAccessible != leftAccessible,
			   "accessible current-line roots name the pane side that selection targets");
		panel.set_anchor_line(core::source_line_ref{
			.Side = core::diff_side::right,
			.Line = 11U,
		});
		expect(
			shown(panel) == std::vector<std::string>{ "right-12" },
			"source selection keeps only the exact file, side, and one-based-to-zero-based line");
	}

	auto test_unplaced_anchor_wording_is_explicit() -> void
	{
		auto outdated = root("outdated", "src/file.cpp");
		outdated.Line.reset();
		outdated.OriginalLine       = 19U;
		outdated.AnchorAvailability = application::github_conversation_anchor_availability::outdated_revision;
		auto file                   = root("file", "src/file.cpp");
		file.Line.reset();
		file.AnchorAvailability = application::github_conversation_anchor_availability::file_level;
		auto missingSide        = root("missing-side", "src/file.cpp");
		missingSide.DiffSide.reset();
		missingSide.AnchorAvailability = application::github_conversation_anchor_availability::missing_side;
		auto missingLine               = root("missing-line", "src/file.cpp");
		missingLine.Line.reset();
		missingLine.AnchorAvailability        = application::github_conversation_anchor_availability::missing_current_line;
		auto                          current = root("current", "src/file.cpp");
		qt::github_conversation_panel panel;
		panel.adopt(
			{ outdated, file, missingSide, missingLine, current },
			QStringLiteral("five conversations"));
		panel.set_current_endpoint_placement(true, false);
		const auto outdatedText = panel.model()->index(0, 0).data().toString();
		const auto fileText     = panel.model()->index(1, 0).data().toString();
		const auto sideText     = panel.model()->index(2, 0).data().toString();
		const auto lineText     = panel.model()->index(3, 0).data().toString();
		const auto currentText  = panel.model()->index(4, 0).data().toString();
		expect(outdatedText.contains(QStringLiteral("outdated revision"))
				   && outdatedText.contains(QStringLiteral("original line 19")),
			   "outdated anchors remain visibly unplaced without guessing a current line");
		expect(fileText.contains(QStringLiteral("File-level thread"))
				   && fileText.contains(QStringLiteral("no source line anchor")),
			   "file-level anchors explicitly explain why no source line can be selected");
		expect(sideText.contains(QStringLiteral("diff side is missing"))
				   && lineText.contains(QStringLiteral("current line is missing")),
			   "missing placement facts remain explicit instead of defaulting a pane or line");
		expect(currentText.contains(
				   QStringLiteral("does not expose the current pull-request diff revision")),
			   "current GitHub coordinates name a selected-revision mismatch instead of appearing placed");
	}

	/// [[BPR-1781]]. Overview's Comments takes a new comment of its own: the
	/// composer asks to open and keep the draft, posts only by Comment, takes
	/// the host's presentation for its own key alone and starts again empty
	/// once the comment is posted.
	void test_a_new_comment_is_written_under_the_conversation()
	{
		bpr::qt::github_conversation_panel panel;
		panel.resize(420, 600);
		panel.show();
		QApplication::processEvents();
		expect(!panel.new_comment()->isVisible(), "the composer waits until it is offered");
		panel.set_new_comment_offered(true);
		QApplication::processEvents();
		expect(panel.new_comment()->isVisible() && !panel.new_comment_send()->isEnabled(), "offered, it shows with Comment off");
		auto asked = std::vector<std::pair<bpr::core::conversation_reply_action, QString>>{ };
		panel.set_new_comment_dispatch([&asked](const bpr::core::conversation_reply_action aAction, const QString& aText) { asked.emplace_back(aAction, aText); });
		panel.new_comment_editor()->setPlainText(QStringLiteral("Looks good"));
		expect(asked.size() == 2U && asked[0].first == bpr::core::conversation_reply_action::open && asked[1].first == bpr::core::conversation_reply_action::edit && asked[1].second == QStringLiteral("Looks good"),
			   "the first words open the draft and keep them");
		const auto key = std::string{ bpr::core::new_pull_request_comment };
		panel.present_new_comment(bpr::qt::thread_reply_presentation{ .Thread = key, .Body = QStringLiteral("Looks good"), .Publishable = true, .Notice = QStringLiteral("Saved locally.") });
		expect(panel.new_comment_send()->isEnabled() && panel.new_comment_status()->text() == QStringLiteral("Saved locally."), "the host's presentation enables Comment and says the draft is saved");
		panel.present_new_comment(bpr::qt::thread_reply_presentation{ .Thread = "issue-comment:other", .Body = QStringLiteral("a reply"), .Publishable = true });
		expect(panel.new_comment_editor()->toPlainText() == QStringLiteral("Looks good"), "another draft's presentation leaves this one's words alone");
		asked.clear();
		panel.new_comment_editor()->setPlainText(QStringLiteral("Looks good to me"));
		expect(!asked.empty() && asked.front().first == bpr::core::conversation_reply_action::open, "written in again after another draft, it is opened again");
		panel.present_new_comment(bpr::qt::thread_reply_presentation{ .Thread = key, .Body = QStringLiteral("Looks good to me"), .Publishable = true });
		asked.clear();
		panel.new_comment_send()->click();
		expect(asked.size() == 1U && asked.front().first == bpr::core::conversation_reply_action::send, "Comment asks to send, once");
		panel.present_new_comment(std::nullopt);
		expect(panel.new_comment_editor()->toPlainText().isEmpty() && !panel.new_comment_send()->isEnabled() && panel.new_comment_status()->text() == QStringLiteral("Comment posted."),
			   "posted, the composer starts again empty and says so");
		panel.set_new_comment_offered(false);
		QApplication::processEvents();
		expect(!panel.new_comment()->isVisible(), "taken away beside the code");
	}

	void test_later_threads_are_hidden_with_their_replies()
	{
		qt::github_conversation_panel panel;
		auto                          later      = root("later", "src/file.cpp");
		auto                          historical = root("historical", "src/file.cpp");
		historical.AnchorAvailability            = application::github_conversation_anchor_availability::outdated_revision;
		historical.Outdated                      = true;
		auto reply                               = later;
		reply.Id                                 = "later-reply";
		reply.ParentId                           = "later";
		panel.adopt({ later, reply, historical }, QStringLiteral("Three posts"));
		expect(panel.model()->index_for_id("later-reply").isValid() && panel.cards()->thread_count() == 2U, "without a selected file the review-wide projection retains every thread");
		panel.set_anchor_file(core::file_identity_of("src/file.cpp"));
		panel.set_current_endpoint_placement(false, false);
		const auto excludesLater = [&] { return !panel.model()->index_for_id("later").isValid() && !panel.model()->index_for_id("later-reply").isValid() && panel.cards()->thread_count() == 1U; };
		expect(excludesLater(), "both views hide the entire later thread and retain explicitly historical unplaced threads");
		expect(panel.current_head_thread_count(core::file_identity_of("src/file.cpp")) == 1U, "notice count uses the unfiltered retained index");
		panel.set_current_endpoint_placement(false, true);
		expect(!excludesLater(), "negative control: the current-head forest fails the older-pair exclusion predicate");
		panel.set_current_endpoint_placement(false, false);
		panel.set_anchor_file(std::nullopt);
		expect(panel.model()->index_for_id("later-reply").isValid() && panel.cards()->thread_count() == 2U, "leaving the file restores the review-wide forest without claiming either endpoint is current");
		panel.set_anchor_file(core::file_identity_of("src/file.cpp"));
		expect(excludesLater(), "returning to the older file restores its exclusion");
		panel.set_discuss(true);
		expect(panel.model()->index_for_id("later-reply").isValid() && panel.cards()->thread_count() == 2U, "Show them's Discuss route restores the complete forest");
	}

	auto test_invalid_projected_side_is_never_guessed_as_right() -> void
	{
		auto invalid     = root("invalid-side", "src/file.cpp");
		invalid.DiffSide = static_cast<application::github_conversation_diff_side>(
			255U);
		qt::github_conversation_panel panel;
		panel.adopt({ invalid }, QStringLiteral("one conversation"));
		panel.set_current_endpoint_placement(true, true);
		const auto index = panel.model()->index_for_id("invalid-side");
		expect(index.isValid()
				   && index.data(qt::github_conversation_model::anchor_availability_role)
						   .toInt()
					   == static_cast<int>(application::
											   github_conversation_anchor_availability::missing_side)
				   && index.data().toString().contains(
					   QStringLiteral("diff side is missing"))
				   && !index.data().toString().contains(
					   QStringLiteral("head/right side")),
			   "an invalid projected side is visibly unplaced instead of guessed as right");
		panel.set_anchor_file(core::file_identity_of("src/file.cpp"));
		panel.set_anchor_line(core::source_line_ref{
			.Side = core::diff_side::right,
			.Line = 11U,
		});
		expect(panel.model()->rowCount() == 0,
			   "an invalid projected side cannot match a right-side source selection");
	}

	void test_pr_wide_index_and_selected_detail()
	{
		qt::github_conversation_panel panel;
		panel.resize(1400, 900);
		panel.set_anchor_file(core::file_identity_of("absent.cpp"));
		panel.set_anchor_line(core::source_line_ref{ .Side = core::diff_side::right, .Line = 99U });
		panel.set_current_endpoint_placement(false, false);
		auto data = two_files();
		data.push_back(conversation{ .Id = "issue", .Kind = application::github_conversation_kind::issue_comment, .Body = "PR-wide comment" });
		data.push_back(conversation{ .Id = "review", .Kind = application::github_conversation_kind::review, .Body = "Review summary" });
		panel.adopt(data, QStringLiteral("Accepted conversations"));
		panel.set_discuss(true);
		panel.show();
		process_events();
		expect(shown(panel).size() == data.size() && panel.cards()->thread_count() == 4U,
			   "Discuss includes both files, PR comments and reviews despite unavailable comparison and absent code anchor");
		expect(panel.reveal_button() == nullptr, "focus-only reveal control is removed");
		panel.view()->setCurrentIndex(panel.model()->index_for_id("right-reply"));
		process_events();
		expect(panel.cards()->current_thread() == "right-root",
			   "exact post selection makes its owning thread current");
		panel.cards()->set_action_dispatch([](const qt::card_action, const std::string&) { });
		const auto openInCode = [&panel](const std::string& aThread) {
			auto* const menu    = panel.cards()->thread_menu(aThread);
			const auto  offered = menu != nullptr && menu->actions().size() > 3 && menu->actions()[3]->isEnabled();
			delete menu;
			return offered;
		};
		// [[BPR-1581]]. A thread on a file is offered Open in Review whether
		// or not its exact endpoint is current: without one it opens among
		// its file's unplaced threads. A PR-wide comment never is.
		expect(openInCode("right-root"), "a thread on a file offers Open in Review without its exact endpoint");
		panel.set_current_endpoint_placement(true, true);
		expect(openInCode("right-root"), "and with it");
		panel.set_current_endpoint_placement(false, false);
		expect(openInCode("right-root"), "losing exact endpoint facts keeps Open in Review");
		expect(!openInCode("issue"), "a PR-wide comment never offers Open in Review");
		// A thread's menu makes its thread current; read the reply again.
		panel.view()->setCurrentIndex(panel.model()->index_for_id("right-reply"));
		panel.scope_this_file_button()->setChecked(true);
		process_events();
		expect(panel.cards()->thread_count() == 1U && panel.discuss_summary()->text().contains(QStringLiteral("pathless")),
			   "explicit file scope excludes and explains pathless content");
		panel.findChild<QToolButton*>(QStringLiteral("conversationClearFilters"))->click();
		expect(panel.cards()->thread_count() == 4U && panel.anchor_file() == core::file_identity_of("absent.cpp"),
			   "clear filters restores all content without changing code anchor");
		panel.set_discuss(false);
		panel.set_discuss(true);
		expect(panel.model()->id_for_index(panel.view()->currentIndex()) == "right-reply", "Discuss round trip restores exact selected post");
		panel.clear(QStringLiteral("Loading replacement"));
		expect(panel.model()->rowCount() == 0 && panel.cards()->cards().empty() && panel.cards()->current_thread().empty(),
			   "authority clear removes every prior conversation identity");
	}

	/// [[BPR-1089]]. An Overview count's drill-down lists exactly its
	/// conversations: the whole pull request in scope, no other filter, the
	/// list kept across a refresh with the current thread, and cleared by the
	/// glyph or by Clear filters.
	void test_an_overview_count_lists_exactly_its_conversations()
	{
		qt::github_conversation_panel panel;
		panel.resize(1400, 900);
		auto data = two_files();
		data.push_back(conversation{ .Id = "issue", .Kind = application::github_conversation_kind::issue_comment, .Body = "PR-wide comment" });
		data.push_back(conversation{ .Id = "review", .Kind = application::github_conversation_kind::review, .Body = "Review summary" });
		panel.adopt(data, QStringLiteral("Accepted conversations"));
		panel.set_discuss(true);
		panel.show();
		process_events();
		auto cleared = 0;
		panel.set_listing_cleared([&] { ++cleared; });
		panel.scope_this_file_button()->setChecked(true);
		panel.filter_unresolved_button()->setChecked(true);
		const auto ids = [&panel] {
			auto result = std::set<std::string>{ };
			for (const auto& card : panel.cards()->cards()) {
				result.insert(card.Id);
			}
			return result;
		};
		panel.list_conversations({ "issue", "left-root" }, QStringLiteral("issue · open · src/left.cpp — 2 threads"), QStringLiteral("Listed from the Overview"));
		process_events();
		expect(ids() == std::set<std::string>{ "issue", "left-root" } && panel.listing().has_value() && panel.listing_caption()->isVisibleTo(&panel)
				   && panel.listing_caption()->text().startsWith(QStringLiteral("issue · open")),
			   "Discuss lists exactly the two counted conversations under the count's caption");
		expect(panel.scope_whole_pull_request_button()->isChecked() && !panel.filter_unresolved_button()->isChecked() && !panel.filter_unread_button()->isChecked(),
			   "the whole pull request is in scope and no other filter narrows the list");
		panel.view()->setCurrentIndex(panel.model()->index_for_id("left-root"));
		process_events();
		auto refreshed = data;
		refreshed.push_back(root("new-root", "src/new.cpp"));
		panel.adopt(refreshed, QStringLiteral("Accepted conversations"));
		process_events();
		expect(ids() == std::set<std::string>{ "issue", "left-root" } && panel.cards()->current_thread() == "left-root",
			   "a refresh keeps the list exact and the current thread");
		panel.list_conversations({ "right-root" }, QStringLiteral("question · open · src/right.cpp — 1 thread"), QStringLiteral("Listed from the Overview"));
		process_events();
		expect(ids() == std::set<std::string>{ "right-root" }, "listing again replaces the list");
		panel.clear_listing_button()->click();
		process_events();
		expect(!panel.listing() && ids().size() == 5U && !panel.listing_caption()->isVisibleTo(&panel) && cleared == 1, "the glyph shows every conversation again");
		panel.list_conversations({ "review" }, QStringLiteral("Not counted · Pull request — 1 thread"), QStringLiteral("Listed from the Overview"));
		panel.findChild<QToolButton*>(QStringLiteral("conversationClearFilters"))->click();
		process_events();
		expect(!panel.listing() && ids().size() == 5U && cleared == 2, "Clear filters clears the list too");
	}

	void test_contextual_scope_survives_clear_and_refresh()
	{
		qt::github_conversation_panel panel;
		panel.set_anchor_file(core::file_identity_of("src/left.cpp"));
		panel.adopt(two_files(), QStringLiteral("Accepted"));
		expect(panel.cards()->scope() == qt::cards_scope::this_file && panel.cards()->thread_count() == 1U,
			   "Read begins with contextual cards");
		panel.clear(QStringLiteral("Refreshing"));
		panel.adopt(two_files(), QStringLiteral("Accepted replacement observation"));
		expect(panel.cards()->scope() == qt::cards_scope::this_file && panel.cards()->thread_count() == 1U,
			   "refresh gap does not expose whole-PR cards in contextual Read");
		panel.set_discuss(true);
		expect(panel.cards()->thread_count() == 2U, "entering Discuss establishes PR-wide scope");
		panel.set_discuss(false);
		expect(panel.cards()->thread_count() == 1U, "returning to Read restores contextual scope");
		panel.set_discuss(true);
		expect(panel.cards()->thread_count() == 2U, "returning to Discuss preserves independent PR-wide scope");
	}

	// [[BPR-0972]]. Choosing a file re-filters the retained forest by the
	// file's own conversations: the panel and its cards visit that file's
	// nodes, not the whole review, and hand the cards no new forest.
	void test_choosing_a_file_visits_only_its_conversations()
	{
		for (const auto threads : { 32U, 4096U }) {
			auto conversations = std::vector<conversation>{ };
			for (unsigned thread = 0; thread < threads; ++thread) {
				const auto id = "t" + std::to_string(thread);
				conversations.push_back(root(id, "src/file" + std::to_string(thread % (threads / 4U)) + ".cpp"));
				conversations.push_back(reply(id + "-reply", id));
			}
			qt::github_conversation_panel panel;
			panel.adopt(conversations, QStringLiteral("available"));
			panel.set_anchor_file(core::file_identity_of("src/file0.cpp"));
			const auto nodes   = panel.projection_nodes_visited();
			const auto cards   = panel.cards()->layout_nodes_visited();
			const auto layouts = panel.cards()->layouts();
			panel.set_anchor_file(core::file_identity_of("src/file1.cpp"));
			const auto panelVisits = panel.projection_nodes_visited() - nodes;
			const auto cardVisits  = panel.cards()->layout_nodes_visited() - cards;
			std::cout << "conversation-select threads=" << threads << " panel_nodes_visited=" << panelVisits
					  << " card_nodes_visited=" << cardVisits << " layouts=" << panel.cards()->layouts() - layouts << '\n';
			expect(panelVisits == 8U && cardVisits == 8U && panel.cards()->layouts() == layouts + 1U,
				   "choosing a file visits its four threads and their replies once each");
			const auto kept = shown(panel);
			expect(kept.size() == 8U && contains(kept, "t1") && contains(kept, "t1-reply") && !contains(kept, "t0"),
				   "the chosen file's conversations, and only those, are shown");
		}
	}

	/// \brief Repeated paths share exact identity without retaining an old list's mapping.
	void test_repeated_paths_preserve_exact_file_scope_on_refresh()
	{
		auto conversations = std::vector{
			root("first", "src/file.cpp"), reply("first-post", "first"),
			root("second", "src/file.cpp"), reply("second-post", "second"),
			root("distinct", "Src/file.cpp"), reply("distinct-post", "distinct"),
			root("pathless", "")
		};
		qt::github_conversation_panel panel;
		panel.adopt(conversations, QStringLiteral("available"));
		panel.set_anchor_file(core::file_identity_of("src/file.cpp"));
		auto selected = shown(panel);
		expect(selected.size() == 4U && contains(selected, "first-post") && contains(selected, "second-post")
				   && !contains(selected, "distinct") && !contains(selected, "pathless") && panel.cards()->thread_count() == 2U,
			   "same paths share file scope while case-distinct and empty paths remain separate");
		conversations[0].Path = "src/replaced.cpp";
		panel.adopt(conversations, QStringLiteral("refreshed"));
		selected = shown(panel);
		expect(selected.size() == 2U && contains(selected, "second-post") && !contains(selected, "first")
				   && panel.cards()->thread_count() == 1U,
			   "a new list does not reuse the old path mapping");
		panel.set_anchor_file(core::file_identity_of("src/replaced.cpp"));
		selected = shown(panel);
		expect(selected.size() == 2U && contains(selected, "first-post") && !contains(selected, "second")
				   && panel.cards()->thread_count() == 1U,
			   "the replacement path scopes both the model and cards exactly");
	}

	void test_compatibility_anchor_refresh()
	{
		qt::github_conversation_panel panel;
		panel.resize(1000, 700);
		auto thread           = root("file-thread", "src/file.cpp");
		thread.RemoteSubject  = application::github_conversation_remote_subject::file;
		thread.SemanticAnchor = core::source_line_ref{ .Side = core::diff_side::right, .Line = 11U };
		auto post             = reply("file-comment", "file-thread");
		post.RemoteSubject    = thread.RemoteSubject;
		post.SemanticAnchor   = thread.SemanticAnchor;
		post.Body             = "Canonical permalink and raw prose";
		post.ReviewerProse    = "Only the reviewer prose";
		panel.adopt({ thread, post }, QStringLiteral("available"));
		panel.set_current_endpoint_placement(false, false);
		panel.set_anchor_file(core::file_identity_of("src/file.cpp"));
		panel.set_anchor_line(thread.SemanticAnchor);
		panel.show();
		process_events();
		const auto index = panel.model()->index_for_id("file-comment");
		expect(index.isValid(), "a retained compatibility anchor does not borrow native-current placement");
		panel.view()->setCurrentIndex(index);
		process_events();
		expect(index.data(qt::github_conversation_model::body_role).toString() == QStringLiteral("Only the reviewer prose"), "normal comment reader projects prose without representation syntax");
		expect(panel.cards()->current_thread() == "file-thread", "the compatibility reply makes its thread current");
		panel.cards()->open_thread("file-thread");
		process_events();
		const auto supportShown = [&panel] {
			auto* const card = panel.cards()->open_card();
			auto* const note = card != nullptr ? card->findChild<QLabel*>(QStringLiteral("threadSupportNote")) : nullptr;
			return note != nullptr && note->isVisible() && note->text().contains(QStringLiteral("file review comment"));
		};
		expect(supportShown(), "the open thread states the honest server representation");
		if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
			expect(panel.grab().save(path + QStringLiteral("-semantic-support.png")), "semantic support detail rendered for inspection");
		}
		const auto before = panel.model()->statistics();
		panel.adopt({ thread, post }, QStringLiteral("available"));
		expect(panel.model()->statistics().ModelResets == before.ModelResets, "identical compatibility refresh has no model reset");
		auto other = *thread.SemanticAnchor;
		other.Line = 12U;
		panel.set_anchor_line(other);
		expect(panel.model()->rowCount() == 0, "another source line does not acquire the fallback thread");
		panel.set_anchor_line(std::nullopt);
		thread.SemanticAnchor.reset();
		thread.Line.reset();
		thread.DiffSide.reset();
		thread.AnchorAvailability = application::github_conversation_anchor_availability::file_level;
		post.SemanticAnchor.reset();
		post.ReviewerProse.reset();
		panel.adopt({ thread, post }, QStringLiteral("available"));
		expect(!panel.model()->semantic_anchor(panel.model()->index_for_id("file-thread")), "external removal clears semantic placement without changing identity");
		panel.cards()->open_thread("file-thread");
		process_events();
		expect(!supportShown(), "an external edit withdraws stale support information");
		panel.close();
	}

	/// \brief A palette in the dark scheme's roles, for the theme checks.
	[[nodiscard]] auto dark_palette() -> QPalette
	{
		auto palette = QPalette{ QColor{ 0x16, 0x1B, 0x22 } };
		palette.setColor(QPalette::Base, QColor{ 0x0D, 0x11, 0x17 });
		palette.setColor(QPalette::Text, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::ButtonText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::PlaceholderText, QColor{ 0x7D, 0x85, 0x90 });
		palette.setColor(QPalette::Highlight, QColor{ 0x38, 0x8B, 0xFD });
		return palette;
	}

	/// \brief Where a band control sits, in the band's coordinates.
	[[nodiscard]] auto in_band(const QWidget& aBand, const QWidget& aControl) -> QRect
	{
		return QRect{ aControl.mapTo(&aBand, QPoint{ 0, 0 }), aControl.size() };
	}

	/// \brief [[BPR-1337]]. Discuss's top is one band: the scope as two
	/// segments with the file inside it, Threads' two filter chips, and
	/// the counts with Clear filters at the right end, every control named;
	/// the ordinary availability line leaves the counts to say it.
	void test_discuss_top_is_one_named_band()
	{
		auto panel = qt::github_conversation_panel{ };
		panel.resize(1400, 900);
		auto data = two_files();
		data.push_back(conversation{ .Id = "issue", .Kind = application::github_conversation_kind::issue_comment, .Body = "PR-wide comment" });
		data.push_back(conversation{ .Id = "review", .Kind = application::github_conversation_kind::review, .Body = "Review summary" });
		panel.set_availability_ordinary(true);
		panel.adopt(data, QStringLiteral("2 review threads available."));
		panel.set_discuss(true);
		panel.show();
		process_events();
		auto* const band       = panel.discuss_controls();
		auto* const whole      = panel.scope_whole_pull_request_button();
		auto* const file       = panel.scope_this_file_button();
		auto* const choice     = panel.findChild<QComboBox*>(QStringLiteral("conversationFileScope"));
		auto* const unread     = panel.filter_unread_button();
		auto* const unresolved = panel.filter_unresolved_button();
		auto* const clear      = panel.findChild<QToolButton*>(QStringLiteral("conversationClearFilters"));
		auto* const summary    = panel.discuss_summary();
		expect(choice != nullptr && clear != nullptr, "the band holds the file choice and Clear filters");

		// Every control keeps its name, and says what it does when hovered.
		const auto named = std::vector<std::pair<QWidget*, QString>>{
			{ whole, QStringLiteral("Whole pull request") },
			{ file, QStringLiteral("File scope") },
			{ choice, QStringLiteral("Thread file scope") },
			{ unread, QStringLiteral("Unread only") },
			{ unresolved, QStringLiteral("Unresolved only") },
			{ clear, QStringLiteral("Clear filters") },
			{ panel.clear_listing_button(), QStringLiteral("Show every thread") },
			{ summary, QStringLiteral("Threads summary") },
		};
		for (const auto& [control, name] : named) {
			expect(control->accessibleName() == name && (control == summary || !control->toolTip().isEmpty()),
				   "each band control keeps its accessible name and has a tooltip");
		}
		expect(unread->toolButtonStyle() == Qt::ToolButtonIconOnly && unresolved->toolButtonStyle() == Qt::ToolButtonIconOnly
				   && clear->toolButtonStyle() == Qt::ToolButtonIconOnly && whole->toolButtonStyle() == Qt::ToolButtonTextOnly,
			   "the filters and Clear are icons; the scope's two choices are words");

		// One row at this width: scope, filters, then the counts and Clear at
		// the right end, every control centred on the same line.
		expect(whole->isChecked() && !choice->isVisibleTo(&panel), "the whole pull request is in scope and the file choice is hidden");
		const auto row = in_band(*band, *whole).center().y();
		for (auto* const control : std::vector<QWidget*>{ file, unread, unresolved, summary, clear }) {
			expect(std::abs(in_band(*band, *control).center().y() - row) <= 1, "the band is one row");
		}
		expect(in_band(*band, *whole).right() < in_band(*band, *file).left() + 1 && in_band(*band, *file).right() < in_band(*band, *unread).left()
				   && in_band(*band, *unread).right() < in_band(*band, *unresolved).left()
				   && in_band(*band, *unresolved).right() < in_band(*band, *summary).left() && in_band(*band, *summary).right() < in_band(*band, *clear).left(),
			   "scope, filters, counts and Clear read left to right");
		expect(in_band(*band, *whole).left() == 12 && band->width() - in_band(*band, *clear).right() - 1 == 16,
			   "the band starts at the list's edge and ends at its right margin");
		expect(band->height() <= 50 && whole->height() == 30 && unread->height() == 30, "one 30-pixel row of chips in a band under 50 pixels");
		expect(summary->text() == QStringLiteral("4 threads, 2 unresolved, 0 new"), "the counts say the whole sentence");

		// The ordinary availability line is said by the counts.
		expect(!panel.availability_label()->isVisibleTo(&panel) && summary->toolTip().contains(QStringLiteral("2 review threads available.")),
			   "in Discuss an ordinary availability line moves into the counts' tooltip");
		panel.set_availability_ordinary(false);
		process_events();
		expect(panel.availability_label()->isVisibleTo(&panel), "any other availability line stays shown in Discuss");
		panel.set_availability_ordinary(true);

		// File: the file is chosen inside the scope.
		file->click();
		process_events();
		expect(file->isChecked() && choice->isVisibleTo(&panel) && in_band(*band, *choice).left() - in_band(*band, *file).right() <= 8,
			   "choosing File shows the file choice right after it");
		expect(summary->text().contains(QStringLiteral("pathless")) && summary->toolTip().contains(QStringLiteral("File leaves them out")),
			   "File says it leaves out what is not on a file");
		whole->click();
		process_events();
		expect(!choice->isVisibleTo(&panel), "back to the whole pull request, the file choice hides again");

		// The keyboard walks the band in reading order, then the notes. The
		// scope is one stop, as two radio buttons are: Tab reaches the chosen
		// one and the arrows choose the other.
		panel.activateWindow();
		whole->setFocus(Qt::TabFocusReason);
		process_events();
		const auto press = [](const Qt::Key aKey) {
			QKeyEvent key{ QEvent::KeyPress, aKey, Qt::NoModifier };
			QApplication::sendEvent(QApplication::focusWidget(), &key);
			process_events();
		};
		const auto walk = [&](const std::vector<QWidget*>& aOrder) {
			for (auto* const next : aOrder) {
				press(Qt::Key_Tab);
				expect(QApplication::focusWidget() == next, "Tab walks scope, file, filters and Clear in reading order, then the notes");
			}
		};
		walk({ unread, unresolved, clear, panel.cards() });
		whole->setFocus(Qt::TabFocusReason);
		press(Qt::Key_Right);
		expect(QApplication::focusWidget() == file && file->isChecked() && choice->isVisibleTo(&panel), "Right chooses File and shows the file choice");
		walk({ choice, unread, unresolved, clear, panel.cards() });
		file->setFocus(Qt::TabFocusReason);
		press(Qt::Key_Left);
		expect(QApplication::focusWidget() == whole && whole->isChecked() && !choice->isVisibleTo(&panel), "Left chooses the whole pull request again");
		// A drill-down's pill sits between the filters and the counts, and so
		// does its × in the keyboard's order.
		panel.list_conversations({ "issue", "left-root" }, QStringLiteral("issue · open · src/left.cpp — 2 threads"), QStringLiteral("Listed from the Overview"));
		process_events();
		auto* const pill = panel.findChild<QWidget*>(QStringLiteral("conversationListingRow"));
		expect(pill->isVisibleTo(&panel) && in_band(*band, *pill).left() > in_band(*band, *unresolved).right() && in_band(*band, *pill).right() < in_band(*band, *summary).left()
				   && pill->isAncestorOf(panel.listing_caption()) && pill->isAncestorOf(panel.clear_listing_button()),
			   "the drill-down is a pill after the filters, holding its caption and its ×");
		unresolved->setFocus(Qt::TabFocusReason);
		walk({ panel.clear_listing_button(), clear, panel.cards() });
		// Leaving Discuss and coming back keeps that order.
		panel.set_discuss(false);
		panel.set_discuss(true);
		process_events();
		whole->setFocus(Qt::TabFocusReason);
		walk({ unread, unresolved, panel.clear_listing_button(), clear, panel.cards() });
		panel.clear_listing_button()->click();
		process_events();

		// Beside the code nothing of the band shows, and the filters are the
		// drawer's two icons again.
		panel.set_discuss(false);
		process_events();
		expect(!band->isVisibleTo(&panel) && panel.read_tools()->isAncestorOf(unread) && unread->autoRaise() && !unread->icon().isNull(),
			   "beside the code the filters are the drawer's icons");
		panel.close();
	}

	/// \brief [[BPR-1337]]. Beside the composer the list is narrow: the band
	/// wraps rather than clipping, and every control stays whole inside it.
	void test_discuss_top_wraps_when_narrow()
	{
		auto panel = qt::github_conversation_panel{ };
		panel.set_availability_ordinary(true);
		panel.adopt(two_files(), QStringLiteral("2 review threads available."));
		panel.set_discuss(true);
		panel.resize(300, 700);
		panel.show();
		process_events();
		auto* const band = panel.discuss_controls();
		panel.scope_this_file_button()->click();
		process_events();
		const auto controls = std::vector<QWidget*>{ panel.scope_whole_pull_request_button(), panel.scope_this_file_button(),
													 panel.findChild<QComboBox*>(QStringLiteral("conversationFileScope")), panel.filter_unread_button(),
													 panel.filter_unresolved_button(), panel.findChild<QToolButton*>(QStringLiteral("conversationClearFilters")),
													 panel.discuss_summary() };
		auto       rows     = std::set<int>{ };
		for (auto* const control : controls) {
			const auto box = in_band(*band, *control);
			expect(control->isVisibleTo(&panel) && band->rect().contains(box), "every control is whole inside the narrow band");
			rows.insert(box.center().y());
			for (auto* const other : controls) {
				expect(other == control || !in_band(*band, *other).intersects(box), "no two controls overlap");
			}
		}
		expect(rows.size() >= 2U && panel.cards()->geometry().top() >= band->geometry().bottom(), "the band wraps onto more rows, and the notes start below it");
		panel.close();
	}

	/// \brief [[BPR-1337]]. The band and its chips follow the theme on screen,
	/// and the Only new glyph's dot is drawn in the glyph's ink: before, it
	/// was always black and vanished in a dark theme.
	void test_discuss_top_follows_the_theme()
	{
		for (const auto dark : { false, true }) {
			auto panel = qt::github_conversation_panel{ };
			if (dark) {
				panel.setPalette(dark_palette());
			}
			panel.adopt(two_files(), QStringLiteral("2 review threads available."));
			panel.set_discuss(true);
			panel.resize(1000, 600);
			panel.show();
			process_events();
			auto* const band  = panel.discuss_controls();
			const auto  image = band->grab().toImage();
			const auto  base  = panel.palette().color(QPalette::Base);
			const auto  fill  = image.pixelColor(QPoint{ image.width() / 2, 2 });
			expect(std::abs(fill.lightness() - base.lightness()) <= 2, "the band is drawn in the palette's base");
			expect((fill.lightness() < 128) == dark, "the band is dark in the dark theme and light in the light one");
			const auto chosen = panel.scope_whole_pull_request_button()->grab().toImage();
			expect(chosen.pixelColor(QPoint{ 6, chosen.height() / 2 }) != fill, "the chosen scope is filled, not bare");
			panel.close();

			// The drawer's Only new icon: a ring with a dot, both in its ink.
			auto beside = qt::github_conversation_panel{ };
			if (dark) {
				beside.setPalette(dark_palette());
			}
			beside.relayout();
			const auto icon   = beside.filter_unread_button()->icon().pixmap(QSize{ 48, 48 }, 1.0).toImage();
			const auto centre = icon.pixelColor(QPoint{ 24, 24 });
			const auto ink    = beside.palette().color(QPalette::ButtonText);
			expect(centre.alpha() > 200 && std::abs(centre.lightness() - ink.lightness()) < 40, "the Only new dot is drawn in the glyph's ink");
		}
	}

} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_choosing_a_file_visits_only_its_conversations();
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewConversationPanelTests"));
	test_repeated_paths_preserve_exact_file_scope_on_refresh();
	test_compatibility_anchor_refresh();
	test_discuss_lists_the_notes_with_its_controls();
	test_anchoring_keeps_one_file();
	test_replies_have_no_anchor_of_their_own();
	test_changing_the_anchor_follows_immediately();
	test_anchor_and_unchanged_refresh_measure_cards_once();
	test_anchor_round_trip_preserves_identical_projection();
	test_a_file_without_threads_is_empty();
	test_an_orphan_is_kept_by_its_own_anchor();
	test_exact_source_anchor_filters_file_side_and_line();
	test_unplaced_anchor_wording_is_explicit();
	test_later_threads_are_hidden_with_their_replies();
	test_invalid_projected_side_is_never_guessed_as_right();
	test_pr_wide_index_and_selected_detail();
	test_an_overview_count_lists_exactly_its_conversations();
	test_contextual_scope_survives_clear_and_refresh();
	test_discuss_top_is_one_named_band();
	test_discuss_top_wraps_when_narrow();
	test_discuss_top_follows_the_theme();
	test_a_new_comment_is_written_under_the_conversation();
	std::cout << "conversation-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
