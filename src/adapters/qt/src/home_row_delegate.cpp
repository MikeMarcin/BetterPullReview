#include "bpr/qt/home_row_delegate.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QList>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		/// The second line and the right column are set a little smaller than the first.
		constexpr auto secondary_scale = 0.92;
		/// Between the identity and the title.
		constexpr auto identity_separator = QLatin1StringView{ "  " };

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
			row_layout(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex, const bool aMarks, const QColor& aReasonInk)
			{
				const auto identity = aIndex.data(home_row_role::identity).toString();
				const auto title    = aIndex.data(home_row_role::title).toString();
				const auto why      = aIndex.data(home_row_role::why).toString();
				const auto reason   = aIndex.data(home_row_role::reason_word).toString();
				Time                = aIndex.data(home_row_role::time).toString();
				Progress            = aIndex.data(home_row_role::progress).toString();
				Chip                = aIndex.data(home_row_role::account_chip).toString();
				const auto action   = aIndex.data(home_row_role::row_action).toString();

				First  = title.isEmpty() ? identity : identity + identity_separator + title;
				Second = why;
				if (!reason.isEmpty()) {
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
				const auto actionExtent = action_size(aOption, action);
				SideWidth               = std::max({ sideMetrics.horizontalAdvance(Time),
													 sideMetrics.horizontalAdvance(Progress),
													 actionExtent.width() });
				ChipWidth               = Chip.isEmpty() ? 0 : sideMetrics.horizontalAdvance(Chip);

				Width                = row_width(aOption);
				TextLeft             = home_row_delegate::inset + (aMarks ? home_row_delegate::mark_width : 0);
				const auto right     = Width - home_row_delegate::inset;
				SideLeft             = right - SideWidth;
				ChipLeft             = SideLeft - (SideWidth > 0 && ChipWidth > 0 ? home_row_delegate::column_gap : 0) - ChipWidth;
				const auto textRight = SideWidth > 0 || ChipWidth > 0 ? ChipLeft - home_row_delegate::column_gap : right;
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
				SecondHeight = Second.isEmpty() ? 0 : lay_out(SecondLayout, Second, SideFont, TextWidth, secondFormats);

				const auto content = (home_row_delegate::vertical_padding * 2) + FirstHeight
					+ (SecondHeight > 0 ? home_row_delegate::line_gap + SecondHeight : 0);
				const auto sideLines = (Time.isEmpty() ? 0 : 1) + (Progress.isEmpty() ? 0 : 1);
				const auto side      = (home_row_delegate::vertical_padding * 2)
					+ std::max(sideLines * SideLineHeight, actionExtent.height());
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
			QString     Time;
			QString     Progress;
			QString     Chip;
			QFont       SideFont;
			QTextLayout FirstLayout;
			QTextLayout SecondLayout;
			int         ReasonStart    = -1;
			int         ReasonLength   = 0;
			int         SideLineHeight = 0;
			int         SideWidth      = 0;
			int         ChipWidth      = 0;
			int         Width          = 0;
			int         TextLeft       = 0;
			int         TextWidth      = 1;
			int         SideLeft       = 0;
			int         ChipLeft       = 0;
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
		const row_layout layout{ option, aIndex, Marks, reasonInk };

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
		if (!layout.Second.isEmpty()) {
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
			aPainter->drawText(QRect{ layout.ChipLeft, layout.ContentTop, layout.ChipWidth, layout.SideLineHeight },
							   Qt::AlignRight | Qt::AlignVCenter, layout.Chip);
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
		const row_layout layout{ option, aIndex, Marks, QColor{ } };
		return { layout.Width, layout.TotalHeight };
	}

	auto home_row_delegate::action_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QRect
	{
		const auto action = aIndex.data(home_row_role::row_action).toString();
		if (action.isEmpty()) {
			return { };
		}
		const auto       size = action_size(aOption, action);
		const row_layout layout{ aOption, aIndex, false, QColor{ } };
		return QRect{ aOption.rect.left() + layout.Width - inset - size.width(),
					  aOption.rect.top() + ((layout.RowHeight - size.height()) / 2),
					  size.width(),
					  size.height() };
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
		const row_layout layout{ option, aIndex, Marks, QColor{ } };
		return layout.lines();
	}
} // namespace bpr::qt
