#ifndef BPR_SRC_ADAPTERS_QT_SRC_WAITING_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_WAITING_PANEL_HPP

#include <QColor>
#include <QPixmap>
#include <QWidget>

class QPalette;

namespace bpr::qt {
	/// \brief How far a waiting panel's ground leans toward the attention colour.
	inline constexpr auto waiting_tint = 0.05;

	/// \brief [[BPR-1541]] v002. The panel a publication awaiting GitHub sits in:
	/// the text ground warmed a little toward the theme's attention colour, the
	/// page's hairline edge, and a rail on its leading side, the one accent. A
	/// caution, not an alarm: nothing in it is red.
	/// \details [[BPR-1782]]. Shared by Compose review and the tour's Publish, so
	/// both say "working" the same way.
	class waiting_frame final : public QWidget
	{
	public:
		explicit waiting_frame(QWidget* aParent);
		void set_attention(const QColor& aColour);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QColor Attention{ 0xB0, 0x7A, 0x12 };
	};

	/// \brief [[BPR-1541]] v002. The "unknown" mark, a ring with a question mark,
	/// in the attention colour.
	[[nodiscard]] auto waiting_mark(const QColor& aInk, qreal aRatio) -> QPixmap;

	/// \brief The built-in attention colour for the palette's light or dark ground.
	[[nodiscard]] auto waiting_attention_colour(const QPalette& aPalette) -> QColor;

	/// \brief What is safe, said more quietly than why: text mixed toward the
	/// panel's ground, never below 4.5:1 against it.
	[[nodiscard]] auto waiting_quiet_text(const QPalette& aPalette, const QColor& aAttention) -> QColor;
} // namespace bpr::qt

#endif
