#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_KEYBOARD_FOCUS_RING_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_KEYBOARD_FOCUS_RING_HPP

#include <QColor>
#include <QPalette>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <vector>

class QPainter;
class QTextEdit;

namespace bpr::qt {
	/// \brief The colour a focus ring is drawn in over the `aSurface` colour of
	/// `aPalette`: its `Highlight`, lifted only as far as the indicator floor
	/// needs (`core::visible_focus_indicator`, [[BPR-1157]]).
	[[nodiscard]] auto focus_ring_colour(const QPalette& aPalette, QPalette::ColorRole aSurface) -> QColor;

	/// \brief The width of every focus ring the window draws, in logical pixels.
	inline constexpr qreal focus_ring_width = 2.0;

	/// \brief A focus ring over one control, shown while it has focus.
	///
	/// \details [[BPR-1157]]. A transparent child that covers its control and
	/// draws a `focus_ring_width` ring inside its edge, so the cue never
	/// changes the control's footprint and is drawn the same on every
	/// platform: macOS styles draw no focus rectangle of their own, and the
	/// owner could not tell what had focus on the cycle page
	/// (`BPR-1157-native-keyboard-v001`, 2026-10-02). Its colour is
	/// `focus_ring_colour` over the control's own background. It takes no
	/// pointer input, never takes focus, and reports itself to accessibility
	/// as invisible decoration, so the control keeps its own accessible focus.
	/// A popup opening keeps the ring; any other loss of focus hides it.
	class keyboard_focus_ring final : public QWidget
	{
	public:
		/// \brief Watches `aControl` and draws the ring over it.
		keyboard_focus_ring(QWidget& aControl, const QString& aObjectName);

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief Where the words from `aFirst` to `aLast` of `aReader`'s text
	/// stand on its viewport, one box a line: a link the keyboard is on.
	[[nodiscard]] auto text_focus_boxes(const QTextEdit& aReader, int aFirst, int aLast) -> std::vector<QRectF>;

	/// \brief Draws the window's focus ring close round each of `aBoxes` on
	/// `aReader`'s viewport: the mark of the link the keyboard is on.
	///
	/// \details A reader of text marks its keyboard's link with this ring and
	/// never by selecting the link's words (owner, 2026-11-25, of a link that
	/// came up selected: "I cant actually click go to draft properly until I
	/// deselect it and then click it again"). A selection is the reader's own
	/// to make, and a press of the pointer on selected words starts a drag of
	/// them. The ring stands a little outside the words. Where the words are
	/// at the viewport's edge, with no room beyond them, that side is drawn
	/// at the edge, so the ring shows on every side.
	void paint_text_focus_ring(QPainter& aPainter, const QTextEdit& aReader, const std::vector<QRectF>& aBoxes);

	/// \brief Puts a `keyboard_focus_ring` named `aObjectName` on every control
	/// under `aRoot` that Tab can reach, except `aExcept` and the controls
	/// under it, which draw their own.
	void ring_keyboard_focus(QWidget& aRoot, const QString& aObjectName, const QWidget* aExcept = nullptr);
}

#endif
