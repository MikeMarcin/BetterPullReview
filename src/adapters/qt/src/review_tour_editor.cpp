#include "bpr/qt/review_tour_editor.hpp"

#include "bpr/core/tour_steps.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include "conversation_glyphs.hpp"
#include "waiting_panel.hpp"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QDropEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextLayout>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
namespace bpr::qt {
	namespace {
		constexpr int preparation_role = Qt::UserRole + 1;

		/// \brief The outline's rows carry what the route draws of each stop.
		constexpr int number_role    = Qt::UserRole + 2;
		constexpr int source_role    = Qt::UserRole + 3;
		constexpr int excerpt_role   = Qt::UserRole + 4;
		constexpr int file_role      = Qt::UserRole + 5;
		constexpr int divider_role   = Qt::UserRole + 6;
		constexpr int current_role   = Qt::UserRole + 7;
		constexpr int attention_role = Qt::UserRole + 8;
		/// The route's measures: the grip, the circle's column, the text.
		constexpr int grip_width   = 18;
		constexpr int ring_left    = 22;
		constexpr int ring_size    = 22;
		constexpr int text_left    = ring_left + ring_size + 10;
		constexpr int divider_band = 26;

		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			const auto keep = 1.0 - aShare;
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
									static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
		}

		/// The route's amber: where you read, as the drawers draw it.
		[[nodiscard]] auto route_amber(const QPalette& aPalette) -> QColor
		{
			const auto dark = aPalette.color(QPalette::Window).lightness() < 128;
			return dark ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
		}

		[[nodiscard]] auto route_green(const QPalette& aPalette) -> QColor
		{
			const auto dark = aPalette.color(QPalette::Window).lightness() < 128;
			return dark ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
		}

		[[nodiscard]] auto code_font_of(const QFont& aFont) -> QFont
		{
			auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
			code.setPointSizeF(std::max(1.0, aFont.pointSizeF() * 0.86));
			return code;
		}

