#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MEASURED_ALIGNMENT_AXIS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MEASURED_ALIGNMENT_AXIS_HPP

#include "bpr/core/diff_model.hpp"

#include <QtTypes>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

class QPlainTextEdit;

namespace bpr::qt {
	/// \brief Identifies a presentation fact that invalidates measured diff geometry.
	///
	/// \details There is no `line_spacing`. BPR-0118 retired the code line
	/// height on the product owner's decision, after measuring that
	/// `QPlainTextDocumentLayout` ignores every line-height mode Qt offers.
	/// A flag naming a fact that can never occur is a branch nothing reaches
	/// and a reader has to rule out.
	enum class measured_geometry_change : std::uint32_t {
		none                 = 0U,
		accepted_diff        = 1U << 0U,
		file_or_epoch        = 1U << 1U,
		font                 = 1U << 2U,
		zoom                 = 1U << 3U,
		tab_width            = 1U << 4U,
		device_pixel_ratio   = 1U << 5U,
		screen               = 1U << 6U,
		viewport_or_divider  = 1U << 7U,
		wrapping             = 1U << 8U,
		folding              = 1U << 9U,
		document_layout      = 1U << 10U,
		variable_height_item = 1U << 11U,
	};

	[[nodiscard]] constexpr auto operator|(
		const measured_geometry_change aLeft,
		const measured_geometry_change aRight) noexcept
		-> measured_geometry_change
	{
		return static_cast<measured_geometry_change>(
			static_cast<std::uint32_t>(aLeft)
			| static_cast<std::uint32_t>(aRight));
	}

	[[nodiscard]] constexpr auto operator&(
		const measured_geometry_change aLeft,
		const measured_geometry_change aRight) noexcept
		-> measured_geometry_change
	{
		return static_cast<measured_geometry_change>(
			static_cast<std::uint32_t>(aLeft)
			& static_cast<std::uint32_t>(aRight));
	}

	/// \brief Holds line-boundary facts measured from one live Qt text pane.
	struct pane_line_measurements
	{
		std::vector<qreal> Boundaries;
		qreal              DocumentExtent   = 0.0;
		qreal              ViewportExtent   = 0.0;
		qreal              DevicePixelRatio = 1.0;
		/// Whether the pane shows Qt's empty block after a final line break.
		/// It is the pane's last line, so it is the line [[BPR-1291]]'s
		/// scroll-past-end can bring to the top.
		bool TrailingVisualBlock = false;

		auto operator<=>(const pane_line_measurements&) const = default;
	};

	enum class pane_measurement_progress {
		pending,
		complete,
		invalidated,
	};

	/// \brief The block heights one unwrapped pane's measurements found, kept
	/// for its next measurement.
	///
	/// \details [[BPR-1123]]. An unwrapped pane measures each block in a
	/// transient layout ([[BPR-1032]]), so, unlike a native layout, nothing
	/// keeps the answer. Every later measurement of the pane (after a viewport
	/// or divider change, a restarted measurement, a style or DPI event) would
	/// otherwise shape every block again, although an unwrapped block's height
	/// and natural width do not depend on the viewport.
	///
	/// The owner keeps one of these per pane and gives it to each measurement
	/// of that pane. A measurement reads the heights an earlier one recorded
	/// and records the ones it measures. A recorded block is reused only while
	/// every fact its height depends on is unchanged: the document and its
	/// revision and block count, the pane's and the document's fonts, the
	/// device pixel ratio, the margin, the tab stop, and the properties of the
	/// block's own additional formats that its layout reads. Anything else
	/// starts it empty again. The owner clears it whenever it replaces the
	/// pane's text, because a document's revision alone does not identify its
	/// content.
	///
	/// [[BPR-1142]]. It costs 16 bytes a block: a 64-bit key standing for the
	/// block's formats, and its height and natural width in Qt's 1/64 px
	/// layout units, which hold every value a text layout produces exactly.
	class pane_block_heights final
	{
	public:
		pane_block_heights();
		~pane_block_heights();
		pane_block_heights(pane_block_heights&&) noexcept;
		auto operator=(pane_block_heights&&) noexcept -> pane_block_heights&;
		pane_block_heights(const pane_block_heights&)                    = delete;
		auto operator=(const pane_block_heights&) -> pane_block_heights& = delete;

		/// \brief Forgets every recorded block and frees their storage.
		void clear() noexcept;

	private:
		friend class plain_text_pane_measurement;
		struct state;
		std::unique_ptr<state> State;
	};

