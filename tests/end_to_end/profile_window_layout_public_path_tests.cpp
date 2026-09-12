#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QThread>

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
	void open_review(app::github_application& aOwner, qt::workbench_window& aWindow, int aNumber)
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
		// Only the first window signs in: a sibling's landing can still show
		// sign-in while the account is already connected, and a second grant
		// would supersede the first window's session.
		if (aOwner.status().PublicPreviewSignedOut && aWindow.home_landing()->state() == qt::landing_state::sign_in) {
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
		aWindow.modality()->action(qt::review_modality::read)->trigger();
		wait_for([&] { return aWindow.central_splitter()->isVisible() && aWindow.line_numbers_action()->isEnabled(); }, "review drawer actions available");
		QApplication::processEvents();
	}
	auto drag_files(qt::workbench_window& aWindow, int aWidth) -> int
	{
		auto*      splitter = aWindow.central_splitter();
		const auto before   = aWindow.files_drawer()->width();
		splitter->move_handle(2, aWindow.files_drawer()->x() + aWidth);
		wait_for([&] { return aWindow.files_drawer()->width() != before; }, "real splitter drag changes file width");
		return aWindow.files_drawer()->width();
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
	std::array<std::uint64_t, 4> saved{ }; // A/B identities and chosen files widths.
	if (phase != "seed") {
		std::ifstream input{ root / "layout-expectations.txt" };
		for (auto& value : saved) {
			input >> value;
			expect(input.good() && value > 0, "seed expectations readable");
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
		wait_for([&] { return b->status_text().contains(QStringLiteral("saved workbench layout is unavailable")); }, "optional layout failure is visible");
		quitWindow = b;
	} else if (phase == "seed") {
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 1; }, "first window admitted");
		saved[0] = desktop->windows(core::default_bpr_profile_id).front().Window.value();
		auto* a  = select_window(*desktop, saved[0]);
		expect(a != nullptr, "first window visible");
		open_review(*owner, *a, 17);
		trigger(*a, "newBprWindowAction");
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == 2; }, "second window admitted");
		for (const auto context : desktop->windows(core::default_bpr_profile_id)) {
			if (context.Window.value() != saved[0]) {
				saved[1] = context.Window.value();
			}
		}
		auto* b = select_window(*desktop, saved[1]);
		expect(b != nullptr, "second window visible");
		open_review(*owner, *b, 18);
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
		a->line_numbers_action()->trigger(); // Quit immediately, without waiting for persistence.
		// The toggle is the host's decision projected back; the quit waits for
		// the projection, not for the store.
		wait_for([&] { return !a->line_numbers_action()->isChecked() && b->line_numbers_action()->isChecked(); },
				 "last accepted layout change belongs only to A", [&] { std::cerr << "line numbers a=" << a->line_numbers_action()->isChecked()
																				  << " b=" << b->line_numbers_action()->isChecked()
																				  << " a enabled=" << a->line_numbers_action()->isEnabled()
																				  << " a home=" << a->showing_home()
																				  << " a status=" << a->status_text().toStdString()
																				  << " a session=" << a->session_panel()->status_text() << '\n'; });
		quitWindow = a;
	} else {
		const auto expected = phase == "restore" ? 2U : 1U;
		wait_for([&] { return desktop->windows(core::default_bpr_profile_id).size() == expected; }, "retained window count");
		auto* b = select_window(*desktop, saved[1]);
		expect(b != nullptr, "surviving stable window");
		open_review(*owner, *b, 18);
		wait_for([&] { return !b->repositories_drawer_toggle_action()->isChecked(); }, "B restores its own drawer state");
		expect(b->conversations_drawer_toggle_action()->isChecked() && b->line_numbers_action()->isChecked(), "B preserves independent source and conversation state");
		expect(static_cast<std::uint64_t>(b->files_drawer()->width()) == saved[3], "B restores chosen files width");
		if (phase == "restore") {
			auto* a = select_window(*desktop, saved[0]);
			expect(a != nullptr, "A stable window restored");
			open_review(*owner, *a, 17);
			wait_for([&] { return !a->line_numbers_action()->isChecked() && !a->conversations_drawer_toggle_action()->isChecked(); }, "immediate-Quit final layout restored");
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
					open_review(*owner, *fresh, 17);
					expect(fresh->repositories_drawer_toggle_action()->isChecked() && fresh->conversations_drawer_toggle_action()->isChecked() && fresh->line_numbers_action()->isChecked(), "fresh window seeds profile defaults, not either sibling");
					expect(fresh->files_drawer()->width() == 320, "fresh files width comes from profile defaults");
				}
			}
		}
		quitWindow = b;
	}
	expect(quitWindow != nullptr, "Quit exact window available");
	trigger(*quitWindow, "quitProfilesAction");
	wait_for([&] { return desktop->is_quiescent(); }, "orderly Quit drains latest layout writes");
	desktop.reset();
	std::cout << "profile-window-layout:" << phase << ":pass\n";
	return EXIT_SUCCESS;
}
