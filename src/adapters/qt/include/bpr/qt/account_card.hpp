#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ACCOUNT_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ACCOUNT_CARD_HPP

#include "bpr/application/github_projection.hpp"

#include <QString>
#include <QToolButton>
#include <QWidget>

class QLabel;
class QPaintEvent;

namespace bpr::qt {
	/// \brief Accounts & hosts' account card: who is signed in, and on what terms.
	///
	/// \details [[BPR-0652]]. One card per server the accepted session names:
	/// the login and display name, the host, the connection state as the
	/// session panel decided it, and the rate limit the session projection
	/// carries. No credential is ever shown. The device-grant and remembered
	/// facts, the enterprise server card and sign-out wait on BPR-0660; this
	/// card says only what the session projection says.
	class account_card final : public QWidget
	{
	public:
		explicit account_card(QWidget* aParent = nullptr);

		void apply(const application::github_session_projection& aSession, const QString& aHost);
		void set_status(const QString& aStatus, bool aConnected);
		void clear();

		[[nodiscard]] auto login_label() const noexcept -> QLabel*;
		[[nodiscard]] auto detail_label() const noexcept -> QLabel*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		[[nodiscard]] auto rate_limit_label() const noexcept -> QLabel*;
		/// \brief [[BPR-0660]]. The grant line: kind, date, renewal and whether it is remembered.
		[[nodiscard]] auto grant_label() const noexcept -> QLabel*;
		/// \brief [[BPR-0660]]. Sign out, shown while an account is signed in; the
		/// window binds it to the deactivation intent.
		[[nodiscard]] auto sign_out_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto signed_in() const noexcept -> bool;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QLabel*      Avatar    = nullptr;
		QLabel*      Login     = nullptr;
		QLabel*      Detail    = nullptr;
		QLabel*      Status    = nullptr;
		QLabel*      RateLimit = nullptr;
		QLabel*      Grant     = nullptr;
		QToolButton* SignOut   = nullptr;
		bool         SignedIn  = false;
		bool         Connected = false;
	};
} // namespace bpr::qt

#endif
