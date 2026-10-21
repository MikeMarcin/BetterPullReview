#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diff_link_map.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/source_find_bar.hpp"
#include <QColor>
#include <QList>
#include <QRect>
#include <QTextEdit>

#include <QPointer>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>

class QEvent;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTextDocument;
class QResizeEvent;
class QScrollBar;
class QSplitter;
class QSplitterHandle;
class QStackedLayout;
class QTimer;
class QWheelEvent;

namespace bpr::qt {
	class pending_veil;
	class line_details_pane;
	class line_details_splitter;

	/// \brief Identifies the Qt-facing source that requested a geometry rebuild.
	enum class diff_geometry_rebuild_trigger : std::uint32_t {
		none                     = 0U,
		accepted_diff            = 1U << 0U,
		font_event               = 1U << 1U,
		style_event              = 1U << 2U,
		show_event               = 1U << 3U,
		device_pixel_ratio_event = 1U << 4U,
		screen_change_event      = 1U << 5U,
		tab_width                = 1U << 6U,
		pane_viewport_resize     = 1U << 7U,
		workbench_resize         = 1U << 8U,
	};

	[[nodiscard]] constexpr auto operator|(
		const diff_geometry_rebuild_trigger aLeft,
		const diff_geometry_rebuild_trigger aRight) noexcept
		-> diff_geometry_rebuild_trigger
	{
		return static_cast<diff_geometry_rebuild_trigger>(
			static_cast<std::uint32_t>(aLeft)
			| static_cast<std::uint32_t>(aRight));
	}

	[[nodiscard]] constexpr auto operator&(
		const diff_geometry_rebuild_trigger aLeft,
		const diff_geometry_rebuild_trigger aRight) noexcept
		-> diff_geometry_rebuild_trigger
	{
		return static_cast<diff_geometry_rebuild_trigger>(
			static_cast<std::uint32_t>(aLeft)
			& static_cast<std::uint32_t>(aRight));
	}

	/// \brief One successful rebuild's coalesced semantic and Qt causes.
	struct diff_geometry_rebuild_cause
	{
		measured_geometry_change      Changes = measured_geometry_change::none;
		diff_geometry_rebuild_trigger Triggers
			= diff_geometry_rebuild_trigger::none;

		auto operator<=>(const diff_geometry_rebuild_cause&) const = default;
	};

	inline constexpr std::size_t diff_geometry_rebuild_history_capacity = 32U;

	/// \brief Records directly observable composed-workbench activity.
	struct diff_workbench_statistics
	{
		std::uint64_t Publications = 0U;
		/// Find converts exact endpoints at result adoption, never during traversal.
		std::uint64_t FindMatchingRuns        = 0U;
		std::uint64_t FindCachedRuns          = 0U;
		std::uint64_t FindEndpointConversions = 0U;
		std::uint64_t FindHighlightsVisited   = 0U;
		/// [[BPR-1264]]. Publications whose source documents were filled over
		/// several bounded GUI turns, the turns spent filling them, and the
		/// staged fills a newer request superseded before adoption.
		std::uint64_t StagedPublications           = 0U;
		std::uint64_t PublicationFillBatches       = 0U;
		std::uint64_t SupersededStagedPublications = 0U;
		std::uint64_t Failures                     = 0U;
		std::uint64_t Invalidations                = 0U;
		std::uint64_t GeometryRebuilds             = 0U;
		std::uint64_t GeometryFailures             = 0U;
		std::uint64_t FrameApplications            = 0U;
		std::uint64_t SelectionChanges             = 0U;
		std::uint64_t RejectedStaleInput           = 0U;
		std::uint64_t SemanticIndexAdoptions       = 0U;
		std::uint64_t SemanticQueries              = 0U;
		std::uint64_t PaneMeasurements             = 0U;
		std::uint64_t MeasurementBatches           = 0U;
		std::uint64_t MeasurementRestarts          = 0U;
		std::uint32_t MaximumMeasurementBatchLines = 0U;
		/// [[BPR-1123]]. Lines a completed measurement took from the pane's
		/// recorded block heights instead of shaping them again.
		std::uint64_t MeasurementLinesReused = 0U;
		std::uint64_t TokenSpanAdoptions     = 0U;
		std::uint64_t RejectedTokenSpans     = 0U;
		std::uint64_t SyntaxBlocksFormatted  = 0U;
		/// Classified spans the painter stepped over or consumed. Counted
		/// separately from the blocks it formatted because those two can come
		/// apart: a painter that scanned the list from the front would format
		/// exactly the same blocks and visit every span in the file to do it,
		/// which is the shape of the cost a block count cannot see.
		std::uint64_t SyntaxSpansVisited = 0U;
		/// Alignment segments the band painter examined. Counted for the same
		/// reason as the spans above: the band count is bounded by the
		/// viewport whether or not the search that found them was.
		std::uint64_t AlignmentSegmentsVisited = 0U;
		std::uint64_t GroupBoundariesPainted   = 0U;
		std::uint64_t MovedBandsPainted        = 0U;
		/// [[BPR-0986]]. Current-line bands composed for the reading line.
		std::uint64_t CurrentLineBandsPainted = 0U;
		/// [[BPR-1107]]. White space marks (dots and tab lines) the panes actually
		/// drew, counted where they are drawn, as the boundary rules are.
		std::uint64_t WhiteSpacePainted = 0U;
		/// [[BPR-1336]]. Zoom gestures the code panes turned into steps, and
		/// reading positions a font change kept on the reader's line.
		std::uint64_t CodeZoomGestures     = 0U;
		std::uint64_t ReadingPositionsHeld = 0U;
		/// [[BPR-1334]]. Line details projections the core was asked for.
		std::uint64_t LineDetailsProjections = 0U;
		std::size_t   SemanticIndexEntries   = 0U;
		std::array<
			diff_geometry_rebuild_cause,
			diff_geometry_rebuild_history_capacity>
					GeometryRebuildHistory{ };
		std::size_t GeometryRebuildHistorySize     = 0U;
		bool        GeometryRebuildHistoryOverflow = false;

