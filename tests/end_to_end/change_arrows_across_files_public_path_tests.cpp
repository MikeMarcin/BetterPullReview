// [[BPR-1328]]. The change arrows continue across files, through the real
// composition, the workbench host and the Git/cache actor, on the constructed
// nested-folders pull request served by the fake GitHub server.
//
// The owner, 2026-10-08: "It doesn't feel good for next change arrows to get
// greyed out when there's only 1 change". The course walks Next change from
// the first change of the first file to the last change of the last, and
// Previous change all the way back. Each press either steps inside the open
// comparison or crosses into the neighbouring file, entering it at its first
// change going forward and its last going back. Only the two ends of the pull
// request disable an arrow, and each says why.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

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
	const auto    attended = aArgc > 1 && std::string_view{ aArgv[1] } == "--attended";
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-change-arrows-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::nested_folders_v1, options, [&](auto aResult) {
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
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
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
	expect(next->shortcuts().value(0) == QKeySequence{ QStringLiteral("Ctrl+Down") } && previous->shortcuts().value(0) == QKeySequence{ QStringLiteral("Ctrl+Up") }
			   && next->toolTip().startsWith(QStringLiteral("Next change (%1)").arg(keyOf("Ctrl+Down")))
			   && previous->toolTip().startsWith(QStringLiteral("Previous change (%1)").arg(keyOf("Ctrl+Up")))
			   && window.next_file_action()->toolTip().startsWith(QStringLiteral("Next file (%1)").arg(keyOf("Alt+Down")))
			   && window.previous_file_action()->toolTip().startsWith(QStringLiteral("Previous file (%1)").arg(keyOf("Alt+Up"))),
		   "each arrow's tooltip names its key: Command-Down and Command-Up for the changes, Option-Down and Option-Up for the files");
	expect(!previous->isEnabled() && says(previous, QStringLiteral("Previous change"), QStringLiteral("First change in the pull request.")),
		   "the first change of the first file disables Previous change and says why");

	if (attended) {
		// The owner looks at a file with one change between two others: the
		// case the note was about.
		for (auto presses = 0; presses < 200; ++presses) {
			const auto here = *where(window);
			if (here.Changes == 1U && here.Row > 0 && here.Row + 1 < rows) {
				break;
			}
			next->trigger();
			wait_for([&] { return where(window) && *where(window) != here; }, "the attended walk moves");
		}
		std::cout << "CHANGE_ARROWS_ATTENDED_READY source=synthetic fixture=nested-folders-v1 file=" << path_at(window, where(window)->Row).toStdString() << '\n'
				  << std::flush;
		const auto result = QApplication::exec();
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "attended shutdown drains");
		application.reset();
		expect(e2e::remove_course_root(temporary), "attended private root cleaned");
		return result;
	}

	// Forward: every change of the pull request, in order, across every file.
	auto crossings    = 0;
	auto steps        = 0;
	auto oneChangeMid = 0;
	for (auto presses = 0; presses < 400 && next->isEnabled(); ++presses) {
		const auto here    = *where(window);
		const auto crosses = here.Change >= here.Changes;
		if (here.Changes == 1U && here.Row > 0 && here.Row + 1 < rows) {
			expect(previous->isEnabled() && next->isEnabled(), "a one-change file between two others keeps both change arrows enabled");
			if (oneChangeMid++ == 0) {
				render(window, QStringLiteral("one-change-middle"));
			}
		}
		if (crosses) {
			expect(says(next, QStringLiteral("Next change"), QStringLiteral("Continue to the next file: %1").arg(path_at(window, here.Row + 1))),
				   "Next change at a file's last change names the next file");
		}
		next->trigger();
		wait_for([&] {
			const auto now = where(window);
			return now && (crosses ? now->Row == here.Row + 1 && now->Change == (now->Changes == 0U ? 0U : 1U) : now->Row == here.Row && now->Change == here.Change + 1U);
		},
				 crosses ? "Next change continues to the first change of the next file" : "Next change steps inside the comparison");
		(crosses ? crossings : steps) += 1;
	}
	const auto end = *where(window);
	expect(end.Row == rows - 1 && end.Change == end.Changes && crossings == rows - 1,
		   "Next change walks to the last change of the last file, crossing each file once");
	expect(!next->isEnabled() && says(next, QStringLiteral("Next change"), QStringLiteral("Last change in the pull request.")) && previous->isEnabled(),
		   "only the last change of the last file disables Next change, and says why");
	expect(oneChangeMid > 0, "the walk passed a one-change file between two others");
	render(window, QStringLiteral("last-change"));

	// Back: Previous change enters each previous file at its last change.
	auto returns = 0;
	for (auto presses = 0; presses < 400 && previous->isEnabled(); ++presses) {
		const auto here    = *where(window);
		const auto crosses = here.Change <= 1U;
		if (crosses) {
			expect(says(previous, QStringLiteral("Previous change"), QStringLiteral("Go back to the previous file: %1").arg(path_at(window, here.Row - 1))),
				   "Previous change at a file's first change names the previous file");
		}
		previous->trigger();
		wait_for([&] {
			const auto now = where(window);
			return now && (crosses ? now->Row == here.Row - 1 && now->Change == now->Changes : now->Row == here.Row && now->Change == here.Change - 1U);
		},
				 crosses ? "Previous change goes back to the last change of the previous file" : "Previous change steps inside the comparison");
		returns += crosses ? 1 : 0;
	}
	const auto start = *where(window);
	expect(start.Row == 0 && start.Change == 1U && returns == rows - 1,
		   "Previous change walks back to the first change of the first file");
	expect(!previous->isEnabled() && says(previous, QStringLiteral("Previous change"), QStringLiteral("First change in the pull request.")),
		   "the walk ends where the pull request begins");

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "composed course shuts down and drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "composed private course root cleaned");
	std::cout << "CHANGE_ARROWS_PUBLIC_PATH_OK qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString()
			  << " files=" << rows << " crossings=" << crossings << " steps=" << steps << " one_change_middle=" << oneChangeMid << '\n';
}
