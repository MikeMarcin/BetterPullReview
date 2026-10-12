#include "bpr/qt/github_cli_consent.hpp"

#include "this_machine_text.hpp"

#include <QAbstractButton>
#include <QObject>
#include <QWidget>

namespace bpr::qt {
	namespace {
		// The supporting detail behind Show Details. Every residual risk the
		// earlier one-paragraph warning named is still here; only its place
		// changed. The gh and Git minimums mirror the capability admission in
		// gh_cli_git_smart_transport.cpp and change with it.
		[[nodiscard]] auto github_cli_read_consent_details() -> QString
		{
			return QObject::tr(
				"Account: gh has no per-request account selector. Switching gh accounts outside "
				"BetterPullReview, even between the identity check and a request, changes the account "
				"used. There is no exact account guarantee; a detected change stops the session or write.\n\n"
				"Machine configuration: gh may follow redirects and uses its executable, configuration "
				"(including socket routing) and CA certificates as configured on this machine. Replacing "
				"any of them, and child-process behavior, remain risks. Only gh's default home-directory "
				"configuration is used; custom CLI environment overrides are ignored.\n\n"
				"File writes: gh may write only to a temporary object store BetterPullReview owns, so an "
				"older gh configuration that needs migration is refused.\n\n"
				"Full comparisons: Git and gh's own shell credential helper may run as child processes. "
				"They need gh 2.96 or later and Git 2.49 or later, each checked locally for the commands "
				"it runs, allow 32 MiB per file and stop at a supervised temporary-store limit.\n\n"
				"Token: BetterPullReview never reads, stores or passes your gh token and does not change "
				"gh's sign-in.\n\n"
				"Publishing: each review or inline comment asks for its own confirmation naming the host, "
				"account, repository and pull request.");
		}
	}

	auto make_github_cli_read_consent(
		QWidget*       aParent,
		const QString& aTitle,
		const QString& aHost) -> std::unique_ptr<QMessageBox>
	{
		auto question = std::make_unique<QMessageBox>(
			QMessageBox::Warning,
			aTitle,
			QObject::tr("Use your installed gh to read pull requests on %1?").arg(aHost),
			QMessageBox::Yes | QMessageBox::Cancel,
			aParent);
		question->setTextFormat(Qt::PlainText);
		// [[BPR-1140]]. The machine is named by the product's one rule:
		// "this Mac" on macOS, "this computer" on Windows and Linux.
		const auto informative = QObject::tr(
			"BetterPullReview will read as the account gh is signed in to, and that account can change "
			"outside this app. You trust %1's gh executable, configuration and certificates as they "
			"are set up. Connecting for reads does not authorize publishing; each review or comment asks "
			"separately. After a successful connection, this choice will be reused on later launches until you sign out or replace it.");
		question->setInformativeText(informative.arg(detail::this_machine_text()));
		question->setDetailedText(github_cli_read_consent_details());
		question->button(QMessageBox::Yes)->setText(QObject::tr("Connect"));
		question->setDefaultButton(QMessageBox::Cancel);
		question->setEscapeButton(QMessageBox::Cancel);
		return question;
	}

	auto ask_github_cli_read_consent(
		QWidget*       aParent,
		const QString& aTitle,
		const QString& aHost) -> bool
	{
		const auto question = make_github_cli_read_consent(aParent, aTitle, aHost);
		return question->exec() == QMessageBox::Yes;
	}
}