		auto operator<=>(const diff_workbench_statistics&) const = default;
	};

	/// \brief Presents one immutable computed diff through the selected Qt path.
	/// \brief A line's mark in the gutter edge: a thread sits there.
	enum class diff_line_mark_kind : std::uint8_t {
		thread,
		new_thread,
	};

	struct diff_line_mark
	{
		std::uint32_t       Line = 0U;
		diff_line_mark_kind Kind = diff_line_mark_kind::thread;
		/// A resolved thread's number is tinted lighter.
		bool Resolved = false;
		/// My unpublished note: its number is outlined, dashed.
		bool Draft = false;
		/// The open thread: its line is washed amber and its number barred.
		bool Open = false;
		/// The note under the pointer: its line is washed lightly.
		bool Hovered = false;
		/// [[BPR-1212]]. My draft, being written in the drawer: its line is
		/// washed blue and bracketed at the gutter's edge, where the card
		/// joins it. Amber is where you read, blue is where you write.
		bool Writing = false;
		/// [[BPR-1212]] slice 1. A draft on several lines: the first line the
		/// bracket spans (a unified pane's is its row); `Line` is its last.
		/// [[BPR-1721]]. Also a published thread's first line, which its
		/// open wash spans.
		std::optional<std::uint32_t> FirstLine;
		/// What the marker's tooltip and accessible name say.
		QString Description;
		/// The side and zero-based source line the mark answers for; a unified
		/// pane's `Line` is its row.
		core::diff_side Side       = core::diff_side::right;
		std::uint32_t   SourceLine = 0U;

		auto operator==(const diff_line_mark&) const -> bool = default;
	};

	/// \brief [[BPR-1290]]. One thread's tick on the code's scroll bar.
	struct diff_thread_tick
	{
		core::diff_side Side = core::diff_side::right;
		std::uint32_t   Line = 0U;
		/// Where the thumb's centre is when the line is in view, from the
		/// groove's top (0) to its bottom (1).
		qreal Fraction = 0.0;
		bool  Unread   = false;
		bool  Resolved = false;
		bool  Draft    = false;
		bool  Open     = false;
		/// The thread's marker description, "Line 656: 4 posts, 2 new".
		QString Description;

		auto operator==(const diff_thread_tick&) const -> bool = default;
	};

