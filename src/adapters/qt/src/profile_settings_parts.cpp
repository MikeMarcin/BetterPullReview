#include "profile_settings_parts.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QGridLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTreeView>

#include <algorithm>
#include <array>

namespace bpr::qt::detail {
	namespace {
		constexpr auto secondary_scale = 0.92;
		constexpr auto tag_scale       = 0.85;
		constexpr auto tint_property   = "bprComparisonTint";

		[[nodiscard]] auto with_alpha(QColor aColour, const qreal aAlpha) -> QColor
		{
			aColour.setAlphaF(static_cast<float>(aAlpha));
			return aColour;
		}

		/// `aOver` at `aOver`'s alpha laid on the opaque `aGround`.
		[[nodiscard]] auto composite(const QColor& aGround, const QColor& aOver) -> QColor
		{
			const auto alpha = aOver.alphaF();
			const auto mix   = [alpha](const float aBelow, const float aAbove) { return aBelow + (aAbove - aBelow) * alpha; };
			return QColor::fromRgbF(mix(aGround.redF(), aOver.redF()), mix(aGround.greenF(), aOver.greenF()), mix(aGround.blueF(), aOver.blueF()));
		}

		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Active, QPalette::Window).lightnessF() < 0.5; }

		[[nodiscard]] auto tone_role(const tag_tone aTone) -> core::appearance_role
		{
			switch (aTone) {
			case tag_tone::fresh:   return core::appearance_role::state_unread;
			case tag_tone::updated: return core::appearance_role::state_verified;
			case tag_tone::caution: return core::appearance_role::state_attention;
			case tag_tone::quiet:   break;
			}
			return core::appearance_role::state_stale;
		}

		/// The ground a widget's tag sits on: a well's panel, else the window.
		[[nodiscard]] auto ground_of(const QWidget& aWidget) -> QColor
		{
			for (const auto* parent = aWidget.parentWidget(); parent != nullptr && !parent->isWindow(); parent = parent->parentWidget()) {
				if (dynamic_cast<const rounded_well*>(parent) != nullptr || dynamic_cast<const comparison_table*>(parent) != nullptr) {
					return aWidget.palette().color(QPalette::AlternateBase);
				}
			}
			return aWidget.palette().color(QPalette::Window);
		}

		[[nodiscard]] auto tag_face(const QFont& aFont) -> QFont
		{
			auto small = aFont;
			if (small.pointSizeF() > 0.0) {
				small.setPointSizeF(small.pointSizeF() * tag_scale);
			} else if (small.pixelSize() > 0) {
				small.setPixelSize(std::max(1, static_cast<int>(std::lround(small.pixelSize() * tag_scale))));
			}
			return small;
		}

		/// Where a row's parts go at `aWidth`.
		struct row_geometry
		{
			/// A section's disclosure triangle; empty where it has none.
			QRect   Disclosure;
			QRect   Check;
			QPointF Mark;
			QRect   Name;
			QPointF Tag;
			QRect   Second;
			int     Height = 0;
		};

		constexpr auto row_inset_x      = 10;
		constexpr auto row_inset_top    = 6;
		constexpr auto row_inset_bottom = 7;
		constexpr auto heading_air      = 14;
		constexpr auto heading_below    = 4;
		constexpr auto sparkle_side     = 13;
		/// The disclosure triangle's box, and the room a section row gives it.
		constexpr auto disclosure_side  = 12;
		constexpr auto disclosure_width = 16;

		[[nodiscard]] auto name_font(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QFont
		{
			auto font = aOption.font;
			font.setWeight(aIndex.data(row_strong_role).toBool() ? QFont::DemiBold : QFont::Medium);
			return font;
		}

		[[nodiscard]] auto row_name(const QModelIndex& aIndex) -> QString
		{
			const auto named = aIndex.data(row_name_role);
			return named.isValid() ? named.toString() : aIndex.data(Qt::DisplayRole).toString();
		}

		[[nodiscard]] auto row_second(const QModelIndex& aIndex) -> QString
		{
			const auto second = aIndex.data(row_second_role);
			if (second.isValid()) {
				return second.toString();
			}
			// A tree's second column is the row's second line.
			return aIndex.siblingAtColumn(1).isValid() ? aIndex.siblingAtColumn(1).data(Qt::DisplayRole).toString() : QString{ };
		}

		/// [[BPR-2204]] (owner, look profile-settings-dialogs-v001: "It might be
		/// nice if the tree branches on the left were collapsable"). A tree whose
		/// items expand gives each section row a disclosure triangle before its
		/// tick; every section keeps the triangle's room so their ticks line up.
		[[nodiscard]] auto disclosure_room(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> bool
		{
			const auto* const tree = qobject_cast<const QTreeView*>(aOption.widget);
			return tree != nullptr && tree->itemsExpandable() && !aIndex.parent().isValid();
		}

		[[nodiscard]] auto lay_out_row(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex, const QRect& aRect) -> row_geometry
		{
			auto        geometry = row_geometry{ };
			const auto* widget   = aOption.widget;
			auto* const style    = widget != nullptr ? widget->style() : QApplication::style();
			const auto  nameFace = name_font(aOption, aIndex);
			const auto  names    = QFontMetrics{ nameFace };
			const auto  seconds  = QFontMetrics{ secondary_font(aOption.font) };
			const auto  lineH    = names.height();
			auto        left     = aRect.left() + row_inset_x;
			// A section's own row opens with more room above it.
			const auto top = aRect.top() + row_inset_top + (aIndex.data(row_strong_role).toBool() ? 8 : 0);
			if (disclosure_room(aOption, aIndex)) {
				if (aIndex.model()->hasChildren(aIndex)) {
					geometry.Disclosure = QRect{ left - 2, top + (lineH - disclosure_side) / 2, disclosure_side, disclosure_side };
				}
				left += disclosure_width;
			}
			const auto right    = aRect.right() - row_inset_x;
			const auto tickW    = style->pixelMetric(QStyle::PM_IndicatorWidth, &aOption, widget);
			const auto tickH    = style->pixelMetric(QStyle::PM_IndicatorHeight, &aOption, widget);
			geometry.Check      = QRect{ left, top + (lineH - tickH) / 2, tickW, tickH };
			const auto textLeft = left + tickW + 7;
			auto       x        = textLeft;
			const auto mark     = static_cast<row_mark>(aIndex.data(row_mark_role).toInt());
			if (mark == row_mark::agent) {
				geometry.Mark = QPointF{ static_cast<qreal>(x), top + (lineH - sparkle_side) / 2.0 };
				x += sparkle_side + 5;
			}
			const auto tag     = aIndex.data(row_tag_role).toString();
			const auto tagSize = tag.isEmpty() ? QSizeF{ } : tag_size(aOption.font, tag);
			const auto tagRoom = tag.isEmpty() ? 0 : static_cast<int>(std::ceil(tagSize.width())) + 6;
			const auto name    = row_name(aIndex);
			const auto room    = std::max(right - x - tagRoom, 20);
			geometry.Name      = names.boundingRect(QRect{ x, top, room, 100'000 }, Qt::TextWordWrap, name);
			geometry.Name.setWidth(room);
			if (!tag.isEmpty()) {
				const auto oneLine = geometry.Name.height() <= lineH;
				const auto tagX    = oneLine ? x + names.horizontalAdvance(name) + 6 : right - tagSize.width();
				geometry.Tag       = QPointF{ static_cast<qreal>(tagX), top + (lineH - tagSize.height()) / 2.0 };
			}
			auto       bottom = geometry.Name.bottom();
			const auto second = row_second(aIndex);
			if (!second.isEmpty()) {
				geometry.Second = seconds.boundingRect(QRect{ textLeft, bottom + 2, std::max(right - textLeft, 20), 100'000 }, Qt::TextWordWrap, second);
				geometry.Second.setWidth(std::max(right - textLeft, 20));
				bottom = geometry.Second.bottom();
			}
			geometry.Height = bottom - aRect.top() + row_inset_bottom + 1;
			return geometry;
		}

		/// A row's tick. A section whose items include one with no tick of
		/// its own (a prompt with unsaved edits) has no state from the tree:
		/// it shows what its ticked items say.
		[[nodiscard]] auto shown_check_state(const QModelIndex& aIndex) -> Qt::CheckState
		{
			const auto own = aIndex.data(Qt::CheckStateRole);
			if (own.isValid() || !aIndex.model()->hasChildren(aIndex)) {
				return static_cast<Qt::CheckState>(own.toInt());
			}
			auto on  = 0;
			auto off = 0;
			for (auto row = 0; row < aIndex.model()->rowCount(aIndex); ++row) {
				const auto child = aIndex.model()->index(row, 0, aIndex).data(Qt::CheckStateRole);
				if (child.isValid()) {
					(static_cast<Qt::CheckState>(child.toInt()) == Qt::Unchecked ? off : on) += 1;
				}
			}
			return on == 0 ? Qt::Unchecked : off == 0 ? Qt::Checked
													  : Qt::PartiallyChecked;
		}

		/// The width a row is laid out at: the view's, less a tree's indent.
		[[nodiscard]] auto row_width(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> int
		{
			const auto* const view = qobject_cast<const QAbstractItemView*>(aOption.widget);
			if (view == nullptr) {
				return std::max(aOption.rect.width(), 120);
			}
			auto width = view->viewport()->width();
			if (const auto* const tree = qobject_cast<const QTreeView*>(view); tree != nullptr) {
				auto depth = tree->rootIsDecorated() ? 1 : 0;
				for (auto parent = aIndex.parent(); parent.isValid(); parent = parent.parent()) {
					++depth;
				}
				width -= depth * tree->indentation();
			}
			return std::max(width, 120);
		}
	} // namespace

	auto role_colour(const QPalette& aPalette, const core::appearance_role aRole) -> QColor
	{
		const auto window = aPalette.color(QPalette::Active, QPalette::Window).rgb();
		for (const auto variant : { core::appearance_variant::light, core::appearance_variant::dark, core::appearance_variant::high_contrast_light, core::appearance_variant::high_contrast_dark }) {
			const auto& palette = builtin_appearance_palette(variant);
			if (to_presentation_color(palette[core::index_of(core::appearance_role::application_surface)]).rgb() == window) {
				return to_presentation_color(palette[core::index_of(aRole)]);
			}
		}
		return to_presentation_color(builtin_appearance_palette(dark(aPalette) ? core::appearance_variant::dark : core::appearance_variant::light)[core::index_of(aRole)]);
	}

	auto tag_wash(const QPalette& aPalette, const tag_tone aTone) -> QColor
	{
		const auto night = dark(aPalette);
		switch (aTone) {
		case tag_tone::quiet:   return with_alpha(aPalette.color(QPalette::Text), night ? 0.09 : 0.06);
		case tag_tone::fresh:   return with_alpha(role_colour(aPalette, tone_role(aTone)), night ? 0.24 : 0.13);
		case tag_tone::updated: return with_alpha(role_colour(aPalette, tone_role(aTone)), night ? 0.14 : 0.11);
		case tag_tone::caution: return with_alpha(role_colour(aPalette, tone_role(aTone)), night ? 0.17 : 0.14);
		}
		return { };
	}

	auto tag_ink(const QPalette& aPalette, const tag_tone aTone, const QColor& aGround) -> QColor
	{
		if (aTone == tag_tone::quiet) {
			return aPalette.color(QPalette::PlaceholderText);
		}
		// The ground holds the text floor; the wash under the words, the emphasis floor.
		const auto washed  = composite(aGround, tag_wash(aPalette, aTone));
		const auto grounds = std::array{ core::contrast_ground{ .Colour = to_appearance_color(aGround) },
										 core::contrast_ground{ .Colour = to_appearance_color(washed), .MinimumHundredths = core::emphasis_text_contrast_minimum_hundredths } };
		return to_presentation_color(core::readable_signal_ink(to_appearance_color(role_colour(aPalette, tone_role(aTone))), grounds));
	}

	auto refused_ink(const QPalette& aPalette, const QColor& aGround) -> QColor
	{
		const auto grounds = std::array{ core::contrast_ground{ .Colour = to_appearance_color(aGround), .MinimumHundredths = core::indicator_contrast_minimum_hundredths } };
		return to_presentation_color(core::readable_signal_ink(to_appearance_color(role_colour(aPalette, core::appearance_role::state_defect)), grounds));
	}

	auto secondary_font(const QFont& aFont) -> QFont
	{
		auto font = aFont;
		if (font.pointSizeF() > 0.0) {
			font.setPointSizeF(font.pointSizeF() * secondary_scale);
		} else if (font.pixelSize() > 0) {
			font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * secondary_scale))));
		}
		return font;
	}

	auto quiet_selection(const QPalette& aPalette) -> QColor { return with_alpha(aPalette.color(QPalette::Text), dark(aPalette) ? 0.14 : 0.11); }

	auto hairline(const QPalette& aPalette) -> QColor { return with_alpha(aPalette.color(QPalette::Text), dark(aPalette) ? 0.11 : 0.10); }

	auto well_edge(const QPalette& aPalette) -> QColor { return with_alpha(aPalette.color(QPalette::Text), dark(aPalette) ? 0.12 : 0.15); }

	auto tag_size(const QFont& aFont, const QString& aText) -> QSizeF
	{
		const auto metrics = QFontMetricsF{ tag_face(aFont) };
		return { metrics.horizontalAdvance(aText) + 12.0, std::ceil(metrics.height()) + 3.0 };
	}

	auto paint_tag(QPainter& aPainter, const QPointF& aTopLeft, const QString& aText, const tag_tone aTone, const QPalette& aPalette, const QFont& aFont, const QColor& aGround) -> qreal
	{
		const auto size = tag_size(aFont, aText);
		const auto box  = QRectF{ aTopLeft, size };
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(tag_wash(aPalette, aTone));
		aPainter.drawRoundedRect(box, 4.5, 4.5);
		aPainter.setFont(tag_face(aFont));
		aPainter.setPen(tag_ink(aPalette, aTone, aGround));
		aPainter.drawText(box, Qt::AlignCenter, aText);
		aPainter.restore();
		return size.width();
	}

	void paint_sparkle(QPainter& aPainter, const QPointF& aTopLeft, const qreal aSide, const QColor& aInk)
	{
		// The design's sparkle, drawn on a 14-unit square.
		const auto scale = aSide / 14.0;
		const auto at    = [&](const qreal aX, const qreal aY) { return aTopLeft + QPointF{ aX * scale, aY * scale }; };
		auto       star  = QPainterPath{ };
		star.moveTo(at(6.0, 1.2));
		star.lineTo(at(7.25, 4.75));
		star.lineTo(at(10.8, 6.0));
		star.lineTo(at(7.25, 7.25));
		star.lineTo(at(6.0, 10.8));
		star.lineTo(at(4.75, 7.25));
		star.lineTo(at(1.2, 6.0));
		star.lineTo(at(4.75, 4.75));
		star.closeSubpath();
		star.moveTo(at(11.0, 8.6));
		star.lineTo(at(11.6, 10.4));
		star.lineTo(at(13.4, 11.0));
		star.lineTo(at(11.6, 11.6));
		star.lineTo(at(11.0, 13.4));
		star.lineTo(at(10.4, 11.6));
		star.lineTo(at(8.6, 11.0));
		star.lineTo(at(10.4, 10.4));
		star.closeSubpath();
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(aInk);
		aPainter.drawPath(star);
		aPainter.restore();
	}

	void paint_refused(QPainter& aPainter, const QRectF& aBox, const QColor& aInk)
	{
		const auto side = std::min(aBox.width(), aBox.height());
		const auto ring = QRectF{ aBox.center().x() - side / 2.0, aBox.center().y() - side / 2.0, side, side }.adjusted(0.9, 0.9, -0.9, -0.9);
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.setPen(QPen{ aInk, 1.5, Qt::SolidLine, Qt::RoundCap });
		aPainter.setBrush(Qt::NoBrush);
		aPainter.drawEllipse(ring);
		const auto reach = ring.width() * 0.25;
		aPainter.drawLine(ring.center() + QPointF{ -reach, reach }, ring.center() + QPointF{ reach, -reach });
		aPainter.restore();
	}

	tag_label::tag_label(const QString& aText, const tag_tone aTone, QWidget* const aParent)
		: QLabel{ aText, aParent }
		, Tone{ aTone }
	{
		setTextFormat(Qt::PlainText);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		setFocusPolicy(Qt::NoFocus);
	}

	auto tag_label::sizeHint() const -> QSize { return tag_size(font(), text()).toSize() + QSize{ 1, 0 }; }

	auto tag_label::minimumSizeHint() const -> QSize { return sizeHint(); }

	void tag_label::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto size    = tag_size(font(), text());
		static_cast<void>(paint_tag(painter, QPointF{ 0.0, (height() - size.height()) / 2.0 }, text(), Tone, palette(), font(), ground_of(*this)));
	}

	namespace {
		class wrapped_height final : public QObject
		{
		public:
			using QObject::QObject;

		protected:
			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::Resize) {
					auto* const label  = static_cast<QLabel*>(aWatched);
					const auto  height = label->heightForWidth(label->width());
					if (height > 0 && height != label->minimumHeight()) {
						label->setMinimumHeight(height);
					}
				}
				return QObject::eventFilter(aWatched, aEvent);
			}
		};
	} // namespace

	void follow_wrapped_height(QLabel& aLabel) { aLabel.installEventFilter(new wrapped_height{ &aLabel }); }

	line_label::line_label(const QString& aText, QWidget* const aParent)
		: QLabel{ aText, aParent }
	{
		setTextFormat(Qt::PlainText);
		setWordWrap(true);
		setAlignment(Qt::AlignLeft | Qt::AlignTop);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	}

	auto line_label::sizeHint() const -> QSize
	{
		const auto width = fontMetrics().horizontalAdvance(text()) + 2 + contentsMargins().left() + contentsMargins().right();
		return { width, std::max(heightForWidth(width), Lines) };
	}

	auto line_label::minimumSizeHint() const -> QSize
	{
		// Grids and nested rows do not all ask a label's height for its
		// width: the least it needs is what its text takes at the width it
		// was last given, one line until it has one.
		const auto least = std::min(sizeHint().width(), fontMetrics().horizontalAdvance(QStringLiteral("MMMMMM")));
		return { least, std::max(Lines, fontMetrics().height()) };
	}

	void line_label::resizeEvent(QResizeEvent* const aEvent)
	{
		QLabel::resizeEvent(aEvent);
		const auto lines = heightForWidth(width());
		if (lines != Lines) {
			Lines = lines;
			updateGeometry();
		}
	}

	sparkle_mark::sparkle_mark(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}

	auto sparkle_mark::sizeHint() const -> QSize { return { sparkle_side, fontMetrics().height() }; }

	void sparkle_mark::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		paint_sparkle(painter, QPointF{ 0.0, (height() - sparkle_side) / 2.0 }, sparkle_side, agent_accent(palette()));
	}

	rounded_well::rounded_well(QWidget* const aParent)
		: QFrame{ aParent }
	{
		setFrameShape(QFrame::NoFrame);
		setAutoFillBackground(false);
	}

	void rounded_well::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ well_edge(palette()), 1.0 });
		painter.setBrush(palette().color(QPalette::AlternateBase));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 7.0, 7.0);
	}

	filled_group::filled_group(QWidget* const aParent)
		: QFrame{ aParent }
	{
		setFrameShape(QFrame::NoFrame);
		setAutoFillBackground(false);
	}

	void filled_group::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto night = dark(palette());
		painter.setPen(QPen{ with_alpha(palette().color(QPalette::WindowText), night ? 0.08 : 0.07), 1.0 });
		painter.setBrush(with_alpha(palette().color(QPalette::WindowText), night ? 0.05 : 0.035));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 9.0, 9.0);
	}

	comparison_table::comparison_table(QWidget* const aParent)
		: QWidget{ aParent }
		, Grid{ new QGridLayout{ this } }
	{
		Grid->setContentsMargins(0, 0, 0, 0);
		Grid->setSpacing(0);
		Grid->setColumnStretch(0, 1);
		Grid->setColumnStretch(1, 1);
	}

	void comparison_table::set_tinted(QWidget& aCell) { aCell.setProperty(tint_property, true); }

	void comparison_table::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto outline = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		auto       shape   = QPainterPath{ };
		shape.addRoundedRect(outline, 7.0, 7.0);
		painter.fillPath(shape, palette().color(QPalette::AlternateBase));
		painter.save();
		painter.setClipPath(shape);
		for (const auto* const cell : findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
			if (cell->property(tint_property).toBool() && cell->isVisible()) {
				painter.fillRect(cell->geometry(), tag_wash(palette(), tag_tone::updated));
			}
		}
		// A hairline above every row but the header, and one between the sides.
		painter.setRenderHint(QPainter::Antialiasing, false);
		painter.setPen(QPen{ hairline(palette()), 1.0 });
		for (auto row = 1; row < Grid->rowCount(); ++row) {
			if (const auto* const item = Grid->itemAtPosition(row, 0); item != nullptr) {
				const auto top = item->geometry().top();
				painter.drawLine(QPoint{ 0, top }, QPoint{ width(), top });
			}
		}
		const auto* const right = Grid->itemAtPosition(0, 1);
		if (right != nullptr && right != Grid->itemAtPosition(0, 0)) {
			const auto between = right->geometry().left();
			painter.drawLine(QPoint{ between, 0 }, QPoint{ between, height() });
		}
		painter.restore();
		painter.setPen(QPen{ well_edge(palette()), 1.0 });
		painter.setBrush(Qt::NoBrush);
		painter.drawRoundedRect(outline, 7.0, 7.0);
	}

	void settings_row_delegate::paint(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto* const widget  = option.widget;
		auto* const       style   = widget != nullptr ? widget->style() : QApplication::style();
		const auto&       palette = option.palette;
		const auto        enabled = (option.state & QStyle::State_Enabled) != 0;
		const auto        ground  = palette.color(QPalette::AlternateBase);
		aPainter->save();
		if (aIndex.data(row_heading_role).toBool()) {
			auto face = option.font;
			face.setWeight(QFont::DemiBold);
			aPainter->setFont(face);
			// A heading takes no input, so its option is disabled: it is still body text.
			aPainter->setPen(palette.color(QPalette::Active, QPalette::Text));
			aPainter->drawText(option.rect.adjusted(row_inset_x, 0, -row_inset_x, -heading_below), Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, row_name(aIndex));
			aPainter->restore();
			return;
		}
		if ((option.state & QStyle::State_Selected) != 0) {
			aPainter->fillRect(option.rect, quiet_selection(palette));
		}
		if ((option.state & QStyle::State_HasFocus) != 0) {
			auto focus = QStyleOptionFocusRect{ };
			focus.QStyleOption::operator=(option);
			focus.rect            = option.rect.adjusted(1, 1, -1, -1);
			focus.backgroundColor = ground;
			style->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, aPainter, widget);
		}
		const auto geometry = lay_out_row(option, aIndex, option.rect);
		const auto flags    = aIndex.flags();
		if (!geometry.Disclosure.isEmpty()) {
			// A chevron in the secondary colour: right when collapsed, down when open.
			const auto* const tree = qobject_cast<const QTreeView*>(widget);
			const auto        open = tree != nullptr && tree->isExpanded(aIndex);
			const auto        box  = QRectF{ geometry.Disclosure }.adjusted(3.0, 3.0, -3.0, -3.0);
			auto              path = QPainterPath{ };
			if (open) {
				path.moveTo(box.left(), box.top() + box.height() * 0.3);
				path.lineTo(box.center().x(), box.top() + box.height() * 0.75);
				path.lineTo(box.right(), box.top() + box.height() * 0.3);
			} else {
				path.moveTo(box.left() + box.width() * 0.3, box.top());
				path.lineTo(box.left() + box.width() * 0.75, box.center().y());
				path.lineTo(box.left() + box.width() * 0.3, box.bottom());
			}
			aPainter->save();
			aPainter->setRenderHint(QPainter::Antialiasing, true);
			aPainter->setPen(QPen{ palette.color(QPalette::Active, QPalette::PlaceholderText), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter->setBrush(Qt::NoBrush);
			aPainter->drawPath(path);
			aPainter->restore();
		}
		const auto mark = static_cast<row_mark>(aIndex.data(row_mark_role).toInt());
		if (flags.testFlag(Qt::ItemIsUserCheckable)) {
			// The style's own check box, as the design's ticks are, not the
			// item view's grey check.
			auto check = QStyleOptionButton{ };
			check.QStyleOption::operator=(option);
			check.rect  = geometry.Check;
			check.state = (option.state & (QStyle::State_Enabled | QStyle::State_Active)) | QStyle::State_Raised;
			switch (shown_check_state(aIndex)) {
			case Qt::Checked:          check.state |= QStyle::State_On; break;
			case Qt::PartiallyChecked: check.state |= QStyle::State_NoChange; break;
			case Qt::Unchecked:        check.state |= QStyle::State_Off; break;
			}
			style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, aPainter, widget);
		} else if (mark == row_mark::refused) {
			const auto side = std::min(geometry.Check.width(), geometry.Check.height()) + 1;
			paint_refused(*aPainter, QRectF{ QPointF{ geometry.Check.center() } - QPointF{ side / 2.0, side / 2.0 }, QSizeF{ static_cast<qreal>(side), static_cast<qreal>(side) } }, refused_ink(palette, ground));
		}
		if (mark == row_mark::agent) {
			paint_sparkle(*aPainter, geometry.Mark, sparkle_side, enabled ? agent_accent(palette) : agent_accent_disabled(palette));
		}
		const auto text = palette.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Text);
		aPainter->setFont(name_font(option, aIndex));
		aPainter->setPen(text);
		aPainter->drawText(geometry.Name, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, row_name(aIndex));
		if (const auto tag = aIndex.data(row_tag_role).toString(); !tag.isEmpty()) {
			if (!enabled) {
				aPainter->setOpacity(0.55);
			}
			static_cast<void>(paint_tag(*aPainter, geometry.Tag, tag, static_cast<tag_tone>(aIndex.data(row_tone_role).toInt()), palette, option.font, ground));
			aPainter->setOpacity(1.0);
		}
		if (const auto second = row_second(aIndex); !second.isEmpty()) {
			aPainter->setFont(secondary_font(option.font));
			auto quiet = palette.color(QPalette::PlaceholderText);
			if (!enabled) {
				quiet.setAlphaF(quiet.alphaF() * 0.7F);
			}
			aPainter->setPen(quiet);
			aPainter->drawText(geometry.Second, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, second);
		}
		aPainter->restore();
	}

	auto settings_row_delegate::sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto width = row_width(option, aIndex);
		if (aIndex.data(row_heading_role).toBool()) {
			auto face = option.font;
			face.setWeight(QFont::DemiBold);
			return { width, QFontMetrics{ face }.height() + heading_air + heading_below };
		}
		return { width, lay_out_row(option, aIndex, QRect{ 0, 0, width, 100 }).Height };
	}

	auto settings_row_delegate::editorEvent(QEvent* const aEvent, QAbstractItemModel* const aModel, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> bool
	{
		const auto flags = aIndex.flags();
		if (aEvent->type() == QEvent::MouseButtonRelease || aEvent->type() == QEvent::MouseButtonDblClick) {
			// The disclosure triangle opens and closes its section.
			auto option = aOption;
			initStyleOption(&option, aIndex);
			const auto disclosure = lay_out_row(option, aIndex, option.rect).Disclosure.adjusted(-4, -4, 4, 4);
			auto*      tree       = const_cast<QTreeView*>(qobject_cast<const QTreeView*>(aOption.widget));
			if (tree != nullptr && !disclosure.isEmpty() && disclosure.contains(static_cast<QMouseEvent*>(aEvent)->position().toPoint())) {
				if (aEvent->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent*>(aEvent)->button() == Qt::LeftButton) {
					tree->setExpanded(aIndex, !tree->isExpanded(aIndex));
				}
				return true;
			}
		}
		if (!flags.testFlag(Qt::ItemIsUserCheckable) || !flags.testFlag(Qt::ItemIsEnabled)) {
			return false;
		}
		switch (aEvent->type()) {
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick: {
			const auto* const mouse  = static_cast<QMouseEvent*>(aEvent);
			auto              option = aOption;
			initStyleOption(&option, aIndex);
			const auto check = lay_out_row(option, aIndex, option.rect).Check.adjusted(-3, -3, 3, 3);
			if (mouse->button() != Qt::LeftButton || !check.contains(mouse->position().toPoint())) {
				return false;
			}
			if (aEvent->type() == QEvent::MouseButtonDblClick) {
				return true;
			}
			break;
		}
		case QEvent::KeyPress: {
			const auto key = static_cast<QKeyEvent*>(aEvent)->key();
			if (key != Qt::Key_Space && key != Qt::Key_Select) {
				return false;
			}
			break;
		}
		default: return false;
		}
		const auto state = shown_check_state(aIndex);
		return aModel->setData(aIndex, state == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
	}
} // namespace bpr::qt::detail
