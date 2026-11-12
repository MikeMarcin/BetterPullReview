#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ACCOUNT_SETUP_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ACCOUNT_SETUP_VIEW_HPP

#include "bpr/application/account_setup_projection.hpp"
#include "bpr/core/account_setup.hpp"

#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>

class QEvent;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief RFC-BPR-0032 amendment r1. What the reviewer can ask of the
	/// account setup; each is a request to the host, never an effect here.
	struct account_setup_intents
	{
		std::function<void(core::account_setup_event)> Dispatch;
		/// Opens the code's address, through the host that validated it.
		std::function<void()> OpenVerificationAddress;
		/// GitHub Enterprise Server keeps its existing way in (Accounts and
		/// hosts); the setup offers it as a link.
		std::function<void()> AddEnterpriseServer;
	};

	/// \brief RFC-BPR-0032 amendment r1. Home's account setup: How to
	/// connect, Sign in, Confirm account, Ready, in the window and in order.
	///
	/// \details Presentation only, built with the Agent tab setup's parts
	/// (RFC-BPR-0092): numbered steps that fold to one line once done, whole
	/// cards to choose, and check marks for what is running, passed or failed.
	/// The host's projection says where the setup stands; the words for each
	/// state are written here. Nothing opens a window: the device code, gh's
	/// accounts and the consent to connect one are all in this view. The way
	/// card picked before Continue is this view's own state.
	class account_setup_view final : public QWidget
	{
	public:
		explicit account_setup_view(QWidget* aParent = nullptr);

		void set_intents(account_setup_intents aIntents);
		void present(const application::account_setup_projection& aProjection);

		[[nodiscard]] auto projection() const noexcept -> const application::account_setup_projection& { return Projection; }
		/// The way card the reviewer has picked, before Continue.
		[[nodiscard]] auto picked_way() const noexcept -> core::account_setup_way { return Picked; }
		/// Every label's and button's text, in order, for tests and the
		/// accessible summary.
		[[nodiscard]] auto step_text() const -> QString;
		/// The words a problem is said with: what happened, then what to do.
		[[nodiscard]] static auto problem_words(core::account_setup_problem aProblem) -> QString;
		/// The one line a published change is announced with, politely and
		/// once: where the setup stands, or what went wrong.
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto announcements() const noexcept -> std::uint64_t { return Announcements; }

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void               announce();
		void               rebuild();
		void               build_way(QVBoxLayout& aBody);
		void               build_browser(QVBoxLayout& aBody);
		void               build_github_cli(QVBoxLayout& aBody);
		void               build_confirm(QVBoxLayout& aBody);
		void               build_ready(QVBoxLayout& aBody);
		void               dispatch(core::account_setup_event aEvent);
		[[nodiscard]] auto chosen_login() const -> QString;

		account_setup_intents                 Intents;
		application::account_setup_projection Projection;
		core::account_setup_way               Picked      = core::account_setup_way::browser;
		bool                                  DetailsOpen = false;
		QString                               LastAnnouncement;
		std::uint64_t                         Announcements = 0U;
		QVBoxLayout*                          Steps         = nullptr;
		QLabel*                               Intro         = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ACCOUNT_SETUP_VIEW_HPP