	class diff_workbench final
		: public QWidget,
		  public application::diff_projection_port
	{
	public:
		using selection_sink     = std::function<void(
			const std::optional<core::semantic_selection>&)>;
		using selection_observer = std::function<void(
			const std::optional<core::semantic_selection>&,
			bool)>;

		explicit diff_workbench(QWidget* aParent = nullptr);
		~diff_workbench() override;

		diff_workbench(const diff_workbench&)                    = delete;
		auto operator=(const diff_workbench&) -> diff_workbench& = delete;
		diff_workbench(diff_workbench&&)                         = delete;
		auto operator=(diff_workbench&&) -> diff_workbench&      = delete;

		void publish(const application::diff_publication& aPublication) override;
		void fail(const application::diff_failure_publication& aFailure) override;
		void invalidate(const core::pull_request_authority& aAuthority) override;

		/// \brief Clears stale geometry and presents an exact pending context.
		void set_loading(const core::diff_request_context& aContext);
		/// \brief Clears material and optionally presents a host-supplied unavailable reason.
		void clear(QString aStatus = { });
		/// \brief Publishes exact semantic selections for application binding.
		void set_selection_sink(selection_sink aSink);
		/// \brief Reports explicit reading gestures before the accepted selection sink.
		void set_reading_gesture_sink(std::function<void()> aSink);
		/// \brief Observes Qt-local selections without replacing the application sink.
		void set_selection_observer(selection_observer aObserver);
		/// \brief [[BPR-1132]]. Hides the status row, for a composition whose
		/// comparison header says it instead; the status is still kept.
		void set_status_shown(bool aShown);
		/// \brief [[BPR-1132]]. Reports each status: its words, and whether they
		/// say more than that the comparison is ready (loading, a refusal, an
		/// approximate alignment).
		void set_status_observer(std::function<void(const QString&, bool)> aObserver);
		/// \brief Whether the status says more than that the diff is ready.
		[[nodiscard]] auto status_notable() const noexcept -> bool;
		/// \brief [[BPR-1287]]. What the comparison area says while no file
		/// comparison is chosen, and the gestures it offers to choose one.
		struct idle_direction
		{
			QString Title;
			QString Detail;
			/// "Open first file" when set; the composition's own gesture.
			std::function<void()> OpenFirst;
			/// "Show files" when set; the composition's own gesture.
			std::function<void()> ShowFiles;
		};
		/// \brief [[BPR-1287]]. With no file comparison chosen the workbench
		/// shows this direction instead of two empty panes (owner, BPR-1083
		/// chip-menu-v001 aside: "We need to not show a blank diff here and
		/// instead have some clear direction to open a file.").
		void set_idle_direction(idle_direction aDirection);
		/// \brief True while no file comparison is chosen: no context is
		/// loading, shown, refused or invalidated.
		[[nodiscard]] auto idle() const noexcept -> bool;
		[[nodiscard]] auto idle_page() const noexcept -> QWidget*;
		[[nodiscard]] auto idle_open_first_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto idle_show_files_button() const noexcept -> QPushButton*;
		/// \brief Selects and reveals one exact zero-based conversation anchor.
		/// \brief [[BPR-0657]]. Selects a conversation's line as a click on it
		/// would, so a draft started from its card anchors there; the anchor the
		/// line carries is kept.
		[[nodiscard]] auto select_conversation_line(core::diff_side aSide, std::uint32_t aLine) -> bool;
		/// \brief Restores an exact core-validated selection and reveals its semantic line.
		[[nodiscard]] auto restore_selection(const core::semantic_selection& aSelection) -> bool;
		/// \brief Native text selection expressed as actual zero-based source endpoints.
		[[nodiscard]] auto selected_source_range() const -> std::optional<std::pair<core::source_line_ref, core::source_line_ref>>;
		/// \brief Reveals an exact interval after its owner validates commit/blob/path identity.
		[[nodiscard]] auto restore_source_range(core::source_line_ref aFirst, core::source_line_ref aLast) -> bool;
		/// \brief [[BPR-1212]]. Told when Return (false) or Shift+Return (true)
		/// is pressed while a code pane itself has keyboard focus. Keypad Enter
		/// is Return; any other modifier is left to the pane. Without a sink
		/// the keys keep the pane's own behaviour.
		void set_line_draft_dispatch(std::function<void(bool aSuggest)> aDispatch);
		/// \brief [[BPR-1212]]. The selected lines a new comment would cover,
		/// first and last, from the one selection the mouse and the keyboard
		/// share: the pane's selected lines when they hold the selected line,
		/// otherwise that line alone. None with no line selected. The ends may
		/// name different sides in one pane; the core refuses that.
		[[nodiscard]] auto line_draft_range() const -> std::optional<std::pair<core::source_line_ref, core::source_line_ref>>;
		/// \brief [[BPR-1212]]. Moves the keyboard line cursor of the pane
		/// that has focus, or of the right pane, as Up and Down (`aStep`
		/// -1 or 1) do, extending the selection with `aExtend` as Shift does.
		/// \returns Whether a line was selected.
		auto move_line_cursor(int aStep, bool aExtend) -> bool;
		/// \brief The last polite announcement made for the comparison.
		[[nodiscard]] auto last_announcement() const noexcept -> const QString&;
		[[nodiscard]] auto reveal_conversation_line(
			core::diff_side aSide,
			std::uint32_t   aLine) -> bool;
		/// \brief Whether the actual source line intersects the currently visible code viewport.
		[[nodiscard]] auto source_line_visible(core::diff_side aSide, std::uint32_t aLine) const -> bool;
		void               clear_conversation_anchor();
		[[nodiscard]] auto conversation_anchor() const noexcept
			-> const std::optional<core::source_line_ref>&;
		[[nodiscard]] auto conversation_anchor_marker_rect(
			core::diff_side aSide) const -> QRect;
		/// \brief Where one zero-based line of a pane is, in this widget's
		/// coordinates, however far it is from view; empty when the pane has
		/// no such line.
		///
		/// \details [[BPR-0648]]. The comments drawer aligns its cards to the
		/// lines their threads sit on, and follows the shared scroll through
		/// `set_frame_observer`. This is the only geometry it reads.
		/// [[BPR-1542]]: exact far from view, so a thread taller than the
		/// drawer rises with its line until its foot shows.
		[[nodiscard]] auto line_rect(core::diff_side aSide, std::uint32_t aLine) const
			-> QRect;
		/// \brief Marks lines that carry a thread (amber) or a new one (blue).
		void               set_line_marks(core::diff_side aSide, std::vector<diff_line_mark> aMarks);
		[[nodiscard]] auto line_marks(core::diff_side aSide) const -> const std::vector<diff_line_mark>&;
		/// \brief [[BPR-1290]], [[BPR-1335]]. Shows each pane's threads in its
		/// gutter, the code's scroll bar a tick per thread, and the head pane
		/// its "+" button. A thread's line number keeps its place and its cell
		/// is tinted; the gutter is its numbers only. Off, a mark is the 4 px
		/// bar at the code's edge.
		void               set_thread_markers(bool aEnabled);
		[[nodiscard]] auto thread_markers() const noexcept -> bool;
		/// \brief The amber of a thread and the blue of a new one.
		void set_mark_colours(const QColor& aThread, const QColor& aNew);
		/// \brief Told when a thread's tinted line number is pressed, with its
		/// side and zero-based source line.
		void set_line_mark_dispatch(std::function<void(core::diff_side, std::uint32_t)> aDispatch);
		/// \brief [[BPR-1212]], [[BPR-1335]]. Told when the head pane's "+"
		/// button is clicked, or dragged across lines, with the first and last
		/// zero-based lines it covered, in order: Add comment.
		void set_line_add_dispatch(std::function<void(std::uint32_t aFirst, std::uint32_t aLast)> aDispatch);
		/// \brief [[BPR-1212]], [[BPR-1335]]. Where the head pane's "+" button
		/// is while the pointer is in its column on its line, in the pane's
		/// coordinates; empty while it is not shown.
		[[nodiscard]] auto line_add_rect() const -> QRect;
		/// \brief [[BPR-1290]]. Told when a thread's scroll-bar tick is
		/// Shift-clicked, with its side and zero-based source line. A plain
		/// press on the bar stays the bar's own.
		void set_thread_tick_dispatch(std::function<void(core::diff_side, std::uint32_t)> aDispatch);
		/// \brief [[BPR-1290]]. Scrolls the code as a wheel over it would.
		/// \returns Whether the code moved.
		[[nodiscard]] auto scroll_by_wheel(const QWheelEvent& aEvent) -> bool;
		/// \brief The scroll bar's ticks, as last placed.
		[[nodiscard]] auto thread_ticks() const -> std::vector<diff_thread_tick>;
		/// \brief Where one thread's tick is, in its scroll bar's coordinates;
		/// empty when the bar has no tick for that line.
		[[nodiscard]] auto thread_tick_rect(core::diff_side aSide, std::uint32_t aLine) const -> QRect;
		/// \brief The tick Shift-click would go to while Shift is held over
		/// the bar, as ringed.
		[[nodiscard]] auto targeted_thread_tick() const -> std::optional<diff_thread_tick>;
		/// \brief Where one pane's tinted number cell for a thread on a
		/// zero-based line is, in this widget's coordinates; empty while not
		/// shown.
		[[nodiscard]] auto line_mark_rect(core::diff_side aSide, std::uint32_t aLine) const -> QRect;
		/// \brief Told after every applied scroll frame and geometry rebuild.
		void set_frame_observer(std::function<void()> aObserver);

		[[nodiscard]] auto left_pane() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto right_pane() const noexcept -> QPlainTextEdit*;
		/// \brief [[BPR-PENDING-AE]]. The one-pane comparison's source, shown
		/// while `one_pane_enabled`.
		[[nodiscard]] auto unified_pane() const noexcept -> QPlainTextEdit*;
		/// \brief [[BPR-PENDING-AE]]. Before a pane's code menu opens at a
		/// viewport point: selects the line the point's row shows unless the
		/// selected lines hold it. In one pane a row is its head line, or the
		/// base line a removed row shows.
		/// \returns Whether the point is on a line of the shown pane.
		auto select_line_for_menu(QPlainTextEdit* aPane, QPoint aPosition) -> bool;
		/// \brief Focuses the visible source editor in split or unified presentation.
		void focus_source();
		/// \brief Returns source focus to the exact side selected by an external occurrence.
		void focus_source(core::diff_side aSide);
		/// \brief Composes pane search gestures and exact-current result decorations.
		void set_source_find_dispatch(std::function<void(const source_find_intent&)> aDispatch);
		void set_source_find_changed(std::function<void()> aChanged);
		void open_source_find();
		void navigate_source_find(core::change_navigation_direction aDirection);
		void close_source_find();
		void retarget_source_find();
		void present_source_find(const application::source_search_projection& aProjection);
		/// \brief Actual worker matching/cache activity without query or source content.
		void               observe_source_find_execution(std::uint64_t aMatching, std::uint64_t aCached);
		[[nodiscard]] auto source_find() const -> source_find_bar*;
		[[nodiscard]] auto source_find_available() const -> bool;
		[[nodiscard]] auto source_find_navigable() const -> bool;
		/// \brief Selects original UTF-8 spans only on the accepted exact blob.
		[[nodiscard]] auto reveal_source_match(const core::source_match_location& aLocation, bool aExternal = false) -> bool;
		/// \brief Nonempty native source selection for literal PR query seeding.
		[[nodiscard]] auto source_search_seed() const -> QString;
		[[nodiscard]] auto detail_pane() const noexcept -> QPlainTextEdit*;
		/// \brief [[BPR-1334]]. Line details, the strip under the comparison;
		/// the same widget as `detail_pane`.
		[[nodiscard]] auto line_details() const noexcept -> line_details_pane*;
		/// \brief [[BPR-1334]]. Shows or hides Line details. Presentation
		/// only: the preference is the profile's and arrives from the host.
		void set_line_details_visible(bool aVisible);
		/// \brief Applies the profile's accepted center-ribbon width in logical pixels.
		void               set_center_ribbon_width(std::uint32_t aWidth);
		[[nodiscard]] auto line_details_visible() const noexcept -> bool;
		/// \brief [[BPR-1334]]. This window's preferred strip height in
		/// logical pixels; empty is the default.
		void               set_line_details_height(std::optional<std::uint32_t> aHeight);
		[[nodiscard]] auto line_details_height() const noexcept -> std::optional<std::uint32_t>;
		/// \brief [[BPR-1334]]. Told the height after an explicit gesture on
		/// the seam; empty after Reset. Never told about a clamp.
		void               set_line_details_resize_sink(std::function<void(std::optional<std::uint32_t>)> aSink);
		[[nodiscard]] auto line_details_handle() const -> QSplitterHandle*;
		/// \brief Applies one semantic previous/next change gesture.
		///
		/// \details The workbench owns the one immutable semantic index. Presenters
		/// pass the exact core-projected stop-versus-wrap policy with the gesture;
		/// the comparison contains no duplicate navigation controls and never
		/// recomputes mapping in Qt.
		[[nodiscard]] auto navigate_change(
			core::change_navigation_direction aDirection,
			core::change_navigation_policy    aPolicy) -> bool;
		/// \brief [[BPR-1532]]. Applies First change (`previous`) or Last
		/// change (`next`).
		///
		/// \details The core decides the end. An end already selected is kept
		/// and scrolled back into view, so the gesture always shows that change.
		/// Returns whether the selection changed.
		[[nodiscard]] auto navigate_outermost_change(core::change_navigation_direction aDirection) -> bool;
		[[nodiscard]] auto shared_scrollbar() const noexcept -> QScrollBar*;
		[[nodiscard]] auto comparison_splitter() const noexcept
			-> diff_comparison_splitter*;
		[[nodiscard]] auto link_map() const noexcept -> diff_link_map*;
		[[nodiscard]] auto scroll_controller() const noexcept
			-> comparison_scroll_controller*;
		[[nodiscard]] auto selection() const noexcept
			-> const std::optional<core::semantic_selection>&;
		[[nodiscard]] auto current_context() const noexcept
			-> const std::optional<core::diff_request_context>&;
		[[nodiscard]] auto current_semantic_index() const noexcept
			-> const std::shared_ptr<const core::diff_semantic_index>&;
		[[nodiscard]] auto current_result() const noexcept
			-> const std::shared_ptr<const core::computed_file_diff>&;
		/// \brief Whether native geometry is awaiting GUI continuations.
		[[nodiscard]] auto geometry_pending() const noexcept -> bool;
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto statistics() const noexcept
			-> diff_workbench_statistics;

		/// \brief Adopts the semantic colours the panes paint the diff with.
		///
		/// \details `QPalette` has no role for "an added line", so the pane
		/// cannot get these the way it gets its background and selection. They
		/// come from the composed appearance palette, which is the single
		/// authority on what each `core::appearance_role` looks like.
		///
		/// Supplying them is optional. A workbench that was never given a
		/// palette paints no line backgrounds at all rather than inventing
		/// colours, so a composition that forgot this shows an obviously
		/// unpainted diff instead of a plausible but unowned one.
		void adopt_appearance(const appearance_profile& aProfile);

		/// \brief Shows or hides both panes' line-number gutters.
		///
		/// \details Presentation only. The workbench holds no opinion about
		/// whether numbering should be on: the state is `core`'s, the setting
		/// is durable, and this applies whatever the projection last published.
		/// Hiding returns the gutter's width to the code rather than leaving an
		/// empty margin, and the measured geometry follows the viewport it
		/// changes.
		void set_line_numbers_visible(bool aVisible);
		/// \brief Applies the core-projected presentation without changing semantic selection.
		void               set_one_pane_enabled(bool aEnabled);
		[[nodiscard]] auto one_pane_enabled() const noexcept -> bool;

		/// \brief Adopts the classification of one side's blob.
		///
		/// \details The pane runs no lexer. Spans arrive from the appearance
		/// host, which keys them by blob identity and definition version alone,
		/// so a theme, palette, or font change repaints from the same retained
		/// classification and cannot cause a reparse. A pane that classified
		/// for itself would defeat that cache while every counter still read
		/// as though the cache were working.
		///
		/// Spans must be non-decreasing in `Line`. That is how the classifier
		/// produces them — one pass over the blob, line by line — and it is
		/// what lets painting find the visible window by search instead of by
		/// scanning every span on every scroll. A list that violates it is
		/// refused whole and counted, because painting it linearly would
		/// reintroduce exactly the unbounded repaint BPR-0196 removed.
		///
		/// \returns Whether the spans were adopted.
		auto adopt_token_spans(
			core::diff_side                 aSide,
			application::shared_token_spans aSpans) -> bool;

		[[nodiscard]] auto token_spans(core::diff_side aSide) const noexcept
			-> const application::shared_token_spans&;
		/// \brief [[BPR-1576]]. A classification of this side was asked for:
		/// until its spans are adopted the panes say their colours are coming.
		void await_syntax(core::diff_side aSide);
		/// \brief [[BPR-1576]]. No colours are coming for this comparison: the
		/// classifier failed or refused.
		void settle_syntax();
		/// \brief [[BPR-1576]]. The panes' loading-state presenter.
		[[nodiscard]] auto comparison_veil() const noexcept -> pending_veil*;

		/// \brief [[BPR-1336]]. Where ⌘-scroll and pinch over the code send a step.
		///
		/// \details The panes decide nothing about zoom: a gesture leaves as one
		/// step and the accepted zoom comes back as a font. Without a sink the
		/// gesture is still consumed, so a pane never zooms itself.
		void set_code_zoom_dispatch(std::function<void(core::code_zoom_step)> aDispatch);
		/// \brief [[BPR-1336]]. Shows the accepted zoom for a moment and announces it.
		void               show_code_zoom(std::uint32_t aPercent, std::uint32_t aCentipoints);
		[[nodiscard]] auto code_zoom_indicator() const noexcept -> QWidget*;

	protected:
		[[nodiscard]] auto event(QEvent* aEvent) -> bool override;
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent)
			-> bool override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// \brief The semantic colours one side paints with.
		struct diff_presentation_colors
		{
			QColor Added;
			QColor Removed;
			QColor IntralineAdded;
			QColor IntralineRemoved;
			QColor Moved;
			/// One colour per core role, indexed by the role's own value. A
			/// token span names a role and never a colour, so this table is
			/// the only place a role becomes visible, and it is copied whole
			/// from the composed palette rather than derived here.
			std::array<QColor, core::appearance_role_count> Syntax;
			/// The composed palette and the variant it belongs to, kept so the
			/// painter can ask the core questions that depend on the whole
			/// palette rather than on one colour -- whether an emphasis fill
			/// has to carry its own foreground, most of all.
			core::appearance_palette Palette{ };
			core::appearance_variant Variant
				= core::appearance_variant::light;
			bool Supplied = false;
		};

