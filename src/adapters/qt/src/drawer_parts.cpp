#include "bpr/qt/drawer_parts.hpp"

#include "bpr/qt/keyboard_focus_ring.hpp"

#include "painted_glyph_icon.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadioButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The open card's edge, in logical pixels: the accepted 3.
		constexpr qreal card_edge = 3.0;
		/// The card's corner radius: the accepted 8.
		constexpr qreal card_radius = 8.0;
		/// The notch's half-height and depth: the accepted 6.
		constexpr qreal notch_size = 6.0;

		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			const auto keep = 1.0 - aShare;
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare),
									static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
									static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
		}

		[[nodiscard]] auto ink_of(const QPalette& aPalette) -> QColor
		{
			return aPalette.color(QPalette::Active, QPalette::WindowText);
		}

		/// A layout's items that are buttons a person presses, in a row.
		[[nodiscard]] auto stacked_buttons(const QLayout& aLayout) -> int
		{
			auto run  = 0;
			auto most = 0;
			for (auto index = 0; index < aLayout.count(); ++index) {
				const auto* const widget  = aLayout.itemAt(index)->widget();
				const auto* const button  = qobject_cast<const QAbstractButton*>(widget);
				const auto        pressed = button != nullptr && !button->isHidden() && qobject_cast<const QCheckBox*>(button) == nullptr
					&& qobject_cast<const QRadioButton*>(button) == nullptr;
				run  = pressed ? run + 1 : (widget == nullptr && aLayout.itemAt(index)->spacerItem() != nullptr ? run : 0);
				most = std::max(most, run);
			}
			return most;
		}

		void collect_layouts(const QLayout& aLayout, QList<const QLayout*>& aOut)
		{
			aOut.push_back(&aLayout);
			for (auto index = 0; index < aLayout.count(); ++index) {
				if (const auto* const child = aLayout.itemAt(index)->layout(); child != nullptr) {
					collect_layouts(*child, aOut);
				}
			}
		}

		[[nodiscard]] auto name_of(const QWidget& aWidget) -> QString
		{
			return aWidget.objectName().isEmpty() ? QString::fromLatin1(aWidget.metaObject()->className()) : aWidget.objectName();
		}

		/// Whether a stack or tab widget is a tab container's page holder.
		[[nodiscard]] auto holds_tab_pages(const QWidget& aWidget) -> bool
		{
			if (qobject_cast<const QTabWidget*>(&aWidget) != nullptr) {
				return true;
			}
			if (qobject_cast<const QStackedWidget*>(&aWidget) == nullptr || aWidget.parentWidget() == nullptr) {
				return false;
			}
			// A drawer's header pattern: a tab bar beside the page stack.
			const auto bars = aWidget.parentWidget()->findChildren<QTabBar*>();
			return std::ranges::any_of(bars, [&aWidget](const QTabBar* aBar) { return !aWidget.isAncestorOf(aBar); });
		}
	}

	drawer_count_pill::drawer_count_pill(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setFocusPolicy(Qt::NoFocus);
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
	}

	void drawer_count_pill::set_count(const std::size_t aCount)
	{
		Text = aCount == 0U ? QString{ } : QString::number(aCount);
		setVisible(aCount != 0U);
		updateGeometry();
		update();
	}

	auto drawer_count_pill::text() const -> const QString&
	{
		return Text;
	}

	auto drawer_count_pill::sizeHint() const -> QSize
	{
		const auto height = fontMetrics().height();
		return QSize{ std::max(height, fontMetrics().horizontalAdvance(Text) + height / 2 + 4), height };
	}

	void drawer_count_pill::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(Qt::NoPen);
		painter.setBrush(palette().color(QPalette::Active, QPalette::Highlight));
		const auto box = QRectF{ rect() };
		painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
		painter.setPen(palette().color(QPalette::Active, QPalette::HighlightedText));
		painter.drawText(rect(), Qt::AlignCenter, Text);
	}

	drawer_header::drawer_header(QWidget* const aParent)
		: QWidget{ aParent }
	{
		Grid = new QGridLayout{ this };
		Grid->setContentsMargins(0, 0, 6, 0);
		Grid->setHorizontalSpacing(1);
		Grid->setVerticalSpacing(0);
		Grid->setColumnStretch(1, 1);
	}

	void drawer_header::set_parts(QWidget* const aTabs, QWidget* const aTools)
	{
		Tabs  = aTabs;
		Tools = aTools;
		Grid->addWidget(Tabs, 0, 0, Qt::AlignBottom | Qt::AlignLeft);
		arrange();
	}

	auto drawer_header::wrapped() const noexcept -> bool
	{
		return Wrapped;
	}

	void drawer_header::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		arrange();
	}

	auto drawer_header::event(QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::LayoutRequest || aEvent->type() == QEvent::FontChange) {
			arrange();
		}
		return QWidget::event(aEvent);
	}

	void drawer_header::arrange()
	{
		if (Tabs == nullptr || Tools == nullptr) {
			return;
		}
		const auto wrap = width() > 0 && Tabs->sizeHint().width() + Tools->sizeHint().width() + 8 > width();
		if (wrap == Wrapped && Grid->indexOf(Tools) >= 0) {
			return;
		}
		Wrapped = wrap;
		// Wrapped, the tabs take the whole row.
		Grid->setContentsMargins(0, 0, wrap ? 0 : 6, 0);
		Grid->removeWidget(Tools);
		if (wrap) {
			Grid->addWidget(Tools, 1, 0, 1, 3, Qt::AlignRight | Qt::AlignVCenter);
		} else {
			Grid->addWidget(Tools, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
		}
	}

	void configure_drawer_tabs(QTabBar& aTabs)
	{
		aTabs.setDocumentMode(true);
		aTabs.setExpanding(false);
		aTabs.setDrawBase(true);
		aTabs.setFocusPolicy(Qt::StrongFocus);
	}

	auto make_drawer_tool(QWidget* const aParent, const QIcon& aIcon, const QString& aName, const QString& aObjectName) -> QToolButton*
	{
		auto* const tool = new QToolButton{ aParent };
		tool->setObjectName(aObjectName);
		tool->setIcon(aIcon);
		tool->setToolTip(aName);
		tool->setAccessibleName(aName);
		tool->setAutoRaise(true);
		tool->setFocusPolicy(Qt::StrongFocus);
		tool->setToolButtonStyle(Qt::ToolButtonIconOnly);
		return tool;
	}

	auto drawer_icon(const drawer_glyph aGlyph, const QPalette& aPalette) -> QIcon
	{
		const auto ink      = aPalette.color(QPalette::Active, QPalette::ButtonText);
		const auto disabled = aPalette.color(QPalette::Disabled, QPalette::ButtonText);
		switch (aGlyph) {
		case drawer_glyph::tour:
			return painted_glyph_icon([](QPainter& aPainter, const qreal aSide, const QColor& aInk) {
				aPainter.setPen(QPen{ aInk, aSide / 12.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter.setBrush(Qt::NoBrush);
				const auto unit = aSide / 24.0;
				aPainter.drawEllipse(QPointF{ 6.0 * unit, 18.0 * unit }, 2.2 * unit, 2.2 * unit);
				aPainter.drawEllipse(QPointF{ 18.0 * unit, 6.0 * unit }, 2.2 * unit, 2.2 * unit);
				auto route = QPainterPath{ QPointF{ 8.0 * unit, 17.0 * unit } };
				route.cubicTo(QPointF{ 14.0 * unit, 17.0 * unit }, QPointF{ 10.0 * unit, 8.0 * unit }, QPointF{ 16.0 * unit, 7.0 * unit });
				aPainter.drawPath(route);
			},
									  ink, disabled);
		case drawer_glyph::hide_right:
			return painted_glyph_icon([](QPainter& aPainter, const qreal aSide, const QColor& aInk) {
				aPainter.setPen(QPen{ aInk, aSide / 12.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter.setBrush(Qt::NoBrush);
				const auto unit = aSide / 24.0;
				aPainter.drawRoundedRect(QRectF{ 3.5 * unit, 5.0 * unit, 17.0 * unit, 14.0 * unit }, 2.0 * unit, 2.0 * unit);
				aPainter.drawLine(QPointF{ 14.5 * unit, 5.0 * unit }, QPointF{ 14.5 * unit, 19.0 * unit });
				aPainter.drawLine(QPointF{ 8.0 * unit, 9.5 * unit }, QPointF{ 10.5 * unit, 12.0 * unit });
				aPainter.drawLine(QPointF{ 10.5 * unit, 12.0 * unit }, QPointF{ 8.0 * unit, 14.5 * unit });
			},
									  ink, disabled);
		case drawer_glyph::page:
			return painted_glyph_icon([](QPainter& aPainter, const qreal aSide, const QColor& aInk) {
				aPainter.setPen(QPen{ aInk, aSide / 16.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter.setBrush(Qt::NoBrush);
				const auto unit = aSide / 24.0;
				auto       page = QPainterPath{ QPointF{ 6.0 * unit, 3.5 * unit } };
				page.lineTo(15.0 * unit, 3.5 * unit);
				page.lineTo(18.5 * unit, 7.0 * unit);
				page.lineTo(18.5 * unit, 20.5 * unit);
				page.lineTo(6.0 * unit, 20.5 * unit);
				page.closeSubpath();
				aPainter.drawPath(page);
				for (const auto y : { 11.0, 14.5 }) {
					aPainter.drawLine(QPointF{ 9.0 * unit, y * unit }, QPointF{ 15.0 * unit, y * unit });
				}
				aPainter.drawLine(QPointF{ 9.0 * unit, 18.0 * unit }, QPointF{ 12.5 * unit, 18.0 * unit });
			},
									  ink, disabled);
		}
		return { };
	}

	void paint_drawer_card(QPainter& aPainter, const QRectF& aBox, const QPalette& aPalette, const drawer_card_state aState,
						   const drawer_ownership aOwnership, const drawer_card_colours& aColours, const bool aHovered,
						   const std::optional<qreal> aNotchY)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto base   = aPalette.color(QPalette::Active, QPalette::Base);
		const auto rule   = mixed(aPalette.color(QPalette::Active, QPalette::Window), ink_of(aPalette), 0.16);
		const auto dashed = aOwnership != drawer_ownership::published;
		const auto box    = aBox.adjusted(0.5, 0.5, -0.5, -0.5);
		if (aState == drawer_card_state::closed) {
			if (aHovered) {
				aPainter.setPen(QPen{ rule, 1.0 });
				aPainter.setBrush(base);
				aPainter.drawRoundedRect(box, card_radius, card_radius);
			}
			if (dashed) {
				aPainter.setBrush(Qt::NoBrush);
				aPainter.setPen(QPen{ mixed(aPalette.color(QPalette::Active, QPalette::Window), ink_of(aPalette), 0.32), 1.5, Qt::DashLine });
				aPainter.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), card_radius, card_radius);
			}
			aPainter.restore();
			return;
		}
		const auto edge = aState == drawer_card_state::writing ? aColours.Writing : aColours.Reading;
		// The shadow: two soft offsets of the ink, as the accepted card's.
		for (const auto& [grow, alpha] : { std::pair{ 6.0, 10 }, std::pair{ 2.0, 18 } }) {
			auto shadow = ink_of(aPalette);
			shadow.setAlpha(alpha);
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(shadow);
			aPainter.drawRoundedRect(box.adjusted(-grow / 3.0, grow / 3.0, grow / 3.0, grow), card_radius + grow / 2.0, card_radius + grow / 2.0);
		}
		aPainter.setBrush(base);
		aPainter.setPen(dashed ? QPen{ mixed(base, ink_of(aPalette), 0.32), 1.5, Qt::DashLine } : QPen{ mixed(base, edge, 0.45), 1.0 });
		aPainter.drawRoundedRect(box, card_radius, card_radius);
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(aOwnership == drawer_ownership::private_note ? mixed(base, ink_of(aPalette), 0.5) : edge);
		aPainter.drawRoundedRect(QRectF{ box.left() - 0.5, box.top() + 6.0, card_edge, box.height() - 12.0 }, 1.5, 1.5);
		if (aNotchY) {
			auto notch = QPainterPath{ QPointF{ box.left() - 0.5, *aNotchY - notch_size } };
			notch.lineTo(box.left() - 0.5 - notch_size, *aNotchY);
			notch.lineTo(box.left() - 0.5, *aNotchY + notch_size);
			notch.closeSubpath();
			aPainter.drawPath(notch);
		}
		aPainter.restore();
	}

	drawer_card::drawer_card(QWidget* const aParent)
		: QFrame{ aParent }
	{
		setFrameShape(QFrame::NoFrame);
		setAttribute(Qt::WA_Hover, true);
		setContentsMargins(static_cast<int>(card_edge) + 9, 6, 8, 7);
	}

	void drawer_card::set_state(const drawer_card_state aState)
	{
		State = aState;
		update();
	}

	auto drawer_card::state() const noexcept -> drawer_card_state
	{
		return State;
	}

	void drawer_card::set_ownership(const drawer_ownership aOwnership)
	{
		Ownership = aOwnership;
		update();
	}

	auto drawer_card::ownership() const noexcept -> drawer_ownership
	{
		return Ownership;
	}

	void drawer_card::set_colours(const drawer_card_colours& aColours)
	{
		Colours = aColours;
		update();
	}

	void drawer_card::set_notch(const std::optional<int> aY)
	{
		Notch = aY;
		update();
	}

	void drawer_card::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		paint_drawer_card(painter, QRectF{ rect() }, palette(), State, Ownership, Colours, Hovered,
						  Notch ? std::optional<qreal>{ static_cast<qreal>(*Notch) } : std::nullopt);
	}

	void drawer_card::enterEvent(QEnterEvent* const aEvent)
	{
		Hovered = true;
		update();
		QFrame::enterEvent(aEvent);
	}

	void drawer_card::leaveEvent(QEvent* const aEvent)
	{
		Hovered = false;
		update();
		QFrame::leaveEvent(aEvent);
	}

	drawer_empty_state::drawer_empty_state(const QIcon& aGlyph, const QString& aSentence, QAction* const aAction, QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("drawerEmptyState"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(22, 22, 22, 22);
		column->setSpacing(9);
		column->addStretch(1);
		auto* const glyph = new QLabel{ this };
		glyph->setPixmap(aGlyph.pixmap(28, 28, QIcon::Disabled));
		glyph->setAlignment(Qt::AlignCenter);
		glyph->setAccessibleName(QString{ });
		column->addWidget(glyph, 0, Qt::AlignHCenter);
		Sentence = new QLabel{ aSentence, this };
		Sentence->setObjectName(QStringLiteral("drawerEmptySentence"));
		Sentence->setTextFormat(Qt::PlainText);
		Sentence->setWordWrap(true);
		Sentence->setAlignment(Qt::AlignCenter);
		Sentence->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(Sentence);
		if (aAction != nullptr) {
			Action = new QToolButton{ this };
			Action->setObjectName(QStringLiteral("drawerEmptyAction"));
			Action->setDefaultAction(aAction);
			Action->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			Action->setFocusPolicy(Qt::StrongFocus);
			column->addWidget(Action, 0, Qt::AlignHCenter);
		}
		column->addStretch(1);
		setAccessibleName(aSentence);
	}

	auto drawer_empty_state::sentence() const -> QString
	{
		return Sentence->text();
	}

	auto drawer_empty_state::action_button() const noexcept -> QToolButton*
	{
		return Action;
	}

	// ---- drawer_section_header ---------------------------------------------

	namespace {
		constexpr int   section_chevron = 7;
		constexpr int   section_gap     = 8;
		constexpr int   section_inset   = 4;
		constexpr qreal section_corner  = 4.0;
	} // namespace

	drawer_section_header::drawer_section_header(QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setCheckable(true);
		setAutoRaise(true);
		setArrowType(Qt::RightArrow);
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover, true);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		auto bold = font();
		bold.setWeight(QFont::DemiBold);
		setFont(bold);
		connect(this, &QToolButton::toggled, this, [this](const bool aOpen) {
			setArrowType(aOpen ? Qt::DownArrow : Qt::RightArrow);
			describe();
		});
		describe();
	}

	void drawer_section_header::set_count(const QString& aCount)
	{
		if (Count == aCount) {
			return;
		}
		Count = aCount;
		describe();
		updateGeometry();
		update();
	}

	auto drawer_section_header::count() const -> const QString&
	{
		return Count;
	}

	void drawer_section_header::describe()
	{
		setAccessibleDescription(isChecked() ? QStringLiteral("Expanded; press to hide the details") : QStringLiteral("Collapsed; press to show the details"));
	}

	auto drawer_section_header::sizeHint() const -> QSize
	{
		const auto metrics = fontMetrics();
		const auto count   = Count.isEmpty() ? 0 : metrics.horizontalAdvance(Count) + section_gap;
		return QSize{ section_inset + metrics.horizontalAdvance(text()) + section_gap + count + section_chevron + section_inset, std::max(26, metrics.height() + 10) };
	}

	auto drawer_section_header::minimumSizeHint() const -> QSize
	{
		return QSize{ section_inset + section_chevron + section_inset, sizeHint().height() };
	}

	void drawer_section_header::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  group   = isEnabled() ? QPalette::Active : QPalette::Disabled;
		const auto  frame   = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		if (isEnabled() && (isDown() || underMouse())) {
			auto tint = colours.color(QPalette::Active, QPalette::Mid);
			tint.setAlpha(isDown() ? 90 : 45);
			painter.setPen(Qt::NoPen);
			painter.setBrush(tint);
			painter.drawRoundedRect(frame, section_corner, section_corner);
		}
		const auto ink   = colours.color(group, QPalette::WindowText);
		const auto faint = colours.color(group, QPalette::PlaceholderText);
		// The chevron at the right edge, in the faint ink.
		const auto middle = static_cast<qreal>(height()) / 2.0;
		const auto side   = static_cast<qreal>(section_chevron);
		const auto left   = static_cast<qreal>(width() - section_inset - section_chevron);
		auto       arrow  = QPainterPath{ };
		if (isChecked()) {
			arrow.moveTo(left, middle - (side * 0.3));
			arrow.lineTo(left + side, middle - (side * 0.3));
			arrow.lineTo(left + (side / 2.0), middle + (side * 0.45));
		} else {
			arrow.moveTo(left + (side * 0.15), middle - (side / 2.0));
			arrow.lineTo(left + (side * 0.9), middle);
			arrow.lineTo(left + (side * 0.15), middle + (side / 2.0));
		}
		arrow.closeSubpath();
		painter.setPen(Qt::NoPen);
		painter.setBrush(faint);
		painter.drawPath(arrow);

		auto right = width() - section_inset - section_chevron - section_gap;
		if (!Count.isEmpty()) {
			auto plain = font();
			plain.setWeight(QFont::Normal);
			painter.setFont(plain);
			const auto counted = QFontMetrics{ plain }.horizontalAdvance(Count);
			painter.setPen(faint);
			painter.drawText(QRect{ right - counted, 0, counted, height() }, Qt::AlignRight | Qt::AlignVCenter, Count);
			right -= counted + section_gap;
		}
		painter.setFont(font());
		painter.setPen(ink);
		const auto room = std::max(0, right - section_inset);
		painter.drawText(QRect{ section_inset, 0, room, height() }, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, room));

		if (hasFocus()) {
			constexpr auto ring = focus_ring_width / 2.0;
			painter.setPen(QPen{ focus_ring_colour(colours, QPalette::Window), focus_ring_width });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ rect() }.adjusted(ring, ring, -ring, -ring), section_corner - ring, section_corner - ring);
		}
	}

	// ---- drawer_meter ------------------------------------------------------

	drawer_meter::drawer_meter(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	void drawer_meter::set_segments(std::vector<std::pair<double, QColor>> aSegments, const double aTotal)
	{
		Segments = std::move(aSegments);
		Total    = aTotal;
		update();
	}

	auto drawer_meter::segments() const -> const std::vector<std::pair<double, QColor>>&
	{
		return Segments;
	}

	auto drawer_meter::sizeHint() const -> QSize
	{
		return QSize{ 120, 6 };
	}

	auto drawer_meter::minimumSizeHint() const -> QSize
	{
		return QSize{ 24, 6 };
	}

	void drawer_meter::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto bar   = QRectF{ 0.0, (height() - 6.0) / 2.0, static_cast<qreal>(width()), 6.0 };
		auto       track = QPainterPath{ };
		track.addRoundedRect(bar, 3.0, 3.0);
		painter.setPen(Qt::NoPen);
		painter.fillPath(track, palette().color(QPalette::Active, QPalette::Mid));
		if (Total <= 0.0) {
			return;
		}
		painter.setClipPath(track);
		auto left = bar.left();
		for (const auto& [weight, colour] : Segments) {
			const auto span = bar.width() * std::clamp(weight / Total, 0.0, 1.0);
			painter.fillRect(QRectF{ left, bar.top(), span, bar.height() }, colour);
			left += span;
		}
	}

	auto drawer_principle_violations(const QWidget& aRoot, const drawer_principles& aRules) -> QStringList
	{
		auto       result = QStringList{ };
		const auto check  = [&aRoot, &aRules, &result](const QWidget* const aWidget) {
			if (aRules.StyleSheets && !aWidget->styleSheet().isEmpty()) {
				result.push_back(QStringLiteral("%1 has a style sheet").arg(name_of(*aWidget)));
			}
			if (aRules.NestedTabs) {
				const auto* const bar    = qobject_cast<const QTabBar*>(aWidget);
				const auto        tabbed = qobject_cast<const QTabWidget*>(aWidget) != nullptr
					|| (bar != nullptr && qobject_cast<const QTabWidget*>(bar->parentWidget()) == nullptr);
				if (tabbed) {
					for (auto* ancestor = aWidget->parentWidget(); ancestor != nullptr && ancestor != &aRoot; ancestor = ancestor->parentWidget()) {
						if (holds_tab_pages(*ancestor)) {
							result.push_back(QStringLiteral("%1 is a tab row inside another tab's page").arg(name_of(*aWidget)));
							break;
						}
					}
				}
			}
			if (aRules.EmptyValueRows) {
				// Drawn means not hidden by itself or by anything up to the root.
				if (const auto* const label = qobject_cast<const QLabel*>(aWidget); label != nullptr && (label == &aRoot ? !label->isHidden() : label->isVisibleTo(&aRoot))) {
					const auto text = label->text().trimmed();
					if (text == QStringLiteral("0") || text == QStringLiteral("—") || text == QStringLiteral("-")
						|| text.compare(QStringLiteral("not available"), Qt::CaseInsensitive) == 0) {
						result.push_back(QStringLiteral("%1 shows an empty value \"%2\"").arg(name_of(*aWidget), text));
					}
				}
			}
			if (aRules.StackedButtons && aWidget->layout() != nullptr) {
				auto layouts = QList<const QLayout*>{ };
				collect_layouts(*aWidget->layout(), layouts);
				for (const auto* const layout : layouts) {
					const auto* const box = qobject_cast<const QBoxLayout*>(layout);
					if (box != nullptr && (box->direction() == QBoxLayout::TopToBottom || box->direction() == QBoxLayout::BottomToTop)
						&& stacked_buttons(*box) >= 3) {
						result.push_back(QStringLiteral("%1 stacks three or more buttons in a column").arg(name_of(*aWidget)));
					}
				}
			}
		};
		check(&aRoot);
		for (const auto* const widget : aRoot.findChildren<QWidget*>()) {
			check(widget);
		}
		result.removeDuplicates();
		return result;
	}
} // namespace bpr::qt
