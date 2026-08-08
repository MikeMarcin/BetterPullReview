#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/core/diff_interaction.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diff_link_map.hpp"
#include <QColor>
#include <QList>
#include <QTextEdit>

#include <QPointer>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>

class QEvent;
class QLabel;
class QPlainTextEdit;
class QResizeEvent;
class QScrollBar;
class QSplitter;
class QToolButton;

namespace bpr::qt {
	/// \brief Records directly observable composed-workbench activity.
	struct diff_workbench_statistics
	{
		std::uint64_t Publications          = 0U;
		std::uint64_t Failures              = 0U;
		std::uint64_t Invalidations         = 0U;
		std::uint64_t GeometryRebuilds      = 0U;
		std::uint64_t GeometryFailures      = 0U;
		std::uint64_t FrameApplications     = 0U;
		std::uint64_t SelectionChanges      = 0U;
		std::uint64_t RejectedStaleInput    = 0U;
		std::uint64_t SemanticIndexBuilds   = 0U;
		std::uint64_t SemanticQueries       = 0U;
		std::uint64_t PaneMeasurements      = 0U;
		std::uint64_t TokenSpanAdoptions    = 0U;
		std::uint64_t RejectedTokenSpans    = 0U;
		std::uint64_t SyntaxBlocksFormatted = 0U;
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
		std::size_t   SemanticIndexEntries     = 0U;

		auto operator<=>(const diff_workbench_statistics&) const = default;
	};

	/// \brief Presents one immutable computed diff through the selected Qt path.
	class diff_workbench final
		: public QWidget,
		  public application::diff_projection_port
	{
	public:
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
		void clear();

		[[nodiscard]] auto left_pane() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto right_pane() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto detail_pane() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto previous_change_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto next_change_button() const noexcept -> QToolButton*;
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
		[[nodiscard]] auto current_result() const noexcept
			-> const std::shared_ptr<const core::computed_file_diff>&;
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
			bool                                            Supplied = false;
		};

		void adopt_code_typography(const code_typography_event& aTypography);
		void update_diff_paint();
		void apply_syntax_formats(
			QPlainTextEdit& aPane,
			core::diff_side aSide,
			core::line_span aPaintable);
		/// \brief Hands each pane the lines its change groups start on.
		void apply_group_boundaries();
		/// \brief The lines one side's selection covers.
		[[nodiscard]] auto selected_lines(core::diff_side aSide) const
			-> std::optional<core::line_span>;
		/// \brief Whether the named segment takes part in a detected move.
		[[nodiscard]] auto moved(const core::alignment_segment_id& aSegment) const
			-> bool;
		void append_intraline_emphasis(
			QList<QTextEdit::ExtraSelection>& aSelections,
			QPlainTextEdit&                   aPane,
			core::diff_side                   aSide,
			core::line_span                   aPaintable);
		/// \brief Tells the link map where the pane viewports actually are.
		void declare_link_map_band();
		void schedule_geometry_rebuild(measured_geometry_change aChange);
		void rebuild_geometry();
		void apply_frame(const comparison_scroll_frame& aFrame);
		void select_line(core::diff_side aSide, std::uint32_t aLine);
		void select_link(const diff_link_map_identity& aIdentity, bool aReveal);
		void navigate(core::change_navigation_direction aDirection);
		void synchronize_geometry_selection(bool aReveal);
		void apply_interaction(bool aReveal, bool aAnnounceSelection);
		void update_detail();
		void update_selection_paint();
		void update_action_state();
		void announce_accessible_status(QString aText);
		void update_status(QString aText);
		void clear_projection(QString aStatus);

		QToolButton*                  PreviousChange = nullptr;
		QToolButton*                  NextChange     = nullptr;
		QLabel*                       Status         = nullptr;
		diff_comparison_splitter*     Comparison     = nullptr;
		QPlainTextEdit*               Left           = nullptr;
		QPlainTextEdit*               Right          = nullptr;
		QScrollBar*                   SharedScroll   = nullptr;
		comparison_scroll_controller* Controller     = nullptr;
		diff_link_map*                LinkMap        = nullptr;
		QSplitter*                    Vertical       = nullptr;
		QPlainTextEdit*               Detail         = nullptr;

		std::optional<core::diff_request_context> CurrentContext;
		diff_presentation_colors                  Colors;
		/// Segment identities that take part in a detected move, built once
		/// per publication. `file_alignment::Moves` is unordered with respect
		/// to the segment list, so answering per segment by searching it would
		/// make painting one screenful cost the whole move table.
		std::set<core::alignment_segment_id>            MovedSegments;
		application::shared_token_spans                 LeftSpans;
		application::shared_token_spans                 RightSpans;
		std::shared_ptr<const core::computed_file_diff> CurrentResult;
		std::optional<core::diff_semantic_index>        SemanticIndex;
		std::optional<core::line_detail_input>          CurrentDetail;
		core::diff_interaction_state                    Interaction;
		measured_alignment_axis_cache                   AxisCache;
		diff_workbench_statistics                       Statistics;
		QString                                         LastAnnouncement;
		/// The last published tab stop. Absent until the first publication, so
		/// adopting the initial typography is not mistaken for a change.
		std::optional<qreal> AppliedTabStopDistance;
		bool                 GeometryScheduled = false;
		bool                 ApplyingFrame     = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DIFF_WORKBENCH_HPP
