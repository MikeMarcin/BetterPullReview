#include "bpr/qt/refresh_note.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include "bpr/qt/home_row_delegate.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	refresh_note::refresh_note(QWidget& aWindow)
		: QWidget{ &aWindow }
		, Window{ &aWindow }
	{
		setObjectName(QStringLiteral("refreshNote"));
		setFocusPolicy(Qt::NoFocus);
		setAccessibleName(QStringLiteral("Refresh"));
		RetryButton = new quiet_push_button{ QStringLiteral("Retry"), this };
		RetryButton->setObjectName(QStringLiteral("refreshNoteRetry"));
		RetryButton->setAccessibleName(QStringLiteral("Retry refresh"));
		RetryButton->hide();
		connect(RetryButton, &QPushButton::clicked, this, [this] {
			dismiss();
			if (Retry) {
				Retry();
			}
		});
		Linger = new QTimer{ this };
		Linger->setSingleShot(true);
		connect(Linger, &QTimer::timeout, this, [this] { dismiss(); });
		Frames = new QTimer{ this };
		Frames->setInterval(80);
		connect(Frames, &QTimer::timeout, this, [this] {
			Frame = (Frame + 1) % home_open_frames;
			update();
		});
		aWindow.installEventFilter(this);
		hide();
	}

	void refresh_note::present(const refresh_note_tone aTone, const QString& aText, QWidget* const aAnchor, const int aLingerMs)
	{
		if (Anchor != nullptr && Anchor != aAnchor) {
			Anchor->removeEventFilter(this);
		}
		Tone   = aTone;
		Text   = aText;
		Anchor = aAnchor;
		if (Anchor != nullptr) {
			Anchor->installEventFilter(this);
		}
		RetryButton->setVisible(Tone == refresh_note_tone::failed && static_cast<bool>(Retry));
		setAccessibleDescription(Text);
		if (aLingerMs > 0) {
			Linger->start(aLingerMs);
		} else {
			Linger->stop();
		}
		if (Tone == refresh_note_tone::busy && FullMotion) {
			Frames->start();
		} else {
			Frames->stop();
		}
		place();
		show();
		raise();
		update();
	}

	void refresh_note::dismiss()
	{
		Linger->stop();
		Frames->stop();
		hide();
	}

	void refresh_note::set_retry(std::function<void()> aRetry)
	{
		Retry = std::move(aRetry);
	}

	void refresh_note::set_full_motion(const bool aFull)
	{
		FullMotion = aFull;
		if (isVisible() && Tone == refresh_note_tone::busy && FullMotion) {
			Frames->start();
		} else {
			Frames->stop();
		}
		update();
	}

	void refresh_note::set_failure_ink(const QColor& aInk)
	{
		FailureInk = aInk;
		update();
	}

	auto refresh_note::tone() const noexcept -> refresh_note_tone
	{
		return Tone;
	}

	auto refresh_note::text() const -> QString
	{
		return Text;
	}

	auto refresh_note::anchor() const noexcept -> QWidget*
	{
		return Anchor;
	}

	auto refresh_note::retry_button() const noexcept -> QPushButton*
	{
		return RetryButton;
	}

	auto refresh_note::frame() const noexcept -> int
	{
		return Frame;
	}

	void refresh_note::place()
	{
		if (Window == nullptr) {
			return;
		}
		const auto metrics = QFontMetrics{ font() };
		const auto line    = metrics.height();
		const auto retry   = RetryButton->isVisible() ? RetryButton->sizeHint() : QSize{ };
		const auto room    = std::min(maximum, std::max(160, Window->width() - (2 * padding)));
		const auto textMax = room - (2 * padding) - line - gap - (retry.isEmpty() ? 0 : retry.width() + gap);
		const auto bounds  = metrics.boundingRect(QRect{ 0, 0, std::max(40, textMax), line * 4 }, Qt::TextWordWrap, Text);
		const auto width   = (2 * padding) + line + gap + bounds.width() + (retry.isEmpty() ? 0 : retry.width() + gap) + 2;
		const auto height  = std::max(bounds.height(), retry.height()) + (2 * padding) - 4;
		// Right-aligned under the anchor, so it reads as the button's own word.
		auto right = Window->width() - padding;
		auto top   = padding;
		if (Anchor != nullptr && Anchor->isVisible()) {
			const auto at = Anchor->mapTo(Window, QPoint{ Anchor->width(), Anchor->height() });
			right         = std::min(right, at.x());
			top           = at.y() + 6;
		}
		setGeometry(std::max(padding, right - width), top, width, height);
		if (RetryButton->isVisible()) {
			RetryButton->setGeometry(QRect{ QPoint{ width - padding - retry.width(), (height - retry.height()) / 2 }, retry });
		}
	}

	auto refresh_note::ground() const -> QColor
	{
		return palette().color(QPalette::Window);
	}

	auto refresh_note::failure() const -> QColor
	{
		return FailureInk.isValid() ? FailureInk : palette().color(QPalette::WindowText);
	}

	void refresh_note::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto ink    = palette().color(QPalette::WindowText);
		const auto failed = Tone == refresh_note_tone::failed;
		// Opaque, so the content under it never shows through the words: the
		// window's ground with its ink's plate, as Home's status band.
		painter.setPen(QPen{ failed ? mix_colour(failure(), ground(), 0.45) : mix_colour(ink, ground(), 0.22), 1.0 });
		painter.setBrush(failed ? mix_colour(failure(), ground(), failure_fill) : mix_colour(ink, ground(), plate));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
		const auto line = QFontMetrics{ font() }.height();
		const auto mark = QRect{ padding, padding - 2, line, line };
		switch (Tone) {
		case refresh_note_tone::busy: {
			auto faint = ink;
			faint.setAlphaF(0.25F);
			draw_home_open_mark(painter, mark, FullMotion ? Frame : 0, ink, faint);
			break;
		}
		case refresh_note_tone::settled: {
			painter.save();
			auto pen = QPen{ ink, std::max(1.5, line / 9.0) };
			pen.setCapStyle(Qt::RoundCap);
			pen.setJoinStyle(Qt::RoundJoin);
			painter.setPen(pen);
			auto       tick = QPainterPath{ };
			const auto box  = QRectF{ mark }.adjusted(2.0, 2.0, -2.0, -2.0);
			tick.moveTo(box.left(), box.center().y());
			tick.lineTo(box.left() + (box.width() * 0.38), box.bottom() - (box.height() * 0.12));
			tick.lineTo(box.right(), box.top() + (box.height() * 0.12));
			painter.drawPath(tick);
			painter.restore();
			break;
		}
		case refresh_note_tone::failed:
			draw_home_failure_mark(painter, mark, font(), failure());
			break;
		}
		const auto right = RetryButton->isVisible() ? RetryButton->geometry().left() - gap : width() - padding;
		painter.setPen(ink);
		painter.drawText(QRect{ mark.right() + gap, padding - 2, std::max(0, right - mark.right() - gap), height() - (2 * padding) + 4 },
						 Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, Text);
	}

	void refresh_note::mousePressEvent(QMouseEvent* const aEvent)
	{
		// A press anywhere on the words puts the note away.
		dismiss();
		if (aEvent != nullptr) {
			aEvent->accept();
		}
	}

	auto refresh_note::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent != nullptr && isVisible() && (aWatched == Window || aWatched == Anchor)) {
			switch (aEvent->type()) {
			case QEvent::Resize:
			case QEvent::Move:
				place();
				break;
			case QEvent::Hide:
				if (aWatched == Anchor) {
					dismiss();
				}
				break;
			default:
				break;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
