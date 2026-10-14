#include "bpr/core/diff_compute.hpp"
#include "product_diff.hpp"
#include "bpr/qt/measured_alignment_axis.hpp"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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
		const auto outcome = bpr::tests::product_file_diff({
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

	auto test_bounded_native_measurement() -> void
	{
		auto pane = QPlainTextEdit{ };
		pane.resize(240, 220);
		pane.setLineWrapMode(QPlainTextEdit::WidgetWidth);
		QString source;
		for (int line = 0; line < 40; ++line) {
			source += line % 3 == 0 ? QStringLiteral("Unicode αβγ 日本語 and a long line that wraps across the narrow native source pane\n") : QStringLiteral("short\n");
		}
		pane.setPlainText(source);
		QTextCursor styled{ pane.document() };
		styled.movePosition(QTextCursor::NextBlock);
		styled.select(QTextCursor::LineUnderCursor);
		QTextCharFormat format;
		format.setFontPointSize(24);
		styled.mergeCharFormat(format);
		pane.show();
		QApplication::processEvents();
		const auto reference = qt::measure_plain_text_pane(pane, 40U);
		expect(reference.has_value(), "mixed wrapped native reference measures");
		QApplication::processEvents();
		qt::plain_text_pane_measurement measurement{ pane, 40U };
		expect(!measurement.result() && measurement.advance(0U) == qt::pane_measurement_progress::pending
				   && measurement.measured_lines() == 0U,
			   "zero budget publishes no partial geometry");
		auto          progress = qt::pane_measurement_progress::pending;
		std::uint32_t previous = 0U;
		while (progress == qt::pane_measurement_progress::pending) {
			progress = measurement.advance(3U);
			expect(measurement.measured_lines() - previous <= 3U, "a batch never exceeds its native block budget");
			previous = measurement.measured_lines();
			if (progress == qt::pane_measurement_progress::pending) {
				expect(!measurement.result(), "incomplete geometry cannot escape between batches");
			}
			QApplication::processEvents();
		}
		expect(progress == qt::pane_measurement_progress::complete && measurement.result()
				   && *measurement.result() == *qt::measure_plain_text_pane(pane, 40U),
			   "batched native wrapped/Unicode/mixed-font boundaries equal the synchronous reference exactly");
		const auto stale_after = [&](auto aChange) {
			QApplication::processEvents();
			qt::plain_text_pane_measurement pending{ pane, 40U };
			expect(pending.advance(1U) == qt::pane_measurement_progress::pending, "one block leaves native work pending");
			aChange();
			expect(pending.advance(3U) == qt::pane_measurement_progress::invalidated && !pending.result(),
				   "changed pane facts invalidate partial measurement atomically");
		};
		stale_after([&] { pane.setPlainText(source + QStringLiteral("different")); });
		pane.setPlainText(source);
		stale_after([&] { auto font = pane.font(); font.setPointSizeF(font.pointSizeF() + 1); pane.setFont(font); });
		stale_after([&] { pane.resize(pane.width() + 40, pane.height()); });
		stale_after([&] { pane.setTabStopDistance(pane.tabStopDistance() + 8); });
		stale_after([&] { pane.setLineWrapMode(QPlainTextEdit::NoWrap); });
		stale_after([&] { pane.document()->setDocumentMargin(pane.document()->documentMargin() + 1); });
		auto transient = std::make_unique<QPlainTextEdit>();
		transient->setPlainText(QStringLiteral("one\ntwo\n"));
		qt::plain_text_pane_measurement destroyed{ *transient, 2U };
		transient.reset();
		expect(destroyed.advance(1U) == qt::pane_measurement_progress::invalidated && !destroyed.result(), "destroyed pane cannot be dereferenced by a continuation");
		qt::plain_text_pane_measurement wrongCount{ pane, 1U };
		expect(wrongCount.advance(1U) == qt::pane_measurement_progress::invalidated, "invalid logical count is refused before work");
		QPlainTextEdit                  empty;
		qt::plain_text_pane_measurement emptyMeasurement{ empty, 0U };
		expect(emptyMeasurement.advance(0U) == qt::pane_measurement_progress::complete
				   && emptyMeasurement.result() && *emptyMeasurement.result() == *qt::measure_plain_text_pane(empty, 0U),
			   "empty native geometry completes without inventing a source line");
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

		const auto expectedScale = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expectedScale.isEmpty()) {
			const auto scale = expectedScale.toDouble();
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

		const auto renderPath = qEnvironmentVariable("BPR_QT_DIFF_AXIS_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			expect(surface.grab().save(renderPath),
				   "live natural-layout pane raster saves");
		}
	}

	/// Gives every `wide` a larger font through the block's additional
	/// formats, which is where the review panes put syntax. The panes then
	/// mark the block dirty, which lays it out. Here the block is left
	/// unlaid, so a measurement that dropped a block's additional formats
	/// would get that line's height wrong.
	void set_larger_word_formats(QTextDocument& aDocument)
	{
		auto format = QTextCharFormat{ };
		format.setFontPointSize(30);
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			const auto at = block.text().indexOf(QStringLiteral("wide"));
			if (at >= 0) {
				block.layout()->setFormats({ QTextLayout::FormatRange{ .start = static_cast<int>(at), .length = 4, .format = format } });
			}
		}
	}

	/// The native answer for every block, the one the pane lays out and
	/// paints. Asking for it lays every block out, so it runs after the
	/// measurement under test.
	[[nodiscard]] auto native_boundaries(const QPlainTextEdit& aPane, const std::uint32_t aLines) -> std::vector<qreal>
	{
		const auto* const document = aPane.document();
		auto              values   = std::vector<qreal>{ document->documentMargin() };
		auto              block    = document->begin();
		for (std::uint32_t line = 0U; line < aLines && block.isValid(); ++line, block = block.next()) {
			values.push_back(values.back() + document->documentLayout()->blockBoundingRect(block).height());
		}
		return values;
	}

	/// [[BPR-1032]]. A `NoWrap` pane is measured without shaping each block in
	/// the pane's own layout, which Qt 6.11 would keep a HarfBuzz buffer in for
	/// the block's lifetime. The boundaries must still be the pane's native
	/// ones exactly, including a fallback-font line, a larger inline format
	/// and a block's additional formats. The pane's horizontal range
	/// must still cover its widest line. Without a trailing newline the last
	/// measured block is the document's last, which carries the bottom margin.
	auto test_transient_no_wrap_measurement(const bool aTrailingNewline) -> void
	{
		auto pane = QPlainTextEdit{ };
		pane.resize(260, 160);
		pane.setLineWrapMode(QPlainTextEdit::NoWrap);
		QString        source;
		constexpr auto lines = 400U;
		for (auto line = 0U; line < lines; ++line) {
			switch (line % 7U) {
			case 0U: source += QStringLiteral("fallback 日本語 αβγ 😀 line\n"); break;
			case 1U: source += QStringLiteral("\tindented\twith tabs\n"); break;
			case 2U: source += QStringLiteral("a wide word in a larger font\n"); break;
			case 3U: source += QString{ }.append(QChar{ '\n' }); break;
			// Widest in the middle, far below the viewport, so the pane has
			// not laid its widest line out before the measurement starts.
			case 4U: source += QStringLiteral("x").repeated(static_cast<int>(20U + std::min(line, lines - line))) + QChar{ '\n' }; break;
			default: source += QStringLiteral("plain ascii source line\n"); break;
			}
		}
		if (!aTrailingNewline) {
			source.chop(1);
		}
		pane.setPlainText(source);
		QTextCursor styled{ pane.document() };
		styled.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 5);
		styled.select(QTextCursor::LineUnderCursor);
		QTextCharFormat larger;
		larger.setFontPointSize(24);
		styled.mergeCharFormat(larger);
		pane.show();
		QApplication::processEvents();
		set_larger_word_formats(*pane.document());
		QApplication::processEvents();

		qt::plain_text_pane_measurement measurement{ pane, lines };
		auto                            progress = qt::pane_measurement_progress::pending;
		while (progress == qt::pane_measurement_progress::pending) {
			progress = measurement.advance(64U);
			QApplication::processEvents();
		}
		expect(progress == qt::pane_measurement_progress::complete && measurement.result(),
			   "the transient no-wrap measurement completes");
		const auto measured    = measurement.result()->Boundaries;
		const auto synchronous = qt::measure_plain_text_pane(pane, lines);
		expect(synchronous && synchronous->Boundaries == measured, "batched and synchronous no-wrap boundaries agree exactly");

		// Line 201 sits far below the viewport and is never the widest so
		// far, so only a measurement that laid it out in the pane's own layout
		// would leave lines there.
		const auto offScreen = pane.document()->findBlockByNumber(201);
		expect(offScreen.isValid() && offScreen.layout()->lineCount() == 0,
			   "measurement leaves an off-screen block's own layout unshaped");
		const auto measuredRange = pane.horizontalScrollBar()->maximum();
		const auto measuredWidth = pane.document()->documentLayout()->documentSize().width();

		const auto native = native_boundaries(pane, lines);
		expect(native.size() == measured.size(), "native reference covers every line");
		for (std::size_t index = 0U; index < native.size(); ++index) {
			if (native[index] != measured[index]) {
				std::cerr << "boundary " << index << ": measured " << measured[index] << ", native " << native[index] << '\n';
				expect(false, "transient no-wrap boundaries equal the pane's native boundaries exactly");
			}
		}
		expect(measured[1] != measured[2] || measured[3] != measured[4] || measured[6] - measured[5] != measured[7] - measured[6],
			   "the fixture's lines really differ in height");
		QApplication::processEvents();
		expect(pane.document()->documentLayout()->documentSize().width() == measuredWidth
				   && pane.horizontalScrollBar()->maximum() == measuredRange,
			   "the pane's horizontal range already covered its widest line");
	}

	/// \brief Every boundary equals the pane's native one, or names the first
	/// that does not.
	void expect_native_boundaries(const std::vector<qreal>& aMeasured, const QPlainTextEdit& aPane, const std::uint32_t aLines, const std::string_view aMessage)
	{
		const auto native = native_boundaries(aPane, aLines);
		for (std::size_t index = 0U; index < std::max(native.size(), aMeasured.size()); ++index) {
			if (index >= native.size() || index >= aMeasured.size() || native[index] != aMeasured[index]) {
				std::cerr << "boundary " << index << " differs from the native one\n";
				expect(false, aMessage);
			}
		}
	}

	/// [[BPR-1123]]. A transient layout keeps nothing, so every measurement of
	/// an unwrapped pane would shape every block again ([[BPR-1032]]). With the
	/// pane's recorded block heights, a measurement after a viewport change
	/// reuses every block, and still lands on the pane's native boundaries and
	/// its horizontal range, even after a width change emptied the pane's own
	/// layouts. A font change, a block's changed formats, a text edit and an
	/// explicit clear each measure again.
	auto test_recorded_block_heights() -> void
	{
		auto pane = QPlainTextEdit{ };
		pane.resize(260, 160);
		pane.setLineWrapMode(QPlainTextEdit::NoWrap);
		// A scroll bar that appears mid-measurement would invalidate it.
		pane.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		pane.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		QString        source;
		constexpr auto lines = 120U;
		for (auto line = 0U; line < lines; ++line) {
			switch (line % 5U) {
			case 0U: source += QStringLiteral("fallback 日本語 αβγ 😀 line\n"); break;
			case 1U: source += QStringLiteral("a wide word in a larger font\n"); break;
			// Widest in the middle, far below the viewport.
			case 2U: source += QStringLiteral("x").repeated(static_cast<int>(20U + std::min(line, lines - line))) + QChar{ '\n' }; break;
			default: source += QStringLiteral("plain ascii source line\n"); break;
			}
		}
		source.chop(1);
		pane.setPlainText(source);
		pane.show();
		QApplication::processEvents();
		set_larger_word_formats(*pane.document());
		QApplication::processEvents();

		auto       heights = qt::pane_block_heights{ };
		const auto measure = [&pane, &heights] {
			QApplication::processEvents();
			qt::plain_text_pane_measurement measurement{ pane, lines, heights };
			auto                            progress = qt::pane_measurement_progress::pending;
			while (progress == qt::pane_measurement_progress::pending) {
				progress = measurement.advance(32U);
			}
			expect(progress == qt::pane_measurement_progress::complete && measurement.result() != nullptr,
				   "a measurement with recorded block heights completes");
			return std::pair{ measurement.result()->Boundaries, measurement.reused_lines() };
		};

		const auto [first, firstReused] = measure();
		expect(firstReused == 0U, "the first measurement has nothing recorded to reuse");

		pane.resize(260, 220);
		const auto [taller, tallerReused] = measure();
		expect(taller == first, "a taller viewport leaves every boundary where it was");
		expect(tallerReused == lines, "a viewport-only change reuses every recorded block");

		// A width change makes the pane lay its document out again, which
		// empties every block's own layout and resets its widest line.
		pane.resize(340, 220);
		const auto [wider, widerReused] = measure();
		expect(widerReused == lines && wider == first, "a width change reuses every recorded block, and every boundary stays");
		QApplication::processEvents();
		const auto measuredWidth = pane.document()->documentLayout()->documentSize().width();
		expect_native_boundaries(first, pane, lines, "the reused boundaries are the pane's native ones");
		QApplication::processEvents();
		expect(pane.document()->documentLayout()->documentSize().width() == measuredWidth,
			   "a reused width still lays the widest line out, so the pane's horizontal range already covered it");

		auto font = pane.font();
		font.setPointSizeF(font.pointSizeF() + 3.0);
		pane.setFont(font);
		const auto [larger, largerReused] = measure();
		expect_native_boundaries(larger, pane, lines, "after a font change the boundaries are the pane's native ones");
		expect(largerReused == 0U && larger != first, "a font change measures every block again");

		// A highlighter's way to change a block's formats. The revision does
		// not move, so only the block's own formats say its height may have.
		const auto restyled = pane.document()->findBlockByNumber(3);
		auto       format   = QTextCharFormat{ };
		format.setFontPointSize(30);
		restyled.layout()->setFormats({ QTextLayout::FormatRange{ .start = 0, .length = 5, .format = format } });
		pane.document()->markContentsDirty(restyled.position(), restyled.length());
		const auto [reformatted, reformattedReused] = measure();
		expect_native_boundaries(reformatted, pane, lines, "a block whose formats changed is measured again");
		expect(reformattedReused == lines - 1U && reformatted != larger, "only the block whose formats changed is measured again");

		auto edit = QTextCursor{ pane.document()->findBlockByNumber(7) };
		edit.insertText(QStringLiteral("edited "));
		const auto [edited, editedReused] = measure();
		expect_native_boundaries(edited, pane, lines, "after an edit the boundaries are the pane's native ones");
		expect(editedReused == 0U, "a text edit measures every block again");

		heights.clear();
		const auto [cleared, clearedReused] = measure();
		expect(clearedReused == 0U && cleared == edited, "a cleared record measures every block again, to the same boundaries");
	}

	/// Measures a pane to completion with its recorded block heights, and
	/// says how many lines took a recorded height.
	[[nodiscard]] auto measure_recorded(const QPlainTextEdit& aPane, const std::uint32_t aLines, qt::pane_block_heights& aHeights)
		-> std::pair<std::vector<qreal>, std::uint32_t>
	{
		QApplication::processEvents();
		qt::plain_text_pane_measurement measurement{ aPane, aLines, aHeights };
		auto                            progress = qt::pane_measurement_progress::pending;
		while (progress == qt::pane_measurement_progress::pending) {
			progress = measurement.advance(32U);
		}
		expect(progress == qt::pane_measurement_progress::complete && measurement.result() != nullptr,
			   "a measurement with recorded block heights completes");
		return { measurement.result()->Boundaries, measurement.reused_lines() };
	}

	/// Gives one block new additional formats the way the review panes
	/// highlight: the block's own layout, then its contents marked dirty.
	void restyle(QTextDocument& aDocument, const int aBlock, const QList<QTextLayout::FormatRange>& aFormats)
	{
		const auto block = aDocument.findBlockByNumber(aBlock);
		block.layout()->setFormats(aFormats);
		aDocument.markContentsDirty(block.position(), block.length());
	}

	[[nodiscard]] auto coloured(const int aStart, const int aLength, const QColor& aColour) -> QTextLayout::FormatRange
	{
		auto range = QTextLayout::FormatRange{ .start = aStart, .length = aLength, .format = QTextCharFormat{ } };
		range.format.setForeground(aColour);
		return range;
	}

	/// An unwrapped pane with its scroll bars always shown, so none appears
	/// in the middle of a measurement.
	void prepare_unwrapped(QPlainTextEdit& aPane, const QString& aSource)
	{
		aPane.resize(260, 160);
		aPane.setLineWrapMode(QPlainTextEdit::NoWrap);
		aPane.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		aPane.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		aPane.setPlainText(aSource);
		aPane.show();
		QApplication::processEvents();
	}

	/// [[BPR-1142]]. The record keys a block's additional formats by the
	/// properties its layout reads. Highlighting that changes a block's
	/// layout without touching its text measures that block again, although
	/// neither the document's revision nor the block's own revision or user
	/// state moves: that is why the key is a hash of the formats and not a
	/// generation counter. Recolouring every block, or giving a block an
	/// equal list of its own, reuses them all. A block recorded as not
	/// reusable (here, one measured while hidden) is measured again.
	auto test_format_key() -> void
	{
		constexpr auto lines  = 60U;
		auto           source = QString{ };
		for (auto line = 0U; line < lines; ++line) {
			source += QStringLiteral("auto value = compute(line, %1);\n").arg(line);
		}
		source.chop(1);
		auto pane = QPlainTextEdit{ };
		prepare_unwrapped(pane, source);
		auto& document = *pane.document();
		// A keyword and a call on every line, as the review panes colour syntax.
		const auto colourAll = [&document](const QColor& aColour) {
			for (auto block = 0; block < document.blockCount(); ++block) {
				restyle(document, block, { coloured(0, 4, aColour), coloured(13, 7, aColour) });
			}
		};
		const auto blue = QColor{ 0x20, 0x60, 0xC0 };
		const auto red  = QColor{ 0xC0, 0x30, 0x20 };
		colourAll(blue);

		auto heights                    = qt::pane_block_heights{ };
		const auto [first, firstReused] = measure_recorded(pane, lines, heights);
		expect(firstReused == 0U, "the first measurement has nothing recorded to reuse");
		expect_native_boundaries(first, pane, lines, "the highlighted pane's boundaries are its native ones");

		colourAll(red);
		const auto [recoloured, recolouredReused] = measure_recorded(pane, lines, heights);
		expect(recolouredReused == lines && recoloured == first, "recolouring every block reuses every recorded block, because colour is not laid out");

		colourAll(red);
		const auto [rebuilt, rebuiltReused] = measure_recorded(pane, lines, heights);
		expect(rebuiltReused == lines && rebuilt == first, "an equal format list of the block's own reuses every recorded block");

		// The review panes' own emphasis: the keyword turns bold, and the
		// text stays as it was.
		const auto emphasised    = document.findBlockByNumber(5);
		const auto revision      = document.revision();
		const auto blockRevision = emphasised.revision();
		const auto userState     = emphasised.userState();
		auto       bold          = coloured(0, 4, red);
		bold.format.setFontWeight(QFont::Bold);
		restyle(document, 5, { bold, coloured(13, 7, red) });
		expect(document.revision() == revision && emphasised.revision() == blockRevision && emphasised.userState() == userState,
			   "a format-only change moves neither the document's revision nor the block's revision or user state");
		const auto [bolded, boldedReused] = measure_recorded(pane, lines, heights);
		expect(boldedReused == lines - 1U, "a block whose keyword turned bold is measured again, and only that block");
		expect_native_boundaries(bolded, pane, lines, "after the keyword turned bold the boundaries are the pane's native ones");

		// Far below the viewport, a larger size on the same text changes the
		// block's height.
		auto larger = coloured(0, 4, red);
		larger.format.setFontPointSize(26);
		restyle(document, 40, { larger, coloured(13, 7, red) });
		const auto [enlarged, enlargedReused] = measure_recorded(pane, lines, heights);
		expect_native_boundaries(enlarged, pane, lines, "a block whose highlighting changed its height is measured again, to its native height");
		expect(enlargedReused == lines - 1U && enlarged != bolded, "a block whose highlighting changed its height is measured again, and only that block");

		larger.start = 13;
		restyle(document, 40, { coloured(0, 4, red), larger });
		const auto [moved, movedReused] = measure_recorded(pane, lines, heights);
		expect(movedReused == lines - 1U, "the same format over a different range measures the block again");
		expect_native_boundaries(moved, pane, lines, "after a range moved the boundaries are the pane's native ones");

		auto hidden = document.findBlockByNumber(50);
		hidden.setVisible(false);
		document.markContentsDirty(hidden.position(), hidden.length());
		const auto [whileHidden, whileHiddenReused] = measure_recorded(pane, lines, heights);
		expect(whileHiddenReused == lines - 1U, "a hidden block is measured natively, and every other block is reused");
		expect_native_boundaries(whileHidden, pane, lines, "with a block hidden the boundaries are the pane's native ones");
		hidden.setVisible(true);
		document.markContentsDirty(hidden.position(), hidden.length());
		const auto [shown, shownReused] = measure_recorded(pane, lines, heights);
		expect(shownReused == lines - 1U, "a block recorded as not reusable is measured again");
		expect_native_boundaries(shown, pane, lines, "the block recorded while hidden gets its own height back");
		expect(shown == moved, "showing the block again restores every boundary");
		const auto [again, againReused] = measure_recorded(pane, lines, heights);
		expect(againReused == lines && again == shown, "once measured again, the block is reused");
	}

	/// Absolute letter spacing, in 1/64 px, for one letter of a line.
	[[nodiscard]] auto letter_spaced(const int aStart, const int aUnits) -> QTextLayout::FormatRange
	{
		auto range = QTextLayout::FormatRange{ .start = aStart, .length = 1, .format = QTextCharFormat{ } };
		range.format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
		range.format.setFontLetterSpacing(static_cast<qreal>(aUnits) / 64.0);
		return range;
	}

	/// [[BPR-1142]]. One run of `test_recorded_block_precision`, with the two
	/// widest lines' natural widths moved by `aShift` 1/64 px.
	void check_recorded_block_precision(const int aShift)
	{
		constexpr auto lines   = 96U;
		constexpr auto widest  = 52;
		constexpr auto matched = 46;
		auto           source  = QString{ };
		for (auto line = 0; line < static_cast<int>(lines); ++line) {
			if (line == widest || line == matched) {
				source += QStringLiteral("the widest line in the pane, one of two alike, and long enough to run well past the right edge of its viewport\n");
				continue;
			}
			switch (line % 6) {
			case 0:  source += QStringLiteral("fallback 日本語 αβγ 😀 line\n"); break;
			case 1:  source += QStringLiteral("\tindented\twith tabs\n"); break;
			case 2:  source += QStringLiteral("two visual lines") + QChar{ QChar::LineSeparator } + QStringLiteral("in one block\n"); break;
			case 3:  source += QStringLiteral("spaced letters and spaced words\n"); break;
			case 4:  source += QStringLiteral("superscript and a baseline offset\n"); break;
			default: source += QStringLiteral("plain ascii source line\n"); break;
			}
		}
		source.chop(1);
		auto pane = QPlainTextEdit{ };
		auto font = pane.font();
		font.setPointSizeF(10.3);
		pane.setFont(font);
		prepare_unwrapped(pane, source);
		auto& document = *pane.document();

		auto sized = QTextLayout::FormatRange{ .start = 0, .length = 3, .format = QTextCharFormat{ } };
		sized.format.setFontPointSize(11.7);
		sized.format.setFontHintingPreference(QFont::PreferNoHinting);
		auto spaced = QTextLayout::FormatRange{ .start = 0, .length = 20, .format = QTextCharFormat{ } };
		spaced.format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
		spaced.format.setFontLetterSpacing(0.37);
		spaced.format.setFontWordSpacing(1.13);
		auto superscript = QTextLayout::FormatRange{ .start = 0, .length = 11, .format = QTextCharFormat{ } };
		superscript.format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
		auto offset = QTextLayout::FormatRange{ .start = 18, .length = 8, .format = QTextCharFormat{ } };
		offset.format.setBaselineOffset(13.0);
		offset.format.setFontPointSize(8.9);
		for (auto block = 0; block < document.blockCount(); ++block) {
			// The two alike are split into the same runs. They differ only in
			// their first letter's spacing, so the widest is 1/64 px wider.
			if (block == widest || block == matched) {
				restyle(document, block, { letter_spaced(0, block == widest ? 1 : 0), letter_spaced(5, aShift) });
				continue;
			}
			switch (block % 6) {
			case 0:  restyle(document, block, { sized, QTextLayout::FormatRange{ .start = 9, .length = 3, .format = sized.format } }); break;
			case 2:  restyle(document, block, { sized, QTextLayout::FormatRange{ .start = 4, .length = 6, .format = offset.format } }); break;
			case 3:  restyle(document, block, { spaced }); break;
			case 4:  restyle(document, block, { superscript, offset }); break;
			default: break;
			}
		}

		const auto expected = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expected.isEmpty()) {
			expect(close(pane.devicePixelRatioF(), expected.toDouble(), 0.05), "the precision fixture runs at the selected Qt scale");
		}

		auto heights                    = qt::pane_block_heights{ };
		const auto [first, firstReused] = measure_recorded(pane, lines, heights);
		expect(firstReused == 0U, "the first measurement has nothing recorded to reuse");

		pane.resize(260, 220);
		const auto [taller, tallerReused] = measure_recorded(pane, lines, heights);
		expect(tallerReused == lines, "every recorded height and width comes back from 1/64 px layout units, so every block is reused");
		expect(taller == first, "the reused boundaries are the measured ones to the last bit");

		// A width change makes the pane lay its document out again, which
		// empties every block's own layout and resets its widest line. The
		// pane then lays out its first and last pages again, but the two
		// alike lie between them, so the widest line is found again from the
		// recorded widths.
		pane.resize(340, 220);
		QApplication::processEvents();
		expect(document.findBlockByNumber(matched).layout()->lineCount() == 0 && document.findBlockByNumber(widest).layout()->lineCount() == 0,
			   "the width change emptied the two alike's own layouts");
		const auto [widened, widenedReused] = measure_recorded(pane, lines, heights);
		expect(widenedReused == lines && widened == first, "after a width change every block is reused, and every boundary stays");
		QApplication::processEvents();
		const auto measuredWidth = document.documentLayout()->documentSize().width();
		expect_native_boundaries(first, pane, lines, "the reused boundaries are the pane's native ones to the last bit");
		QApplication::processEvents();
		expect(document.documentLayout()->documentSize().width() == measuredWidth,
			   "a recorded width 1/64 px wider than the widest before it still has its line laid out natively");

		// Both are laid out natively by now.
		const auto widestWidth  = document.findBlockByNumber(widest).layout()->lineAt(0).naturalTextWidth();
		const auto matchedWidth = document.findBlockByNumber(matched).layout()->lineAt(0).naturalTextWidth();
		expect(widestWidth - matchedWidth == 1.0 / 64.0, "the widest line is 1/64 px wider than the line alike");
		expect(matchedWidth > static_cast<qreal>(pane.viewport()->width()),
			   "the two alike run past the viewport, so only the widest-line rule has them laid out");
		std::cout << "precision fixture at scale " << pane.devicePixelRatioF() << ", shift " << aShift << "/64: the widest line's natural width is "
				  << widestWidth * 64.0 << "/64 px\n";
	}

	/// [[BPR-1142]]. The record keeps heights and widths in Qt's 1/64 px
	/// layout units. A block's height is whole pixels, because Qt rounds each
	/// line's height up, but a natural width need not be. At the scale the
	/// row runs at (100, 125, 150 or 200%), with fractional font sizes, letter
	/// and word spacing, superscript, a baseline offset and blocks of two
	/// visual lines, every value a transient layout measured comes back
	/// exactly: the next measurement reuses every block and lands on the
	/// pane's native boundaries to the last bit.
	///
	/// The widest line is 1/64 px wider than the line alike before it, and a
	/// reused width still has it laid out natively, so the pane's horizontal
	/// range covers it. The pair's widths are moved through four consecutive
	/// 1/64 px steps, so a record that rounded widths to any coarser grid would
	/// put the pair on one value in at least one of the four and fail.
	auto test_recorded_block_precision() -> void
	{
		for (auto shift = 0; shift < 4; ++shift) {
			check_recorded_block_precision(shift);
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
	test_bounded_native_measurement();
	test_transient_no_wrap_measurement(true);
	test_transient_no_wrap_measurement(false);
	test_recorded_block_heights();
	test_format_key();
	test_recorded_block_precision();

	std::cout << "measured-alignment-axis-tests:pass\n";
	return EXIT_SUCCESS;
}
