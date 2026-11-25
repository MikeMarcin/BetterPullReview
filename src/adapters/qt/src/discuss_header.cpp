#include "discuss_header.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"

#include "button_surface.hpp"
#include "conversation_open_card.hpp"

#include <QFocusEvent>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QWidgetItem>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The height and corner of Threads' chips ([[BPR-1293]]).
		constexpr int   tool_height     = 30;
		constexpr qreal tool_radius     = 7.0;
		constexpr int   glyph_side      = 16;
		constexpr int   tool_padding    = 9;
		constexpr int   segment_padding = 12;
		constexpr int   inner_gap       = 5;
		constexpr int   bare_side       = 22;
		constexpr int   part_gap        = 14;
		/// A drawer toggle's corner: a chip's, at the drawer's smaller size.
		constexpr qreal drawer_toggle_radius = 6.0;
		/// What an off drawer toggle's glyph reads at on the drawer. Above the
		/// 3:1 a mark is held to by the third a thin stroke loses to
		/// antialiasing, and far enough under the ink that the same glyph on
		/// and off differ by 3:1 in each built-in theme.
		constexpr double dimmed_glyph_contrast = 4.0;
		/// How far an on drawer toggle's face goes from the drawer's ground
		/// toward its accent: further in a dark theme, where the same step
		/// shows less.
		constexpr qreal drawer_toggle_fill      = 0.26;
		constexpr qreal drawer_toggle_fill_dark = 0.40;

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			const auto channel = [aAmount](const int aA, const int aB) {
				return static_cast<int>(std::lround(aA + (aB - aA) * aAmount));
			};
			return QColor{ channel(aFrom.red(), aTo.red()), channel(aFrom.green(), aTo.green()), channel(aFrom.blue(), aTo.blue()) };
		}

		/// \brief `aInk` taken toward `aGround` until it reads there at
		/// `dimmed_glyph_contrast`: an off toggle's glyph. An ink that already
		/// reads below that is kept.
		[[nodiscard]] auto dimmed(const QColor& aInk, const QColor& aGround) -> QColor
		{
			const auto ground = to_appearance_color(aGround);
			auto       result = aInk;
			for (auto hundredths = 1; hundredths < 100; ++hundredths) {
				const auto shade = blend(aInk, aGround, hundredths / 100.0);
				if (contrast_ratio(to_appearance_color(shade), ground) < dimmed_glyph_contrast) {
					break;
				}
				result = shade;
			}
			return result;
		}

		[[nodiscard]] auto weighted(QFont aFont, const QFont::Weight aWeight) -> QFont
		{
			aFont.setWeight(aWeight);
			return aFont;
		}

		[[nodiscard]] auto colours_of(const QWidget& aWidget, const QColor& aNew) -> note_colours
		{
			const auto& palette = aWidget.palette();
			return note_colours_of(palette, aNew.isValid() ? aNew : palette.color(QPalette::Active, QPalette::Highlight), QColor{ });
		}

		/// The signal's soft fill: what is on.
		[[nodiscard]] auto signal_soft(const note_colours& aColours) -> QColor
		{
			return blend(aColours.Signal, aColours.Panel, aColours.Dark ? 0.72 : 0.86);
		}

		[[nodiscard]] auto signal_edge(const note_colours& aColours) -> QColor
		{
			return blend(aColours.Signal, aColours.Rule, 0.45);
		}

		/// A chip's rounded box, or a segment's: rounded only on its outer
		/// side, the first segment's left and the last one's right.
		[[nodiscard]] auto segment_path(const QRectF& aBox, const discuss_tool_shape aShape) -> QPainterPath
		{
			auto path = QPainterPath{ };
			path.addRoundedRect(aBox, tool_radius, tool_radius);
			if (aShape == discuss_tool_shape::segment_first) {
				auto square = QPainterPath{ };
				square.addRect(QRectF{ aBox.center().x(), aBox.top(), aBox.width() / 2.0, aBox.height() });
				path = path.united(square);
			} else if (aShape == discuss_tool_shape::segment_last) {
				auto square = QPainterPath{ };
				square.addRect(QRectF{ aBox.left(), aBox.top(), aBox.width() / 2.0, aBox.height() });
				path = path.united(square);
			}
			return path.simplified();
		}
	} // namespace

	discuss_tool::discuss_tool(QWidget* const aParent)
		: QToolButton{ aParent }
	{
		setAttribute(Qt::WA_Hover, true);
	}

	void discuss_tool::set_shape(const discuss_tool_shape aShape)
	{
		if (Shape == aShape) {
			return;
		}
		Shape = aShape;
		setCursor(Shape == discuss_tool_shape::native ? Qt::ArrowCursor : Qt::PointingHandCursor);
		updateGeometry();
		update();
	}

	void discuss_tool::set_glyph(const std::optional<conversation_glyph> aGlyph)
	{
		Glyph = aGlyph;
		updateGeometry();
		update();
	}

	void discuss_tool::set_count(const QString& aCount, const bool aNew)
	{
		if (Count == aCount && CountIsNew == aNew) {
			return;
		}
		Count      = aCount;
		CountIsNew = aNew;
		updateGeometry();
		update();
	}

	void discuss_tool::set_new_ink(const QColor& aInk)
	{
		if (NewInk != aInk) {
			NewInk = aInk;
			update();
		}
	}

	void discuss_tool::set_toggle_tip(const QString& aTip)
	{
		ToggleTip = aTip;
		tell_toggle_state();
	}

	void discuss_tool::checkStateSet()
	{
		QToolButton::checkStateSet();
		tell_toggle_state();
	}

	void discuss_tool::tell_toggle_state()
	{
		if (!ToggleTip.isEmpty()) {
			setToolTip(ToggleTip + (isChecked() ? QStringLiteral(": on") : QStringLiteral(": off")));
		}
	}

	auto discuss_tool::sizeHint() const -> QSize
	{
		if (Shape == discuss_tool_shape::native) {
			// The style's icon tool, squared: the box is drawn to its edge.
			const auto tool = QToolButton::sizeHint();
			const auto side = std::max(tool.width(), tool.height());
			return QSize{ side, side };
		}
		if (Shape == discuss_tool_shape::bare) {
			return QSize{ bare_side, bare_side };
		}
		const auto words = toolButtonStyle() != Qt::ToolButtonIconOnly && !text().isEmpty();
		const auto glyph = Glyph.has_value() && toolButtonStyle() != Qt::ToolButtonTextOnly;
		// A segment's words get a little more room than a chip's.
		const auto padding = Shape == discuss_tool_shape::chip ? tool_padding : segment_padding;
		auto       width   = padding;
		if (glyph) {
			width += glyph_side;
		}
		if (words) {
			// A segment is as wide as its words in semibold, the weight they
			// take when chosen, so choosing one moves nothing.
			const auto face = Shape == discuss_tool_shape::chip ? font() : weighted(font(), QFont::DemiBold);
			width += (glyph ? inner_gap : 0) + QFontMetrics{ face }.horizontalAdvance(text());
		}
		if (!Count.isEmpty()) {
			width += (glyph || words ? inner_gap : 0) + QFontMetrics{ weighted(font(), QFont::DemiBold) }.horizontalAdvance(Count);
		}
		width += padding;
		return QSize{ std::max(width, tool_height), tool_height };
	}

	void discuss_tool::focusInEvent(QFocusEvent* const aEvent)
	{
		const auto reason = aEvent->reason();
		KeyboardFocus     = reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason || reason == Qt::ShortcutFocusReason;
		QToolButton::focusInEvent(aEvent);
	}

	auto discuss_tool::ringed() const -> bool
	{
		return hasFocus() && KeyboardFocus;
	}

	auto discuss_tool::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void discuss_tool::paint_drawer_icon()
	{
		// Discuss's chip at the drawer's size, on the drawer's own ground.
		// Off is an empty hairline box around a dimmed glyph. On fills the
		// box toward the accent inside an accent edge, with the glyph in the
		// whole ink. The owner, 2026-11-24: "it's not obvious when its on or
		// off still, there is a dimmed version for off from discuss look to
		// that for inspiration". The face before had no off look of its own
		// (the bare glyph of the arrows beside it), and its on fill was mixed
		// toward the cards' base, a step from the drawer's ground too small
		// to see. The platform style is not asked: outside a toolbar the
		// macOS style draws a square bezel under every tool button, checked
		// or not.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto colours = button_colours_on(palette(), QPalette::Window);
		const auto box     = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		if (!isEnabled()) {
			// A filter that cannot be used is a bare glyph, as a disabled
			// arrow is: only a toggle that can be turned has its box.
			paint_button_glyph(painter, *this, iconSize(), colours.DisabledInk);
			return;
		}
		const auto pressed = isDown();
		const auto hovered = underMouse();
		auto       fill    = QColor{ Qt::transparent };
		auto       edge    = blend(colours.Ground, colours.Ink, 0.16);
		auto       glyph   = dimmed(colours.Ink, colours.Ground);
		if (isChecked()) {
			// The focus ring's colour is the accent held to 3:1 on this
			// ground, so the edge alone marks the state.
			const auto base = colours.Ground.lightness() < 128 ? drawer_toggle_fill_dark : drawer_toggle_fill;
			const auto lift = pressed ? 0.12 : (hovered ? 0.06 : 0.0);
			fill            = blend(colours.Ground, colours.Focus, base + lift);
			edge            = colours.Focus;
			glyph           = colours.Ink;
		} else if (pressed || hovered) {
			// Under the pointer an off toggle answers in the ink, not the
			// accent: it is still off.
			fill  = blend(colours.Ground, colours.Ink, pressed ? 0.10 : 0.045);
			edge  = blend(colours.Ground, colours.Ink, 0.26);
			glyph = colours.Ink;
		}
		painter.setPen(QPen{ edge, 1.0 });
		painter.setBrush(fill);
		painter.drawRoundedRect(box, drawer_toggle_radius, drawer_toggle_radius);
		if (ringed()) {
			const auto inset = (focus_ring_width - 1.0) / 2.0;
			painter.setPen(QPen{ colours.Focus, focus_ring_width });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box.adjusted(inset, inset, -inset, -inset), drawer_toggle_radius, drawer_toggle_radius);
		}
		paint_button_glyph(painter, *this, iconSize(), glyph);
	}

	void discuss_tool::paintEvent(QPaintEvent* /*aEvent*/)
	{
		if (Shape == discuss_tool_shape::native) {
			paint_drawer_icon();
			return;
		}
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto colours = colours_of(*this, NewInk);
		const auto hovered = underMouse() && isEnabled();
		const auto on      = isChecked() || isDown();
		const auto box     = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		if (Shape == discuss_tool_shape::bare) {
			if (hovered || ringed()) {
				painter.setPen(ringed() ? QPen{ colours.Signal, 1.5 } : Qt::NoPen);
				painter.setBrush(hovered ? colours.Hover : Qt::transparent);
				painter.drawEllipse(box.adjusted(1.0, 1.0, -1.0, -1.0));
			}
			if (Glyph) {
				const auto side = 14.0;
				paint_conversation_glyph(painter, QRectF{ (width() - side) / 2.0, (height() - side) / 2.0, side, side }, *Glyph, hovered ? colours.Ink : colours.Ink2);
			}
			return;
		}
		const auto path = segment_path(box, Shape);
		painter.setPen(Qt::NoPen);
		painter.setBrush(on ? signal_soft(colours) : hovered ? colours.Hover
															 : colours.Panel);
		painter.drawPath(path);
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen{ on ? signal_edge(colours) : hovered ? colours.Rule2
																 : colours.Rule,
							 1.0 });
		// Two segments share one edge: the first one's.
		if (Shape == discuss_tool_shape::segment_last) {
			painter.save();
			painter.setClipRect(rect().adjusted(1, 0, 0, 0));
			painter.drawPath(path);
			painter.restore();
		} else {
			painter.drawPath(path);
		}
		if (ringed()) {
			painter.setPen(QPen{ colours.Signal, 2.0 });
			painter.drawPath(segment_path(box.adjusted(1.0, 1.0, -1.0, -1.0), Shape));
		}
		const auto ink   = !isEnabled() ? colours.Faint : on || hovered ? colours.Ink
																		: colours.Ink2;
		const auto words = toolButtonStyle() != Qt::ToolButtonIconOnly && !text().isEmpty();
		const auto glyph = Glyph.has_value() && toolButtonStyle() != Qt::ToolButtonTextOnly;
		auto       x     = Shape == discuss_tool_shape::chip ? tool_padding : segment_padding;
		if (Shape != discuss_tool_shape::chip && words && !glyph && Count.isEmpty()) {
			// A segment's words are centred, so both halves read evenly.
			const auto face = isChecked() ? weighted(font(), QFont::DemiBold) : font();
			x               = (width() - QFontMetrics{ face }.horizontalAdvance(text())) / 2;
		}
		if (glyph) {
			paint_conversation_glyph(painter, QRectF{ static_cast<qreal>(x), (height() - glyph_side) / 2.0, static_cast<qreal>(glyph_side), static_cast<qreal>(glyph_side) }, *Glyph, ink);
			x += glyph_side;
		}
		if (words) {
			x += glyph ? inner_gap : 0;
			const auto face = Shape != discuss_tool_shape::chip && isChecked() ? weighted(font(), QFont::DemiBold) : font();
			painter.setFont(face);
			painter.setPen(ink);
			const auto advance = QFontMetrics{ face }.horizontalAdvance(text());
			painter.drawText(QRect{ x, 0, advance + 1, height() }, Qt::AlignLeft | Qt::AlignVCenter, text());
			x += advance;
		}
		if (!Count.isEmpty()) {
			x += glyph || words ? inner_gap : 0;
			const auto bold = weighted(font(), QFont::DemiBold);
			painter.setFont(bold);
			painter.setPen(CountIsNew && isEnabled() ? colours.New : ink);
			painter.drawText(QRect{ x, 0, QFontMetrics{ bold }.horizontalAdvance(Count) + 1, height() }, Qt::AlignLeft | Qt::AlignVCenter, Count);
		}
	}

	discuss_counts::discuss_counts(QWidget* const aParent)
		: QLabel{ aParent }
	{
		setTextFormat(Qt::PlainText);
	}

	void discuss_counts::set_parts(std::vector<discuss_count_part> aParts)
	{
		Parts = std::move(aParts);
		updateGeometry();
		update();
	}

	auto discuss_counts::sizeHint() const -> QSize
	{
		const auto number = weighted(font(), QFont::DemiBold);
		auto       width  = 0;
		for (auto index = std::size_t{ 0U }; index < Parts.size(); ++index) {
			width += (index == 0U ? 0 : part_gap) + QFontMetrics{ number }.horizontalAdvance(Parts[index].Number)
				+ QFontMetrics{ font() }.horizontalAdvance(QLatin1Char{ ' ' } + Parts[index].Words);
		}
		const auto margins = contentsMargins();
		return QSize{ width + margins.left() + margins.right() + 1, std::max(QFontMetrics{ number }.height() + margins.top() + margins.bottom(), tool_height) };
	}

	auto discuss_counts::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void discuss_counts::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto colours = colours_of(*this, QColor{ });
		const auto area    = contentsRect();
		const auto number  = weighted(font(), QFont::DemiBold);
		const auto metrics = QFontMetrics{ number };
		const auto line    = area.top() + (area.height() + metrics.ascent() - metrics.descent()) / 2;
		auto       x       = area.left();
		for (auto index = std::size_t{ 0U }; index < Parts.size(); ++index) {
			const auto& part = Parts[index];
			x += index == 0U ? 0 : part_gap;
			painter.setFont(number);
			painter.setPen(colours.Ink);
			painter.drawText(QPoint{ x, line }, part.Number);
			x += metrics.horizontalAdvance(part.Number);
			painter.setFont(font());
			painter.setPen(colours.Ink2);
			const auto spaced = QLatin1Char{ ' ' } + part.Words;
			painter.drawText(QPoint{ x, line }, spaced);
			x += QFontMetrics{ font() }.horizontalAdvance(spaced);
		}
	}

	discuss_pill::discuss_pill(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setMinimumHeight(tool_height);
	}

	void discuss_pill::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto colours = colours_of(*this, QColor{ });
		painter.setPen(QPen{ signal_edge(colours), 1.0 });
		painter.setBrush(signal_soft(colours));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), tool_radius, tool_radius);
	}

	discuss_band::discuss_band(QWidget* const aParent)
		: QWidget{ aParent }
	{
	}

	void discuss_band::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto colours = colours_of(*this, QColor{ });
		painter.fillRect(rect(), colours.Panel);
		painter.fillRect(QRect{ 0, height() - 1, width(), 1 }, colours.Rule);
	}

	discuss_flow_layout::discuss_flow_layout(QWidget* const aParent)
		: QLayout{ aParent }
	{
	}

	discuss_flow_layout::~discuss_flow_layout()
	{
		while (auto* const item = takeAt(0)) {
			delete item;
		}
	}

	void discuss_flow_layout::add_trailing(QWidget* const aWidget)
	{
		addChildWidget(aWidget);
		Trailing = new QWidgetItem{ aWidget };
		Items.push_back(Trailing);
		invalidate();
	}

	void discuss_flow_layout::addItem(QLayoutItem* const aItem)
	{
		// The trailing item stays last.
		if (Trailing != nullptr) {
			Items.insert(Items.size() - 1, aItem);
		} else {
			Items.push_back(aItem);
		}
	}

	auto discuss_flow_layout::count() const -> int
	{
		return static_cast<int>(Items.size());
	}

	auto discuss_flow_layout::itemAt(const int aIndex) const -> QLayoutItem*
	{
		return aIndex >= 0 && aIndex < Items.size() ? Items[aIndex] : nullptr;
	}

	auto discuss_flow_layout::takeAt(const int aIndex) -> QLayoutItem*
	{
		if (aIndex < 0 || aIndex >= Items.size()) {
			return nullptr;
		}
		auto* const item = Items.takeAt(aIndex);
		if (item == Trailing) {
			Trailing = nullptr;
		}
		return item;
	}

	auto discuss_flow_layout::expandingDirections() const -> Qt::Orientations
	{
		return { };
	}

	auto discuss_flow_layout::hasHeightForWidth() const -> bool
	{
		return true;
	}

	auto discuss_flow_layout::heightForWidth(const int aWidth) const -> int
	{
		return arrange(QRect{ 0, 0, aWidth, 0 }, false);
	}

	auto discuss_flow_layout::sizeHint() const -> QSize
	{
		// Everything on one row.
		auto       width  = 0;
		auto       height = 0;
		auto       first  = true;
		const auto gap    = spacing();
		for (auto* const item : Items) {
			if (item->isEmpty()) {
				continue;
			}
			const auto size = item->sizeHint();
			width += (first ? 0 : gap) + size.width();
			height = std::max(height, size.height());
			first  = false;
		}
		const auto margins = contentsMargins();
		return QSize{ width + margins.left() + margins.right(), height + margins.top() + margins.bottom() };
	}

	auto discuss_flow_layout::minimumSize() const -> QSize
	{
		auto width = 0;
		for (auto* const item : Items) {
			if (!item->isEmpty()) {
				width = std::max(width, item->minimumSize().width());
			}
		}
		const auto margins = contentsMargins();
		return QSize{ width + margins.left() + margins.right(), heightForWidth(width + margins.left() + margins.right()) };
	}

	void discuss_flow_layout::setGeometry(const QRect& aRect)
	{
		QLayout::setGeometry(aRect);
		arrange(aRect, true);
	}

	auto discuss_flow_layout::arrange(const QRect& aRect, const bool aApply) const -> int
	{
		const auto margins = contentsMargins();
		const auto area    = aRect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
		const auto gap     = spacing();
		struct placed
		{
			QLayoutItem* Item;
			QSize        Size;
			int          X;
		};
		auto rows = std::vector<std::vector<placed>>{ { } };
		auto x    = area.left();
		for (auto* const item : Items) {
			if (item->isEmpty()) {
				continue;
			}
			auto       size  = item->sizeHint();
			const auto lower = item->minimumSize().width();
			size.setWidth(std::max(lower, std::min(size.width(), area.width())));
			const auto start = rows.back().empty() ? area.left() : x + gap;
			// [[BPR-1089]]. A drill-down pill shrinks rather than push the
			// counts and Clear off the band's row: the item before the
			// trailing one gives up width down to its minimum.
			if (item == Trailing && !rows.back().empty() && start + size.width() > area.right() + 1) {
				auto&      last   = rows.back().back();
				const auto excess = start + size.width() - (area.right() + 1);
				const auto spare  = last.Size.width() - last.Item->minimumSize().width();
				if (spare >= excess) {
					last.Size.setWidth(last.Size.width() - excess);
					x -= excess;
				}
			}
			const auto shrunkStart = rows.back().empty() ? area.left() : x + gap;
			if (!rows.back().empty() && shrunkStart + size.width() > area.right() + 1) {
				rows.emplace_back();
				x = area.left();
			}
			auto at = rows.back().empty() ? area.left() : x + gap;
			if (item == Trailing) {
				at = std::max(at, area.right() + 1 - size.width());
			}
			rows.back().push_back(placed{ item, size, at });
			x = at + size.width();
		}
		auto y     = area.top();
		auto total = 0;
		for (const auto& row : rows) {
			if (row.empty()) {
				continue;
			}
			auto height = 0;
			for (const auto& entry : row) {
				height = std::max(height, entry.Size.height());
			}
			if (aApply) {
				for (const auto& entry : row) {
					entry.Item->setGeometry(QRect{ QPoint{ entry.X, y + (height - entry.Size.height()) / 2 }, entry.Size });
				}
			}
			total += (total == 0 ? 0 : gap) + height;
			y += height + gap;
		}
		return total + margins.top() + margins.bottom();
	}
} // namespace bpr::qt
