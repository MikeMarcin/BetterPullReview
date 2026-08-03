#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/diff_workbench.hpp"

#include <QAccessible>
#include <QApplication>
#include <QFont>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QString>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
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
		const qreal aLeft,
		const qreal aRight,
		const qreal aTolerance = 0.000'001) noexcept -> bool
	{
		return std::abs(aLeft - aRight) <= aTolerance;
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto key(
		const core::diff_options& aOptions,
		const std::uint8_t        aSeed) -> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "fixture options normalize");
		const auto scope     = opaque<core::pull_request_authority_scope_id>(11U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ static_cast<std::int64_t>(1U + aSeed) },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ static_cast<std::int64_t>(2U + aSeed) },
			},
			.LeftBlob  = blob(static_cast<std::uint8_t>(21U + aSeed)),
			.RightBlob = blob(static_cast<std::uint8_t>(51U + aSeed)),
			.File      = opaque<core::file_identity>(static_cast<std::uint8_t>(31U + aSeed)),
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
		const std::string_view    aLeft,
		const std::string_view    aRight,
		const core::diff_options& aOptions,
		const std::uint8_t        aSeed) -> core::computed_file_diff
	{
		auto       left    = bytes(aLeft);
		auto       right   = bytes(aRight);
		const auto outcome = core::compute_file_diff({
			.Key        = key(aOptions, aSeed),
			.Options    = aOptions,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(outcome.Reason == core::diff_reason_code::none, "fixture diff computes");
		expect(outcome.Result.has_value(), "fixture diff publishes");
		return *outcome.Result;
	}

	[[nodiscard]] auto context(
		const core::computed_file_diff& aDiff,
		const std::uint64_t             aRequest,
		const std::uint64_t             aGeneration) -> core::diff_request_context
	{
		return {
			.Request            = core::request_id{ aRequest },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = aDiff.Alignment.Key,
			.Generation         = core::request_generation{ aGeneration },
		};
	}

	[[nodiscard]] auto representative_text() -> std::pair<std::string, std::string>
	{
		auto left  = std::string(240U, 'L') + "-long-common-line\n";
		auto right = left;
		for (std::size_t line = 0U; line < 28U; ++line) {
			const auto common = "prefix-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		left += "old-a\nold-b\nold-c\nold-d\n";
		right += "new-a\nnew-b\nnew-c\nnew-d\nnew-e\nnew-f\nnew-g\n";
		for (std::size_t line = 0U; line < 12U; ++line) {
			const auto common = "middle-a-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		left += "delete-a\ndelete-b\ndelete-c\ndelete-d\n";
		for (std::size_t line = 0U; line < 12U; ++line) {
			const auto common = "middle-b-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		right += "insert-a\ninsert-b\ninsert-c\ninsert-d\ninsert-e\n";
		for (std::size_t line = 0U; line < 50U; ++line) {
			const auto common = "tail-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto segment_of_kind(
		const core::computed_file_diff& aDiff,
		const core::alignment_kind      aKind,
		const std::size_t               aOccurrence = 0U)
		-> const core::alignment_segment&
	{
		auto occurrence = std::size_t{ 0U };
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == aKind && occurrence++ == aOccurrence) {
				return segment;
			}
		}
		throw std::runtime_error{ "fixture segment kind absent" };
	}

	void process_events()
	{
		for (auto count = 0; count < 4; ++count) {
			QApplication::processEvents();
		}
	}

	struct accessible_announcement
	{
		QString                             Message;
		QObject*                            Target = nullptr;
		QAccessible::AnnouncementPoliteness Politeness
			= QAccessible::AnnouncementPoliteness::Polite;
	};

	std::vector<accessible_announcement>* AnnouncementSink = nullptr;

	void capture_accessibility_update(QAccessibleEvent* const aEvent)
	{
		if (AnnouncementSink == nullptr || aEvent == nullptr
			|| aEvent->type() != QAccessible::Announcement) {
			return;
		}
		const auto* const announcement
			= static_cast<const QAccessibleAnnouncementEvent*>(aEvent);
		AnnouncementSink->push_back({
			.Message    = announcement->message(),
			.Target     = aEvent->object(),
			.Politeness = announcement->politeness(),
		});
	}

	class scoped_accessibility_capture final
	{
	public:
		explicit scoped_accessibility_capture(
			std::vector<accessible_announcement>& aAnnouncements)
			: Previous{
				QAccessible::installUpdateHandler(capture_accessibility_update)
			}
		{
			AnnouncementSink = &aAnnouncements;
		}

		~scoped_accessibility_capture()
		{
			AnnouncementSink = nullptr;
			static_cast<void>(QAccessible::installUpdateHandler(Previous));
		}

		scoped_accessibility_capture(const scoped_accessibility_capture&) = delete;
		auto operator=(const scoped_accessibility_capture&)
			-> scoped_accessibility_capture&                         = delete;
		scoped_accessibility_capture(scoped_accessibility_capture&&) = delete;
		auto operator=(scoped_accessibility_capture&&)
			-> scoped_accessibility_capture& = delete;

	private:
		QAccessible::UpdateHandler Previous = nullptr;
	};

	[[nodiscard]] auto announcement_count(
		const std::vector<accessible_announcement>& aAnnouncements,
		const QObject*                              aTarget,
		const QString&                              aMessage) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(
			aAnnouncements,
			[aTarget, &aMessage](const accessible_announcement& aAnnouncement) {
				return aAnnouncement.Target == aTarget
					&& aAnnouncement.Message == aMessage
					&& aAnnouncement.Politeness
					== QAccessible::AnnouncementPoliteness::Polite;
			}));
	}

	struct rendered_pane_state
	{
		int    ScrollValue = 0;
		QImage Pixels;
	};

	[[nodiscard]] auto rendered_state(QPlainTextEdit& aPane)
		-> rendered_pane_state
	{
		process_events();
		return {
			.ScrollValue = aPane.verticalScrollBar()->value(),
			.Pixels      = aPane.viewport()->grab().toImage(),
		};
	}

	[[nodiscard]] auto send_wheel(
		QWidget&      aSurface,
		const QPoint& aPixel,
		const QPoint& aAngle = { }) -> bool
	{
		auto event = QWheelEvent{
			QPointF{ 8.0, 8.0 },
			QPointF{ 8.0, 8.0 },
			aPixel,
			aAngle,
			Qt::NoButton,
			Qt::NoModifier,
			Qt::NoScrollPhase,
			false,
		};
		event.ignore();
		QCoreApplication::sendEvent(&aSurface, &event);
		return event.isAccepted();
	}

	void send_click(QWidget& aSurface, const QPointF aPosition)
	{
		auto press = QMouseEvent{
			QEvent::MouseButtonPress,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		auto release = QMouseEvent{
			QEvent::MouseButtonRelease,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::NoButton,
			Qt::NoModifier,
		};
		QCoreApplication::sendEvent(&aSurface, &press);
		QCoreApplication::sendEvent(&aSurface, &release);
	}

	void send_key(QWidget& aSurface, const Qt::Key aKey)
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QCoreApplication::sendEvent(&aSurface, &press);
		QCoreApplication::sendEvent(&aSurface, &release);
	}

	void test_selected_composition()
	{
		auto announcements               = std::vector<accessible_announcement>{ };
		auto capture                     = scoped_accessibility_capture{ announcements };
		const auto [leftText, rightText] = representative_text();
		auto       options               = core::diff_options{ };
		const auto diff                  = computed(leftText, rightText, options, 0U);
		expect(diff.Alignment.Groups.size() >= 3U, "fixture has several semantic changes");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		if (const auto* expectedScale = std::getenv("BPR_EXPECTED_QT_SCALE")) {
			expect(
				close(
					workbench.devicePixelRatioF(),
					QString::fromLocal8Bit(expectedScale).toDouble(),
					0.05),
				"selected workbench observes the requested Qt scale");
		}

		const auto firstContext = context(diff, 1U, 1U);
		workbench.set_loading(firstContext);
		workbench.set_loading(firstContext);
		expect(workbench.current_result() == nullptr, "loading clears old result and geometry");
		expect(
			announcement_count(
				announcements,
				&workbench,
				QStringLiteral("Loading selected file comparison…"))
				== 1U,
			"loading status is one polite deduplicated workbench announcement");
		workbench.publish({
			.Context = firstContext,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();
		expect(workbench.current_result() != nullptr, "accepted immutable result reaches workbench");
		expect(
			announcement_count(
				announcements, &workbench, workbench.status_text())
				== 1U,
			"ready status is one polite workbench announcement");
		expect(
			std::ranges::count_if(
				announcements,
				[&workbench](const accessible_announcement& aAnnouncement) {
					return aAnnouncement.Target == &workbench
						|| aAnnouncement.Target == workbench.link_map();
				})
				== 2,
			"published loading and ready states do not add a duplicate link-map announcement");
		expect(
			workbench.left_pane()->toPlainText() == QString::fromStdString(leftText)
				&& workbench.right_pane()->toPlainText() == QString::fromStdString(rightText),
			"natural panes contain exact source documents without alignment rows");
		expect(
			static_cast<std::uint32_t>(workbench.left_pane()->document()->blockCount())
					<= diff.Alignment.LeftLineCount + 1U
				&& static_cast<std::uint32_t>(workbench.right_pane()->document()->blockCount())
					<= diff.Alignment.RightLineCount + 1U,
			"Qt panes add at most their optional trailing visual block");
		expect(
			workbench.shared_scrollbar()->isEnabled()
				&& workbench.left_pane()->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff
				&& workbench.right_pane()->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
			"one shared scrollbar is the visible vertical authority");
		expect(
			!workbench.accessibleName().isEmpty()
				&& !workbench.left_pane()->accessibleDescription().isEmpty()
				&& !workbench.right_pane()->accessibleDescription().isEmpty()
				&& !workbench.link_map()->accessibleName().isEmpty()
				&& !workbench.shared_scrollbar()->accessibleDescription().isEmpty()
				&& !workbench.detail_pane()->accessibleDescription().isEmpty(),
			"composed focus surfaces expose accessible identities and status");
		workbench.next_change_button()->click();
		process_events();
		expect(
			workbench.previous_change_button()->isEnabled()
				&& workbench.next_change_button()->isEnabled(),
			"middle semantic selection enables both navigation commands");
		workbench.previous_change_button()->setFocus(Qt::OtherFocusReason);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.previous_change_button(),
			"focus enters the first workbench command");
		send_key(*workbench.previous_change_button(), Qt::Key_Tab);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.next_change_button(),
			"Tab follows the explicit command order");
		send_key(*workbench.next_change_button(), Qt::Key_Tab);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.left_pane(),
			"Tab advances from commands into the earlier natural source pane");
		workbench.previous_change_button()->setFocus(Qt::OtherFocusReason);
		process_events();

		auto* const controller = workbench.scroll_controller();
		controller->set_position(controller->maximum_position() * 0.45);
		const auto beforeLeftWheel = controller->position();
		process_events();
		const auto nativeWheelStartLeft  = rendered_state(*workbench.left_pane());
		const auto nativeWheelStartRight = rendered_state(*workbench.right_pane());
		expect(send_wheel(*workbench.left_pane()->viewport(), QPoint{ 0, -32 }),
			   "left pane wheel routes through shared controller");
		const auto leftWheel            = controller->position();
		const auto leftWheelNativeLeft  = rendered_state(*workbench.left_pane());
		const auto leftWheelNativeRight = rendered_state(*workbench.right_pane());
		controller->set_position(beforeLeftWheel);
		expect(send_wheel(*workbench.right_pane()->viewport(), QPoint{ 0, -32 }),
			   "right pane wheel routes through shared controller");
		const auto rightWheel            = controller->position();
		const auto rightWheelNativeLeft  = rendered_state(*workbench.left_pane());
		const auto rightWheelNativeRight = rendered_state(*workbench.right_pane());
		controller->set_position(beforeLeftWheel);
		expect(send_wheel(*workbench.link_map(), QPoint{ 0, -32 }),
			   "divider wheel routes through shared controller");
		const auto mapWheel            = controller->position();
		const auto mapWheelNativeLeft  = rendered_state(*workbench.left_pane());
		const auto mapWheelNativeRight = rendered_state(*workbench.right_pane());
		expect(
			close(leftWheel, rightWheel) && close(rightWheel, mapWheel)
				&& leftWheel > beforeLeftWheel,
			"all three wheel receivers produce the same signed comparison-space motion");
		expect(
			leftWheelNativeLeft.ScrollValue == rightWheelNativeLeft.ScrollValue
				&& rightWheelNativeLeft.ScrollValue == mapWheelNativeLeft.ScrollValue
				&& leftWheelNativeRight.ScrollValue == rightWheelNativeRight.ScrollValue
				&& rightWheelNativeRight.ScrollValue == mapWheelNativeRight.ScrollValue
				&& (leftWheelNativeLeft.ScrollValue != nativeWheelStartLeft.ScrollValue
					|| leftWheelNativeRight.ScrollValue
						!= nativeWheelStartRight.ScrollValue),
			"all three input receivers apply the same motion to the native panes");
		const auto qBeforeHorizontal     = controller->position();
		const auto rightBeforeHorizontal = workbench.right_pane()
											   ->horizontalScrollBar()
											   ->value();
		expect(
			workbench.left_pane()->horizontalScrollBar()->maximum() > 0
				&& send_wheel(
					*workbench.left_pane()->viewport(), QPoint{ -24, 0 }),
			"left pane admits pane-local horizontal wheel input");
		expect(
			close(controller->position(), qBeforeHorizontal)
				&& workbench.left_pane()->horizontalScrollBar()->value() > 0
				&& workbench.right_pane()->horizontalScrollBar()->value()
					== rightBeforeHorizontal,
			"horizontal input moves only its natural source pane and preserves q");

		const auto& insertion = segment_of_kind(diff, core::alignment_kind::right_only);
		expect(controller->reveal_segment(insertion.Id, 0.2), "insertion reveal succeeds");
		const auto insertionFirst      = *controller->current_frame();
		const auto insertionFirstLeft  = rendered_state(*workbench.left_pane());
		const auto insertionFirstRight = rendered_state(*workbench.right_pane());
		expect(controller->reveal_segment(insertion.Id, 0.8), "insertion late reveal succeeds");
		const auto insertionSecond      = *controller->current_frame();
		const auto insertionSecondLeft  = rendered_state(*workbench.left_pane());
		const auto insertionSecondRight = rendered_state(*workbench.right_pane());
		expect(
			close(
				insertionFirst.Left.ProjectedDocumentY,
				insertionSecond.Left.ProjectedDocumentY)
				&& insertionSecond.Right.ProjectedDocumentY
					> insertionFirst.Right.ProjectedDocumentY,
			"right insertion pins left while right advances");
		expect(
			insertionFirstLeft.ScrollValue == insertionSecondLeft.ScrollValue
				&& insertionSecondRight.ScrollValue
					> insertionFirstRight.ScrollValue
				&& insertionFirstLeft.Pixels == insertionSecondLeft.Pixels
				&& insertionFirstRight.Pixels != insertionSecondRight.Pixels,
			"right insertion visibly pins the native left pane while the right pane advances");
		const auto& deletion = segment_of_kind(diff, core::alignment_kind::left_only);
		expect(controller->reveal_segment(deletion.Id, 0.2), "deletion reveal succeeds");
		const auto deletionFirst      = *controller->current_frame();
		const auto deletionFirstLeft  = rendered_state(*workbench.left_pane());
		const auto deletionFirstRight = rendered_state(*workbench.right_pane());
		expect(controller->reveal_segment(deletion.Id, 0.8), "deletion late reveal succeeds");
		const auto deletionSecond      = *controller->current_frame();
		const auto deletionSecondLeft  = rendered_state(*workbench.left_pane());
		const auto deletionSecondRight = rendered_state(*workbench.right_pane());
		expect(
			deletionSecond.Left.ProjectedDocumentY > deletionFirst.Left.ProjectedDocumentY
				&& close(
					deletionFirst.Right.ProjectedDocumentY,
					deletionSecond.Right.ProjectedDocumentY),
			"left deletion pins right while left advances");
		expect(
			deletionSecondLeft.ScrollValue > deletionFirstLeft.ScrollValue
				&& deletionFirstRight.ScrollValue == deletionSecondRight.ScrollValue
				&& deletionFirstLeft.Pixels != deletionSecondLeft.Pixels
				&& deletionFirstRight.Pixels == deletionSecondRight.Pixels,
			"left deletion visibly advances the native left pane while pinning the right pane");
		const auto& replacement = segment_of_kind(diff, core::alignment_kind::replacement);
		expect(controller->reveal_segment(replacement.Id, 0.2), "replacement reveal succeeds");
		const auto replacementFirst      = *controller->current_frame();
		const auto replacementFirstLeft  = rendered_state(*workbench.left_pane());
		const auto replacementFirstRight = rendered_state(*workbench.right_pane());
		expect(controller->reveal_segment(replacement.Id, 0.8), "replacement late reveal succeeds");
		const auto replacementSecond      = *controller->current_frame();
		const auto replacementSecondLeft  = rendered_state(*workbench.left_pane());
		const auto replacementSecondRight = rendered_state(*workbench.right_pane());
		const auto leftAdvance            = replacementSecond.Left.ProjectedDocumentY
			- replacementFirst.Left.ProjectedDocumentY;
		const auto rightAdvance = replacementSecond.Right.ProjectedDocumentY
			- replacementFirst.Right.ProjectedDocumentY;
		expect(
			leftAdvance > 0.0 && rightAdvance > leftAdvance,
			"unequal replacement advances both panes proportionally");
		expect(
			replacementSecondLeft.ScrollValue > replacementFirstLeft.ScrollValue
				&& replacementSecondRight.ScrollValue
					> replacementFirstRight.ScrollValue
				&& replacementSecondRight.ScrollValue
						- replacementFirstRight.ScrollValue
					> replacementSecondLeft.ScrollValue
						- replacementFirstLeft.ScrollValue
				&& replacementFirstLeft.Pixels != replacementSecondLeft.Pixels
				&& replacementFirstRight.Pixels != replacementSecondRight.Pixels,
			"unequal replacement visibly advances both native panes proportionally");

		controller->set_position(controller->maximum_position() * 0.5);
		const auto reversible      = controller->position();
		const auto reversibleLeft  = rendered_state(*workbench.left_pane());
		const auto reversibleRight = rendered_state(*workbench.right_pane());
		for (std::size_t cycle = 0U; cycle < 1'000U; ++cycle) {
			controller->scroll_by(0.1);
			controller->scroll_by(-0.1);
		}
		expect(close(controller->position(), reversible, 0.000'000'001),
			   "forward and reverse motion does not accumulate drift");
		const auto reversedLeft  = rendered_state(*workbench.left_pane());
		const auto reversedRight = rendered_state(*workbench.right_pane());
		expect(
			reversedLeft.ScrollValue == reversibleLeft.ScrollValue
				&& reversedRight.ScrollValue == reversibleRight.ScrollValue
				&& reversedLeft.Pixels == reversibleLeft.Pixels
				&& reversedRight.Pixels == reversibleRight.Pixels,
			"forward and reverse traces restore both native pane origins and rendered frames");
		send_key(*workbench.left_pane(), Qt::Key_Home);
		expect(close(controller->position(), controller->minimum_position()),
			   "Home from the left source reaches comparison beginning");
		send_key(*workbench.link_map(), Qt::Key_PageDown);
		expect(controller->position() > controller->minimum_position(),
			   "Page from the divider advances comparison space");
		send_key(*workbench.right_pane(), Qt::Key_End);
		expect(close(controller->position(), controller->maximum_position()),
			   "End from the right source reaches the asymmetric tail without dead travel");
		workbench.shared_scrollbar()->setValue(
			(workbench.shared_scrollbar()->minimum()
			 + workbench.shared_scrollbar()->maximum())
			/ 2);
		process_events();
		expect(
			controller->position() > controller->minimum_position()
				&& controller->position() < controller->maximum_position(),
			"shared scrollbar drives the same retained q");

		expect(controller->reveal_segment(replacement.Id, 0.5),
			   "replacement is visible for ribbon interaction");
		process_events();
		const auto visible = workbench.link_map()->visible_links();
		const auto ribbon  = std::ranges::find(
			visible, replacement.Id,
			[](const qt::visible_diff_link& aLink) { return aLink.Identity.Segment; });
		expect(ribbon != visible.end(), "replacement ribbon is visible in selected composition");
		auto point = ribbon->Path.boundingRect().center();
		expect(ribbon->Path.contains(point), "ribbon midpoint is an interactive interior point");
		const auto announcementsBeforeRibbon = announcements.size();
		send_click(*workbench.link_map(), point);
		process_events();
		auto ribbonWorkbenchAnnouncements = std::size_t{ 0U };
		auto ribbonLinkMapAnnouncements   = std::size_t{ 0U };
		for (auto index = announcementsBeforeRibbon;
			 index < announcements.size();
			 ++index) {
			ribbonWorkbenchAnnouncements += announcements[index].Target == &workbench
				? 1U
				: 0U;
			ribbonLinkMapAnnouncements += announcements[index].Target
					== workbench.link_map()
				? 1U
				: 0U;
		}
		expect(
			workbench.selection().has_value()
				&& workbench.selection()->Group == ribbon->Identity.Group
				&& workbench.link_map()->selected().has_value()
				&& workbench.link_map()->selected()->Group == workbench.selection()->Group
				&& workbench.detail_pane()->toPlainText().startsWith(QStringLiteral("Change ")),
			"ribbon, semantic selection, top highlight, and detail agree on stable group identity");
		expect(
			ribbonWorkbenchAnnouncements == 1U
				&& ribbonLinkMapAnnouncements == 0U,
			"ribbon selection emits one combined polite workbench announcement without a link-map duplicate");
		const auto selectedGroup = workbench.selection()->Group;
		workbench.next_change_button()->click();
		process_events();
		expect(workbench.selection()->Group != selectedGroup,
			   "Next change navigates the canonical group vector");
		workbench.previous_change_button()->click();
		process_events();
		expect(workbench.selection()->Group == selectedGroup,
			   "Previous change returns to the same stable group");

		const auto targetBlock = workbench.left_pane()->document()->findBlockByNumber(
			static_cast<int>(replacement.Left.Begin));
		QTextCursor cursor{ targetBlock };
		const auto  cursorPoint = workbench.left_pane()->cursorRect(cursor).center();
		send_click(*workbench.left_pane()->viewport(), cursorPoint);
		process_events();
		expect(
			workbench.selection().has_value()
				&& workbench.selection()->PreferredLine.Side == core::diff_side::left
				&& workbench.selection()->PreferredLine.Line == replacement.Left.Begin,
			"top-pane line click becomes a core semantic line intent");

		const auto selectionBeforeMetrics = workbench.selection();
		const auto rebuildsBeforeMetrics  = workbench.statistics().GeometryRebuilds;
		workbench.resize(1'080, 700);
		auto font = workbench.left_pane()->font();
		font.setPointSizeF(font.pointSizeF() + 2.0);
		workbench.setFont(font);
		process_events();
		expect(
			workbench.statistics().GeometryRebuilds > rebuildsBeforeMetrics
				&& workbench.selection() == selectionBeforeMetrics
				&& workbench.link_map()->selected().has_value(),
			"resize/font metric change rebuilds Qt geometry and preserves semantic identity");
		auto* const vertical = workbench.findChild<QSplitter*>(
			QStringLiteral("diffWorkbenchVerticalSplitter"));
		expect(vertical != nullptr, "selected composition exposes its vertical splitter");
		const auto outerSizeBeforeDivider = workbench.size();
		const auto viewportBeforeDivider  = workbench.left_pane()->viewport()->height();
		const auto rebuildsBeforeDivider  = workbench.statistics().GeometryRebuilds;
		const auto verticalSizes          = vertical->sizes();
		const auto verticalTotal          = std::accumulate(
			verticalSizes.begin(), verticalSizes.end(), 0);
		const auto smallerTop = std::max(140, verticalTotal / 3);
		vertical->setSizes({ smallerTop, verticalTotal - smallerTop });
		process_events();
		expect(
			workbench.size() == outerSizeBeforeDivider
				&& workbench.left_pane()->viewport()->height()
					< viewportBeforeDivider
				&& workbench.statistics().GeometryRebuilds
					== rebuildsBeforeDivider + 1U
				&& workbench.selection() == selectionBeforeMetrics,
			"detail-divider resize coalesces one viewport-axis rebuild without changing selection");
		send_key(*workbench.right_pane(), Qt::Key_End);
		process_events();
		expect(
			workbench.left_pane()->verticalScrollBar()->value()
					== workbench.left_pane()->verticalScrollBar()->maximum()
				&& workbench.right_pane()->verticalScrollBar()->value()
					== workbench.right_pane()->verticalScrollBar()->maximum(),
			"rebuilt viewport extents let End reveal both native document tails");
		const auto rebuildsBeforeDpi = workbench.statistics().GeometryRebuilds;
		auto       dpiChange         = QEvent{ QEvent::DevicePixelRatioChange };
		QCoreApplication::sendEvent(&workbench, &dpiChange);
		process_events();
		expect(
			workbench.statistics().GeometryRebuilds > rebuildsBeforeDpi
				&& workbench.selection() == selectionBeforeMetrics,
			"DPI invalidation rebuilds measured geometry without replacing semantic selection");

		const auto initialRender   = workbench.grab().toImage();
		const auto originalPalette = QApplication::palette();
		auto       changedPalette  = originalPalette;
		changedPalette.setColor(QPalette::Window, QColor{ 31, 35, 41 });
		changedPalette.setColor(QPalette::Base, QColor{ 22, 25, 30 });
		changedPalette.setColor(QPalette::Text, QColor{ 236, 239, 244 });
		changedPalette.setColor(QPalette::WindowText, QColor{ 236, 239, 244 });
		QApplication::setPalette(changedPalette);
		process_events();
		const auto changedRender = workbench.grab().toImage();
		expect(
			workbench.selection() == selectionBeforeMetrics
				&& initialRender != changedRender,
			"inherited theme transition changes rendered pixels without changing selection");
		QApplication::setPalette(originalPalette);
		process_events();

		const auto second        = computed(leftText, rightText + "option-tail\n", options, 1U);
		const auto secondContext = context(second, 2U, 2U);
		workbench.set_loading(secondContext);
		workbench.publish({
			.Context = firstContext,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		expect(
			workbench.current_result() == nullptr
				&& workbench.statistics().RejectedStaleInput >= 1U,
			"file/epoch replacement rejects an old completion before geometry appears");
		workbench.publish({
			.Context = secondContext,
			.Result  = std::make_shared<const core::computed_file_diff>(second),
		});
		process_events();
		expect(workbench.current_result() != nullptr, "replacement comparison publishes");

		auto optionVariant       = options;
		optionVariant.Whitespace = core::whitespace_policy::ignore_trailing;
		const auto third         = computed(leftText, rightText, optionVariant, 2U);
		const auto thirdContext  = context(third, 3U, 3U);
		workbench.set_loading(thirdContext);
		workbench.fail({
			.Context = secondContext,
			.Reason  = core::diff_reason_code::host_failure,
		});
		expect(workbench.current_result() == nullptr,
			   "stale failure cannot replace a newer options selection");
		workbench.publish({
			.Context = thirdContext,
			.Result  = std::make_shared<const core::computed_file_diff>(third),
		});
		process_events();
		workbench.fail({
			.Context = thirdContext,
			.Reason  = core::diff_reason_code::host_failure,
		});
		expect(
			workbench.current_result() == nullptr
				&& !workbench.current_context().has_value()
				&& workbench.status_text() == QStringLiteral("Comparison unavailable"),
			"exact failure clears selected geometry and presents a typed status");
		expect(
			announcement_count(
				announcements,
				&workbench,
				QStringLiteral("Comparison unavailable"))
				== 1U,
			"exact failure is one polite workbench announcement");
		workbench.set_loading(thirdContext);
		workbench.publish({
			.Context = thirdContext,
			.Result  = std::make_shared<const core::computed_file_diff>(third),
		});
		process_events();
		workbench.invalidate(third.Alignment.Key.Authority);
		expect(
			workbench.current_result() == nullptr
				&& !workbench.current_context().has_value()
				&& workbench.status_text() == QStringLiteral("Comparison invalidated"),
			"authority/cache invalidation synchronously clears result and geometry");
		expect(
			announcement_count(
				announcements,
				&workbench,
				QStringLiteral("Comparison invalidated"))
				== 1U,
			"invalidation is one polite workbench announcement");
		const auto rejectedBeforeLate = workbench.statistics().RejectedStaleInput;
		workbench.publish({
			.Context = thirdContext,
			.Result  = std::make_shared<const core::computed_file_diff>(third),
		});
		expect(
			workbench.current_result() == nullptr
				&& workbench.statistics().RejectedStaleInput
					== rejectedBeforeLate + 1U,
			"late publication after invalidation cannot recreate cleared Qt state");

		if (const auto* renderPath = std::getenv("BPR_QT_DIFF_WORKBENCH_RENDER_PATH")) {
			workbench.set_loading(firstContext);
			workbench.publish({
				.Context = firstContext,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			});
			process_events();
			expect(workbench.grab().save(QString::fromLocal8Bit(renderPath)),
				   "selected workbench raster saves");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(QStringLiteral("BetterPullReviewDiffWorkbenchTests"));
	test_selected_composition();
	std::cout << "diff-workbench-tests:pass\n";
	return EXIT_SUCCESS;
}
