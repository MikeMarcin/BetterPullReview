#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/measured_alignment_axis.hpp"

#include <QApplication>
#include <QFont>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto close(
		const qreal aActual,
		const qreal aExpected,
		const qreal aTolerance = 0.000'001) noexcept -> bool
	{
		return std::abs(aActual - aExpected) <= aTolerance;
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(
				aSeed + static_cast<std::uint8_t>(index));
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const core::diff_options& aOptions)
		-> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "fixture options are normalized");
		const auto scope
			= opaque<core::pull_request_authority_scope_id>(11U);
		const auto incarnation
			= opaque<core::epoch_catalog_incarnation_id>(12U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority
			= opaque<core::host_authority_fingerprint>(13U),
			.Account     = core::github_account_id{ 40U },
			.Viewer      = core::github_viewer_actor_id{ 50U },
			.Repository  = core::repository_id{ 101U },
			.Cache       = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest = core::pull_request_id{ 202U },
			.Number      = core::pull_request_number{ 17U },
			.Scope       = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ 1 },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<core::file_identity>(31U),
			.Options   = *digest,
			.Algorithm = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto bytes(const std::string_view aText)
		-> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	[[nodiscard]] auto computed(
		const std::string_view aLeft,
		const std::string_view aRight) -> core::computed_file_diff
	{
		auto       left    = bytes(aLeft);
		auto       right   = bytes(aRight);
		auto       options = core::diff_options{ };
		const auto outcome = core::compute_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(outcome.Reason == core::diff_reason_code::none,
			   "fixture diff computes");
		expect(outcome.Result.has_value(), "fixture diff publishes");
		expect(core::validate_computed_file_diff(*outcome.Result).valid(),
			   "fixture diff validates");
		return *outcome.Result;
	}

	[[nodiscard]] auto measurements(
		std::vector<qreal> aBoundaries,
		const qreal        aViewport = 100.0) -> qt::pane_line_measurements
	{
		const auto document = aBoundaries.empty()
			? 0.0
			: aBoundaries.back();
		return {
			.Boundaries       = std::move(aBoundaries),
			.DocumentExtent   = document,
			.ViewportExtent   = aViewport,
			.DevicePixelRatio = 1.0,
		};
	}

	[[nodiscard]] auto find_segment(
		const qt::measured_alignment_axis& aAxis,
		const core::alignment_kind         aKind)
		-> const qt::measured_alignment_segment&
	{
		const auto found = std::ranges::find_if(
			aAxis.segments(),
			[aKind](const auto& aSegment) {
				return aSegment.Kind == aKind;
			});
		expect(found != aAxis.segments().end(), "expected measured segment exists");
		return *found;
	}

	auto test_insertion_deletion_and_continuity() -> void
	{
		const auto insertion = computed(
			"alpha\ncharlie\ndelta\n",
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n");
		const auto built = qt::build_measured_alignment_axis(
			insertion.Alignment,
			measurements({ 0.0, 10.0, 20.0, 30.0 }, 80.0),
			measurements({ 0.0, 10.0, 25.0, 40.0, 50.0, 60.0 }, 120.0));
		expect(built.valid(), "right insertion axis builds");
		const auto& axis    = *built.Axis;
		const auto& segment = find_segment(
			axis, core::alignment_kind::right_only);
		const auto quarter = axis.project(
			segment.ComparisonBegin
			+ (segment.ComparisonEnd - segment.ComparisonBegin) * 0.25);
		const auto threeQuarter = axis.project(
			segment.ComparisonBegin
			+ (segment.ComparisonEnd - segment.ComparisonBegin) * 0.75);
		expect(quarter.has_value() && threeQuarter.has_value(),
			   "right insertion samples project");
		expect(close(quarter->LeftDocumentY, threeQuarter->LeftDocumentY),
			   "right insertion pins left boundary");
		expect(threeQuarter->RightDocumentY > quarter->RightDocumentY,
			   "right insertion advances right document");
		expect(quarter->Segment == segment.Id,
			   "projection retains semantic segment identity");
		const auto atBegin = axis.project(segment.ComparisonBegin);
		const auto atEnd   = axis.project(segment.ComparisonEnd);
		expect(atBegin.has_value() && atEnd.has_value()
				   && atBegin->Segment == segment.Id
				   && atEnd->Segment != segment.Id,
			   "exact boundaries select the following positive interval without ambiguity");
		expect(close(atBegin->LeftDocumentY, segment.LeftBegin)
				   && close(atBegin->RightDocumentY, segment.RightBegin)
				   && close(atEnd->LeftDocumentY, segment.LeftEnd)
				   && close(atEnd->RightDocumentY, segment.RightEnd),
			   "boundary handoff preserves continuous pane coordinates");

		const auto deletion = computed(
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n",
			"alpha\ncharlie\ndelta\n");
		const auto deleted = qt::build_measured_alignment_axis(
			deletion.Alignment,
			measurements({ 0.0, 10.0, 25.0, 40.0, 50.0, 60.0 }),
			measurements({ 0.0, 10.0, 20.0, 30.0 }));
		expect(deleted.valid(), "left deletion axis builds");
		const auto& deletedSegment = find_segment(
			*deleted.Axis, core::alignment_kind::left_only);
		const auto first = deleted.Axis->project(
			deletedSegment.ComparisonBegin + 5.0);
		const auto second = deleted.Axis->project(
			deletedSegment.ComparisonEnd - 5.0);
		expect(first.has_value() && second.has_value(),
			   "left deletion samples project");
		expect(close(first->RightDocumentY, second->RightDocumentY),
			   "left deletion pins right boundary");
		expect(second->LeftDocumentY > first->LeftDocumentY,
			   "left deletion advances left document");

		const auto segments = axis.segments();
		for (std::size_t index = 1U; index < segments.size(); ++index) {
			expect(close(
					   segments[index - 1U].ComparisonEnd,
					   segments[index].ComparisonBegin),
				   "comparison intervals are continuous");
			expect(close(segments[index - 1U].LeftEnd, segments[index].LeftBegin)
					   && close(segments[index - 1U].RightEnd, segments[index].RightBegin),
				   "pane coordinates are continuous at semantic boundaries");
		}
	}

	auto test_unequal_replacement_zero_extent_and_clamping() -> void
	{
		const auto replacement = computed(
			"left-a\nleft-b\nleft-c\nleft-d\n",
			"right-a\nright-b\nright-c\nright-d\nright-e\nright-f\nright-g\n");
		const auto built = qt::build_measured_alignment_axis(
			replacement.Alignment,
			measurements({ 0.0, 10.0, 20.0, 30.0, 40.0 }),
			measurements({ 0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0 }));
		expect(built.valid(), "4:7 replacement axis builds");
		expect(close(built.Axis->comparison_extent(), 70.0),
			   "max measured replacement extent owns comparison length");
		const auto midpoint = built.Axis->project(35.0);
		expect(midpoint.has_value(), "unequal replacement midpoint projects");
		expect(close(midpoint->LeftDocumentY, 20.0)
				   && close(midpoint->RightDocumentY, 35.0),
			   "unequal replacement advances proportionally");
		const auto beginning = built.Axis->project(-100.0);
		const auto ending    = built.Axis->project(1'000.0);
		expect(beginning.has_value() && ending.has_value(),
			   "out-of-range positions clamp to legal end caps");
		expect(close(beginning->ComparisonPosition, 0.0)
				   && close(ending->ComparisonPosition, 70.0)
				   && close(ending->LeftDocumentY, 40.0)
				   && close(ending->RightDocumentY, 70.0),
			   "start and end content are reachable without dead travel");

		const auto mirror = computed(
			"left-a\nleft-b\nleft-c\nleft-d\nleft-e\nleft-f\nleft-g\n",
			"right-a\nright-b\nright-c\nright-d\n");
		const auto mirrored = qt::build_measured_alignment_axis(
			mirror.Alignment,
			measurements({ 0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0 }),
			measurements({ 0.0, 10.0, 20.0, 30.0, 40.0 }));
		expect(mirrored.valid(), "7:4 replacement axis builds");
		const auto mirrorMidpoint = mirrored.Axis->project(35.0);
		expect(mirrorMidpoint.has_value()
				   && close(mirrorMidpoint->LeftDocumentY, 35.0)
				   && close(mirrorMidpoint->RightDocumentY, 20.0),
			   "mirrored unequal replacement converges at the same endpoint");

		const auto empty = computed("", "");
		const auto zero  = qt::build_measured_alignment_axis(
			empty.Alignment,
			measurements({ 4.0 }, 60.0),
			measurements({ 7.0 }, 90.0));
		expect(zero.valid() && zero.Axis->segments().empty()
				   && zero.Axis->positive_segment_count() == 0U,
			   "empty comparison has no synthetic positive segment");
		const auto zeroProjection = zero.Axis->project(0.0);
		expect(zeroProjection.has_value()
				   && !zeroProjection->Segment.has_value()
				   && close(zeroProjection->LeftDocumentY, 4.0)
				   && close(zeroProjection->RightDocumentY, 7.0),
			   "zero extent projects its retained semantic boundary safely");

		const auto equal  = computed("alpha\n", "alpha\n");
		const auto hidden = qt::build_measured_alignment_axis(
			equal.Alignment,
			measurements({ 4.0, 4.0 }),
			measurements({ 7.0, 7.0 }));
		expect(hidden.valid() && hidden.Axis->segments().size() == 1U
				   && hidden.Axis->positive_segment_count() == 0U,
			   "zero-height semantic segment remains retained outside positive lookup");
		expect(hidden.Axis->segments().front().Id
				   == equal.Alignment.Segments.front().Id,
			   "hidden presentation geometry preserves semantic identity");
	}

	auto test_validation_and_coalesced_invalidation() -> void
	{
		const auto diff  = computed("alpha\nbeta\n", "alpha\nbeta\n");
		const auto left  = measurements({ 0.0, 10.0, 20.0 });
		const auto right = measurements({ 0.0, 12.0, 24.0 });
		auto       cache = qt::measured_alignment_axis_cache{ };
		expect(cache.dirty() && cache.axis() == nullptr,
			   "new cache has no usable stale axis");
		expect(cache.rebuild(diff.Alignment, left, right)
				   == qt::measured_alignment_error::none,
			   "valid geometry rebuilds cache");
		const auto originalId = cache.axis()->segments().front().Id;
		cache.invalidate(
			qt::measured_geometry_change::accepted_diff
			| qt::measured_geometry_change::file_or_epoch
			| qt::measured_geometry_change::font
			| qt::measured_geometry_change::zoom
			| qt::measured_geometry_change::tab_width
			| qt::measured_geometry_change::device_pixel_ratio
			| qt::measured_geometry_change::screen
			| qt::measured_geometry_change::viewport_or_divider
			| qt::measured_geometry_change::wrapping
			| qt::measured_geometry_change::folding
			| qt::measured_geometry_change::document_layout
			| qt::measured_geometry_change::variable_height_item);
		expect(cache.dirty() && cache.axis() == nullptr,
			   "invalidated cache cannot publish stale measured geometry");
		expect(cache.rebuild_count() == 1U,
			   "settled invalidations coalesce before one rebuild");
		expect(cache.rebuild(diff.Alignment, left, right)
				   == qt::measured_alignment_error::none,
			   "coalesced geometry facts rebuild once");
		expect(cache.geometry_generation() == 2U
				   && cache.rebuild_count() == 2U,
			   "successful rebuild advances one geometry generation");
		expect(cache.axis()->segments().front().Id == originalId,
			   "presentation rebuild retains semantic segment identity");
		const auto rebuildsBeforeProjection = cache.rebuild_count();
		expect(cache.axis()->project(6.0).has_value()
				   && cache.rebuild_count() == rebuildsBeforeProjection
				   && !cache.dirty(),
			   "ordinary position projection does not invalidate or rebuild geometry");

		auto decreasing       = left;
		decreasing.Boundaries = { 0.0, 12.0, 11.0 };
		expect(qt::build_measured_alignment_axis(
				   diff.Alignment, decreasing, right)
					   .Error
				   == qt::measured_alignment_error::invalid_pane_geometry,
			   "decreasing pane boundaries fail closed");
		expect(cache.rebuild(diff.Alignment, decreasing, right)
					   == qt::measured_alignment_error::invalid_pane_geometry
				   && cache.dirty() && cache.axis() == nullptr,
			   "failed cache rebuild cannot expose prior geometry");
		auto mismatched = left;
		mismatched.Boundaries.pop_back();
		expect(qt::build_measured_alignment_axis(
				   diff.Alignment, mismatched, right)
					   .Error
				   == qt::measured_alignment_error::line_count_mismatch,
			   "line-count mismatch fails closed");
	}

	auto test_live_plain_text_measurement() -> void
	{
		auto surface = QWidget{ };
		auto left    = new QPlainTextEdit{ &surface };
		auto right   = new QPlainTextEdit{ &surface };
		left->setLineWrapMode(QPlainTextEdit::NoWrap);
		right->setLineWrapMode(QPlainTextEdit::NoWrap);
		left->setPlainText(QStringLiteral("alpha\ncharlie\ndelta\n"));
		right->setPlainText(
			QStringLiteral("alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n"));
		left->setGeometry(10, 10, 430, 180);
		right->setGeometry(470, 10, 430, 280);
		surface.resize(920, 310);
		surface.show();
		QApplication::processEvents();

		const auto diff = computed(
			"alpha\ncharlie\ndelta\n",
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n");
		const auto leftGeometry = qt::measure_plain_text_pane(
			*left, diff.Alignment.LeftLineCount);
		const auto rightGeometry = qt::measure_plain_text_pane(
			*right, diff.Alignment.RightLineCount);
		expect(leftGeometry.has_value() && rightGeometry.has_value(),
			   "live QPlainTextEdit line boundaries measure");
		expect(!qt::measure_plain_text_pane(
					*left, diff.Alignment.LeftLineCount - 1U)
					.has_value(),
			   "live pane rejects a semantic line count that omits real content");
		expect(leftGeometry->Boundaries.size()
					   == static_cast<std::size_t>(diff.Alignment.LeftLineCount) + 1U
				   && rightGeometry->Boundaries.size()
					   == static_cast<std::size_t>(diff.Alignment.RightLineCount) + 1U,
			   "Qt trailing visual block does not invent a source line");
		expect(std::ranges::is_sorted(leftGeometry->Boundaries)
				   && std::ranges::is_sorted(rightGeometry->Boundaries),
			   "live document boundaries are monotonic");
		const auto built = qt::build_measured_alignment_axis(
			diff.Alignment, *leftGeometry, *rightGeometry);
		expect(built.valid(), "live pane facts build measured axis");
		expect(close(
				   built.Axis->left_viewport_extent(),
				   static_cast<qreal>(left->viewport()->height()))
				   && close(
					   built.Axis->right_viewport_extent(),
					   static_cast<qreal>(right->viewport()->height())),
			   "each pane retains its own live viewport extent");
		expect(!close(
				   built.Axis->left_viewport_extent(),
				   built.Axis->right_viewport_extent()),
			   "different viewport heights remain independent measured facts");

		auto emptyPane = QPlainTextEdit{ };
		emptyPane.setPlainText(QString{ });
		emptyPane.resize(320, 140);
		emptyPane.show();
		QApplication::processEvents();
		const auto emptyGeometry = qt::measure_plain_text_pane(emptyPane, 0U);
		expect(emptyGeometry.has_value()
				   && emptyGeometry->Boundaries.size() == 1U,
			   "live empty pane exposes one boundary without a source row");

		if (const auto* expectedScale = std::getenv("BPR_EXPECTED_QT_SCALE")) {
			const auto scale = QString::fromLocal8Bit(expectedScale).toDouble();
			expect(close(leftGeometry->DevicePixelRatio, scale, 0.05)
					   && close(rightGeometry->DevicePixelRatio, scale, 0.05),
				   "live pane measurement observes selected Qt scale");
		}

		const auto before = leftGeometry->Boundaries;
		auto       font   = left->font();
		font.setPointSizeF(font.pointSizeF() + 2.0);
		left->setFont(font);
		left->document()->setDefaultFont(font);
		left->setTabStopDistance(left->tabStopDistance() + 8.0);
		left->resize(left->width(), left->height() - 30);
		QApplication::processEvents();
		const auto changed = qt::measure_plain_text_pane(
			*left, diff.Alignment.LeftLineCount);
		expect(changed.has_value(), "changed live pane remeasures");
		expect(changed->Boundaries != before,
			   "font geometry change alters measured document boundaries");
		expect(changed->ViewportExtent
				   == static_cast<qreal>(left->viewport()->height()),
			   "viewport resize is reflected in measured facts");

		if (const auto* renderPath = std::getenv("BPR_QT_DIFF_AXIS_RENDER_PATH")) {
			expect(surface.grab().save(QString::fromLocal8Bit(renderPath)),
				   "live natural-layout pane raster saves");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewDiffAxisTests"));

	test_insertion_deletion_and_continuity();
	test_unequal_replacement_zero_extent_and_clamping();
	test_validation_and_coalesced_invalidation();
	test_live_plain_text_measurement();

	std::cout << "measured-alignment-axis-tests:pass\n";
	return EXIT_SUCCESS;
}
