// [[BPR-1328]]. The file and change arrows are a bounded previous and next,
// through the real composition, the workbench host and the Git/cache actor,
// on the constructed nested-folders pull request served by the fake GitHub
// server.
//
// The owner, 2026-10-08: "It doesn't feel good for next change arrows to get
// greyed out when there's only 1 change", then, rejecting arrows that crossed
// into the next file: "Just let it be a bounded prev and next that you can
// always use." The course opens every file of the pull request with Next file
// and, in each, walks Next change to its last change and Previous change back
// to its first. One more press at either end, by arrow or key, changes
// nothing: the same file, the same change, the same comparison. No arrow is
// ever disabled, and an arrow at its bound says so.
//
// The owner's final keys, 2026-10-08: "Okay final choice: option cmd down next
// file. Option down next change". `--attended` opens the nested-folders pull
// request on a one-change file between two others; `--attended-keys` opens the
// inline-drawer pull request on its 1557-line file, where Command-Down and
// Command-Up scroll a comparison longer than the window and the threads offer
// an editor to write in.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QThread>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	/// \brief Where the comparison stands: the Files row of its file and the
	/// 1-based ordinal of its selected change (zero with none selected).
	struct place
	{
		int         Row     = -1;
		std::size_t Change  = 0U;
		std::size_t Changes = 0U;

		auto operator==(const place&) const -> bool = default;
	};

	[[nodiscard]] auto where(const qt::workbench_window& aWindow) -> std::optional<place>
	{
		const auto* surface = aWindow.comparison_surface();
		const auto& result  = surface->current_result();
		const auto& context = surface->current_context();
		if (!result || !context) {
			return std::nullopt;
		}
		const auto* files = aWindow.files_drawer()->contents();
		const auto  row   = files->index_for_file(context->Key.File).row();
		if (row < 0) {
			return std::nullopt;
		}
		const auto& groups    = result->Alignment.Groups;
		const auto& selection = surface->selection();
		auto        change    = std::size_t{ 0U };
		if (selection && selection->Group) {
			const auto found = std::ranges::find(groups, *selection->Group, &core::change_group::Id);
			change           = found == groups.end() ? 0U : static_cast<std::size_t>(std::ranges::distance(groups.begin(), found) + 1);
		}
		return place{ .Row = row, .Change = change, .Changes = groups.size() };
	}

	[[nodiscard]] auto path_at(const qt::workbench_window& aWindow, const int aRow) -> QString
	{
		const auto* files = aWindow.files_drawer()->contents();
		return files->data(files->index(aRow), Qt::DisplayRole).toString();
	}

	/// \brief The arrow's tooltip: its name and key in the platform's
	/// notation, then the line that says where it goes or why it cannot.
	[[nodiscard]] auto says(const QAction* const aAction, const QString& aName, const QString& aDetail) -> bool
	{
		const auto title = QStringLiteral("%1 (%2)").arg(aName, aAction->shortcut().toString(QKeySequence::NativeText));
		return aAction->toolTip() == (aDetail.isEmpty() ? title : title + QLatin1Char{ '\n' } + aDetail)
			&& (aDetail.isEmpty() || aAction->statusTip() == aDetail);
	}

	/// \brief Lets a press that should do nothing have every chance to do
	/// something.
	void settle()
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 250) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	/// \brief [[BPR-1518]] Sends an actual source click after ordinary scrolling.
	void click_line(qt::diff_workbench& aSurface, const core::diff_side aSide, const std::uint32_t aLine)
	{
		auto& source = *(aSide == core::diff_side::left ? aSurface.left_pane() : aSurface.right_pane());
		aSurface.scroll_controller()->home();
		settle();
		const auto row = source.document()->findBlockByNumber(static_cast<int>(aLine));
		expect(row.isValid(), "clicked source line exists");
		const auto point = source.cursorRect(QTextCursor{ row }).center() + QPoint{ 8, 0 };
		expect(source.viewport()->rect().contains(point), "the actual source click is visible");
		auto press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ point }, QPointF{ source.viewport()->mapToGlobal(point) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ point }, QPointF{ source.viewport()->mapToGlobal(point) }, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QCoreApplication::sendEvent(source.viewport(), &press);
		QCoreApplication::sendEvent(source.viewport(), &release);
		settle();
		const auto& selection = aSurface.selection();
		expect(selection && selection->Kind == core::semantic_selection_kind::line
				   && selection->PreferredLine == core::source_line_ref{ aSurface.current_result()->Alignment.Digest, aSide, aLine },
			   "mouse click replaces the prior navigation selection with the exact file, side and source line");
	}

	[[nodiscard]] auto cursor_target(const std::vector<core::change_group>& aGroups,
									 const core::diff_side aSide, const std::uint32_t aLine, const bool aForward)
		-> std::optional<core::change_group_id>
	{
		auto target = std::optional<core::change_group_id>{ };
		for (const auto& group : aGroups) {
			const auto span = aSide == core::diff_side::left ? group.Left : group.Right;
			if (aForward && span.Begin > aLine) {
				return group.Id;
			}
			if (!aForward && span.End <= aLine) {
				target = group.Id;
			}
		}
		return target;
	}

	void render(qt::workbench_window& aWindow, const QString& aName);

	void check_cursor_action(qt::workbench_window& aWindow, const core::diff_side aSide,
							 const std::uint32_t aLine, const bool aForward)
	{
		auto&      surface = *aWindow.comparison_surface();
		const auto result  = surface.current_result();
		const auto context = surface.current_context();
		const auto target  = cursor_target(result->Alignment.Groups, aSide, aLine, aForward);
		click_line(surface, aSide, aLine);
		const auto  clicked     = surface.selection();
		const auto  leftScroll  = surface.left_pane()->verticalScrollBar()->value();
		const auto  rightScroll = surface.right_pane()->verticalScrollBar()->value();
		auto* const action      = aForward ? aWindow.next_change_action() : aWindow.previous_change_action();
		action->trigger();
		settle();
		expect(surface.current_context() == context && surface.current_result() == result, "cursor navigation retains exact file and comparison");
		if (target) {
			const auto expected = core::select_group(*surface.current_semantic_index(), *target);
			expect(expected && surface.selection() == expected, "composed action selects the exact next or previous hunk and canonical source anchor after the clicked line");
			auto&      targetPane = *(expected->PreferredLine.Side == core::diff_side::left ? surface.left_pane() : surface.right_pane());
			const auto block      = targetPane.document()->findBlockByNumber(static_cast<int>(expected->PreferredLine.Line));
			const auto targetRect = targetPane.cursorRect(QTextCursor{ block });
			const auto visible    = targetPane.viewport()->rect().intersects(targetRect);
			if (!visible) {
				std::cerr << "CURSOR_REVEAL file=" << path_at(aWindow, where(aWindow)->Row).toStdString()
						  << " origin=" << static_cast<int>(aSide) << ':' << aLine << " forward=" << aForward
						  << " target=" << static_cast<int>(expected->PreferredLine.Side) << ':' << expected->PreferredLine.Line
						  << " target_rect=" << targetRect.x() << ',' << targetRect.y() << ',' << targetRect.width() << ',' << targetRect.height()
						  << " viewport=" << targetPane.viewport()->width() << ',' << targetPane.viewport()->height() << '\n';
				render(aWindow, QStringLiteral("cursor-reveal-failure"));
			}
			expect(visible, "selected hunk source anchor is visible after the composed action");
			if (aForward && target != result->Alignment.Groups.front().Id) {
				render(aWindow, QStringLiteral("cursor-next-change"));
			}
		} else {
			expect(surface.selection() == clicked && surface.left_pane()->verticalScrollBar()->value() == leftScroll
					   && surface.right_pane()->verticalScrollBar()->value() == rightScroll,
				   "bounded source navigation preserves the clicked line and both visible positions");
			expect(aWindow.statusBar()->currentMessage() == (aForward ? QStringLiteral("Last change in this file.") : QStringLiteral("First change in this file.")), "source position projects the exact directional bound");
		}
	}

	/// \brief Exercises unchanged line origins, both directions and viewport reveal.
	void check_cursor_navigation(qt::workbench_window& aWindow, int& aOrigins, bool& aUnequal)
	{
		auto&      surface = *aWindow.comparison_surface();
		const auto result  = surface.current_result();
		if (result->Alignment.Groups.size() < 2U) {
			return;
		}
		for (const auto& segment : result->Alignment.Segments) {
			if (segment.Kind != core::alignment_kind::equal) {
				continue;
			}
			check_cursor_action(aWindow, core::diff_side::left, segment.Left.Begin, true);
			check_cursor_action(aWindow, core::diff_side::left, segment.Left.Begin, false);
			check_cursor_action(aWindow, core::diff_side::right, segment.Right.Begin, true);
			check_cursor_action(aWindow, core::diff_side::right, segment.Right.Begin, false);
			aOrigins += 4;
			aUnequal = aUnequal || segment.Left.Begin != segment.Right.Begin;
		}
		// Leave the original arrow walk at its documented first-group origin.
		expect(surface.restore_selection(*core::select_group(*surface.current_semantic_index(), result->Alignment.Groups.front().Id)), "course restores its first-group starting point");
	}

	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_CHANGE_ARROWS_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "render directory created");
		expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "composed window render saved");
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	const auto    keys     = aArgc > 1 && std::string_view{ aArgv[1] } == "--attended-keys";
	const auto    attended = keys || (aArgc > 1 && std::string_view{ aArgv[1] } == "--attended");
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-change-arrows-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	const auto                               fixture  = keys ? app::detail::github_public_path_fixture::inline_drawer_v1 : app::detail::github_public_path_fixture::nested_folders_v1;
	auto                                     startup  = access::start(fixture, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composed startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(keys ? QStringLiteral("bpr-fixtures/inline-drawer#22") : QStringLiteral("bpr-fixtures/nested-folders#21"));
	window.home_open_action()->trigger();
	// The accepted pull request applies the opening modality when it lands, so
	// the course states Review only after that.
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "the pull request opens on Overview");

	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, step) != app::detail::course_modality_result::pending; }, "the course enters Review");
	expect(step.Result == app::detail::course_modality_result::entered, "Review shows the comparison and the Files drawer");
	auto* const files = window.files_drawer()->contents();
	wait_for([&] { return files->rowCount() > 2; }, "the pull request's files are listed");
	const auto rows = files->rowCount();

	auto* const next     = window.next_change_action();
	auto* const previous = window.previous_change_action();
	window.next_file_action()->trigger();
	wait_for([&] {
		const auto now = where(window);
		return now && now->Row == 0 && now->Change == 1U;
	},
			 "Next file opens the first file on its first change");
	expect(window.current_modality() == qt::review_modality::review && !window.showing_overview(),
		   "the comparison is shown in Review");
	const auto keyOf = [](const char* const aKey) { return QKeySequence{ QString::fromLatin1(aKey) }.toString(QKeySequence::NativeText); };
	expect(next->shortcuts().value(0) == QKeySequence{ QStringLiteral("Alt+Down") } && previous->shortcuts().value(0) == QKeySequence{ QStringLiteral("Alt+Up") }
			   && next->toolTip().startsWith(QStringLiteral("Next change (%1)").arg(keyOf("Alt+Down")))
			   && previous->toolTip().startsWith(QStringLiteral("Previous change (%1)").arg(keyOf("Alt+Up")))
			   && window.next_file_action()->toolTip().startsWith(QStringLiteral("Next file (%1)").arg(keyOf("Ctrl+Alt+Down")))
			   && window.previous_file_action()->toolTip().startsWith(QStringLiteral("Previous file (%1)").arg(keyOf("Ctrl+Alt+Up"))),
		   "each arrow's tooltip names its key: Option-Down and Option-Up for the changes, Option-Command-Down and Option-Command-Up for the files");
	auto* const nextFile     = window.next_file_action();
	auto* const previousFile = window.previous_file_action();
	const auto  allEnabled   = [&] { return next->isEnabled() && previous->isEnabled() && nextFile->isEnabled() && previousFile->isEnabled(); };
	expect(allEnabled() && says(previous, QStringLiteral("Previous change"), QStringLiteral("First change in this file."))
			   && says(previousFile, QStringLiteral("Previous file"), QStringLiteral("First file in this pull request.")),
		   "the first change of the first file keeps every arrow enabled and names the bounds");

	// One press at a bound: nothing moves and nothing reloads.
	auto       bounds       = 0;
	const auto pressAtBound = [&](QAction* const aAction, const QString& aMessage, const char* aWhat) {
		const auto  here   = *where(window);
		const auto* before = window.comparison_surface()->current_result().get();
		window.statusBar()->clearMessage();
		aAction->trigger();
		settle();
		expect(where(window) == here && window.comparison_surface()->current_result().get() == before, aWhat);
		expect(allEnabled() && window.statusBar()->currentMessage() == aMessage, "the arrow stays enabled and the status bar says why nothing moved");
		++bounds;
	};
	if (!attended) {
		pressAtBound(previous, QStringLiteral("First change in this file."), "Previous change at the first change of the first file does nothing");
		pressAtBound(previousFile, QStringLiteral("First file in this pull request."), "Previous file on the first file does nothing");
	}

	if (attended) {
		// The owner looks at a file with one change between two others, the
		// case the note was about, or for the keys at the long file.
		const auto wanted = QStringLiteral("src/ui/conversation_cards_view.cpp");
		for (auto presses = 0; presses < 200; ++presses) {
			const auto here = *where(window);
			if (keys ? path_at(window, here.Row).endsWith(QStringLiteral("conversation_cards_view.cpp")) && here.Change == 1U
					 : here.Changes == 1U && here.Row > 0 && here.Row + 1 < rows) {
				break;
			}
			expect(here.Row + 1 < rows, ("the walk finds " + (keys ? wanted : QStringLiteral("a one-change file between two others")).toStdString()).c_str());
			nextFile->trigger();
			wait_for([&] { return where(window) && where(window)->Row == here.Row + 1; }, "the attended walk moves");
		}
		std::cout << "CHANGE_ARROWS_ATTENDED_READY source=synthetic fixture=" << (keys ? "inline-drawer-v1" : "nested-folders-v1") << " file=" << path_at(window, where(window)->Row).toStdString() << '\n'
				  << std::flush;
		const auto result = QApplication::exec();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "attended shutdown drains");
		application.reset();
		expect(e2e::remove_course_root(temporary), "attended private root cleaned");
		return result;
	}

	// Every file, in order: Next change to its last change, Previous change
	// back to its first, one more press at each end.
	auto cursorOrigins = 0;
	auto unequalCursor = false;
	auto steps         = 0;
	auto oneChangeMid  = 0;
	for (auto row = 0; row < rows; ++row) {
		const auto previousSelection = window.comparison_surface()->selection();
		if (row > 0) {
			const auto from = where(window)->Row;
			nextFile->trigger();
			wait_for([&] { return where(window) && where(window)->Row == from + 1; }, "Next file opens the next file");
		}
		wait_for([&] {
			const auto now = where(window);
			return now && now->Row == row && now->Change == (now->Changes == 0U ? 0U : 1U);
		},
				 "each file opens on its first change");
		if (row > 0 && previousSelection) {
			expect(!window.comparison_surface()->restore_selection(*previousSelection), "replacement comparison refuses the previous file's source origin");
		}
		const auto opened = *where(window);
		if (opened.Changes == 1U && row > 0 && row + 1 < rows) {
			expect(allEnabled() && says(next, QStringLiteral("Next change"), QStringLiteral("Last change in this file."))
					   && says(previous, QStringLiteral("Previous change"), QStringLiteral("First change in this file.")),
				   "a one-change file between two others keeps both change arrows enabled and names both bounds");
			if (oneChangeMid++ == 0) {
				render(window, QStringLiteral("one-change-middle"));
			}
		}
		if (opened.Changes == 0U) {
			continue;
		}
		while (where(window)->Change < where(window)->Changes) {
			const auto here = *where(window);
			expect(says(next, QStringLiteral("Next change"), QString{ }), "Next change with a change after it names no bound");
			next->trigger();
			wait_for([&] { return where(window) && *where(window) == place{ .Row = here.Row, .Change = here.Change + 1U, .Changes = here.Changes }; },
					 "Next change steps inside the comparison");
			++steps;
		}
		expect(says(next, QStringLiteral("Next change"), QStringLiteral("Last change in this file.")), "the last change names its bound");
		pressAtBound(next, QStringLiteral("Last change in this file."), "Next change at a file's last change stays on it: no other file, no wrap");
		while (where(window)->Change > 1U) {
			const auto here = *where(window);
			previous->trigger();
			wait_for([&] { return where(window) && *where(window) == place{ .Row = here.Row, .Change = here.Change - 1U, .Changes = here.Changes }; },
					 "Previous change steps inside the comparison");
			++steps;
		}
		pressAtBound(previous, QStringLiteral("First change in this file."), "Previous change at a file's first change stays on it");
		check_cursor_navigation(window, cursorOrigins, unequalCursor);
	}
	expect(where(window)->Row == rows - 1 && says(nextFile, QStringLiteral("Next file"), QStringLiteral("Last file in this pull request.")),
		   "the walk ends on the last file, and Next file names its bound");
	pressAtBound(nextFile, QStringLiteral("Last file in this pull request."), "Next file on the last file does nothing");
	expect(cursorOrigins >= 8 && unequalCursor, "composed cursor course covers both sides, both directions and unequal line mappings");
	expect(oneChangeMid > 0, "the walk passed a one-change file between two others");
	render(window, QStringLiteral("last-change"));

	// A replacement pair in the same file cannot retain the old clicked origin.
	auto& surface = *window.comparison_surface();
	click_line(surface, core::diff_side::right, 0U);
	const auto  staleSelection = *surface.selection();
	const auto  staleKey       = surface.current_context()->Key;
	auto* const epochs         = window.epoch_selectors();
	auto        baseRow        = -1;
	for (auto row = 0; row < epochs->history_model()->rowCount(); ++row) {
		const auto anchor = epochs->history_model()->anchor_at(epochs->history_model()->index(row, 0));
		if (anchor && anchor->Identity.Kind == core::comparison_anchor_kind::epoch_base) {
			baseRow = row;
			break;
		}
	}
	expect(baseRow >= 0, "same-file replacement has a retained base endpoint");
	epochs->right_selector()->setCurrentIndex(baseRow);
	wait_for([&] { return surface.current_result() && !surface.geometry_pending()
					   && surface.current_result()->Alignment.Digest != staleSelection.Comparison
					   && !access::epoch_refresh_identity(*application).TabPending; }, "same-file base/base comparison replaces old material");
	expect(surface.current_context()->Key.File == staleKey.File && !surface.restore_selection(staleSelection), "the same file's newer comparison refuses the old exact source selection");
	click_line(surface, core::diff_side::right, 0U);
	const auto replacementSelection = surface.selection();
	for (auto* const action : { next, previous }) {
		action->trigger();
		settle();
		expect(surface.selection() == replacementSelection && window.statusBar()->currentMessage() == QStringLiteral("This comparison has no changes to navigate."), "both composed directions retain the replacement comparison's source origin and no-changes bound");
	}

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "composed course shuts down and drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "composed private course root cleaned");
	std::cout << "CHANGE_ARROWS_PUBLIC_PATH_OK qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString()
			  << " files=" << rows << " bounds=" << bounds << " steps=" << steps << " one_change_middle=" << oneChangeMid << " cursor_origins=" << cursorOrigins << " unequal_cursor=" << unequalCursor << '\n';
}
