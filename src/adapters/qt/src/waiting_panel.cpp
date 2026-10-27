#include "waiting_panel.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/destructive_question.hpp"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aAmount),
									static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aAmount),
									static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aAmount));
		}
	} // namespace

	waiting_frame::waiting_frame(QWidget* aParent)
		: QWidget{ aParent }
	{ }

	void waiting_frame::set_attention(const QColor& aColour)
	{
		Attention = aColour;
		update();
	}

	void waiting_frame::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto        painter = QPainter{ this };
		const auto& colours = palette();
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto ground = colours.color(QPalette::Active, QPalette::Base);
		const auto box    = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		// One accent only: the rail. The edge is the page's own hairline.
		painter.setPen(QPen{ blend(ground, colours.color(QPalette::Active, QPalette::Text), 0.16), 1.0 });
		painter.setBrush(blend(ground, Attention, waiting_tint));
		painter.drawRoundedRect(box, 6.0, 6.0);
		auto edge = QPainterPath{ };
		edge.addRoundedRect(box, 6.0, 6.0);
		painter.setClipPath(edge);
		painter.fillRect(QRectF{ box.left(), box.top(), 3.0, box.height() }, Attention);
	}

	auto waiting_mark(const QColor& aInk, const qreal aRatio) -> QPixmap
	{
		constexpr auto size   = 18.0;
		const auto     ratio  = std::max<qreal>(1.0, aRatio);
		const auto     pixels = static_cast<int>(std::ceil(size * ratio));
		auto           image  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
		image.fill(Qt::transparent);
		image.setDevicePixelRatio(ratio);
		{
			auto painter = QPainter{ &image };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(QPen{ aInk, 1.6 });
			painter.setBrush(Qt::NoBrush);
			painter.drawEllipse(QRectF{ 1.3, 1.3, size - 2.6, size - 2.6 });
			auto font = painter.font();
			font.setPixelSize(11);
			font.setBold(true);
			painter.setFont(font);
			painter.drawText(QRectF{ 0.0, 0.0, size, size }, Qt::AlignCenter, QStringLiteral("?"));
		}
		return QPixmap::fromImage(image);
	}

	auto waiting_attention_colour(const QPalette& aPalette) -> QColor
	{
		const auto variant = aPalette.color(QPalette::Base).lightnessF() < 0.5 ? core::appearance_variant::dark : core::appearance_variant::light;
		return to_presentation_color(builtin_appearance_palette(variant)[core::index_of(core::appearance_role::state_attention)]);
	}

	auto waiting_quiet_text(const QPalette& aPalette, const QColor& aAttention) -> QColor
	{
		const auto ground = blend(aPalette.color(QPalette::Active, QPalette::Base), aAttention, waiting_tint);
		const auto text   = aPalette.color(QPalette::Active, QPalette::WindowText);
		auto       quiet  = blend(text, ground, 0.28);
		if (destructive_contrast(quiet, ground) < 4.5) {
			quiet = text;
		}
		return quiet;
	}
} // namespace bpr::qt
