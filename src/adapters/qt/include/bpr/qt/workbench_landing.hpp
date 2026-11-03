#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_LANDING_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_LANDING_HPP

#include <QColor>
#include <QPalette>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>

class QEvent;
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
		/// A review is open behind Home: the Active review card above Home's
		/// lists, with the way back to it and its close mark ([[BPR-1573]],
		/// [[BPR-1843]]).
		back_to_review,
		/// A connection is being validated; no enrollment is offered yet.
		connecting,
	};

	/// \brief Home's first-launch card, or the Active review card over an open review.
	///
	/// \details [[BPR-0646]], [[BPR-0775]]. RFC-BPR-0032 accepted Home as the
	/// bar first. Signed out, the landing is the card "Sign in to GitHub" with
	/// Sign in filled, Use GitHub CLI and Add enterprise server bordered, the
	/// three explicit ways in without opening Settings.
	///
	/// [[BPR-1843]]. Over an open review it is the Active review card, which
	/// the window stands under the bar and above every list: a tinted card
	/// with the accent's rail at its left, "Active review" over the pull
	/// request's identity and literal title, where the reading was left, and
	/// at its right Back to the review (filled, the page's default; its
	/// description and tooltip name the review and its key) and an × that is
	/// Close review ([[BPR-1573]]): named and described for assistive
	/// technology, reachable by Tab, bound to no key.
	/// Signed in with nothing open it shows nothing and
	/// the window hides it. The surface is presentation only: each button
	/// reports a gesture through its callback and the window decides where it
	/// leads.
	class workbench_landing final : public QWidget
	{
	public:
		explicit workbench_landing(QWidget* aParent = nullptr);

		void set_state(landing_state aState);
		/// \brief The open review's identity ("owner/name#123"), the card's heading.
		void set_review_identity(const QString& aIdentity);
		/// \brief [[BPR-1843]]. The open review's literal title; empty when unknown.
		void set_review_title(const QString& aTitle);
		/// \brief [[BPR-1843]]. A height to keep, empty, where nothing is
		/// offered: the place the Active review card held while another pull
		/// request opens from Home. Zero keeps none.
		void set_held_height(int aHeight);
		/// \brief Where the reading was left, such as "file 9 of 12"; empty when unknown.
		void               set_review_position(const QString& aPosition);
		[[nodiscard]] auto state() const noexcept -> landing_state;

		/// \brief Sign in on the first-launch card, Back to the review beside the bar.
		[[nodiscard]] auto primary_button() const noexcept -> QPushButton*;
		/// \brief Use GitHub CLI on the first-launch card; hidden otherwise.
		[[nodiscard]] auto secondary_button() const noexcept -> QPushButton*;
		/// \brief Add enterprise server on the first-launch card; hidden otherwise.
		[[nodiscard]] auto tertiary_button() const noexcept -> QPushButton*;
		/// \brief The card's title; over an open review, its identity.
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		/// \brief The card's sentence; over an open review, where the reading was left.
		[[nodiscard]] auto body_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1843]]. "Active review", the card's caption; shown only
		/// over an open review.
		[[nodiscard]] auto caption_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1843]]. The open review's literal title beside its identity.
		[[nodiscard]] auto review_title_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1573]] [[BPR-1843]]. Close review: the card's × after
		/// Back to the review; shown only over an open review.
		[[nodiscard]] auto close_button() const noexcept -> QPushButton*;

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
		/// \brief [[BPR-1573]]. Close review's gesture; the window decides.
		void set_close_dispatch(std::function<void()> aDispatch);
		/// \brief Shows the CLI path only when the composition can inspect gh.
		void set_github_cli_available(bool aAvailable);

		/// \brief [[BPR-1843]]. The Active review card's colours for a palette.
		struct review_card_ink
		{
			QColor Ground;
			QColor Edge;
			QColor Rail;
			QColor Caption;
			QColor Muted;
		};
		[[nodiscard]] static auto review_card_ink_for(const QPalette& aPalette) -> review_card_ink;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void apply_illustration_layout();
		void apply_arrangement();
		void apply_card_ink();
		/// \brief [[BPR-1843]]. Fits the literal title to its one line.
		void                  fit_review_title();
		void                  set_close_idle();
		landing_state         State = landing_state::sign_in;
		QString               ReviewIdentity;
		QString               ReviewTitle;
		QString               ReviewPosition;
		QLabel*               Caption             = nullptr;
		QLabel*               Title               = nullptr;
		QLabel*               ReviewTitleLabel    = nullptr;
		QLabel*               Body                = nullptr;
		QPushButton*          Primary             = nullptr;
		QPushButton*          Secondary           = nullptr;
		QPushButton*          Tertiary            = nullptr;
		QPushButton*          Close               = nullptr;
		QHBoxLayout*          Content             = nullptr;
		QHBoxLayout*          Header              = nullptr;
		QHBoxLayout*          Buttons             = nullptr;
		QHBoxLayout*          Aside               = nullptr;
		QVBoxLayout*          Column              = nullptr;
		bool                  ApplyingInk         = false;
		int                   HeldHeight          = 0;
		QWidget*              Illustration        = nullptr;
		bool                  IllustrationEnabled = true;
		bool                  GithubCliAvailable  = false;
		std::function<void()> PrimaryDispatch;
		std::function<void()> SecondaryDispatch;
		std::function<void()> TertiaryDispatch;
		std::function<void()> CloseDispatch;
	};
} // namespace bpr::qt

#endif
