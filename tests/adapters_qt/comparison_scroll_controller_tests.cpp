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
#include <QScrollBar>
#include <QStyleHints>
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
		expect(!send_wheel(combinedSurface, QPoint{ 0, -2 })
				   && close(controller.position(), 0.0),
			   "replaced wheel surface no longer routes vertical input");
		expect(send_wheel(newViewport, QPoint{ 0, -2 })
				   && close(controller.position(), 2.0),
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

		harness.Controller.set_axis(insertion_axis());
		harness.Controller.set_position(0.0);
		auto       previous    = *harness.Controller.current_frame();
		const auto sampleCount = static_cast<std::size_t>(
			std::floor(harness.Controller.maximum_position() * 4.0));
		for (std::size_t sample = 1U; sample < sampleCount; ++sample) {
			const auto position = static_cast<qreal>(sample) / 4.0;
			harness.Controller.set_position(position);
			const auto current = *harness.Controller.current_frame();
			expect(current.Position > previous.Position,
				   "each positive comparison sample advances retained position");
			expect(!close(current.Left.ProjectedDocumentY,
						  previous.Left.ProjectedDocumentY)
					   || !close(current.Right.ProjectedDocumentY,
								 previous.Right.ProjectedDocumentY),
				   "each positive comparison sample advances at least one pane");
			previous = current;
		}
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

		harness.Controller.set_position(5.0);
		expect(send_wheel(
				   harness.Left, QPoint{ 0, -4 }, QPoint{ 0, -120 }),
			   "pixel wheel event is consumed");
		expect(close(harness.Controller.position(), 9.0),
			   "pixel delta takes precedence over angle fallback");
		harness.Controller.set_position(5.0);
		expect(send_wheel(harness.Left, { }, QPoint{ 0, -120 }),
			   "angle wheel fallback is consumed");
		const auto lines = std::max(
			1, QGuiApplication::styleHints()->wheelScrollLines());
		expect(close(
				   harness.Controller.position(),
				   std::min(60.0, 5.0 + static_cast<qreal>(lines) * 16.0)),
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
		harness.Controller.end();
		expect(close(harness.Controller.position(), 60.0)
				   && close(harness.Controller.current_frame()
								->Left.ProjectedDocumentY,
							30.0)
				   && close(harness.Controller.current_frame()
								->Right.ProjectedDocumentY,
							60.0)
				   && harness.Controller.current_frame()->Left.TrailingClamped
				   && harness.Controller.current_frame()->Right.TrailingClamped,
			   "End reaches the final legal endpoint");
		harness.Controller.home();
		expect(close(harness.Controller.position(), 0.0)
				   && close(harness.Controller.current_frame()
								->Left.ProjectedDocumentY,
							0.0)
				   && close(harness.Controller.current_frame()
								->Right.ProjectedDocumentY,
							0.0)
				   && harness.Controller.current_frame()->Left.LeadingClamped
				   && harness.Controller.current_frame()->Right.LeadingClamped,
			   "Home reaches the first legal endpoint");

		harness.Shared.setValue(25'000);
		expect(close(harness.Controller.position(), 15.0),
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
		expect(close(harness.Controller.position(), 60.0),
			   "scrollbar maximum action reaches final endpoint");
		harness.Shared.triggerAction(QAbstractSlider::SliderToMinimum);
		expect(close(harness.Controller.position(), 0.0),
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
		expect(close(harness.Controller.position(), 0.0),
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
		expect(!replaceOnce
				   && harness.Controller.current_frame()->Segment
					   == reentrantInsertion.Id
				   && close(harness.Controller.position(), 105.0)
				   && close(harness.Controller.maximum_position(), 180.0)
				   && close(
					   harness.Controller.current_frame()->Left.ProjectedDocumentY,
					   30.0)
				   && close(
					   harness.Controller.current_frame()->Right.ProjectedDocumentY,
					   105.0)
				   && harness.Shared.value() == 58'333,
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

		if (const auto* expected = std::getenv("BPR_EXPECTED_QT_SCALE")) {
			const auto scale = QString::fromLocal8Bit(expected).toDouble();
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
	test_precision_wheel_phases_and_coalescing();
	test_navigation_reveal_feedback_and_horizontal_isolation();
	test_same_comparison_remeasurement_and_reentrant_sinks();
	test_live_text_viewport_keyboard_and_scale();

	std::cout << "comparison-scroll-controller-tests:pass\n";
	return EXIT_SUCCESS;
}
