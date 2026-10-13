#include "bpr/qt/github_cli_consent.hpp"

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
				"Account: each operation uses the chosen account's existing gh sign-in. BetterPullReview retrieves its credential into memory, validates the account, and uses the same credential for that operation. Changing gh's active account does not redirect it.\n\n"
				"Token: BetterPullReview does not save the token or change gh's sign-in. It clears its owned sensitive buffers when the operation and child processes finish. Copies in system or child-process memory, swap and crash dumps remain outside exact erasure guarantees.\n\n"
				"Machine trust: the configured gh executable, CA certificates, redirects and child processes remain trusted. Each call uses private account configuration; ambient socket routing and CLI configuration overrides are excluded.\n\n"
				"File writes: gh may write only to a temporary object store BetterPullReview owns.\n\n"
				"Full comparisons: Git and gh's own shell credential helper may run as child processes, with the same operation credential. They need gh 2.96 or later and Git 2.49 or later, each checked locally for the commands it runs, allow 32 MiB per file and stop at a supervised temporary-store limit.\n\n"
				"Connection: a successful choice is reused until you sign out or replace it. Remembered metadata saves no token and is checked again before use.\n\n"
				"Publishing: connecting for reads does not authorize publishing. Submit review, inline Publish and the explicit tour preview supply their own exact-operation authority.");
		}
	}

	auto make_github_cli_read_consent(
		QWidget*       aParent,
		const QString& aTitle,
		const QString& aHost,
		const QString& aLogin) -> std::unique_ptr<QMessageBox>
	{
		auto question = std::make_unique<QMessageBox>(
			QMessageBox::Question,
			aTitle,
			aLogin.isEmpty() ? QObject::tr("Choose a GitHub CLI account on %1.").arg(aHost) : QObject::tr("Use %1’s GitHub CLI sign-in on %2?").arg(aLogin, aHost),
			QMessageBox::Yes | QMessageBox::Cancel,
			aParent);
		question->setTextFormat(Qt::PlainText);
		// [[BPR-1140]]. The machine is named by the product's one rule:
		// "this Mac" on macOS, "this computer" on Windows and Linux.
		question->setInformativeText(aLogin.isEmpty() ? QObject::tr("Choose an account before connecting. Its existing GitHub CLI sign-in must be available on this server.") : QObject::tr("BetterPullReview uses this account's existing sign-in without saving its token. "
																																															"This connection will be reused until you sign out or replace it. "
																																															"Connecting for reads does not authorize publishing."));
		question->setDetailedText(github_cli_read_consent_details());
		question->button(QMessageBox::Yes)->setText(QObject::tr("Connect"));
		question->button(QMessageBox::Yes)->setEnabled(!aLogin.isEmpty());
		question->setDefaultButton(QMessageBox::Cancel);
		question->setEscapeButton(QMessageBox::Cancel);
		return question;
	}

	auto ask_github_cli_read_consent(
		QWidget*       aParent,
		const QString& aTitle,
		const QString& aHost,
		const QString& aLogin) -> bool
	{
		const auto question = make_github_cli_read_consent(aParent, aTitle, aHost, aLogin);
		return question->exec() == QMessageBox::Yes;
	}
	auto github_cli_publication_failure_text(application::review_mutation_failure aFailure,
											 const QString& aLogin, const QString& aHost, bool aStarted) -> QString
	{
		const auto prefix = aStarted ? QObject::tr("No further publication.") : QObject::tr("Not published.");
		switch (aFailure) {
		case application::review_mutation_failure::credential_unavailable:
			return prefix + QObject::tr(" Check %1’s GitHub CLI sign-in and credential access on %2, then try again. Your draft is kept.").arg(aLogin, aHost);
		case application::review_mutation_failure::credential_tool_failed:
			return prefix + QObject::tr(" GitHub CLI could not complete the credential check for %1 on %2. Check GitHub CLI, then try again. Your draft is kept.").arg(aLogin, aHost);
		case application::review_mutation_failure::unsupported:
			return prefix + QObject::tr(" This connection does not support this operation. Your draft is kept.");
		case application::review_mutation_failure::cancelled:
			return QObject::tr("Publication cancelled. Your draft is kept.");
		default:
			return { };
		}
	}
	auto github_cli_publication_recovery_text(const application::review_mutation_failure aFailure,
											  const QString& aLogin, const QString& aHost) -> QString
	{
		auto reason = QString{ };
		switch (aFailure) {
		case application::review_mutation_failure::credential_unavailable:
			reason = QObject::tr(" Check %1’s GitHub CLI sign-in and credential access on %2.").arg(aLogin, aHost);
			break;
		case application::review_mutation_failure::credential_tool_failed:
			reason = QObject::tr(" GitHub CLI could not check %1’s credential on %2.").arg(aLogin, aHost);
			break;
		case application::review_mutation_failure::cancelled:
			reason = QObject::tr(" The check was cancelled.");
			break;
		case application::review_mutation_failure::unsupported:
			reason = QObject::tr(" This connection cannot check the publication.");
			break;
		default:
			reason = QObject::tr(" Check the pull request and connection before retrying.");
			break;
		}
		return QObject::tr("Publication result is still unknown; retry is safe.") + reason
			+ QObject::tr(" Your draft is locked. Retry checks GitHub without publishing again.");
	}
}
