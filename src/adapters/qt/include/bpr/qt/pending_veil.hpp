#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PENDING_VEIL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PENDING_VEIL_HPP

#include "bpr/core/pending_presentation.hpp"

#include <QColor>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>

class QEvent;
class QPaintEvent;
class QPainter;
class QPushButton;
class QTimer;

namespace bpr::qt {
	/// \brief [[BPR-1576]]. What a pane's placeholders look like before its
	/// content arrives: the shape the content will take, so nothing moves when
	/// it does.
	enum class pending_shape : std::uint8_t {
		/// A list: one bar per row at the row's height.
		rows,
		/// Prose: paragraph lines of varied length.
		text,
		/// Code: short lines at varied indents beside a gutter.
		code,
		/// One line: a heading or a value.
		line,
		/// Name and value pairs: a side column of facts.
		pairs,
		/// No placeholders: the words alone.
		block,
	};

	/// \brief [[BPR-1576]]. What a waiting pane says, in its own words.
	struct pending_words
	{
		/// Said once the wait shows: "Loading files…".
		QString Loading;
		/// Said once the wait is slow: what it still waits on.
		QString Waiting;
		/// Said while part has arrived: what is still coming.
		QString Coming;
		/// Said on failure: what failed and why.
		QString Failure;
		/// Announced when a wait that showed ends: "Files loaded."
		QString Done;

		auto operator==(const pending_words&) const -> bool = default;
	};

	/// \brief [[BPR-1576]]. The application's one loading-state presenter,
	/// laid over a pane that waits on data.
	///
	/// \details The owner, 2026-10-17: "We need a clearer visual signal in
	/// panes when data is not yet ready." Every waiting pane takes one of the
	/// core's looks (`core::decide_pending_look`) from its real facts; this
	/// draws them the same way everywhere, over the pane and never inside its
	/// layout, so nothing under it moves:
	///
	/// - placeholders in the content's own shape, in a faint tint of the
	///   pane's ink, with the words line reserved above them;
	/// - after the reveal, the turning mark and the words on that line, and
	///   after the slow mark, a second line saying what the read still waits on;
	/// - over retained content being replaced, a light veil of the pane's own
	///   ground and a two-pixel rule along its top edge that sweeps (a still
	///   dashed rule under reduced motion); nothing is blocked;
	/// - over content that is partly there, one plate at the pane's foot that
	///   says what is still coming;
	/// - a failure as one sentence in the failure ink and Retry: in place of
	///   the content, or as a strip above retained content.
	///
	/// The veil measures how long a wait has lasted on a monotonic clock and
	/// wakes exactly when the core says the look changes by time; it never
	/// shows progress it was not told about. Assistive technology sees it as
	/// busy from the first moment and hears each change the core says to
	/// announce, once, politely. Every colour is the pane's palette or the
	/// theme's failure ink, so it follows all four themes; high contrast drops
	/// the veil's wash and strengthens the placeholders.
	class pending_veil final : public QWidget
	{
	public:
		/// \param aTarget The pane the veil lies over; the veil's parent.
		/// \param aShape The placeholders' shape.
		/// \param aName What the pane is, for assistive technology: "Files".
		/// \param aArea A child of `aTarget` the veil covers instead of the
		/// whole pane, so a header above it stays readable; null covers it all.
		pending_veil(QWidget& aTarget, pending_shape aShape, QString aName, QWidget* aArea = nullptr);
		~pending_veil() override;

		pending_veil(const pending_veil&)                    = delete;
		auto operator=(const pending_veil&) -> pending_veil& = delete;

		/// \brief Takes the pane's current facts and words.
		/// \details The wait's clock starts when the activity becomes loading,
		/// and again on `restart`. The look is decided at once and again
		/// whenever time alone would change it.
		void present(core::pending_activity aActivity, bool aHasContent, bool aPartial, const pending_words& aWords);
		/// \brief Ready: the pane's own content, nothing over it.
		void present_ready();
		/// \brief Starts the wait's clock again for a new read already loading.
		void restart();
		/// \brief What Retry does; without one, a failure offers none.
		void set_retry(std::function<void()> aRetry);
		void set_full_motion(bool aFull);
		void set_high_contrast(bool aHigh);
		void set_failure_ink(const QColor& aInk);
		/// \brief Whether this pane's own changes are announced. A pane whose
		/// wait another surface already says (a refresh's control) stays quiet.
		void set_announces(bool aAnnounces);
		/// \brief The row height the `rows` placeholders take; the font's line
		/// and a half when unset.
		void set_row_height(int aHeight);
		/// \brief How many rows or pairs the placeholders stand for, when the
		/// count is known ("2 files reported"); none fills the pane.
		void set_placeholder_count(int aCount);
		/// \brief A monotonic clock in milliseconds, for courses.
		void set_clock(std::function<std::int64_t()> aClock);
		/// \brief Decides the look again now, as the wake does.
		void advance();

