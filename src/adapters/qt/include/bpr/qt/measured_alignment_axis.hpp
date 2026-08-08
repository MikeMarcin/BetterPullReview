#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MEASURED_ALIGNMENT_AXIS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MEASURED_ALIGNMENT_AXIS_HPP

#include "bpr/core/diff_model.hpp"

#include <QtTypes>

#include <compare>
#include <cstddef>
#include <cstdint>
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

		auto operator<=>(const pane_line_measurements&) const = default;
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
		qreal              ComparisonExtent    = 0.0;
		qreal              LeftDocumentBegin   = 0.0;
		qreal              LeftDocumentExtent  = 0.0;
		qreal              RightDocumentBegin  = 0.0;
		qreal              RightDocumentExtent = 0.0;
		qreal              LeftViewportExtent  = 0.0;
		qreal              RightViewportExtent = 0.0;
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