		void               adopt_code_typography(const code_typography_event& aTypography);
		void               note_reading_position();
		void               hold_reading_position(QPlainTextEdit& aPane);
		[[nodiscard]] auto code_zoom_gesture(QEvent& aEvent) -> bool;
		void               update_diff_paint();
		void               rebuild_unified_document();
		void               update_unified_paint();
		void               update_unified_link_map();
		[[nodiscard]] auto unified_row_for(core::diff_side aSide, std::uint32_t aLine) const -> std::optional<std::uint32_t>;
		void               reveal_unified_row(std::uint32_t aRow);
		void               apply_syntax_formats(
			QPlainTextEdit& aPane,
			core::diff_side aSide,
			core::line_span aPaintable);
		/// \brief Hands each pane the lines its change groups start on.
		void apply_group_boundaries();
		/// \brief The lines one side's selection covers.
		[[nodiscard]] auto selected_lines(core::diff_side aSide) const
			-> std::optional<core::line_span>;
		/// \brief [[BPR-0986]]. The reading line, when it is on this side.
		[[nodiscard]] auto current_line(core::diff_side aSide) const
			-> std::optional<std::uint32_t>;
		/// \brief Whether the named segment takes part in a detected move.
		[[nodiscard]] auto moved(const core::alignment_segment_id& aSegment) const
			-> bool;
		/// \brief Marks the changed characters inside each paintable pair.
		///
		/// \details `aWashed` is the span this pane's selection wash covers, so
		/// the emphasis can take that wash on too. Without it the mark keeps
		/// its added or removed hue on a selected row, which puts a red or
		/// green patch inside a blue band and makes the changed characters
		/// harder to find rather than easier.
		void append_intraline_emphasis(
			QList<QTextEdit::ExtraSelection>& aSelections,
			QPlainTextEdit&                   aPane,
			core::diff_side                   aSide,
			core::line_span                   aPaintable,
			std::optional<core::line_span>    aWashed);
		/// \brief Tells the link map where the pane viewports actually are.
		void declare_link_map_band();
		void schedule_geometry_rebuild(
			measured_geometry_change      aChange,
			diff_geometry_rebuild_trigger aTrigger);
		void queue_geometry_continuation();
		void rebuild_geometry();
		/// \brief [[BPR-1264]]. Fills a staged publication's documents within one
		/// bounded GUI turn, and adopts the publication once both are complete.
		void continue_publication();
		void queue_publication_continuation();
		void drop_staged_publication();
		/// \brief The publication's state changes, with the source documents
		/// either replaced in place or adopted already filled.
		void               adopt_publication(const application::diff_publication& aPublication,
											 QTextDocument* aLeft, QTextDocument* aRight);
		void               apply_frame(const comparison_scroll_frame& aFrame);
		[[nodiscard]] auto select_line(
			core::diff_side aSide,
			std::uint32_t   aLine,
			bool            aReportUnchangedInteraction = false) -> bool;
		void               select_link(const diff_link_map_identity& aIdentity, bool aReveal);
		void               synchronize_geometry_selection(bool aReveal);
		[[nodiscard]] auto reveal_exact_line(
			const core::semantic_selection& aSelection,
			const core::source_line_ref& aLine, bool aAtTop = false) -> bool;
		/// \brief Fully reveals a keyboard-selected row at the nearest edge.
		void reveal_keyboard_row(QPlainTextEdit* aPane, const QRect& aRow, const QRect& aVisible);
		/// \brief [[BPR-1476]]. A row's rect in a pane's viewport, however far
		/// it is from view: the keyboard's reveal and, since [[BPR-1542]],
		/// `line_rect`, which the open thread in the Comments drawer hangs from.
		[[nodiscard]] auto exact_row_rect(QPlainTextEdit* aPane, std::uint32_t aRow) const -> QRect;
		void               apply_interaction(bool aReveal, bool aAnnounceSelection);
		void               update_detail();
		/// \brief [[BPR-1334]]. Projects the current line, or the panes'
		/// text selection, into Line details.
		void update_line_details();
		void queue_line_details();
		void update_selection_paint();
		void announce_accessible_status(QString aText);
		/// \brief [[BPR-1335]]. The sentence that names the current line's thread, or empty.
		[[nodiscard]] auto thread_note() const -> QString;
		void               apply_pending();
		void               update_status(QString aText, bool aRoutine = false);
		void               emit_selection(bool aInteractive = false) noexcept;
		void               emit_reading_gesture() noexcept;
		void               emit_selection_observer(bool aInteractive) noexcept;
		/// \brief [[BPR-1212]]. Up, Down and Return on one pane; true when taken.
		[[nodiscard]] auto handle_line_key(QPlainTextEdit* aPane, QEvent* aEvent) -> bool;
		auto               move_pane_line_cursor(QPlainTextEdit* aPane, int aStep, bool aExtend) -> bool;
		/// \brief The rows a pane shows lines on: a side's lines, or the one
		/// pane's rows.
		[[nodiscard]] auto pane_row_count(const QPlainTextEdit* aPane) const -> std::uint32_t;
		/// \brief Where a pane's line cursor is: its anchor row and its moving
		/// row, which are the selected line's when it holds no selection.
		[[nodiscard]] auto line_cursor_ends(QPlainTextEdit* aPane) const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>;
		/// \brief Selects the line a pane's row shows.
		[[nodiscard]] auto select_pane_row(QPlainTextEdit* aPane, std::uint32_t aRow) -> bool;
		/// \brief Holds whole rows from the anchor to the next in the pane's
		/// text cursor, keeping its scroll.
		void place_line_cursor(QPlainTextEdit* aPane, std::uint32_t aAnchor, std::uint32_t aNext);
		/// \brief [[BPR-PENDING-AE]]. A click (`aExtend` false), Shift-click or
		/// drag on a line number.
		void select_numbered_line(QPlainTextEdit* aPane, std::uint32_t aRow, bool aExtend);
		void announce_line_selection();
		void clear_projection(QString aStatus);
		void set_idle(bool aIdle);

