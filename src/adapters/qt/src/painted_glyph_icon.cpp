#include "painted_glyph_icon.hpp"

#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		class painted_glyph_engine final : public QIconEngine
		{
		public:
			explicit painted_glyph_engine(mode_glyph_painter aPaint)
				: Paint{ std::move(aPaint) }
			{
			}

			void paint(QPainter* const aPainter, const QRect& aRect, const QIcon::Mode aMode, const QIcon::State /*aState*/) override
			{
				const auto side = std::min(aRect.width(), aRect.height());
				if (aPainter == nullptr || side <= 0) {
					return;
				}
				aPainter->save();
				aPainter->setRenderHint(QPainter::Antialiasing, true);
				const auto left = aRect.x() + ((aRect.width() - side) / 2);
				const auto top  = aRect.y() + ((aRect.height() - side) / 2);
				aPainter->translate(QPoint{ left, top });
				Paint(*aPainter, static_cast<qreal>(side), aMode);
				aPainter->restore();
			}

			[[nodiscard]] auto pixmap(const QSize& aSize, const QIcon::Mode aMode, const QIcon::State aState) -> QPixmap override
			{
				return scaledPixmap(aSize, aMode, aState, 1.0);
			}

			/// `aSize` is logical; QIcon sets the returned pixmap's ratio to fit
			/// it, so a device-sized pixmap shows at `aSize` on every screen.
			[[nodiscard]] auto scaledPixmap(const QSize& aSize, const QIcon::Mode aMode, const QIcon::State /*aState*/, const qreal aScale) -> QPixmap override
			{
				const auto side  = std::min(aSize.width(), aSize.height());
				const auto scale = aScale > 0.0 ? aScale : 1.0;
				if (side <= 0) {
					return { };
				}
				const auto pixels = static_cast<int>(std::lround(static_cast<qreal>(side) * scale));
				auto       result = QPixmap{ pixels, pixels };
				result.setDevicePixelRatio(scale);
				result.fill(Qt::transparent);
				auto painter = QPainter{ &result };
				painter.setRenderHint(QPainter::Antialiasing, true);
				Paint(painter, static_cast<qreal>(side), aMode);
				return result;
			}

			[[nodiscard]] auto actualSize(const QSize& aSize, const QIcon::Mode /*aMode*/, const QIcon::State /*aState*/) -> QSize override
			{
				const auto side = std::min(aSize.width(), aSize.height());
				return side > 0 ? QSize{ side, side } : QSize{ };
			}

			[[nodiscard]] auto clone() const -> QIconEngine* override
			{
				return new painted_glyph_engine{ Paint };
			}

			[[nodiscard]] auto key() const -> QString override
			{
				return QStringLiteral("bpr-painted-glyph");
			}

			[[nodiscard]] auto isNull() -> bool override
			{
				return !Paint;
			}

		private:
			mode_glyph_painter Paint;
		};
	} // namespace

	auto painted_glyph_icon(glyph_painter aPaint, const QColor& aInk, const QColor& aDisabled) -> QIcon
	{
		// One ink for every mode but the disabled one.
		if (!aPaint) {
			return painted_mode_icon({ });
		}
		return painted_mode_icon([paint = std::move(aPaint), aInk, aDisabled](QPainter& aPainter, const qreal aSide, const QIcon::Mode aMode) {
			paint(aPainter, aSide, aMode == QIcon::Disabled ? aDisabled : aInk);
		});
	}

	auto painted_mode_icon(mode_glyph_painter aPaint) -> QIcon
	{
		return QIcon{ new painted_glyph_engine{ std::move(aPaint) } };
	}
} // namespace bpr::qt