		[[nodiscard]] auto look() const noexcept -> core::pending_look;
		[[nodiscard]] auto words() const noexcept -> const pending_words&;
		/// \brief What the words line says now, or empty.
		[[nodiscard]] auto shown_text() const -> QString;
		[[nodiscard]] auto busy() const noexcept -> bool;
		[[nodiscard]] auto retry_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto full_motion() const noexcept -> bool;
		[[nodiscard]] auto frame() const noexcept -> int;
		/// \brief The rule's sweep, a fraction of the width; animation only.
		[[nodiscard]] auto sweep() const noexcept -> qreal;
		/// \brief How many placeholders the last paint drew, for evidence.
		[[nodiscard]] auto placeholders() const noexcept -> int;
		/// \brief The failure strip or the partial plate, in the veil.
		[[nodiscard]] auto plate_rect() const -> QRect;
		[[nodiscard]] auto pane_name() const -> QString;

		static constexpr int   inset             = 12;
		static constexpr int   rule_height       = 2;
		static constexpr int   frame_interval    = 40;
		static constexpr qreal wash_strength     = 0.42;
		static constexpr qreal skeleton_strength = 0.09;
		/// High contrast: placeholders that read, and no wash at all.
		static constexpr qreal skeleton_contrast = 0.28;
		static constexpr qreal plate_strength    = 0.07;
		static constexpr qreal failure_strength  = 0.12;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void               decide();
		void               follow_area();
		void               layout_controls();
		void               announce(core::pending_look aBefore, core::pending_look aAfter);
		void               paint_placeholders(QPainter& aPainter, const QRect& aArea);
		void               paint_words(QPainter& aPainter);
		void               paint_rule(QPainter& aPainter);
		void               paint_plate(QPainter& aPainter);
		[[nodiscard]] auto now() const -> std::int64_t;
		[[nodiscard]] auto ground() const -> QColor;
		[[nodiscard]] auto ink() const -> QColor;
		[[nodiscard]] auto failure() const -> QColor;
		[[nodiscard]] auto line_height() const -> int;
		[[nodiscard]] auto words_rect() const -> QRect;
		[[nodiscard]] auto animated() const -> bool;

		QPointer<QWidget>             Target;
		QPointer<QWidget>             Area;
		pending_shape                 Shape;
		QString                       Name;
		core::pending_activity        Activity   = core::pending_activity::idle;
		bool                          HasContent = false;
		bool                          Partial    = false;
		pending_words                 Words;
		core::pending_look            Look = core::pending_look::ready;
		std::int64_t                  StartedAt{ 0 };
		std::function<std::int64_t()> Clock;
		std::function<void()>         Retry;
		QPushButton*                  RetryButton = nullptr;
		QTimer*                       Wake        = nullptr;
		QTimer*                       Frames      = nullptr;
		QColor                        FailureInk;
		bool                          FullMotion   = false;
		bool                          HighContrast = false;
		bool                          Announces    = true;
		int                           RowHeight    = 0;
		int                           Count        = 0;
		int                           Frame        = 0;
		int                           Tick         = 0;
		int                           Painted      = 0;
	};

	/// \brief [[BPR-1576]]. The pattern's turning mark on its own, for a line
	/// that already says its wait in words (the Updates line, a status line).
	///
	/// \details As tall as a line of its font and square. It keeps its place
	/// when hidden, so a line that gains or loses the mark does not move its
	/// words. It turns only under full motion; under reduced motion it is
	/// drawn still.
	class pending_mark final : public QWidget
	{
	public:
		explicit pending_mark(QWidget* aParent = nullptr);

		void               set_turning(bool aTurning);
		void               set_full_motion(bool aFull);
		[[nodiscard]] auto turning() const noexcept -> bool;
		[[nodiscard]] auto frame() const noexcept -> int;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void follow();

		QTimer* Frames     = nullptr;
		bool    Turning    = false;
		bool    FullMotion = false;
		int     Frame      = 0;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PENDING_VEIL_HPP
