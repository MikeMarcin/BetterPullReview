#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	constexpr auto draft_body = "Kept when the review is closed.";

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

	void settle()
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 150) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	/// Window renders for the attended look, when a root is named.
	void render(app::github_application& aApplication, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_CLOSE_REVIEW_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		auto& window = aApplication.window();
		for (const auto& [mode, suffix] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			static_cast<void>(access::select_appearance_mode(aApplication, mode));
			settle();
			expect(QDir{ }.mkpath(root) && window.grab().save(root + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + suffix + QStringLiteral(".png")), "composed window rendered");
		}
	}

	[[nodiscard]] auto holds(const std::vector<std::string>& aBodies) -> bool
	{
		return std::ranges::find(aBodies, std::string{ draft_body }) != aBodies.end();
	}

	/// [[BPR-1573]]. Close review beside Back to the review: Home over the
	/// review offers both, Close review releases the review in this window by
	/// the path opening another pull request takes, and the comment being
	/// written is there again when the same pull request is opened.
	void run_close_review_course()
	{
		QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-close-review-XXXXXX") };
		const auto    dataRoot = e2e::private_data_root(temporary);
		expect(dataRoot.has_value(), "private close review course root");
		auto options                   = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot    = dataRoot;
		options.IndependentWindowReads = true;
		options.RepeatReadSessions     = true;
		std::unique_ptr<app::github_application> application;
		bool                                     terminal = false;
		auto                                     startup  = access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
            terminal = true;
            if (aResult) {
                application = std::move(*aResult);
            }
		});
		wait_for([&] { return terminal && startup->is_quiescent(); }, "close review course startup terminal");
		expect(application != nullptr, "close review course composition starts");
		application->show();
		auto& window = application->window();
		window.resize(1280, 860);
		window.activateWindow();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "close review course account connects");
		const auto open = [&] {
			window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
			window.home_open_action()->trigger();
			wait_for([&] { return !window.showing_home() && window.next_file_action()->isEnabled() && access::close_review(*application).LedgerComposed; }, "the pull request opens with its drafts ledger");
		};
		open();
		wait_for([&] { return access::reveal_fixture_file(*application, "review.cpp") == app::detail::fixture_file_reveal_result::shown; }, "review.cpp is shown in Review");
		wait_for([&] { return window.current_modality() == qt::review_modality::review && !window.document_tabs()->tabs().empty(); }, "the review reads a file in a Review tab");
		auto seeded = app::detail::fixture_file_reveal_result::pending;
		wait_for([&] { seeded = access::seed_attended_comment_draft(*application, core::diff_side::right, draft_body); return seeded != app::detail::fixture_file_reveal_result::pending; }, "a new comment is written");
		expect(seeded == app::detail::fixture_file_reveal_result::shown, "the comment being written holds its body");
		wait_for([&] { return holds(access::close_review(*application).SavedDraftBodies); }, "the comment being written is saved");

		// Home over the review: Back to the review, its key, then Close review.
		window.home_action()->trigger();
		settle();
		auto* const landing = window.home_landing();
		expect(landing != nullptr && landing->state() == qt::landing_state::back_to_review, "Home over the review offers the way back");
		auto* const back  = landing->primary_button();
		auto* const close = landing->close_button();
		auto* const key   = landing->shortcut_label();
		expect(close->isVisible() && close->text() == QStringLiteral("Close review") && close->accessibleName() == QStringLiteral("Close review")
				   && close->accessibleDescription().startsWith(back->accessibleDescription().section(QStringLiteral(". Returns"), 0, 0))
				   && close->accessibleDescription().startsWith(QStringLiteral("Reviewing octo-org/review-repository")) && close->accessibleDescription().contains(QStringLiteral("#17"))
				   && close->accessibleDescription().contains(QStringLiteral("drafts are kept")) && close->toolTip() == close->accessibleDescription()
				   && !close->isDefault() && back->isDefault() && close->focusPolicy() == Qt::StrongFocus,
			   "Close review is named, described and bordered beside the filled way back");
		const auto at = [&](QWidget* aWidget) { return aWidget->mapTo(&window, QPoint{ }); };
		expect(at(back).x() < at(key).x() && at(key).x() < at(close).x()
				   && std::abs((at(close).y() + close->height() / 2) - (at(back).y() + back->height() / 2)) <= 2
				   && at(close).x() - (at(key).x() + key->width()) <= 16,
			   "Close review follows the way back and its key on the same line");
		expect(close->shortcut().isEmpty(), "Close review binds no key");
		render(*application, QStringLiteral("home-over-review"));
		back->setFocus(Qt::TabFocusReason);
		tests::window_system_key(*back, Qt::Key_Tab);
		QApplication::processEvents();
		expect(window.focusWidget() == close, "Tab from Back to the review reaches Close review");
		const auto before = access::close_review(*application);
		expect(before.ReadHoldsPullRequest && before.SnapshotHeld && before.ReadingDestination && before.TabDestination && before.Tabs > 0U,
			   "the window holds the review before it is closed");
		expect(before.RecentEntries == 1U && before.RecentOpen == 1U, "Recently viewed lists the review as open in this window");

		// Space on the focused button, as a keyboard gives it.
		tests::window_system_key(*close, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
		wait_for([&] { return landing->state() != qt::landing_state::back_to_review; }, "Close review closes the review");
		settle();
		const auto after = access::close_review(*application);
		expect(!after.ReadHoldsPullRequest && !after.SnapshotHeld, "the window's read lane holds no pull request");
		expect(!after.ReadingDestination && !after.TabDestination && after.Tabs == 0U && !after.TabChosen && window.document_tabs()->tabs().empty(),
			   "the review's reading and its tabs are released");
		expect(!after.LedgerComposed, "the review's drafts ledger is released, as opening another pull request releases it");
		wait_for([&] { const auto recent = access::close_review(*application); return recent.RecentEntries == 1U && recent.RecentOpen == 0U && recent.RecentResumable == 1U; },
				 "Recently viewed keeps the closed review, no longer open, with its reading to resume");
		expect(window.showing_home() && !landing->isVisible() && !close->isVisible() && !back->isVisible(),
			   "Home afterwards offers no way back and no Close review");
		expect(window.focusWidget() == window.home_open_bar(), "the bar takes focus once the buttons are gone");
		expect(window.statusBar()->currentMessage() == QStringLiteral("Closed the review of octo-org/review-repository#17."),
			   "the status bar says what was closed: " + window.statusBar()->currentMessage().toStdString());
		expect(!window.github_refresh_action()->isEnabled() && window.github_open_action()->isEnabled() && window.status_text().startsWith(QStringLiteral("Signed in as ")),
			   "the session stays, with nothing to refresh");
		window.home_action()->trigger();
		settle();
		expect(window.showing_home() && !landing->isVisible(), "Home still has nothing to return to");
		render(*application, QStringLiteral("home-after-close"));

		// The same pull request again: the comment being written is still there.
		open();
		wait_for([&] { return holds(access::close_review(*application).SavedDraftBodies); }, "the comment written before Close review is there when the pull request opens again");
		expect(window.document_tabs()->tabs().empty(), "the reopened review starts without the closed review's tabs");
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "close review course drains all commands");
		application.reset();
		expect(e2e::remove_course_root(temporary), "close review course root removed");
		std::cout << "close review course passed\n"
				  << std::flush;
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication qtApplication{ aArgc, aArgv };
	run_close_review_course();
	return EXIT_SUCCESS;
}
