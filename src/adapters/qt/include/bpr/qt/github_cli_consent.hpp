#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CLI_CONSENT_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CLI_CONSENT_HPP

#include <QMessageBox>
#include <QString>

#include <memory>

class QWidget;

namespace bpr::qt {
	/// \brief Builds RFC-BPR-0015's GitHub CLI read opt-in for one exact host.
	/// \details [[BPR-0933]]. The decision the reviewer reads is short: the
	/// host, the installed `gh` and the account and configuration it trusts,
	/// that the account can change outside BetterPullReview, and that reading
	/// does not authorize publishing. The complete residual-risk list sits
	/// behind the box's own Show Details disclosure. Connect is the only
	/// accepting button; Cancel is both the default and the escape button, so
	/// Return and Escape each decline. The caller owns the returned box.
	/// \param aParent Window the question belongs to.
	/// \param aTitle Accessible window title; macOS does not draw it.
	/// \param aHost Exact configured hostname the read would use.
	[[nodiscard]] auto make_github_cli_read_consent(
		QWidget*       aParent,
		const QString& aTitle,
		const QString& aHost) -> std::unique_ptr<QMessageBox>;

	/// \brief Asks the question from make_github_cli_read_consent() modally.
	/// \returns True only when the reviewer chose Connect.
	[[nodiscard]] auto ask_github_cli_read_consent(
		QWidget*       aParent,
		const QString& aTitle,
		const QString& aHost) -> bool;
}

#endif
