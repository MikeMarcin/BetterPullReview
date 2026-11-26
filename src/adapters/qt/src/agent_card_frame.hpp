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

	/// \brief A frame that draws a rounded card in the palette.
	class rounded final : public QWidget
	{
	public:
		enum class look : std::uint8_t {
			card,
			callout,
			danger,
			field,
			ready,
		};
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
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
			const auto base   = palette().color(QPalette::Active, QPalette::Base);
			const auto window = palette().color(QPalette::Active, QPalette::Window);
			const auto box    = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
			auto       fill   = base;
			auto       line   = mixed(base, ink, 0.16);
			if (Look == look::callout) {
				fill = mixed(window, palette().color(QPalette::Active, QPalette::Highlight), 0.10);
				line = mixed(window, palette().color(QPalette::Active, QPalette::Highlight), 0.30);
			} else if (Look == look::ready) {
				const auto green = dark(palette()) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
				fill             = mixed(window, green, 0.10);
				line             = mixed(window, green, 0.35);
			} else if (Look == look::danger) {
				const auto red = dark(palette()) ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
				fill           = mixed(window, red, 0.10);
				line           = mixed(window, red, 0.35);
			}
			if (Selected) {
				line = dark(palette()) ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
			}
			painter.setPen(QPen{ line, 1.0 });
			painter.setBrush(fill);
			painter.drawRoundedRect(box, 8.0, 8.0);
			if (Edge.isValid()) {
				painter.setPen(Qt::NoPen);
				painter.setBrush(Edge);
				painter.drawRoundedRect(QRectF{ box.left(), box.top() + 6.0, 3.0, box.height() - 12.0 }, 1.5, 1.5);
			}
		}
	};

	/// \brief A reply card's side margins; its answer is this much narrower.
	inline constexpr auto reply_left_margin  = 12;
	inline constexpr auto reply_right_margin = 10;
}

#endif
