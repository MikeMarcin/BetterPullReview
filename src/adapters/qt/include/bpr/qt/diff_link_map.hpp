#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_LINK_MAP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_LINK_MAP_HPP

#include "bpr/qt/comparison_scroll_controller.hpp"

#include <QLineF>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSplitter>
#include <QSplitterHandle>
#include <QtTypes>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <vector>

class QChangeEvent;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWidget;

namespace bpr::qt {
	/// \brief Identifies one painted semantic correspondence.
	struct diff_link_map_identity
	{
		core::alignment_segment_id           Segment;
		std::optional<core::change_group_id> Group;

		auto operator<=>(const diff_link_map_identity&) const = default;
	};

	/// \brief Describes one retained path in the current visible frame.
	struct visible_diff_link
	{
		diff_link_map_identity                Identity;
		core::alignment_kind                  Kind = core::alignment_kind::equal;
		QPainterPath                          Path;
		qreal                                 LeftTop     = 0.0;
		qreal                                 LeftBottom  = 0.0;
		qreal                                 RightTop    = 0.0;
		qreal                                 RightBottom = 0.0;
		std::optional<core::move_relation_id> SourceMove;
		std::optional<core::move_relation_id> DestinationMove;
		std::optional<qreal>                  SourceMarkerY;
		std::optional<qreal>                  DestinationMarkerY;
	};

	/// \brief Describes one temporary selected or hovered move overlay.
	struct visible_diff_move_overlay
	{
		core::move_relation_id Relation;
		QPainterPath           Path;
		qreal                  SourceY      = 0.0;
		qreal                  DestinationY = 0.0;
	};

	/// \brief Records bounded lookup, paint, interaction, and callback work.
	struct diff_link_map_statistics
	{
		std::uint64_t GeometryRebuilds        = 0U;
		std::uint64_t FrameUpdates            = 0U;
		std::uint64_t VisibilityQueries       = 0U;
		std::uint64_t PaintEvents             = 0U;
		std::uint64_t HitTests                = 0U;
		std::uint64_t PaletteOrStyleChanges   = 0U;
		std::uint64_t SelectionCallbacks      = 0U;
		std::uint64_t HoverCallbacks          = 0U;
		std::uint64_t RevealCallbacks         = 0U;
		std::uint64_t CallbackFailures        = 0U;
		std::uint64_t RejectedStaleFrames     = 0U;
		std::uint64_t RangeSearches           = 0U;
		std::uint64_t CandidateEntriesVisited = 0U;
		std::uint64_t VisiblePathsBuilt       = 0U;
		std::uint64_t PaintedPathsVisited     = 0U;
		std::uint64_t CapacityGrowths         = 0U;
		std::size_t   RetainedChangedEntries  = 0U;
		std::size_t   LastCandidateEntries    = 0U;
		std::size_t   LastVisiblePaths        = 0U;
		std::size_t   LastPaintedPaths        = 0U;
		std::size_t   LastMoveMarkers         = 0U;
		std::size_t   LastMoveOverlays        = 0U;
		std::size_t   PeakCandidateEntries    = 0U;
		std::size_t   PeakVisiblePaths        = 0U;
	};

	struct diff_link_map_configuration
	{
		int   LogicalWidth      = 28;
		qreal ControlFraction   = 0.43;
		qreal NearVisibleMargin = 64.0;
		qreal MoveMarkerExtent  = 5.0;
	};

	using diff_link_map_identity_sink
		= std::function<void(const diff_link_map_identity&)>;
	using diff_link_map_hover_sink
		= std::function<void(const std::optional<diff_link_map_identity>&)>;

	/// \brief One change's row bands and converging ribbons in a unified viewport.
	struct visible_one_pane_diff_link
	{
		diff_link_map_identity Identity;
		std::optional<QRectF>  Removed;
		std::optional<QRectF>  Added;
		QPainterPath           RemovedRibbon;
		QPainterPath           AddedRibbon;
		QLineF                 Spine;
	};

