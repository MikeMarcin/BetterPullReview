#include "side_chat_parts.hpp"

#include "bpr/qt/agent_feature.hpp"

#include <QKeyEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QPainter>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt::side_chat {
	auto mixed(const QColor& aFrom, const QColor& aTo, const double aAmount) -> QColor
	{
		return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aAmount), static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aAmount),
								static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aAmount));
	}

	auto small(QLabel* aLabel) -> QLabel*
	{
		auto font = aLabel->font();
		font.setPointSizeF(font.pointSizeF() * 0.92);
		aLabel->setFont(font);
		return aLabel;
	}

	auto words(const QString& aText, QWidget* aParent) -> QLabel*
	{
		auto* const label = new QLabel{ aText, aParent };
		label->setWordWrap(true);
		label->setTextFormat(Qt::PlainText);
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		return label;
	}

	auto quiet(QLabel* aLabel) -> QLabel*
	{
		aLabel->setForegroundRole(QPalette::PlaceholderText);
		return aLabel;
	}

	void clear(QVBoxLayout* aLayout)
	{
		while (auto* const item = aLayout->takeAt(0)) {
			if (auto* const widget = item->widget()) {
				widget->hide();
				widget->deleteLater();
			}
			if (auto* const inner = item->layout()) {
				clear(static_cast<QVBoxLayout*>(inner));
			}
			delete item;
		}
	}

	void paint_seam(QWidget& aColumn)
	{
		auto       painter = QPainter{ &aColumn };
		const auto ground  = aColumn.palette().color(QPalette::Active, QPalette::Window);
		const auto accent  = agent_accent(aColumn.palette());
		const auto washed  = std::min(aColumn.width(), 160);
		auto       wash    = QLinearGradient{ 0.0, 0.0, static_cast<qreal>(washed), 0.0 };
		wash.setColorAt(0.0, mixed(ground, accent, 0.06));
		wash.setColorAt(1.0, ground);
		painter.fillRect(QRect{ 0, 0, washed, aColumn.height() }, wash);
		painter.fillRect(QRect{ 0, 0, 1, aColumn.height() }, mixed(ground, accent, 0.5));
	}

	void fit_bubble(QWidget& aBubble, const int aAvailable)
	{
		const auto most = aAvailable * 4 / 5;
		aBubble.setMaximumWidth(most);
		for (auto* const label : aBubble.findChildren<QLabel*>()) {
			auto widest = 0;
			for (const auto& line : label->text().split(QLatin1Char('\n'))) {
				widest = std::max(widest, label->fontMetrics().horizontalAdvance(line));
			}
			label->setMinimumWidth(std::min(widest + 2, most - 24));
		}
	}

	send_keys::send_keys(QObject* aParent, std::function<void(bool aOpposite)> aSend)
		: QObject{ aParent }
		, Send{ std::move(aSend) }
	{
	}

	bool send_keys::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		if (aEvent->type() == QEvent::KeyPress) {
			const auto* key = static_cast<QKeyEvent*>(aEvent);
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !(key->modifiers() & Qt::ShiftModifier)) {
				// RFC-BPR-0126: Cmd+Return (Ctrl+Return off macOS) is the
				// other follow-up for this one message.
				Send((key->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) != 0);
				return true;
			}
		}
		return QObject::eventFilter(aWatched, aEvent);
	}

	void bubble_frame::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(Qt::NoPen);
		painter.setBrush(mixed(palette().color(QPalette::Active, QPalette::Base), palette().color(QPalette::Active, QPalette::WindowText), 0.08));
		painter.drawRoundedRect(QRectF{ rect() }, 12.0, 12.0);
	}

	void proposal_frame::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto accent  = agent_accent(palette());
		painter.fillRect(rect(), mixed(palette().color(QPalette::Active, QPalette::Base), accent, 0.06));
		painter.fillRect(QRect{ 0, 0, 2, height() }, accent);
	}
} // namespace bpr::qt::side_chat
