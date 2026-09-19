#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_thread_map.hpp"

#include <QApplication>
#include <QComboBox>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollBar>
#include <QToolButton>
#include <QTreeView>
#include <QWheelEvent>

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
		panel.resize(1400, 900);
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
				   && panel.discuss_summary()->text() == QStringLiteral("1 conversations, 1 unresolved, 0 new"),
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
	auto test_anchor_round_trip_preserves_identical_projection() -> void
	{
		qt::github_conversation_panel panel;
		const auto                    projection = std::vector{ root("t", "src/a.cpp"), reply("p", "t") };
		panel.adopt(projection, QStringLiteral("available"));
		panel.view()->setCurrentIndex(panel.model()->index_for_id("p"));
		panel.view()->collapse(panel.model()->index_for_id("t"));
		const auto measurements  = panel.cards()->measurements();
		const auto mapStatistics = panel.thread_map()->statistics();
		for (auto refresh = 0; refresh < 4; ++refresh) {
			panel.set_anchor_file(core::file_identity_of("src/a.cpp"));
			panel.set_anchor_file(std::nullopt);
		}
		expect(panel.cards()->measurements() == measurements
				   && panel.model()->id_for_index(panel.view()->currentIndex()) == "p"
				   && !panel.view()->isExpanded(panel.model()->index_for_id("t"))
				   && panel.cards()->current_thread() == "t",
			   "equivalent anchor projections retain measurements, selected reply and expansion");
		expect(panel.thread_map()->statistics().Layouts == mapStatistics.Layouts,
			   "equivalent anchor projections do not rebuild the thread map");
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
		expect(panel.cards()->current_thread() == "right-root" && panel.thread_map()->focus_identity() == "right-reply",
			   "exact post selection highlights its owning compact index entry and graph node");
		for (auto row = 0; row < panel.model()->rowCount(); ++row) {
			const auto id = panel.model()->id_for_index(panel.model()->index(row, 0));
			expect(panel.view()->isRowHidden(row, { }) == (id != "right-root"), "only selected conversation has full bodies in detail");
		}
		for (const auto& card : panel.cards()->cards()) {
			expect(std::ranges::none_of(card.Actions, [](const auto& aAction) { return aAction.first == qt::card_action::open_in_code; }),
				   "unavailable exact endpoints never offer open in code");
		}
		panel.set_current_endpoint_placement(true, true);
		expect(std::ranges::any_of(panel.cards()->cards(), [](const qt::thread_card& aCard) {
				   return aCard.Id == "right-root" && std::ranges::any_of(aCard.Actions, [](const auto& aAction) { return aAction.first == qt::card_action::open_in_code; });
			   }),
			   "restored exact endpoints invalidate cached action geometry and offer open in code");
		panel.set_current_endpoint_placement(false, false);
		expect(std::ranges::all_of(panel.cards()->cards(), [](const qt::thread_card& aCard) {
				   return std::ranges::none_of(aCard.Actions, [](const auto& aAction) { return aAction.first == qt::card_action::open_in_code; });
			   }),
			   "losing exact endpoint facts removes cached open-in-code actions");
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
		expect(panel.model()->rowCount() == 0 && panel.cards()->cards().empty() && panel.thread_map()->focus_identity().empty(),
			   "authority clear removes every prior conversation identity");
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

	void test_index_arrows_retain_focus_until_explicit_activation()
	{
		qt::github_conversation_panel panel;
		panel.resize(500, 900);
		panel.adopt(two_files(), QStringLiteral("Accepted conversations"));
		panel.set_discuss(true);
		panel.show();
		panel.activateWindow();
		panel.cards()->setFocus(Qt::OtherFocusReason);
		panel.cards()->set_current_thread("left-root");
		process_events();

		expect(panel.cards()->isVisible() && !panel.discuss_column()->isVisible()
				   && panel.cards()->hasFocus(),
			   "narrow Discuss begins in the focused conversation index");
		auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QApplication::sendEvent(panel.cards(), &down);
		process_events();
		expect(panel.cards()->current_thread() == "right-root"
				   && panel.cards()->isVisible() && !panel.discuss_column()->isVisible()
				   && panel.cards()->hasFocus(),
			   "Down advances the index without stealing focus or opening detail");
		QApplication::sendEvent(panel.cards(), &down);
		process_events();
		expect(panel.cards()->current_thread() == "left-root"
				   && panel.cards()->isVisible() && !panel.discuss_column()->isVisible()
				   && panel.cards()->hasFocus(),
			   "repeated arrow navigation remains in the index and wraps by identity");

		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(panel.cards(), &enter);
		process_events();
		expect(!panel.cards()->isVisible() && panel.discuss_column()->isVisible()
				   && panel.view()->hasFocus(),
			   "Return explicitly opens the selected full-body detail and moves focus once");
	}

	void test_long_detail_scroll_routes()
	{
		for (const auto points : { 13, 20 }) {
			for (const auto width : { 900, 1400 }) {
				qt::github_conversation_panel panel;
				auto                          font = panel.font();
				font.setPointSize(points);
				panel.setFont(font);
				panel.resize(width, 900);
				auto data = two_files();
				data[1].Body.clear();
				for (auto line = 0; line < 120; ++line) {
					data[1].Body += "A complete long comment line with a stable reading position.\n";
				}
				data[1].Body += "FINAL SENTINEL";
				panel.adopt(data, QStringLiteral("Accepted"));
				panel.set_discuss(true);
				panel.show();
				process_events();
				// Enter detail through the index, including the narrow page route.
				auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
				QApplication::sendEvent(panel.cards(), &enter);
				panel.view()->setCurrentIndex(panel.model()->index_for_id("left-reply"));
				process_events();
				auto* const view = panel.view();
				auto* const bar  = view->verticalScrollBar();
				expect(view->verticalScrollMode() == QAbstractItemView::ScrollPerPixel && bar->maximum() > view->viewport()->height(),
					   "one long post has a pixel reading range beyond a viewport");
				const auto compactHeight = panel.cards()->cards().front().Rect.height();
				expect(compactHeight < 400, "compact index height is independent of long body length");
				bar->setValue(0);
				for (const auto key : { Qt::Key_PageDown, Qt::Key_Space }) {
					const auto before = bar->value();
					auto       event  = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
					QApplication::sendEvent(view, &event);
					expect(bar->value() > before, "paging scrolls within the same long comment");
				}
				const auto saved = bar->value();
				data.back().Body += " refreshed";
				panel.adopt(data, QStringLiteral("Refreshed"));
				process_events();
				expect(bar->value() == saved && panel.model()->id_for_index(view->currentIndex()) == "left-reply", "refresh preserves reading offset and exact post");
				panel.set_discuss(false);
				panel.set_discuss(true);
				process_events();
				expect(bar->value() == saved && panel.model()->id_for_index(view->currentIndex()) == "left-reply", "mode round trip preserves reading offset and exact post");
				const auto readingPost = panel.model()->id_for_index(view->indexAt(QPoint{ 1, 1 }));
				panel.resize(width + 100, 900);
				process_events();
				expect(panel.model()->id_for_index(view->indexAt(QPoint{ 1, 1 })) == readingPost, "resize retains the logical post being read");
				auto end = QKeyEvent{ QEvent::KeyPress, Qt::Key_End, Qt::NoModifier };
				QApplication::sendEvent(view, &end);
				expect(bar->value() == bar->maximum(), "End reaches the final full-body extent");
				expect(view->visualRect(panel.model()->index_for_id("left-deep")).bottom() <= view->viewport()->height(), "the final reply fits at the end of the reading range");
				auto boundary = QWheelEvent{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, QPoint{ 0, -45 }, QPoint{ }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
				QApplication::sendEvent(view->viewport(), &boundary);
				expect(boundary.isAccepted() && bar->value() == bar->maximum(), "the detail consumes boundary wheel input without transferring ownership");
				auto pageUp = QKeyEvent{ QEvent::KeyPress, Qt::Key_PageUp, Qt::NoModifier };
				QApplication::sendEvent(view, &pageUp);
				expect(bar->value() < bar->maximum(), "PageUp moves backward inside the long post");
				auto home = QKeyEvent{ QEvent::KeyPress, Qt::Key_Home, Qt::NoModifier };
				QApplication::sendEvent(view, &home);
				for (const auto pixels : { false, true }) {
					const auto before = bar->value();
					auto       wheel  = QWheelEvent{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, pixels ? QPoint{ 0, -45 } : QPoint{ }, pixels ? QPoint{ } : QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
					QApplication::sendEvent(view->viewport(), &wheel);
					expect(wheel.isAccepted() && bar->value() > before, "angle and pixel wheel events advance the detail owner");
				}
				std::cout << "long-detail width=" << width << " font=" << points << " scale=" << panel.devicePixelRatioF() << " range=" << bar->maximum() << '\n';
			}
		}
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
	test_anchor_and_unchanged_refresh_measure_cards_once();
	test_anchor_round_trip_preserves_identical_projection();
	test_a_file_without_threads_is_empty();
	test_an_orphan_is_kept_by_its_own_anchor();
	test_exact_source_anchor_filters_file_side_and_line();
	test_unplaced_anchor_wording_is_explicit();
	test_invalid_projected_side_is_never_guessed_as_right();
	test_pr_wide_index_and_selected_detail();
	test_contextual_scope_survives_clear_and_refresh();
	test_index_arrows_retain_focus_until_explicit_activation();
	test_long_detail_scroll_routes();
	std::cout << "conversation-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