	/// \brief The correspondence map at its one-column limit; contains no file overview.
	class one_pane_diff_link_map final : public QWidget
	{
	public:
		explicit one_pane_diff_link_map(QWidget* aParent = nullptr);
		/// \brief Adopts only changes intersecting the source viewport, in document order.
		void               set_links(std::vector<visible_one_pane_diff_link> aLinks,
									 std::optional<core::change_group_id> aSelected, QRectF aViewportBand);
		[[nodiscard]] auto visible_links() const noexcept -> std::span<const visible_one_pane_diff_link>;
		void               set_selection_sink(diff_link_map_identity_sink aSink);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;

	private:
		std::vector<visible_one_pane_diff_link> Links;
		std::optional<core::change_group_id>    Selected;
		QRectF                                  ViewportBand;
		diff_link_map_identity_sink             SelectionSink;
	};

	/// \brief Paints and interacts with the center correspondence divider.
	class diff_link_map final : public QSplitterHandle
	{
	public:
		explicit diff_link_map(
			Qt::Orientation             aOrientation,
			QSplitter&                  aParent,
			diff_link_map_configuration aConfiguration = { });

		/// \brief Updates the logical divider width, retaining ribbon identities.
		void set_logical_width(int aWidth);

		/// \brief Replaces retained measured geometry and exact move relations.
		void set_geometry(
			measured_alignment_axis              aAxis,
			std::span<const core::move_relation> aMoves = { });
		void clear_geometry();
		/// \brief Applies one same-comparison shared-scroll frame.
		/// \brief Declares where the pane viewports sit against this widget.
		///
		/// \details Ribbon endpoints are computed in a pane's viewport
		/// coordinates, and this widget is a sibling of the panes rather than
		/// a child of either, so its own top and height are not theirs. They
		/// differ by the pane frame and by whatever a horizontal scrollbar
		/// takes, which is why a ribbon drawn at its own y met the pane in the
		/// wrong place - BPR-0208. Given the band, the conversion is exact;
		/// assumed, it is wrong by however much the layout happens to differ.
		void set_viewport_band(qreal aTopOffset, qreal aHeight);
		/// \brief [[RFC-BPR-0090]]. Ribbons whose right end reaches below this
		/// height of the right pane's viewport are drawn dimmed, or none.
		/// \details While the head pane edits a range in place, the rows below
		/// it move with its length, so a ribbon that touches or follows it no
		/// longer meets its own row.
		void               set_dimmed_from(std::optional<qreal> aRightTop);
		[[nodiscard]] auto dimmed_from() const noexcept -> std::optional<qreal>;

		[[nodiscard]] auto set_frame(const comparison_scroll_frame& aFrame)
			-> bool;
		/// \brief Returns the frame currently used for paint and hit testing.
		[[nodiscard]] auto current_frame() const noexcept
			-> const std::optional<comparison_scroll_frame>&;

		void set_selected(
			std::optional<diff_link_map_identity> aSelection,
			bool                                  aAnnounce = true);
		[[nodiscard]] auto selected() const noexcept
			-> const std::optional<diff_link_map_identity>&;
		[[nodiscard]] auto hovered() const noexcept
			-> const std::optional<diff_link_map_identity>&;

		void set_selection_sink(diff_link_map_identity_sink aSink);
		/// \brief Delegates selection announcements to a composed parent when disabled.
		void set_selection_announcements_enabled(bool aEnabled) noexcept;
		void set_hover_sink(diff_link_map_hover_sink aSink);
		void set_reveal_sink(diff_link_map_identity_sink aSink);

