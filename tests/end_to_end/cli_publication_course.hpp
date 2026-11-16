#ifndef BPR_TESTS_END_TO_END_CLI_PUBLICATION_COURSE_HPP
#define BPR_TESTS_END_TO_END_CLI_PUBLICATION_COURSE_HPP

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "account_setup_course.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>

#include <memory>

namespace bpr::e2e {
	/// Exercises the same chosen-account connection, while detecting any extra
	/// popup. RFC-BPR-0032 amendment r1: the account is chosen and consented to
	/// in Home's account setup, so a gh read question is now itself a failure.
	class cli_publication_course final
	{
	public:
		explicit cli_publication_course(bool aEnabled)
			: Enabled{ aEnabled }
		{
			QObject::connect(&Timer, &QTimer::timeout, [this] {
				const tests::exit_on_failure inTheEventLoop;
				auto*                        prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
				if (!prompt) {
					return;
				}
				if (prompt->text().startsWith(QStringLiteral("Use "))
					&& (prompt->text().contains(QStringLiteral("gh")) || prompt->text().contains(QStringLiteral("GitHub CLI")))) {
					++TrustQuestions;
					prompt->button(QMessageBox::Yes)->click();
				} else {
					++PublicationQuestions;
					prompt->button(QMessageBox::Cancel)->click();
				}
			});
			if (Enabled) {
				Timer.start(1);
			}
		}
		void configure(app::detail::github_public_path_fixture_options& aOptions) const
		{
			if (!Enabled) {
				return;
			}
			aOptions.PublicPreviewEnabledCourse = true;
			aOptions.IndependentWindowReads     = true;
			aOptions.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{ { .Hostname = "github.com", .Login = "octo-reviewer" } };
			aOptions.GithubCliStrictCapability  = Strict;
		}
		void connect(qt::workbench_window& aWindow) const
		{
			if (!Enabled) {
				aWindow.github_connect_action()->trigger();
				return;
			}
			if (aWindow.session_panel()->status() == core::github_status::session_ready) {
				return;
			}
			// [[BPR-2192]]. Restarted after the account was consented to in
			// the setup, the application restores that account by itself
			// (establishing, then ready) and asks nothing: the course does not
			// open the setup again, and the consent it counts is the first.
			if (Driver != nullptr && Driver->connects() > 0U && aWindow.session_panel()->status() == core::github_status::establishing_session) {
				return;
			}
			aWindow.home_action()->trigger();
			QApplication::processEvents();
			auto plan  = account_setup_driver::plan{ };
			plan.Login = "octo-reviewer";
			Driver     = std::make_unique<account_setup_driver>(aWindow, std::move(plan));
			Driver->begin();
		}
		void check() const
		{
			if (Enabled) {
				tests::expect(Driver != nullptr && Driver->connects() > 0U && TrustQuestions == 0U,
							  "CLI course consented to the chosen account in the account setup, and no read question opened");
				tests::expect(PublicationQuestions == 0U, "ordinary review/tour gesture opened no publication popup");
			}
		}
		std::shared_ptr<std::atomic_bool> Strict = std::make_shared<std::atomic_bool>(true);

	private:
		bool                                          Enabled;
		QTimer                                        Timer;
		mutable std::unique_ptr<account_setup_driver> Driver;
		std::size_t                                   TrustQuestions = 0U, PublicationQuestions = 0U;
	};
}
#endif
