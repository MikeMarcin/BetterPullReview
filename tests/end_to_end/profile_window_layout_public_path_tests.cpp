#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/application/home_view_state.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QScreen>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTextEdit>
#include <QThread>
#include <QToolButton>
#include <QTreeView>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage,
				  const std::function<void()>& aOnFailure = { })
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aOnFailure) {
			aOnFailure();
		}
		expect(aCondition(), aMessage);
	}
	void trigger(qt::workbench_window& aWindow, const char* aName)
	{
		auto* action = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(action != nullptr, "public action exists");
		wait_for([&] { return action->isEnabled(); }, "public action enabled");
		action->trigger();
	}
	void open_review(app::github_application& aOwner, qt::workbench_window& aWindow, int aNumber, bool aSignIn)
	{
		aWindow.resize(1800, 900);
		// [[BPR-0740]]. Since BPR-0733 a restarted window shows Home with the
		// saved reading under Recently viewed rather than reopening it, so the
		// paste field is driven from Home in every phase, as the sibling
		// courses do.
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		// [[BPR-0732]]. Where preview is composed a pasted public link no
		// longer stops at sign-in: it opens signed out, and the github_com
		// fixture answers the anonymous read of one pull request once, not the
		// independent and repeated reads this course makes. The account course
		// signs in from the landing card first, as the restoration course does;
		// an account opens the destination without a stop.
		// Only the first window of a phase signs in, and it signs in before its
		// first paste: `PublicPreviewSignedOut` is still false until that paste
		// activates the shipped signed-out session, so it cannot select the
		// sign-in, and a sibling that signs in later enrols a second grant
		// whose identity change retires every window's session, including the
		// first window's anonymous review (settled under this card, d18).
		expect(!aSignIn || aWindow.home_landing()->state() == qt::landing_state::sign_in, "first window of a phase lands signed out");
		if (aSignIn) {
			aWindow.home_landing()->primary_button()->click();
			wait_for([&] {
				return aWindow.session_panel()->status() == core::github_status::session_ready
					&& !aOwner.status().PublicPreviewSignedOut;
			},
					 "Home sign-in reaches an account session");
		}
		aWindow.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/%1").arg(aNumber));
		aWindow.home_open_action()->trigger();
		wait_for([&] { return aWindow.session_panel()->status() == core::github_status::pull_request_ready; },
				 "exact public review opens", [&] { std::cerr << "status=" << aWindow.session_panel()->status_text()
															  << " diagnostic=" << aWindow.session_panel()->diagnostic_text()
															  << " notice=" << aWindow.home_open_notice()->text().toStdString()
															  << " home=" << aWindow.showing_home() << '\n'; });
		aWindow.modality()->action(qt::review_modality::review)->trigger();
		wait_for([&] { return aWindow.central_splitter()->isVisible() && aWindow.line_numbers_action()->isEnabled(); }, "review drawer actions available");
		QApplication::processEvents();
	}
	auto resize_summary(qt::workbench_window& aWindow, const int aDirection) -> std::uint32_t
	{
		aWindow.modality()->action(qt::review_modality::overview)->trigger();
		wait_for([&] { return aWindow.showing_overview(); }, "composed Overview appears");
		auto* page  = aWindow.overview_page();
		auto* split = page->findChild<QSplitter*>(QStringLiteral("overviewSummarySplitter"));
		wait_for([&] { return split && split->handle(1)->isVisible(); }, "summary seam visible on the accepted description");
		auto* grip = split->handle(1);
		grip->setFocus();
		for (int line = 0; line < 8; ++line) {
			QKeyEvent key{ QEvent::KeyPress, aDirection > 0 ? Qt::Key_Down : Qt::Key_Up, Qt::NoModifier };
			QApplication::sendEvent(grip, &key);
			QApplication::processEvents();
		}
		wait_for([&] { return page->summary_fraction().has_value(); }, "explicit summary choice is retained");
		const auto choice = *page->summary_fraction();
		// [[BPR-1333]]. Since 1f81ec5b4 one top-right control in the reader's
		// corner enters and leaves the full reader; the two named push buttons
		// are gone, and since dbbccdf6e it is not checkable, so its name says
		// which way it goes. Look it up and check it before use, so a renamed
		// control fails here by name instead of faulting in
		// QAbstractButton::click.
		auto* fullView = page->findChild<QToolButton*>(QStringLiteral("overviewFullView"));
		expect(fullView != nullptr && fullView->isVisible() && fullView->accessibleName() == QStringLiteral("Enter full view"),
			   "composed Overview offers its full-view control");
		fullView->click();
		wait_for([&] { return fullView->accessibleName() == QStringLiteral("Exit full view") && !page->files_view()->isVisible(); },
				 "composed full description opens");
		auto* document = page->body_view()->rendered_view()->document();
		fullView->click();
		wait_for([&] { return fullView->accessibleName() == QStringLiteral("Enter full view") && page->files_view()->isVisible(); },
				 "full reader returns to composed Files");
		expect(page->summary_fraction() == choice && page->body_view()->rendered_view()->document() == document, "full reader keeps the split preference and reader document");
		aWindow.modality()->action(qt::review_modality::review)->trigger();
		return choice;
	}

	auto drag_files(qt::workbench_window& aWindow, int aWidth) -> int
	{
		auto*      splitter = aWindow.central_splitter();
		const auto before   = aWindow.files_drawer()->width();
		splitter->move_handle(2, aWindow.files_drawer()->x() + aWidth);
		wait_for([&] { return aWindow.files_drawer()->width() != before; }, "real splitter drag changes file width");
		return aWindow.files_drawer()->width();
	}
	/// \brief [[BPR-1334]]. Makes A's Line details taller with two presses of
	/// Up on its seam, as a keyboard reader would, and returns the height the
	/// window now prefers.
	auto grow_line_details(qt::workbench_window& aWindow) -> std::uint32_t
	{
		auto* const surface = aWindow.comparison_surface();
		wait_for([&] { return surface->line_details_visible() && surface->line_details()->isVisible() && surface->line_details_handle() != nullptr; },
				 "Line details shows under the comparison by default");
		auto* const seam = surface->line_details_handle();
		seam->setFocus();
		for (auto press = 0; press < 2; ++press) {
			auto up      = QKeyEvent{ QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier };
			auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Up, Qt::NoModifier };
			QApplication::sendEvent(seam, &up);
			QApplication::sendEvent(seam, &release);
		}
		wait_for([&] { return surface->line_details_height().has_value(); }, "the seam reports an explicit height");
		return *surface->line_details_height();
	}
	auto select_window(app::profile_desktop& aDesktop, std::uint64_t aId) -> qt::workbench_window*
	{
		for (const auto context : aDesktop.windows(core::default_bpr_profile_id)) {
			if (context.Window.value() == aId) {
				return aDesktop.window(context);
			}
		}
		return nullptr;
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 3, "phase and private root required");
	const auto   phase = std::string{ aArgv[1] };
	const auto   root  = std::filesystem::canonical(aArgv[2]);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto                   layoutUnavailable = QStringLiteral("The saved workbench layout is unavailable.");
	std::array<std::uint64_t, 7> saved{ }; // A/B identities and chosen files widths, summaries, A's Line details height.
	std::array<QRect, 2>         geometry;
	if (phase != "seed") {
		std::ifstream input{ root / "layout-expectations.txt" };
		for (auto& value : saved) {
			input >> value;
			expect(input.good() && value > 0, "seed expectations readable");
		}
		std::ifstream placements{ root / "geometry-expectations.txt" };
		for (auto& rectangle : geometry) {
			int x = 0, y = 0, width = 0, height = 0;
			placements >> x >> y >> width >> height;
			expect(placements.good() && width > 0 && height > 0, "seed native placement observations readable");
			rectangle = { x, y, width, height };
		}
	}
	auto options                      = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = root;
	options.RegisteredDeviceCourse    = true;
	options.IndependentWindowReads    = true;
	options.RepeatReadSessions        = true;
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "profile startup terminal");
	expect(owner != nullptr, "optional layout permits desktop startup");
	qt::workbench_window* quitWindow = nullptr;
	if (phase == "broken") {
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 2; }, "optional layout failure retains admitted identities");
		auto* b = select_window(*desktop, saved[1]);
		expect(b != nullptr && b->showing_home(), "broken optional layout leaves usable Home");
		// [[BPR-0742]]. The fallback is not silent. Every restarted window says
		// on its status line that the saved layout is unavailable, after the
		// startup layout publication that would clear a one-shot failure.
		for (const auto context : desktop->windows(core::default_bpr_profile_id)) {
			auto* restarted = desktop->window(context);
			expect(restarted != nullptr, "restarted window available");
			wait_for([&] { return restarted->statistics().Publications > 0U && restarted->status_text().contains(layoutUnavailable); },
					 "a restarted window tells the user the saved layout is unavailable",
					 [&] { std::cerr << "window=" << context.Window.value() << " publications=" << restarted->statistics().Publications
									 << " failures=" << restarted->statistics().Failures
									 << " status=" << restarted->status_text().toStdString() << '\n'; });
		}
		const auto homePublications = b->statistics().Publications;
		// [[BPR-0740]]. Since BPR-0718 a store that cannot open at composition
		// yields the composed defaults for every window rather than a per-window
		// load failure; the unreadable store is preserved (the contract checks
		// its version and the allocator) and the window reads with the profile
		// defaults, not its saved width.
		open_review(*owner, *b, 18, true);
		expect(b->files_drawer()->width() == 320 && static_cast<std::uint64_t>(b->files_drawer()->width()) != saved[3], "unreadable optional layout yields the composed defaults, not the saved width");
		expect(b->statistics().Publications > homePublications && b->status_text().contains(layoutUnavailable),
			   "the unavailable layout sentence survives the review's workbench projections");
		quitWindow = b;
	} else if (phase == "seed") {
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 1; }, "first window admitted");
		saved[0] = desktop->windows(core::default_bpr_profile_id).front().Window.value();
		auto* a  = select_window(*desktop, saved[0]);
		expect(a != nullptr, "first window visible");
		open_review(*owner, *a, 17, true);
		trigger(*a, "newBprWindowAction");
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 2; }, "second window admitted");
		for (const auto context : desktop->windows(core::default_bpr_profile_id)) {
			if (context.Window.value() != saved[0]) {
				saved[1] = context.Window.value();
			}
		}
		auto* b = select_window(*desktop, saved[1]);
		expect(b != nullptr, "second window visible");
		open_review(*owner, *b, 18, false);
		saved[4] = resize_summary(*a, 1);
		saved[5] = resize_summary(*b, -1);
		expect(saved[4] != saved[5], "windows choose distinct summary fractions");
		// [[BPR-1588]]. B hides its Status drawer from the drawer's own Hide;
		// A keeps its open.
		b->modality()->action(qt::review_modality::overview)->trigger();
		wait_for([&] { return b->showing_overview() && b->overview_page()->status_drawer()->hide_button()->isVisible(); }, "B's Status drawer shows on the Overview");
		b->overview_page()->status_drawer()->hide_button()->click();
		wait_for([&] { return b->overview_page()->status_drawer()->rail()->isVisible(); }, "Hide leaves B's Status rail");
		b->modality()->action(qt::review_modality::review)->trigger();
		// [[BPR-1334]]. A chooses a Line details height; then B hides Line
		// details, which is the profile's choice and so hides it in A too.
		saved[6] = grow_line_details(*a);
		b->line_details_action()->trigger();
		wait_for([&] { return !a->line_details_action()->isChecked() && !b->line_details_action()->isChecked()
						   && !a->comparison_surface()->line_details_visible() && !b->comparison_surface()->line_details_visible(); },
				 "View > Line Details hides the strip in every window of the profile");
		expect(a->comparison_surface()->line_details_height() == std::optional{ static_cast<std::uint32_t>(saved[6]) }
				   && !b->comparison_surface()->line_details_height().has_value(),
			   "the height stays A's own");
		saved[2] = static_cast<std::uint64_t>(drag_files(*a, 370));
		saved[3] = static_cast<std::uint64_t>(drag_files(*b, 430));
		expect(saved[2] != saved[3], "windows have distinct chosen drawer widths");
		a->conversations_drawer_toggle_action()->trigger();
		b->repositories_drawer_toggle_action()->trigger();
		expect(!a->conversations_drawer_toggle_action()->isChecked() && b->conversations_drawer_toggle_action()->isChecked(), "drawer state remains independent");
		expect(a->repositories_drawer_toggle_action()->isChecked() && !b->repositories_drawer_toggle_action()->isChecked(), "second drawer state remains independent");
		std::ofstream output{ root / "layout-expectations.txt" };
		for (const auto value : saved) {
			output << value << '\n';
		}
		output.close();
		expect(output.good(), "seed chosen layout observations saved");
		a->one_pane_action()->trigger();
		a->line_numbers_action()->trigger(); // Quit immediately, without waiting for persistence.
		// The toggle is the host's decision projected back; the quit waits for
		// the projection, not for the store.
		wait_for([&] { return !a->line_numbers_action()->isChecked() && b->line_numbers_action()->isChecked() && a->one_pane_action()->isChecked() && !b->one_pane_action()->isChecked(); },
				 "last accepted layout change belongs only to A", [&] { std::cerr << "line numbers a=" << a->line_numbers_action()->isChecked()
																				  << " b=" << b->line_numbers_action()->isChecked()
																				  << " a enabled=" << a->line_numbers_action()->isEnabled()
																				  << " a home=" << a->showing_home()
																				  << " a status=" << a->status_text().toStdString()
																				  << " a session=" << a->session_panel()->status_text() << '\n'; });
		// [[BPR-0783]]. Home's arrangement belongs to the window, so it takes
		// the same durable path as the drawers: A groups by organization with
		// the state headers off; B keeps the default arrangement, collapses
		// Recently viewed and filters for what was requested of me. Neither
		// arrangement may arrive in the other window, or in a fresh one.
		for (auto* const window : { a, b }) {
			if (!window->showing_home()) {
				window->home_action()->trigger();
			}
			wait_for([window] { return window->showing_home(); }, "Home is shown for its own arrangement");
		}
		a->home_group_action(application::home_grouping::organization)->trigger();
		a->home_state_headers_action()->trigger();
		b->home_list()->recent_panel()->set_collapsed(true);
		b->home_list()->state_panel(qt::home_group::my_pull_requests)->set_collapsed(true);
		auto* const requested = b->home_view_menu()->findChild<QAction*>(QStringLiteral("homeFilterRequestedAction"));
		expect(requested != nullptr, "the View menu names its Requested item");
		// A Filter gesture is refused until the queue is ready to be filtered,
		// so it is offered again rather than assumed to have been taken.
		wait_for([&] {
			if (!b->home_view().FilterOwned) {
				requested->trigger();
			}
			return b->home_view().FilterOwned; },
				 "B's Filter gesture is taken",
				 [&] { std::cerr << "b owned=" << b->home_view().FilterOwned
								 << " home=" << b->showing_home() << '\n'; });
		expect(a->home_view().Grouping == application::home_grouping::organization && !a->home_view().StateHeaders
				   && a->home_view().Collapsed == 0U && !a->home_view().FilterOwned,
			   "A holds only its own Home arrangement");
		expect(b->home_view().Grouping == application::home_grouping::none && b->home_view().StateHeaders
				   && b->home_view().Collapsed == (application::home_collapsed_panel::recently_viewed | application::home_collapsed_panel::my_pull_requests)
				   && b->home_view().Filter.Role == core::github_role::requested_reviewer,
			   "B holds only its own");
		const auto available = QGuiApplication::primaryScreen()->availableGeometry();
		a->setGeometry(available.left(), available.top() + 50, a->minimumWidth(), std::max(a->minimumHeight(), std::min(500, available.height() - 160)));
		b->setGeometry(available.left(), available.top() + 70, b->minimumWidth(), std::max(b->minimumHeight(), std::min(620, available.height() - 100)));
		QApplication::processEvents();
		geometry = { a->normalGeometry(), b->normalGeometry() };
		expect(geometry[0] != geometry[1], "two native top-level placements are distinct");
		std::ofstream placements{ root / "geometry-expectations.txt" };
		for (const auto& rectangle : geometry) {
			placements << rectangle.x() << ' ' << rectangle.y() << ' ' << rectangle.width() << ' ' << rectangle.height() << '\n';
		}
		placements.close();
		expect(placements.good(), "observed native geometry saved before immediate Quit");
		quitWindow = a;
	} else {
		const auto expected = phase == "restore" ? 2U : 1U;
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == expected; }, "retained window count");
		auto* b = select_window(*desktop, saved[1]);
		expect(b != nullptr, "surviving stable window");
		wait_for([&] { return b->normalGeometry() == geometry[1]; }, "B restores its own native position and size before any review opens");
		if (phase == "restore") {
			auto* a = select_window(*desktop, saved[0]);
			expect(a != nullptr, "A exists for native placement restoration");
			wait_for([&] { return a->normalGeometry() == geometry[0]; }, "A restores its distinct native position and size");
		}
		open_review(*owner, *b, 18, true);
		wait_for([&] { return b->overview_page()->summary_fraction() == saved[5]; }, "B restores only its own summary fraction");
		wait_for([&] { return b->overview_page()->status_drawer()->drawer().State == core::workbench_drawer_state::collapsed; },
				 "B restores its collapsed Status drawer");
		expect(!b->status_text().contains(layoutUnavailable), "a readable layout store reports no unavailable layout");
		wait_for([&] { return !b->repositories_drawer_toggle_action()->isChecked(); }, "B restores its own drawer state");
		expect(b->conversations_drawer_toggle_action()->isChecked() && b->line_numbers_action()->isChecked(), "B preserves independent source and conversation state");
		// [[BPR-1334]]. The profile's Line details choice survives the restart.
		wait_for([&] { return !b->line_details_action()->isChecked() && !b->comparison_surface()->line_details_visible(); },
				 "Line details stays hidden after a restart");
		expect(static_cast<std::uint64_t>(b->files_drawer()->width()) == saved[3], "B restores chosen files width");
		// [[BPR-0783]]. The window's Home view came back from its own row.
		wait_for([&] { return b->home_view().Collapsed == (application::home_collapsed_panel::recently_viewed | application::home_collapsed_panel::my_pull_requests); },
				 "B restores the Recently viewed panel it collapsed",
				 [&] { std::cerr << "b collapsed=" << static_cast<unsigned>(b->home_view().Collapsed)
								 << " owned=" << b->home_view().FilterOwned << '\n'; });
		expect(b->home_view().FilterOwned && b->home_view().Filter.Role == core::github_role::requested_reviewer
				   && b->home_view().Grouping == application::home_grouping::none && b->home_view().StateHeaders,
			   "B restores its own Filter and its own arrangement");
		if (phase == "restore") {
			auto* a = select_window(*desktop, saved[0]);
			expect(a != nullptr, "A stable window restored");
			open_review(*owner, *a, 17, false);
			wait_for([&] { return a->overview_page()->summary_fraction() == saved[4]; }, "A restores its distinct summary fraction");
			expect(a->overview_page()->status_drawer()->drawer().State == core::workbench_drawer_state::expanded, "A's Status drawer stays open");
			wait_for([&] { return a->comparison_surface()->line_details_height() == std::optional{ static_cast<std::uint32_t>(saved[6]) }; },
					 "A restores its own Line details height");
			expect(!b->comparison_surface()->line_details_height().has_value(), "B never had A's Line details height");
			wait_for([&] { return !a->line_numbers_action()->isChecked() && !a->conversations_drawer_toggle_action()->isChecked() && a->one_pane_action()->isChecked(); }, "immediate-Quit final layout restored");
			wait_for([&] { return a->home_view().Grouping == application::home_grouping::organization; },
					 "A restores the grouping it chose",
					 [&] { std::cerr << "a grouping=" << static_cast<unsigned>(a->home_view().Grouping) << '\n'; });
			expect(!a->home_view().StateHeaders && a->home_view().Collapsed == 0U && !a->home_view().FilterOwned,
				   "A copied neither B's collapse nor B's filter");
			expect(a->repositories_drawer_toggle_action()->isChecked(), "A did not copy sibling repositories state");
			expect(static_cast<std::uint64_t>(a->files_drawer()->width()) == saved[2], "A restores chosen files width");
			const auto artifact = qEnvironmentVariable("BPR_PROFILE_LAYOUT_ARTIFACT_ROOT");
			if (!artifact.isEmpty()) {
				expect(QDir{ }.mkpath(artifact), "layout raster directory");
				expect(a->grab().save(artifact + QStringLiteral("/restored-window-a.png")) && b->grab().save(artifact + QStringLiteral("/restored-window-b.png")), "restored layout rasters saved");
			}
			a->close();
			wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 1; }, "explicit close retires only A");
		} else {
			trigger(*b, "newBprWindowAction");
			wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 2; }, "fresh window after closed-row cleanup");
			for (const auto context : desktop->windows(core::default_bpr_profile_id)) {
				if (context.Window.value() != saved[1]) {
					auto* fresh = desktop->window(context);
					expect(fresh != nullptr, "fresh exact window is available");
					if (!fresh->showing_home()) {
						std::cerr << "fresh id=" << context.Window.value() << " generation=" << context.Generation.value()
								  << " status=" << fresh->session_panel()->status_text()
								  << " notice=" << fresh->home_open_notice()->text().toStdString() << '\n';
					}
					wait_for([&] { return fresh->showing_home(); }, "fresh window starts Home");
					open_review(*owner, *fresh, 17, false);
					expect(fresh->repositories_drawer_toggle_action()->isChecked() && fresh->conversations_drawer_toggle_action()->isChecked() && fresh->line_numbers_action()->isChecked() && !fresh->one_pane_action()->isChecked(), "fresh window seeds profile defaults, not either sibling");
					expect(!fresh->overview_page()->summary_fraction(), "new windows do not inherit a sibling summary preference");
					// [[BPR-1334]]. Shown or hidden is the profile's; the height is the window's.
					expect(!fresh->line_details_action()->isChecked() && !fresh->comparison_surface()->line_details_height().has_value(),
						   "a fresh window takes the profile's Line details choice and no sibling's height");
					expect(fresh->files_drawer()->width() == 320, "fresh files width comes from profile defaults");
					// [[BPR-0783]]. No window's Home arrangement is a profile default,
					// so a fresh window starts at Home's own.
					expect(fresh->home_view() == application::home_view_state{ },
						   "a fresh window starts with Home's default arrangement, not a sibling's");
				}
			}
		}
		b->setGeometry(geometry[1]);
		quitWindow = b;
	}
	expect(quitWindow != nullptr, "Quit exact window available");
	trigger(*quitWindow, "quitProfilesAction");
	wait_for([&] { return desktop->is_quiescent(); }, "orderly Quit drains latest layout writes");
	expect(desktop->exit_code() == 0, "drained layout writes do not report a cleanup failure");
	desktop.reset();
	std::cout << "profile-window-layout:" << phase << ":pass\n";
	return EXIT_SUCCESS;
}