		/// \brief [[BPR-1593]]. RFC-BPR-0085 D4: each stop on the route, a
		/// numbered circle on an amber line, its source in a code chip, its
		/// readiness in its ring; a file divider where the route changes file.
		class route_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;
			bool Page = false;

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				// The current stop's card sets its own height.
				if (const auto given = aIndex.data(Qt::SizeHintRole); given.isValid() && given.toSize().height() > 0) {
					return given.toSize();
				}
				const auto metrics = QFontMetrics{ aOption.font };
				const auto code    = QFontMetrics{ code_font_of(aOption.font) };
				const auto width   = std::max(120, aOption.rect.width() > 0 ? aOption.rect.width() : 300);
				auto       height  = 8 + std::max(ring_size, metrics.height()) + 4 + code.height() + 6;
				const auto excerpt = aIndex.data(excerpt_role).toString();
				if (!Page && !excerpt.isEmpty()) {
					const auto lines = std::min(2, static_cast<int>(std::ceil(static_cast<double>(metrics.horizontalAdvance(excerpt)) / std::max(1, width - text_left - 10))));
					height += lines * metrics.lineSpacing() + 2;
				}
				if (aIndex.data(divider_role).toBool()) {
					height += divider_band;
				}
				return { width, height + 6 };
			}

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				aPainter->save();
				aPainter->setRenderHint(QPainter::Antialiasing, true);
				const auto& palette  = aOption.palette;
				const auto  ink      = palette.color(QPalette::Active, QPalette::WindowText);
				const auto  faint    = palette.color(QPalette::Active, QPalette::PlaceholderText);
				const auto  base     = palette.color(QPalette::Active, QPalette::Base);
				const auto  window   = palette.color(QPalette::Active, QPalette::Window);
				const auto  amber    = route_amber(palette);
				const auto  rect     = aOption.rect;
				const auto  current  = aIndex.data(current_role).toBool();
				const auto  selected = aOption.state.testFlag(QStyle::State_Selected);
				const auto  hovered  = aOption.state.testFlag(QStyle::State_MouseOver);
				const auto  model    = aIndex.model();
				const auto  last     = aIndex.row() + 1 == model->rowCount();
				const auto  first    = aIndex.row() == 0;
				auto        top      = rect.top();
				if (aIndex.data(divider_role).toBool()) {
					// The file the route enters, in a code chip.
					const auto code  = code_font_of(aOption.font);
					const auto file  = aIndex.data(file_role).toString();
					const auto width = QFontMetrics{ code }.horizontalAdvance(file) + 12;
					const auto chip  = QRectF{ static_cast<qreal>(rect.left() + grip_width - 6), top + 6.0, static_cast<qreal>(width), divider_band - 10.0 };
					aPainter->setPen(QPen{ mixed(window, ink, 0.18), 1.0 });
					aPainter->setBrush(mixed(window, ink, 0.05));
					aPainter->drawRoundedRect(chip, 4.0, 4.0);
					aPainter->setFont(code);
					aPainter->setPen(mixed(ink, window, 0.25));
					aPainter->drawText(chip, Qt::AlignCenter, file);
					top += divider_band;
				}
				const auto ringCentre = QPointF{ rect.left() + ring_left + ring_size / 2.0, top + 8.0 + ring_size / 2.0 };
				// The route line, through every stop.
				auto line = amber;
				line.setAlphaF(0.55F);
				aPainter->setPen(QPen{ line, 2.0 });
				if (!first) {
					aPainter->drawLine(QPointF{ ringCentre.x(), static_cast<qreal>(rect.top()) }, QPointF{ ringCentre.x(), ringCentre.y() - ring_size / 2.0 });
				}
				if (!last) {
					aPainter->drawLine(QPointF{ ringCentre.x(), ringCentre.y() + ring_size / 2.0 }, QPointF{ ringCentre.x(), static_cast<qreal>(rect.bottom() + 1) });
				}
				// The current stop's ground, and the selection's.
				if (current || selected || hovered) {
					const auto ground = QRectF{ rect.adjusted(grip_width - 4, 2, -2, -2) };
					aPainter->setPen(current ? QPen{ mixed(base, amber, 0.55), 1.0 } : Qt::NoPen);
					aPainter->setBrush(current ? base : mixed(window, ink, selected ? 0.07 : 0.04));
					aPainter->drawRoundedRect(ground, 8.0, 8.0);
					if (current) {
						aPainter->setPen(Qt::NoPen);
						aPainter->setBrush(amber);
						aPainter->drawRoundedRect(QRectF{ ground.left(), ground.top() + 6.0, 3.0, ground.height() - 12.0 }, 1.5, 1.5);
					}
				}
				// The grip, while it can be taken.
				if (Page || hovered || selected) {
					aPainter->setPen(Qt::NoPen);
					aPainter->setBrush(faint);
					for (int row = -1; row <= 1; ++row) {
						for (int column = 0; column <= 1; ++column) {
							aPainter->drawEllipse(QPointF{ rect.left() + 6.0 + column * 4.0, ringCentre.y() + row * 4.0 }, 1.1, 1.1);
						}
					}
				}
				// The ring: its readiness.
				const auto preparation = static_cast<core::tour_preparation>(aIndex.data(preparation_role).toInt());
				const auto ring        = QRectF{ ringCentre.x() - ring_size / 2.0, ringCentre.y() - ring_size / 2.0, static_cast<qreal>(ring_size), static_cast<qreal>(ring_size) };
				const auto number      = aIndex.data(number_role).toString();
				auto       small       = aOption.font;
				small.setWeight(QFont::DemiBold);
				small.setPointSizeF(std::max(1.0, small.pointSizeF() * 0.86));
				aPainter->setFont(small);
				switch (preparation) {
				case core::tour_preparation::ready: {
					const auto green = route_green(palette);
					aPainter->setPen(QPen{ green, 1.6 });
					aPainter->setBrush(base);
					aPainter->drawEllipse(ring.adjusted(1.0, 1.0, -1.0, -1.0));
					auto tick = QPainterPath{ };
					tick.moveTo(ring.left() + 6.5, ring.center().y() + 0.5);
					tick.lineTo(ring.left() + 9.5, ring.center().y() + 3.5);
					tick.lineTo(ring.right() - 6.0, ring.center().y() - 3.0);
					aPainter->setBrush(Qt::NoBrush);
					aPainter->drawPath(tick);
					break;
				}
				case core::tour_preparation::suggested:
					aPainter->setPen(QPen{ faint, 1.4, Qt::DashLine });
					aPainter->setBrush(base);
					aPainter->drawEllipse(ring.adjusted(1.0, 1.0, -1.0, -1.0));
					aPainter->setPen(faint);
					aPainter->drawText(ring, Qt::AlignCenter, number);
					break;
				case core::tour_preparation::editing:
				case core::tour_preparation::needs_attention:
					aPainter->setPen(QPen{ amber, 1.6 });
					aPainter->setBrush(current ? amber : base);
					aPainter->drawEllipse(ring.adjusted(1.0, 1.0, -1.0, -1.0));
					aPainter->setPen(current ? base : mixed(ink, amber, 0.35));
					aPainter->drawText(ring, Qt::AlignCenter, number);
					if (preparation == core::tour_preparation::needs_attention) {
						// Needs attention: a warning on the ring, in any colours.
						auto       warn = QPainterPath{ };
						const auto x    = ring.right() - 3.0;
						const auto y    = ring.top() - 2.0;
						warn.moveTo(x, y);
						warn.lineTo(x + 5.0, y + 8.5);
						warn.lineTo(x - 5.0, y + 8.5);
						warn.closeSubpath();
						aPainter->setPen(QPen{ base, 1.5 });
						aPainter->setBrush(amber);
						aPainter->drawPath(warn);
					}
					break;
				}
				// The current stop writes its own title, source and words, in its
				// card over this row.
				if (current && !Page) {
					aPainter->restore();
					return;
				}
				// The title, and what is wrong with the stop, at the right.
				auto title = aOption.font;
				title.setWeight(QFont::DemiBold);
				if (preparation == core::tour_preparation::suggested) {
					title.setWeight(QFont::Normal);
					title.setItalic(true);
				}
				const auto left  = rect.left() + text_left;
				auto       right = rect.right() - 10;
				if (aIndex.data(attention_role).toBool() || preparation == core::tour_preparation::needs_attention) {
					const auto words        = QObject::tr("Lines changed");
					const auto smallMetrics = QFontMetrics{ small };
					const auto w            = smallMetrics.horizontalAdvance(words);
					aPainter->setFont(small);
					aPainter->setPen(amber);
					aPainter->drawText(QRect{ right - w, static_cast<int>(ring.top()), w + 1, ring_size }, Qt::AlignRight | Qt::AlignVCenter, words);
					auto       warn = QPainterPath{ };
					const auto wx   = right - w - 14.0;
					warn.moveTo(wx + 5.0, ring.center().y() - 5.0);
					warn.lineTo(wx + 10.0, ring.center().y() + 4.0);
					warn.lineTo(wx, ring.center().y() + 4.0);
					warn.closeSubpath();
					aPainter->setBrush(Qt::NoBrush);
					aPainter->setPen(QPen{ amber, 1.2 });
					aPainter->drawPath(warn);
					right -= w + 20;
				}
				aPainter->setFont(title);
				aPainter->setPen(preparation == core::tour_preparation::suggested ? faint : ink);
				const auto words = QFontMetrics{ title }.elidedText(aIndex.data(Qt::DisplayRole).toString(), Qt::ElideRight, std::max(10, right - left));
				aPainter->drawText(QRect{ left, static_cast<int>(ring.top()), std::max(10, right - left), ring_size }, Qt::AlignLeft | Qt::AlignVCenter, words);
				// Its source, in the code's figures.
				auto       y      = static_cast<int>(ring.bottom()) + 4;
				const auto source = aIndex.data(source_role).toString();
				if (!source.isEmpty()) {
					const auto code = code_font_of(aOption.font);
					const auto cm   = QFontMetrics{ code };
					const auto chip = QRectF{ static_cast<qreal>(left), static_cast<qreal>(y), static_cast<qreal>(std::min(cm.horizontalAdvance(source) + 12, rect.right() - 10 - left)), static_cast<qreal>(cm.height() + 2) };
					aPainter->setPen(QPen{ mixed(base, ink, 0.18), 1.0 });
					aPainter->setBrush(mixed(window, ink, 0.05));
					aPainter->drawRoundedRect(chip, 4.0, 4.0);
					auto boldCode = code;
					boldCode.setWeight(QFont::DemiBold);
					aPainter->setFont(boldCode);
					aPainter->setPen(mixed(ink, window, 0.15));
					aPainter->drawText(chip, Qt::AlignCenter, cm.elidedText(source, Qt::ElideMiddle, static_cast<int>(chip.width()) - 10));
					y += cm.height() + 6;
				}
				// A line or two of its explanation, on the route.
				const auto excerpt = aIndex.data(excerpt_role).toString();
				if (!Page && !excerpt.isEmpty()) {
					aPainter->setFont(aOption.font);
					aPainter->setPen(mixed(ink, window, 0.3));
					const auto bounds = QRect{ left, y, rect.right() - 10 - left, rect.bottom() - y - 4 };
					aPainter->drawText(bounds, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, QFontMetrics{ aOption.font }.elidedText(excerpt, Qt::ElideRight, bounds.width() * 2 - 20));
				}
				if (aOption.state.testFlag(QStyle::State_HasFocus) && aOption.widget != nullptr && aOption.widget->hasFocus()) {
					aPainter->setPen(QPen{ palette.color(QPalette::Active, QPalette::Highlight), 2.0 });
					aPainter->setBrush(Qt::NoBrush);
					aPainter->drawRoundedRect(QRectF{ rect.adjusted(grip_width - 3, 3, -3, -3) }, 8.0, 8.0);
				}
				aPainter->restore();
			}
		};

		[[nodiscard]] auto attention_red(const QPalette& aPalette) -> QColor
		{
			const auto dark = aPalette.color(QPalette::Window).lightness() < 128;
			return dark ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
		}

		/// \brief [[BPR-1784]]. Change, as the agent setup draws it: words in
		/// the accent, no frame, a focus ring when the keyboard is on it.
		class step_change final : public QPushButton
		{
		public:
			using QPushButton::QPushButton;

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { metrics.horizontalAdvance(text()) + 12, metrics.height() + 6 };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				auto bold = font();
				bold.setWeight(QFont::DemiBold);
				painter.setFont(bold);
				painter.setPen(palette().color(QPalette::Active, QPalette::Highlight));
				painter.drawText(rect(), Qt::AlignCenter, text());
				if (hasFocus()) {
					painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 1.5 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), 5.0, 5.0);
				}
			}
		};

		/// \brief [[BPR-1784]]. One step's header, in the agent setup's
		/// language (RFC-BPR-0092): a numbered circle, the step's name, and
		/// once done what was written and Change. The circle is the accent
		/// while open, green with a tick once done, a red ring with "!" when
		/// it needs the author, and quiet ahead; never the stops' amber.
		class tour_step_row final : public QAbstractButton
		{
		public:
			static constexpr int marker = 24;
			static constexpr int indent = marker + 10;

			tour_step_row(const int aNumber, const QString& aName, QWidget* aParent)
				: QAbstractButton{ aParent }
				, Number{ aNumber }
				, Name{ aName }
			{
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
				setAttribute(Qt::WA_Hover, true);
				setCheckable(true);
				setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
				Change = new step_change{ tr("Change"), this };
				Change->setAccessibleName(tr("Change the %1").arg(aName.toLower()));
				Change->setCursor(Qt::PointingHandCursor);
				Change->hide();
				present(core::tour_step_state::to_do, { });
			}
			int                   Number;
			QString               Name;
			QString               Summary;
			core::tour_step_state State  = core::tour_step_state::to_do;
			step_change*          Change = nullptr;

			void present(const core::tour_step_state aState, const QString& aSummary)
			{
				State   = aState;
				Summary = aSummary;
				Change->setVisible(aState == core::tour_step_state::done);
				auto said = QString{ };
				switch (aState) {
				case core::tour_step_state::to_do:           said = tr("to do"); break;
				case core::tour_step_state::current:         said = tr("open"); break;
				case core::tour_step_state::done:            said = tr("done"); break;
				case core::tour_step_state::needs_attention: said = tr("needs attention"); break;
				}
				setAccessibleName(tr("Step %1, %2, %3").arg(Number).arg(Name, said));
				setAccessibleDescription(aSummary);
				setToolTip(aState == core::tour_step_state::current ? QString{ } : aState == core::tour_step_state::done ? tr("Change the %1").arg(Name.toLower())
																														 : tr("Open the %1").arg(Name.toLower()));
				place_change();
				update();
			}
			[[nodiscard]] auto sizeHint() const -> QSize override { return { 200, std::max(marker, QFontMetrics{ font() }.lineSpacing()) + 8 }; }
			[[nodiscard]] auto minimumSizeHint() const -> QSize override { return { 120, sizeHint().height() }; }

		protected:
			void resizeEvent(QResizeEvent* aEvent) override
			{
				QAbstractButton::resizeEvent(aEvent);
				place_change();
			}
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) {
					click();
					return;
				}
				QAbstractButton::keyPressEvent(aEvent);
			}
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
				const auto window = palette().color(QPalette::Active, QPalette::Window);
				const auto accent = palette().color(QPalette::Active, QPalette::Highlight);
				if (underMouse() && State != core::tour_step_state::current) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(mixed(window, ink, 0.04));
					painter.drawRoundedRect(QRectF{ rect() }, 6.0, 6.0);
				}
				const auto box    = QRectF{ 1.0, (height() - marker) / 2.0 + 1.0, marker - 2.0, marker - 2.0 };
				auto       number = font();
				number.setWeight(QFont::DemiBold);
				number.setPointSizeF(std::max(1.0, number.pointSizeF() * 0.85));
				switch (State) {
				case core::tour_step_state::done: {
					painter.setPen(Qt::NoPen);
					painter.setBrush(route_green(palette()));
					painter.drawEllipse(box);
					auto tick = QPen{ QColor{ Qt::white }, 2.0 };
					tick.setCapStyle(Qt::RoundCap);
					tick.setJoinStyle(Qt::RoundJoin);
					painter.setPen(tick);
					const auto c = box.center();
					painter.drawPolyline(std::array{ QPointF{ c.x() - 4.5, c.y() }, QPointF{ c.x() - 1.5, c.y() + 3.0 }, QPointF{ c.x() + 4.5, c.y() - 3.5 } }.data(), 3);
					break;
				}
				case core::tour_step_state::current:
					painter.setPen(Qt::NoPen);
					painter.setBrush(accent);
					painter.drawEllipse(box);
					painter.setFont(number);
					painter.setPen(palette().color(QPalette::Active, QPalette::HighlightedText));
					painter.drawText(box, Qt::AlignCenter, QString::number(Number));
					break;
				case core::tour_step_state::needs_attention: {
					const auto red = attention_red(palette());
					painter.setPen(QPen{ red, 1.5 });
					painter.setBrush(Qt::NoBrush);
					painter.drawEllipse(box.adjusted(0.5, 0.5, -0.5, -0.5));
					painter.setFont(number);
					painter.drawText(box, Qt::AlignCenter, QStringLiteral("!"));
					break;
				}
				case core::tour_step_state::to_do:
					painter.setPen(QPen{ mixed(window, ink, 0.35), 1.2 });
					painter.setBrush(Qt::NoBrush);
					painter.drawEllipse(box);
					painter.setFont(number);
					painter.setPen(mixed(window, ink, 0.55));
					painter.drawText(box, Qt::AlignCenter, QString::number(Number));
					break;
				}
				auto bold = font();
				bold.setWeight(QFont::DemiBold);
				const auto nameWidth = QFontMetrics{ bold }.horizontalAdvance(Name);
				painter.setFont(bold);
				painter.setPen(State == core::tour_step_state::to_do ? mixed(window, ink, 0.55) : ink);
				painter.drawText(QRect{ indent, 0, nameWidth + 2, height() }, Qt::AlignLeft | Qt::AlignVCenter, Name);
				if (!Summary.isEmpty()) {
					const auto left  = indent + nameWidth + 10;
					const auto right = Change->isVisible() ? Change->x() - 6 : width() - 6;
					painter.setFont(font());
					painter.setPen(State == core::tour_step_state::needs_attention ? attention_red(palette()) : mixed(ink, window, 0.35));
					const auto flat = QString{ Summary }.replace(QLatin1Char{ '\n' }, QLatin1Char{ ' ' }).simplified();
					painter.drawText(QRect{ left, 0, std::max(0, right - left), height() }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ font() }.elidedText(flat, Qt::ElideRight, std::max(0, right - left)));
				}
				if (hasFocus()) {
					painter.setPen(QPen{ accent, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), 6.0, 6.0);
				}
			}

		private:
			void place_change()
			{
				const auto hint = Change->sizeHint();
				Change->setGeometry(width() - hint.width(), (height() - hint.height()) / 2, hint.width(), hint.height());
			}
		};

		/// \brief [[BPR-1784]]. The three steps, joined as the agent setup
		/// joins its own: a quiet rail from each circle to the next.
		class tour_steps_frame final : public QWidget
		{
		public:
			explicit tour_steps_frame(QWidget* aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("tourSteps"));
				setAccessibleName(tr("Tour steps"));
			}
			std::array<tour_step_row*, core::tour_step_count> Rows{ };

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto       painter = QPainter{ this };
				const auto ink     = palette().color(QPalette::Active, QPalette::WindowText);
				const auto window  = palette().color(QPalette::Active, QPalette::Window);
				painter.setPen(QPen{ mixed(window, ink, 0.16), 1.5 });
				for (std::size_t index = 0U; index + 1U < Rows.size(); ++index) {
					if (Rows[index] == nullptr || Rows[index + 1U] == nullptr || !Rows[index]->isVisible()) {
						continue;
					}
					const auto from = Rows[index]->mapTo(this, QPoint{ tour_step_row::marker / 2, (Rows[index]->height() + tour_step_row::marker) / 2 + 2 });
					const auto to   = Rows[index + 1U]->mapTo(this, QPoint{ tour_step_row::marker / 2, (Rows[index + 1U]->height() - tour_step_row::marker) / 2 - 2 });
					if (to.y() > from.y()) {
						painter.drawLine(QPointF{ from.x() + 0.5, static_cast<qreal>(from.y()) }, QPointF{ from.x() + 0.5, static_cast<qreal>(to.y()) });
					}
				}
			}
		};

		/// \brief [[BPR-1593]]. The code around the stop's lines, numbered, under
		/// the amber bracket, as the comparison draws the open thread's lines.
		class excerpt_view final : public QWidget
		{
		public:
			explicit excerpt_view(QWidget* aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("tourStopExcerpt"));
				setAccessibleName(tr("The stop's code"));
				setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			}
			std::optional<tour_excerpt> Shown;

			void present(std::optional<tour_excerpt> aExcerpt)
			{
				const auto hasLines = aExcerpt.has_value() && !aExcerpt->Lines.empty();
				Shown               = std::move(aExcerpt);
				setVisible(hasLines);
				updateGeometry();
				update();
			}
			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto lines = Shown ? static_cast<int>(Shown->Lines.size()) : 0;
				return { 300, 12 + lines * QFontMetrics{ code_font_of(font()) }.lineSpacing() };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				if (!Shown) {
					return;
				}
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
				const auto base   = palette().color(QPalette::Active, QPalette::Base);
				const auto amber  = route_amber(palette());
				const auto code   = code_font_of(font());
				const auto cm     = QFontMetrics{ code };
				const auto gutter = cm.horizontalAdvance(QString::number(Shown->First + Shown->Lines.size())) + 20;
				painter.setPen(QPen{ mixed(base, ink, 0.16), 1.0 });
				painter.setBrush(base);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
				painter.setFont(code);
				for (std::size_t index = 0U; index < Shown->Lines.size(); ++index) {
					const auto number = Shown->First + static_cast<std::uint32_t>(index);
					const auto y      = 6 + static_cast<int>(index) * cm.lineSpacing();
					const auto inside = number >= Shown->StopFirst && number <= Shown->StopLast;
					if (inside) {
						auto wash = amber;
						wash.setAlphaF(0.16F);
						painter.fillRect(QRect{ 1, y, width() - 2, cm.lineSpacing() }, wash);
						painter.fillRect(QRect{ 1, y, 3, cm.lineSpacing() }, amber);
					}
					painter.setPen(inside ? ink : mixed(ink, base, 0.5));
					painter.drawText(QRect{ 8, y, gutter - 14, cm.lineSpacing() }, Qt::AlignRight | Qt::AlignVCenter, QString::number(number));
					painter.setPen(ink);
					painter.drawText(QRect{ gutter, y, width() - gutter - 8, cm.lineSpacing() }, Qt::AlignLeft | Qt::AlignVCenter, cm.elidedText(Shown->Lines[index], Qt::ElideRight, width() - gutter - 8));
				}
			}
		};

		// The list translates a drag to one semantic move. Only the accepted
		// projection may reorder its rows, so a refused drop leaves them intact.
		class tour_outline final : public QListWidget
		{
		public:
			using QListWidget::QListWidget;
			std::function<void(std::size_t)> Move;

		protected:
			void mousePressEvent(QMouseEvent* aEvent) override
			{
				DragFromHandle = aEvent->button() == Qt::LeftButton && on_handle(aEvent->position().toPoint());
				QListWidget::mousePressEvent(aEvent);
			}
			void mouseMoveEvent(QMouseEvent* aEvent) override
			{
				viewport()->setCursor(on_handle(aEvent->position().toPoint()) ? Qt::OpenHandCursor : Qt::ArrowCursor);
				QListWidget::mouseMoveEvent(aEvent);
			}
			void startDrag(Qt::DropActions) override
			{
				Dragged = currentItem() ? currentItem()->data(Qt::UserRole).toULongLong() : 0;
				if (!Dragged || !DragFromHandle || selectedItems().empty()) {
					return;
				}
				// QAbstractItemView's default drag removes source rows after a
				// successful Move. Projection already performed that semantic move.
				auto* drag = new QDrag{ this };
				drag->setMimeData(model()->mimeData(selectedIndexes()));
				drag->exec(Qt::MoveAction, Qt::MoveAction);
				drag->deleteLater();
				Dragged = 0;
			}
			void dropEvent(QDropEvent* aEvent) override
			{
				if (aEvent->source() != this || !Dragged || !currentItem() || currentItem()->data(Qt::UserRole).toULongLong() != Dragged) {
					aEvent->ignore();
					return;
				}
				const auto index       = indexAt(aEvent->position().toPoint());
				auto       destination = index.isValid() ? index.row() : count();
				if (dropIndicatorPosition() == QAbstractItemView::BelowItem) {
					++destination;
				}
				const auto selected  = selectedItems();
				const auto preceding = std::ranges::count_if(selected, [&](auto* aItem) { return row(aItem) < destination; });
				destination -= static_cast<int>(preceding);
				if (destination >= 0 && destination <= count() - selected.size() && Move) {
					Move(static_cast<std::size_t>(destination));
				}
				aEvent->setDropAction(Qt::MoveAction);
				aEvent->accept();
			}

		private:
			bool on_handle(const QPoint& aPosition) const
			{
				const auto* item = itemAt(aPosition);
				return item && aPosition.x() - visualItemRect(item).left() < std::max(18, fontMetrics().height()) + 6;
			}
			bool       DragFromHandle = false;
			qulonglong Dragged        = 0;
		};
		auto paragraph(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse);
			return label;
		}
		void reveal_focus(QWidget* aWidget)
		{
			for (auto* parent = aWidget->parentWidget(); parent; parent = parent->parentWidget()) {
				if (auto* scroll = qobject_cast<QScrollArea*>(parent)) {
					scroll->ensureWidgetVisible(aWidget, 8, 8);
				}
			}
		}
		void update_text(QPlainTextEdit* aEditor, const std::string& aText)
		{
			// QTextDocument uses paragraph separators internally. A save acknowledgement
			// must not replace an unchanged document just because its raw source uses CRLF.
			const auto text = QString::fromStdString(aText).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
			if (aEditor->toPlainText() != text) {
				const auto block = QSignalBlocker{ aEditor };
				aEditor->setPlainText(text);
			}
		}
	}

	/// \brief [[BPR-1757]]. A stop's actions on one row that keeps to
	/// the width it is given.
	///
	/// \details Its fixed parts (the writing button and More) always show,
	/// unless something else hid them. Each yielding action shows, in the
	/// order it was added, only while it is wanted and the row has room for
	/// it whole; otherwise it waits behind More, whose menu offers every one
	/// of them. Nothing on the row is squeezed below its words, and the row
	/// asks its parent for no more than its fixed parts.
	class stop_action_row final : public QWidget
	{
	public:
		explicit stop_action_row(QWidget* const aParent)
			: QWidget{ aParent }
			, Row{ new QHBoxLayout{ this } }
		{
			setObjectName(QStringLiteral("tourStopActions"));
			Row->setContentsMargins(0, 0, 0, 0);
			Row->setSpacing(2);
		}

		[[nodiscard]] auto row() const noexcept -> QHBoxLayout*
		{
			return Row;
		}

		/// \brief Makes \p aButton, already on the row, give way before every
		/// yielding action added ahead of it.
		void yield(QAbstractButton* const aButton, const bool aWanted)
		{
			Yielding.push_back({ aButton, aWanted });
			refit();
		}

		void want(const QAbstractButton* const aButton, const bool aWanted)
		{
			for (auto& entry : Yielding) {
				if (entry.Button == aButton) {
					entry.Wanted = aWanted;
				}
			}
			refit();
		}

		[[nodiscard]] auto sizeHint() const -> QSize override
		{
			auto parts = std::vector<const QWidget*>{ };
			fixed_parts(parts);
			for (const auto& entry : Yielding) {
				if (entry.Wanted) {
					parts.push_back(entry.Button);
				}
			}
			return QSize{ width_of(parts), Row->sizeHint().height() };
		}

		[[nodiscard]] auto minimumSizeHint() const -> QSize override
		{
			auto parts = std::vector<const QWidget*>{ };
			fixed_parts(parts);
			return QSize{ width_of(parts), Row->minimumSize().height() };
		}

	protected:
		void resizeEvent(QResizeEvent* const aEvent) override
		{
			QWidget::resizeEvent(aEvent);
			refit();
		}

		auto event(QEvent* const aEvent) -> bool override
		{
			// A part's words changed (Enhance text, Enhance selection).
			if (aEvent->type() == QEvent::LayoutRequest) {
				refit();
			}
			return QWidget::event(aEvent);
		}

	private:
		struct yielding
		{
			QAbstractButton* Button = nullptr;
			bool             Wanted = false;
		};

		[[nodiscard]] auto is_yielding(const QWidget* const aWidget) const -> bool
		{
			return std::ranges::any_of(Yielding, [aWidget](const yielding& aEntry) { return aEntry.Button == aWidget; });
		}

		void fixed_parts(std::vector<const QWidget*>& aParts) const
		{
			for (auto index = 0; index < Row->count(); ++index) {
				if (const auto* const part = Row->itemAt(index)->widget(); part != nullptr && !part->isHidden() && !is_yielding(part)) {
					aParts.push_back(part);
				}
			}
		}

		[[nodiscard]] auto width_of(const std::vector<const QWidget*>& aParts) const -> int
		{
			auto width = 0;
			for (const auto* const part : aParts) {
				width += part->sizeHint().width();
			}
			return width + Row->spacing() * std::max(0, static_cast<int>(aParts.size()) - 1);
		}

		void refit()
		{
			if (Fitting) {
				return;
			}
			Fitting   = true;
			auto used = std::vector<const QWidget*>{ };
			fixed_parts(used);
			auto room = true;
			for (const auto& entry : Yielding) {
				auto show = false;
				if (entry.Wanted && room) {
					used.push_back(entry.Button);
					show = width_of(used) <= width();
					if (!show) {
						used.pop_back();
						room = false;
					}
				}
				if (entry.Button->isHidden() == show) {
					entry.Button->setVisible(show);
				}
			}
			Fitting = false;
		}

		QHBoxLayout*          Row = nullptr;
		std::vector<yielding> Yielding;
		bool                  Fitting = false;
	};

	review_tour_editor::review_tour_editor(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("reviewTourEditor"));
		setAccessibleName(tr("Prepare review tour"));
		connect(qApp, &QGuiApplication::focusWindowChanged, this, [this](QWindow* aWindow) {
			if (aWindow != window()->windowHandle() && WritingCancel) {
				WritingCancel();
			}
		});
		// [[BPR-1593]]. RFC-BPR-0085 D4. One tour, two places: the route in
		// the drawer (layout A) and the tour page (layout C). Every control
		// is made once and moved between them, so both are the same tour.
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		// What needs saying before the tour: a GitHub tour to load, a save to
		// retry, a conflict to settle, what changed on the website.
		Notices           = new QWidget{ this };
		auto* const notes = new QVBoxLayout{ Notices };
		notes->setContentsMargins(10, 8, 10, 4);
		notes->setSpacing(6);
		Imports = new QComboBox{ Notices };
		Imports->setObjectName(QStringLiteral("tourImportChoices"));
		Imports->setAccessibleName(tr("GitHub tour index and list"));
		Imports->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		Import = new quiet_push_button{ tr("Load selected GitHub tour"), Notices };
		Import->setObjectName(QStringLiteral("tourImport"));
		connect(Import, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::import_selected); });
		notes->addWidget(Imports);
		notes->addWidget(Import);
		RevisionChanges = new QLabel{ Notices };
		RevisionChanges->setObjectName(QStringLiteral("tourRevisionChanges"));
		RevisionChanges->setWordWrap(true);
		notes->addWidget(RevisionChanges);
		NextChanged = new QPushButton{ tr("Next changed stop"), Notices };
		NextChanged->setObjectName(QStringLiteral("tourNextChanged"));
		notes->addWidget(NextChanged, 0, Qt::AlignLeft);
		connect(NextChanged, &QPushButton::clicked, this, [this] { next_changed_stop(); });
		Conflict = new QPlainTextEdit{ Notices };
		Conflict->setObjectName(QStringLiteral("tourConflict"));
		Conflict->setAccessibleName(tr("Saved copy from another window"));
		Conflict->setReadOnly(true);
		Conflict->hide();
		notes->addWidget(Conflict);
		auto* const conflictRow = new QHBoxLayout{ };
		Retry                   = new quiet_push_button{ tr("Retry local save"), Notices };
		connect(Retry, &QPushButton::clicked, this, [this] {if(Action){Action(tour_editor_action::retry_save);} });
		conflictRow->addWidget(Retry);
		UseSaved = new QPushButton{ tr("Discard my edits and use saved copy"), Notices };
		connect(UseSaved, &QPushButton::clicked, this, [this] {if(Action){Action(tour_editor_action::use_saved_copy);} });
		conflictRow->addWidget(UseSaved);
		KeepLocalConflict = new QPushButton{ tr("Keep my proposed version"), Notices };
		KeepLocalConflict->setObjectName(QStringLiteral("tourKeepLocalConflict"));
		connect(KeepLocalConflict, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::keep_local_conflict); });
		conflictRow->addWidget(KeepLocalConflict);
		UseRemoteConflict = new QPushButton{ tr("Use website version"), Notices };
		UseRemoteConflict->setObjectName(QStringLiteral("tourUseRemoteConflict"));
		connect(UseRemoteConflict, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::use_remote_conflict); });
		conflictRow->addWidget(UseRemoteConflict);
		conflictRow->addStretch(1);
		notes->addLayout(conflictRow);
		WritingStatus = paragraph({ }, Notices);
		WritingStatus->setObjectName(QStringLiteral("tourWritingStatus"));
		WritingStatus->setForegroundRole(QPalette::PlaceholderText);
		notes->addWidget(WritingStatus);
		layout->addWidget(Notices);

		// The section stack: the brief's fields, the stop, the local preview.
		// Its tabs are never shown (RFC-BPR-0085 principle 1): the route and
		// the outline choose its page.
		Roadmap = new QTabWidget{ this };
		Roadmap->setObjectName(QStringLiteral("tourRoadmap"));
		Roadmap->setAccessibleName(tr("Review tour sections"));
		Roadmap->tabBar()->hide();
		Roadmap->setDocumentMode(true);

		// [[BPR-1784]]. The tour's three steps, as the agent setup draws its
		// own: 1 Problem, 2 Solution, 3 Stops, each a header that opens its
		// step in place. Problem and Solution hold their fields; Stops holds
		// the route.
		auto* const frame    = new tour_steps_frame{ this };
		Brief                = frame;
		auto* const briefCol = new QVBoxLayout{ Brief };
		briefCol->setContentsMargins(0, 0, 0, 0);
		briefCol->setSpacing(4);
		const auto block = [&](const int aIndex, const QString& aTitle, const QString& aName) {
			auto* const step = new QWidget{ Brief };
			step->setObjectName(QStringLiteral("tourStep%1").arg(aIndex + 1));
			auto* const column = new QVBoxLayout{ step };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(4);
			auto* const row = new tour_step_row{ aIndex + 1, aTitle, step };
			row->setObjectName(aName + QStringLiteral("Row"));
			row->Change->setObjectName(aName + QStringLiteral("Change"));
			column->addWidget(row);
			frame->Rows[static_cast<std::size_t>(aIndex)] = row;
			StepRows[aIndex]                              = row;
			const auto open                               = [this, aIndex] {
				choose_step(aIndex == 2 ? -1 : aIndex);
			};
			connect(row, &QAbstractButton::clicked, this, open);
			connect(row->Change, &QPushButton::clicked, this, open);
			auto* const slot = new QWidget{ step };
			auto* const body = new QVBoxLayout{ slot };
			// The route keeps the drawer's width: its grip, not an indent, sets it in.
			body->setContentsMargins(aIndex == 2 ? 8 : tour_step_row::indent, 0, 0, 8);
			body->setSpacing(6);
			column->addWidget(slot);
			slot->hide();
			briefCol->addWidget(step);
			return slot;
		};
		const auto field = [&](const int aIndex, const QString& aTitle, const QString& aPrompt, const QString& aHelp, const QString& aName, core::tour_command_kind aKind, tour_editor_action aMapping) {
			BriefSlots[aIndex] = block(aIndex, aTitle, aName);
			auto* const slot   = static_cast<QVBoxLayout*>(BriefSlots[aIndex]->layout());
			auto* const host   = new QWidget{ Roadmap };
			auto* const hold   = new QVBoxLayout{ host };
			hold->setContentsMargins(0, 0, 0, 0);
			BriefHosts[aIndex] = host;
			Roadmap->addTab(host, aTitle);
			auto* const panel  = new QWidget{ BriefSlots[aIndex] };
			auto* const column = new QVBoxLayout{ panel };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(6);
			BriefPanels[aIndex] = panel;
			slot->addWidget(panel);
			auto* editor = new scoped_prose_editor{ panel };
			editor->setObjectName(aName);
			editor->setAccessibleName(aTitle + tr(" Markdown source"));
			editor->setAccessibleDescription(aHelp);
			editor->setPlaceholderText(aPrompt);
			editor->setTabChangesFocus(true);
			editor->setMinimumHeight(110);
			column->addWidget(editor, 1);
			// [[BPR-1784]]. Next shares the assistance row, so the step fits the
			// narrowest drawer.
			auto* const first = new QHBoxLayout{ };
			first->setSpacing(6);
			add_writing_button(editor, aKind == core::tour_command_kind::problem ? core::writing_hook::problem : core::writing_hook::solution, first);
			first->addStretch(1);
			column->addLayout(first);
			auto* const tools = new QHBoxLayout{ };
			tools->setSpacing(6);
			auto* map = new quiet_push_button{ tr("Use selected description text"), panel };
			map->setObjectName(aName + QStringLiteral("Map"));
			map->setToolTip(tr("Select text in the pull request description's Source view, then use it in this field."));
			tools->addWidget(map);
			tools->addStretch(1);
			column->addLayout(tools);
			// Moving on is explicit: Next, or the key that marks a stop Ready
			// and goes on.
			auto* const next = new QPushButton{ tr("Next"), panel };
			next->setObjectName(aName + QStringLiteral("Next"));
			next->setShortcut(shortcut_key(shortcut_command::tour_ready_next));
			next->setToolTip((aIndex == 0 ? tr("Go on to the solution (%1)") : tr("Go on to the stops (%1)")).arg(next->shortcut().toString(QKeySequence::NativeText)));
			first->addWidget(next);
			connect(map, &QPushButton::clicked, this, [this, aMapping] { trigger(aMapping); });
			connect(next, &QPushButton::clicked, this, [this, aIndex] { choose_step(aIndex == 0 ? 1 : -1); });
			connect(editor, &QPlainTextEdit::textChanged, this, [this, editor, aKind] { edit(aKind, editor->raw()); });
			return editor;
		};
		Problem   = field(0, tr("Problem"), tr("What problem does this change solve?"), tr("Describe what was wrong, missing, or unnecessarily difficult before this change."), QStringLiteral("tourProblem"), core::tour_command_kind::problem, tour_editor_action::map_problem);
		Solution  = field(1, tr("Solution"), tr("How does this change solve the problem?"), tr("Explain the central approach and any important limits. Save detailed implementation decisions for the tour."), QStringLiteral("tourSolution"), core::tour_command_kind::solution, tour_editor_action::map_solution);
		StopsSlot = block(2, tr("Stops"), QStringLiteral("tourStops"));
		build_stops();
		Preview = new markdown_body_view{ Roadmap };
		Preview->setObjectName(QStringLiteral("tourPreview"));
		auto* previewPage   = new QWidget{ Roadmap };
		auto* previewLayout = new QVBoxLayout{ previewPage };
		previewLayout->setContentsMargins(0, 0, 0, 0);
		previewLayout->addWidget(paragraph(tr("Local preview. Nothing is sent. GitHub may draw HTML and disclosures differently; the Markdown is the source."), previewPage));
		previewLayout->addWidget(Preview, 1);
		Roadmap->addTab(previewPage, tr("Preview"));
		connect(Roadmap, &QTabWidget::currentChanged, this, [this](const int aIndex) {
			if (WritingCancel) {
				WritingCancel();
			}
			// [[BPR-1593]]. Choosing a section opens it where this mode shows
			// it: the brief's field in place on the route.
			const auto chosen = aIndex < 2 ? aIndex : -1;
			if (BriefOpen != chosen && (aIndex < 3 || Mode == tour_editor_mode::page)) {
				open_brief(chosen);
			}
			refresh_preview();
		});

		// On GitHub: what publishing sends, and the controls that send it.
		PublicationPanel = new QWidget{ this };
		PublicationPanel->setObjectName(QStringLiteral("tourPublicationPanel"));
		auto* const publication = new QVBoxLayout{ PublicationPanel };
		publication->setContentsMargins(0, 0, 0, 0);
		publication->setSpacing(8);
		auto* const onGithub = new QLabel{ tr("On GitHub"), PublicationPanel };
		auto        heading  = onGithub->font();
		heading.setWeight(QFont::DemiBold);
		onGithub->setFont(heading);
		publication->addWidget(onGithub);
		PublicationPreview = new markdown_body_view{ PublicationPanel };
		PublicationPreview->setObjectName(QStringLiteral("tourPublicationPreview"));
		publication->addWidget(PublicationPreview, 1);
		// [[BPR-1782]]. A publication under way, or one that did not finish, is
		// said in Compose review's waiting panel: the mark, a plain title, what
		// is being sent, and what is safe. Resume and Check status stay below.
		PublicationWorking = new waiting_frame{ PublicationPanel };
		PublicationWorking->setObjectName(QStringLiteral("tourPublicationWorking"));
		{
			auto* const row = new QHBoxLayout{ PublicationWorking };
			row->setContentsMargins(15, 12, 14, 13);
			row->setSpacing(10);
			WorkingMark = new QLabel{ PublicationWorking };
			WorkingMark->setObjectName(QStringLiteral("tourPublicationWorkingMark"));
			WorkingMark->setFixedSize(18, 18);
			WorkingMark->setAccessibleName(tr("Status unknown"));
			row->addWidget(WorkingMark, 0, Qt::AlignTop);
			auto* const text = new QVBoxLayout;
			text->setContentsMargins(0, 0, 0, 0);
			text->setSpacing(4);
			const auto words = [this](const QString& aName) {
				auto* const label = new QLabel{ PublicationWorking };
				label->setObjectName(aName);
				label->setTextFormat(Qt::PlainText);
				label->setWordWrap(true);
				label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
				label->setMinimumWidth(0);
				label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
				return label;
			};
			WorkingTitle = words(QStringLiteral("tourPublicationWorkingTitle"));
			WorkingTitle->setTextInteractionFlags(Qt::NoTextInteraction);
			auto titleFont = WorkingTitle->font();
			titleFont.setWeight(QFont::DemiBold);
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.08);
			WorkingTitle->setFont(titleFont);
			text->addWidget(WorkingTitle);
			WorkingStages = words(QStringLiteral("tourPublicationWorkingStages"));
			text->addWidget(WorkingStages);
			WorkingStatus = words(QStringLiteral("tourPublicationWorkingStatus"));
			text->addWidget(WorkingStatus);
			row->addLayout(text, 1);
		}
		PublicationWorking->hide();
		publication->addWidget(PublicationWorking);
		PublicationStatus = paragraph({ }, PublicationPanel);
		PublicationStatus->setObjectName(QStringLiteral("tourPublicationStatus"));
		PublicationStatus->setForegroundRole(QPalette::PlaceholderText);
		publication->addWidget(PublicationStatus);
		PublicationLink = paragraph({ }, PublicationPanel);
		PublicationLink->setObjectName(QStringLiteral("tourPublicationLink"));
		PublicationLink->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		PublicationLink->setAccessibleName(tr("Where GitHub shows the tour"));
		PublicationLink->hide();
		publication->addWidget(PublicationLink);
		restyle_working();
		PublicationMode = new QComboBox{ PublicationPanel };
		PublicationMode->setObjectName(QStringLiteral("tourPublicationMode"));
		PublicationMode->addItems({ tr("Keep single-review publication"), tr("Publish with compatibility comments (non-atomic)") });
		publication->addWidget(PublicationMode);
		DescriptionEdit = new QCheckBox{ tr("Update PR description after publishing"), PublicationPanel };
		DescriptionEdit->setObjectName(QStringLiteral("tourDescriptionEdit"));
		// [[BPR-1787]]. The brief is published in one place, the description
		// by default; the tooltip says where it goes either way.
		DescriptionEdit->setChecked(true);
		DescriptionEdit->setToolTip(tr("On: Problem and Solution go in the PR description, under the tour link at its top, and the tour comment lists only the route. Matching Problem and Solution headings are updated in place; other description text is kept. Off: Problem and Solution go in the tour comment and the description is not edited. A tour already on GitHub keeps them where they were first published. Compatibility comments become visible one at a time; interrupted publication may remain partial."));
		publication->addWidget(DescriptionEdit);
		auto* const sendRow = new QHBoxLayout{ };
		sendRow->setSpacing(6);
		const auto button = [&](const QString& aLabel, const QString& aName, tour_editor_action aAction) {
			QPushButton* value = aAction == tour_editor_action::preview_publication
				? static_cast<QPushButton*>(new quiet_push_button{ aLabel, PublicationPanel })
				: new QPushButton{ aLabel, PublicationPanel };
			value->setObjectName(aName);
			connect(value, &QPushButton::clicked, this, [this, aAction] { trigger(aAction); });
			sendRow->addWidget(value);
			return value;
		};
		PreparePublication = button(tr("Preview"), QStringLiteral("tourPreparePublication"), tour_editor_action::preview_publication);
		PublishTour        = button(tr("Publish tour"), QStringLiteral("tourPublish"), tour_editor_action::publish_publication);
		PublishTour->setDefault(true);
		sendRow->addStretch(1);
		publication->addLayout(sendRow);
		auto* const recoverRow = new QHBoxLayout{ };
		recoverRow->setSpacing(6);
		const auto recover = [&](const QString& aLabel, const QString& aName, tour_editor_action aAction) {
			QPushButton* value = aAction == tour_editor_action::resume_publication ? new QPushButton{ aLabel, PublicationPanel } : static_cast<QPushButton*>(new quiet_push_button{ aLabel, PublicationPanel });
			value->setObjectName(aName);
			value->setFlat(true);
			connect(value, &QPushButton::clicked, this, [this, aAction] { trigger(aAction); });
			recoverRow->addWidget(value);
			return value;
		};
		ResumeTour    = recover(tr("Resume"), QStringLiteral("tourResume"), tour_editor_action::resume_publication);
		ReconcileTour = recover(tr("Check status"), QStringLiteral("tourReconcile"), tour_editor_action::reconcile_publication);
		StopTour      = recover(tr("Stop"), QStringLiteral("tourStop"), tour_editor_action::stop_publication);
		ResumeTour->setToolTip(tr("Resume captured publication"));
		ReconcileTour->setToolTip(tr("Check publication status"));
		StopTour->setToolTip(tr("Stop publication"));
		recoverRow->addStretch(1);
		publication->addLayout(recoverRow);
		connect(PublicationMode, &QComboBox::currentIndexChanged, this, [this] { PublishTour->setEnabled(false); });
		connect(DescriptionEdit, &QCheckBox::toggled, this, [this] { PublishTour->setEnabled(false); });
		present_publication({ });

		// The foot: the private draft's state, and Preview and publish.
		Foot = new QWidget{ this };
		Foot->setObjectName(QStringLiteral("tourFoot"));
		auto* const footRow = new QHBoxLayout{ Foot };
		footRow->setContentsMargins(10, 6, 10, 8);
		footRow->setSpacing(6);
		Status = new QLabel{ tr("Open a pull request with an accepted account to prepare a private tour."), Foot };
		Status->setObjectName(QStringLiteral("tourSaveStatus"));
		Status->setWordWrap(true);
		Status->setTextFormat(Qt::PlainText);
		Status->setForegroundRole(QPalette::PlaceholderText);
		Status->setAccessibleName(tr("Tour save status"));
		footRow->addWidget(Status, 1);
		FootState      = Status;
		PreviewPublish = new QPushButton{ tr("Preview and publish"), Foot };
		PreviewPublish->setObjectName(QStringLiteral("tourPreviewAndPublish"));
		PreviewPublish->setToolTip(tr("Open the tour page with what GitHub will show. Nothing is sent until Publish."));
		connect(PreviewPublish, &QPushButton::clicked, this, [this] {
			if (ExpandDispatch) {
				ExpandDispatch(true);
			}
		});
		footRow->addWidget(PreviewPublish, 0, Qt::AlignTop);

		// The route: the brief, then the stops, in the drawer.
		RouteColumn = new QWidget{ this };
		RouteColumn->setObjectName(QStringLiteral("tourRoute"));
		auto* const routeLayout = new QVBoxLayout{ RouteColumn };
		routeLayout->setContentsMargins(10, 8, 10, 6);
		routeLayout->setSpacing(10);
		layout->addWidget(RouteColumn, 1);

		// The page: the outline, the section, and what GitHub will show.
		PageColumns = new QWidget{ this };
		PageColumns->setObjectName(QStringLiteral("tourPage"));
		auto* const pageRow = new QHBoxLayout{ PageColumns };
		pageRow->setContentsMargins(0, 0, 0, 0);
		pageRow->setSpacing(0);
		PageLeft = new QWidget{ PageColumns };
		PageLeft->setObjectName(QStringLiteral("tourPageOutline"));
		PageLeft->setFixedWidth(290);
		auto* const leftColumn = new QVBoxLayout{ PageLeft };
		leftColumn->setContentsMargins(14, 14, 10, 14);
		leftColumn->setSpacing(10);
		auto* const pageTitle = new QLabel{ tr("Review tour"), PageLeft };
		pageTitle->setObjectName(QStringLiteral("tourPageTitle"));
		auto titleFont = pageTitle->font();
		titleFont.setWeight(QFont::DemiBold);
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
		pageTitle->setFont(titleFont);
		leftColumn->addWidget(pageTitle);
		pageRow->addWidget(PageLeft);
		auto* const centre       = new QWidget{ PageColumns };
		auto* const centreColumn = new QVBoxLayout{ centre };
		centreColumn->setContentsMargins(18, 14, 18, 14);
		centreColumn->addWidget(Roadmap, 1);
		pageRow->addWidget(centre, 1);
		auto* const right = new QWidget{ PageColumns };
		right->setObjectName(QStringLiteral("tourPageGithub"));
		right->setFixedWidth(360);
		PageRight               = right;
		auto* const rightColumn = new QVBoxLayout{ right };
		rightColumn->setContentsMargins(14, 14, 14, 14);
		rightColumn->addWidget(PublicationPanel, 1);
		pageRow->addWidget(right);
		layout->addWidget(PageColumns, 1);
		layout->addWidget(Foot);

		for (auto* widget : findChildren<QWidget*>()) {
			widget->installEventFilter(this);
			widget->setAttribute(Qt::WA_MacShowFocusRect);
			if (widget->focusPolicy() & Qt::TabFocus) {
				// Inside the control's bounds, including scroll viewports: some
				// native styles omit button focus when full keyboard access is off.
				new keyboard_focus_ring{ *widget, QStringLiteral("tourKeyboardFocus") };
			}
		}
		Mode = tour_editor_mode::page;
		set_mode(tour_editor_mode::route);
		present({ });
	}
	auto review_tour_editor::compatibility_publication() const -> bool { return PublicationMode->currentIndex() == 1; }
	auto review_tour_editor::description_publication() const -> bool { return DescriptionEdit->isChecked(); }
	void review_tour_editor::present_publication(const application::tour_publication_presentation& aProjection)
	{
		using phase       = application::tour_publication_phase;
		const auto status = QString::fromStdString(aProjection.Status);
		PublicationPreview->set_body(aProjection.Markdown, "tour-publication");
		// [[BPR-1593]]. What a prepared publication sends; until then the
		// tour's own preview shows what GitHub will show.
		PublicationPreview->setVisible(!aProjection.Markdown.empty());
		// [[BPR-1782]]. As Compose review waits for GitHub: the panel says it,
		// the status line steps aside, and nothing that would change what is
		// being sent can be used meanwhile. Stop stays.
		const auto publishing = aProjection.Phase == phase::publishing;
		const auto attention  = aProjection.Phase == phase::attention;
		WorkingTitle->setText(publishing ? tr("Publishing the tour to GitHub\u2026") : tr("Publishing didn't finish"));
		auto stages = QStringList{ };
		for (const auto& stage : aProjection.Stages) {
			stages.push_back(QString::fromStdString(stage));
		}
		WorkingStages->setText(stages.isEmpty() ? QString{ } : tr("Sending, in order:") + QStringLiteral("\n") + stages.join(QStringLiteral("\n")));
		WorkingStages->setVisible(publishing && !stages.isEmpty());
		WorkingStatus->setText(status);
		PublicationWorking->setVisible(publishing || attention);
		PublicationWorking->setAccessibleName(WorkingTitle->text());
		PublicationWorking->setAccessibleDescription(WorkingStatus->text());
		PublicationStatus->setText(status);
		PublicationStatus->setVisible(!publishing && !attention && !status.isEmpty());
		const auto link = QString::fromStdString(aProjection.Link);
		PublicationLink->setText(link.isEmpty() ? QString{ } : tr("On GitHub: %1").arg(link));
		PublicationLink->setVisible(aProjection.Phase == phase::published && !link.isEmpty());
		PublishTour->setText(publishing ? tr("Publishing\u2026") : aProjection.Phase == phase::published ? tr("Published")
																										 : tr("Publish tour"));
		PublishTour->setEnabled(aProjection.Publish && !publishing);
		PreparePublication->setEnabled(!publishing);
		PublicationMode->setEnabled(!publishing);
		DescriptionEdit->setEnabled(!publishing);
		ResumeTour->setEnabled(aProjection.Resume);
		ReconcileTour->setEnabled(aProjection.Reconcile);
		StopTour->setEnabled(aProjection.Stop);
	}
	void review_tour_editor::restyle_working()
	{
		const auto attention = waiting_attention_colour(palette());
		static_cast<waiting_frame*>(PublicationWorking)->set_attention(attention);
		WorkingMark->setPixmap(waiting_mark(attention, devicePixelRatioF()));
		auto quiet = WorkingStatus->palette();
		quiet.setColor(QPalette::WindowText, waiting_quiet_text(palette(), attention));
		WorkingStatus->setPalette(quiet);
	}

	void review_tour_editor::set_dispatch(std::function<void(core::tour_command)> aDispatch, std::function<void(tour_editor_action)> aAction)
	{
		Dispatch = std::move(aDispatch);
		Action   = std::move(aAction);
	}
	auto review_tour_editor::import_choice() const -> std::size_t { return static_cast<std::size_t>(Imports->currentIndex()); }
	void review_tour_editor::edit(core::tour_command_kind aKind, const std::string& aText)
	{
		if (Applying || !Shown.Draft || !Dispatch) {
			return;
		}
		EditingKind = aKind;
		Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = aKind, .Text = aText });
	}
	void review_tour_editor::set_presented_observer(std::function<void()> aObserver)
	{
		PresentedObserver = std::move(aObserver);
	}

	void review_tour_editor::go_to_stop(const core::tour_stop_id aStop)
	{
		show_stops();
		if (!Shown.Draft) {
			return;
		}
		const auto found = std::ranges::find(Shown.Draft->Stops, aStop, &core::tour_stop::Id);
		if (found == Shown.Draft->Stops.end()) {
			return;
		}
		const auto row = static_cast<int>(found - Shown.Draft->Stops.begin());
		if (Outline->currentRow() != row) {
			Outline->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
			return;
		}
		Selected = aStop;
		place_stop_card();
		trigger(tour_editor_action::navigate_stop);
	}

	void review_tour_editor::present(const application::tour_presentation& aProjection)
	{
		// [[BPR-1781]]. Whatever path the presentation leaves by, a summary
		// elsewhere hears of it.
		struct presented
		{
			const std::function<void()>& Observer;
			~presented()
			{
				if (Observer) {
					Observer();
				}
			}
		} const notify{ PresentedObserver };
		Applying         = true;
		SelectionChanged = false;
		const bool same  = Shown.Draft && aProjection.Draft && Shown.Draft->Owner == aProjection.Draft->Owner && Shown.Draft->Id == aProjection.Draft->Id;
		if (!same) {
			Selected = { };
		}
		if (aProjection.SelectionRevision != Shown.SelectionRevision || aProjection.Selected != Shown.Selected) {
			Selected         = aProjection.Selected;
			SelectionChanged = true;
		}
		if (Shown.ImportChoices != aProjection.ImportChoices) {
			const auto block = QSignalBlocker{ Imports };
			Imports->clear();
			for (const auto& choice : aProjection.ImportChoices) {
				Imports->addItem(QString::fromStdString(choice));
			}
			// Multiple plausible roots require an explicit choice, even when a link is preferred.
			Imports->setCurrentIndex(Imports->count() == 1 ? 0 : -1);
		}
		Imports->setVisible(!aProjection.ImportChoices.empty());
		Import->setVisible(!aProjection.ImportChoices.empty());
		Shown               = aProjection;
		const bool editable = Shown.Draft && !Shown.Loading && !Shown.Draft->ReadOnly;
		const bool bounded  = same && Shown.Refusal == core::tour_refusal::bounds;
		Problem->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::problem));
		Solution->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::solution));
		Title->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::edit_stop));
		Body->setReadOnly(!editable || (bounded && EditingKind != core::tour_command_kind::edit_stop));
		if (Shown.Draft && (Shown.Refusal == core::tour_refusal::none || !same)) {
			Problem->present(Shown.Draft->Problem, !same);
			Solution->present(Shown.Draft->Solution, !same);
		} else if (!Shown.Draft) {
			Problem->present({ }, !same);
			Solution->present({ }, !same);
		}
		auto status = tr("Open a pull request with an accepted account to prepare a private tour.");
		if (Shown.Loading) {
			status = tr("Loading private tour…");
		} else if (Shown.Refusal != core::tour_refusal::none) {
			status = Shown.Refusal == core::tour_refusal::bounds ? tr("This edit exceeds the local tour limit. Your editor text is retained but has not been saved.") : Shown.Refusal == core::tour_refusal::incomplete ? tr("Add a title, explanation and available source before marking this stop Ready. Incomplete text is saved locally.")
																																																						: tr("This edit was refused because its draft or source changed. Your text is retained.");
		} else if (Shown.Conflict) {
			status = tr("Another window saved this draft. Your edits are retained above; the saved copy is below. Choose which copy to keep.");
		} else if (Shown.Failure) {
			status = tr("Local save unavailable. Your edits remain in this window and are not yet durable. Retry the save.");
		} else if (Shown.Draft) {
			status = Shown.Saving ? tr("Saving locally…") : Shown.SavedRevision == Shown.Draft->Revision ? tr("Saved locally. Nothing published.")
																										 : tr("Private draft. Nothing published.");
		}
		if (Shown.Draft && !Shown.Draft->ImportNotice.empty()) {
			status += " " + QString::fromStdString(Shown.Draft->ImportNotice);
		}
		if (!Shown.ImportNotice.empty()) {
			status += " " + QString::fromStdString(Shown.ImportNotice);
		}
		Status->setText(status);
		QString changes = tr("No prior local tour version.");
		if (Shown.Draft && Shown.Draft->HasPriorObservation) {
			changes = tr("Changes at the last differing local website observation:");
			if (Shown.Draft->ObservedChanges.empty()) {
				changes = tr("No website differences recorded locally.");
			}
			for (const auto& change : Shown.Draft->ObservedChanges) {
				const auto kind = change.Kind == core::tour_change_kind::added ? tr("Added") : change.Kind == core::tour_change_kind::removed ? tr("Removed")
					: change.Kind == core::tour_change_kind::reordered                                                                        ? tr("Reordered")
					: change.Kind == core::tour_change_kind::source                                                                           ? tr("Source changed")
																																			  : tr("Text changed");
				changes += "\n" + kind + ": " + QString::fromStdString(change.Title);
			}
		}
		RevisionChanges->setText(changes);
		NextChanged->setEnabled(Shown.Draft && !Shown.Draft->ObservedChanges.empty());
		// [[BPR-1593]]. Said only when the website's tour differs.
		RevisionChanges->setVisible(Shown.Draft && Shown.Draft->HasPriorObservation);
		NextChanged->setVisible(Shown.Draft && !Shown.Draft->ObservedChanges.empty());

		Retry->setVisible(Shown.Failure.has_value() && !Shown.Conflict);
		UseSaved->setVisible(static_cast<bool>(Shown.Conflict));
		const bool remoteConflict = Shown.Draft && !Shown.Draft->Conflicts.empty();
		KeepLocalConflict->setVisible(remoteConflict && !Shown.Conflict);
		UseRemoteConflict->setVisible(remoteConflict && !Shown.Conflict);
		Conflict->setVisible(static_cast<bool>(Shown.Conflict) || remoteConflict);
		if (remoteConflict && !Shown.Conflict) {
			const auto& conflict = Shown.Draft->Conflicts.front();
			update_text(Conflict, "Website conflict — saved versions\n\nBASELINE\n" + conflict.Baseline + "\n\nMY PROPOSAL\n" + conflict.Local + "\n\nWEBSITE\n" + conflict.Remote);
			Conflict->setAccessibleName(tr("Baseline, my proposal and website versions"));
		}
		if (Shown.Conflict) {
			Conflict->setAccessibleName(tr("Saved copy from another window"));
			auto text = "## Problem\n" + Shown.Conflict->Problem + "\n\n## Solution\n" + Shown.Conflict->Solution;
			for (const auto& stop : Shown.Conflict->Stops) {
				text += "\n\n### " + stop.Title + "\n" + stop.Body;
			}
			update_text(Conflict, text);
		}
		present_stops(!same);
		// [[BPR-1784]]. Another draft opens on its first step not yet
		// written; after that, only the author moves between steps.
		if (!same) {
			const auto first = core::decide_tour_steps(Shown.Draft.get(), std::nullopt).Current;
			open_brief(first == core::tour_step::problem ? 0 : first == core::tour_step::solution ? 1
																								  : -1);
		}
		refresh_steps();
		refresh_preview();
		place_stop_card();
		refresh_excerpt();
		refresh_foot();
		Applying = false;
	}
	void review_tour_editor::refresh_preview()
	{
		if ((Roadmap->currentIndex() != Roadmap->count() - 1 && Mode != tour_editor_mode::page) || !Shown.Draft) {
			return;
		}
		auto body = "## Problem\n\n" + Shown.Draft->Problem + "\n\n## Solution\n\n" + Shown.Draft->Solution;
		body += "\n\n## Tour stops\n";
		for (std::size_t i = 0; i < Shown.Draft->Stops.size(); ++i) {
			const auto& stop = Shown.Draft->Stops[i];
			body += "\n### " + std::to_string(i + 1) + " — " + stop.Title + "\n\n" + stop.Body + "\n\n";
			if (stop.Original) {
				body += "Source: " + stop.Original->Path + " · " + (stop.Original->First.Side == core::diff_side::left ? "base" : "head") + " lines " + std::to_string(stop.Original->First.Line + 1U) + "–" + std::to_string(stop.Original->Last.Line + 1U) + "\n\n";
			} else if (!stop.Destination) {
				body += "Source unavailable.\n\n";
			}
			if (stop.Destination && !stop.Original) {
				const auto& source = *stop.Destination;
				body += "Pinned source: " + source.Path + " · " + std::string{ core::canonical_hex(source.Commit).view() } + " · lines " + std::to_string(source.FirstLine + 1U) + "–" + std::to_string(source.LastLine + 1U) + "\n\n";
			}
			const auto plan = std::ranges::find(Shown.Destinations, stop.Id, &application::tour_stop_destination::Stop);
			if (plan != Shown.Destinations.end()) {
				if (!plan->SourceLink.empty()) {
					body += "[Original source](" + plan->SourceLink + ")\n\n";
				}
				if (plan->Plan.IncompatibleBody) {
					body += "Publication requires a native line subject for this suggestion.\n\n";
				} else if (plan->Plan.RequiresCompatibility) {
					body += "This destination requires an explicit compatibility publication plan. It cannot join one pending review.\n\n";
				}
				if (plan->Plan.NeedsQualification) {
					body += "Publication support for this exact source and operation is not yet qualified.\n\n";
				}
			}
			for (const auto& related : stop.Related) {
				body += "Related: " + related.Path + " · lines " + std::to_string(related.First.Line + 1U) + "–" + std::to_string(related.Last.Line + 1U) + "\n\n";
			}
		}
		Preview->set_body(std::move(body), std::to_string(Shown.Draft->Revision));
	}
	void review_tour_editor::set_writing_dispatch(std::function<void(core::writing_hook, prose_capture)> aRequest, std::function<void()> aCancel)
	{
		WritingRequest = std::move(aRequest);
		WritingCancel  = std::move(aCancel);
	}
	auto review_tour_editor::writing_editor(core::writing_hook aHook) const -> scoped_prose_editor* { return aHook == core::writing_hook::problem ? Problem : aHook == core::writing_hook::solution ? Solution
																										  : aHook == core::writing_hook::comment                                                    ? Body
																																																	: nullptr; }
	auto review_tour_editor::writing_visible(core::writing_hook aHook) const -> bool
	{
		if (!isVisible() || !window()->isActiveWindow()) {
			return false;
		}
		// [[BPR-1593]]. A field is offered where it shows: open in the route's
		// brief or its stop, or the tour page's section.
		switch (aHook) {
		case core::writing_hook::problem:  return Problem->isVisible();
		case core::writing_hook::solution: return Solution->isVisible();
		case core::writing_hook::comment:  return selected_stop() && Body->isVisible() && !Body->viewport()->visibleRegion().isEmpty();
		case core::writing_hook::outline:  return Outline->isVisible();
		case core::writing_hook::feedback: return false; // [[BPR-1090]] No tour field takes the feedback hook.
		}
		return false;
	}

	void review_tour_editor::add_writing_button(scoped_prose_editor* aEditor, core::writing_hook aHook, QBoxLayout* aLayout)
	{
		auto* button = new quiet_push_button{ aEditor->parentWidget() };
		button->setObjectName(aHook == core::writing_hook::problem ? QStringLiteral("tourProblemMagic") : aHook == core::writing_hook::solution ? QStringLiteral("tourSolutionMagic")
																																				: QStringLiteral("tourStopMagic"));
		button->setFocusPolicy(Qt::TabFocus);
		// [[BPR-1776]]. Raised, as the shared chip: flat it read as a label.
		button->setToolTip(tr("Use the selected agent once. The result replaces only the captured text and can be undone."));
		aLayout->addWidget(button, 0, Qt::AlignLeft);
		const auto label = [aEditor, aHook, button] { button->setText(aEditor->textCursor().hasSelection() ? tr("Enhance selection") : core::writing_blank(aEditor->raw()) ? (aHook == core::writing_hook::comment ? tr("Generate comment") : tr("Recover explanation"))
																																										   : tr("Enhance text")); };
		connect(aEditor, &QPlainTextEdit::selectionChanged, this, label);
		connect(aEditor, &QPlainTextEdit::textChanged, this, label);
		connect(button, &QPushButton::clicked, this, [this, aEditor, aHook] {
			if (!aEditor->isReadOnly() && WritingRequest) {
				if (auto captured = aEditor->capture()) {
					WritingRequest(aHook, std::move(*captured));
				}
			}
		});
		aEditor->Invalidated = [this, label] {
			label();
			if (WritingCancel) {
				WritingCancel();
			}
		};
		label();
	}
	void review_tour_editor::present_outline(const std::vector<core::tour_outline_entry>& aEntries)
	{
		SuggestedOutline->clear();
		for (const auto& entry : aEntries) {
			auto* item = new QListWidgetItem{ QString::fromStdString(entry.Title + "\n" + entry.Suggestion.Reason + (entry.Suggestion.Question.empty() ? "" : "\n" + entry.Suggestion.Question)), SuggestedOutline };
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(Qt::Checked);
		}
		SuggestedOutline->setVisible(!aEntries.empty());
		InsertOutline->setVisible(!aEntries.empty());
	}
	auto review_tour_editor::retained_outline_entries() const -> std::vector<std::size_t>
	{
		std::vector<std::size_t> retained;
		for (int row = 0; row < SuggestedOutline->count(); ++row) {
			if (SuggestedOutline->item(row)->checkState() == Qt::Checked) {
				retained.push_back(static_cast<std::size_t>(row));
			}
		}
		return retained;
	}
	void review_tour_editor::show_problem()
	{
		open_brief(0);
		Problem->setFocus();
	}
	void review_tour_editor::hideEvent(QHideEvent* aEvent)
	{
		if (WritingCancel) {
			WritingCancel();
		}
		QWidget::hideEvent(aEvent);
	}
	bool review_tour_editor::focusNextPrevChild(bool aNext)
	{
		// Cocoa's text-only keyboard preference can otherwise skip every control
		// on a prose page. This form exposes its full visible keyboard route.
		auto* current = QApplication::focusWidget();
		if (!current) {
			return QWidget::focusNextPrevChild(aNext);
		}
		for (auto* next = aNext ? current->nextInFocusChain() : current->previousInFocusChain(); next != current; next = aNext ? next->nextInFocusChain() : next->previousInFocusChain()) {
			if (next->window() == window() && next->isVisible() && next->isEnabled() && (next->focusPolicy() & Qt::TabFocus) && !next->focusProxy()) {
				next->setFocus(aNext ? Qt::TabFocusReason : Qt::BacktabFocusReason);
				if (QApplication::focusWidget() != current) {
					reveal_focus(next);
					return true;
				}
			}
		}
		return false;
	}
	bool review_tour_editor::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		if (aEvent->type() == QEvent::InputMethod) {
			// Preedit text is not committed text: another presentation must wait for it, not flush it.
			if (static_cast<QInputMethodEvent*>(aEvent)->preeditString().isEmpty()) {
				Composing.erase(aWatched);
			} else {
				Composing.insert(aWatched);
			}
		} else if (aEvent->type() == QEvent::FocusOut || aEvent->type() == QEvent::Hide) {
			Composing.erase(aWatched);
		}
		if (aEvent->type() == QEvent::ShortcutOverride || aEvent->type() == QEvent::KeyPress) {
			auto* key = static_cast<QKeyEvent*>(aEvent);
			// [[BPR-1805]]. As a comment's editor takes them: Return in the
			// read stop writes it; Escape or Command/Control+Return reads it again.
			const bool enter    = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
			const bool reading  = StopReader != nullptr && aWatched == StopReader->rendered_view() && enter && key->modifiers() == Qt::NoModifier;
			const bool finished = (aWatched == Title || aWatched == Body) && !composing() && ((key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier) || (enter && (key->modifiers() & ~Qt::KeypadModifier) == Qt::ControlModifier));
			if (reading || finished) {
				key->accept();
				if (aEvent->type() == QEvent::KeyPress) {
					reading ? begin_stop_edit() : finish_stop_edit();
				}
				return true;
			}
			if ((key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) && !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
				key->accept();
				if (aEvent->type() == QEvent::KeyPress) {
					focusNextPrevChild(key->key() == Qt::Key_Tab && !(key->modifiers() & Qt::ShiftModifier));
				}
				return true;
			}
			auto* action = key->matches(QKeySequence::Undo) ? UndoStructure : key->matches(QKeySequence::Redo) ? RedoStructure
																											   : nullptr;
			if (action && action->isEnabled() && Shown.Refusal != core::tour_refusal::bounds) {
				// Structural history is newer than native text history: the host
				// clears it on the next text edit. Otherwise native editing owns Undo.
				key->accept();
				if (aEvent->type() == QEvent::KeyPress) {
					action->trigger();
				}
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
	void review_tour_editor::show_writing_notice(const QString& aText) { WritingStatus->setText(aText); }
	void review_tour_editor::show_notice(const QString& aText) { Status->setText(aText); }
	void review_tour_editor::trigger(tour_editor_action aAction)
	{
		if (Shown.Refusal == core::tour_refusal::bounds && aAction != tour_editor_action::retry_save && aAction != tour_editor_action::use_saved_copy && aAction != tour_editor_action::navigate_stop && aAction != tour_editor_action::navigate_related) {
			show_notice(tr("Shorten the unsaved text or use native Undo before changing the tour. Your full text is retained."));
			return;
		}
		if (Action) {
			Action(aAction);
		}
	}
	void review_tour_editor::show_stops()
	{
		open_brief(-1);
		Roadmap->setCurrentIndex(2);
		place_stop_card();
		const auto* stop = selected_stop();
		auto* const add  = stop == nullptr && Mode == tour_editor_mode::route ? OutlineTools->findChild<QPushButton*>(QStringLiteral("tourAddStop")) : nullptr;
		if (add != nullptr) {
			// [[BPR-1784]]. No stop yet: the step's first control adds one.
			add->setFocus(Qt::OtherFocusReason);
		} else if (stop != nullptr && StopView.Mode == core::tour_stop_view_mode::read) {
			// [[BPR-1805]]. A read stop: the pencil, so Space writes it.
			EditStop->setFocus();
		} else if (stop != nullptr && stop->Title.empty()) {
			Title->setFocus();
		} else {
			Body->setFocus();
		}
	}
	auto review_tour_editor::selected_stop() const -> const core::tour_stop*
	{
		if (!Shown.Draft) {
			return nullptr;
		}
		const auto found = std::ranges::find(Shown.Draft->Stops, Selected, &core::tour_stop::Id);
		return found == Shown.Draft->Stops.end() ? nullptr : &*found;
	}
	auto review_tour_editor::selected_related() const -> std::optional<core::tour_source_range>
	{
		const auto* stop = selected_stop();
		const auto  row  = Related->currentRow();
		return stop && row >= 0 && static_cast<std::size_t>(row) < stop->Related.size() ? std::optional{ stop->Related[static_cast<std::size_t>(row)] } : std::nullopt;
	}
	void review_tour_editor::build_stops()
	{
		auto* outline = new tour_outline{ this };
		Outline       = outline;
		Outline->setSelectionMode(QAbstractItemView::ExtendedSelection);
		Outline->setItemDelegate(new route_delegate{ Outline });
		Outline->setDragDropMode(QAbstractItemView::InternalMove);
		Outline->setDefaultDropAction(Qt::MoveAction);
		Outline->setMouseTracking(true);
		Outline->setFrameShape(QFrame::NoFrame);
		Outline->viewport()->setAutoFillBackground(false);
		Outline->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		Outline->setContextMenuPolicy(Qt::CustomContextMenu);
		auto* menu = new QMenu{ Outline };
		menu->setObjectName(QStringLiteral("tourStopMenu"));
		const auto action = [&](const QString& aText, const QString& aName, const QList<QKeySequence>& aKeys, auto aCallback) {
			auto* value = menu->addAction(aText);
			value->setObjectName(aName);
			value->setShortcuts(aKeys);
			value->setShortcutContext(Qt::WidgetShortcut);
			Outline->addAction(value);
			connect(value, &QAction::triggered, this, aCallback);
			return value;
		};
		action(tr("Insert after"), QStringLiteral("tourInsertAfter"), { }, [this] { trigger(tour_editor_action::insert_after); });
		DuplicateStops = action(tr("Duplicate"), QStringLiteral("tourDuplicate"), shortcut_keys(shortcut_command::tour_duplicate), [this] { structural(core::tour_command_kind::duplicate); });
		MergeStops     = action(tr("Merge"), QStringLiteral("tourMerge"), { }, [this] { structural(core::tour_command_kind::merge_next); });
		MergeStops->setToolTip(tr("Select two or more adjacent stops to merge in route order."));
		// [[BPR-1241]]. Option-R (Alt+R) is the window's Compose review; a second claim
		// on it in the same window is ambiguous and neither action would run. This
		// repoints the stop at the lines selected in either pane and copies no code,
		// so it is not a diff tool's Copy to Right and takes none of its keys.
		auto* const retarget = action(tr("Use selected lines"), QStringLiteral("tourRetarget"), shortcut_keys(shortcut_command::tour_retarget), [this] { trigger(tour_editor_action::retarget_stop); });
		retarget->setToolTip(tr("Point this stop at the lines selected in the comparison (%1). No code is copied or changed.").arg(retarget->shortcut().toString(QKeySequence::NativeText)));
		menu->setToolTipsVisible(true);
		RemoveStops         = action(tr("Remove"), QStringLiteral("tourRemove"), shortcut_keys(shortcut_command::tour_remove), [this] { structural(core::tour_command_kind::remove); });
		const auto shortcut = [&](const QString& aName, const shortcut_command aKeys, QWidget* aOwner, auto aCallback) {
			auto* value = new QAction{ aOwner };
			value->setObjectName(aName);
			value->setShortcuts(shortcut_keys(aKeys));
			value->setShortcutContext(Qt::WidgetWithChildrenShortcut);
			aOwner->addAction(value);
			connect(value, &QAction::triggered, this, aCallback);
			return value;
		};
		shortcut(QStringLiteral("tourMoveUp"), shortcut_command::tour_move_up, Outline, [this] { move_selection(-1); });
		shortcut(QStringLiteral("tourMoveDown"), shortcut_command::tour_move_down, Outline, [this] { move_selection(1); });
		UndoStructure = shortcut(QStringLiteral("tourUndo"), shortcut_command::tour_undo, this, [this] { trigger(tour_editor_action::undo_structure); });
		RedoStructure = shortcut(QStringLiteral("tourRedo"), shortcut_command::tour_redo, this, [this] { trigger(tour_editor_action::redo_structure); });
		outline->Move = [this](std::size_t aPosition) { structural(core::tour_command_kind::move, aPosition); };
		connect(Outline, &QListWidget::itemSelectionChanged, this, [this] { update_stop_actions(); });
		connect(Outline, &QListWidget::customContextMenuRequested, this, [this, menu](const QPoint& aPosition) {
			if (auto* item = Outline->itemAt(aPosition)) {
				Outline->setCurrentItem(item, item->isSelected() ? QItemSelectionModel::NoUpdate : QItemSelectionModel::ClearAndSelect);
				update_stop_actions();
				menu->popup(Outline->viewport()->mapToGlobal(aPosition));
			}
		});
		Outline->setObjectName(QStringLiteral("tourOutline"));
		Outline->setAccessibleName(tr("Ordered tour stops; drag the handles or use Alt+Shift+Up and Alt+Shift+Down to reorder; Shift or Command/Control selects multiple stops"));
		Outline->setWordWrap(true);
		Outline->setTextElideMode(Qt::ElideNone);

		// Add a stop, or ask for an outline; the suggestions wait below.
		OutlineTools            = new QWidget{ this };
		auto* const toolsColumn = new QVBoxLayout{ OutlineTools };
		toolsColumn->setContentsMargins(0, 0, 0, 0);
		toolsColumn->setSpacing(6);
		auto* const toolsRow = new QHBoxLayout{ };
		toolsRow->setSpacing(6);
		auto* add = new quiet_push_button{ tr("Add stop"), OutlineTools };
		add->setObjectName(QStringLiteral("tourAddStop"));
		add->setToolTip(tr("Add a stop on the lines selected in the code"));
		toolsRow->addWidget(add);
		connect(add, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::add_stop); });
		auto* suggest = new quiet_push_button{ tr("Suggest an outline"), OutlineTools };
		suggest->setObjectName(QStringLiteral("tourSuggestOutline"));
		suggest->setToolTip(tr("Ask once for titles and source destinations. Review the suggestions before inserting empty stops."));
		toolsRow->addWidget(suggest);
		toolsRow->addStretch(1);
		toolsColumn->addLayout(toolsRow);
		connect(suggest, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::suggest_outline); });
		SuggestedOutline = new QListWidget{ OutlineTools };
		SuggestedOutline->setObjectName(QStringLiteral("tourSuggestedOutline"));
		SuggestedOutline->setAccessibleName(tr("Suggested skeletons; check the stops to insert"));
		SuggestedOutline->setWordWrap(true);
		SuggestedOutline->setTextElideMode(Qt::ElideNone);
		SuggestedOutline->setMaximumHeight(140);
		toolsColumn->addWidget(SuggestedOutline);
		InsertOutline = new quiet_push_button{ tr("Insert selected skeleton stops"), OutlineTools };
		InsertOutline->setObjectName(QStringLiteral("tourInsertOutline"));
		toolsColumn->addWidget(InsertOutline, 0, Qt::AlignLeft);
		connect(InsertOutline, &QPushButton::clicked, this, [this] { trigger(tour_editor_action::insert_outline); });
		present_outline({ });

		// The stop, written in place: its title, its source, its explanation.
		StopFields = new QWidget{ this };
		StopFields->setObjectName(QStringLiteral("tourStopFields"));
		// [[BPR-1805]]. On the tour page the stop keeps to its content; the
		// column's spare height stays below it, not between its title and text.
		StopFields->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
		auto* fields = new QVBoxLayout{ StopFields };
		fields->setContentsMargins(0, 0, 0, 0);
		fields->setSpacing(6);
		auto* const titleRow = new QHBoxLayout{ };
		titleRow->setSpacing(6);
		Title = new QLineEdit{ StopFields };
		Title->setObjectName(QStringLiteral("tourStopTitle"));
		Title->setAccessibleName(tr("Stop title"));
		Title->setPlaceholderText(tr("What should the reviewer notice?"));
		Title->setMaxLength(std::numeric_limits<int>::max());
		Title->setFrame(false);
		auto titleFont = Title->font();
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		Title->installEventFilter(this);
		titleRow->addWidget(Title, 1);
		// [[BPR-1805]]. Read, the title is text, as a comment's author line
		// is; the pencil beside it opens the title and source.
		ReadTitle = new QLabel{ StopFields };
		ReadTitle->setObjectName(QStringLiteral("tourStopReadTitle"));
		ReadTitle->setTextFormat(Qt::PlainText);
		ReadTitle->setWordWrap(true);
		ReadTitle->setFont(titleFont);
		ReadTitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
		ReadTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		titleRow->addWidget(ReadTitle, 1);
		StopState = new QLabel{ StopFields };
		StopState->setObjectName(QStringLiteral("tourStopState"));
		StopState->setForegroundRole(QPalette::PlaceholderText);
		titleRow->addWidget(StopState, 0, Qt::AlignTop);
		EditStop = new QToolButton{ StopFields };
		EditStop->setObjectName(QStringLiteral("tourStopEdit"));
		// The comments' pencil, as Edit comment draws it.
		EditStop->setIcon(conversation_icon(conversation_glyph::edit, palette()));
		EditStop->setIconSize(QSize{ 16, 16 });
		EditStop->setToolButtonStyle(Qt::ToolButtonIconOnly);
		EditStop->setAutoRaise(true);
		EditStop->setCursor(Qt::ArrowCursor);
		EditStop->setFocusPolicy(Qt::StrongFocus);
		EditStop->setAccessibleName(tr("Edit stop"));
		EditStop->setToolTip(tr("Edit the stop's title and explanation (Return)"));
		titleRow->addWidget(EditStop, 0, Qt::AlignTop);
		connect(EditStop, &QToolButton::clicked, this, [this] { begin_stop_edit(); });
		DoneStop = new quiet_push_button{ tr("Done"), StopFields };
		DoneStop->setObjectName(QStringLiteral("tourStopDone"));
		DoneStop->setAccessibleName(tr("Done editing stop"));
		DoneStop->setToolTip(tr("Read the stop as it will be shown (%1 or Esc). Your text is saved as you type.").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
		titleRow->addWidget(DoneStop, 0, Qt::AlignTop);
		connect(DoneStop, &QPushButton::clicked, this, [this] { finish_stop_edit(); });
		fields->addLayout(titleRow);
		Source = paragraph({ }, StopFields);
		Source->setObjectName(QStringLiteral("tourStopSource"));
		Source->setFont(code_font_of(font()));
		fields->addWidget(Source);
		Related = new QListWidget{ StopFields };
		Related->setObjectName(QStringLiteral("tourRelatedSources"));
		Related->setAccessibleName(tr("Related source destinations"));
		Related->setWordWrap(true);
		Related->setTextElideMode(Qt::ElideNone);
		Related->setFlow(QListView::LeftToRight);
		Related->setWrapping(true);
		Related->setFrameShape(QFrame::NoFrame);
		Related->setFont(code_font_of(font()));
		Related->setMaximumHeight(56);
		Related->viewport()->setAutoFillBackground(false);
		fields->addWidget(Related);
		Body = new scoped_prose_editor{ StopFields };
		Body->setObjectName(QStringLiteral("tourStopBody"));
		Body->setAccessibleName(tr("Stop explanation Markdown source"));
		Body->setPlaceholderText(tr("What would you like checked?"));
		Body->setMinimumHeight(110);
		Body->setTabChangesFocus(true);
		Body->installEventFilter(this);
		fields->addWidget(Body);
		// [[BPR-1805]]. The explanation as the reviewer will read it, rendered
		// as a comment is. Return in it, like the pencil, opens the source.
		StopReader = new markdown_body_view{ StopFields };
		StopReader->setObjectName(QStringLiteral("tourStopReader"));
		StopReader->setAccessibleName(tr("Stop explanation"));
		StopReader->set_in_place(true);
		StopReader->set_context_controls(true);
		apply_reader_code_font();
		StopReader->set_empty_body_text(tr("No explanation yet. Use the pencil to write one."));
		// Placing the card reparents the reader, which reports a new height:
		// the card is placed again once, after, never from inside itself.
		StopReader->set_height_observer([this] {
			if (!StopPlacePending) {
				StopPlacePending = true;
				QMetaObject::invokeMethod(
					this,
					[this] {
						StopPlacePending = false;
						place_stop_card();
					},
					Qt::QueuedConnection);
			}
		});
		StopReader->rendered_view()->installEventFilter(this);
		fields->addWidget(StopReader);
		const auto button = [this](const QString& aText, const QString& aName, QHBoxLayout* aRow, auto aCallback) {
			// [[BPR-1776]]. Every stop action is the shared chip, raised at
			// rest, so it reads as something to press.
			QPushButton* value = new quiet_push_button{ aText, StopFields };
			value->setObjectName(aName);
			aRow->addWidget(value);
			connect(value, &QPushButton::clicked, this, aCallback);
			return value;
		};
		// [[BPR-1757]]. The row keeps to the stop's width: in a narrow
		// drawer or page an action with no room waits behind More, which
		// offers it too, rather than lose its words.
		StopActions         = new stop_action_row{ StopFields };
		auto* const sources = StopActions->row();
		add_writing_button(Body, core::writing_hook::comment, sources);
		StopMagic            = StopFields->findChild<QPushButton*>(QStringLiteral("tourStopMagic"));
		auto* const useLines = button(tr("Use selected lines"), QStringLiteral("tourUseSelectedLines"), sources, [retarget] { retarget->trigger(); });
		useLines->setToolTip(retarget->toolTip());
		auto* const addRelated = button(tr("Related source"), QStringLiteral("tourAddRelated"), sources, [this] { trigger(tour_editor_action::add_related); });
		addRelated->setToolTip(tr("Link the lines selected in the code as related source"));
		auto* const removeRelated = button(tr("Remove link"), QStringLiteral("tourRemoveRelated"), sources, [this] {
			if (!selected_related() || !Dispatch || Shown.Refusal == core::tour_refusal::bounds) {
				return;
			}
			auto value = *selected_stop();
			value.Related.erase(value.Related.begin() + Related->currentRow());
			Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = core::tour_command_kind::edit_stop, .Stop = value.Id, .Value = std::move(value) });
		});
		// [[BPR-1593]]. The rest of the stop's actions, behind More: the
		// route has room for the three a stop is written with.
		auto* const more = new QToolButton{ StopFields };
		more->setObjectName(QStringLiteral("tourStopMore"));
		// [[BPR-1776]]. One sign: the conversation cards' More glyph. An
		// instant-popup menu would add Qt's own arrow beside it, so the menu
		// opens under the button on a press instead.
		more->setIcon(conversation_icon(conversation_glyph::more, palette()));
		more->setIconSize(QSize{ 16, 16 });
		more->setToolButtonStyle(Qt::ToolButtonIconOnly);
		more->setAutoRaise(true);
		more->setCursor(Qt::ArrowCursor);
		more->setToolTip(tr("More: related source, insert, duplicate, merge, remove"));
		more->setAccessibleName(tr("More stop actions"));
		auto* const moreMenu = new QMenu{ more };
		moreMenu->setObjectName(QStringLiteral("tourStopMoreMenu"));
		moreMenu->addAction(tr("Related source"), addRelated, &QPushButton::click);
		moreMenu->addAction(tr("Remove related link"), removeRelated, &QPushButton::click);
		moreMenu->addSeparator();
		for (auto* const item : menu->actions()) {
			moreMenu->addAction(item);
		}
		connect(more, &QToolButton::clicked, moreMenu, [more, moreMenu] { moreMenu->popup(more->mapToGlobal(QPoint{ 0, more->height() })); });
		sources->addWidget(more);
		AddRelated    = addRelated;
		RemoveRelated = removeRelated;
		MoreButton    = more;
		sources->addStretch(1);
		fields->addWidget(StopActions);
		// The first to stay is the first a stop is written with.
		StopActions->yield(useLines, true);
		StopActions->yield(addRelated, Mode == tour_editor_mode::page);
		StopActions->yield(removeRelated, false);
		connect(Related, &QListWidget::currentRowChanged, removeRelated, [this](const int aRow) { StopActions->want(RemoveRelated, aRow >= 0 && Mode == tour_editor_mode::page); });
		Related->setToolTip(tr("Double-click a related source to inspect it. Show in code restores this stop."));
		connect(Related, &QListWidget::itemActivated, this, [this] { trigger(tour_editor_action::navigate_related); });
		Reaffirm = new quiet_push_button{ tr("Reaffirm explanation for current head"), StopFields };
		Reaffirm->setObjectName(QStringLiteral("tourReaffirm"));
		fields->addWidget(Reaffirm, 0, Qt::AlignLeft);
		connect(Reaffirm, &QPushButton::clicked, this, [this] { structural(core::tour_command_kind::reaffirm); });
		auto* nav = new QHBoxLayout;
		nav->setSpacing(6);
		fields->addLayout(nav);
		auto* const returnToTour = button(tr("Show in code"), QStringLiteral("tourReturn"), nav, [this] { trigger(tour_editor_action::navigate_stop); });
		returnToTour->setToolTip(tr("Show the stop's lines in the code"));
		nav->addStretch(1);
		auto* complete = new QPushButton{ tr("Ready, next"), StopFields };
		complete->setObjectName(QStringLiteral("tourComplete"));
		complete->setDefault(true);
		complete->setShortcut(shortcut_key(shortcut_command::tour_ready_next));
		complete->setToolTip(tr("Mark this stop ready and go to the next (%1)").arg(complete->shortcut().toString(QKeySequence::NativeText)));
		nav->addWidget(complete);
		connect(complete, &QPushButton::clicked, this, [this] {structural(core::tour_command_kind::complete); if(const auto* stop=selected_stop();stop && stop->Preparation==core::tour_preparation::ready){next_stop(1);} });
		connect(Title, &QLineEdit::textEdited, this, [this] { edit_stop(); });
		connect(Body, &QPlainTextEdit::textChanged, this, [this] {
			edit_stop();
			place_stop_card();
		});

		// The page's stop: its number and title, Previous and Next, its code.
		StopsHost               = new QWidget{ Roadmap };
		auto* const stopsColumn = new QVBoxLayout{ StopsHost };
		stopsColumn->setContentsMargins(0, 0, 0, 0);
		stopsColumn->setSpacing(10);
		StopHeader            = new QWidget{ StopsHost };
		auto* const headerRow = new QHBoxLayout{ StopHeader };
		headerRow->setContentsMargins(0, 0, 0, 0);
		StopHeading = new QLabel{ StopHeader };
		StopHeading->setObjectName(QStringLiteral("tourStopHeading"));
		auto headingFont = StopHeading->font();
		headingFont.setWeight(QFont::DemiBold);
		headingFont.setPointSizeF(headingFont.pointSizeF() * 1.25);
		StopHeading->setFont(headingFont);
		StopHeading->setTextFormat(Qt::PlainText);
		headerRow->addWidget(StopHeading, 1);
		Previous = new QPushButton{ tr("Previous"), StopHeader };
		Previous->setObjectName(QStringLiteral("tourPrevious"));
		Previous->setFlat(true);
		connect(Previous, &QPushButton::clicked, this, [this] { next_stop(-1); });
		headerRow->addWidget(Previous);
		Next = new QPushButton{ tr("Next"), StopHeader };
		Next->setObjectName(QStringLiteral("tourNext"));
		Next->setFlat(true);
		connect(Next, &QPushButton::clicked, this, [this] { next_stop(1); });
		headerRow->addWidget(Next);
		stopsColumn->addWidget(StopHeader);
		Excerpt = new excerpt_view{ StopsHost };
		Excerpt->hide();
		stopsColumn->addWidget(Excerpt);
		stopsColumn->addStretch(0);
		Roadmap->addTab(StopsHost, tr("Tour stops"));

		connect(Outline, &QListWidget::currentRowChanged, this, [this](int aRow) {
			if (Applying || aRow < 0 || !Shown.Draft || static_cast<std::size_t>(aRow) >= Shown.Draft->Stops.size()) {
				return;
			}
			if (Shown.Refusal == core::tour_refusal::bounds) {
				const auto block = QSignalBlocker{ Outline };
				if (const auto* stop = selected_stop()) {
					Outline->setCurrentRow(static_cast<int>(stop - Shown.Draft->Stops.data()), QItemSelectionModel::ClearAndSelect);
				}
				show_notice(tr("Shorten the unsaved text or use native Undo before selecting another stop. Your full text is retained."));
				return;
			}
			if (WritingCancel) {
				WritingCancel();
			}
			Selected         = Shown.Draft->Stops[static_cast<std::size_t>(aRow)].Id;
			SelectionChanged = false;
			// [[BPR-1784]]. Choosing a stop is working on the stops.
			if (Mode == tour_editor_mode::route && BriefOpen >= 0) {
				open_brief(-1);
			}
			Applying = true;
			present_stops(false);
			Applying = false;
			place_stop_card();
			refresh_excerpt();
			if (Mode == tour_editor_mode::page) {
				Roadmap->setCurrentIndex(2);
			}
			trigger(tour_editor_action::navigate_stop);
		});
	}
	void review_tour_editor::present_stops(bool aOwnerChanged)
	{
		// [[BPR-1593]]. Rows may move or go; the stop's fields leave the
		// route's card first, as a removed row deletes its widget. Only then:
		// a save that changes no row keeps the card, and the keyboard in it.
		auto shownIds = std::vector<qulonglong>{ };
		for (int index = 0; index < Outline->count(); ++index) {
			shownIds.push_back(Outline->item(index)->data(Qt::UserRole).toULongLong());
		}
		auto nextIds = std::vector<qulonglong>{ };
		if (Shown.Draft && !aOwnerChanged) {
			for (const auto& stop : Shown.Draft->Stops) {
				nextIds.push_back(stop.Id.value());
			}
		}
		const auto restructured = aOwnerChanged || shownIds != nextIds;
		if (restructured && Outline->isAncestorOf(StopFields)) {
			RefocusStop = StopFields->isAncestorOf(QApplication::focusWidget()) ? QPointer<QWidget>{ QApplication::focusWidget() } : QPointer<QWidget>{ };
			StopFields->setParent(this);
			StopFields->hide();
			for (int index = 0; index < Outline->count(); ++index) {
				if (Outline->itemWidget(Outline->item(index)) != nullptr) {
					Outline->removeItemWidget(Outline->item(index));
					Outline->item(index)->setSizeHint({ });
				}
			}
		}
		if (Editing.value()) {
			BodyPositions[Editing] = Body->textCursor().position();
		}
		if (aOwnerChanged) {
			// Detach the native document before destroying another owner's private text.
			auto* blank = new QTextDocument{ Body };
			blank->setDocumentLayout(new QPlainTextDocumentLayout{ blank });
			Body->bind_document(blank);
			for (const auto& [id, document] : BodyDocuments) {
				static_cast<void>(id);
				delete document;
			}
			BodyDocuments.clear();
			BodyPositions.clear();
			Editing = { };
		}
		const auto block = QSignalBlocker{ Outline };
		const auto count = Shown.Draft ? Shown.Draft->Stops.size() : 0U;
		for (std::size_t index = 0; index < count; ++index) {
			const auto& stop  = Shown.Draft->Stops[index];
			const auto  row   = static_cast<int>(index);
			auto        found = row;
			while (found < Outline->count() && Outline->item(found)->data(Qt::UserRole).toULongLong() != stop.Id.value()) {
				++found;
			}
			if (found == Outline->count()) {
				auto* item = new QListWidgetItem;
				item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
				item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(stop.Id.value()));
				Outline->insertItem(row, item);
			} else if (found != row) {
				Outline->insertItem(row, Outline->takeItem(found));
			}
			auto*      item  = Outline->item(row);
			const auto state = stop.Preparation == core::tour_preparation::ready ? tr("Ready") : stop.Preparation == core::tour_preparation::needs_attention ? tr("Needs attention")
				: stop.Preparation == core::tour_preparation::suggested                                                                                      ? tr("Suggested / unvisited")
																																							 : tr("Editing");
			const auto title = stop.Title.empty() ? tr("Untitled stop") : QString::fromStdString(stop.Title);
			// [[BPR-1593]]. The route draws the number in its ring; the row's
			// text stays "N. Title" for whoever reads it as text.
			item->setText(tr("%1. %2").arg(row + 1).arg(stop.Preparation == core::tour_preparation::suggested ? tr("Suggested: %1").arg(title) : title));
			item->setData(Qt::DisplayRole, stop.Preparation == core::tour_preparation::suggested ? tr("Suggested: %1").arg(title) : title);
			item->setData(number_role, QString::number(row + 1));
			item->setData(preparation_role, static_cast<int>(stop.Preparation));
			const auto* const range = stop.Original ? &*stop.Original : stop.Mapped ? &*stop.Mapped
																					: nullptr;
			auto              where = QString{ };
			auto              path  = QString{ };
			if (range != nullptr) {
				path  = QString::fromStdString(range->Path);
				where = range->First.Line == range->Last.Line ? QStringLiteral("%1:%2").arg(path.section(QLatin1Char{ '/' }, -1)).arg(range->First.Line + 1U)
															  : QStringLiteral("%1:%2–%3").arg(path.section(QLatin1Char{ '/' }, -1)).arg(range->First.Line + 1U).arg(range->Last.Line + 1U);
			} else if (stop.Destination) {
				path  = QString::fromStdString(stop.Destination->Path);
				where = QStringLiteral("%1:%2").arg(path.section(QLatin1Char{ '/' }, -1)).arg(stop.Destination->FirstLine + 1U);
			}
			item->setData(source_role, where);
			item->setData(file_role, path.section(QLatin1Char{ '/' }, -1));
			auto previousPath = QString{ };
			if (index > 0U) {
				const auto& before = Shown.Draft->Stops[index - 1U];
				const auto* prior  = before.Original ? &*before.Original : before.Mapped ? &*before.Mapped
																						 : nullptr;
				previousPath       = prior != nullptr ? QString::fromStdString(prior->Path) : before.Destination ? QString::fromStdString(before.Destination->Path)
																												 : QString{ };
			}
			item->setData(divider_role, index > 0U && !path.isEmpty() && path != previousPath);
			auto words = QString::fromStdString(stop.Body).simplified();
			item->setData(excerpt_role, words.left(240));
			item->setData(attention_role, stop.Mapping != core::tour_mapping::exact_original && stop.Mapping != core::tour_mapping::unchanged_material && stop.Mapping != core::tour_mapping::unavailable);
			item->setData(current_role, false);
			const auto spoken = tr("%1. %2. %3").arg(row + 1).arg(title, state);
			item->setData(Qt::AccessibleTextRole, spoken);
			item->setToolTip(spoken);
		}
		while (Outline->count() > static_cast<int>(count)) {
			delete Outline->takeItem(Outline->count() - 1);
		}
		if (!selected_stop()) {
			Selected         = count ? Shown.Draft->Stops.front().Id : core::tour_stop_id{ };
			SelectionChanged = count != 0;
		}
		const auto* stop = selected_stop();
		StopFields->setEnabled(stop != nullptr && !Shown.Loading);
		UndoStructure->setEnabled(Shown.CanUndo);
		RedoStructure->setEnabled(Shown.CanRedo);
		update_stop_actions();
		if (!stop) {
			Title->clear();
			Body->clear();
			Source->setText(tr("Select source in the comparison, then add a tour stop. You can keep writing the brief while source is unavailable."));
			Related->clear();
			Editing = { };
			view_stop(core::tour_stop_view_event::open);
			return;
		}
		const auto row = static_cast<int>(stop - Shown.Draft->Stops.data());
		Outline->setCurrentRow(row, QItemSelectionModel::NoUpdate);
		if (SelectionChanged) {
			Outline->clearSelection();
			Outline->item(row)->setSelected(true);
		}
		update_stop_actions();
		const bool changed = Editing != stop->Id;
		if (changed) {
			auto& document = BodyDocuments[stop->Id];
			if (!document) {
				document = new QTextDocument{ this };
				document->setDocumentLayout(new QPlainTextDocumentLayout{ document });
				document->setPlainText(QString::fromStdString(stop->Body));
			}
			Body->bind_document(document);
			Editing = stop->Id;
		}
		if (changed || Shown.Refusal == core::tour_refusal::none) {
			if (Title->text() != QString::fromStdString(stop->Title)) {
				Title->setText(QString::fromStdString(stop->Title));
				if (!Title->hasFocus()) {
					Title->setCursorPosition(0);
				}
			}
			Body->present(stop->Body);
		}
		if (changed) {
			auto cursor = Body->textCursor();
			cursor.setPosition(std::min(BodyPositions[stop->Id], Body->document()->characterCount() - 1));
			Body->setTextCursor(cursor);
		}
		// [[BPR-1805]]. Another stop opens to read, unless it has nothing to
		// read yet; the open one keeps its mode, as a save presents it again.
		ReadTitle->setText(stop->Title.empty() ? tr("Untitled stop") : QString::fromStdString(stop->Title));
		ReadTitle->setForegroundRole(stop->Title.empty() ? QPalette::PlaceholderText : QPalette::WindowText);
		StopReader->set_body(stop->Body, "tour-stop-" + std::to_string(stop->Id.value()));
		view_stop(core::tour_stop_view_event::open);
		// [[BPR-1783]]. A stop with no title yet, a new one above all, takes
		// the keyboard in its title; Tab goes on to the body.
		if (changed && stop->Title.empty() && StopView.Mode == core::tour_stop_view_mode::edit) {
			if (RefocusStop != nullptr) {
				RefocusStop = Title;
			} else if (StopFields->isAncestorOf(QApplication::focusWidget())) {
				Title->setFocus(Qt::OtherFocusReason);
			}
		}
		const auto label = [](const core::tour_source_range& aSource) { return QStringLiteral("%1 · %2:%3–%4 · %5").arg(QString::fromStdString(aSource.Path), aSource.First.Side == core::diff_side::left ? QStringLiteral("base") : QStringLiteral("head")).arg(aSource.First.Line + 1U).arg(aSource.Last.Line + 1U).arg(QString::fromStdString(std::string{ core::canonical_hex(aSource.Commit).view() }).left(12)); };
		for (int index = 0; index < Outline->count(); ++index) {
			Outline->item(index)->setData(current_role, index == row);
		}
		Source->setText(stop->Original ? tr("Original: ") + label(*stop->Original) : tr("Source unavailable — your text remains editable."));
		if (!stop->Original && stop->Destination) {
			const auto& destination = *stop->Destination;
			Source->setText(QStringLiteral("%1 · %2–%3 · %4").arg(QString::fromStdString(destination.Path)).arg(destination.FirstLine + 1U).arg(destination.LastLine + 1U).arg(QString::fromStdString(std::string{ core::canonical_hex(destination.Commit).view() }).left(12)));
		}
		if (stop->Mapped) {
			Source->setText(Source->text() + tr("\nProposed mapping: ") + label(*stop->Mapped));
		}
		const auto mapping = stop->Mapping == core::tour_mapping::exact_original ? tr("Exact original") : stop->Mapping == core::tour_mapping::unchanged_material ? tr("Moved with unchanged selected material")
			: stop->Mapping == core::tour_mapping::changed_material                                                                                               ? tr("Changed material")
			: stop->Mapping == core::tour_mapping::ambiguous                                                                                                      ? tr("Ambiguous")
			: stop->Mapping == core::tour_mapping::missing                                                                                                        ? tr("Missing")
			: stop->Mapping == core::tour_mapping::author_selected                                                                                                ? tr("Author-selected replacement")
																																								  : tr("Source unavailable");
		Source->setText(Source->text() + "\n" + mapping);
		if (stop->Suggestion) {
			const auto& suggestion = *stop->Suggestion;
			Source->setText(Source->text() + tr("\nPrivate outline reason: ") + QString::fromStdString(suggestion.Reason));
			if (!suggestion.Question.empty()) {
				Source->setText(Source->text() + tr("\nAuthor question: ") + QString::fromStdString(suggestion.Question));
			}
		}
		if (Shown.Draft->ObservedHead && stop->ReaffirmedHead != Shown.Draft->ObservedHead) {
			Source->setText(Source->text() + tr("\nHead or surrounding contract changed. Reaffirm or revise the explanation before publishing; identical selected bytes are not proof."));
		}
		if (stop->Mapping != core::tour_mapping::exact_original && stop->Mapping != core::tour_mapping::unchanged_material) {
			Source->setText(Source->text() + tr("\nSource needs attention; original destination is retained."));
		}
		// [[BPR-1593]]. The source in the code's figures; what it means, on
		// request, and said only when something needs doing.
		{
			const auto details   = Source->text();
			const auto attention = details.contains(tr("Source needs attention")) || details.contains(tr("Head or surrounding contract changed"));
			Source->setToolTip(details);
			auto chip = Outline->item(row)->data(source_role).toString();
			if (chip.isEmpty()) {
				chip = tr("Source unavailable — your text remains editable.");
			}
			if (attention) {
				chip += QStringLiteral("  ·  ") + (details.contains(tr("Head or surrounding contract changed")) ? tr("The head changed: reaffirm or revise before publishing") : tr("Source needs attention"));
			}
			Source->setText(chip);
			Reaffirm->setVisible(Shown.Draft->ObservedHead && stop->ReaffirmedHead != Shown.Draft->ObservedHead);
			StopState->setText(stop->Preparation == core::tour_preparation::ready ? tr("Ready") : stop->Preparation == core::tour_preparation::needs_attention ? tr("Needs attention")
								   : stop->Preparation == core::tour_preparation::suggested                                                                    ? tr("Suggested")
																																							   : tr("Editing"));
			StopHeading->setText(tr("Stop %1 of %2").arg(row + 1).arg(Outline->count()));
			Previous->setEnabled(row > 0);
			Next->setEnabled(row + 1 < Outline->count());
			Previous->setText(row > 0 ? tr("Stop %1").arg(row) : tr("Previous"));
			Next->setText(row + 1 < Outline->count() ? tr("Stop %1").arg(row + 2) : tr("Next"));
		}
		for (std::size_t index = 0; index < stop->Related.size(); ++index) {
			if (static_cast<int>(index) >= Related->count()) {
				Related->addItem(label(stop->Related[index]));
			} else {
				Related->item(static_cast<int>(index))->setText(label(stop->Related[index]));
			}
		}
		while (Related->count() > static_cast<int>(stop->Related.size())) {
			delete Related->takeItem(Related->count() - 1);
		}
		for (auto it = BodyDocuments.begin(); it != BodyDocuments.end();) {
			if (std::ranges::find(Shown.Draft->Stops, it->first, &core::tour_stop::Id) == Shown.Draft->Stops.end()) {
				delete it->second;
				BodyPositions.erase(it->first);
				it = BodyDocuments.erase(it);
			} else {
				++it;
			}
		}
	}
	void review_tour_editor::edit_stop()
	{
		if (Applying || !Dispatch || !selected_stop()) {
			return;
		}
		auto value  = *selected_stop();
		value.Title = Title->text().toUtf8().toStdString();
		value.Body  = Body->raw();
		EditingKind = core::tour_command_kind::edit_stop;
		Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = core::tour_command_kind::edit_stop, .Stop = value.Id, .Value = std::move(value) });
	}
	void review_tour_editor::view_stop(const core::tour_stop_view_event aEvent)
	{
		const auto* const stop  = selected_stop();
		const auto        title = Title->text().toUtf8().toStdString();
		auto              input = core::tour_stop_view_input{ .Stop = stop, .Event = aEvent, .Editable = Shown.Draft && !Shown.Loading && !Shown.Draft->ReadOnly, .Composing = composing() };
		if (stop != nullptr && Editing == stop->Id) {
			input.Held = core::tour_stop_held_text{ .Title = title, .Body = Body->raw() };
		}
		const auto before = StopView;
		StopView          = core::decide_tour_stop_view(StopView, input);
		if (before.Mode == core::tour_stop_view_mode::edit && StopView.Mode == core::tour_stop_view_mode::read && WritingCancel) {
			// The agent writes into the editor; reading, there is none to write into.
			WritingCancel();
		}
		present_stop_view();
	}
	void review_tour_editor::present_stop_view()
	{
		if (StopReader == nullptr || ReadTitle == nullptr || EditStop == nullptr || DoneStop == nullptr) {
			return;
		}
		const bool writing = StopView.Mode == core::tour_stop_view_mode::edit;
		auto*      focused = QApplication::focusWidget();
		const bool inText  = focused != nullptr && (focused == Title || focused == Body || Body->isAncestorOf(focused));
		const bool onRead  = focused != nullptr && (focused == EditStop || focused == StopReader->rendered_view());
		Title->setVisible(writing);
		Body->setVisible(writing);
		DoneStop->setVisible(writing);
		if (StopMagic != nullptr) {
			StopMagic->setVisible(writing);
		}
		ReadTitle->setVisible(!writing);
		StopReader->setVisible(!writing);
		EditStop->setVisible(!writing);
		// [[BPR-1805]] repair (owner review, 2026-10-29). A stop being written
		// but not yet ready says "Editing" only while its editor is open: read,
		// that word beside the pencil made the reader look like the editor.
		// Ready, Needs attention and Suggested still show either way.
		if (StopState != nullptr) {
			const auto* const stop = selected_stop();
			StopState->setVisible(writing || (stop != nullptr && stop->Preparation != core::tour_preparation::editing));
		}
		EditStop->setEnabled(Shown.Draft && !Shown.Loading && !Shown.Draft->ReadOnly);
		// The keyboard stays with the stop as it changes between read and written.
		if (writing && onRead) {
			if (const auto* stop = selected_stop(); stop != nullptr && stop->Title.empty()) {
				Title->setFocus(Qt::OtherFocusReason);
			} else {
				Body->setFocus(Qt::OtherFocusReason);
			}
		} else if (!writing && inText) {
			EditStop->setFocus(Qt::OtherFocusReason);
		}
		if (RefocusStop != nullptr && (RefocusStop == Title || RefocusStop == Body) && !writing) {
			RefocusStop = EditStop;
		}
	}
	void review_tour_editor::begin_stop_edit()
	{
		if (selected_stop() == nullptr) {
			return;
		}
		if (StopReader->rendered_view()->hasFocus()) {
			EditStop->setFocus(Qt::OtherFocusReason);
		}
		const bool wasReading = StopView.Mode == core::tour_stop_view_mode::read;
		view_stop(core::tour_stop_view_event::begin_edit);
		place_stop_card();
		if (wasReading && StopView.Mode == core::tour_stop_view_mode::edit && !Title->hasFocus() && !Body->hasFocus()) {
			(selected_stop()->Title.empty() ? static_cast<QWidget*>(Title) : static_cast<QWidget*>(Body))->setFocus(Qt::OtherFocusReason);
		}
	}
	void review_tour_editor::finish_stop_edit()
	{
		if (selected_stop() == nullptr) {
			return;
		}
		view_stop(core::tour_stop_view_event::finish_edit);
		if (StopView.Mode == core::tour_stop_view_mode::edit) {
			show_notice(tr("This stop's text has not been saved yet, so it stays open. Your full text is retained."));
		}
		place_stop_card();
	}
	void review_tour_editor::structural(core::tour_command_kind aKind, std::size_t aPosition)
	{
		if (!Dispatch || !selected_stop()) {
			return;
		}
		if (Shown.Refusal == core::tour_refusal::bounds) {
			show_notice(tr("Shorten the unsaved text or use native Undo before arranging stops. Your full text is retained."));
			return;
		}
		const bool grouped   = aKind == core::tour_command_kind::move || aKind == core::tour_command_kind::duplicate || aKind == core::tour_command_kind::merge_next || aKind == core::tour_command_kind::remove;
		const auto selection = grouped ? selected_stops() : std::vector<core::tour_stop_id>{ };
		if (grouped && selection.empty()) {
			return;
		}
		Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = aKind, .Stop = Selected, .Position = aPosition, .Selection = selection });
		if (aKind == core::tour_command_kind::move && Shown.Refusal == core::tour_refusal::none) {
			select_stops(selection);
		}
	}
	auto review_tour_editor::selected_stops() const -> std::vector<core::tour_stop_id>
	{
		std::vector<core::tour_stop_id> result;
		for (int row = 0; row < Outline->count(); ++row) {
			const auto* item = Outline->item(row);
			if (item->isSelected()) {
				result.emplace_back(item->data(Qt::UserRole).toULongLong());
			}
		}
		return result;
	}
	void review_tour_editor::select_stops(const std::vector<core::tour_stop_id>& aStops)
	{
		const QSignalBlocker block{ Outline };
		for (int row = 0; row < Outline->count(); ++row) {
			auto* item = Outline->item(row);
			item->setSelected(std::ranges::any_of(aStops, [&](auto aId) { return aId.value() == item->data(Qt::UserRole).toULongLong(); }));
		}
		update_stop_actions();
	}
	void review_tour_editor::update_stop_actions()
	{
		const auto selection = selected_stops();
		const bool editable  = Shown.Draft && !Shown.Draft->ReadOnly && !Shown.Loading && Shown.Refusal == core::tour_refusal::none;
		DuplicateStops->setEnabled(editable && !selection.empty());
		RemoveStops->setEnabled(editable && !selection.empty());
		bool adjacent = selection.size() >= 2;
		if (adjacent && Shown.Draft) {
			const auto first = std::ranges::find(Shown.Draft->Stops, selection.front(), &core::tour_stop::Id);
			const auto last  = std::ranges::find(Shown.Draft->Stops, selection.back(), &core::tour_stop::Id);
			adjacent         = first != Shown.Draft->Stops.end() && last != Shown.Draft->Stops.end() && static_cast<std::size_t>(last - first + 1) == selection.size();
		}
		MergeStops->setEnabled(editable && adjacent);
	}
	void review_tour_editor::move_selection(int aOffset)
	{
		const auto selection = selected_stops();
		if (selection.empty() || !Shown.Draft) {
			return;
		}
		const auto first    = std::ranges::find(Shown.Draft->Stops, selection.front(), &core::tour_stop::Id);
		const auto position = first - Shown.Draft->Stops.begin() + aOffset;
		if (position >= 0 && static_cast<std::size_t>(position) <= Shown.Draft->Stops.size() - selection.size()) {
			structural(core::tour_command_kind::move, static_cast<std::size_t>(position));
		}
	}
	void review_tour_editor::next_changed_stop()
	{
		if (!Shown.Draft) {
			return;
		}
		const auto& stops   = Shown.Draft->Stops;
		const auto  current = std::ranges::find(stops, Selected, &core::tour_stop::Id);
		const auto  start   = current == stops.end() ? 0U : static_cast<std::size_t>(current - stops.begin() + 1);
		for (std::size_t offset = 0; offset < stops.size(); ++offset) {
			const auto index   = (start + offset) % stops.size();
			const bool changed = std::ranges::any_of(Shown.Draft->ObservedChanges, [&](const auto& aChange) { return aChange.Stop == stops[index].Id && aChange.Kind != core::tour_change_kind::removed; });
			if (changed) {
				show_stops();
				Outline->setCurrentRow(static_cast<int>(index), QItemSelectionModel::ClearAndSelect);
				return;
			}
		}
	}

	void review_tour_editor::next_stop(int aOffset)
	{
		const auto row = Outline->currentRow() + aOffset;
		if (row >= 0 && row < Outline->count()) {
			Outline->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
		}
	}
	auto review_tour_editor::minimumSizeHint() const -> QSize { return { 280, 240 }; }

	void review_tour_editor::set_expand_dispatch(std::function<void(bool)> aDispatch) { ExpandDispatch = std::move(aDispatch); }
	void review_tour_editor::set_collapse_dispatch(std::function<void()> aDispatch) { CollapseDispatch = std::move(aDispatch); }
	void review_tour_editor::set_excerpt_source(std::function<std::optional<tour_excerpt>(const core::tour_source_range&)> aSource)
	{
		ExcerptSource = std::move(aSource);
		refresh_excerpt();
	}

	void review_tour_editor::set_mode(const tour_editor_mode aMode)
	{
		if (Mode == aMode) {
			return;
		}
		Mode = aMode;
		// [[BPR-1593]]. The same controls, moved: the brief, the outline and
		// the stop go where this mode draws them.
		auto* const delegate = static_cast<route_delegate*>(Outline->itemDelegate());
		delegate->Page       = aMode == tour_editor_mode::page;
		// The stop's fields leave the route's card before the card goes:
		// removing an item's widget deletes it.
		StopFields->setParent(this);
		for (int index = 0; index < Outline->count(); ++index) {
			// [[BPR-1757]]. The card is deleted later; until then it
			// must not stand in the page's outline at the route's width.
			if (auto* const card = Outline->itemWidget(Outline->item(index)); card != nullptr) {
				card->hide();
			}
			Outline->removeItemWidget(Outline->item(index));
			Outline->item(index)->setSizeHint({ });
		}
		if (aMode == tour_editor_mode::route) {
			auto* const column = static_cast<QVBoxLayout*>(RouteColumn->layout());
			column->insertWidget(0, Brief);
			column->addStretch(1);
			// [[BPR-1784]]. The route is step 3's body.
			StopsSlot->layout()->addWidget(Outline);
			StopsSlot->layout()->addWidget(OutlineTools);
			for (int field = 0; field < 2; ++field) {
				BriefSlots[field]->layout()->addWidget(BriefPanels[field]);
			}
			Outline->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			Outline->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			Outline->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			RouteColumn->show();
			PageColumns->hide();
			PreviewPublish->show();
			if (auto* const previewPage = Roadmap->widget(Roadmap->count() - 1); previewPage != nullptr && Preview->parentWidget() != previewPage) {
				static_cast<QVBoxLayout*>(previewPage->layout())->addWidget(Preview, 1);
			}
		} else {
			auto* const left = static_cast<QVBoxLayout*>(PageLeft->layout());
			// Remove the route's stretch, which the route adds last.
			if (auto* const column = static_cast<QVBoxLayout*>(RouteColumn->layout()); column->count() > 0 && column->itemAt(column->count() - 1)->spacerItem() != nullptr) {
				delete column->takeAt(column->count() - 1);
			}
			left->insertWidget(1, Brief);
			left->insertWidget(2, Outline, 1);
			left->insertWidget(3, OutlineTools);
			for (int field = 0; field < 2; ++field) {
				BriefSlots[field]->hide();
				BriefHosts[field]->layout()->addWidget(BriefPanels[field]);
			}
			StopsSlot->hide();
			static_cast<QVBoxLayout*>(StopsHost->layout())->insertWidget(2, StopFields, 0);
			StopFields->show();
			Outline->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
			Outline->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
			Outline->setMinimumHeight(0);
			Outline->setMaximumHeight(QWIDGETSIZE_MAX);
			RouteColumn->hide();
			PageColumns->show();
			PreviewPublish->hide();
			static_cast<QVBoxLayout*>(PublicationPanel->layout())->insertWidget(1, Preview, 1);
			refresh_preview();
			if (BriefOpen >= 0) {
				Roadmap->setCurrentIndex(BriefOpen);
			} else {
				Roadmap->setCurrentIndex(2);
			}
		}
		StopHeader->setVisible(aMode == tour_editor_mode::page);
		// The route keeps Related source behind More; the page has room.
		if (StopActions != nullptr) {
			StopActions->want(AddRelated, aMode == tour_editor_mode::page);
			StopActions->want(RemoveRelated, aMode == tour_editor_mode::page && Related->currentRow() >= 0);
		}
		fit_page_columns();
		// [[BPR-1784]]. The open step stays open in either place.
		open_brief(BriefOpen);
		place_stop_card();
		refresh_excerpt();
		refresh_foot();
	}

	void review_tour_editor::open_brief(const int aField)
	{
		BriefOpen = aField;
		for (int field = 0; field < 3; ++field) {
			auto* const row = static_cast<QAbstractButton*>(StepRows[field]);
			const auto  on  = field == 2 ? aField < 0 : field == aField;
			if (row != nullptr) {
				const auto block = QSignalBlocker{ row };
				row->setChecked(on);
			}
			if (auto* const slot = field == 2 ? StopsSlot : BriefSlots[field]; slot != nullptr && Mode == tour_editor_mode::route) {
				slot->setVisible(on);
			}
		}
		refresh_steps();
		if (aField >= 0) {
			if (Roadmap->currentIndex() != aField) {
				Roadmap->setCurrentIndex(aField);
			}
		} else if (Roadmap->currentIndex() < 2) {
			Roadmap->setCurrentIndex(2);
		}
		place_stop_card();
		fit_route();
	}

	void review_tour_editor::choose_step(const int aField)
	{
		if (aField < 0) {
			show_stops();
			return;
		}
		open_brief(aField);
		focus_step();
	}

	void review_tour_editor::open_step(const core::tour_step aStep)
	{
		switch (aStep) {
		case core::tour_step::problem:  choose_step(0); break;
		case core::tour_step::solution: choose_step(1); break;
		case core::tour_step::stops:    choose_step(-1); break;
		}
	}

	void review_tour_editor::focus_step()
	{
		// [[BPR-1784]]. The keyboard goes to the open step's first field:
		// Problem's text, Solution's text, or the stop's.
		if (BriefOpen == 0) {
			Problem->setFocus(Qt::OtherFocusReason);
		} else if (BriefOpen == 1) {
			Solution->setFocus(Qt::OtherFocusReason);
		}
	}

	void review_tour_editor::refresh_steps()
	{
		const auto open = BriefOpen == 0 ? core::tour_step::problem : BriefOpen == 1 ? core::tour_step::solution
																					 : core::tour_step::stops;
		Steps           = core::decide_tour_steps(Shown.Draft.get(), open);
		for (std::size_t index = 0U; index < core::tour_step_count; ++index) {
			auto* const row = static_cast<tour_step_row*>(StepRows[index]);
			if (row == nullptr) {
				continue;
			}
			const auto state   = Steps.States[index];
			auto       summary = QString{ };
			if (index < 2U && state == core::tour_step_state::done && Shown.Draft) {
				summary = QString::fromStdString(index == 0U ? Shown.Draft->Problem : Shown.Draft->Solution);
			} else if (index < 2U && state == core::tour_step_state::needs_attention) {
				summary = tr("Not written yet");
			} else if (index == 2U && state != core::tour_step_state::current && Steps.Stops > 0U) {
				summary = Steps.Stops == 1U ? (Steps.ReadyStops == 1U ? tr("1 stop, ready") : tr("1 stop, not ready yet")) : tr("%1 of %2 stops ready").arg(Steps.ReadyStops).arg(Steps.Stops);
			}
			row->present(state, summary);
		}
		Brief->update();
	}

	void review_tour_editor::place_stop_card()
	{
		if (Outline == nullptr || StopFields == nullptr) {
			return;
		}
		Related->setVisible(Related->count() > 0);
		// The explanation is as tall as what is written, 110 to 320 pixels,
		// then it scrolls; the document it shows is the stop's own.
		if (auto* const layout = Body->document()->documentLayout(); layout != nullptr) {
			const auto lines  = static_cast<int>(std::ceil(layout->documentSize().height()));
			const auto height = std::clamp(lines * QFontMetrics{ Body->font() }.lineSpacing() + 18, 110, 320);
			if (Body->height() != height || Body->minimumHeight() != height) {
				Body->setFixedHeight(height);
			}
		}
		// [[BPR-1805]]. The read explanation is as tall as it renders at the
		// stop's width: it reads in place, as a comment does, and never scrolls.
		if (StopReader->isVisibleTo(StopFields)) {
			const auto available = Mode == tour_editor_mode::route ? std::max(200, Outline->viewport()->width()) - (text_left - 6) - 12 : StopFields->contentsRect().width();
			if (available > 0) {
				const auto height = StopReader->heightForWidth(available);
				if (StopReader->height() != height || StopReader->minimumHeight() != height) {
					StopReader->setFixedHeight(height);
				}
			}
		}
		if (Mode != tour_editor_mode::route) {
			fit_route();
			return;
		}
		// The current stop opens in place on the route, under its title.
		const auto* const stop = selected_stop();
		const auto        row  = stop != nullptr && Shown.Draft ? static_cast<int>(stop - Shown.Draft->Stops.data()) : -1;
		for (int index = 0; index < Outline->count(); ++index) {
			auto* const item = Outline->item(index);
			if (index != row && Outline->itemWidget(item) != nullptr) {
				if (Outline->itemWidget(item)->isAncestorOf(StopFields)) {
					StopFields->setParent(this);
					StopFields->hide();
				}
				Outline->itemWidget(item)->hide();
				Outline->removeItemWidget(item);
				item->setSizeHint({ });
			}
		}
		if (row < 0 || row >= Outline->count()) {
			StopFields->hide();
			fit_route();
			return;
		}
		auto* const item = Outline->item(row);
		auto*       card = Outline->itemWidget(item);
		if (card == nullptr) {
			card = new QWidget{ };
			card->setObjectName(QStringLiteral("tourStopCard"));
			card->setAutoFillBackground(false);
			auto* const column = new QVBoxLayout{ card };
			column->setContentsMargins(text_left - 6, 10, 12, 10);
			column->addWidget(StopFields);
			StopFields->show();
			Outline->setItemWidget(item, card);
		}
		const auto width = std::max(200, Outline->viewport()->width());
		StopFields->show();
		StopFields->layout()->activate();
		card->layout()->activate();
		const auto height  = std::max(card->layout()->hasHeightForWidth() ? card->layout()->totalHeightForWidth(width) : 0, card->layout()->totalSizeHint().height());
		const auto divider = item->data(divider_role).toBool() ? divider_band : 0;
		if (item->sizeHint().height() != height + divider) {
			item->setSizeHint({ width, height + divider });
		}
		card->setContentsMargins(0, divider, 0, 0);
		fit_route();
		if (RefocusStop != nullptr && RefocusStop->isVisible()) {
			RefocusStop->setFocus(Qt::OtherFocusReason);
			RefocusStop = nullptr;
		}
	}

	void review_tour_editor::fit_route()
	{
		if (Mode != tour_editor_mode::route || Outline == nullptr) {
			return;
		}
		// The route is as tall as its stops: the drawer's column scrolls, the
		// route never does.
		Outline->doItemsLayout();
		auto height = 4;
		for (int index = 0; index < Outline->count(); ++index) {
			height += Outline->sizeHintForRow(index);
		}
		// Empty, the route takes no room, and still takes the keyboard.
		Outline->setFixedHeight(Outline->count() == 0 ? 2 : std::max(height, 40));
		// [[BPR-1784]]. The route sits in step 3's body, several widgets
		// below the column: each says its new size, and the column lays out
		// now, so the step grows with its route rather than a frame later.
		for (auto* widget = Outline->parentWidget(); widget != nullptr && widget != RouteColumn; widget = widget->parentWidget()) {
			widget->updateGeometry();
		}
		if (RouteColumn != nullptr && RouteColumn->layout() != nullptr) {
			RouteColumn->layout()->activate();
		}
	}

	void review_tour_editor::refresh_excerpt()
	{
		auto* const view = static_cast<excerpt_view*>(Excerpt);
		const auto* stop = selected_stop();
		if (Mode != tour_editor_mode::page || stop == nullptr || !ExcerptSource) {
			view->present(std::nullopt);
			return;
		}
		const auto* const range = stop->Original ? &*stop->Original : stop->Mapped ? &*stop->Mapped
																				   : nullptr;
		view->present(range != nullptr ? ExcerptSource(*range) : std::nullopt);
	}

	void review_tour_editor::refresh_foot()
	{
		PreviewPublish->setEnabled(Shown.Draft != nullptr && !Shown.Loading);
	}

	void review_tour_editor::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		fit_page_columns();
		place_stop_card();
	}

	void review_tour_editor::fit_page_columns()
	{
		// [[BPR-1757]]. The outline and On GitHub keep their 290 and
		// 360 pixels while the section between them has its own 340. In a
		// narrower page, down to the window's floor, they give way together
		// in proportion, to 230 and 290 at the least, so no column runs past
		// the window's edge.
		if (PageColumns == nullptr || PageLeft == nullptr || PageRight == nullptr) {
			return;
		}
		constexpr auto outline      = 290;
		constexpr auto panel        = 360;
		constexpr auto section      = 340;
		constexpr auto outlineFloor = 230;
		constexpr auto panelFloor   = 290;
		constexpr auto give         = (outline - outlineFloor) + (panel - panelFloor);
		// The page asks for no more than its floors; fixed side widths would
		// otherwise make the window's layout hold it wider than the window.
		if (PageColumns->minimumWidth() != section + outlineFloor + panelFloor) {
			PageColumns->setMinimumWidth(section + outlineFloor + panelFloor);
		}
		const auto spare = std::clamp(width() - section - outlineFloor - panelFloor, 0, give);
		const auto left  = outlineFloor + spare * (outline - outlineFloor) / give;
		const auto right = panelFloor + spare - (left - outlineFloor);
		if (PageLeft->minimumWidth() != left || PageLeft->maximumWidth() != left) {
			PageLeft->setFixedWidth(left);
		}
		if (PageRight->minimumWidth() != right || PageRight->maximumWidth() != right) {
			PageRight->setFixedWidth(right);
		}
	}

	void review_tour_editor::set_code_font(const QFont& aFont)
	{
		if (ReaderCodeFont == aFont) {
			return;
		}
		ReaderCodeFont = aFont;
		apply_reader_code_font();
		place_stop_card();
	}

	void review_tour_editor::apply_reader_code_font()
	{
		// [[BPR-1805]] repair (owner review, 2026-10-29). Code in a stop reads
		// as code in a comment does: the profile's code font, by default the
		// default typography's, never the platform's bare fixed font, which an
		// offscreen or unconfigured platform resolves to a small serif.
		const auto code = ReaderCodeFont.value_or(to_presentation_font(default_typography_settings().Code));
		const auto tab  = 4.0 * QFontMetricsF{ code }.horizontalAdvance(QLatin1Char{ ' ' });
		for (auto* const reader : { StopReader, Preview, PublicationPreview }) {
			if (reader != nullptr) {
				reader->set_code_font(code, tab);
			}
		}
	}

	void review_tour_editor::set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		for (auto* const reader : { StopReader, Preview, PublicationPreview }) {
			if (reader != nullptr) {
				reader->set_code_syntax_dispatch(aDispatch);
				reader->request_code_syntax();
			}
		}
	}

	void review_tour_editor::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		// A reader takes an answer only for code it shows.
		for (auto* const reader : { StopReader, Preview, PublicationPreview }) {
			if (reader != nullptr) {
				reader->apply_code_syntax(aSpans);
			}
		}
	}

	void review_tour_editor::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::PaletteChange) {
			Source->setFont(code_font_of(font()));
			Related->setFont(code_font_of(font()));
			apply_reader_code_font();
			place_stop_card();
		}
		// [[BPR-1776]]. More's glyph is drawn in the palette's ink.
		if (aEvent->type() == QEvent::PaletteChange) {
			restyle_working();
			if (auto* const more = qobject_cast<QToolButton*>(MoreButton); more != nullptr) {
				more->setIcon(conversation_icon(conversation_glyph::more, palette()));
			}
			EditStop->setIcon(conversation_icon(conversation_glyph::edit, palette()));
		}
	}

}
