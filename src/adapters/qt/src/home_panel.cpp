#include "bpr/qt/home_panel.hpp"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMargins>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QRectF>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		constexpr auto chevron_extent   = 12;
		constexpr auto header_gap       = 8;
		constexpr auto chevron_gap      = 6;
		constexpr auto summary_line_gap = 2;
		constexpr auto summary_scale    = 0.92;
		constexpr auto focus_ring_width = 2.0;
		constexpr auto focus_ring_inset = 2.0;

		[[nodiscard]] auto vertical_padding(const home_panel_style aStyle) noexcept -> int
		{
			return aStyle == home_panel_style::panel ? 8 : 5;
		}

		[[nodiscard]] auto leading_inset(const home_panel_style aStyle) noexcept -> int
		{
			return aStyle == home_panel_style::panel ? 10 : 8;
		}

		[[nodiscard]] auto trailing_inset(const home_panel_style aStyle) noexcept -> int
		{
			return aStyle == home_panel_style::panel ? 14 : 12;
		}

		[[nodiscard]] auto title_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			font.setWeight(QFont::DemiBold);
			return font;
		}

		[[nodiscard]] auto summary_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			if (font.pointSizeF() > 0.0) {
				font.setPointSizeF(font.pointSizeF() * summary_scale);
			}
			return font;
		}

		/// \brief Where each part of a header goes at one width.
		struct header_geometry
		{
			QRect Chevron;
			QRect Avatar;
			QRect Title;
			QRect Count;
			QRect Trailing;
			QRect Summary;
			bool  SummaryBelow = false;
			int   Height       = 0;
		};

		[[nodiscard]] auto measure_header(
			const home_panel& aPanel,
			const QFont&      aFont,
			const int         aWidth) -> header_geometry
		{
			const auto style        = aPanel.style_kind();
			const auto titleMetrics = QFontMetrics{ title_font(aFont) };
			const auto plainMetrics = QFontMetrics{ aFont };
			// [[BPR-0783]]. A group's avatar tile and its trailing chip each set
			// the header's line, so neither is clipped by a shorter one.
			const auto tile       = aPanel.avatar_name().isEmpty() ? 0 : home_tile_extent(aFont);
			const auto chip       = !aPanel.trailing().isEmpty() && aPanel.trailing_is_chip()
				? home_chip_size(summary_font(aFont), aPanel.trailing()).height()
				: 0;
			const auto lineHeight = std::max({ titleMetrics.height(), chevron_extent, tile, chip });
			const auto top        = vertical_padding(style);
			const auto rail       = aPanel.tone().isValid() ? home_panel::rail_width : 0;
			auto       geometry   = header_geometry{ };
			auto       x          = rail + leading_inset(style);
			geometry.Chevron      = QRect{ x, top + ((lineHeight - chevron_extent) / 2), chevron_extent, chevron_extent };
			x += chevron_extent + chevron_gap;
			if (tile > 0) {
				geometry.Avatar = QRect{ x, top + ((lineHeight - tile) / 2), tile, tile };
				x += tile + chevron_gap;
			}
			const auto titleWidth = titleMetrics.horizontalAdvance(aPanel.title());
			geometry.Title        = QRect{ x, top, titleWidth, lineHeight };
			x += titleWidth + header_gap;
			const auto countText = QString::number(aPanel.count());
			geometry.Count       = QRect{ x, top, plainMetrics.horizontalAdvance(countText), lineHeight };
			auto right           = aWidth - trailing_inset(style);
			geometry.Height      = (top * 2) + lineHeight;
			if (!aPanel.trailing().isEmpty()) {
				const auto trailingFont = summary_font(aFont);
				const auto extent       = aPanel.trailing_is_chip()
					? home_chip_size(trailingFont, aPanel.trailing())
					: QSize{ QFontMetrics{ trailingFont }.horizontalAdvance(aPanel.trailing()), lineHeight };
				geometry.Trailing       = QRect{ right - extent.width(),
												 top + ((lineHeight - extent.height()) / 2),
												 extent.width(),
												 extent.height() };
				right                   = geometry.Trailing.left() - header_gap;
			}
			if (aPanel.collapsed() && !aPanel.summary().isEmpty()) {
				const auto summaryMetrics = QFontMetrics{ summary_font(aFont) };
				const auto summaryWidth   = summaryMetrics.horizontalAdvance(aPanel.summary());
				if (geometry.Count.right() + (header_gap * 2) + summaryWidth <= right) {
					geometry.Summary = QRect{ right - summaryWidth, top, summaryWidth, lineHeight };
				} else {
					// A summary that does not fit beside the name wraps beneath it
					// rather than losing its end.
					const auto width  = std::max(1, right - geometry.Title.left());
					const auto bounds = summaryMetrics.boundingRect(
						QRect{ 0, 0, width, 1'000'000 }, Qt::TextWordWrap, aPanel.summary());
					geometry.Summary      = QRect{ geometry.Title.left(), top + lineHeight + summary_line_gap, width, bounds.height() };
					geometry.SummaryBelow = true;
					geometry.Height += summary_line_gap + bounds.height();
				}
			}
			return geometry;
		}

		/// \brief Announces a header as an expandable button with its state.
		class home_panel_header_accessible final : public QAccessibleWidget
		{
		public:
			explicit home_panel_header_accessible(home_panel_header* const aHeader)
				: QAccessibleWidget{ aHeader, QAccessible::Button }
			{
			}

			[[nodiscard]] auto state() const -> QAccessible::State override
			{
				auto       state     = QAccessibleWidget::state();
				const auto collapsed = header().panel().collapsed();
				state.expandable     = true;
				state.expanded       = !collapsed;
				state.collapsed      = collapsed;
				return state;
			}

			[[nodiscard]] auto actionNames() const -> QStringList override
			{
				auto names = QAccessibleWidget::actionNames();
				for (const auto& name : { QAccessibleActionInterface::pressAction(), QAccessibleActionInterface::toggleAction() }) {
					if (!names.contains(name)) {
						names.push_back(name);
					}
				}
				return names;
			}

			void doAction(const QString& aName) override
			{
				if (aName == QAccessibleActionInterface::pressAction()
					|| aName == QAccessibleActionInterface::toggleAction()) {
					auto& panel = header().panel();
					panel.set_collapsed(!panel.collapsed());
					return;
				}
				QAccessibleWidget::doAction(aName);
			}

		private:
			[[nodiscard]] auto header() const -> home_panel_header&
			{
				return *static_cast<home_panel_header*>(widget());
			}
		};

		[[nodiscard]] auto home_accessible_factory(const QString& aKey, QObject* const aObject)
			-> QAccessibleInterface*
		{
			static_cast<void>(aKey);
			if (auto* const header = dynamic_cast<home_panel_header*>(aObject); header != nullptr) {
				return new home_panel_header_accessible{ header };
			}
			return nullptr;
		}

		void install_home_accessibility()
		{
			static auto installed = false;
			if (!installed) {
				QAccessible::installFactory(&home_accessible_factory);
				installed = true;
			}
		}
	} // namespace

	// ---- home_panel_header --------------------------------------------------

	home_panel_header::home_panel_header(home_panel& aPanel)
		: QWidget{ &aPanel }
		, Panel{ aPanel }
	{
		setObjectName(QStringLiteral("homePanelHeader"));
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	auto home_panel_header::panel() const noexcept -> home_panel&
	{
		return Panel;
	}

	auto home_panel_header::chevron_rect() const -> QRect
	{
		return measure_header(Panel, font(), width()).Chevron;
	}

	auto home_panel_header::hasHeightForWidth() const -> bool
	{
		return true;
	}

	auto home_panel_header::heightForWidth(const int aWidth) const -> int
	{
		return measure_header(Panel, font(), aWidth).Height;
	}

	auto home_panel_header::sizeHint() const -> QSize
	{
		const auto width = std::max(this->width(), 1);
		return { width, heightForWidth(width) };
	}

	void home_panel_header::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto        painter  = QPainter{ this };
		const auto& colours  = palette();
		const auto  base     = colours.color(QPalette::Active, QPalette::Base);
		const auto  mid      = colours.color(QPalette::Active, QPalette::Mid);
		const auto  tone     = Panel.tone();
		const auto  tint     = tone.isValid() ? mix_colour(tone, base, home_strength::header)
											  : mix_colour(mid, base, home_strength::neutral_header);
		const auto  rule     = tone.isValid() ? mix_colour(tone, mid, home_strength::header_rule) : mid;
		const auto  panelled = Panel.style_kind() == home_panel_style::panel;
		painter.setRenderHint(QPainter::Antialiasing, true);

		// The panel's own outline, in header coordinates, so a header that has
		// moved down the panel stays inside it.
		auto outline = QPainterPath{ };
		outline.addRoundedRect(QRectF{ Panel.rect() }.adjusted(0.5, 0.5, -0.5, -0.5).translated(0.0, -static_cast<qreal>(Panel.header_offset())),
							   home_panel::radius, home_panel::radius);
		if (panelled) {
			painter.setClipPath(outline);
		}
		painter.fillRect(rect(), tint);
		if (!Panel.collapsed()) {
			painter.fillRect(QRect{ 0, height() - 1, width(), 1 }, rule);
		}
		if (panelled) {
			painter.setClipRect(rect());
			painter.setPen(QPen{ mid, 1.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawPath(outline);
			painter.setClipPath(outline);
		}
		if (tone.isValid()) {
			painter.fillRect(QRect{ 0, 0, home_panel::rail_width, height() }, mix_colour(tone, base, home_strength::rail));
		}
		painter.setClipping(false);

		const auto geometry = measure_header(Panel, font(), width());
		auto       arrow    = QStyleOption{ };
		arrow.initFrom(this);
		arrow.rect         = geometry.Chevron;
		const auto chevron = colours.color(QPalette::Active, QPalette::PlaceholderText);
		arrow.palette.setColor(QPalette::ButtonText, chevron);
		arrow.palette.setColor(QPalette::WindowText, chevron);
		arrow.palette.setColor(QPalette::Text, chevron);
		style()->drawPrimitive(Panel.collapsed() ? QStyle::PE_IndicatorArrowRight : QStyle::PE_IndicatorArrowDown,
							   &arrow, &painter, this);

		// [[BPR-1583]]. The group's avatar is its owner's own: an account or an
		// organization, or a repository's owner, in the colour that owner has on
		// every surface. Rows and bands keep colour for state.
		const auto neutral = home_chip_ink_for(colours, QPalette::Active, false);
		const auto grounds = identity_avatar_grounds_of(colours);
		if (!geometry.Avatar.isEmpty()) {
			const auto owner = Panel.avatar_name().section(QLatin1Char{ '/' }, 0, 0);
			paint_identity_avatar(painter, QRectF{ geometry.Avatar }.adjusted(0.5, 0.5, -0.5, -0.5),
								  { .Identity = owner, .Shape = Panel.avatar_round() ? identity_avatar_shape::person : identity_avatar_shape::group, .Generated = true },
								  grounds, font());
		}
		painter.setPen(colours.color(QPalette::Active, QPalette::Text));
		painter.setFont(title_font(font()));
		painter.drawText(geometry.Title, Qt::AlignLeft | Qt::AlignVCenter, Panel.title());
		painter.setPen(colours.color(QPalette::Active, QPalette::PlaceholderText));
		painter.setFont(font());
		painter.drawText(geometry.Count, Qt::AlignLeft | Qt::AlignVCenter, QString::number(Panel.count()));
		if (!geometry.Trailing.isEmpty()) {
			if (Panel.trailing_is_chip()) {
				draw_home_chip(painter, geometry.Trailing, Panel.trailing(), summary_font(font()), neutral, grounds);
			} else {
				painter.setFont(summary_font(font()));
				painter.setPen(colours.color(QPalette::Active, QPalette::PlaceholderText));
				painter.drawText(geometry.Trailing, Qt::AlignRight | Qt::AlignVCenter, Panel.trailing());
			}
		}
		if (!geometry.Summary.isEmpty()) {
			painter.setFont(summary_font(font()));
			painter.drawText(geometry.Summary,
							 static_cast<int>(geometry.SummaryBelow ? Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap : Qt::AlignRight | Qt::AlignVCenter),
							 Panel.summary());
		}
		if (hasFocus()) {
			// The palette's focus colour, as the bar's frame shows its own.
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(QPen{ colours.color(QPalette::Active, QPalette::Highlight), focus_ring_width });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ rect() }.adjusted(focus_ring_inset, focus_ring_inset, -focus_ring_inset, -focus_ring_inset),
									home_panel::radius - focus_ring_inset, home_panel::radius - focus_ring_inset);
		}
	}

	void home_panel_header::keyPressEvent(QKeyEvent* const aEvent)
	{
		switch (aEvent->key()) {
		case Qt::Key_Space:
		case Qt::Key_Return:
		case Qt::Key_Enter:
			Panel.set_collapsed(!Panel.collapsed());
			aEvent->accept();
			return;
		case Qt::Key_Left:
			Panel.set_collapsed(true);
			aEvent->accept();
			return;
		case Qt::Key_Right:
			Panel.set_collapsed(false);
			aEvent->accept();
			return;
		case Qt::Key_Up:
		case Qt::Key_Down:
			if (Panel.navigate(aEvent->key())) {
				aEvent->accept();
				return;
			}
			break;
		default:
			break;
		}
		QWidget::keyPressEvent(aEvent);
	}

	void home_panel_header::mousePressEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() == Qt::LeftButton) {
			aEvent->accept();
			return;
		}
		QWidget::mousePressEvent(aEvent);
	}

	void home_panel_header::mouseReleaseEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() == Qt::LeftButton && rect().contains(aEvent->position().toPoint())) {
			Panel.set_collapsed(!Panel.collapsed());
			aEvent->accept();
			return;
		}
		QWidget::mouseReleaseEvent(aEvent);
	}

	void home_panel_header::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::FontChange:
		case QEvent::StyleChange:
			updateGeometry();
			update();
			break;
		case QEvent::PaletteChange:
			update();
			break;
		default:
			break;
		}
	}

	// ---- home_panel ---------------------------------------------------------

	home_panel::home_panel(
		const std::optional<home_group> aState,
		const home_panel_style          aStyle,
		QWidget* const                  aParent)
		: QWidget{ aParent }
		, State{ aState }
		, Style{ aStyle }
	{
		install_home_accessibility();
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Header = new home_panel_header{ *this };
		Column = new QVBoxLayout{ this };
		Column->setSpacing(0);
		refresh_header();
	}

	void home_panel::set_title(const QString& aTitle)
	{
		if (Title == aTitle) {
			return;
		}
		Title = aTitle;
		refresh_header();
	}

	void home_panel::set_count(const int aCount)
	{
		if (Count == aCount) {
			return;
		}
		Count = aCount;
		refresh_header();
	}

	void home_panel::set_summary(const QString& aSummary)
	{
		if (Summary == aSummary) {
			return;
		}
		Summary = aSummary;
		refresh_header();
	}

	void home_panel::set_avatar(const QString& aName, const bool aRound)
	{
		if (Avatar == aName && AvatarRound == aRound) {
			return;
		}
		Avatar      = aName;
		AvatarRound = aRound;
		refresh_header();
	}

	void home_panel::set_trailing(const QString& aText, const bool aChip)
	{
		if (Trailing == aText && TrailingChip == aChip) {
			return;
		}
		Trailing     = aText;
		TrailingChip = aChip;
		refresh_header();
	}

	void home_panel::set_header_shown(const bool aShown)
	{
		if (HeaderShown == aShown) {
			return;
		}
		HeaderShown = aShown;
		Header->setVisible(aShown);
		refresh_header();
	}

	void home_panel::set_content(QWidget* const aContent)
	{
		if (Content == aContent) {
			return;
		}
		if (Content != nullptr) {
			Column->removeWidget(Content);
		}
		Content = aContent;
		if (Content != nullptr) {
			Content->setParent(this);
			Column->addWidget(Content);
			Content->setVisible(!Collapsed);
		}
		place_header();
	}

	void home_panel::set_collapsed(const bool aCollapsed)
	{
		if (Collapsed == aCollapsed) {
			return;
		}
		if (aCollapsed && Content != nullptr && Content->isAncestorOf(QApplication::focusWidget())) {
			// The keyboard stays on the panel it was in rather than falling to
			// whatever follows the content that is about to disappear.
			Header->setFocus(Qt::OtherFocusReason);
		}
		Collapsed = aCollapsed;
		if (Content != nullptr) {
			Content->setVisible(!Collapsed);
		}
		if (Collapsed) {
			HeaderOffset = 0;
		}
		refresh_header();
		auto changed      = QAccessible::State{ };
		changed.expanded  = true;
		changed.collapsed = true;
		auto event        = QAccessibleStateChangeEvent{ Header, changed };
		QAccessible::updateAccessibility(&event);
		if (CollapseObserver) {
			CollapseObserver(Collapsed);
		}
	}

	void home_panel::set_collapse_observer(std::function<void(bool)> aObserver)
	{
		CollapseObserver = std::move(aObserver);
	}

	void home_panel::set_header_navigator(std::function<bool(int aKey)> aNavigator)
	{
		Navigator = std::move(aNavigator);
	}

	void home_panel::set_state_colours(const home_state_colours& aColours)
	{
		Colours = aColours;
		update();
		Header->update();
	}

	void home_panel::set_header_offset(const int aOffset)
	{
		const auto limit   = std::max(0, height() - HeaderHeight);
		const auto clamped = Collapsed ? 0 : std::clamp(aOffset, 0, limit);
		if (clamped == HeaderOffset) {
			return;
		}
		HeaderOffset = clamped;
		place_header();
		Header->update();
	}

	auto home_panel::state() const noexcept -> std::optional<home_group>
	{
		return State;
	}

	auto home_panel::style_kind() const noexcept -> home_panel_style
	{
		return Style;
	}

	auto home_panel::title() const -> QString
	{
		return Title;
	}

	auto home_panel::count() const noexcept -> int
	{
		return Count;
	}

	auto home_panel::summary() const -> QString
	{
		return Summary;
	}

	auto home_panel::avatar_name() const -> QString
	{
		return Avatar;
	}

	auto home_panel::avatar_round() const noexcept -> bool
	{
		return AvatarRound;
	}

	auto home_panel::trailing() const -> QString
	{
		return Trailing;
	}

	auto home_panel::trailing_is_chip() const noexcept -> bool
	{
		return TrailingChip;
	}

	auto home_panel::header_shown() const noexcept -> bool
	{
		return HeaderShown;
	}

	auto home_panel::collapsed() const noexcept -> bool
	{
		return Collapsed;
	}

	auto home_panel::header() const noexcept -> home_panel_header*
	{
		return Header;
	}

	auto home_panel::header_offset() const noexcept -> int
	{
		return HeaderOffset;
	}

	auto home_panel::header_height() const noexcept -> int
	{
		return HeaderHeight;
	}

	auto home_panel::content() const noexcept -> QWidget*
	{
		return Content;
	}

	auto home_panel::tone() const -> QColor
	{
		return State.has_value() ? Colours.for_group(*State) : QColor{ };
	}

	auto home_panel::navigate(const int aKey) const -> bool
	{
		return Navigator && Navigator(aKey);
	}

	void home_panel::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto        painter = QPainter{ this };
		const auto& colours = palette();
		const auto  base    = colours.color(QPalette::Active, QPalette::Base);
		const auto  state   = tone();
		painter.setRenderHint(QPainter::Antialiasing, true);
		if (Style == home_panel_style::band) {
			if (state.isValid()) {
				painter.fillRect(QRect{ 0, 0, rail_width, height() }, mix_colour(state, base, home_strength::rail));
			}
			return;
		}
		auto outline = QPainterPath{ };
		outline.addRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
		painter.fillPath(outline, base);
		painter.setPen(QPen{ colours.color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.setBrush(Qt::NoBrush);
		painter.drawPath(outline);
		if (state.isValid()) {
			painter.setClipPath(outline);
			painter.fillRect(QRect{ 0, 0, rail_width, height() }, mix_colour(state, base, home_strength::rail));
		}
	}

	void home_panel::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		refresh_header();
	}

	void home_panel::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::FontChange:
		case QEvent::StyleChange:
			refresh_header();
			break;
		case QEvent::PaletteChange:
			update();
			break;
		default:
			break;
		}
	}

	void home_panel::refresh_header()
	{
		const auto panelled = Style == home_panel_style::panel;
		// [[BPR-0783]]. A headerless card keeps the border its top edge is.
		const auto header  = HeaderShown ? Header->heightForWidth(std::max(width(), 1)) : 0;
		const auto margins = QMargins{
			tone().isValid() ? rail_width : (panelled ? 1 : 0),
			header > 0 ? header : (panelled ? 1 : 0),
			panelled ? 1 : 0,
			panelled && !Collapsed ? 1 : 0,
		};
		HeaderHeight = header;
		if (Column->contentsMargins() != margins) {
			Column->setContentsMargins(margins);
			updateGeometry();
		}
		// [[BPR-0783]]. A single-account group's chip, or the host under Account
		// grouping, is part of what the header is announced as.
		const auto name        = Trailing.isEmpty()
			? QStringLiteral("%1, %2").arg(Title).arg(Count)
			: QStringLiteral("%1, %2, %3").arg(Title).arg(Count).arg(Trailing);
		const auto description = Collapsed ? Summary : QString{ };
		if (Header->accessibleName() != name) {
			Header->setAccessibleName(name);
		}
		if (Header->accessibleDescription() != description) {
			Header->setAccessibleDescription(description);
			Header->setToolTip(description);
		}
		place_header();
		Header->update();
	}

	void home_panel::place_header()
	{
		const auto limit = std::max(0, height() - HeaderHeight);
		HeaderOffset     = Collapsed ? 0 : std::clamp(HeaderOffset, 0, limit);
		Header->setGeometry(0, HeaderOffset, width(), HeaderHeight);
		Header->raise();
	}

	void stick_home_header(home_panel& aPanel, const QWidget& aViewport, const int aLine)
	{
		if (aPanel.isHidden()) {
			return;
		}
		const auto top = aPanel.mapTo(&aViewport, QPoint{ }).y();
		// [[BPR-0783]]. A card with no header has nothing to stick, and its rows
		// must not be covered by a header that is not drawn.
		aPanel.set_header_offset(aPanel.collapsed() || !aPanel.header_shown() ? 0 : aLine - top);
		if (aPanel.collapsed() || aPanel.content() == nullptr) {
			return;
		}
		const auto* const header       = aPanel.header();
		const auto        headerBottom = aPanel.header_shown()
			? header->mapTo(&aViewport, QPoint{ 0, header->height() }).y()
			: aLine;
		// The bands are the content's direct children. The panels carry no meta
		// object of their own, so they are found by type rather than by
		// `findChildren`.
		for (auto* const child : aPanel.content()->children()) {
			if (auto* const band = dynamic_cast<home_panel*>(child); band != nullptr) {
				stick_home_header(*band, aViewport, std::max(aLine, headerBottom));
			}
		}
	}
} // namespace bpr::qt