	/// Measures a bounded number of native blocks per GUI-thread call. The
	/// caller owns scheduling and comparison/presentation-generation checks.
	/// No partial measurements are exposed as a usable pane geometry.
	class plain_text_pane_measurement final
	{
	public:
		plain_text_pane_measurement(const QPlainTextEdit& aPane, std::uint32_t aLogicalLineCount);
		/// \brief Measures with the pane's recorded block heights.
		///
		/// \details `aHeights` must outlive the measurement. It is emptied first
		/// if it was recorded under different facts (see `pane_block_heights`).
		plain_text_pane_measurement(const QPlainTextEdit& aPane, std::uint32_t aLogicalLineCount, pane_block_heights& aHeights);
		/// Reuses identical plain same-index rows when both panes' live facts
		/// agree. Both height stores must outlive this measurement and must not be
		/// moved or replaced while it is active. A peer
		/// change or unavailable record falls back to ordinary Qt measurement.
		plain_text_pane_measurement(const QPlainTextEdit& aPane, std::uint32_t aLogicalLineCount, pane_block_heights& aHeights, const QPlainTextEdit& aPeer, const pane_block_heights& aPeerHeights);
		~plain_text_pane_measurement();
		plain_text_pane_measurement(plain_text_pane_measurement&&) noexcept;
		auto operator=(plain_text_pane_measurement&&) noexcept -> plain_text_pane_measurement&;
		plain_text_pane_measurement(const plain_text_pane_measurement&)                    = delete;
		auto operator=(const plain_text_pane_measurement&) -> plain_text_pane_measurement& = delete;

		[[nodiscard]] auto advance(std::uint32_t aMaximumLines) -> pane_measurement_progress;
		[[nodiscard]] auto measured_lines() const noexcept -> std::uint32_t;
		/// \brief How many of the measured lines took a recorded height.
		[[nodiscard]] auto reused_lines() const noexcept -> std::uint32_t;
		[[nodiscard]] auto result() const -> const pane_line_measurements*;

	private:
		struct state;
		std::unique_ptr<state> State;
	};

	/// \brief Measures logical line boundaries from a live natural-layout pane.
	/// \details The requested count comes from the accepted semantic document.
	/// Qt's optional trailing visual block is used only as the final boundary and
	/// never becomes a synthetic source line.
	[[nodiscard]] auto measure_plain_text_pane(
		const QPlainTextEdit& aPane,
		std::uint32_t         aLogicalLineCount)
		-> std::optional<pane_line_measurements>;

	/// \brief Describes one semantic segment in measured comparison coordinates.
	struct measured_alignment_segment
	{
		core::alignment_segment_id           Id;
		core::alignment_kind                 Kind = core::alignment_kind::equal;
		std::optional<core::change_group_id> Group;
		qreal                                LeftBegin       = 0.0;
		qreal                                LeftEnd         = 0.0;
		qreal                                RightBegin      = 0.0;
		qreal                                RightEnd        = 0.0;
		qreal                                ComparisonBegin = 0.0;
		qreal                                ComparisonEnd   = 0.0;

		[[nodiscard]] auto positive_extent() const noexcept -> bool;
		auto               operator<=>(const measured_alignment_segment&) const = default;
	};

	/// \brief Describes the two-pane projection at one comparison position.
	struct measured_alignment_projection
	{
		std::optional<core::alignment_segment_id> Segment;
		qreal                                     ComparisonPosition = 0.0;
		qreal                                     LeftDocumentY      = 0.0;
		qreal                                     RightDocumentY     = 0.0;

		auto operator<=>(const measured_alignment_projection&) const = default;
	};

	struct measured_alignment_build_result;

	/// \brief Owns an immutable, indexed max-extent presentation axis.
	class measured_alignment_axis final
	{
	public:
		[[nodiscard]] auto comparison() const noexcept
			-> core::file_comparison_digest;
		[[nodiscard]] auto segments() const noexcept
			-> std::span<const measured_alignment_segment>;
		[[nodiscard]] auto positive_segment_count() const noexcept
			-> std::size_t;
		[[nodiscard]] auto comparison_extent() const noexcept -> qreal;
		[[nodiscard]] auto left_document_extent() const noexcept -> qreal;
		[[nodiscard]] auto right_document_extent() const noexcept -> qreal;
		[[nodiscard]] auto left_viewport_extent() const noexcept -> qreal;
		[[nodiscard]] auto right_viewport_extent() const noexcept -> qreal;
		/// \brief Document y of a line's top, on one side.
		///
		/// \details The same measurement the ribbon endpoints are built from.
		/// Exposed so the scroll origin can be taken from it too: BPR-0208 was
		/// two derivations of the same quantity that agreed at the top of a
		/// file and drifted apart down it, and sharing one basis is what stops
		/// that being possible rather than merely unlikely.
		[[nodiscard]] auto left_line_origin(std::size_t aLine) const noexcept
			-> std::optional<qreal>;
		[[nodiscard]] auto right_line_origin(std::size_t aLine) const noexcept
			-> std::optional<qreal>;
		/// \brief The scroll origin that puts a side's last line at the top.
		///
		/// \details [[BPR-1291]]. The panes scroll past their end the way VS
		/// Code does, until only the last line shows, so this, not the
		/// document's height less the viewport's, is the furthest a pane
		/// scrolls. Zero for a pane with one line or none.
		[[nodiscard]] auto last_line_origin(core::diff_side aSide) const noexcept
			-> qreal;
		/// \brief The line a pane puts at its top to come nearest an origin.
		///
		/// \details A pane that does not wrap scrolls by whole lines, and line
		/// 0's origin is 0 (the document margin shows above it), every other
		/// line's is its measured top. Clamped to the side's last line.
		[[nodiscard]] auto top_line_for_origin(
			core::diff_side aSide, qreal aOrigin) const noexcept -> std::size_t;