		[[nodiscard]] auto hit_test(QPointF aPosition)
			-> std::optional<diff_link_map_identity>;
		[[nodiscard]] auto visible_links() const noexcept
			-> std::span<const visible_diff_link>;
		[[nodiscard]] auto visible_move_overlays() const noexcept
			-> std::span<const visible_diff_move_overlay>;
		[[nodiscard]] auto statistics() const noexcept
			-> diff_link_map_statistics;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		void mouseDoubleClickEvent(QMouseEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		struct retained_entry
		{
			diff_link_map_identity     Identity;
			core::alignment_kind       Kind       = core::alignment_kind::equal;
			qreal                      LeftBegin  = 0.0;
			qreal                      LeftEnd    = 0.0;
			qreal                      RightBegin = 0.0;
			qreal                      RightEnd   = 0.0;
			std::optional<std::size_t> SourceMove;
			std::optional<std::size_t> DestinationMove;
		};
		struct retained_move
		{
			core::move_relation_id Id;
			std::size_t            Source      = 0U;
			std::size_t            Destination = 0U;
		};

		[[nodiscard]] auto identity_exists(
			const diff_link_map_identity& aIdentity) const noexcept -> bool;
		[[nodiscard]] auto entry_selected(const retained_entry& aEntry) const
			noexcept -> bool;
		[[nodiscard]] auto entry_hovered(const retained_entry& aEntry) const
			noexcept -> bool;
		[[nodiscard]] auto entry_index(
			const diff_link_map_identity& aIdentity) const noexcept
			-> std::optional<std::size_t>;
		[[nodiscard]] auto navigation_index(
			const diff_link_map_identity& aIdentity) const noexcept
			-> std::optional<std::size_t>;
		[[nodiscard]] auto band_height() const noexcept -> qreal;
		void               rebuild_visible_geometry();
		[[nodiscard]] auto refresh_hover_from_pointer() -> bool;
		void               update_accessible_status();
		void               update_hover(std::optional<diff_link_map_identity> aHover);
		void               choose_entry(std::size_t aIndex, bool aReveal);
		void               invoke_identity_sink(
			const diff_link_map_identity_sink& aSink,
			const diff_link_map_identity&      aIdentity,
			std::uint64_t&                     aCounter);
		void               invoke_hover_sink();
		[[nodiscard]] auto path_for(const retained_entry& aEntry) const
			-> QPainterPath;
		[[nodiscard]] auto coordinate(const QPointF& aPoint) const noexcept
			-> qreal;
		[[nodiscard]] auto handle_position() const noexcept -> int;

		diff_link_map_configuration            Configuration;
		std::optional<measured_alignment_axis> Axis;
		std::optional<comparison_scroll_frame> Frame;
		/// Offset of the pane viewports' top from this widget's top.
		qreal ViewportBandTop = 0.0;
		/// Height of the pane viewports. Zero means "not yet declared", and
		/// the widget's own height stands in so a map that is never told
		/// still draws rather than collapsing to nothing.
		qreal                                             ViewportBandHeight = 0.0;
		std::vector<retained_entry>                       Entries;
		std::vector<retained_move>                        Moves;
		std::map<core::alignment_segment_id, std::size_t> EntryBySegment;
		std::vector<std::size_t>                          NavigationEntryIndexes;
		std::vector<visible_diff_link>                    VisibleLinks;
		std::vector<visible_diff_move_overlay>            VisibleMoveOverlays;
		std::vector<std::size_t>                          CandidateEntryIndexes;
		std::vector<std::size_t>                          VisibleEntryIndexes;
		std::vector<std::size_t>                          VisibleLinkByEntry;
		std::vector<std::size_t>                          VisibleMoveIndexes;
		std::vector<bool>                                 EntryVisibilityMarks;
		std::vector<bool>                                 MoveVisibilityMarks;
		std::optional<diff_link_map_identity>             Selection;
		std::optional<diff_link_map_identity>             Hover;
		std::optional<diff_link_map_identity>             PressedIdentity;
		std::optional<QPointF>                            PointerPosition;
		diff_link_map_identity_sink                       SelectionSink;
		diff_link_map_hover_sink                          HoverSink;
		diff_link_map_identity_sink                       RevealSink;
		QPointF                                           PressGlobal;
		int                                               PressHandlePosition           = 0;
		bool                                              Pressed                       = false;
		bool                                              Dragging                      = false;
		bool                                              SelectionAnnouncementsEnabled = true;
		diff_link_map_statistics                          Statistics;
		std::optional<qreal>                              DimmedFrom;
	};

	/// \brief Owns the two-pane splitter whose sole handle is a DiffLinkMap.
	class diff_comparison_splitter final : public QSplitter
	{
	public:
		explicit diff_comparison_splitter(QWidget* aParent = nullptr);
		[[nodiscard]] auto link_map() const noexcept -> diff_link_map*;

	protected:
		[[nodiscard]] auto createHandle() -> QSplitterHandle* override;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_LINK_MAP_HPP
