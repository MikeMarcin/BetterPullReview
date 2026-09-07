#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_thread_map.hpp"

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <cstdlib>
#include <iostream>
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

	/// \brief Returns how far in from a row's left edge its content starts.
	///
	/// \details Measured from pixels rather than from the view's indentation
	/// setting, because the claim is about what a reviewer sees. The first
	/// column differing from the row's own left-edge background is where the
	/// delegate began drawing.
	[[nodiscard]] auto painted_inset(const QImage& aRaster, const QRect& aRow)
		-> int
	{
		const auto ratio = aRaster.devicePixelRatio();
		const auto left  = static_cast<int>(aRow.left() * ratio);
		const auto right = std::min(static_cast<int>(aRow.right() * ratio),
									aRaster.width() - 1);
		const auto first = std::max(0, static_cast<int>(aRow.top() * ratio));
		const auto last  = std::min(static_cast<int>(aRow.bottom() * ratio),
									aRaster.height() - 1);
		// The nearest painted pixel anywhere in the row, not on one chosen
		// scanline: a row's text does not occupy its full height, and sampling
		// a single line finds blank space as often as content.
		auto nearest = -1;
		for (auto y = first; y <= last; ++y) {
			const auto background = aRaster.pixel(left, y);
			for (auto x = left; x <= right; ++x) {
				if (aRaster.pixel(x, y) != background) {
					const auto inset = static_cast<int>(
						static_cast<qreal>(x - left) / ratio);
					nearest = nearest < 0 ? inset : std::min(nearest, inset);
					break;
				}
			}
		}
		return nearest;
	}

	/// \brief Proves depth no longer costs horizontal room.
	///
	/// \details The owner's direction on the BPR-0414 verdict: a linear reply
	/// feed grouped by root, because the staircase spends the width the
	/// comment text needs. Depth is still in the model, and BPR-0418's thread
	/// map is where it is read.
	// [[BPR-0649]]. Discuss moves the same widgets: the tree above the map
	// at the left, the feed at the right, the filters over it and the
	// summary beneath; leaving Discuss puts them back.
	auto test_discuss_widens_into_list_map_and_feed() -> void
	{
		auto panel = qt::github_conversation_panel{ };
		panel.resize(720, 600);
		panel.show();
		process_events();
		panel.adopt({ root("thread", "src/file.cpp"), reply("c1", "thread") },
					QStringLiteral("1 review thread available."));
		process_events();
		expect(!panel.discuss() && !panel.cards()->feed()
				   && !panel.discuss_column()->isAncestorOf(panel.view()),
			   "the drawer opens in the Read arrangement");
		expect(panel.filter_unread_button() != nullptr && panel.filter_unresolved_button() != nullptr
				   && panel.filter_unread_button()->isCheckable() && !panel.filter_unread_button()->isChecked(),
			   "the Unread and Unresolved filters are checkable and off");

		panel.set_discuss(true);
		process_events();
		expect(panel.discuss() && panel.cards()->feed()
				   && panel.discuss_column()->isAncestorOf(panel.view())
				   && panel.discuss_column()->isAncestorOf(panel.thread_map())
				   && !panel.discuss_column()->isAncestorOf(panel.cards())
				   && panel.cards()->isVisibleTo(&panel) && panel.view()->isVisibleTo(&panel),
			   "Discuss puts the tree and the map in the left column and the feed beside them");
		expect(panel.discuss_summary()->isVisibleTo(&panel)
				   && panel.discuss_summary()->text() == QStringLiteral("1 threads, 1 unresolved, 0 new"),
			   "the summary restates the feed's counts");
		panel.filter_unread_button()->setChecked(true);
		process_events();
		expect(panel.cards()->filters().UnreadOnly && panel.cards()->cards().empty(),
			   "Unread filters the feed to what has a post newer than what I read");
		panel.filter_unread_button()->setChecked(false);
		panel.set_discuss(false);
		process_events();
		expect(!panel.discuss() && !panel.cards()->feed()
				   && !panel.discuss_column()->isAncestorOf(panel.view())
				   && !panel.discuss_summary()->isVisibleTo(&panel)
				   && panel.view()->isVisibleTo(&panel) && panel.cards()->isVisibleTo(&panel),
			   "leaving Discuss restores the Read arrangement");
	}

	auto test_replies_are_a_flat_feed_under_their_root() -> void
	{
		auto panel = qt::github_conversation_panel{ };
		panel.resize(360, 1'400);
		panel.show();
		process_events();

		// A chain five replies deep, which under a per-depth indent would push
		// the deepest comment five steps off the left edge.
		auto conversations = std::vector<conversation>{
			root("thread", "src/file.cpp"),
		};
		auto parent = std::string{ "thread" };
		for (auto depth = 1; depth <= 5; ++depth) {
			auto id = "c" + std::to_string(depth);
			conversations.push_back(reply(id, parent));
			parent = std::move(id);
		}
		panel.adopt(conversations, QStringLiteral("1 review thread available."));
		process_events();

		auto* const view = panel.view();
		expect(view->indentation() == 0 && !view->rootIsDecorated(),
			   "the view no longer indents rows by their depth");

		const auto rowOf = [&](const std::string_view aIdentity) {
			const auto index = panel.model()->index_for_id(aIdentity);
			expect(index.isValid(), "the row being measured is in the model");
			const auto rect = view->visualRect(index);
			// A row scrolled out of view has nothing to measure, and a check
			// that silently measured blank pixels would pass for any layout.
			expect(!rect.isEmpty()
					   && view->viewport()->rect().contains(rect.center()),
				   "the row being measured is actually on screen");
			return rect;
		};
		const auto shallow = rowOf("c1");
		const auto deep    = rowOf("c5");
		expect(shallow.left() == deep.left() && shallow.width() == deep.width(),
			   "a reply five deep occupies the same horizontal extent as a reply "
			   "one deep");

		// Pixels, not settings. A view that stopped indenting while the
		// delegate kept a depth-scaled inset would pass the check above.
		const auto raster       = view->viewport()->grab().toImage();
		const auto rootInset    = painted_inset(raster, rowOf("thread"));
		const auto shallowInset = painted_inset(raster, shallow);
		const auto deepInset    = painted_inset(raster, deep);
		expect(rootInset >= 0 && shallowInset >= 0 && deepInset >= 0,
			   "every measured row actually painted something");
		expect(shallowInset == deepInset,
			   "replies start at the same place whatever their depth");
		expect(rootInset < shallowInset,
			   "a reply is still visibly grouped under its root rather than flush "
			   "with it");
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
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewConversationPanelTests"));
	test_replies_are_a_flat_feed_under_their_root();
	test_discuss_widens_into_list_map_and_feed();
	test_anchoring_keeps_one_file();
	test_replies_have_no_anchor_of_their_own();
	test_changing_the_anchor_follows_immediately();
	test_a_file_without_threads_is_empty();
	test_an_orphan_is_kept_by_its_own_anchor();
	test_exact_source_anchor_filters_file_side_and_line();
	test_unplaced_anchor_wording_is_explicit();
	test_invalid_projected_side_is_never_guessed_as_right();
	std::cout << "conversation-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
