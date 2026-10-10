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
class QVBoxLayout;

namespace bpr::qt {
	/// \brief What Home offers besides the bar, by session and open review.
	enum class landing_state : std::uint8_t {
		/// No account: the first-launch card with its two ways in.
		/// [[BPR-0732]]. A public-preview session lands here too; it has no
		/// queue to choose from.
		sign_in,
		/// An account and nothing open: the bar leads Home, so there is no card.
		choose_pull_request,
		/// A review is open behind Home: one button beside the bar goes back to it.
		back_to_review,
	};

	/// \brief Home's first-launch card, or its one button back to an open review.
	///
	/// \details [[BPR-0646]], [[BPR-0775]]. RFC-BPR-0032 accepted Home as the
	/// bar first. Signed out, the landing is the card "Sign in to GitHub" with
	/// Sign in filled, Use GitHub CLI and Add enterprise server bordered, the
	/// three explicit ways in without opening Settings. Over an open review it
	/// is Back to the review and its shortcut, which the window stands at the
	/// bar's right ([[BPR-1381]]); the button's description names the review.
	/// Signed in with nothing open it shows nothing and
	/// the window hides it. The surface is presentation only: each button
	/// reports a gesture through its callback and the window decides where it
	/// leads.
	class workbench_landing final : public QWidget
	{
	public:
		explicit workbench_landing(QWidget* aParent = nullptr);

		void set_state(landing_state aState);
		/// \brief The open review's identity, named by the back-to-review row.
		void set_review_identity(const QString& aIdentity);
		/// \brief Where the reading was left, such as "file 9 of 12"; empty when unknown.
		void               set_review_position(const QString& aPosition);
		[[nodiscard]] auto state() const noexcept -> landing_state;

		/// \brief Sign in on the first-launch card, Back to the review beside the bar.
		[[nodiscard]] auto primary_button() const noexcept -> QPushButton*;
		/// \brief Use GitHub CLI on the first-launch card; hidden otherwise.
		[[nodiscard]] auto secondary_button() const noexcept -> QPushButton*;
		/// \brief Add enterprise server on the first-launch card; hidden otherwise.
		[[nodiscard]] auto tertiary_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto body_label() const noexcept -> QLabel*;
		/// \brief The shortcut beside Back to the review; hidden on the card.
		[[nodiscard]] auto shortcut_label() const noexcept -> QLabel*;

		/// \brief Places one decorative Home illustration beside the card's guidance.
		///
		/// \details The landing owns only layout. The injected widget owns its
		/// pixels and playback, accepts no input, and disappears before the Home
		/// controls are allowed to become cramped. Only the first-launch card
		/// shows it; the return row never does.
		void set_illustration(QWidget* aIllustration);
		/// \brief Releases the illustration for another window-owned eligible slot.
		[[nodiscard]] auto take_illustration() -> QWidget*;
		[[nodiscard]] auto illustration() const noexcept -> QWidget*;
		[[nodiscard]] auto illustration_space_available() const noexcept -> bool;
		void               set_illustration_enabled(bool aEnabled);

		void set_primary_dispatch(std::function<void()> aDispatch);
		void set_secondary_dispatch(std::function<void()> aDispatch);
		void set_tertiary_dispatch(std::function<void()> aDispatch);
		/// \brief Shows the CLI path only when the composition can inspect gh.
		void set_github_cli_available(bool aAvailable);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void                  apply_illustration_layout();
		void                  apply_arrangement();
		landing_state         State = landing_state::sign_in;
		QString               ReviewIdentity;
		QString               ReviewPosition;
		QLabel*               Title               = nullptr;
		QLabel*               Body                = nullptr;
		QLabel*               Shortcut            = nullptr;
		QPushButton*          Primary             = nullptr;
		QPushButton*          Secondary           = nullptr;
		QPushButton*          Tertiary            = nullptr;
		QHBoxLayout*          Content             = nullptr;
		QHBoxLayout*          Header              = nullptr;
		QHBoxLayout*          Buttons             = nullptr;
		QWidget*              Illustration        = nullptr;
		bool                  IllustrationEnabled = true;
		bool                  GithubCliAvailable  = false;
		std::function<void()> PrimaryDispatch;
		std::function<void()> SecondaryDispatch;
		std::function<void()> TertiaryDispatch;
	};
} // namespace bpr::qt

#endif
