#include "bpr/qt/measured_alignment_axis.hpp"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTextBlock>
#include <QTextDocument>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto finite_nonnegative(const qreal aValue) noexcept -> bool
		{
			return std::isfinite(aValue) && aValue >= 0.0;
		}

		[[nodiscard]] auto valid_measurements(
			const pane_line_measurements& aMeasurements,
			const std::uint32_t           aLineCount) noexcept -> bool
		{
			if (aMeasurements.Boundaries.size()
					!= static_cast<std::size_t>(aLineCount) + 1U
				|| !finite_nonnegative(aMeasurements.DocumentExtent)
				|| !finite_nonnegative(aMeasurements.ViewportExtent)
				|| !std::isfinite(aMeasurements.DevicePixelRatio)
				|| aMeasurements.DevicePixelRatio <= 0.0) {
				return false;
			}

			if (!std::ranges::all_of(
					aMeasurements.Boundaries,
					[](const qreal aBoundary) {
						return finite_nonnegative(aBoundary);
					})) {
				return false;
			}

			return std::ranges::is_sorted(aMeasurements.Boundaries)
				&& aMeasurements.Boundaries.back()
				<= aMeasurements.DocumentExtent;
		}

		[[nodiscard]] auto project_within(
			const measured_alignment_segment& aSegment,
			const qreal                       aPosition) noexcept
			-> measured_alignment_projection
		{
			const auto extent = aSegment.ComparisonEnd
				- aSegment.ComparisonBegin;
			const auto ratio = extent > 0.0
				? std::clamp(
					  (aPosition - aSegment.ComparisonBegin) / extent,
					  0.0,
					  1.0)
				: 0.0;
			return {
				.Segment            = aSegment.Id,
				.ComparisonPosition = aPosition,
				.LeftDocumentY      = std::lerp(
					aSegment.LeftBegin, aSegment.LeftEnd, ratio),
				.RightDocumentY = std::lerp(
					aSegment.RightBegin, aSegment.RightEnd, ratio),
			};
		}
	} // namespace

	struct plain_text_pane_measurement::state
	{
		QPointer<const QPlainTextEdit>              Pane;
		QPointer<const QTextDocument>               Document;
		QPointer<const QAbstractTextDocumentLayout> Layout;
		QTextBlock                                  Block;
		QFont                                       Font;
		QFont                                       DocumentFont;
		QSize                                       ViewportSize;
		int                                         Revision     = 0;
		int                                         WrapMode     = 0;
		int                                         WordWrapMode = 0;
		qreal                                       Margin       = 0.0;
		qreal                                       TextWidth    = 0.0;
		qreal                                       TabDistance  = 0.0;
		std::uint32_t                               LineCount    = 0U;
		std::uint32_t                               NextLine     = 0U;
		pane_measurement_progress                   Progress     = pane_measurement_progress::invalidated;
		pane_line_measurements                      Measurements;

		[[nodiscard]] auto current() const -> bool
		{
			return Pane && Document && Layout && Pane->document() == Document
				&& Document->documentLayout() == Layout && Document->revision() == Revision
				&& Pane->viewport() && Pane->viewport()->size() == ViewportSize
				&& Pane->devicePixelRatioF() == Measurements.DevicePixelRatio
				&& Pane->font() == Font && Document->defaultFont() == DocumentFont
				&& Document->documentMargin() == Margin && Document->textWidth() == TextWidth
				&& Pane->tabStopDistance() == TabDistance
				&& static_cast<int>(Pane->lineWrapMode()) == WrapMode
				&& static_cast<int>(Pane->wordWrapMode()) == WordWrapMode;
		}
	};

	plain_text_pane_measurement::plain_text_pane_measurement(
		const QPlainTextEdit& aPane, const std::uint32_t aLogicalLineCount)
		: State{ std::make_unique<state>() }
	{
		const auto* document = aPane.document();
		const auto* viewport = aPane.viewport();
		const auto* layout   = document ? document->documentLayout() : nullptr;
		if (!document || !viewport || !layout
			|| aLogicalLineCount > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
			return;
		}
		const auto count = static_cast<std::uint32_t>(document->blockCount());
		if (count < aLogicalLineCount || count > aLogicalLineCount + 1U
			|| (count == aLogicalLineCount + 1U && !document->lastBlock().text().isEmpty())) {
			return;
		}
		auto& value        = *State;
		value.Pane         = &aPane;
		value.Document     = document;
		value.Layout       = layout;
		value.Block        = document->begin();
		value.Font         = aPane.font();
		value.DocumentFont = document->defaultFont();
		value.ViewportSize = viewport->size();
		value.Revision     = document->revision();
		value.WrapMode     = static_cast<int>(aPane.lineWrapMode());
		value.WordWrapMode = static_cast<int>(aPane.wordWrapMode());
		value.Margin       = document->documentMargin();
		value.TextWidth    = document->textWidth();
		value.TabDistance  = aPane.tabStopDistance();
		value.LineCount    = aLogicalLineCount;
		value.Measurements = { .DocumentExtent   = layout->documentSize().height(),
							   .ViewportExtent   = static_cast<qreal>(viewport->height()),
							   .DevicePixelRatio = aPane.devicePixelRatioF() };
		value.Measurements.Boundaries.reserve(static_cast<std::size_t>(aLogicalLineCount) + 1U);
		value.Measurements.Boundaries.push_back(value.Margin);
		value.Progress = pane_measurement_progress::pending;
	}

	plain_text_pane_measurement::~plain_text_pane_measurement()                                                         = default;
	plain_text_pane_measurement::plain_text_pane_measurement(plain_text_pane_measurement&&) noexcept                    = default;
	auto plain_text_pane_measurement::operator=(plain_text_pane_measurement&&) noexcept -> plain_text_pane_measurement& = default;

	auto plain_text_pane_measurement::advance(const std::uint32_t aMaximumLines) -> pane_measurement_progress
	{
		if (!State || State->Progress == pane_measurement_progress::invalidated) {
			return pane_measurement_progress::invalidated;
		}
		auto& value = *State;
		if (!value.current()) {
			value.Progress = pane_measurement_progress::invalidated;
			return value.Progress;
		}
		if (value.Progress == pane_measurement_progress::complete) {
			return value.Progress;
		}
		const auto count = std::min(aMaximumLines, value.LineCount - value.NextLine);
		for (std::uint32_t index = 0U; index < count; ++index) {
			if (!value.Block.isValid()) {
				value.Progress = pane_measurement_progress::invalidated;
				return value.Progress;
			}
			value.Measurements.Boundaries.push_back(value.Measurements.Boundaries.back()
													+ value.Layout->blockBoundingRect(value.Block).height());
			value.Block = value.Block.next();
			++value.NextLine;
		}
		// Native layout can synchronously change scrollbars and viewport size.
		// Reject that batch instead of mixing measurements from two layouts.
		if (!value.current()) {
			value.Progress = pane_measurement_progress::invalidated;
		} else if (value.NextLine == value.LineCount) {
			value.Measurements.DocumentExtent = std::max(value.Measurements.DocumentExtent,
														 value.Measurements.Boundaries.back());
			value.Progress                    = valid_measurements(value.Measurements, value.LineCount)
				? pane_measurement_progress::complete
				: pane_measurement_progress::invalidated;
		}
		return value.Progress;
	}

	auto plain_text_pane_measurement::measured_lines() const noexcept -> std::uint32_t
	{
		return State ? State->NextLine : 0U;
	}

	auto plain_text_pane_measurement::result() const -> const pane_line_measurements*
	{
		return State && State->Progress == pane_measurement_progress::complete && State->current()
			? &State->Measurements
			: nullptr;
	}

	auto measure_plain_text_pane(
		const QPlainTextEdit& aPane,
		const std::uint32_t   aLogicalLineCount)
		-> std::optional<pane_line_measurements>
	{
		const auto* const document = aPane.document();
		const auto* const viewport = aPane.viewport();
		const auto* const layout   = document != nullptr
			? document->documentLayout()
			: nullptr;
		if (document == nullptr || viewport == nullptr
			|| layout == nullptr
			|| aLogicalLineCount
				> static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
			return std::nullopt;
		}
		const auto blockCount = static_cast<std::uint32_t>(document->blockCount());
		if (blockCount < aLogicalLineCount
			|| blockCount > aLogicalLineCount + 1U
			|| (blockCount == aLogicalLineCount + 1U
				&& !document->lastBlock().text().isEmpty())) {
			return std::nullopt;
		}

		auto measurements = pane_line_measurements{
			.DocumentExtent   = layout->documentSize().height(),
			.ViewportExtent   = static_cast<qreal>(viewport->height()),
			.DevicePixelRatio = aPane.devicePixelRatioF(),
		};
		measurements.Boundaries.reserve(
			static_cast<std::size_t>(aLogicalLineCount) + 1U);
		auto documentY = document->documentMargin();
		measurements.Boundaries.push_back(documentY);
		auto block = document->begin();
		for (std::uint32_t line = 0U;
			 line < aLogicalLineCount;
			 ++line) {
			if (!block.isValid()) {
				return std::nullopt;
			}
			documentY += layout->blockBoundingRect(block).height();
			measurements.Boundaries.push_back(documentY);
			block = block.next();
		}
		measurements.DocumentExtent = std::max(
			measurements.DocumentExtent, measurements.Boundaries.back());

		return valid_measurements(measurements, aLogicalLineCount)
			? std::optional<pane_line_measurements>{ std::move(measurements) }
			: std::nullopt;
	}

	auto measured_alignment_axis::left_line_origin(
		const std::size_t aLine) const noexcept -> std::optional<qreal>
	{
		return aLine < LeftBoundaries.size()
			? std::optional<qreal>{ LeftBoundaries[aLine] }
			: std::nullopt;
	}

	auto measured_alignment_axis::right_line_origin(
		const std::size_t aLine) const noexcept -> std::optional<qreal>
	{
		return aLine < RightBoundaries.size()
			? std::optional<qreal>{ RightBoundaries[aLine] }
			: std::nullopt;
	}

	auto measured_alignment_segment::positive_extent() const noexcept -> bool
	{
		return ComparisonEnd > ComparisonBegin;
	}

	auto measured_alignment_axis::comparison() const noexcept
		-> core::file_comparison_digest
	{
		return Comparison;
	}

	auto measured_alignment_axis::segments() const noexcept
		-> std::span<const measured_alignment_segment>
	{
		return Segments;
	}

	auto measured_alignment_axis::positive_segment_count() const noexcept
		-> std::size_t
	{
		return PositiveSegments.size();
	}

	auto measured_alignment_axis::comparison_extent() const noexcept -> qreal
	{
		return ComparisonExtent;
	}

	auto measured_alignment_axis::left_document_extent() const noexcept -> qreal
	{
		return LeftDocumentExtent;
	}

	auto measured_alignment_axis::right_document_extent() const noexcept -> qreal
	{
		return RightDocumentExtent;
	}

	auto measured_alignment_axis::left_viewport_extent() const noexcept -> qreal
	{
		return LeftViewportExtent;
	}

	auto measured_alignment_axis::right_viewport_extent() const noexcept -> qreal
	{
		return RightViewportExtent;
	}

	auto measured_alignment_axis::clamp(const qreal aPosition) const noexcept -> qreal
	{
		if (!std::isfinite(aPosition)) {
			return 0.0;
		}
		return std::clamp(aPosition, 0.0, ComparisonExtent);
	}

	auto measured_alignment_axis::project(const qreal aPosition) const noexcept
		-> std::optional<measured_alignment_projection>
	{
		if (!std::isfinite(aPosition)) {
			return std::nullopt;
		}

		const auto position = clamp(aPosition);
		if (PositiveSegments.empty()) {
			return measured_alignment_projection{
				.ComparisonPosition = position,
				.LeftDocumentY      = LeftDocumentBegin,
				.RightDocumentY     = RightDocumentBegin,
			};
		}

		const auto found = position >= ComparisonExtent
			? std::prev(PositiveSegments.end())
			: std::ranges::upper_bound(
				  PositiveSegments,
				  position,
				  std::ranges::less{ },
				  [this](const std::size_t aIndex) {
					  return Segments[aIndex].ComparisonEnd;
				  });
		return project_within(Segments[*found], position);
	}

	auto build_measured_alignment_axis(
		const core::file_alignment&   aAlignment,
		const pane_line_measurements& aLeft,
		const pane_line_measurements& aRight)
		-> measured_alignment_build_result
	{
		if (!core::validate_alignment(aAlignment).valid()
			|| aAlignment.Content != core::diff_content_state::text) {
			return { .Error = measured_alignment_error::invalid_alignment };
		}
		if (aLeft.Boundaries.size()
				!= static_cast<std::size_t>(aAlignment.LeftLineCount) + 1U
			|| aRight.Boundaries.size()
				!= static_cast<std::size_t>(aAlignment.RightLineCount) + 1U) {
			return { .Error = measured_alignment_error::line_count_mismatch };
		}
		if (!valid_measurements(aLeft, aAlignment.LeftLineCount)
			|| !valid_measurements(aRight, aAlignment.RightLineCount)) {
			return { .Error = measured_alignment_error::invalid_pane_geometry };
		}

		auto axis                = measured_alignment_axis{ };
		axis.Comparison          = aAlignment.Digest;
		axis.LeftBoundaries      = aLeft.Boundaries;
		axis.RightBoundaries     = aRight.Boundaries;
		axis.LeftDocumentBegin   = aLeft.Boundaries.front();
		axis.RightDocumentBegin  = aRight.Boundaries.front();
		axis.LeftDocumentExtent  = aLeft.DocumentExtent;
		axis.RightDocumentExtent = aRight.DocumentExtent;
		axis.LeftViewportExtent  = aLeft.ViewportExtent;
		axis.RightViewportExtent = aRight.ViewportExtent;
		axis.Segments.reserve(aAlignment.Segments.size());
		axis.PositiveSegments.reserve(aAlignment.Segments.size());

		qreal comparison = 0.0;
		for (const auto& semantic : aAlignment.Segments) {
			const auto leftBegin  = aLeft.Boundaries[semantic.Left.Begin];
			const auto leftEnd    = aLeft.Boundaries[semantic.Left.End];
			const auto rightBegin = aRight.Boundaries[semantic.Right.Begin];
			const auto rightEnd   = aRight.Boundaries[semantic.Right.End];
			const auto extent     = std::max(
				leftEnd - leftBegin, rightEnd - rightBegin);
			const auto comparisonEnd = comparison + extent;
			if (!std::isfinite(comparisonEnd)) {
				return {
					.Error = measured_alignment_error::comparison_extent_overflow,
				};
			}

			axis.Segments.push_back(measured_alignment_segment{
				.Id              = semantic.Id,
				.Kind            = semantic.Kind,
				.Group           = semantic.Group,
				.LeftBegin       = leftBegin,
				.LeftEnd         = leftEnd,
				.RightBegin      = rightBegin,
				.RightEnd        = rightEnd,
				.ComparisonBegin = comparison,
				.ComparisonEnd   = comparisonEnd,
			});
			if (extent > 0.0) {
				axis.PositiveSegments.push_back(axis.Segments.size() - 1U);
			}
			comparison = comparisonEnd;
		}
		axis.ComparisonExtent = comparison;

		return {
			.Axis = std::move(axis),
		};
	}

	void measured_alignment_axis_cache::invalidate(
		const measured_geometry_change aChange) noexcept
	{
		if (aChange == measured_geometry_change::none) {
			return;
		}
		PendingChanges = PendingChanges | aChange;
		Axis.reset();
	}

	auto measured_alignment_axis_cache::rebuild(
		const core::file_alignment&   aAlignment,
		const pane_line_measurements& aLeft,
		const pane_line_measurements& aRight)
		-> measured_alignment_error
	{
		auto result = build_measured_alignment_axis(aAlignment, aLeft, aRight);
		if (!result.valid()) {
			Axis.reset();
			PendingChanges = PendingChanges
				| measured_geometry_change::document_layout;
			return result.Error;
		}

		Axis           = std::move(result.Axis);
		PendingChanges = measured_geometry_change::none;
		++GeometryGeneration;
		++RebuildCount;
		return measured_alignment_error::none;
	}

	auto measured_alignment_axis_cache::axis() const noexcept
		-> const measured_alignment_axis*
	{
		return dirty() ? nullptr : &*Axis;
	}

	auto measured_alignment_axis_cache::dirty() const noexcept -> bool
	{
		return PendingChanges != measured_geometry_change::none
			|| !Axis.has_value();
	}

	auto measured_alignment_axis_cache::pending_changes() const noexcept
		-> measured_geometry_change
	{
		return PendingChanges;
	}

	auto measured_alignment_axis_cache::geometry_generation() const noexcept
		-> std::uint64_t
	{
		return GeometryGeneration;
	}

	auto measured_alignment_axis_cache::rebuild_count() const noexcept
		-> std::uint64_t
	{
		return RebuildCount;
	}
} // namespace bpr::qt
