#ifndef BPR_SRC_ADAPTERS_QT_SRC_SIDE_CHIP_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_SIDE_CHIP_HPP

#include <QAbstractButton>
#include <QPainter>

#include <functional>

namespace bpr::qt {
	/// \brief A source-side chip, filled when selected and outlined otherwise.
	/// \details Toggle may override native checking for a constrained filter.
	class side_chip final : public QAbstractButton
	{
	public:
		explicit side_chip(QWidget* aParent)
			: QAbstractButton{ aParent }
		{
			setCheckable(true);
			setFocusPolicy(Qt::StrongFocus);
		}
		std::function<void()> Toggle;
		QColor                Fill;
		QColor                Ink;
		auto                  sizeHint() const -> QSize override
		{
			const auto metrics = fontMetrics();
			return { metrics.horizontalAdvance(text()) + metrics.height() + 6, metrics.height() + 8 };
		}

	protected:
		/// A filter can keep one side on; a picker uses native exclusive checking.
		void nextCheckState() override
		{
			if (Toggle) {
				Toggle();
			} else {
				QAbstractButton::nextCheckState();
			}
		}
		void paintEvent(QPaintEvent*) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing);
			const auto box    = QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5);
			const auto radius = box.height() / 2.0;
			const auto active = isEnabled() ? QPalette::Active : QPalette::Disabled;
			if (isChecked()) {
				painter.setPen(Qt::NoPen);
				painter.setBrush(Fill);
				painter.drawRoundedRect(box, radius, radius);
				painter.setPen(Ink);
			} else {
				painter.setPen(QPen{ palette().color(active, QPalette::Mid), 1.0 });
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(box, radius, radius);
				painter.setPen(palette().color(active, QPalette::PlaceholderText));
			}
			if (hasFocus()) {
				painter.setBrush(Qt::NoBrush);
				painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 2.0 });
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), radius + 0.5, radius + 0.5);
				painter.setPen(isChecked() ? Ink : palette().color(active, QPalette::PlaceholderText));
			}
			auto label = font();
			label.setWeight(isChecked() ? QFont::DemiBold : QFont::Normal);
			painter.setFont(label);
			painter.drawText(rect(), Qt::AlignCenter, text());
		}
	};

}

#endif
