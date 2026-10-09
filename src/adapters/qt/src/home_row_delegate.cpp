#include "bpr/qt/home_row_delegate.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QList>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The second line and the right column are set a little smaller than the first.
		constexpr auto secondary_scale = 0.92;
		/// Between the identity and the title.
		constexpr auto identity_separator = QLatin1StringView{ "  " };
		/// [[BPR-0783]]. An account chip: the tile's inset in its pill, the gap
		/// after the tile, and the room after the login.
		constexpr auto chip_inset = 2;
		constexpr auto chip_gap   = 5;
		constexpr auto chip_pad   = 8;
		/// The least an initial tile may be, so it holds a character at any font.
		constexpr auto tile_minimum = 12;
		/// A square owner tile's radius; an account's tile is a circle.
		constexpr auto tile_radius = 3.0;
		/// The initial is set inside the tile rather than at the tile's own size.
		constexpr auto tile_initial_scale = 0.78;
		/// [[BPR-1357]]. Between the open row's mark and its line.
		constexpr auto open_mark_gap = 6;
		/// The opening mark's arc: a quarter of a turn, at this share of its ring.
		constexpr auto open_arc_degrees = 100;
		/// [[BPR-1365]]. The Reviewing tag: its text's room inside the outline,
		/// above and beside it, and the outline's corner. A small radius, so it
		/// is never read as an account chip's pill.
		constexpr auto tag_pad_x  = 6;
		constexpr auto tag_pad_y  = 1;
		constexpr auto tag_radius = 3.0;
		/// [[BPR-0783]]. The one host a chip never repeats.
		constexpr auto default_host = QLatin1StringView{ "github.com" };
		/// Between a login and the host it is not on by default.
		constexpr auto chip_host_separator = QLatin1StringView{ " on " };

		[[nodiscard]] auto secondary_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			if (font.pointSizeF() > 0.0) {
				font.setPointSizeF(font.pointSizeF() * secondary_scale);
			} else if (font.pixelSize() > 0) {
				font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * secondary_scale))));
			}
			return font;
		}

		[[nodiscard]] auto style_of(const QStyleOptionViewItem& aOption) -> QStyle*
		{
			return aOption.widget != nullptr ? aOption.widget->style() : QApplication::style();
		}

		[[nodiscard]] auto row_width(const QStyleOptionViewItem& aOption) -> int
		{
			if (aOption.rect.width() > 0) {
				return aOption.rect.width();
			}
			if (const auto* const view = qobject_cast<const QAbstractItemView*>(aOption.widget);
				view != nullptr && view->viewport()->width() > 0) {
				return view->viewport()->width();
			}
			constexpr auto fallbackCharacters = 60;
			return QFontMetrics{ aOption.font }.averageCharWidth() * fallbackCharacters;
		}

		[[nodiscard]] auto has_hairline(const QModelIndex& aIndex) -> bool
		{
			return aIndex.isValid() && aIndex.model() != nullptr
				&& aIndex.row() + 1 < aIndex.model()->rowCount(aIndex.parent());
		}

		/// [[BPR-1338]]. What QToolButton asks of its style for an icon-only
		/// button at the small icon size.
		[[nodiscard]] auto icon_action_size(const QStyleOptionViewItem& aOption) -> QSize
		{
			auto* const style      = style_of(aOption);
			const auto  side       = style->pixelMetric(QStyle::PM_SmallIconSize, nullptr, aOption.widget);
			auto        button     = QStyleOptionToolButton{ };
			button.font            = aOption.font;
			button.palette         = aOption.palette;
			button.direction       = aOption.direction;
			button.iconSize        = QSize{ side, side };
			button.toolButtonStyle = Qt::ToolButtonIconOnly;
			button.subControls     = QStyle::SC_ToolButton;
			button.rect            = QRect{ 0, 0, side, side };
			return style->sizeFromContents(QStyle::CT_ToolButton, &button, QSize{ side, side }, aOption.widget);
		}

		/// The labelled action's extent, or the icon action's where there is none.
		[[nodiscard]] auto row_action_size(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QSize;

		[[nodiscard]] auto action_size(const QStyleOptionViewItem& aOption, const QString& aText) -> QSize
		{
			if (aText.isEmpty()) {
				return { };
			}
			// What QToolButton asks of its style for a text-only button.
			const auto metrics  = QFontMetrics{ aOption.font };
			auto       textSize = metrics.size(Qt::TextShowMnemonic, aText);
			textSize.setWidth(textSize.width() + (metrics.horizontalAdvance(QLatin1Char{ ' ' }) * 2));
			auto button            = QStyleOptionToolButton{ };
			button.font            = aOption.font;
			button.palette         = aOption.palette;
			button.direction       = aOption.direction;
			button.text            = aText;
			button.toolButtonStyle = Qt::ToolButtonTextOnly;
			button.subControls     = QStyle::SC_ToolButton;
			button.rect            = QRect{ QPoint{ }, textSize };
			return style_of(aOption)->sizeFromContents(QStyle::CT_ToolButton, &button, textSize, aOption.widget);
		}

		auto row_action_size(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QSize
		{
			if (const auto action = aIndex.data(home_row_role::row_action).toString(); !action.isEmpty()) {
				return action_size(aOption, action);
			}
			if (!aIndex.data(home_row_role::icon_action).toString().isEmpty()) {
				return icon_action_size(aOption);
			}
			return { };
		}

		[[nodiscard]] auto lay_out(
			QTextLayout&                           aLayout,
			const QString&                         aText,
			const QFont&                           aFont,
			const int                              aWidth,
			const QList<QTextLayout::FormatRange>& aFormats) -> int
		{
			aLayout.setText(aText);
			aLayout.setFont(aFont);
			auto option = QTextOption{ };
			option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			aLayout.setTextOption(option);
			aLayout.setFormats(aFormats);
			aLayout.beginLayout();
			auto height = qreal{ 0.0 };
			for (auto line = aLayout.createLine(); line.isValid(); line = aLayout.createLine()) {
				line.setLineWidth(static_cast<qreal>(aWidth));
				line.setPosition(QPointF{ 0.0, height });
				height += line.height();
			}
			aLayout.endLayout();
			return static_cast<int>(std::ceil(height));
		}

		/// \brief One row's text, laid out for the width it is drawn at.
		///
		/// \details Coordinates are relative to the row's top-left corner. The
		/// same measurement serves the size hint, the painting, the action's
		/// place and the reason word's place, so they cannot disagree.
		class row_layout final
		{
		public:
			row_layout(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex, const bool aMarks, const QColor& aReasonInk,
					   const home_row_open* const aOpen = nullptr)
			{
				const auto identity = aIndex.data(home_row_role::identity).toString();
				const auto title    = aIndex.data(home_row_role::title).toString();
				const auto why      = aIndex.data(home_row_role::why).toString();
				const auto reason   = aIndex.data(home_row_role::reason_word).toString();
				Time                = aIndex.data(home_row_role::time).toString();
				Progress            = aIndex.data(home_row_role::progress).toString();
				Chip                = aIndex.data(home_row_role::account_chip).toString();
				// [[BPR-1365]]. The review this window has open says so in a word.
				Tag = aIndex.data(home_row_role::reviewing).toBool() ? home_reviewing_text() : QString{ };
				// [[BPR-1357]]. The row a pull request is being opened from says
				// so in place of why it is listed, behind a mark, and its action
				// place holds Cancel or Retry instead of the hover action.
				const auto* const open = aOpen != nullptr && aOpen->Phase != home_row_open_phase::none && !aOpen->Identity.isEmpty()
						&& identity == aOpen->Identity
					? aOpen
					: nullptr;
				Opening                = open != nullptr;
				OpenAction             = open != nullptr ? open->Action : QString{ };

				First  = title.isEmpty() ? identity : identity + identity_separator + title;
				Second = open != nullptr ? open->Line : why;
				if (!reason.isEmpty() && !Opening) {
					if (!Second.isEmpty()) {
						Second += QLatin1Char{ ' ' };
					}
					ReasonStart  = static_cast<int>(Second.size());
					ReasonLength = static_cast<int>(reason.size());
					Second += reason;
				}

				SideFont                = secondary_font(aOption.font);
				const auto sideMetrics  = QFontMetrics{ SideFont };
				SideLineHeight          = sideMetrics.height();
				const auto actionExtent = Opening ? action_size(aOption, OpenAction) : row_action_size(aOption, aIndex);
				SideWidth               = std::max({ sideMetrics.horizontalAdvance(Time),
													 sideMetrics.horizontalAdvance(Progress),
													 actionExtent.width() });
				// [[BPR-0783]]. The account chip: a round initial tile and the
				// login, at the row's right before the time.
				const auto chipExtent = home_chip_size(SideFont, Chip);
				ChipWidth             = chipExtent.width();
				ChipHeight            = chipExtent.height();
				TagWidth              = Tag.isEmpty() ? 0 : sideMetrics.horizontalAdvance(Tag) + (tag_pad_x * 2);
				TagHeight             = Tag.isEmpty() ? 0 : SideLineHeight + (tag_pad_y * 2);

				Width                = row_width(aOption);
				TextLeft             = home_row_delegate::inset + (aMarks ? home_row_delegate::mark_width : 0);
				const auto right     = Width - home_row_delegate::inset;
				SideLeft             = right - SideWidth;
				ChipLeft             = SideLeft - (SideWidth > 0 && ChipWidth > 0 ? home_row_delegate::column_gap : 0) - ChipWidth;
				TagLeft              = ChipLeft - ((SideWidth > 0 || ChipWidth > 0) && TagWidth > 0 ? home_row_delegate::column_gap : 0) - TagWidth;
				const auto textRight = SideWidth > 0 || ChipWidth > 0 || TagWidth > 0 ? TagLeft - home_row_delegate::column_gap : right;
				TextWidth            = std::max(1, textRight - TextLeft);

				auto identityFormat   = QTextLayout::FormatRange{ };
				identityFormat.start  = 0;
				identityFormat.length = static_cast<int>(identity.size());
				identityFormat.format.setFontWeight(QFont::DemiBold);
				FirstHeight = lay_out(FirstLayout, First, aOption.font, TextWidth, { identityFormat });

				auto secondFormats = QList<QTextLayout::FormatRange>{ };
				if (ReasonStart >= 0) {
					auto reasonFormat   = QTextLayout::FormatRange{ };
					reasonFormat.start  = ReasonStart;
					reasonFormat.length = ReasonLength;
					reasonFormat.format.setFontWeight(QFont::DemiBold);
					if (aReasonInk.isValid()) {
						reasonFormat.format.setForeground(aReasonInk);
					}
					secondFormats.push_back(reasonFormat);
				}
				// The open row's line is set at the first line's size, behind a
				// mark as tall as that line: it is what the reviewer is waiting on.
				const auto& secondFont = Opening ? aOption.font : SideFont;
				MarkExtent             = Opening ? QFontMetrics{ aOption.font }.height() : 0;
				SecondIndent           = Opening ? MarkExtent + open_mark_gap : 0;
				SecondHeight           = Second.isEmpty() ? 0 : lay_out(SecondLayout, Second, secondFont, std::max(1, TextWidth - SecondIndent), secondFormats);

				const auto content = (home_row_delegate::vertical_padding * 2) + FirstHeight
					+ (SecondHeight > 0 ? home_row_delegate::line_gap + SecondHeight : 0);
				const auto sideLines = (Time.isEmpty() ? 0 : 1) + (Progress.isEmpty() ? 0 : 1);
				const auto side      = (home_row_delegate::vertical_padding * 2)
					+ std::max({ sideLines * SideLineHeight, actionExtent.height(), ChipHeight, TagHeight });
				RowHeight   = std::max({ home_row_delegate::minimum_row_height, content, side });
				TotalHeight = RowHeight + (has_hairline(aIndex) ? home_row_delegate::hairline : 0);
				ContentTop  = ((RowHeight - content) / 2) + home_row_delegate::vertical_padding;
				SecondTop   = ContentTop + FirstHeight + home_row_delegate::line_gap;
			}

			row_layout(const row_layout&)                    = delete;
			auto operator=(const row_layout&) -> row_layout& = delete;
			row_layout(row_layout&&)                         = delete;
			auto operator=(row_layout&&) -> row_layout&      = delete;
			~row_layout()                                    = default;

			[[nodiscard]] auto reason_rect() const -> QRect
			{
				auto result = QRect{ };
				if (ReasonStart < 0) {
					return result;
				}
				const auto reasonEnd = ReasonStart + ReasonLength;
				for (auto index = 0; index < SecondLayout.lineCount(); ++index) {
					const auto line  = SecondLayout.lineAt(index);
					const auto start = std::max(ReasonStart, line.textStart());
					const auto end   = std::min(reasonEnd, line.textStart() + line.textLength());
					if (start >= end) {
						continue;
					}
					const auto left  = line.cursorToX(start);
					const auto right = line.cursorToX(end);
					result |= QRectF{ static_cast<qreal>(TextLeft) + std::min(left, right),
									  static_cast<qreal>(SecondTop) + line.y(),
									  std::abs(right - left),
									  line.height() }
								  .toAlignedRect();
				}
				return result;
			}

			[[nodiscard]] auto lines() const -> QStringList
			{
				auto result = QStringList{ };
				for (auto index = 0; index < FirstLayout.lineCount(); ++index) {
					const auto line = FirstLayout.lineAt(index);
					result.push_back(First.mid(line.textStart(), line.textLength()));
				}
				for (auto index = 0; index < SecondLayout.lineCount(); ++index) {
					const auto line = SecondLayout.lineAt(index);
					result.push_back(Second.mid(line.textStart(), line.textLength()));
				}
				return result;
			}

			QString     First;
			QString     Second;
			QString     OpenAction;
			bool        Opening      = false;
			int         MarkExtent   = 0;
			int         SecondIndent = 0;
			QString     Time;
			QString     Progress;
			QString     Chip;
			QString     Tag;
			QFont       SideFont;
			QTextLayout FirstLayout;
			QTextLayout SecondLayout;
			int         ReasonStart    = -1;
			int         ReasonLength   = 0;
			int         SideLineHeight = 0;
			int         SideWidth      = 0;
			int         ChipWidth      = 0;
			int         ChipHeight     = 0;
			int         Width          = 0;
			int         TextLeft       = 0;
			int         TextWidth      = 1;
			int         SideLeft       = 0;
			int         ChipLeft       = 0;
			int         TagWidth       = 0;
			int         TagHeight      = 0;
			int         TagLeft        = 0;
			int         FirstHeight    = 0;
			int         SecondHeight   = 0;
			int         RowHeight      = 0;
			int         TotalHeight    = 0;
			int         ContentTop     = 0;
			int         SecondTop      = 0;
		};
	} // namespace

	auto home_state_colours::for_group(const home_group aGroup) const -> QColor
	{
		switch (aGroup) {
		case home_group::needs_you:
			return NeedsMe;
		case home_group::waiting_on_others:
			return WaitingOnOthers;
		case home_group::following:
			break;
		}
		return Following;
	}

	auto home_state_colours_for(const appearance_profile& aProfile) -> home_state_colours
	{
		return home_state_colours{
			.NeedsMe         = aProfile.attention_color(attention_tone::needs_me),
			.WaitingOnOthers = aProfile.attention_color(attention_tone::waiting_on_others),
			.Following       = aProfile.attention_color(attention_tone::following),
			.Failure         = aProfile.color(core::appearance_role::state_defect),
		};
	}

	auto mix_colour(const QColor& aTone, const QColor& aGround, const qreal aStrength) -> QColor
	{
		const auto strength = std::clamp(aStrength, qreal{ 0.0 }, qreal{ 1.0 });
		const auto tone     = aTone.toRgb();
		const auto ground   = aGround.toRgb();
		const auto channel  = [strength](const float aTo, const float aFrom) {
			return static_cast<float>(aFrom + ((aTo - aFrom) * strength));
		};
		return QColor::fromRgbF(
			channel(tone.redF(), ground.redF()),
			channel(tone.greenF(), ground.greenF()),
			channel(tone.blueF(), ground.blueF()),
			ground.alphaF());
	}

	auto home_chip_ink_for(
		const QPalette& aPalette, const QPalette::ColorGroup aGroup, const bool aSelected) -> home_chip_ink
	{
		if (aSelected) {
			// A selected row is filled by the style; the chip reads in the
			// style's own selected ink rather than inventing a second one.
			const auto ink = aPalette.color(aGroup, QPalette::HighlightedText);
			return { .Label = ink, .Initial = ink, .Tile = QColor{ Qt::transparent }, .Border = ink };
		}
		return {
			.Label   = aPalette.color(aGroup, QPalette::PlaceholderText),
			.Initial = aPalette.color(aGroup, QPalette::Text),
			.Tile    = aPalette.color(aGroup, QPalette::Mid),
			.Border  = aPalette.color(aGroup, QPalette::Mid),
		};
	}

	auto home_tile_initial(const QString& aName) -> QString
	{
		for (const auto character : aName) {
			if (character.isLetterOrNumber()) {
				return QString{ character }.toUpper();
			}
		}
		return QStringLiteral("?");
	}

	auto home_account_chip_text(const QString& aLogin, const QString& aHost) -> QString
	{
		// RFC-BPR-0028: a host is said only where it differs from the default.
		return aHost.isEmpty() || aHost == default_host
			? aLogin
			: aLogin + chip_host_separator + aHost;
	}

	auto home_reviewing_text() -> QString
	{
		return QStringLiteral("Reviewing");
	}

	auto home_reviewing_description() -> QString
	{
		return QStringLiteral("Reviewing in this window");
	}

	auto home_tile_extent(const QFont& aFont) -> int
	{
		return std::max(tile_minimum, QFontMetrics{ aFont }.height());
	}

	auto home_chip_size(const QFont& aFont, const QString& aText) -> QSize
	{
		if (aText.isEmpty()) {
			return { };
		}
		const auto tile = home_tile_extent(aFont);
		return { (chip_inset * 2) + tile + chip_gap + QFontMetrics{ aFont }.horizontalAdvance(aText) + chip_pad,
				 tile + (chip_inset * 2) };
	}

	void draw_home_tile(
		QPainter&            aPainter,
		const QRect&         aRect,
		const QString&       aInitial,
		const bool           aRound,
		const QFont&         aFont,
		const home_chip_ink& aInk)
	{
		if (aRect.isEmpty()) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto filled = aInk.Tile.alpha() != 0;
		aPainter.setPen(filled ? QPen{ Qt::NoPen } : QPen{ aInk.Border, 1.0 });
		aPainter.setBrush(filled ? QBrush{ aInk.Tile } : QBrush{ Qt::NoBrush });
		const auto box = QRectF{ aRect }.adjusted(0.5, 0.5, -0.5, -0.5);
		if (aRound) {
			aPainter.drawEllipse(box);
		} else {
			aPainter.drawRoundedRect(box, tile_radius, tile_radius);
		}
		auto initial = aFont;
		initial.setWeight(QFont::DemiBold);
		if (initial.pointSizeF() > 0.0) {
			initial.setPointSizeF(initial.pointSizeF() * tile_initial_scale);
		} else if (initial.pixelSize() > 0) {
			initial.setPixelSize(std::max(1, static_cast<int>(std::lround(initial.pixelSize() * tile_initial_scale))));
		}
		aPainter.setFont(initial);
		aPainter.setPen(aInk.Initial);
		aPainter.drawText(aRect, Qt::AlignCenter, aInitial);
		aPainter.restore();
	}

	void draw_home_chip(
		QPainter&            aPainter,
		const QRect&         aRect,
		const QString&       aText,
		const QFont&         aFont,
		const home_chip_ink& aInk)
	{
		if (aText.isEmpty() || aRect.isEmpty()) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto pill = QRectF{ aRect }.adjusted(0.5, 0.5, -0.5, -0.5);
		aPainter.setPen(QPen{ aInk.Border, 1.0 });
		aPainter.setBrush(Qt::NoBrush);
		aPainter.drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
		const auto tile = home_tile_extent(aFont);
		draw_home_tile(aPainter,
					   QRect{ aRect.left() + chip_inset, aRect.top() + chip_inset, tile, tile },
					   home_tile_initial(aText),
					   true,
					   aFont,
					   aInk);
		const auto left = aRect.left() + chip_inset + tile + chip_gap;
		aPainter.setFont(aFont);
		aPainter.setPen(aInk.Label);
		// Never elided: the login is remote literal text, so the chip asked for
		// the width it needs and the row's own text yielded instead.
		aPainter.drawText(QRect{ left, aRect.top(), std::max(0, aRect.right() - left), aRect.height() },
						  Qt::AlignLeft | Qt::AlignVCenter,
						  aText);
		aPainter.restore();
	}

	home_row_delegate::home_row_delegate(QObject* const aParent)
		: QStyledItemDelegate{ aParent }
	{
	}

	void home_row_delegate::set_state_colours(const home_state_colours& aColours)
	{
		Colours = aColours;
	}

	auto home_row_delegate::state_colours() const noexcept -> const home_state_colours&
	{
		return Colours;
	}

	void home_row_delegate::set_marks(const bool aMarks)
	{
		Marks = aMarks;
	}

	auto home_row_delegate::state_colour(const QModelIndex& aIndex) const -> QColor
	{
		const auto state = aIndex.data(home_row_role::state);
		if (!state.isValid()) {
			return { };
		}
		const auto value = state.toInt();
		if (value < static_cast<int>(home_group::needs_you) || value > static_cast<int>(home_group::following)) {
			return { };
		}
		return Colours.for_group(static_cast<home_group>(value));
	}

	void home_row_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto       selected  = option.state.testFlag(QStyle::State_Selected);
		const auto       group     = !option.state.testFlag(QStyle::State_Enabled) ? QPalette::Disabled
			: option.state.testFlag(QStyle::State_Active)                          ? QPalette::Active
																				   : QPalette::Inactive;
		const auto&      colours   = option.palette;
		const auto       tone      = state_colour(aIndex);
		const auto       reasonInk = selected ? colours.color(group, QPalette::HighlightedText) : tone;
		const row_layout layout{ option, aIndex, Marks, reasonInk, &Open };

		// [[BPR-1365]]. The open review's quiet ground, under the style's: the
		// theme's current-line tint, never its selection. A selected row is the
		// style's to fill.
		if (!selected && !layout.Tag.isEmpty()) {
			aPainter->fillRect(QRect{ option.rect.left(), option.rect.top(), layout.Width, layout.RowHeight },
							   colours.color(group, QPalette::AlternateBase));
		}
		// The style draws the row's ground: hover, selection and focus, natively.
		auto native = option;
		native.text.clear();
		native.icon = QIcon{ };
		native.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
		native.rect.setHeight(layout.RowHeight);
		style_of(option)->drawControl(QStyle::CE_ItemViewItem, &native, aPainter, option.widget);

		aPainter->save();
		aPainter->translate(option.rect.topLeft());
		if (Marks && tone.isValid()) {
			aPainter->fillRect(QRect{ 0, 0, mark_width, layout.RowHeight }, tone);
		}
		const auto ink   = colours.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
		const auto quiet = colours.color(group, selected ? QPalette::HighlightedText : QPalette::PlaceholderText);
		aPainter->setPen(ink);
		layout.FirstLayout.draw(aPainter, QPointF{ static_cast<qreal>(layout.TextLeft), static_cast<qreal>(layout.ContentTop) });
		if (layout.Opening) {
			// [[BPR-1357]]. The line the reviewer waits on, in full ink behind
			// its mark; a failure in the failure ink, which every theme holds at
			// text strength. A selected row keeps the style's selected ink.
			const auto failed  = Open.Phase == home_row_open_phase::failed;
			const auto failure = Colours.Failure.isValid() ? Colours.Failure : ink;
			const auto lineInk = selected ? ink : failed ? failure
														 : ink;
			const auto mark    = QRect{ layout.TextLeft, layout.SecondTop, layout.MarkExtent, layout.MarkExtent };
			if (failed) {
				draw_home_failure_mark(*aPainter, mark, option.font, lineInk);
			} else {
				// The arc is the line's own ink on a faint ring of it: on Home
				// colour means state, and blue is Waiting on others.
				auto faint = ink;
				faint.setAlphaF(0.25F);
				draw_home_open_mark(*aPainter, mark, Open.Frame, ink, faint);
			}
			aPainter->setPen(lineInk);
			layout.SecondLayout.draw(aPainter, QPointF{ static_cast<qreal>(layout.TextLeft + layout.SecondIndent), static_cast<qreal>(layout.SecondTop) });
		} else if (!layout.Second.isEmpty()) {
			aPainter->setPen(quiet);
			layout.SecondLayout.draw(aPainter, QPointF{ static_cast<qreal>(layout.TextLeft), static_cast<qreal>(layout.SecondTop) });
		}
		aPainter->setFont(layout.SideFont);
		aPainter->setPen(quiet);
		auto sideTop = layout.ContentTop;
		for (const auto* const text : { &layout.Time, &layout.Progress }) {
			if (text->isEmpty()) {
				continue;
			}
			aPainter->drawText(QRect{ layout.SideLeft, sideTop, layout.SideWidth, layout.SideLineHeight },
							   Qt::AlignRight | Qt::AlignVCenter, *text);
			sideTop += layout.SideLineHeight;
		}
		if (!layout.Chip.isEmpty()) {
			// [[BPR-0783]]. Centred on the row beside the time, in neutral ink:
			// an account is not a state, so the chip carries no state colour.
			draw_home_chip(*aPainter,
						   QRect{ layout.ChipLeft, (layout.RowHeight - layout.ChipHeight) / 2, layout.ChipWidth, layout.ChipHeight },
						   layout.Chip,
						   layout.SideFont,
						   home_chip_ink_for(colours, group, selected));
		}
		if (!layout.Tag.isEmpty()) {
			// [[BPR-1365]]. A word in neutral ink in a thin outline: on Home colour
			// means state, so the tag claims none. A selected row keeps the
			// style's selected ink, as the chip does.
			const auto tagInk  = selected ? colours.color(group, QPalette::HighlightedText) : colours.color(group, QPalette::Text);
			const auto outline = selected ? tagInk : colours.color(group, QPalette::PlaceholderText);
			const auto tag     = QRect{ layout.TagLeft, (layout.RowHeight - layout.TagHeight) / 2, layout.TagWidth, layout.TagHeight };
			aPainter->save();
			aPainter->setRenderHint(QPainter::Antialiasing, true);
			aPainter->setPen(QPen{ outline, 1.0 });
			aPainter->setBrush(Qt::NoBrush);
			aPainter->drawRoundedRect(QRectF{ tag }.adjusted(0.5, 0.5, -0.5, -0.5), tag_radius, tag_radius);
			aPainter->setFont(layout.SideFont);
			aPainter->setPen(tagInk);
			aPainter->drawText(tag, Qt::AlignCenter, layout.Tag);
			aPainter->restore();
		}
		if (has_hairline(aIndex)) {
			aPainter->fillRect(QRect{ inset, layout.RowHeight, std::max(0, layout.Width - inset), hairline },
							   colours.color(group, QPalette::Mid));
		}
		aPainter->restore();
	}

	auto home_row_delegate::sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QSize
	{
		if (!aIndex.isValid()) {
			return QStyledItemDelegate::sizeHint(aOption, aIndex);
		}
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const row_layout layout{ option, aIndex, Marks, QColor{ }, &Open };
		return { layout.Width, layout.TotalHeight };
	}

	auto home_row_delegate::action_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QRect
	{
		const auto size = row_action_size(aOption, aIndex);
		if (size.isEmpty()) {
			return { };
		}
		const row_layout layout{ aOption, aIndex, false, QColor{ } };
		return QRect{ aOption.rect.left() + layout.Width - inset - size.width(),
					  aOption.rect.top() + ((layout.RowHeight - size.height()) / 2),
					  size.width(),
					  size.height() };
	}

	auto home_row_delegate::reviewing_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QRect
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const row_layout layout{ option, aIndex, Marks, QColor{ }, &Open };
		if (layout.Tag.isEmpty()) {
			return { };
		}
		return QRect{ option.rect.left() + layout.TagLeft,
					  option.rect.top() + ((layout.RowHeight - layout.TagHeight) / 2),
					  layout.TagWidth,
					  layout.TagHeight };
	}

	auto home_row_delegate::reason_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QRect
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const row_layout layout{ option, aIndex, Marks, QColor{ } };
		return layout.reason_rect().translated(option.rect.topLeft());
	}

	auto home_row_delegate::line_texts(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QStringList
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const row_layout layout{ option, aIndex, Marks, QColor{ }, &Open };
		return layout.lines();
	}

	void home_row_delegate::set_open(home_row_open aOpen)
	{
		Open = std::move(aOpen);
	}

	auto home_row_delegate::open() const noexcept -> const home_row_open&
	{
		return Open;
	}

	auto home_row_delegate::opens(const QModelIndex& aIndex) const -> bool
	{
		return aIndex.isValid() && Open.Phase != home_row_open_phase::none && !Open.Identity.isEmpty()
			&& aIndex.data(home_row_role::identity).toString() == Open.Identity;
	}

	auto home_row_delegate::open_action_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QRect
	{
		if (!opens(aIndex) || Open.Action.isEmpty()) {
			return { };
		}
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto       size = action_size(option, Open.Action);
		const row_layout layout{ option, aIndex, Marks, QColor{ }, &Open };
		return QRect{ option.rect.left() + layout.Width - inset - size.width(),
					  option.rect.top() + ((layout.RowHeight - size.height()) / 2),
					  size.width(),
					  size.height() };
	}

	void draw_home_open_mark(QPainter& aPainter, const QRect& aRect, const int aFrame, const QColor& aArc, const QColor& aRing)
	{
		if (aRect.isEmpty()) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		// A ring two points inside the line's height, and an arc on it that
		// turns one step a frame.
		const auto side   = static_cast<qreal>(std::min(aRect.width(), aRect.height()));
		const auto stroke = std::max(qreal{ 1.5 }, side / 8.0);
		const auto inset  = (stroke / 2.0) + 2.0;
		const auto box    = QRectF{ QPointF{ aRect.left() + ((aRect.width() - side) / 2.0), aRect.top() + ((aRect.height() - side) / 2.0) },
									QSizeF{ side, side } }
								.adjusted(inset, inset, -inset, -inset);
		aPainter.setBrush(Qt::NoBrush);
		aPainter.setPen(QPen{ aRing, stroke });
		aPainter.drawEllipse(box);
		auto arc = QPen{ aArc, stroke };
		arc.setCapStyle(Qt::RoundCap);
		aPainter.setPen(arc);
		constexpr auto step  = 360 / home_open_frames;
		const auto     start = 90 - ((((aFrame % home_open_frames) + home_open_frames) % home_open_frames) * step);
		aPainter.drawArc(box, start * 16, -open_arc_degrees * 16);
		aPainter.restore();
	}

	void draw_home_failure_mark(QPainter& aPainter, const QRect& aRect, const QFont& aFont, const QColor& aInk)
	{
		if (aRect.isEmpty()) {
			return;
		}
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto side   = static_cast<qreal>(std::min(aRect.width(), aRect.height()));
		const auto stroke = std::max(qreal{ 1.5 }, side / 8.0);
		const auto inset  = (stroke / 2.0) + 2.0;
		const auto box    = QRectF{ QPointF{ aRect.left() + ((aRect.width() - side) / 2.0), aRect.top() + ((aRect.height() - side) / 2.0) },
									QSizeF{ side, side } }
								.adjusted(inset, inset, -inset, -inset);
		aPainter.setBrush(Qt::NoBrush);
		aPainter.setPen(QPen{ aInk, stroke });
		aPainter.drawEllipse(box);
		auto mark = aFont;
		mark.setWeight(QFont::Bold);
		if (mark.pointSizeF() > 0.0) {
			mark.setPointSizeF(mark.pointSizeF() * tile_initial_scale);
		} else if (mark.pixelSize() > 0) {
			mark.setPixelSize(std::max(1, static_cast<int>(std::lround(mark.pixelSize() * tile_initial_scale))));
		}
		aPainter.setFont(mark);
		aPainter.drawText(box, Qt::AlignCenter, QStringLiteral("!"));
		aPainter.restore();
	}
} // namespace bpr::qt
