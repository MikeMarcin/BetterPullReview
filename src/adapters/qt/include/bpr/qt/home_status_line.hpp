#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_STATUS_LINE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_STATUS_LINE_HPP

#include <QColor>
#include <QRect>
#include <QSize>
#include <QWidget>

#include <cstdint>
#include <vector>

class QAbstractButton;
class QEvent;
class QHBoxLayout;
class QLabel;
class QPaintEvent;
class QTimer;

namespace bpr::qt {
	/// \brief [[BPR-1357]]. What Home's status band is saying.
	enum class home_status_tone : std::uint8_t {
		/// Something the reviewer did, said back: no mark, a neutral band.
		notice,
		/// Something is under way: a turning mark on the neutral band.
		busy,
		/// Something went wrong: a ringed exclamation in the failure ink.
		problem,
	};

	/// \brief [[BPR-1357]]. Home's status band, directly under the Open bar.
	///
	/// \details The owner, 2026-10-09: "I think the general status text on the
	/// home page is still a bit lacking, it's kind of hard to see." Home's status
	/// was a line at the window's bottom edge in the status bar's small type, and
	/// a refusal or a load in helper text under the bar. The band puts it where
	/// the eye already is, at body size in full ink, behind a mark that says its
	/// tone. Only a problem is tinted, in the theme's failure ink: on Home colour
	/// means state. Every colour is the palette's or that ink, so it follows
	/// light, dark and high contrast.
	///
	/// It holds labels and buttons it does not own the meaning of: it shows
	/// whenever one of its labels is not hidden, and draws the tone it is told.
	/// The window decides what is said; idle, every label is hidden and so is
	/// the band, so nothing is added to a quiet Home.
	class home_status_line final : public QWidget
	{
	public:
		explicit home_status_line(QWidget* aParent = nullptr);

		/// \brief Takes a label into the band's text column. Labels are stacked;
		/// the window shows one at a time.
		void adopt_text(QLabel& aLabel);
		/// \brief Takes a button into the band's trailing row.
		void adopt_button(QAbstractButton& aButton);
		void set_tone(home_status_tone aTone);
		/// \brief The failure ink: the theme's defect colour at text strength.
		void set_failure_ink(const QColor& aInk);

		[[nodiscard]] auto tone() const noexcept -> home_status_tone;
		[[nodiscard]] auto failure_ink() const -> QColor;
		/// \brief The mark's place, empty for a notice.
		[[nodiscard]] auto mark_rect() const -> QRect;
		/// \brief The band's fill and edge for its tone, over the palette's window.
		[[nodiscard]] auto fill() const -> QColor;
		[[nodiscard]] auto edge() const -> QColor;
		/// \brief The mark's current turn, for evidence; it is animation only.
		[[nodiscard]] auto frame() const noexcept -> int;

		[[nodiscard]] auto sizeHint() const -> QSize override;

		static constexpr int   inset_horizontal = 12;
		static constexpr int   inset_vertical   = 8;
		static constexpr int   mark_gap         = 8;
		static constexpr qreal radius           = 6.0;
		static constexpr qreal fill_strength    = 0.10;
		static constexpr qreal edge_strength    = 0.45;
		static constexpr int   turn_interval    = 80;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void               follow_labels();
		void               apply_margins();
		[[nodiscard]] auto tone_colour() const -> QColor;

		QHBoxLayout*         Row   = nullptr;
		QWidget*             Texts = nullptr;
		QTimer*              Turn  = nullptr;
		std::vector<QLabel*> Labels;
		home_status_tone     Tone = home_status_tone::notice;
		QColor               Failure;
		int                  Frame = 0;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_STATUS_LINE_HPP
