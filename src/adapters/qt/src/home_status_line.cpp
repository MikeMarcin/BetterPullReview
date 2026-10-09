#include "bpr/qt/home_status_line.hpp"

#include "bpr/qt/home_row_delegate.hpp"

#include <QAbstractButton>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QRectF>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	home_status_line::home_status_line(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("homeStatusLine"));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Row = new QHBoxLayout{ this };
		Row->setSpacing(mark_gap);
		Texts = new QWidget{ this };
		Texts->setObjectName(QStringLiteral("homeStatusLineTexts"));
		auto* const column = new QVBoxLayout{ Texts };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(2);
		Row->addWidget(Texts, 1, Qt::AlignVCenter);
		Turn = new QTimer{ this };
		Turn->setInterval(turn_interval);
		connect(Turn, &QTimer::timeout, this, [this] {
			Frame = (Frame + 1) % home_open_frames;
			update(mark_rect());
		});
		apply_margins();
		hide();
	}

	void home_status_line::adopt_text(QLabel& aLabel)
	{
		aLabel.setParent(Texts);
		static_cast<QVBoxLayout*>(Texts->layout())->addWidget(&aLabel);
		// Full ink at body size: the band is what the reviewer reads, not helper text.
		aLabel.setForegroundRole(QPalette::WindowText);
		aLabel.installEventFilter(this);
		Labels.push_back(&aLabel);
		follow_labels();
	}

	void home_status_line::adopt_button(QAbstractButton& aButton)
	{
		aButton.setParent(this);
		Row->addWidget(&aButton, 0, Qt::AlignVCenter);
	}

	void home_status_line::set_tone(const home_status_tone aTone)
	{
		if (Tone == aTone) {
			return;
		}
		Tone = aTone;
		apply_margins();
		if (Tone == home_status_tone::busy && isVisible()) {
			Turn->start();
		} else {
			Turn->stop();
		}
		update();
	}

	void home_status_line::set_failure_ink(const QColor& aInk)
	{
		Failure = aInk;
		update();
	}

	auto home_status_line::tone() const noexcept -> home_status_tone
	{
		return Tone;
	}

	auto home_status_line::failure_ink() const -> QColor
	{
		return Failure.isValid() ? Failure : palette().color(QPalette::WindowText);
	}

	auto home_status_line::frame() const noexcept -> int
	{
		return Frame;
	}

	auto home_status_line::mark_rect() const -> QRect
	{
		if (Tone == home_status_tone::notice) {
			return { };
		}
		// As tall as a line of text and level with the first, however many
		// lines the text wraps to.
		const auto extent = QFontMetrics{ font() }.height();
		return QRect{ inset_horizontal, Texts->y(), extent, extent };
	}

	auto home_status_line::tone_colour() const -> QColor
	{
		// Only a problem is coloured: on Home colour means state, so work under
		// way is said by its turning mark on the neutral band.
		return Tone == home_status_tone::problem ? failure_ink() : palette().color(QPalette::Mid);
	}

	auto home_status_line::fill() const -> QColor
	{
		const auto ground = palette().color(QPalette::Window);
		return Tone == home_status_tone::problem ? mix_colour(tone_colour(), ground, fill_strength) : palette().color(QPalette::Base);
	}

	auto home_status_line::edge() const -> QColor
	{
		const auto ground = palette().color(QPalette::Window);
		return Tone == home_status_tone::problem ? mix_colour(tone_colour(), ground, edge_strength) : palette().color(QPalette::Mid);
	}

	auto home_status_line::sizeHint() const -> QSize
	{
		return QWidget::sizeHint();
	}

	void home_status_line::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		QPainter painter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ edge(), 1.0 });
		painter.setBrush(fill());
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
		const auto mark = mark_rect();
		if (Tone == home_status_tone::busy) {
			const auto ink   = palette().color(QPalette::WindowText);
			auto       faint = ink;
			faint.setAlphaF(0.25F);
			draw_home_open_mark(painter, mark, Frame, ink, faint);
		} else if (Tone == home_status_tone::problem) {
			draw_home_failure_mark(painter, mark, font(), failure_ink());
		}
	}

	void home_status_line::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent != nullptr && (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange)) {
			apply_margins();
		}
		if (aEvent != nullptr && aEvent->type() == QEvent::PaletteChange) {
			update();
		}
	}

	auto home_status_line::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent != nullptr && (aEvent->type() == QEvent::ShowToParent || aEvent->type() == QEvent::HideToParent)
			&& std::ranges::find(Labels, aWatched) != Labels.end()) {
			// A label shown or hidden by the window shows or hides the band in
			// the same turn; Qt has already applied the label's own change.
			follow_labels();
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void home_status_line::follow_labels()
	{
		const auto said = std::ranges::any_of(Labels, [](const QLabel* const aLabel) { return !aLabel->isHidden(); });
		if (said != !isHidden()) {
			setVisible(said);
		}
		if (said && Tone == home_status_tone::busy) {
			Turn->start();
		} else {
			Turn->stop();
		}
	}

	void home_status_line::apply_margins()
	{
		const auto extent = Tone == home_status_tone::notice ? 0 : QFontMetrics{ font() }.height() + mark_gap;
		Row->setContentsMargins(inset_horizontal + extent, inset_vertical, inset_horizontal - 4, inset_vertical);
	}
} // namespace bpr::qt
