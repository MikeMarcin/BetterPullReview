#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REFRESH_NOTE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REFRESH_NOTE_HPP

#include <QColor>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPushButton;
class QTimer;

namespace bpr::qt {
	/// \brief [[BPR-1575]]. What a refresh note says.
	enum class refresh_note_tone : std::uint8_t {
		/// A read is under way: the turning mark.
		busy,
		/// A read answered: a tick.
		settled,
		/// A read failed: the ringed exclamation in the failure ink, and Retry.
		failed,
	};

	/// \brief [[BPR-1575]]. One sentence about a refresh, under its button.
	///
	/// \details The owner, 2026-10-17: "I wish that refresh on both Home and PR
	/// was more visible as an action. It's kind of hard to tell it's doing
	/// anything." The glyph turning says a read is under way; this says it in
	/// words where the eye already is, under the button that was pressed:
	/// "Refreshing the pull request…", then what the read brought ("Updated:
	/// 2 new comments and 1 new push.") or why it failed, with Retry.
	///
	/// It is a child of the window laid over the content, right-aligned under
	/// its anchor, so it moves nothing. It never takes focus by itself; only
	/// its Retry is in the tab order while it shows. A press on the note puts
	/// it away; a result puts itself away after a few seconds, a failure after
	/// longer, and the failure stays said on the button and in the status bar.
	/// Every colour is the palette's or the theme's failure ink.
	class refresh_note final : public QWidget
	{
	public:
		explicit refresh_note(QWidget& aWindow);

		/// \brief Says `aText` under `aAnchor` in `aTone`. A result or a
		/// failure puts itself away after `aLingerMs`; zero keeps it.
		void present(refresh_note_tone aTone, const QString& aText, QWidget* aAnchor, int aLingerMs = 0);
		void dismiss();
		/// \brief What Retry does; a failure without one offers none.
		void set_retry(std::function<void()> aRetry);
		void set_full_motion(bool aFull);
		void set_failure_ink(const QColor& aInk);

		[[nodiscard]] auto tone() const noexcept -> refresh_note_tone;
		[[nodiscard]] auto text() const -> QString;
		[[nodiscard]] auto anchor() const noexcept -> QWidget*;
		[[nodiscard]] auto retry_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto frame() const noexcept -> int;

		static constexpr int   padding      = 10;
		static constexpr int   gap          = 8;
		static constexpr int   maximum      = 440;
		static constexpr qreal radius       = 6.0;
		static constexpr qreal plate        = 0.07;
		static constexpr qreal failure_fill = 0.12;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void               place();
		[[nodiscard]] auto ground() const -> QColor;
		[[nodiscard]] auto failure() const -> QColor;

		QPointer<QWidget>     Window;
		QPointer<QWidget>     Anchor;
		refresh_note_tone     Tone = refresh_note_tone::busy;
		QString               Text;
		std::function<void()> Retry;
		QPushButton*          RetryButton = nullptr;
		QTimer*               Linger      = nullptr;
		QTimer*               Frames      = nullptr;
		QColor                FailureInk;
		bool                  FullMotion = false;
		int                   Frame      = 0;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REFRESH_NOTE_HPP
