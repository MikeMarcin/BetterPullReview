#ifndef BPR_SRC_ADAPTERS_QT_SRC_AGENT_CARD_FRAME_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_AGENT_CARD_FRAME_HPP

#include <QColor>
#include <QPainter>
#include <QPalette>
#include <QRectF>
#include <QWidget>

#include <cstdint>

/// \file
/// \brief The rounded frame the Agent tab draws its cards in: the state's
/// callout, the ready card, a finding's card, the question's field and the
/// reply's card. The one transcript's card look (RFC-BPR-0130 stage 5) draws
/// a turn in it as the Agent tab did.
namespace bpr::qt::agent_card {
	[[nodiscard]] inline auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
	{
		const auto keep = 1.0 - aShare;
		return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
								static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
	}

	[[nodiscard]] inline auto dark(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Window).lightness() < 128; }

	/// \brief The looks a rounded card is drawn in.
	enum class card_look : std::uint8_t {
		card,
		callout,
		danger,
		field,
		ready,
	};

	/// \brief Draws `aWidget`'s rect as a rounded card in its palette: the
	/// Agent tab's cards and every chat's notice card.
	inline void paint_card(QWidget& aWidget, const card_look aLook, const QColor& aEdge = { }, const bool aSelected = false)
	{
		auto painter = QPainter{ &aWidget };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = aWidget.palette();
		const auto  ink     = colours.color(QPalette::Active, QPalette::WindowText);
		const auto  base    = colours.color(QPalette::Active, QPalette::Base);
		const auto  window  = colours.color(QPalette::Active, QPalette::Window);
		const auto  box     = QRectF{ aWidget.rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		auto        fill    = base;
		auto        line    = mixed(base, ink, 0.16);
		if (aLook == card_look::callout) {
			fill = mixed(window, colours.color(QPalette::Active, QPalette::Highlight), 0.10);
			line = mixed(window, colours.color(QPalette::Active, QPalette::Highlight), 0.30);
		} else if (aLook == card_look::ready) {
			const auto green = dark(colours) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
			fill             = mixed(window, green, 0.10);
			line             = mixed(window, green, 0.35);
		} else if (aLook == card_look::danger) {
			const auto red = dark(colours) ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
			fill           = mixed(window, red, 0.10);
			line           = mixed(window, red, 0.35);
		}
		if (aSelected) {
			line = dark(colours) ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
		}
		painter.setPen(QPen{ line, 1.0 });
		painter.setBrush(fill);
		painter.drawRoundedRect(box, 8.0, 8.0);
		if (aEdge.isValid()) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(aEdge);
			painter.drawRoundedRect(QRectF{ box.left(), box.top() + 6.0, 3.0, box.height() - 12.0 }, 1.5, 1.5);
		}
	}

	/// \brief A frame that draws a rounded card in the palette.
	class rounded final : public QWidget
	{
	public:
		using look = card_look;
		rounded(const look aLook, QWidget* aParent)
			: QWidget{ aParent }
			, Look{ aLook }
		{
		}
		look Look;
		/// The severity's edge, on a finding's card.
		QColor Edge;
		bool   Selected = false;

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override { paint_card(*this, Look, Edge, Selected); }
	};

	/// \brief A reply card's side margins; its answer is this much narrower.
	inline constexpr auto reply_left_margin  = 12;
	inline constexpr auto reply_right_margin = 10;
}

#endif
