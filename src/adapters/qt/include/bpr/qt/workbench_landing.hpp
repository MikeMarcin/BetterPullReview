#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_LANDING_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_LANDING_HPP

#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>

class QLabel;
class QHBoxLayout;
class QPaintEvent;
class QPushButton;
class QResizeEvent;

namespace bpr::qt {
	/// \brief What the centre has nothing to show for, and what to do about it.
	enum class landing_state : std::uint8_t {
		/// No account: the way in is the bar below, signed out, or an account.
		/// [[BPR-0732]]. A public-preview session lands here too; it has no
		/// queue to choose from.
		sign_in,
		/// A session but no pull request: the way in is the queue or the locator.
		choose_pull_request,
		/// A review is open behind Home: the way back is one press.
		back_to_review,
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

		void set_state(landing_state aState);
		/// \brief The open review\'s identity, named by the back-to-review state.
		void set_review_identity(const QString& aIdentity);
		/// \brief [[BPR-0732]]. Whether a queue exists to choose from (an account).
		///
		/// \details Without one, "choose another" over an open review means
		/// the bar below, not a queue that is not shown.
		void               set_queue_offered(bool aOffered);
		[[nodiscard]] auto state() const noexcept -> landing_state;

		/// \brief Sign in (signed out) or choose from the queue (signed in).
		[[nodiscard]] auto primary_button() const noexcept -> QPushButton*;
		/// \brief Open Accounts and hosts, for the settings or the locator.
		[[nodiscard]] auto secondary_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto body_label() const noexcept -> QLabel*;

		/// \brief Places one decorative Home illustration beside the guidance.
		///
		/// \details The landing owns only layout. The injected widget owns its
		/// pixels and playback, accepts no input, and disappears before the Home
		/// controls are allowed to become cramped.
		void set_illustration(QWidget* aIllustration);
		/// \brief Releases the illustration for another window-owned eligible slot.
		[[nodiscard]] auto take_illustration() -> QWidget*;
		[[nodiscard]] auto illustration() const noexcept -> QWidget*;
		[[nodiscard]] auto illustration_space_available() const noexcept -> bool;
		void               set_illustration_enabled(bool aEnabled);

		void set_primary_dispatch(std::function<void()> aDispatch);
		void set_secondary_dispatch(std::function<void()> aDispatch);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void                  apply_illustration_layout();
		landing_state         State        = landing_state::sign_in;
		bool                  QueueOffered = true;
		QString               ReviewIdentity;
		QLabel*               Title               = nullptr;
		QLabel*               Body                = nullptr;
		QPushButton*          Primary             = nullptr;
		QPushButton*          Secondary           = nullptr;
		QHBoxLayout*          Content             = nullptr;
		QWidget*              Illustration        = nullptr;
		bool                  IllustrationEnabled = true;
		std::function<void()> PrimaryDispatch;
		std::function<void()> SecondaryDispatch;
	};
} // namespace bpr::qt

#endif
