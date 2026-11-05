#include "bpr/qt/home_filter_field.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QRectF>

#include <utility>

namespace bpr::qt {
	namespace {
		/// The field's place in Home's column: the bar's own inset, so the
		/// magnifier stands under the bar's label.
		constexpr auto column_inset   = 24;
		constexpr auto line_inset     = 12;
		constexpr auto vertical_inset = 6;
		constexpr auto rest_rule      = 1.0;
		constexpr auto focus_rule     = 1.5;

		[[nodiscard]] auto count_text(const int aShown) -> QString
		{
			return aShown == 0 ? QStringLiteral("No matches")
				: aShown == 1  ? QStringLiteral("1 match")
							   : QStringLiteral("%1 matches").arg(aShown);
		}
	} // namespace

	home_filter_field::home_filter_field(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("homeFilter"));
		setContentsMargins(column_inset, 2, column_inset, 0);
		Edit = new QLineEdit{ this };
		Edit->setObjectName(QStringLiteral("homeFilterField"));
		Edit->setFrame(false);
		Edit->setClearButtonEnabled(true);
		Edit->setPlaceholderText(QStringLiteral("Filter by number, title, repository or author"));
		Edit->setAccessibleName(QStringLiteral("Filter Home"));
		Edit->setAccessibleDescription(QStringLiteral(
			"Type to filter Home's pull requests by number, title, repository or author. Up and Down choose a result, Return opens it, Escape clears the filter."));
		// The window's ground, as the lists below it: this is part of them.
		Edit->setAttribute(Qt::WA_MacShowFocusRect, false);
		auto ground = Edit->palette();
		ground.setColor(QPalette::Base, Qt::transparent);
		Edit->setPalette(ground);
		Edit->installEventFilter(this);
		Count = new QLabel{ this };
		Count->setObjectName(QStringLiteral("homeFilterCount"));
		Count->setForegroundRole(QPalette::PlaceholderText);
		Count->hide();
		Shortcut = new QLabel{ this };
		Shortcut->setObjectName(QStringLiteral("homeFilterShortcut"));
		Shortcut->setForegroundRole(QPalette::PlaceholderText);
		Shortcut->setAccessibleName(QStringLiteral("Shortcut to the filter"));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(line_inset + glyph_extent + 8, vertical_inset, line_inset, vertical_inset);
		row->setSpacing(8);
		row->addWidget(Edit, 1);
		row->addWidget(Count);
		row->addWidget(Shortcut);
		setFocusProxy(Edit);
		connect(Edit, &QLineEdit::textChanged, this, [this](const QString& aText) {
			update_trailing();
			if (QueryObserver) {
				QueryObserver(aText);
			}
		});
		update_trailing();
	}

	auto home_filter_field::line_edit() const noexcept -> QLineEdit*
	{
		return Edit;
	}

	auto home_filter_field::count_label() const noexcept -> QLabel*
	{
		return Count;
	}

	auto home_filter_field::shortcut_label() const noexcept -> QLabel*
	{
		return Shortcut;
	}

	auto home_filter_field::query() const -> QString
	{
		return Edit->text();
	}

	void home_filter_field::set_shortcut(const QKeySequence& aKey)
	{
		Shortcut->setText(aKey.toString(QKeySequence::NativeText));
		update_trailing();
	}

	void home_filter_field::set_count(const int aShown)
	{
		Shown = aShown;
		update_trailing();
	}

	void home_filter_field::focus_field()
	{
		Edit->setFocus(Qt::ShortcutFocusReason);
		Edit->selectAll();
	}

	void home_filter_field::take_typing(const QString& aText)
	{
		Edit->setFocus(Qt::OtherFocusReason);
		if (!aText.isEmpty()) {
			Edit->end(false);
			Edit->insert(aText);
		} else {
			Edit->selectAll();
		}
	}

	void home_filter_field::clear_query()
	{
		Edit->clear();
	}

	void home_filter_field::set_query_observer(std::function<void(const QString&)> aObserver)
	{
		QueryObserver = std::move(aObserver);
	}

	void home_filter_field::set_step_dispatch(std::function<bool(int aStep)> aDispatch)
	{
		StepDispatch = std::move(aDispatch);
	}

	void home_filter_field::set_open_dispatch(std::function<void(bool aNewWindow)> aDispatch)
	{
		OpenDispatch = std::move(aDispatch);
	}

	void home_filter_field::set_leave_dispatch(std::function<void()> aDispatch)
	{
		LeaveDispatch = std::move(aDispatch);
	}

	void home_filter_field::update_trailing()
	{
		const auto filtering = !Edit->text().trimmed().isEmpty();
		Count->setText(count_text(Shown));
		Count->setVisible(filtering);
		Count->setAccessibleName(filtering ? count_text(Shown) : QString{ });
		Shortcut->setVisible(!filtering && !Shortcut->text().isEmpty());
	}

	void home_filter_field::paintEvent(QPaintEvent* const aEvent)
	{
		QWidget::paintEvent(aEvent);
		auto       painter = QPainter{ this };
		const auto area    = contentsRect();
		const auto focused = Edit->hasFocus();
		const auto ink     = palette().color(focused ? QPalette::WindowText : QPalette::PlaceholderText);
		painter.setRenderHint(QPainter::Antialiasing, true);
		// The magnifier: a ring and its handle, in the placeholder's ink at
		// rest and the text's while the field has the keyboard.
		const auto side   = static_cast<qreal>(glyph_extent);
		const auto top    = area.top() + ((area.height() - glyph_extent) / 2.0);
		const auto left   = area.left() + static_cast<qreal>(line_inset);
		const auto stroke = 1.5;
		const auto ring   = QRectF{ left + (stroke / 2.0), top + (stroke / 2.0), side * 0.66, side * 0.66 };
		painter.setPen(QPen{ ink, stroke, Qt::SolidLine, Qt::RoundCap });
		painter.setBrush(Qt::NoBrush);
		painter.drawEllipse(ring);
		const auto from = ring.center() + QPointF{ ring.width() * 0.36, ring.height() * 0.36 };
		painter.drawLine(from, QPointF{ left + side - (stroke / 2.0), top + side - (stroke / 2.0) });
		// The hairline the lists' own rules echo; heavier, in the text's ink,
		// while the field has the keyboard. Never a state colour.
		const auto rule = focused ? focus_rule : rest_rule;
		auto       line = palette().color(focused ? QPalette::WindowText : QPalette::Mid);
		painter.setPen(QPen{ line, rule });
		const auto y = static_cast<qreal>(area.bottom()) + 1.0 - (rule / 2.0);
		painter.drawLine(QPointF{ static_cast<qreal>(area.left()), y }, QPointF{ static_cast<qreal>(area.right()) + 1.0, y });
	}

	auto home_filter_field::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == Edit && aEvent != nullptr) {
			if (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut) {
				update();
			}
			if (aEvent->type() == QEvent::KeyPress) {
				const auto* const key       = static_cast<const QKeyEvent*>(aEvent);
				const auto        modifiers = key->modifiers() & ~Qt::KeypadModifier;
				switch (key->key()) {
				case Qt::Key_Down:
				case Qt::Key_Up:
					if (modifiers == Qt::NoModifier) {
						if (StepDispatch) {
							StepDispatch(key->key() == Qt::Key_Down ? 1 : -1);
						}
						return true;
					}
					break;
				case Qt::Key_Return:
				case Qt::Key_Enter:
					if (OpenDispatch) {
						OpenDispatch(modifiers.testFlag(Qt::ControlModifier));
					}
					return true;
				case Qt::Key_Escape:
					if (!Edit->text().isEmpty()) {
						Edit->clear();
					} else if (LeaveDispatch) {
						LeaveDispatch();
					}
					return true;
				default:
					break;
				}
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void home_filter_field::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent != nullptr && aEvent->type() == QEvent::PaletteChange) {
			auto ground = Edit->palette();
			if (ground.color(QPalette::Base) != Qt::transparent) {
				ground.setColor(QPalette::Base, Qt::transparent);
				Edit->setPalette(ground);
			}
			update();
		}
	}
} // namespace bpr::qt
