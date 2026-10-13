#ifndef BPR_TESTS_END_TO_END_CLI_PUBLICATION_COURSE_HPP
#define BPR_TESTS_END_TO_END_CLI_PUBLICATION_COURSE_HPP

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>

namespace bpr::e2e {
	/// Exercises the same chosen-account connection, while detecting any extra popup.
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
			aWindow.home_action()->trigger();
			QApplication::processEvents();
			aWindow.home_landing()->secondary_button()->click();
		}
		void check() const
		{
			if (Enabled) {
				tests::expect(TrustQuestions > 0U, "CLI course used explicit chosen-account read trust");
				tests::expect(PublicationQuestions == 0U, "ordinary review/tour gesture opened no publication popup");
			}
		}
		std::shared_ptr<std::atomic_bool> Strict = std::make_shared<std::atomic_bool>(true);

	private:
		bool        Enabled;
		QTimer      Timer;
		std::size_t TrustQuestions = 0U, PublicationQuestions = 0U;
	};
}
#endif