		QLabel* Status = nullptr;
		struct find_presentation;
		std::unique_ptr<find_presentation> Find;
		void                               initialize_source_find();
		void                               dispatch_source_find(source_find_gesture aGesture);
		void                               clear_source_find_document();
		void                               append_find_selections(QPlainTextEdit& aPane, QList<QTextEdit::ExtraSelection>& aSelections);
		void                               refresh_find_ticks();
		void                               place_source_find();
		[[nodiscard]] auto                 find_wash(bool aCurrent) const -> QColor;
		[[nodiscard]] auto                 source_byte_at(QPlainTextEdit& aPane, int aPosition) const -> std::optional<std::pair<core::diff_side, std::size_t>>;
		[[nodiscard]] auto                 source_text_position(core::diff_side aSide, std::size_t aByte) -> std::optional<int>;
		[[nodiscard]] auto                 source_match_fragments(const core::source_match_location& aLocation) -> const std::vector<std::pair<int, int>>&;
		bool                               StatusShown = true;
		// [[BPR-1287]]. The direction shown in place of the panes while idle.
		QWidget* IdlePage = nullptr;
		QWidget* PanesTop = nullptr;
		/// [[BPR-1576]]. The comparison's wait, and its colours' arrival.
		pending_veil*             Veil              = nullptr;
		bool                      ComparisonLoading = false;
		std::array<bool, 2>       SyntaxAwaited{ };
		QStackedLayout*           PanesSlot     = nullptr;
		QLabel*                   IdleTitle     = nullptr;
		QLabel*                   IdleDetail    = nullptr;
		QPushButton*              IdleOpenFirst = nullptr;
		QPushButton*              IdleShowFiles = nullptr;
		idle_direction            IdleDirection;
		bool                      Idle       = true;
		diff_comparison_splitter* Comparison = nullptr;
		QPlainTextEdit*           Left       = nullptr;
		QPlainTextEdit*           Right      = nullptr;
		QPlainTextEdit*           Unified    = nullptr;
		one_pane_diff_link_map*   UnifiedMap = nullptr;
		struct unified_presentation;
		std::unique_ptr<unified_presentation> UnifiedPresentation;
		QScrollBar*                           SharedScroll      = nullptr;
		comparison_scroll_controller*         Controller        = nullptr;
		diff_link_map*                        LinkMap           = nullptr;
		line_details_splitter*                Vertical          = nullptr;
		line_details_pane*                    Detail            = nullptr;
		bool                                  LineDetailsQueued = false;
		/// What a selection announcement names: "Change 3 of 11" or
		/// "Selected line 12".
		QString SelectionHeading;

