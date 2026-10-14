#include "bpr/qt/diff_link_map.hpp"

#include "bpr/application/workbench_runtime_ports.hpp"

#include <QAccessible>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPalette>
#include <QPen>
#include <QPointer>
#include <QResizeEvent>
#include <QStyle>
#include <QStyleOptionFocusRect>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto valid_configuration(
			const diff_link_map_configuration& aConfiguration) noexcept -> bool
		{
			return aConfiguration.LogicalWidth >= static_cast<int>(application::minimum_center_ribbon_width)
				&& aConfiguration.LogicalWidth <= static_cast<int>(application::maximum_center_ribbon_width)
				&& std::isfinite(aConfiguration.ControlFraction)
				&& aConfiguration.ControlFraction > 0.0
				&& aConfiguration.ControlFraction < 0.5
				&& std::isfinite(aConfiguration.NearVisibleMargin)
				&& aConfiguration.NearVisibleMargin >= 0.0
				&& std::isfinite(aConfiguration.MoveMarkerExtent)
				&& aConfiguration.MoveMarkerExtent > 0.0;
		}

		[[nodiscard]] auto changed(const core::alignment_kind aKind) noexcept
			-> bool
		{
			return aKind != core::alignment_kind::equal;
		}

		[[nodiscard]] auto kind_text(const core::alignment_kind aKind)
			-> QString
		{
			switch (aKind) {
			case core::alignment_kind::replacement:
				return QStringLiteral("replacement");
			case core::alignment_kind::left_only:
				return QStringLiteral("deletion");
			case core::alignment_kind::right_only:
				return QStringLiteral("insertion");
			case core::alignment_kind::equal:
				return QStringLiteral("unchanged interval");
			}
			return QStringLiteral("change");
		}

		[[nodiscard]] auto with_alpha(QColor aColor, const int aAlpha) -> QColor
		{
			aColor.setAlpha(aAlpha);
			return aColor;
		}

		[[nodiscard]] auto identity_matches(
			const std::optional<diff_link_map_identity>& aIdentity,
			const diff_link_map_identity&                aEntry) noexcept -> bool
		{
			if (!aIdentity.has_value()) {
				return false;
			}
			return aIdentity->Group.has_value() && aEntry.Group.has_value()
				? aIdentity->Group == aEntry.Group
				: aIdentity->Segment == aEntry.Segment;
		}
	} // namespace

	one_pane_diff_link_map::one_pane_diff_link_map(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("diffOnePaneLinkMap"));
		setAccessibleName(QStringLiteral("One-pane diff correspondence map"));
		setAccessibleDescription(QStringLiteral("Joins removed and added rows of each visible change"));
		setFixedWidth(32);
	}

	void one_pane_diff_link_map::set_links(std::vector<visible_one_pane_diff_link>    aLinks,
										   const std::optional<core::change_group_id> aSelected, const QRectF aViewportBand)
	{
		Links        = std::move(aLinks);
		Selected     = aSelected;
		ViewportBand = aViewportBand;
		for (auto& link : Links) {
			const auto top    = link.Removed ? link.Removed->top() : link.Added->top();
			const auto bottom = link.Added ? link.Added->bottom() : link.Removed->bottom();
			const auto middle = (top + bottom) / 2.0;
			const auto half   = std::min(7.0, (bottom - top) / 2.0);
			link.Spine        = QLineF{ 7.0, middle - half, 7.0, middle + half };
			const auto ribbon = [this, &link](const QRectF& aBand, const bool aRemoved) {
				const auto edge        = static_cast<qreal>(width());
				const auto spineTop    = aRemoved ? link.Spine.y1() : link.Spine.center().y();
				const auto spineBottom = aRemoved ? link.Spine.center().y() : link.Spine.y2();
				auto       path        = QPainterPath{ QPointF{ edge, aBand.top() } };
				path.cubicTo(edge * 0.65, aBand.top(), edge * 0.5, spineTop, 7.0, spineTop);
				path.lineTo(7.0, spineBottom);
				path.cubicTo(edge * 0.5, spineBottom, edge * 0.65, aBand.bottom(), edge, aBand.bottom());
				path.closeSubpath();
				return path;
			};
			if (link.Removed) {
				link.RemovedRibbon = ribbon(*link.Removed, true);
			}
			if (link.Added) {
				link.AddedRibbon = ribbon(*link.Added, false);
			}
		}
		update();
	}

	auto one_pane_diff_link_map::visible_links() const noexcept -> std::span<const visible_one_pane_diff_link>
	{
		return Links;
	}

	void one_pane_diff_link_map::set_selection_sink(diff_link_map_identity_sink aSink)
	{
		SelectionSink = std::move(aSink);
	}

	void one_pane_diff_link_map::paintEvent(QPaintEvent* const aEvent)
	{
		QWidget::paintEvent(aEvent);
		auto painter = QPainter{ this };
		painter.setClipRect(ViewportBand);
		painter.setRenderHint(QPainter::Antialiasing, true);
		// Keep the two-pane map's ribbon fills and selected edge treatment.
		const auto normal       = with_alpha(palette().color(QPalette::WindowText), 144);
		const auto selectedFill = with_alpha(palette().color(QPalette::Highlight), 168);
		for (const auto& link : Links) {
			const auto selected = Selected && Selected == link.Identity.Group;
			for (const auto* path : { &link.RemovedRibbon, &link.AddedRibbon }) {
				painter.fillPath(*path, normal);
				if (selected) {
					painter.fillPath(*path, selectedFill);
					painter.setPen(QPen{ palette().color(QPalette::WindowText), 1.5 });
					painter.drawPath(*path);
				}
			}
			painter.setPen(QPen{ palette().color(selected ? QPalette::Highlight : QPalette::PlaceholderText), 2.0 });
			painter.drawLine(link.Spine);
		}
	}

	void one_pane_diff_link_map::mousePressEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() == Qt::LeftButton && ViewportBand.contains(aEvent->position())) {
			for (const auto& link : Links) {
				const auto hit = link.RemovedRibbon.contains(aEvent->position())
					|| link.AddedRibbon.contains(aEvent->position())
					|| QRectF{ link.Spine.p1(), link.Spine.p2() }.adjusted(-4.0, -3.0, 4.0, 3.0).contains(aEvent->position());
				if (hit && SelectionSink) {
					const auto identity = link.Identity;
					SelectionSink(identity);
					aEvent->accept();
					return;
				}
			}
		}
		QWidget::mousePressEvent(aEvent);
	}

	diff_link_map::diff_link_map(
		const Qt::Orientation       aOrientation,
		QSplitter&                  aParent,
		diff_link_map_configuration aConfiguration)
		: QSplitterHandle{ aOrientation, &aParent }
		, Configuration{ valid_configuration(aConfiguration)
							 ? aConfiguration
							 : diff_link_map_configuration{ } }
	{
		setObjectName(QStringLiteral("diffLinkMap"));
		setAccessibleName(QStringLiteral("Diff correspondence map"));
		setFocusPolicy(Qt::StrongFocus);
		setMouseTracking(true);
		setAttribute(Qt::WA_OpaquePaintEvent, false);
		update_accessible_status();
	}

	void diff_link_map::set_logical_width(const int aWidth)
	{
		auto next         = Configuration;
		next.LogicalWidth = aWidth;
		if (!valid_configuration(next) || Configuration.LogicalWidth == aWidth) {
			return;
		}
		Configuration = next;
		splitter()->setHandleWidth(aWidth);
		updateGeometry();
	}

	void diff_link_map::set_geometry(
		measured_alignment_axis                    aAxis,
		const std::span<const core::move_relation> aMoves)
	{
		const auto sameComparison = Axis.has_value()
			&& Axis->comparison() == aAxis.comparison();
		Axis = std::move(aAxis);
		Entries.clear();
		Moves.clear();
		EntryBySegment.clear();
		VisibleEntryIndexes.clear();
		VisibleMoveIndexes.clear();

		for (const auto& segment : Axis->segments()) {
			if (!changed(segment.Kind)) {
				continue;
			}
			const auto index = Entries.size();
			Entries.push_back(retained_entry{
				.Identity = {
					.Segment = segment.Id,
					.Group   = segment.Group,
				},
				.Kind       = segment.Kind,
				.LeftBegin  = segment.LeftBegin,
				.LeftEnd    = segment.LeftEnd,
				.RightBegin = segment.RightBegin,
				.RightEnd   = segment.RightEnd,
			});
			EntryBySegment.emplace(segment.Id, index);
		}

		for (const auto& move : aMoves) {
			const auto source      = EntryBySegment.find(move.SourceDeletion);
			const auto destination = EntryBySegment.find(move.DestinationInsertion);
			if (!move.Id.valid() || source == EntryBySegment.end()
				|| destination == EntryBySegment.end()
				|| Entries[source->second].Kind != core::alignment_kind::left_only
				|| Entries[destination->second].Kind
					!= core::alignment_kind::right_only
				|| Entries[source->second].SourceMove.has_value()
				|| Entries[destination->second].DestinationMove.has_value()) {
				continue;
			}
			const auto moveIndex = Moves.size();
			Moves.push_back({
				.Id          = move.Id,
				.Source      = source->second,
				.Destination = destination->second,
			});
			Entries[source->second].SourceMove           = moveIndex;
			Entries[destination->second].DestinationMove = moveIndex;
		}
		NavigationEntryIndexes.clear();
		NavigationEntryIndexes.reserve(Entries.size());
		for (std::size_t index = 0U; index < Entries.size(); ++index) {
			const auto sameGroup = !NavigationEntryIndexes.empty()
				&& Entries[index].Identity.Group.has_value()
				&& Entries[NavigationEntryIndexes.back()].Identity.Group
					== Entries[index].Identity.Group;
			if (!sameGroup) {
				NavigationEntryIndexes.push_back(index);
			}
		}

		if (!sameComparison) {
			Frame.reset();
		}
		if (Selection.has_value() && !identity_exists(*Selection)) {
			Selection.reset();
		}
		VisibleLinks.reserve(Entries.size());
		VisibleMoveOverlays.reserve(Moves.size());
		CandidateEntryIndexes.reserve(Entries.size());
		VisibleEntryIndexes.reserve(Entries.size());
		VisibleMoveIndexes.reserve(Moves.size());
		VisibleLinkByEntry.resize(Entries.size());
		EntryVisibilityMarks.assign(Entries.size(), false);
		MoveVisibilityMarks.assign(Moves.size(), false);
		++Statistics.GeometryRebuilds;
		Statistics.RetainedChangedEntries = Entries.size();
		rebuild_visible_geometry();
		update_accessible_status();
		update();
		if (refresh_hover_from_pointer()) {
			invoke_hover_sink();
		}
	}

	void diff_link_map::clear_geometry()
	{
		Axis.reset();
		Frame.reset();
		Entries.clear();
		Moves.clear();
		EntryBySegment.clear();
		NavigationEntryIndexes.clear();
		VisibleLinks.clear();
		VisibleMoveOverlays.clear();
		CandidateEntryIndexes.clear();
		VisibleEntryIndexes.clear();
		VisibleLinkByEntry.clear();
		VisibleMoveIndexes.clear();
		EntryVisibilityMarks.clear();
		MoveVisibilityMarks.clear();
		Selection.reset();
		Statistics.RetainedChangedEntries = 0U;
		Statistics.LastCandidateEntries   = 0U;
		Statistics.LastVisiblePaths       = 0U;
		Statistics.LastMoveMarkers        = 0U;
		Statistics.LastMoveOverlays       = 0U;
		update_accessible_status();
		update();
		if (refresh_hover_from_pointer()) {
			invoke_hover_sink();
		}
	}

	auto diff_link_map::set_frame(const comparison_scroll_frame& aFrame) -> bool
	{
		if (!Axis.has_value() || aFrame.Comparison != Axis->comparison()
			|| !std::isfinite(aFrame.Position)
			|| !std::isfinite(aFrame.Left.DocumentOriginY)
			|| aFrame.Left.DocumentOriginY < 0.0
			|| !std::isfinite(aFrame.Right.DocumentOriginY)
			|| aFrame.Right.DocumentOriginY < 0.0) {
			++Statistics.RejectedStaleFrames;
			return false;
		}
		Frame = aFrame;
		++Statistics.FrameUpdates;
		rebuild_visible_geometry();
		update();
		if (refresh_hover_from_pointer()) {
			invoke_hover_sink();
		}
		return true;
	}

	auto diff_link_map::current_frame() const noexcept
		-> const std::optional<comparison_scroll_frame>&
	{
		return Frame;
	}

	void diff_link_map::set_selected(
		std::optional<diff_link_map_identity> aSelection,
		const bool                            aAnnounce)
	{
		if (aSelection.has_value() && !identity_exists(*aSelection)) {
			aSelection.reset();
		}
		if (Selection == aSelection) {
			return;
		}
		Selection = std::move(aSelection);
		rebuild_visible_geometry();
		update_accessible_status();
		if (aAnnounce && Selection.has_value()) {
			const auto self  = QPointer<diff_link_map>{ this };
			auto       event = QAccessibleAnnouncementEvent{
				this, accessibleDescription()
			};
			event.setPoliteness(
				QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
			if (self == nullptr) {
				return;
			}
		}
		update();
	}

	auto diff_link_map::selected() const noexcept
		-> const std::optional<diff_link_map_identity>&
	{
		return Selection;
	}

	auto diff_link_map::hovered() const noexcept
		-> const std::optional<diff_link_map_identity>&
	{
		return Hover;
	}

	void diff_link_map::set_selection_sink(diff_link_map_identity_sink aSink)
	{
		SelectionSink = std::move(aSink);
	}

	void diff_link_map::set_selection_announcements_enabled(
		const bool aEnabled) noexcept
	{
		SelectionAnnouncementsEnabled = aEnabled;
	}

	void diff_link_map::set_hover_sink(diff_link_map_hover_sink aSink)
	{
		HoverSink = std::move(aSink);
	}

	void diff_link_map::set_reveal_sink(diff_link_map_identity_sink aSink)
	{
		RevealSink = std::move(aSink);
	}

	auto diff_link_map::hit_test(const QPointF aPosition)
		-> std::optional<diff_link_map_identity>
	{
		++Statistics.HitTests;
		auto stroker = QPainterPathStroker{ };
		stroker.setWidth(6.0);
		for (auto link = VisibleLinks.rbegin(); link != VisibleLinks.rend(); ++link) {
			if (link->Path.contains(aPosition)) {
				return link->Identity;
			}
			if (stroker.createStroke(link->Path).contains(aPosition)) {
				return link->Identity;
			}
		}
		return std::nullopt;
	}

	auto diff_link_map::visible_links() const noexcept
		-> std::span<const visible_diff_link>
	{
		return VisibleLinks;
	}

	auto diff_link_map::visible_move_overlays() const noexcept
		-> std::span<const visible_diff_move_overlay>
	{
		return VisibleMoveOverlays;
	}

	auto diff_link_map::statistics() const noexcept -> diff_link_map_statistics
	{
		return Statistics;
	}

	auto diff_link_map::sizeHint() const -> QSize
	{
		const auto base = QSplitterHandle::sizeHint();
		return orientation() == Qt::Horizontal
			? QSize{ Configuration.LogicalWidth, base.height() }
			: QSize{ base.width(), Configuration.LogicalWidth };
	}

	auto diff_link_map::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void diff_link_map::paintEvent(QPaintEvent* const aEvent)
	{
		QSplitterHandle::paintEvent(aEvent);
		auto painter = QPainter{ this };
		painter.setClipRect(rect());
		painter.setRenderHint(QPainter::Antialiasing, true);

		const auto normal = with_alpha(
			palette().color(QPalette::WindowText), 144);
		const auto hoverFill = with_alpha(
			palette().color(QPalette::Highlight), 112);
		const auto selectedFill = with_alpha(
			palette().color(QPalette::Highlight), 168);
		const auto outline         = palette().color(QPalette::Highlight);
		const auto selectedOutline = palette().color(QPalette::WindowText);

		auto ordinaryPaths = QPainterPath{ };
		for (const auto& link : VisibleLinks) {
			ordinaryPaths.addPath(link.Path);
		}
		painter.fillPath(ordinaryPaths, normal);

		Statistics.LastPaintedPaths = 0U;
		Statistics.LastMoveMarkers  = 0U;
		for (std::size_t index = 0U; index < VisibleLinks.size(); ++index) {
			const auto& link  = VisibleLinks[index];
			const auto& entry = Entries[VisibleEntryIndexes[index]];
			if (entry_hovered(entry)) {
				painter.fillPath(link.Path, hoverFill);
				painter.setPen(QPen{ outline, 1.0 });
				painter.drawPath(link.Path);
			}
			if (entry_selected(entry)) {
				painter.fillPath(link.Path, selectedFill);
				painter.setPen(QPen{ selectedOutline, 1.5 });
				painter.drawPath(link.Path);
			}

			const auto extent = Configuration.MoveMarkerExtent;
			painter.setPen(Qt::NoPen);
			painter.setBrush(outline);
			if (link.SourceMarkerY.has_value()) {
				painter.drawPolygon(QPolygonF{
					QPointF{ 0.0, *link.SourceMarkerY },
					QPointF{ extent, *link.SourceMarkerY - extent },
					QPointF{ extent, *link.SourceMarkerY + extent },
				});
				++Statistics.LastMoveMarkers;
			}
			if (link.DestinationMarkerY.has_value()) {
				painter.drawPolygon(QPolygonF{
					QPointF{ static_cast<qreal>(width()),
							 *link.DestinationMarkerY },
					QPointF{ static_cast<qreal>(width()) - extent,
							 *link.DestinationMarkerY - extent },
					QPointF{ static_cast<qreal>(width()) - extent,
							 *link.DestinationMarkerY + extent },
				});
				++Statistics.LastMoveMarkers;
			}
			++Statistics.LastPaintedPaths;
		}
		Statistics.PaintedPathsVisited += Statistics.LastPaintedPaths;

		Statistics.LastMoveOverlays = 0U;
		for (const auto& overlay : VisibleMoveOverlays) {
			auto pen = QPen{ outline, 1.5, Qt::DashLine };
			pen.setCapStyle(Qt::RoundCap);
			painter.setPen(pen);
			painter.setBrush(Qt::NoBrush);
			painter.drawPath(overlay.Path);
			++Statistics.LastMoveOverlays;
		}

		if (hasFocus()) {
			auto option = QStyleOptionFocusRect{ };
			option.initFrom(this);
			option.rect = rect().adjusted(2, 2, -2, -2);
			style()->drawPrimitive(
				QStyle::PE_FrameFocusRect, &option, &painter, this);
		}
		++Statistics.PaintEvents;
	}

	void diff_link_map::resizeEvent(QResizeEvent* const aEvent)
	{
		QSplitterHandle::resizeEvent(aEvent);
		rebuild_visible_geometry();
		if (refresh_hover_from_pointer()) {
			invoke_hover_sink();
		}
	}

	void diff_link_map::changeEvent(QEvent* const aEvent)
	{
		QSplitterHandle::changeEvent(aEvent);
		if (aEvent == nullptr) {
			return;
		}
		switch (aEvent->type()) {
		case QEvent::PaletteChange:
		case QEvent::ApplicationPaletteChange:
		case QEvent::FontChange:
		case QEvent::ApplicationFontChange:
		case QEvent::StyleChange:
		case QEvent::ThemeChange:
			++Statistics.PaletteOrStyleChanges;
			update();
			break;
		default:
			break;
		}
	}

	void diff_link_map::mousePressEvent(QMouseEvent* const aEvent)
	{
		if (aEvent == nullptr || aEvent->button() != Qt::LeftButton
			|| orientation() != Qt::Horizontal) {
			QSplitterHandle::mousePressEvent(aEvent);
			return;
		}
		Pressed             = true;
		Dragging            = false;
		PointerPosition     = aEvent->position();
		PressGlobal         = aEvent->globalPosition();
		PressHandlePosition = handle_position();
		PressedIdentity     = hit_test(aEvent->position());
		setFocus(Qt::MouseFocusReason);
		aEvent->accept();
	}

	void diff_link_map::mouseMoveEvent(QMouseEvent* const aEvent)
	{
		if (aEvent == nullptr) {
			return;
		}
		PointerPosition = aEvent->position();
		if (Pressed && aEvent->buttons().testFlag(Qt::LeftButton)) {
			const auto delta = aEvent->globalPosition() - PressGlobal;
			if (!Dragging
				&& delta.manhattanLength()
					>= static_cast<qreal>(QApplication::startDragDistance())) {
				Dragging = true;
			}
			if (Dragging) {
				moveSplitter(
					PressHandlePosition
					+ static_cast<int>(std::llround(coordinate(delta))));
			}
			aEvent->accept();
			return;
		}
		update_hover(hit_test(aEvent->position()));
		aEvent->accept();
	}

	void diff_link_map::mouseReleaseEvent(QMouseEvent* const aEvent)
	{
		if (aEvent == nullptr || aEvent->button() != Qt::LeftButton || !Pressed) {
			QSplitterHandle::mouseReleaseEvent(aEvent);
			return;
		}
		const auto released = hit_test(aEvent->position());
		const auto select   = !Dragging && PressedIdentity.has_value()
			&& released == PressedIdentity;
		const auto identity = PressedIdentity;
		Pressed             = false;
		Dragging            = false;
		PointerPosition     = aEvent->position();
		PressedIdentity.reset();
		aEvent->accept();
		if (select && identity.has_value()) {
			const auto index = entry_index(*identity);
			if (index.has_value()) {
				choose_entry(*index, false);
			}
		}
	}

	void diff_link_map::mouseDoubleClickEvent(QMouseEvent* const aEvent)
	{
		if (aEvent == nullptr || aEvent->button() != Qt::LeftButton) {
			QSplitterHandle::mouseDoubleClickEvent(aEvent);
			return;
		}
		const auto identity = hit_test(aEvent->position());
		PointerPosition     = aEvent->position();
		aEvent->accept();
		if (identity.has_value()) {
			const auto index = entry_index(*identity);
			if (index.has_value()) {
				choose_entry(*index, true);
			}
		}
	}

	void diff_link_map::leaveEvent(QEvent* const aEvent)
	{
		QSplitterHandle::leaveEvent(aEvent);
		if (!Pressed) {
			PointerPosition.reset();
			update_hover(std::nullopt);
		}
	}

	void diff_link_map::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (aEvent == nullptr || aEvent->modifiers() != Qt::NoModifier
			|| Entries.empty()) {
			QSplitterHandle::keyPressEvent(aEvent);
			return;
		}
		auto index = Selection.has_value()
			? navigation_index(*Selection)
			: std::optional<std::size_t>{ };
		switch (aEvent->key()) {
		case Qt::Key_Up:
			index = index.has_value() && *index > 0U ? *index - 1U : 0U;
			break;
		case Qt::Key_Down:
			index = index.has_value()
				? std::min(*index + 1U, NavigationEntryIndexes.size() - 1U)
				: 0U;
			break;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Space:
			index = index.value_or(0U);
			aEvent->accept();
			choose_entry(NavigationEntryIndexes[*index], true);
			return;
		default:
			QSplitterHandle::keyPressEvent(aEvent);
			return;
		}
		aEvent->accept();
		choose_entry(NavigationEntryIndexes[*index], false);
	}

	auto diff_link_map::identity_exists(
		const diff_link_map_identity& aIdentity) const noexcept -> bool
	{
		const auto found = EntryBySegment.find(aIdentity.Segment);
		return found != EntryBySegment.end()
			&& Entries[found->second].Identity.Group == aIdentity.Group;
	}

	auto diff_link_map::entry_selected(const retained_entry& aEntry) const
		noexcept -> bool
	{
		return identity_matches(Selection, aEntry.Identity);
	}

	auto diff_link_map::entry_hovered(const retained_entry& aEntry) const
		noexcept -> bool
	{
		return identity_matches(Hover, aEntry.Identity);
	}

	auto diff_link_map::entry_index(
		const diff_link_map_identity& aIdentity) const noexcept
		-> std::optional<std::size_t>
	{
		const auto found = EntryBySegment.find(aIdentity.Segment);
		return found != EntryBySegment.end()
			? std::optional<std::size_t>{ found->second }
			: std::nullopt;
	}

	auto diff_link_map::navigation_index(
		const diff_link_map_identity& aIdentity) const noexcept
		-> std::optional<std::size_t>
	{
		const auto entry = entry_index(aIdentity);
		if (!entry.has_value() || NavigationEntryIndexes.empty()) {
			return std::nullopt;
		}
		const auto following = std::ranges::upper_bound(
			NavigationEntryIndexes, *entry);
		return static_cast<std::size_t>(
			std::distance(NavigationEntryIndexes.begin(), following) - 1);
	}

	void diff_link_map::set_viewport_band(
		const qreal aTopOffset, const qreal aHeight)
	{
		if (ViewportBandTop == aTopOffset && ViewportBandHeight == aHeight) {
			return;
		}
		ViewportBandTop    = aTopOffset;
		ViewportBandHeight = aHeight;
		rebuild_visible_geometry();
		update();
	}

	auto diff_link_map::band_height() const noexcept -> qreal
	{
		return ViewportBandHeight > 0.0 ? ViewportBandHeight
										: static_cast<qreal>(height());
	}

	void diff_link_map::rebuild_visible_geometry()
	{
		const auto candidateCapacity = CandidateEntryIndexes.capacity();
		const auto linkCapacity      = VisibleLinks.capacity();
		const auto entryCapacity     = VisibleEntryIndexes.capacity();
		const auto moveCapacity      = VisibleMoveIndexes.capacity();
		const auto overlayCapacity   = VisibleMoveOverlays.capacity();
		for (const auto index : VisibleEntryIndexes) {
			EntryVisibilityMarks[index] = false;
		}
		for (const auto index : VisibleMoveIndexes) {
			MoveVisibilityMarks[index] = false;
		}
		VisibleLinks.clear();
		VisibleMoveOverlays.clear();
		CandidateEntryIndexes.clear();
		VisibleEntryIndexes.clear();
		VisibleMoveIndexes.clear();
		Statistics.LastCandidateEntries = 0U;
		Statistics.LastVisiblePaths     = 0U;
		Statistics.LastMoveMarkers      = 0U;
		Statistics.LastMoveOverlays     = 0U;
		++Statistics.VisibilityQueries;
		if (!Axis.has_value() || !Frame.has_value() || Entries.empty()
			|| width() <= 0 || height() <= 0) {
			return;
		}

		const auto margin    = Configuration.NearVisibleMargin;
		const auto leftLow   = Frame->Left.DocumentOriginY - margin;
		const auto leftHigh  = Frame->Left.DocumentOriginY + band_height() + margin;
		const auto rightLow  = Frame->Right.DocumentOriginY - margin;
		const auto rightHigh = Frame->Right.DocumentOriginY + band_height() + margin;

		const auto leftFirst = std::lower_bound(
			Entries.begin(), Entries.end(), leftLow,
			[](const retained_entry& aEntry, const qreal aValue) {
				return aEntry.LeftEnd < aValue;
			});
		const auto leftLast = std::upper_bound(
			leftFirst, Entries.end(), leftHigh,
			[](const qreal aValue, const retained_entry& aEntry) {
				return aValue < aEntry.LeftBegin;
			});
		const auto rightFirst = std::lower_bound(
			Entries.begin(), Entries.end(), rightLow,
			[](const retained_entry& aEntry, const qreal aValue) {
				return aEntry.RightEnd < aValue;
			});
		const auto rightLast = std::upper_bound(
			rightFirst, Entries.end(), rightHigh,
			[](const qreal aValue, const retained_entry& aEntry) {
				return aValue < aEntry.RightBegin;
			});
		Statistics.RangeSearches += 4U;

		auto leftIndex = static_cast<std::size_t>(
			std::distance(Entries.begin(), leftFirst));
		const auto leftEnd = static_cast<std::size_t>(
			std::distance(Entries.begin(), leftLast));
		auto rightIndex = static_cast<std::size_t>(
			std::distance(Entries.begin(), rightFirst));
		const auto rightEnd = static_cast<std::size_t>(
			std::distance(Entries.begin(), rightLast));
		while (leftIndex < leftEnd || rightIndex < rightEnd) {
			if (rightIndex >= rightEnd
				|| (leftIndex < leftEnd && leftIndex < rightIndex)) {
				CandidateEntryIndexes.push_back(leftIndex++);
			} else if (leftIndex >= leftEnd || rightIndex < leftIndex) {
				CandidateEntryIndexes.push_back(rightIndex++);
			} else {
				CandidateEntryIndexes.push_back(leftIndex);
				++leftIndex;
				++rightIndex;
			}
		}
		Statistics.LastCandidateEntries = CandidateEntryIndexes.size();
		Statistics.CandidateEntriesVisited
			+= Statistics.LastCandidateEntries;
		Statistics.PeakCandidateEntries = std::max(
			Statistics.PeakCandidateEntries,
			Statistics.LastCandidateEntries);

		const auto clip = QRectF{
			0.0,
			ViewportBandTop - margin,
			static_cast<qreal>(width()),
			band_height() + 2.0 * margin,
		};
		for (const auto index : CandidateEntryIndexes) {
			const auto path = path_for(Entries[index]);
			if (!path.boundingRect().intersects(clip)) {
				continue;
			}
			const auto& entry           = Entries[index];
			EntryVisibilityMarks[index] = true;
			VisibleLinkByEntry[index]   = VisibleLinks.size();
			VisibleLinks.push_back({
				.Identity           = entry.Identity,
				.Kind               = entry.Kind,
				.Path               = path,
				.LeftTop            = entry.LeftBegin - Frame->Left.DocumentOriginY + ViewportBandTop,
				.LeftBottom         = entry.LeftEnd - Frame->Left.DocumentOriginY + ViewportBandTop,
				.RightTop           = entry.RightBegin - Frame->Right.DocumentOriginY + ViewportBandTop,
				.RightBottom        = entry.RightEnd - Frame->Right.DocumentOriginY + ViewportBandTop,
				.SourceMove         = entry.SourceMove.has_value()
					? std::optional<core::move_relation_id>{
						  Moves[*entry.SourceMove].Id }
					: std::nullopt,
				.DestinationMove    = entry.DestinationMove.has_value() ? std::optional<core::move_relation_id>{ Moves[*entry.DestinationMove].Id } : std::nullopt,
				.SourceMarkerY      = entry.SourceMove.has_value() ? std::optional<qreal>{ (entry.LeftBegin + entry.LeftEnd) / 2.0 - Frame->Left.DocumentOriginY + ViewportBandTop } : std::nullopt,
				.DestinationMarkerY = entry.DestinationMove.has_value() ? std::optional<qreal>{ (entry.RightBegin + entry.RightEnd) / 2.0 - Frame->Right.DocumentOriginY + ViewportBandTop } : std::nullopt,
			});
			VisibleEntryIndexes.push_back(index);
		}
		const auto appendVisibleMove = [this](const std::size_t aMove) {
			if (!MoveVisibilityMarks[aMove]) {
				MoveVisibilityMarks[aMove] = true;
				VisibleMoveIndexes.push_back(aMove);
			}
		};
		for (const auto index : VisibleEntryIndexes) {
			const auto& entry = Entries[index];
			if (!entry_selected(entry) && !entry_hovered(entry)) {
				continue;
			}
			if (entry.SourceMove.has_value()) {
				appendVisibleMove(*entry.SourceMove);
			}
			if (entry.DestinationMove.has_value()) {
				appendVisibleMove(*entry.DestinationMove);
			}
		}
		for (const auto moveIndex : VisibleMoveIndexes) {
			const auto& move = Moves[moveIndex];
			if (!EntryVisibilityMarks[move.Source]
				|| !EntryVisibilityMarks[move.Destination]) {
				continue;
			}
			const auto sourceIndex      = VisibleLinkByEntry[move.Source];
			const auto destinationIndex = VisibleLinkByEntry[move.Destination];
			const auto sourceY          = *VisibleLinks[sourceIndex].SourceMarkerY;
			const auto destinationY
				= *VisibleLinks[destinationIndex].DestinationMarkerY;
			auto path = QPainterPath{ QPointF{ 0.0, sourceY } };
			path.cubicTo(
				static_cast<qreal>(width()) * Configuration.ControlFraction,
				sourceY,
				static_cast<qreal>(width())
					* (1.0 - Configuration.ControlFraction),
				destinationY,
				static_cast<qreal>(width()),
				destinationY);
			VisibleMoveOverlays.push_back({
				.Relation     = move.Id,
				.Path         = std::move(path),
				.SourceY      = sourceY,
				.DestinationY = destinationY,
			});
		}
		Statistics.LastVisiblePaths = VisibleLinks.size();
		Statistics.VisiblePathsBuilt += Statistics.LastVisiblePaths;
		Statistics.PeakVisiblePaths = std::max(
			Statistics.PeakVisiblePaths,
			Statistics.LastVisiblePaths);
		Statistics.CapacityGrowths
			+= static_cast<std::uint64_t>(
				   CandidateEntryIndexes.capacity() > candidateCapacity)
			+ static_cast<std::uint64_t>(VisibleLinks.capacity() > linkCapacity)
			+ static_cast<std::uint64_t>(
				   VisibleEntryIndexes.capacity() > entryCapacity)
			+ static_cast<std::uint64_t>(
				   VisibleMoveIndexes.capacity() > moveCapacity)
			+ static_cast<std::uint64_t>(
				   VisibleMoveOverlays.capacity() > overlayCapacity);
	}

	auto diff_link_map::refresh_hover_from_pointer() -> bool
	{
		const auto next = PointerPosition.has_value()
			? hit_test(*PointerPosition)
			: std::optional<diff_link_map_identity>{ };
		if (Hover == next) {
			return false;
		}
		Hover = next;
		rebuild_visible_geometry();
		update();
		return true;
	}

	void diff_link_map::update_accessible_status()
	{
		if (!Selection.has_value()) {
			setAccessibleDescription(
				QStringLiteral("%1 semantic changes. Up and Down choose a change; Enter reveals it.")
					.arg(NavigationEntryIndexes.size()));
			return;
		}
		const auto index      = entry_index(*Selection);
		const auto navigation = navigation_index(*Selection);
		if (!index.has_value() || !navigation.has_value()) {
			setAccessibleDescription(QStringLiteral("No selected change."));
			return;
		}
		const auto& entry = Entries[*index];
		auto        moved = QString{ };
		if (entry.SourceMove.has_value()) {
			moved = QStringLiteral(", moved-from marker");
		} else if (entry.DestinationMove.has_value()) {
			moved = QStringLiteral(", moved-to marker");
		}
		setAccessibleDescription(
			QStringLiteral("Change %1 of %2, %3%4, selected. Up and Down choose a change; Enter reveals it.")
				.arg(*navigation + 1U)
				.arg(NavigationEntryIndexes.size())
				.arg(kind_text(entry.Kind), moved));
	}

	void diff_link_map::update_hover(
		std::optional<diff_link_map_identity> aHover)
	{
		if (Hover == aHover) {
			return;
		}
		Hover = std::move(aHover);
		rebuild_visible_geometry();
		update();
		invoke_hover_sink();
	}

	void diff_link_map::choose_entry(
		const std::size_t aIndex, const bool aReveal)
	{
		if (aIndex >= Entries.size()) {
			return;
		}
		const auto identity = Entries[aIndex].Identity;
		const auto self     = QPointer<diff_link_map>{ this };
		set_selected(identity, SelectionAnnouncementsEnabled);
		if (self == nullptr) {
			return;
		}
		self->invoke_identity_sink(
			self->SelectionSink, identity, self->Statistics.SelectionCallbacks);
		if (self == nullptr || !aReveal) {
			return;
		}
		self->invoke_identity_sink(
			self->RevealSink, identity, self->Statistics.RevealCallbacks);
	}

	void diff_link_map::invoke_identity_sink(
		const diff_link_map_identity_sink& aSink,
		const diff_link_map_identity&      aIdentity,
		std::uint64_t&                     aCounter)
	{
		if (!aSink) {
			return;
		}
		const auto sink = aSink;
		const auto self = QPointer<diff_link_map>{ this };
		++aCounter;
		try {
			sink(aIdentity);
		} catch (...) {
			if (self != nullptr) {
				++self->Statistics.CallbackFailures;
			}
		}
	}

	void diff_link_map::invoke_hover_sink()
	{
		if (!HoverSink) {
			return;
		}
		const auto sink = HoverSink;
		const auto self = QPointer<diff_link_map>{ this };
		++Statistics.HoverCallbacks;
		try {
			sink(Hover);
		} catch (...) {
			if (self != nullptr) {
				++self->Statistics.CallbackFailures;
			}
		}
	}

	auto diff_link_map::path_for(const retained_entry& aEntry) const
		-> QPainterPath
	{
		const auto widthValue  = static_cast<qreal>(width());
		const auto control     = widthValue * Configuration.ControlFraction;
		const auto leftTop     = aEntry.LeftBegin - Frame->Left.DocumentOriginY;
		const auto leftBottom  = aEntry.LeftEnd - Frame->Left.DocumentOriginY;
		const auto rightTop    = aEntry.RightBegin - Frame->Right.DocumentOriginY;
		const auto rightBottom = aEntry.RightEnd
			- Frame->Right.DocumentOriginY;
		auto path = QPainterPath{ QPointF{ 0.0, leftTop } };
		path.cubicTo(
			control,
			leftTop,
			widthValue - control,
			rightTop,
			widthValue,
			rightTop);
		path.lineTo(widthValue, rightBottom);
		path.cubicTo(
			widthValue - control,
			rightBottom,
			control,
			leftBottom,
			0.0,
			leftBottom);
		path.closeSubpath();
		return path;
	}

	auto diff_link_map::coordinate(const QPointF& aPoint) const noexcept -> qreal
	{
		return orientation() == Qt::Horizontal ? aPoint.x() : aPoint.y();
	}

	auto diff_link_map::handle_position() const noexcept -> int
	{
		return orientation() == Qt::Horizontal ? x() : y();
	}

	diff_comparison_splitter::diff_comparison_splitter(QWidget* const aParent)
		: QSplitter{ Qt::Horizontal, aParent }
	{
		setObjectName(QStringLiteral("diffComparisonSplitter"));
		setChildrenCollapsible(false);
		static_assert(diff_link_map_configuration{ }.LogicalWidth == application::default_center_ribbon_width);
		setHandleWidth(static_cast<int>(application::default_center_ribbon_width));
	}

	auto diff_comparison_splitter::link_map() const noexcept -> diff_link_map*
	{
		return count() >= 2 ? dynamic_cast<diff_link_map*>(handle(1)) : nullptr;
	}

	auto diff_comparison_splitter::createHandle() -> QSplitterHandle*
	{
		return new diff_link_map{ orientation(), *this };
	}
} // namespace bpr::qt
