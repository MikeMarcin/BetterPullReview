#ifndef BPR_SRC_ADAPTERS_QT_SRC_DRAWER_TAB_BAR_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_DRAWER_TAB_BAR_HPP

#include <QPainter>
#include <QStyle>
#include <QStyleOptionTab>
#include <QStyleOptionTabBarBase>
#include <QTabBar>

#include <algorithm>

/// \file
/// \brief A drawer's tab row whose tabs are each as wide as what they show.
/// Review's right tray and, since [[BPR-1781]], Overview's drawer share it.
namespace bpr::qt::detail {
	/// \brief [[BPR-1592]] [[BPR-1684]]. The tray's tabs, each as wide
	/// as what it shows.
	///
	/// \details A tab is the style's own text tab for its name, plus its
	/// glyph and a gap when it shows one, plus its count when the count is
	/// showing. The style draws each tab's shape and its name; the bar
	/// places them, the glyph and the name as one group centred in the
	/// tab, or ending a gap before the count. Left to the style, the macOS
	/// style centres a tab's name and keeps its icon's width free on both
	/// sides, which made the Agent tab about 30 pixels wider than its spark
	/// and its name; the styles also kept room for the Comments count while
	/// it was hidden, and set a glyph-only tab's glyph off centre.
	class drawer_tab_bar final : public QTabBar
	{
	public:
		using QTabBar::QTabBar;

		/// \brief The width tab `aIndex` takes showing `aText` and, when
		/// `aGlyph`, its glyph; its count is counted while it is showing.
		[[nodiscard]] auto face_width(const int aIndex, const QString& aText, const bool aGlyph) const -> int
		{
			auto option = QStyleOptionTab{ };
			initStyleOption(&option, aIndex);
			option.text            = aText;
			option.icon            = QIcon{ };
			option.leftButtonSize  = QSize{ };
			option.rightButtonSize = QSize{ };
			const auto metrics     = fontMetrics();
			const auto text        = metrics.size(Qt::TextShowMnemonic, aText);
			const auto contents    = QSize{ text.width() + style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &option, this),
											std::max(metrics.height(), iconSize().height()) + style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &option, this) };
			auto       width       = style()->sizeFromContents(QStyle::CT_TabBarTab, &option, contents, this).width();
			if (aGlyph) {
				width += iconSize().width() + (aText.isEmpty() ? 0 : glyph_gap);
			}
			if (const auto* const count = tabButton(aIndex, QTabBar::RightSide); count != nullptr && !count->isHidden()) {
				width += glyph_gap + count->sizeHint().width();
			}
			return width;
		}

	protected:
		[[nodiscard]] auto tabSizeHint(const int aIndex) const -> QSize override
		{
			auto size = QTabBar::tabSizeHint(aIndex);
			size.setWidth(face_width(aIndex, tabText(aIndex), !tabIcon(aIndex).isNull()));
			return size;
		}

		/// A tab never elides: the window chooses a face that fits.
		[[nodiscard]] auto minimumTabSizeHint(const int aIndex) const -> QSize override
		{
			return tabSizeHint(aIndex);
		}

		/// \brief QTabBar's own painting (its base, then each tab with the
		/// open one last) with each label placed by the bar.
		void paintEvent(QPaintEvent* const aEvent) override
		{
			static_cast<void>(aEvent);
			auto painter = QPainter{ this };
			auto base    = QStyleOptionTabBarBase{ };
			base.initFrom(this);
			base.shape        = shape();
			base.documentMode = documentMode();
			auto overlap      = QStyleOptionTab{ };
			overlap.shape     = shape();
			if (const auto lap = style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &overlap, this); lap > 0) {
				base.rect = QRect{ 0, height() - lap, width(), lap };
			}
			for (auto index = 0; index < count(); ++index) {
				base.tabBarRect |= tabRect(index);
			}
			base.selectedTabRect = tabRect(currentIndex());
			if (drawBase()) {
				style()->drawPrimitive(QStyle::PE_FrameTabBarBase, &base, &painter, this);
			}
			for (auto index = 0; index < count(); ++index) {
				if (index != currentIndex()) {
					paint_tab(painter, index);
				}
			}
			if (currentIndex() >= 0) {
				paint_tab(painter, currentIndex());
			}
		}

	private:
		void paint_tab(QPainter& aPainter, const int aIndex) const
		{
			auto option = QStyleOptionTab{ };
			initStyleOption(&option, aIndex);
			if (!(option.state & QStyle::State_Enabled)) {
				option.palette.setCurrentColorGroup(QPalette::Disabled);
			}
			style()->drawControl(QStyle::CE_TabBarTabShape, &option, &aPainter, this);
			// The name, whole, drawn by the style as a plain text tab whose
			// centre is the name's place in the group.
			const auto text       = tabText(aIndex);
			const auto glyph      = !option.icon.isNull();
			auto       label      = option;
			label.text            = text;
			label.icon            = QIcon{ };
			label.leftButtonSize  = QSize{ };
			label.rightButtonSize = QSize{ };
			const auto textWidth  = text.isEmpty() ? 0 : fontMetrics().size(Qt::TextShowMnemonic, text).width();
			const auto group      = (glyph ? iconSize().width() + (text.isEmpty() ? 0 : glyph_gap) : 0) + textWidth;
			auto       left       = option.rect.left() + (option.rect.width() - group) / 2;
			if (const auto* const count = tabButton(aIndex, QTabBar::RightSide); count != nullptr && !count->isHidden()) {
				left = count->geometry().left() - glyph_gap - group;
			}
			const auto textLeft = left + group - textWidth;
			label.rect.translate(textLeft + textWidth / 2 - option.rect.center().x(), 0);
			style()->drawControl(QStyle::CE_TabBarTabLabel, &label, &aPainter, this);
			if (glyph) {
				const auto middle = style()->subElementRect(QStyle::SE_TabBarTabText, &label, this).center().y();
				const auto place  = QRect{ QPoint{ left, middle - iconSize().height() / 2 }, iconSize() };
				option.icon.paint(&aPainter, place, Qt::AlignCenter, (option.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled,
								  (option.state & QStyle::State_Selected) ? QIcon::On : QIcon::Off);
			}
		}

		/// The gap QTabBar and the styles leave between a glyph, a name
		/// and a tab button.
		static constexpr int glyph_gap = 4;
	};

} // namespace bpr::qt::detail

#endif
