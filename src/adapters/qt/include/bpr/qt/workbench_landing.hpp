#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_LANDING_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_LANDING_HPP

#include <QWidget>

#include <cstdint>
#include <functional>

class QLabel;
class QPaintEvent;
class QPushButton;

namespace bpr::qt {
	/// \brief What the centre has nothing to show for, and what to do about it.
	enum class landing_state : std::uint8_t {
		/// No accepted session: the way in is an account.
		sign_in,
		/// A session but no pull request: the way in is the queue or the locator.
		choose_pull_request,
	};

	/// \brief The centre's landing while there is nothing to review.
	///
	/// \details [[BPR-0646]]. Owner direction: the default landing must
	/// funnel a reviewer with no account or no pull request to the right
	/// place rather than show empty panes. The surface is presentation only:
	/// each button reports a gesture through its callback and the window
	/// decides where it leads (the Accounts and hosts page, the queue). It
	/// shows only while the centre holds no comparison and no pull request,
	/// and it never grows past its content, so the comparison beneath keeps
	/// its room the moment there is something to compare.
	class workbench_landing final : public QWidget
	{
	public:
		explicit workbench_landing(QWidget* aParent = nullptr);

		void               set_state(landing_state aState);
		[[nodiscard]] auto state() const noexcept -> landing_state;

		/// \brief Sign in (signed out) or choose from the queue (signed in).
		[[nodiscard]] auto primary_button() const noexcept -> QPushButton*;
		/// \brief Open Accounts and hosts, for the settings or the locator.
		[[nodiscard]] auto secondary_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto body_label() const noexcept -> QLabel*;

		void set_primary_dispatch(std::function<void()> aDispatch);
		void set_secondary_dispatch(std::function<void()> aDispatch);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		landing_state         State     = landing_state::sign_in;
		QLabel*               Title     = nullptr;
		QLabel*               Body      = nullptr;
		QPushButton*          Primary   = nullptr;
		QPushButton*          Secondary = nullptr;
		std::function<void()> PrimaryDispatch;
		std::function<void()> SecondaryDispatch;
	};
} // namespace bpr::qt

#endif
