#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ACCOUNT_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ACCOUNT_CARD_HPP

#include "bpr/application/github_projection.hpp"

#include <QString>
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
		[[nodiscard]] auto signed_in() const noexcept -> bool;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QLabel* Avatar    = nullptr;
		QLabel* Login     = nullptr;
		QLabel* Detail    = nullptr;
		QLabel* Status    = nullptr;
		QLabel* RateLimit = nullptr;
		bool    SignedIn  = false;
		bool    Connected = false;
	};
} // namespace bpr::qt

#endif