		[[nodiscard]] auto clamp(qreal aPosition) const noexcept -> qreal;
		[[nodiscard]] auto project(qreal aPosition) const noexcept
			-> std::optional<measured_alignment_projection>;

	private:
		friend auto build_measured_alignment_axis(
			const core::file_alignment&,
			const pane_line_measurements&,
			const pane_line_measurements&) -> measured_alignment_build_result;

		core::file_comparison_digest            Comparison;
		std::vector<measured_alignment_segment> Segments;
		std::vector<std::size_t>                PositiveSegments;
		/// Per-line document tops, retained rather than discarded after the
		/// segments are built, because the scroll origin needs them too.
		std::vector<qreal> LeftBoundaries;
		std::vector<qreal> RightBoundaries;
		/// The index of each side's last line: the trailing visual block
		/// when the pane shows one.
		std::size_t LeftLastLine        = 0U;
		std::size_t RightLastLine       = 0U;
		qreal       ComparisonExtent    = 0.0;
		qreal       LeftDocumentBegin   = 0.0;
		qreal       LeftDocumentExtent  = 0.0;
		qreal       RightDocumentBegin  = 0.0;
		qreal       RightDocumentExtent = 0.0;
		qreal       LeftViewportExtent  = 0.0;
		qreal       RightViewportExtent = 0.0;
	};

	/// \brief Identifies why measured-axis construction failed closed.
	enum class measured_alignment_error : std::uint8_t {
		none,
		invalid_alignment,
		line_count_mismatch,
		invalid_pane_geometry,
		comparison_extent_overflow,
	};

	/// \brief Describes the measured axis build result.
	struct measured_alignment_build_result
	{
		measured_alignment_error               Error = measured_alignment_error::none;
		std::optional<measured_alignment_axis> Axis;

		[[nodiscard]] auto valid() const noexcept -> bool
		{
			return Error == measured_alignment_error::none && Axis.has_value();
		}
	};

	/// \brief Builds one measured axis from validated semantic and pane facts.
	[[nodiscard]] auto build_measured_alignment_axis(
		const core::file_alignment&   aAlignment,
		const pane_line_measurements& aLeft,
		const pane_line_measurements& aRight)
		-> measured_alignment_build_result;

	/// \brief Caches one measured axis and coalesces explicit invalidations.
	class measured_alignment_axis_cache final
	{
	public:
		/// \brief Invalidates cached geometry for one or more explicit facts.
		void invalidate(measured_geometry_change aChange) noexcept;
		/// \brief Rebuilds the cache from settled semantic and pane facts.
		[[nodiscard]] auto rebuild(
			const core::file_alignment&   aAlignment,
			const pane_line_measurements& aLeft,
			const pane_line_measurements& aRight)
			-> measured_alignment_error;

		[[nodiscard]] auto axis() const noexcept
			-> const measured_alignment_axis*;
		[[nodiscard]] auto dirty() const noexcept -> bool;
		[[nodiscard]] auto pending_changes() const noexcept
			-> measured_geometry_change;
		[[nodiscard]] auto geometry_generation() const noexcept
			-> std::uint64_t;
		[[nodiscard]] auto rebuild_count() const noexcept -> std::uint64_t;

	private:
		std::optional<measured_alignment_axis> Axis;
		measured_geometry_change               PendingChanges
			= measured_geometry_change::accepted_diff;
		std::uint64_t GeometryGeneration = 0U;
		std::uint64_t RebuildCount       = 0U;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MEASURED_ALIGNMENT_AXIS_HPP
