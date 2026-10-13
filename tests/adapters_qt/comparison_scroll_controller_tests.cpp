#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/comparison_scroll_controller.hpp"

#include <QAbstractSlider>
#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointF>
#include <QPointingDevice>
#include <QScrollBar>
#include <QStyleHints>
#include <QThread>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
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

	[[nodiscard]] auto comparison_key(
		const core::diff_options& aOptions,
		const std::uint8_t        aIdentitySeed = 0U)
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
			.LeftBlob  = blob(static_cast<std::uint8_t>(21U + aIdentitySeed)),
			.RightBlob = blob(static_cast<std::uint8_t>(51U + aIdentitySeed)),
			.File      = opaque<core::file_identity>(static_cast<std::uint8_t>(31U + aIdentitySeed)),
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
		const std::string_view aRight,
		const std::uint8_t     aIdentitySeed = 0U) -> core::computed_file_diff
	{
		auto       left    = bytes(aLeft);
		auto       right   = bytes(aRight);
		auto       options = core::diff_options{ };
		const auto outcome = core::compute_file_diff({
			.Key        = comparison_key(options, aIdentitySeed),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(outcome.Reason == core::diff_reason_code::none,
			   "fixture diff computes");
		expect(outcome.Result.has_value(), "fixture diff publishes");
		return *outcome.Result;
	}

	[[nodiscard]] auto measurements(
		std::vector<qreal> aBoundaries,
		const qreal        aViewport = 12.0) -> qt::pane_line_measurements
	{
		const auto documentExtent = aBoundaries.empty()
			? 0.0
			: aBoundaries.back();
		return {
			.Boundaries       = std::move(aBoundaries),
			.DocumentExtent   = documentExtent,
			.ViewportExtent   = aViewport,
			.DevicePixelRatio = 1.0,
		};
	}

	[[nodiscard]] auto build_axis(
		const std::string_view aLeft,
		const std::string_view aRight,
		std::vector<qreal>     aLeftBoundaries,
		std::vector<qreal>     aRightBoundaries,
		const qreal            aLeftViewport  = 12.0,
		const qreal            aRightViewport = 12.0,
		const std::uint8_t     aIdentitySeed  = 0U)
		-> qt::measured_alignment_axis
	{
		const auto diff  = computed(aLeft, aRight, aIdentitySeed);
		auto       built = qt::build_measured_alignment_axis(
			diff.Alignment,
			measurements(std::move(aLeftBoundaries), aLeftViewport),
			measurements(std::move(aRightBoundaries), aRightViewport));
		expect(built.valid(), "controller fixture axis builds");
		return std::move(*built.Axis);
	}

	[[nodiscard]] auto insertion_axis() -> qt::measured_alignment_axis
	{
		return build_axis(
			"alpha\ncharlie\ndelta\n",
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n",
			{ 0.0, 10.0, 20.0, 30.0 },
			{ 0.0, 10.0, 25.0, 40.0, 50.0, 60.0 });
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
		expect(found != aAxis.segments().end(), "expected segment exists");
		return *found;
	}

	struct controller_harness
	{
		QWidget                          Left;
		QWidget                          Right;
		QWidget                          LinkMap;
		QScrollBar                       LeftHorizontal{ Qt::Horizontal };
		QScrollBar                       RightHorizontal{ Qt::Horizontal };
		QScrollBar                       Shared{ Qt::Vertical };
		qt::comparison_scroll_controller Controller;

		explicit controller_harness(
			qt::comparison_scroll_configuration aConfiguration = { })
			: Controller{ Shared, aConfiguration }
		{
			LeftHorizontal.setRange(0, 500);
			RightHorizontal.setRange(0, 500);
			LeftHorizontal.setValue(200);
			RightHorizontal.setValue(200);
			Controller.bind_input_surface(
				qt::comparison_scroll_receiver::left_pane,
				Left,
				&LeftHorizontal);
			Controller.bind_input_surface(
				qt::comparison_scroll_receiver::right_pane,
				Right,
				&RightHorizontal);
			Controller.bind_input_surface(
				qt::comparison_scroll_receiver::link_map, LinkMap);
		}

		[[nodiscard]] auto surface(
			const qt::comparison_scroll_receiver aReceiver) -> QWidget&
		{
			switch (aReceiver) {
			case qt::comparison_scroll_receiver::left_pane:
				return Left;
			case qt::comparison_scroll_receiver::right_pane:
				return Right;
			case qt::comparison_scroll_receiver::link_map:
				return LinkMap;
			}
			return Left;
		}
	};

	[[nodiscard]] auto send_wheel(
		QWidget&                    aSurface,
		const QPoint&               aPixel,
		const QPoint&               aAngle     = { },
		const Qt::KeyboardModifiers aModifiers = Qt::NoModifier,
		const Qt::ScrollPhase       aPhase     = Qt::NoScrollPhase,
		const bool                  aInverted  = false) -> bool
	{
		auto event = QWheelEvent{
			QPointF{ 4.0, 4.0 },
			QPointF{ 4.0, 4.0 },
			aPixel,
			aAngle,
			Qt::NoButton,
			aModifiers,
			aPhase,
			aInverted,
		};
		event.ignore();
		QCoreApplication::sendEvent(&aSurface, &event);
		return event.isAccepted();
	}

	[[nodiscard]] auto send_key(
		QWidget&                    aSurface,
		const Qt::Key               aKey,
		const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) -> bool
	{
		auto event = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		event.ignore();
		QCoreApplication::sendEvent(&aSurface, &event);
		return event.isAccepted();
	}

	auto test_receiver_equivalence_and_handoff() -> void
	{
		constexpr auto receivers = std::array{
			qt::comparison_scroll_receiver::left_pane,
			qt::comparison_scroll_receiver::right_pane,
			qt::comparison_scroll_receiver::link_map,
		};
		std::array<std::vector<qreal>, receivers.size()> traces;
		for (std::size_t receiver = 0U; receiver < receivers.size(); ++receiver) {
			auto harness = controller_harness{ };
			harness.Controller.set_axis(insertion_axis());
			harness.Controller.set_position(8.0);
			for (const auto delta : { -3, -5, 2, -7, 4 }) {
				expect(send_wheel(
						   harness.surface(receivers[receiver]),
						   QPoint{ 0, delta }),
					   "vertical wheel event is consumed");
				traces[receiver].push_back(harness.Controller.position());
			}
		}
		expect(traces[0] == traces[1] && traces[1] == traces[2],
			   "left, right, and link-map input produce identical traces");

		auto handoff = controller_harness{ };
		handoff.Controller.set_axis(insertion_axis());
		handoff.Controller.set_position(8.0);
		expect(send_wheel(handoff.Left, QPoint{ 0, -3 }),
			   "left pane begins one shared gesture trace");
		const auto afterLeft = handoff.Controller.position();
		expect(send_wheel(handoff.Right, QPoint{ 0, -5 }),
			   "right pane continues one shared gesture trace");
		expect(close(handoff.Controller.position(), afterLeft + 5.0),
			   "input receiver handoff does not reset or jump authority");
	}

	auto test_surface_role_rebinding() -> void
	{
		auto combinedSurface = QWidget{ };
		auto newViewport     = QWidget{ };
		auto horizontal      = QScrollBar{ Qt::Horizontal };
		auto shared          = QScrollBar{ Qt::Vertical };
		auto controller      = qt::comparison_scroll_controller{ shared };
		controller.bind_key_surface(
			qt::comparison_scroll_receiver::left_pane, combinedSurface);
		controller.bind_input_surface(
			qt::comparison_scroll_receiver::left_pane,
			combinedSurface,
			&horizontal);
		controller.bind_input_surface(
			qt::comparison_scroll_receiver::left_pane,
			newViewport,
			&horizontal);
		controller.set_axis(insertion_axis());
		expect(send_key(combinedSurface, Qt::Key_PageDown)
				   && controller.position() > 0.0,
			   "key-first binding survives wheel-surface replacement");
		controller.home();
		const auto home = controller.minimum_position();
		expect(!send_wheel(combinedSurface, QPoint{ 0, -2 })
				   && close(controller.position(), home),
			   "replaced wheel surface no longer routes vertical input");
		expect(send_wheel(newViewport, QPoint{ 0, -2 })
				   && close(controller.position(), home + 2.0),
			   "replacement wheel surface routes vertical input");
		const auto beforeKey = controller.position();
		expect(!send_key(newViewport, Qt::Key_PageDown)
				   && close(controller.position(), beforeKey),
			   "wheel-only replacement does not become key authority");
	}

	auto test_pinning_proportional_motion_and_no_dead_travel() -> void
	{
		auto       insertion = insertion_axis();
		const auto inserted  = find_segment(
			insertion, core::alignment_kind::right_only);
		auto harness = controller_harness{ };
		harness.Controller.set_axis(std::move(insertion));
		harness.Controller.set_position(std::lerp(
			inserted.ComparisonBegin, inserted.ComparisonEnd, 0.25));
		const auto first = *harness.Controller.current_frame();
		harness.Controller.set_position(std::lerp(
			inserted.ComparisonBegin, inserted.ComparisonEnd, 0.75));
		const auto second = *harness.Controller.current_frame();
		expect(close(first.Left.ProjectedDocumentY,
					 second.Left.ProjectedDocumentY)
				   && second.Right.ProjectedDocumentY
					   > first.Right.ProjectedDocumentY,
			   "right insertion pins left and advances right");
		expect(close(second.Left.ViewportY, 0.43 * 12.0)
				   && close(second.Right.ViewportY, 0.43 * 12.0),
			   "unclamped pane origins place both projections on one rail");

		auto deletion = build_axis(
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n",
			"alpha\ncharlie\ndelta\n",
			{ 0.0, 10.0, 25.0, 40.0, 50.0, 60.0 },
			{ 0.0, 10.0, 20.0, 30.0 });
		const auto deleted = find_segment(
			deletion, core::alignment_kind::left_only);
		harness.Controller.set_axis(std::move(deletion));
		harness.Controller.set_position(std::lerp(
			deleted.ComparisonBegin, deleted.ComparisonEnd, 0.25));
		const auto deletedFirst = *harness.Controller.current_frame();
		harness.Controller.set_position(std::lerp(
			deleted.ComparisonBegin, deleted.ComparisonEnd, 0.75));
		const auto deletedSecond = *harness.Controller.current_frame();
		expect(deletedSecond.Left.ProjectedDocumentY
					   > deletedFirst.Left.ProjectedDocumentY
				   && close(deletedFirst.Right.ProjectedDocumentY,
							deletedSecond.Right.ProjectedDocumentY),
			   "left deletion advances left and pins right");

		auto fourToSeven = build_axis(
			"left-a\nleft-b\nleft-c\nleft-d\n",
			"right-a\nright-b\nright-c\nright-d\nright-e\nright-f\nright-g\n",
			{ 0.0, 10.0, 20.0, 30.0, 40.0 },
			{ 0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0 });
		harness.Controller.set_axis(std::move(fourToSeven));
		harness.Controller.set_position(35.0);
		expect(close(
				   harness.Controller.current_frame()->Left.ProjectedDocumentY,
				   20.0)
				   && close(
					   harness.Controller.current_frame()->Right.ProjectedDocumentY,
					   35.0),
			   "4:7 replacement motion preserves proportional progress");

		harness.Controller.align_pane_origin(core::diff_side::right);
		const auto rightAligned = *harness.Controller.current_frame();
		expect(close(rightAligned.Right.DocumentOriginY, 30.0)
				   && !close(std::fmod(rightAligned.Left.DocumentOriginY, 10.0), 0.0)
				   && close(rightAligned.Left.ProjectedDocumentY, rightAligned.Right.ProjectedDocumentY * 4.0 / 7.0),
			   "keyboard alignment belongs to the receiving side of an unequal replacement, with normal fractional projection on the other side");
		harness.Controller.align_pane_origin(core::diff_side::left);
		const auto leftAligned = *harness.Controller.current_frame();
		expect(close(std::fmod(leftAligned.Left.DocumentOriginY, 10.0), 0.0)
				   && close(leftAligned.Left.ProjectedDocumentY, leftAligned.Right.ProjectedDocumentY * 4.0 / 7.0),
			   "left-side keyboard alignment also preserves the shared proportional authority");

		auto sevenToFour = build_axis(
			"left-a\nleft-b\nleft-c\nleft-d\nleft-e\nleft-f\nleft-g\n",
			"right-a\nright-b\nright-c\nright-d\n",
			{ 0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0 },
			{ 0.0, 10.0, 20.0, 30.0, 40.0 });
		harness.Controller.set_axis(std::move(sevenToFour));
		harness.Controller.set_position(35.0);
		expect(close(
				   harness.Controller.current_frame()->Left.ProjectedDocumentY,
				   35.0)
				   && close(
					   harness.Controller.current_frame()->Right.ProjectedDocumentY,
					   20.0),
			   "7:4 replacement motion preserves proportional progress");

		// [[BPR-1291]]. Dead travel is a step that moves no pane's origin,
		// not merely one that moves no projection: the rail can cross the
		// comparison while both panes stay clamped, which is the bar moving
		// while nothing scrolls.
		harness.Controller.set_axis(insertion_axis());
		harness.Controller.home();
		auto       previous    = *harness.Controller.current_frame();
		const auto start       = harness.Controller.minimum_position();
		const auto sampleCount = static_cast<std::size_t>(
			std::floor((harness.Controller.maximum_position() - start) * 4.0));
		expect(sampleCount > 8U, "the live range has room to sample");
		for (std::size_t sample = 1U; sample <= sampleCount; ++sample) {
			const auto position = start + static_cast<qreal>(sample) / 4.0;
			harness.Controller.set_position(position);
			const auto current = *harness.Controller.current_frame();
			expect(current.Position > previous.Position,
				   "each positive comparison sample advances retained position");
			expect(current.Left.DocumentOriginY > previous.Left.DocumentOriginY
					   || current.Right.DocumentOriginY > previous.Right.DocumentOriginY,
				   "each positive comparison sample moves at least one pane");
			previous = current;
		}
	}

	auto test_horizontal_drift_and_reorientation() -> void
	{
		auto harness = controller_harness{ };
		harness.Controller.set_axis(insertion_axis());
		harness.Controller.set_position(25.25);
		const auto start = harness.Controller.position();
		harness.LeftHorizontal.setValue(harness.LeftHorizontal.maximum());
		for (auto event = 0; event < 20; ++event) {
			expect(send_wheel(harness.Left, QPoint{ event < 10 ? -20 : 20, event % 2 == 0 ? 1 : -1 }, { }, Qt::NoModifier,
							  event == 0 ? Qt::ScrollBegin : Qt::ScrollUpdate),
				   "horizontal wobble events are consumed");
			expect(close(harness.Controller.position(), start), "horizontal wobble never displaces the vertical authority, including at an edge and reversal");
		}
		expect(send_wheel(harness.Right, QPoint{ -20, -1 }, { }, Qt::NoModifier, Qt::ScrollMomentum)
				   && close(harness.Controller.position(), start),
			   "momentum and pane handoff retain drift suppression");
		expect(send_wheel(harness.Right, { }, { }, Qt::NoModifier, Qt::ScrollEnd)
				   && close(harness.Controller.position(), start),
			   "ending a horizontal gesture releases no discarded motion");
		expect(send_wheel(harness.Left, QPoint{ 0, -1 }, { }, Qt::NoModifier, Qt::ScrollBegin)
				   && close(harness.Controller.position(), start + 1.0),
			   "small vertical-only motion responds immediately in a new gesture");
		expect(send_wheel(harness.Left, QPoint{ -20, -1 }, { }, Qt::NoModifier, Qt::ScrollBegin), "reorientation course starts horizontally");
		expect(send_wheel(harness.Left, QPoint{ 0, -1 }, { }, Qt::NoModifier, Qt::ScrollUpdate)
				   && close(harness.Controller.position(), start + 1.0),
			   "one minor perturbation does not release horizontal intent");
		expect(send_wheel(harness.Left, QPoint{ 0, -1 }, { }, Qt::NoModifier, Qt::ScrollUpdate)
				   && close(harness.Controller.position(), start + 2.0),
			   "sustained small vertical input reorients without a lift or banked jump");
		const auto horizontal = harness.LeftHorizontal.value();
		expect(send_wheel(harness.Left, QPoint{ -8, -8 }, { }, Qt::NoModifier, Qt::ScrollUpdate)
				   && close(harness.Controller.position(), start + 10.0) && harness.LeftHorizontal.value() > horizontal,
			   "comparable diagonal motion moves both available axes");
		expect(send_wheel(harness.Left, QPoint{ -20, 1 }), "unphased gesture establishes horizontal intent");
		harness.Controller.reset_pixel_gesture();
		const auto resetStart = harness.Controller.position();
		expect(send_wheel(harness.Left, QPoint{ 0, -1 }) && close(harness.Controller.position(), resetStart + 1.0), "presentation reset discards stale suppression for unphased input");
		expect(send_wheel(harness.Left, QPoint{ -20, 1 }), "unphased idle course establishes horizontal intent");
		QThread::msleep(260);
		const auto idleStart = harness.Controller.position();
		expect(send_wheel(harness.Left, QPoint{ 0, -1 }) && close(harness.Controller.position(), idleStart + 1.0), "unphased input starts afresh after idle without depending on an absent end event");
		auto firstDevice     = QPointingDevice{ };
		auto secondDevice    = QPointingDevice{ };
		auto horizontalEvent = QWheelEvent{ { }, { }, QPoint{ -20, 1 }, { }, Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false, Qt::MouseEventNotSynthesized, &firstDevice };
		auto verticalEvent   = QWheelEvent{ { }, { }, QPoint{ 0, -1 }, { }, Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false, Qt::MouseEventNotSynthesized, &secondDevice };
		auto gesture         = qt::pixel_scroll_gesture{ };
		expect(gesture.consume(horizontalEvent).y() == 0 && gesture.consume(verticalEvent).y() == -1, "changing pointing devices discards stale horizontal suppression");
		expect(send_wheel(harness.Left, QPoint{ -20, 1 }), "replacement course establishes horizontal intent");
		harness.Controller.set_axis(build_axis("alpha\ncharlie\ndelta\n", "alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n", { 0.0, 10.0, 20.0, 30.0 }, { 0.0, 10.0, 25.0, 40.0, 50.0, 60.0 }, 12.0, 12.0, 9U));
		harness.Controller.set_position(20.0);
		expect(send_wheel(harness.Left, QPoint{ 0, -1 }) && close(harness.Controller.position(), 21.0), "a different comparison does not inherit suppression");
	}

	auto test_precision_wheel_phases_and_coalescing() -> void
	{
		auto harness = controller_harness{ };
		harness.Controller.set_axis(insertion_axis());
		harness.Controller.set_position(25.0);
		const auto beginning = harness.Controller.position();
		for (std::size_t cycle = 0U; cycle < 1'000U; ++cycle) {
			harness.Controller.scroll_by(0.1);
			harness.Controller.scroll_by(-0.1);
		}
		expect(close(harness.Controller.position(), beginning, 0.000'000'001),
			   "repeated forward and reverse subpixel input has no drift");
		expect(harness.Controller.statistics().ScrollbarInputs == 0U,
			   "frame projection never feeds integer scrollbar values back into q");

		harness.Controller.set_position(6.0);
		expect(send_wheel(
				   harness.Left, QPoint{ 0, -4 }, QPoint{ 0, -120 }),
			   "pixel wheel event is consumed");
		expect(close(harness.Controller.position(), 10.0),
			   "pixel delta takes precedence over angle fallback");
		harness.Controller.set_position(6.0);
		expect(send_wheel(harness.Left, { }, QPoint{ 0, -120 }),
			   "angle wheel fallback is consumed");
		const auto lines = std::max(
			1, QGuiApplication::styleHints()->wheelScrollLines());
		expect(close(
				   harness.Controller.position(),
				   std::min(
					   harness.Controller.maximum_position(),
					   6.0 + static_cast<qreal>(lines) * 16.0)),
			   "angle wheel fallback uses platform scroll-line policy");
		harness.Controller.set_position(10.0);
		expect(send_wheel(harness.Left, { }, QPoint{ 0, -1 }),
			   "fine angle wheel input is consumed");
		expect(close(
				   harness.Controller.position(),
				   10.0 + static_cast<qreal>(lines) * 16.0 / 120.0),
			   "fine angle wheel input retains a subpixel comparison position");

		auto natural  = controller_harness{ };
		auto inverted = controller_harness{ };
		natural.Controller.set_axis(insertion_axis());
		inverted.Controller.set_axis(insertion_axis());
		natural.Controller.set_position(10.0);
		inverted.Controller.set_position(10.0);
		expect(send_wheel(natural.Left, QPoint{ 0, -3 }, { },
						  Qt::NoModifier, Qt::ScrollUpdate, false)
				   && send_wheel(inverted.Left, QPoint{ 0, -3 }, { },
								 Qt::NoModifier, Qt::ScrollUpdate, true),
			   "natural and inverted delivered deltas are consumed");
		expect(close(natural.Controller.position(), inverted.Controller.position()),
			   "already-inverted Qt deltas are not inverted a second time");
		const auto beforePhase = natural.Controller.position();
		expect(send_wheel(natural.Left, { }, { }, Qt::NoModifier,
						  Qt::ScrollBegin)
				   && send_wheel(natural.Left, { }, { }, Qt::NoModifier,
								 Qt::ScrollEnd),
			   "zero-delta begin and end phases stay in one consumed gesture");
		expect(close(natural.Controller.position(), beforePhase),
			   "zero-delta gesture phases do not move position");

		auto repaint = controller_harness{ };
		repaint.Controller.set_axis(insertion_axis());
		auto deliveries = std::uint64_t{ 0U };
		repaint.Controller.set_repaint_sink([&deliveries]() { ++deliveries; });
		repaint.Controller.scroll_by(1.0);
		repaint.Controller.scroll_by(1.0);
		repaint.Controller.scroll_by(1.0);
		expect(deliveries == 0U, "repaint delivery is queued");
		QApplication::processEvents();
		expect(deliveries == 1U
				   && repaint.Controller.statistics().RepaintRequestsCoalesced == 2U,
			   "multiple immediate frames coalesce to one repaint delivery");
	}

	auto test_navigation_reveal_feedback_and_horizontal_isolation() -> void
	{
		auto       axis     = insertion_axis();
		const auto inserted = find_segment(
			axis, core::alignment_kind::right_only);
		auto harness = controller_harness{ };
		harness.Controller.set_axis(std::move(axis));
		harness.Controller.set_position(20.0);
		harness.Controller.page_down();
		expect(close(harness.Controller.position(), 30.8),
			   "Page Down advances by ninety percent of the smaller viewport");
		harness.Controller.page_up();
		expect(close(harness.Controller.position(), 20.0),
			   "Page Up reverses the page step without integer round-trip");
		// [[BPR-1291]]. The ends are where the panes stop, not where the
		// rail does: End puts each side's last line (20 and 50) at the top,
		// Home puts both first lines there, and neither end is clamped
		// travel, because the range holds no position at which no pane moves.
		const auto rail = 0.43 * 12.0;
		expect(close(harness.Controller.minimum_position(), rail)
				   && close(harness.Controller.maximum_position(), 50.0 + rail),
			   "the live range runs from the first pane leaving its top to the last line at the top");
		harness.Controller.end();
		expect(close(harness.Controller.position(), harness.Controller.maximum_position())
				   && close(harness.Controller.current_frame()
								->Left.DocumentOriginY,
							20.0)
				   && close(harness.Controller.current_frame()
								->Right.DocumentOriginY,
							50.0)
				   && !harness.Controller.current_frame()->Left.TrailingClamped
				   && !harness.Controller.current_frame()->Right.TrailingClamped,
			   "End puts each side's last line at the top");
		harness.Controller.home();
		expect(close(harness.Controller.position(), harness.Controller.minimum_position())
				   && close(harness.Controller.current_frame()
								->Left.DocumentOriginY,
							0.0)
				   && close(harness.Controller.current_frame()
								->Right.DocumentOriginY,
							0.0)
				   && !harness.Controller.current_frame()->Left.LeadingClamped
				   && !harness.Controller.current_frame()->Right.LeadingClamped,
			   "Home puts both first lines at the top");

		harness.Shared.setValue(25'000);
		expect(close(harness.Controller.position(), rail + 0.25 * 50.0),
			   "scrollbar drag projects into retained comparison position");
		const auto beforeSingle = harness.Controller.position();
		harness.Shared.triggerAction(QAbstractSlider::SliderSingleStepAdd);
		expect(harness.Controller.position() > beforeSingle,
			   "scrollbar single-step action advances comparison position");
		harness.Shared.triggerAction(QAbstractSlider::SliderSingleStepSub);
		const auto beforePage = harness.Controller.position();
		harness.Shared.triggerAction(QAbstractSlider::SliderPageStepAdd);
		expect(harness.Controller.position() > beforePage,
			   "scrollbar page action advances comparison position");
		harness.Shared.triggerAction(QAbstractSlider::SliderPageStepSub);
		harness.Shared.triggerAction(QAbstractSlider::SliderToMaximum);
		expect(close(harness.Controller.position(), harness.Controller.maximum_position()),
			   "scrollbar maximum action reaches final endpoint");
		harness.Shared.triggerAction(QAbstractSlider::SliderToMinimum);
		expect(close(harness.Controller.position(), harness.Controller.minimum_position()),
			   "scrollbar minimum action reaches first endpoint");
		expect(harness.Controller.reveal_segment(inserted.Id, 0.25),
			   "programmatic reveal resolves stable segment identity");
		expect(close(
				   harness.Controller.position(),
				   std::lerp(inserted.ComparisonBegin,
							 inserted.ComparisonEnd, 0.25)),
			   "programmatic reveal targets an exact segment fraction");
		expect(!harness.Controller.reveal_segment(
				   opaque<core::alignment_segment_id>(99U)),
			   "programmatic reveal rejects an unknown segment identity");

		auto nonCommensurate = build_axis(
			"left-a\nleft-b\nleft-c\nleft-d\n",
			"right-a\nright-b\nright-c\nright-d\nright-e\nright-f\nright-g\n",
			{ 0.0, 10.0, 20.0, 30.0, 40.0 },
			{ 0.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 61.0 });
		harness.Controller.set_axis(std::move(nonCommensurate));
		harness.Controller.set_position(20.0);
		harness.Shared.triggerAction(QAbstractSlider::SliderPageStepAdd);
		harness.Shared.triggerAction(QAbstractSlider::SliderPageStepSub);
		expect(close(harness.Controller.position(), 20.0, 0.000'000'001),
			   "opposing scrollbar page actions do not quantize retained q");
		harness.Shared.triggerAction(QAbstractSlider::SliderSingleStepAdd);
		harness.Shared.triggerAction(QAbstractSlider::SliderSingleStepSub);
		expect(close(harness.Controller.position(), 20.0, 0.000'000'001),
			   "opposing scrollbar single actions do not quantize retained q");

		harness.Controller.set_position(20.0);
		auto sinkCalls = std::uint64_t{ 0U };
		harness.Controller.set_frame_sink(
			[&harness, &sinkCalls](const qt::comparison_scroll_frame&) {
				++sinkCalls;
				harness.Controller.scroll_by(10.0);
			});
		expect(sinkCalls == 1U && close(harness.Controller.position(), 20.0)
				   && harness.Controller.statistics().ReentrantInputsSuppressed >= 1U,
			   "frame application suppresses recursive controller input");

		harness.Controller.set_frame_sink({ });
		harness.Controller.set_position(20.0);
		const auto sharedBefore = harness.Controller.position();
		const auto rightBefore  = harness.RightHorizontal.value();
		expect(send_wheel(harness.Left, QPoint{ -8, 0 }),
			   "horizontal pixel input is consumed by its pane");
		expect(harness.LeftHorizontal.value() == 208
				   && harness.RightHorizontal.value() == rightBefore
				   && close(harness.Controller.position(), sharedBefore),
			   "horizontal input changes only the receiving pane");
		expect(send_wheel(harness.Right, QPoint{ 0, -5 }, { },
						  Qt::ShiftModifier),
			   "shift-wheel horizontal input is consumed by its pane");
		expect(harness.RightHorizontal.value() == rightBefore + 5
				   && harness.LeftHorizontal.value() == 208
				   && close(harness.Controller.position(), sharedBefore),
			   "shift-wheel leaves vertical authority and the other pane unchanged");
		expect(!send_wheel(harness.LinkMap, QPoint{ -4, 0 })
				   && close(harness.Controller.position(), sharedBefore),
			   "divider horizontal input has no pane-local target");

		const auto lines = std::max(
			1, QGuiApplication::styleHints()->wheelScrollLines());
		const auto fractionalStep
			= static_cast<qreal>(lines) * 16.0 / 120.0;
		const auto eventsToWhole = static_cast<std::size_t>(
									   std::floor(1.0 / fractionalStep))
			+ 1U;
		harness.LeftHorizontal.setValue(harness.LeftHorizontal.minimum());
		for (std::size_t event = 0U; event < eventsToWhole; ++event) {
			expect(send_wheel(harness.Left, { }, QPoint{ -1, 0 }),
				   "inward horizontal subpixel input is consumed at minimum");
		}
		expect(harness.LeftHorizontal.value() > harness.LeftHorizontal.minimum(),
			   "inward horizontal residual survives minimum-boundary accumulation");
		harness.Controller.bind_input_surface(
			qt::comparison_scroll_receiver::left_pane,
			harness.Left,
			&harness.LeftHorizontal);
		harness.LeftHorizontal.setValue(harness.LeftHorizontal.maximum());
		for (std::size_t event = 0U; event < eventsToWhole; ++event) {
			expect(send_wheel(harness.Left, { }, QPoint{ 1, 0 }),
				   "inward horizontal subpixel input is consumed at maximum");
		}
		expect(harness.LeftHorizontal.value() < harness.LeftHorizontal.maximum(),
			   "inward horizontal residual survives maximum-boundary accumulation");

		auto boundaryConfiguration            = qt::comparison_scroll_configuration{ };
		boundaryConfiguration.AngleStepExtent = 12.0 / static_cast<qreal>(lines);
		auto boundary                         = controller_harness{ boundaryConfiguration };
		boundary.Controller.set_axis(insertion_axis());
		boundary.LeftHorizontal.setValue(boundary.LeftHorizontal.minimum());
		expect(send_wheel(boundary.Left, { }, QPoint{ -8, 0 })
				   && send_wheel(boundary.Left, { }, QPoint{ 1, 0 })
				   && send_wheel(boundary.Left, { }, QPoint{ -3, 0 }),
			   "mixed horizontal subpixel boundary sequence is consumed");
		expect(boundary.LeftHorizontal.value() == 1,
			   "net-inward residual survives a smaller outward boundary delta");
	}

	auto test_same_comparison_remeasurement_and_reentrant_sinks() -> void
	{
		auto       original          = insertion_axis();
		const auto originalInsertion = find_segment(
			original, core::alignment_kind::right_only);
		auto harness = controller_harness{ };
		harness.Controller.set_axis(std::move(original));
		harness.Controller.set_position(std::lerp(
			originalInsertion.ComparisonBegin,
			originalInsertion.ComparisonEnd,
			0.25));
		const auto retainedFrame      = harness.Controller.current_frame();
		const auto retainedStatistics = harness.Controller.statistics();
		harness.Controller.suspend_axis();
		expect(!harness.Shared.isEnabled(), "pending geometry disables the shared bar");
		expect(send_wheel(harness.Left, QPoint{ 0, -20 }), "pending geometry consumes native pane wheel input");
		harness.Controller.end();
		expect(!harness.Controller.reveal_segment(originalInsertion.Id), "pending geometry refuses coordinate reveal");
		expect(harness.Controller.current_frame() == retainedFrame
				   && harness.Controller.statistics().FramesApplied == retainedStatistics.FramesApplied,
			   "suspended input cannot apply a stale frame or lose the retained anchor");
		auto remeasured = build_axis(
			"alpha\ncharlie\ndelta\n",
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n",
			{ 0.0, 20.0, 40.0, 60.0 },
			{ 0.0, 20.0, 50.0, 80.0, 100.0, 120.0 },
			24.0,
			24.0);
		const auto remeasuredInsertion = find_segment(
			remeasured, core::alignment_kind::right_only);
		harness.Controller.set_axis(std::move(remeasured));
		expect(harness.Controller.current_frame()->Segment
					   == remeasuredInsertion.Id
				   && close(
					   harness.Controller.position(),
					   std::lerp(
						   remeasuredInsertion.ComparisonBegin,
						   remeasuredInsertion.ComparisonEnd,
						   0.25))
				   && close(
					   harness.Controller.current_frame()->Left.ViewportY,
					   0.43 * 24.0)
				   && close(
					   harness.Controller.current_frame()->Right.ViewportY,
					   0.43 * 24.0),
			   "same-comparison remeasurement retains segment fraction and recomputes origins");

		auto differentComparison = build_axis(
			"alpha\ncharlie\ndelta\n",
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n",
			{ 0.0, 20.0, 40.0, 60.0 },
			{ 0.0, 20.0, 50.0, 80.0, 100.0, 120.0 },
			24.0,
			24.0,
			7U);
		harness.Controller.set_axis(std::move(differentComparison));
		expect(close(harness.Controller.position(), harness.Controller.minimum_position()),
			   "different-comparison geometry begins at its legal start");
		harness.Controller.set_axis(insertion_axis());
		harness.Controller.set_position(35.0);

		auto reentrantAxis = build_axis(
			"alpha\ncharlie\ndelta\n",
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n",
			{ 0.0, 30.0, 60.0, 90.0 },
			{ 0.0, 30.0, 75.0, 120.0, 150.0, 180.0 },
			18.0,
			18.0);
		const auto reentrantInsertion = find_segment(
			reentrantAxis, core::alignment_kind::right_only);
		auto replaceOnce = true;
		harness.Controller.set_frame_sink(
			[&harness, &reentrantAxis, &replaceOnce](
				const qt::comparison_scroll_frame&) {
				if (replaceOnce) {
					replaceOnce = false;
					harness.Controller.set_axis(std::move(reentrantAxis));
				}
			});
		// Rail 0.43 * 18; the right side's last line is at 150.
		const auto reentrantRail = 0.43 * 18.0;
		expect(!replaceOnce
				   && harness.Controller.current_frame()->Segment
					   == reentrantInsertion.Id
				   && close(harness.Controller.position(), 105.0)
				   && close(harness.Controller.maximum_position(), 150.0 + reentrantRail)
				   && close(
					   harness.Controller.current_frame()->Left.ProjectedDocumentY,
					   30.0)
				   && close(
					   harness.Controller.current_frame()->Right.ProjectedDocumentY,
					   105.0)
				   && harness.Shared.value()
					   == static_cast<int>(std::llround((105.0 - reentrantRail) / 150.0 * 100'000.0)),
			   "reentrant axis replacement is serialized into one coherent frame");

		harness.Controller.set_frame_sink(
			[&harness](const qt::comparison_scroll_frame&) {
				harness.Controller.clear_axis();
			});
		expect(!harness.Controller.current_frame().has_value()
				   && close(harness.Controller.position(), 0.0)
				   && !harness.Shared.isEnabled(),
			   "reentrant clear is serialized without mixed axis and frame state");

		auto throwing = controller_harness{ };
		throwing.Controller.set_axis(insertion_axis());
		throwing.Controller.set_frame_sink(
			[](const qt::comparison_scroll_frame&) {
				throw std::runtime_error{ "synthetic frame sink failure" };
			});
		const auto before = throwing.Controller.position();
		throwing.Controller.scroll_by(1.0);
		expect(close(throwing.Controller.position(), before + 1.0)
				   && throwing.Controller.statistics().FrameSinkFailures == 2U,
			   "throwing frame sink is contained and releases the application guard");
		throwing.Controller.set_frame_sink({ });
		throwing.Controller.set_repaint_sink([]() {
			throw std::runtime_error{ "synthetic repaint sink failure" };
		});
		throwing.Controller.scroll_by(1.0);
		QApplication::processEvents();
		expect(throwing.Controller.statistics().RepaintSinkFailures == 1U
				   && throwing.Controller.statistics().RepaintDeliveries == 0U,
			   "throwing repaint sink is contained without a false delivery");

		auto frameBar = QScrollBar{ Qt::Vertical };
		auto frameController
			= std::make_unique<qt::comparison_scroll_controller>(frameBar);
		frameController->set_frame_sink(
			[&frameController](const qt::comparison_scroll_frame&) {
				frameController.reset();
			});
		frameController->set_axis(insertion_axis());
		expect(frameController == nullptr,
			   "frame sink may destroy the controller without post-callback access");

		auto actionBar = QScrollBar{ Qt::Vertical };
		auto actionController
			= std::make_unique<qt::comparison_scroll_controller>(actionBar);
		actionController->set_axis(insertion_axis());
		auto destroyOnAction = false;
		actionController->set_frame_sink(
			[&actionController, &destroyOnAction](
				const qt::comparison_scroll_frame&) {
				if (destroyOnAction) {
					actionController.reset();
				}
			});
		destroyOnAction = true;
		actionBar.triggerAction(QAbstractSlider::SliderPageStepAdd);
		expect(actionController == nullptr,
			   "scrollbar action tolerates synchronous frame-sink destruction");

		auto repaintBar = QScrollBar{ Qt::Vertical };
		auto repaintController
			= std::make_unique<qt::comparison_scroll_controller>(repaintBar);
		repaintController->set_axis(insertion_axis());
		repaintController->set_repaint_sink(
			[&repaintController]() { repaintController.reset(); });
		repaintController->scroll_by(1.0);
		QApplication::processEvents();
		expect(repaintController == nullptr,
			   "repaint sink may destroy the controller without post-callback access");
	}

	/// \brief [[BPR-1291]]. The owner's `renderer.cpp`: 7 lines against 14,
	/// both ending in a line break, so each pane shows an empty last line, in
	/// a viewport taller than either. Before, the bar travelled the whole
	/// comparison with both panes clamped at their top.
	auto test_short_files_scroll_past_their_end_and_the_bar_agrees() -> void
	{
		constexpr auto base = std::string_view{
			"#include \"renderer.h\"\n\nvoid Renderer::draw()\n{\n\tclear();\n\tpresent();\n}\n"
		};
		constexpr auto head = std::string_view{
			"#include \"renderer.h\"\n\nvoid Renderer::draw()\n{\n\tclear();\n\tdraw_scene();\n\tpresent();\n}\n\n"
			"void Renderer::resize(const int aWidth, const int aHeight)\n{\n\tWidth  = aWidth;\n\tHeight = aHeight;\n}\n"
		};
		const auto lines = [](const std::size_t aCount, const qreal aViewport = 400.0) {
			auto boundaries = std::vector<qreal>{ };
			for (std::size_t line = 0U; line <= aCount; ++line) {
				boundaries.push_back(10.0 * static_cast<qreal>(line));
			}
			auto pane                = measurements(std::move(boundaries), aViewport);
			pane.TrailingVisualBlock = true;
			return pane;
		};
		const auto diff  = computed(base, head, 3U);
		auto       built = qt::build_measured_alignment_axis(diff.Alignment, lines(7U), lines(14U));
		expect(built.valid(), "the short comparison's axis builds");
		expect(close(built.Axis->last_line_origin(core::diff_side::left), 70.0)
				   && close(built.Axis->last_line_origin(core::diff_side::right), 140.0),
			   "each side's last line is the empty line after its final line break");

		auto harness = controller_harness{ };
		harness.Controller.set_axis(std::move(*built.Axis));
		const auto rail = 0.43 * 400.0;
		expect(harness.Shared.isEnabled()
				   && close(harness.Controller.minimum_position(), rail)
				   && close(harness.Controller.maximum_position(), 140.0 + rail),
			   "a file shorter than the viewport has exactly its last line's origin to scroll");

		const auto origins = [&harness] {
			const auto& frame = *harness.Controller.current_frame();
			return std::pair{ frame.Left.DocumentOriginY, frame.Right.DocumentOriginY };
		};
		harness.Shared.triggerAction(QAbstractSlider::SliderToMinimum);
		expect(origins() == std::pair{ 0.0, 0.0 }, "the bar's minimum shows both files from their first line");
		harness.Shared.triggerAction(QAbstractSlider::SliderToMaximum);
		expect(close(origins().first, 70.0) && close(origins().second, 140.0),
			   "the bar's maximum puts each side's last line at the top");

		// Every bar value moves a pane; the ends of the two files stay level
		// once both are moving.
		auto previous = std::pair{ -1.0, -1.0 };
		auto level    = 0U;
		for (auto value = 0; value <= 100'000; value += 500) {
			harness.Shared.setValue(value);
			const auto [left, right] = origins();
			expect(left > previous.first || right > previous.second,
				   "no bar step leaves both panes still");
			if (left > 0.0) {
				expect(close(70.0 - left, 140.0 - right, 0.000'001),
					   "once both panes move, the ends of the two files are level");
				++level;
			}
			previous = { left, right };
		}
		expect(level > 10U, "the sweep reached the part where both panes move");

		harness.Controller.home();
		for (auto turn = 0; turn < 100; ++turn) {
			static_cast<void>(send_wheel(harness.Right, QPoint{ 0, -40 }));
		}
		expect(close(harness.Controller.position(), harness.Controller.maximum_position())
				   && close(origins().second, 140.0),
			   "the wheel scrolls the short file until its last line is at the top, and no further");

		// A remeasurement moves the rail with the viewport. At an end the
		// panes stay at that end; elsewhere the line on the rail stays there.
		const auto remeasured = [&](const qreal aViewport) {
			auto axis = qt::build_measured_alignment_axis(diff.Alignment, lines(7U, aViewport), lines(14U, aViewport));
			expect(axis.valid(), "the remeasured axis builds");
			harness.Controller.set_axis(std::move(*axis.Axis));
		};
		harness.Controller.home();
		remeasured(300.0);
		expect(close(harness.Controller.position(), harness.Controller.minimum_position()) && origins() == std::pair{ 0.0, 0.0 },
			   "a comparison shown from its top stays at its top when the viewport shrinks");
		harness.Controller.end();
		remeasured(500.0);
		expect(close(harness.Controller.position(), harness.Controller.maximum_position()) && close(origins().first, 70.0)
				   && close(origins().second, 140.0),
			   "a comparison at its end keeps its last lines at the top when the viewport grows");
		harness.Controller.set_position(harness.Controller.minimum_position() + 40.0);
		const auto railLine = harness.Controller.current_frame()->Right.ProjectedDocumentY;
		remeasured(400.0);
		expect(close(harness.Controller.current_frame()->Right.ProjectedDocumentY, railLine)
				   && close(origins().second, railLine - rail),
			   "between the ends, the line on the rail stays on the rail");

		// Negative control: one line a side has nothing to scroll, so the bar
		// has no travel and End moves nothing.
		auto single = build_axis("alpha", "bravo", { 0.0, 10.0 }, { 0.0, 10.0 }, 400.0, 400.0, 4U);
		harness.Controller.set_axis(std::move(single));
		harness.Controller.end();
		expect(!harness.Shared.isEnabled()
				   && close(harness.Controller.minimum_position(), harness.Controller.maximum_position())
				   && origins() == std::pair{ 0.0, 0.0 },
			   "a one-line comparison offers no travel");
	}

	auto test_live_text_viewport_keyboard_and_scale() -> void
	{
		auto window  = QWidget{ };
		auto left    = QPlainTextEdit{ &window };
		auto right   = QPlainTextEdit{ &window };
		auto linkMap = QWidget{ &window };
		left.setPlainText(QStringLiteral("alpha\ncharlie\ndelta\n"));
		right.setPlainText(
			QStringLiteral("alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n"));
		left.setGeometry(0, 0, 320, 60);
		linkMap.setGeometry(320, 0, 48, 60);
		right.setGeometry(368, 0, 320, 60);
		linkMap.setFocusPolicy(Qt::StrongFocus);
		window.resize(688, 60);
		window.show();
		QApplication::processEvents();

		const auto diff = computed(
			"alpha\ncharlie\ndelta\n",
			"alpha\nbravo-one\nbravo-two\ncharlie\ndelta\n");
		const auto leftGeometry = qt::measure_plain_text_pane(
			left, diff.Alignment.LeftLineCount);
		const auto rightGeometry = qt::measure_plain_text_pane(
			right, diff.Alignment.RightLineCount);
		expect(leftGeometry.has_value() && rightGeometry.has_value(),
			   "live controller text panes measure");
		auto built = qt::build_measured_alignment_axis(
			diff.Alignment, *leftGeometry, *rightGeometry);
		expect(built.valid(), "live controller text-pane axis builds");

		auto shared     = QScrollBar{ Qt::Vertical };
		auto controller = qt::comparison_scroll_controller{ shared };
		controller.bind_input_surface(
			qt::comparison_scroll_receiver::left_pane,
			*left.viewport(),
			left.horizontalScrollBar());
		controller.bind_key_surface(
			qt::comparison_scroll_receiver::left_pane, left);
		controller.bind_input_surface(
			qt::comparison_scroll_receiver::right_pane,
			*right.viewport(),
			right.horizontalScrollBar());
		controller.bind_key_surface(
			qt::comparison_scroll_receiver::right_pane, right);
		controller.bind_input_surface(
			qt::comparison_scroll_receiver::link_map, linkMap);
		controller.set_axis(std::move(*built.Axis));
		controller.home();
		const auto before      = controller.position();
		const auto leftNative  = left.verticalScrollBar()->value();
		const auto rightNative = right.verticalScrollBar()->value();
		expect(send_wheel(*left.viewport(), QPoint{ 0, -2 })
				   && close(controller.position(), before + 2.0),
			   "wheel input over a live left text viewport enters shared q");
		expect(send_wheel(*right.viewport(), QPoint{ 0, 2 })
				   && close(controller.position(), before),
			   "opposing wheel input over a live right text viewport retraces shared q");
		window.activateWindow();
		left.setFocus();
		QApplication::processEvents();
		expect(left.hasFocus(), "left text editor owns real keyboard focus");
		expect(send_key(left, Qt::Key_PageDown)
				   && controller.position() > before,
			   "Page Down over a focused left text editor enters shared q");
		right.setFocus();
		QApplication::processEvents();
		expect(right.hasFocus(), "right text editor owns real keyboard focus");
		expect(send_key(right, Qt::Key_PageUp)
				   && close(controller.position(), before),
			   "Page Up over a focused right text editor reverses shared q exactly");
		linkMap.setFocus();
		QApplication::processEvents();
		expect(linkMap.hasFocus(), "link map owns real keyboard focus");
		expect(send_key(linkMap, Qt::Key_Home)
				   && close(controller.position(), controller.minimum_position()),
			   "Home over the link map enters shared q");
		right.setFocus();
		QApplication::processEvents();
		expect(send_key(right, Qt::Key_End)
				   && close(controller.position(), controller.maximum_position()),
			   "End over a focused text editor enters shared q");
		expect(left.verticalScrollBar()->value() == leftNative
				   && right.verticalScrollBar()->value() == rightNative,
			   "consumed navigation keys never create pane vertical authority");

		const auto expected = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expected.isEmpty()) {
			const auto scale = expected.toDouble();
			expect(close(leftGeometry->DevicePixelRatio, scale, 0.05)
					   && close(rightGeometry->DevicePixelRatio, scale, 0.05),
				   "live controller measurements observe the selected Qt scale");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewComparisonScrollControllerTests"));

	test_receiver_equivalence_and_handoff();
	test_surface_role_rebinding();
	test_pinning_proportional_motion_and_no_dead_travel();
	test_horizontal_drift_and_reorientation();
	test_precision_wheel_phases_and_coalescing();
	test_navigation_reveal_feedback_and_horizontal_isolation();
	test_same_comparison_remeasurement_and_reentrant_sinks();
	test_short_files_scroll_past_their_end_and_the_bar_agrees();
	test_live_text_viewport_keyboard_and_scale();

	std::cout << "comparison-scroll-controller-tests:pass\n";
	return EXIT_SUCCESS;
}