		std::optional<core::diff_request_context> CurrentContext;
		diff_presentation_colors                  Colors;
		/// Segment identities that take part in a detected move, built once
		/// per publication. `file_alignment::Moves` is unordered with respect
		/// to the segment list, so answering per segment by searching it would
		/// make painting one screenful cost the whole move table.
		std::set<core::alignment_segment_id>             MovedSegments;
		application::shared_token_spans                  LeftSpans;
		application::shared_token_spans                  RightSpans;
		std::shared_ptr<const core::computed_file_diff>  CurrentResult;
		std::shared_ptr<const core::diff_semantic_index> SemanticIndex;
		std::optional<core::line_detail_input>           CurrentDetail;
		core::diff_interaction_state                     Interaction;
		selection_sink                                   SelectionSink;
		std::function<void()>                            ReadingGestureSink;
		std::optional<core::source_line_ref>             RestoredReadingLine;
		std::optional<core::source_line_ref>             PresentationReadingLine;
		/// A removed/added viewport row can belong to the opposite side from
		/// the semantic selection. Keep its side after the pending reveal is
		/// consumed so the next presentation flip measures the same document.
		std::optional<core::diff_side> PresentationReadingSide;
		selection_observer             SelectionObserver;
		/// [[BPR-1132]]. The status's observer, and whether it is routine.
		std::function<void(const QString&, bool)> StatusObserver;
		bool                                      StatusRoutine = false;
		std::function<void()>                     FrameObserver;
		std::optional<core::source_line_ref>      ConversationAnchor;
		measured_alignment_axis_cache             AxisCache;
		/// [[BPR-1123]]. Each source pane's recorded block heights, cleared
		/// whenever its text is replaced.
		pane_block_heights        LeftHeights;
		pane_block_heights        RightHeights;
		diff_workbench_statistics Statistics;
		QString                   LastAnnouncement;
		/// The last published tab stop. Absent until the first publication, so
		/// adopting the initial typography is not mistaken for a change.
		std::optional<qreal>          AppliedTabStopDistance;
		diff_geometry_rebuild_trigger PendingGeometryTriggers
			= diff_geometry_rebuild_trigger::none;
		struct geometry_measurement;
		std::shared_ptr<geometry_measurement> PendingMeasurement;
		/// [[BPR-1264]]. A large publication whose detached documents are
		/// still being filled; nothing else observes them before adoption.
		struct staged_publication;
		std::shared_ptr<staged_publication> PendingPublication;
		bool                                PublicationScheduled  = false;
		bool                                GeometryRevealPending = false;
		bool                                GeometryScheduled     = false;
		bool                                ApplyingFrame         = false;
		bool                                OnePaneEnabled        = false;
		/// [[BPR-1290]]. Thread markers in the gutter and ticks on the bar.
		bool                                                ThreadMarkers = false;
		std::function<void(core::diff_side, std::uint32_t)> LineMarkDispatch;
		std::function<void(core::diff_side, std::uint32_t)> ThreadTickDispatch;
		void                                                refresh_thread_ticks();
		/// [[BPR-1212]]. Return and Shift+Return in a code pane.
		std::function<void(bool)> LineDraftDispatch;
		/// [[BPR-1336]]. Zoom gestures, their partial accumulations, and the pill.
		std::function<void(core::code_zoom_step)> CodeZoomDispatch;
		int                                       ZoomWheelAngle     = 0;
		qreal                                     ZoomPinch          = 0.0;
		QWidget*                                  ZoomIndicator      = nullptr;
		QTimer*                                   ZoomIndicatorTimer = nullptr;
		/// [[BPR-1336]]. Where the reader's line sat when the code font changed:
		/// the row height it was drawn at, its offset in the viewport and, when
		/// it was on screen, the line itself. `HeldLine` is the top line the
		/// font change scrolled to, placed exactly once the remeasured geometry
		/// is in.
		struct reading_hold
		{
			qreal                                Previous = 0.0;
			qreal                                Offset   = 0.0;
			std::optional<core::source_line_ref> Line;
		};
		std::optional<reading_hold> Hold;
		struct held_line
		{
			core::diff_side Side = core::diff_side::left;
			std::uint32_t   Top  = 0U;
		};
		std::optional<held_line> HeldLine;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP
