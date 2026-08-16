#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/diff_workbench.hpp"

#include <QAccessible>
#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QString>
#include <QSysInfo>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QThread>
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

	[[nodiscard]] auto large_comparison_text(
		const std::size_t aLines,
		const std::size_t aChangeInterval)
		-> std::pair<std::string, std::string>
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		left.reserve(aLines * 24U);
		right.reserve(aLines * 24U);
		for (std::size_t line = 0U; line < aLines; ++line) {
			const auto suffix = std::to_string(line) + '\n';
			if (line % aChangeInterval == 0U) {
				left += "earlier-change-" + suffix;
				right += "later-change-" + suffix;
			} else {
				left += "stable-context-" + suffix;
				right += "stable-context-" + suffix;
			}
		}
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto large_insertion_text()
		-> std::pair<std::string, std::string>
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		for (std::size_t line = 0U; line < 32U; ++line) {
			const auto common = "leading-context-" + std::to_string(line) + '\n';
			left += common;
			right += common;
		}
		for (std::size_t line = 0U; line < 5'000U; ++line) {
			right += "large-insertion-" + std::to_string(line) + '\n';
		}
		for (std::size_t line = 0U; line < 32U; ++line) {
			const auto common = "trailing-context-" + std::to_string(line) + '\n';
			left += common;
			right += common;
		}
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto dense_move_text()
		-> std::pair<std::string, std::string>
	{
		auto           left  = std::string{ };
		auto           right = std::string{ };
		constexpr auto moves = 6U;
		for (auto line = 0U; line < moves; ++line) {
			left += "moved-scale-" + std::to_string(line) + '\n';
		}
		for (auto line = 0U; line < 4U; ++line) {
			const auto stable = "move-stable-" + std::to_string(line) + '\n';
			left += stable;
			right += stable;
		}
		for (auto line = moves; line > 0U; --line) {
			right += "moved-scale-" + std::to_string(line - 1U) + '\n';
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
		expect(
			workbench.statistics().GeometryRebuilds == 0U
				&& workbench.statistics().GeometryRebuildHistorySize == 0U,
			"empty-workbench show and resize events do not create attributed geometry work");
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
		const auto  statisticsAfterFirstPublish = workbench.statistics();
		const auto& firstGeometryCause
			= statisticsAfterFirstPublish.GeometryRebuildHistory[0U];
		expect(
			statisticsAfterFirstPublish.GeometryRebuilds > 0U
				&& statisticsAfterFirstPublish.GeometryRebuildHistorySize
					== statisticsAfterFirstPublish.GeometryRebuilds
				&& (firstGeometryCause.Changes & qt::measured_geometry_change::screen)
					== qt::measured_geometry_change::none
				&& (firstGeometryCause.Triggers
					& qt::diff_geometry_rebuild_trigger::show_event)
					== qt::diff_geometry_rebuild_trigger::none,
			"pre-result show and resize events cannot contaminate the first accepted diff attribution");
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
		const auto firstGroup = workbench.selection()->Group;
		expect(workbench.navigate_change(core::change_navigation_direction::next,
										 core::change_navigation_policy::stop),
			   "the semantic navigation bridge selects the next canonical group");
		process_events();
		expect(
			workbench.selection()->Group != firstGroup,
			"semantic navigation changes the one retained selection");
		expect(workbench.findChildren<QToolButton*>().isEmpty(),
			   "the comparison surface contains no duplicate change-navigation presenter");
		workbench.left_pane()->setFocus(Qt::OtherFocusReason);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.left_pane(),
			"focus enters the earlier natural source pane directly");
		send_key(*workbench.left_pane(), Qt::Key_Tab);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.link_map(),
			"Tab follows the comparison surface's internal focus order");
		send_key(*workbench.link_map(), Qt::Key_Tab);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.right_pane(),
			"Tab advances from the link map into the later natural source pane");
		workbench.left_pane()->setFocus(Qt::OtherFocusReason);
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
		const auto renderedReplacement    = workbench.link_map()->current_frame();
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
			renderedReplacement.has_value()
				&& renderedReplacement->Left.DocumentOriginY > 0.0
				&& renderedReplacement->Right.DocumentOriginY > 0.0,
			"rendered correspondence frame retains both applied pane origins");
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
		expect(workbench.navigate_change(core::change_navigation_direction::next,
										 core::change_navigation_policy::stop),
			   "Next change navigates the canonical group vector");
		process_events();
		expect(workbench.selection()->Group != selectedGroup,
			   "Next change navigates the canonical group vector");
		expect(workbench.navigate_change(core::change_navigation_direction::previous,
										 core::change_navigation_policy::stop),
			   "Previous change returns through the same semantic bridge");
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
		const auto outerSizeBeforeDivider  = workbench.size();
		const auto viewportBeforeDivider   = workbench.left_pane()->viewport()->height();
		const auto statisticsBeforeDivider = workbench.statistics();
		const auto verticalSizes           = vertical->sizes();
		const auto verticalTotal           = std::accumulate(
			verticalSizes.begin(), verticalSizes.end(), 0);
		const auto largerTop = std::min(
			verticalTotal - 48, verticalSizes.front() + 32);
		expect(largerTop > verticalSizes.front(),
			   "the compact detail pane retains room for a divider gesture");
		vertical->setSizes({ largerTop, verticalTotal - largerTop });
		process_events();
		expect(
			workbench.size() == outerSizeBeforeDivider
				&& workbench.left_pane()->viewport()->height()
					> viewportBeforeDivider
				&& workbench.statistics().GeometryRebuilds
					== statisticsBeforeDivider.GeometryRebuilds + 1U
				&& workbench.statistics().GeometryRebuildHistorySize
					== statisticsBeforeDivider.GeometryRebuildHistorySize + 1U
				&& (workbench.statistics()
						.GeometryRebuildHistory[workbench.statistics().GeometryRebuildHistorySize - 1U]
						.Triggers
					& qt::diff_geometry_rebuild_trigger::pane_viewport_resize)
					!= qt::diff_geometry_rebuild_trigger::none
				&& workbench.selection() == selectionBeforeMetrics,
			"detail-divider resize coalesces one attributed viewport-axis rebuild without changing selection");
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

		const auto initialRender           = workbench.grab().toImage();
		const auto originalPalette         = QApplication::palette();
		const auto originalFont            = workbench.font();
		const auto statisticsBeforePalette = workbench.statistics();
		auto       changedPalette          = originalPalette;
		changedPalette.setColor(QPalette::Window, QColor{ 31, 35, 41 });
		changedPalette.setColor(QPalette::Base, QColor{ 22, 25, 30 });
		changedPalette.setColor(QPalette::Text, QColor{ 236, 239, 244 });
		changedPalette.setColor(QPalette::WindowText, QColor{ 236, 239, 244 });
		QApplication::setPalette(changedPalette);
		process_events();
		const auto changedRender = workbench.grab().toImage();
		expect(
			workbench.selection() == selectionBeforeMetrics
				&& workbench.font() == originalFont
				&& workbench.statistics().GeometryRebuilds
					== statisticsBeforePalette.GeometryRebuilds
				&& workbench.statistics().GeometryRebuildHistorySize
					== statisticsBeforePalette.GeometryRebuildHistorySize
				&& initialRender != changedRender,
			"inherited palette transition changes pixels without rebuilding geometry "
			"or replacing font and selection");
		QApplication::setPalette(originalPalette);
		process_events();
		const auto statisticsBeforeStyle = workbench.statistics();
		auto       styleChange           = QEvent{ QEvent::StyleChange };
		QCoreApplication::sendEvent(&workbench, &styleChange);
		process_events();
		const auto  statisticsAfterStyle = workbench.statistics();
		const auto& styleCause           = statisticsAfterStyle.GeometryRebuildHistory[statisticsAfterStyle.GeometryRebuildHistorySize - 1U];
		expect(
			statisticsAfterStyle.GeometryRebuilds
					== statisticsBeforeStyle.GeometryRebuilds + 1U
				&& statisticsAfterStyle.GeometryRebuildHistorySize
					== statisticsBeforeStyle.GeometryRebuildHistorySize + 1U
				&& !statisticsAfterStyle.GeometryRebuildHistoryOverflow
				&& (styleCause.Changes
					& qt::measured_geometry_change::viewport_or_divider)
					!= qt::measured_geometry_change::none
				&& (styleCause.Triggers
					& qt::diff_geometry_rebuild_trigger::style_event)
					!= qt::diff_geometry_rebuild_trigger::none
				&& workbench.selection() == selectionBeforeMetrics,
			"style invalidation records its exact rebuild cause while preserving semantic selection");

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

	void test_semantic_selection_callback_tracks_both_panes_and_clear()
	{
		const auto [leftText, rightText] = representative_text();
		const auto diff                  = computed(leftText, rightText, { }, 53U);
		auto       workbench             = qt::diff_workbench{ };
		auto       selections
			= std::vector<std::optional<core::semantic_selection>>{ };
		workbench.set_selection_sink(
			[&selections](const std::optional<core::semantic_selection>& aSelection) {
				selections.push_back(aSelection);
			});
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		const auto selected = context(diff, 106U, 106U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();
		expect(!selections.empty() && selections.back() == workbench.selection(),
			   "accepted publication emits its reconciled semantic selection");

		const auto leftBlock = workbench.left_pane()->document()->findBlockByNumber(0);
		send_click(
			*workbench.left_pane()->viewport(),
			workbench.left_pane()->cursorRect(QTextCursor{ leftBlock }).center());
		process_events();
		expect(selections.back().has_value()
				   && selections.back()->PreferredLine.Side == core::diff_side::left
				   && selections.back() == workbench.selection(),
			   "a left-pane click emits the exact left semantic source line");

		const auto rightBlock = workbench.right_pane()->document()->findBlockByNumber(0);
		send_click(
			*workbench.right_pane()->viewport(),
			workbench.right_pane()->cursorRect(QTextCursor{ rightBlock }).center());
		process_events();
		expect(selections.back().has_value()
				   && selections.back()->PreferredLine.Side == core::diff_side::right
				   && selections.back() == workbench.selection(),
			   "a right-pane click emits the exact right semantic source line");

		workbench.clear();
		expect(!selections.back().has_value(),
			   "clearing the projection emits an absent semantic selection");
	}

	void test_scale_and_high_frequency_evidence()
	{
		const auto [leftText, rightText] = large_comparison_text(2'000U, 8U);
		const auto diff                  = computed(leftText, rightText, { }, 40U);
		expect(
			diff.Alignment.Groups.size() >= 200U,
			"dense fixture retains many separated semantic changes");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		const auto selectedContext = context(diff, 80U, 80U);
		workbench.set_loading(selectedContext);
		auto timer = QElapsedTimer{ };
		timer.start();
		workbench.publish({
			.Context = selectedContext,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();
		const auto publicationMilliseconds = timer.elapsed();
		const auto publicationStats        = workbench.statistics();
		expect(
			publicationStats.SemanticIndexBuilds == 1U
				&& publicationStats.SemanticIndexEntries
					>= diff.Alignment.Segments.size()
				&& publicationStats.GeometryRebuilds == 1U
				&& publicationStats.PaneMeasurements == 2U,
			"large publication validates/indexes semantics once and measures both panes once");

		auto* const controller = workbench.scroll_controller();
		auto* const linkMap    = workbench.link_map();
		const auto  start      = controller->maximum_position() * 0.25;
		controller->set_position(start);
		process_events();
		const auto unitStart = controller->position();
		expect(
			send_wheel(*workbench.left_pane()->viewport(), QPoint{ 0, -1 }),
			"scale probe establishes one accepted pixel-wheel unit");
		const auto unit = controller->position() - unitStart;
		expect(!close(unit, 0.0), "one pixel input retains nonzero subpixel motion");
		controller->set_position(start);
		process_events();

		const auto workbenchBefore  = workbench.statistics();
		const auto controllerBefore = controller->statistics();
		const auto linkBefore       = linkMap->statistics();
		timer.restart();
		constexpr auto inputCount = 1'000U;
		for (auto input = 0U; input < inputCount; ++input) {
			expect(
				send_wheel(*workbench.left_pane()->viewport(), QPoint{ 0, -1 }),
				"rapid pixel input remains routed through the shared controller");
		}
		const auto inputDispatchMilliseconds = timer.elapsed();
		const auto expectedPosition          = std::clamp(
			start + unit * static_cast<qreal>(inputCount),
			0.0,
			controller->maximum_position());
		expect(
			close(controller->position(), expectedPosition, 0.000'01),
			"rapid input retains the exact summed and clamped subpixel position");
		const auto beforeDrain = controller->statistics();
		timer.restart();
		process_events();
		const auto eventDrainMilliseconds = timer.elapsed();
		const auto workbenchAfter         = workbench.statistics();
		const auto controllerAfter        = controller->statistics();
		const auto linkAfter              = linkMap->statistics();
		const auto visibilityQueries      = linkAfter.VisibilityQueries
			- linkBefore.VisibilityQueries;
		const auto paintEvents = linkAfter.PaintEvents - linkBefore.PaintEvents;
		expect(
			workbenchAfter.SemanticIndexBuilds
					== workbenchBefore.SemanticIndexBuilds
				&& workbenchAfter.SemanticQueries
					== workbenchBefore.SemanticQueries
				&& workbenchAfter.GeometryRebuilds
					== workbenchBefore.GeometryRebuilds
				&& workbenchAfter.PaneMeasurements
					== workbenchBefore.PaneMeasurements,
			"ordinary rapid scroll performs no semantic query, index rebuild, axis rebuild, or pane scan");
		expect(
			controllerAfter.FramesApplied - controllerBefore.FramesApplied
					== inputCount
				&& beforeDrain.RepaintRequestsCoalesced
					> controllerBefore.RepaintRequestsCoalesced
				&& controllerAfter.RepaintDeliveries
					== controllerBefore.RepaintDeliveries + 1U,
			"rapid frames preserve motion while repaint delivery coalesces once");
		expect(
			linkAfter.RangeSearches - linkBefore.RangeSearches
					>= inputCount * 4U
				&& linkAfter.PeakCandidateEntries
					< linkAfter.RetainedChangedEntries
				&& linkAfter.PeakVisiblePaths
					<= linkAfter.PeakCandidateEntries
				&& linkAfter.CandidateEntriesVisited
						- linkBefore.CandidateEntriesVisited
					<= visibilityQueries * linkAfter.PeakCandidateEntries
				&& linkAfter.VisiblePathsBuilt - linkBefore.VisiblePathsBuilt
					<= visibilityQueries * linkAfter.PeakVisiblePaths
				&& linkAfter.PaintedPathsVisited
						- linkBefore.PaintedPathsVisited
					<= paintEvents * linkAfter.PeakVisiblePaths
				&& linkAfter.CapacityGrowths == linkBefore.CapacityGrowths,
			"every rapid frame and paint stays within peak near-visible work and grows no retained buffers");

		const auto resizeBefore    = workbench.statistics();
		const auto resizeSelection = workbench.selection();
		for (auto resize = 0U; resize < 100U; ++resize) {
			workbench.resize(
				1'180 + static_cast<int>(resize % 7U),
				740 + static_cast<int>(resize % 5U));
		}
		process_events();
		const auto resizeAfter = workbench.statistics();
		expect(
			resizeAfter.GeometryRebuilds == resizeBefore.GeometryRebuilds + 1U
				&& resizeAfter.PaneMeasurements
					== resizeBefore.PaneMeasurements + 2U,
			"resize storm coalesces to one two-pane axis rebuild");
		expect(
			resizeAfter.SemanticIndexBuilds
					== resizeBefore.SemanticIndexBuilds
				&& resizeAfter.SemanticQueries == resizeBefore.SemanticQueries
				&& workbench.selection() == resizeSelection
				&& std::isfinite(controller->position()),
			"resize rebuild performs no semantic query and preserves selection and position");
		expect(
			workbench.size() == QSize{ 1'181, 744 },
			"resize storm applies its final requested dimensions");

		auto moveOptions                 = core::diff_options{ };
		moveOptions.Moves                = core::move_detection_policy::deterministic_exact;
		const auto [moveLeft, moveRight] = dense_move_text();
		const auto moved                 = computed(moveLeft, moveRight, moveOptions, 42U);
		expect(
			moved.Alignment.Moves.size() == 6U,
			"move-enabled scale fixture retains several selected move relations");
		const auto movedContext = context(moved, 82U, 82U);
		workbench.set_loading(movedContext);
		workbench.publish({
			.Context = movedContext,
			.Result  = std::make_shared<const core::computed_file_diff>(moved),
		});
		process_events();
		expect(
			workbench.link_map()->statistics().LastMoveOverlays
				== moved.Alignment.Moves.size(),
			"move-enabled selected group resolves every visible overlay without frame sorting");

		const auto [insertionLeft, insertionRight] = large_insertion_text();
		const auto insertion                       = computed(
			insertionLeft, insertionRight, { }, 41U);
		const auto insertionContext = context(insertion, 81U, 81U);
		workbench.set_loading(insertionContext);
		workbench.publish({
			.Context = insertionContext,
			.Result  = std::make_shared<const core::computed_file_diff>(insertion),
		});
		process_events();
		expect(
			insertion.Alignment.RightLineCount
					> insertion.Alignment.LeftLineCount + 4'000U
				&& workbench.statistics().SemanticIndexBuilds == 3U
				&& workbench.link_map()->statistics().LastVisiblePaths
					<= workbench.link_map()->statistics().LastCandidateEntries,
			"very large one-sided insertion remains indexed and visible-path bounded");

		std::cout
			<< "bpr0080-scale-context platform="
			<< QSysInfo::prettyProductName().toStdString()
			<< " arch=" << QSysInfo::currentCpuArchitecture().toStdString()
			<< " qt=" << QT_VERSION_STR
			<< " threads=" << QThread::idealThreadCount()
			<< " dpr=" << workbench.devicePixelRatioF()
			<< " lines=" << diff.Alignment.LeftLineCount << '/'
			<< diff.Alignment.RightLineCount
			<< " segments=" << diff.Alignment.Segments.size()
			<< " groups=" << diff.Alignment.Groups.size()
			<< " publish_ms=" << publicationMilliseconds
			<< " input_dispatch_ms=" << inputDispatchMilliseconds
			<< " event_drain_ms=" << eventDrainMilliseconds << '\n';
	}

	/// \brief The painted diff must not grow with the file it is painting.
	auto test_line_painting_stays_bounded_by_the_viewport() -> void
	{
		// Every line differs, so an unbounded painter emits one selection per
		// line per pane. That is the shape of the defect: the widget count
		// stays at its bound and QPlainTextEdit still walks the whole
		// extra-selection list on every paint.
		// The same change interval the existing large-comparison test already
		// proves computable, so only the length varies - which is the thing
		// under test. An all-changed comparison would be a better stress but is
		// quadratic, and the core refuses it on its own matrix-cell budget.
		const auto small = large_comparison_text(200U, 8U);
		const auto large = large_comparison_text(2'000U, 8U);
		const auto lines = [](const auto& aPair) {
			return std::ranges::count(aPair.first, '\n');
		};
		expect(lines(large) >= 10 * lines(small),
			   "the large comparison really is an order of magnitude longer");

		const auto measure = [](const std::pair<std::string, std::string>& aText)
			-> std::size_t {
			const auto diff      = computed(aText.first, aText.second, { }, 40U);
			auto       workbench = qt::diff_workbench{ };
			workbench.resize(1'200, 760);
			workbench.show();
			process_events();
			// Painting only happens once the surface has been given the
			// semantic palette; a workbench without one paints nothing.
			workbench.adopt_appearance(qt::appearance_profile{ });
			const auto selected = context(diff, 80U, 80U);
			workbench.set_loading(selected);
			workbench.publish({
				.Context = selected,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			});
			process_events();
			return static_cast<std::size_t>(
				workbench.left_pane()->extraSelections().size());
		};

		const auto smallSelections = measure(small);
		const auto largeSelections = measure(large);
		expect(smallSelections > 0U,
			   "an all-changed comparison paints something at all, so the "
			   "bound below is not satisfied by painting nothing");
		// The ratio is the claim, exactly as it is for the widget count. A
		// hundredfold longer file must not cost a hundredfold more selections.
		expect(largeSelections <= 4U * smallSelections,
			   "a comparison ten times longer paints no more than a constant "
			   "factor more line backgrounds");
	}
	/// The left side of the classified fixture. Line 1 is the load-bearing
	/// one: nothing about `that continues here` reads as a comment, and only
	/// the `/*` two lines above makes it one. A pane that reasoned about the
	/// line in front of it could not colour it, so painting it proves the
	/// pane took the answer from the complete-blob classification.
	///
	/// Line 4 carries a two-byte character ahead of a span, which is where a
	/// byte offset used as a character index goes wrong by exactly one.
	constexpr std::string_view classified_left
		= "/* a long banner comment\n"
		  "   that continues here\n"
		  "   and ends here */\n"
		  "int alpha = 1;\n"
		  "const char* na\xc3\xafve = \"text\";\n"
		  "int beta = 2;\n";
	constexpr std::string_view classified_right
		= "/* a long banner comment\n"
		  "   that continues here\n"
		  "   and ends here */\n"
		  "int alpha = 1;\n"
		  "const char* na\xc3\xafve = \"text\";\n"
		  "int beta = 3;\n";

	[[nodiscard]] auto classified_spans() -> application::token_span_list
	{
		using role = core::appearance_role;
		return {
			{ .Line = 0U, .Column = 0U, .Length = 24U, .Role = role::syntax_comment },
			{ .Line = 1U, .Column = 0U, .Length = 22U, .Role = role::syntax_comment },
			{ .Line = 2U, .Column = 0U, .Length = 19U, .Role = role::syntax_comment },
			{ .Line = 3U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
			{ .Line = 4U, .Column = 0U, .Length = 5U, .Role = role::syntax_keyword },
			// Byte 21 of the line, but character 20: the `ï` ahead of it is two
			// bytes and one UTF-16 unit.
			{ .Line = 4U, .Column = 21U, .Length = 6U, .Role = role::syntax_string },
			{ .Line = 5U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
		};
	}

	[[nodiscard]] auto segment_by_id(
		const core::computed_file_diff&   aDiff,
		const core::alignment_segment_id& aId) -> const core::alignment_segment&
	{
		const auto found = std::ranges::find_if(
			aDiff.Alignment.Segments,
			[&aId](const core::alignment_segment& aSegment) {
				return aSegment.Id == aId;
			});
		expect(
			found != aDiff.Alignment.Segments.end(),
			"a move names a segment the alignment contains");
		return *found;
	}

	/// \brief The diff line band painted on one line, if any.
	///
	/// \details The **first** full-width selection on the line, not the last.
	/// Three different things are full-width - the diff band, the change-group
	/// boundary, and the reviewer's selection - and they are emitted in that
	/// order because that is the order they must composite in. Reading the last
	/// one reports the selection colour on whichever line the reviewer is on,
	/// which is what the first version of this helper did: it reported the
	/// selection highlight and made a correctly painted moved band look wrong.
	///
	/// Anything narrower is intraline emphasis and is deliberately not reported
	/// here.
	[[nodiscard]] auto band_color(QPlainTextEdit& aPane, const int aLine)
		-> QColor
	{
		for (const auto& selection : aPane.extraSelections()) {
			if (
				selection.format.boolProperty(QTextFormat::FullWidthSelection)
				&& selection.cursor.blockNumber() == aLine) {
				return selection.format.background().color();
			}
		}
		return { };
	}

	/// \brief The character range of one line's intraline emphasis.
	[[nodiscard]] auto emphasis_range(
		QPlainTextEdit& aPane,
		const int       aLine,
		const QColor&   aColor) -> std::optional<std::pair<qsizetype, qsizetype>>
	{
		for (const auto& selection : aPane.extraSelections()) {
			if (
				selection.format.boolProperty(QTextFormat::FullWidthSelection)
				|| selection.cursor.blockNumber() != aLine
				|| selection.format.background().color() != aColor) {
				continue;
			}
			const auto block = selection.cursor.block();
			return std::pair{
				static_cast<qsizetype>(
					selection.cursor.selectionStart() - block.position()),
				static_cast<qsizetype>(
					selection.cursor.selectionEnd()
					- selection.cursor.selectionStart()),
			};
		}
		return std::nullopt;
	}

	[[nodiscard]] auto block_formats(QPlainTextEdit& aPane, const int aLine)
		-> QList<QTextLayout::FormatRange>
	{
		const auto block = aPane.document()->findBlockByNumber(aLine);
		if (!block.isValid() || block.layout() == nullptr) {
			return { };
		}
		return block.layout()->formats();
	}

	/// \brief A classified blob must reach the pane's rendered formats.
	auto test_syntax_spans_colour_the_panes() -> void
	{
		const auto diff      = computed(classified_left, classified_right, { }, 44U);
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();

		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 90U, 90U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		expect(
			block_formats(*workbench.left_pane(), 1).isEmpty(),
			"a pane that was handed no classification paints no token colour, "
			"rather than inventing one");

		const auto adopted = workbench.adopt_token_spans(
			core::diff_side::left,
			std::make_shared<const application::token_span_list>(
				classified_spans()));
		expect(adopted, "an ordered classification is adopted");
		process_events();

		const auto comment      = profile.color(core::appearance_role::syntax_comment);
		const auto continuation = block_formats(*workbench.left_pane(), 1);
		expect(
			continuation.size() == 1 && continuation.front().start == 0
				&& continuation.front().length == 22
				&& continuation.front().format.foreground().color() == comment,
			"a line the complete-blob pass classified as comment is painted as "
			"one, though nothing in the line itself says so");

		const auto stringColor = profile.color(core::appearance_role::syntax_string);
		const auto literalLine = block_formats(*workbench.left_pane(), 4);
		expect(
			literalLine.size() == 2,
			"both spans of the literal line are painted");
		expect(
			literalLine.at(1).start == 20 && literalLine.at(1).length == 6
				&& literalLine.at(1).format.foreground().color() == stringColor,
			"a span behind a two-byte character starts at its character index, "
			"not at its byte offset");

		expect(
			block_formats(*workbench.left_pane(), 3).size() == 1
				&& block_formats(*workbench.right_pane(), 3).isEmpty(),
			"a classification adopted for one side colours that side alone");

		const auto visibleLinks = workbench.link_map()->visible_links();
		expect(!visibleLinks.empty(),
			   "the selected comparison exposes a semantic group to retain");
		send_click(
			*workbench.link_map(), visibleLinks.front().Path.boundingRect().center());
		process_events();
		const auto resultBeforePreview    = workbench.current_result();
		const auto contextBeforePreview   = workbench.current_context();
		const auto selectionBeforePreview = workbench.selection();
		const auto positionBeforePreview  = workbench.scroll_controller()->position();
		const auto spansBeforePreview
			= workbench.token_spans(core::diff_side::left);
		const auto statisticsBeforePreview = workbench.statistics();
		const auto profileGeneration       = profile.generation();
		auto       palettePreview          = profile.palette();
		palettePreview[core::index_of(core::appearance_role::syntax_comment)]
			= core::appearance_color{ 1U, 2U, 3U, 255U };
		expect(profile.preview_palette(palettePreview),
			   "a colour-only preview advances presentation");
		workbench.adopt_appearance(profile);
		process_events();
		const auto statisticsAfterPreview = workbench.statistics();
		expect(
			block_formats(*workbench.left_pane(), 1).front().format.foreground().color()
				== QColor{ 1, 2, 3 },
			"the selected comparison repaints from the preview palette immediately");
		expect(profile.generation() == profileGeneration + 1U
				   && statisticsAfterPreview.SyntaxBlocksFormatted
					   > statisticsBeforePreview.SyntaxBlocksFormatted,
			   "only the presentation generation and paint work advance");
		expect(workbench.current_result() == resultBeforePreview
				   && workbench.current_context() == contextBeforePreview
				   && workbench.selection() == selectionBeforePreview
				   && workbench.scroll_controller()->position()
					   == positionBeforePreview,
			   "colour preview preserves selected diff identity, semantic selection, "
			   "and comparison position");
		expect(workbench.token_spans(core::diff_side::left) == spansBeforePreview
				   && statisticsAfterPreview.TokenSpanAdoptions
					   == statisticsBeforePreview.TokenSpanAdoptions
				   && statisticsAfterPreview.SemanticIndexBuilds
					   == statisticsBeforePreview.SemanticIndexBuilds
				   && statisticsAfterPreview.SelectionChanges
					   == statisticsBeforePreview.SelectionChanges
				   && statisticsAfterPreview.GeometryRebuilds
					   == statisticsBeforePreview.GeometryRebuilds,
			   "colour preview preserves span identity and rebuilds neither semantics "
			   "nor measured geometry");

		// The whole point of keying spans by blob and definition: appearance is
		// not part of the key, so a theme change repaints and never reparses.
		const auto beforeTheme = workbench.statistics();
		auto       dark        = qt::appearance_profile{ };
		static_cast<void>(dark.select_mode(core::appearance_mode::dark));
		workbench.adopt_appearance(dark);
		process_events();
		const auto darkComment = dark.color(core::appearance_role::syntax_comment);
		const auto repainted   = block_formats(*workbench.left_pane(), 1);
		expect(
			darkComment != comment && repainted.size() == 1
				&& repainted.front().format.foreground().color() == darkComment,
			"switching theme repaints the retained classification in the new "
			"palette");
		expect(
			workbench.statistics().TokenSpanAdoptions
				== beforeTheme.TokenSpanAdoptions,
			"switching theme adopts no new classification, so nothing "
			"downstream of the pane had to reparse");

		auto unordered = classified_spans();
		std::ranges::reverse(unordered);
		const auto refused = workbench.adopt_token_spans(
			core::diff_side::left,
			std::make_shared<const application::token_span_list>(unordered));
		expect(
			!refused
				&& workbench.statistics().RejectedTokenSpans
					== beforeTheme.RejectedTokenSpans + 1U,
			"a classification that is not ordered by line is refused whole");
		process_events();
		expect(
			block_formats(*workbench.left_pane(), 1).size() == 1,
			"a refused classification leaves the adopted one painting");

		// A classification belongs to one blob, so the next comparison must not
		// inherit it.
		workbench.clear();
		process_events();
		expect(
			!workbench.token_spans(core::diff_side::left),
			"clearing the projection drops the classification with it");
	}

	/// \brief Token colouring must not grow with the file it is colouring.
	auto test_syntax_formatting_stays_bounded_by_the_viewport() -> void
	{
		using role = core::appearance_role;
		struct measurement
		{
			std::uint64_t Blocks   = 0U;
			std::uint64_t Spans    = 0U;
			std::uint64_t Segments = 0U;
		};
		const auto measure
			= [](const std::size_t aLines) -> measurement {
			const auto text      = large_comparison_text(aLines, 8U);
			const auto diff      = computed(text.first, text.second, { }, 45U);
			auto       workbench = qt::diff_workbench{ };
			workbench.resize(1'200, 760);
			workbench.show();
			process_events();
			workbench.adopt_appearance(qt::appearance_profile{ });
			const auto selected = context(diff, 91U, 91U);
			workbench.set_loading(selected);
			workbench.publish({
				.Context = selected,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			});
			process_events();

			// One span on every line, which is the worst case a classifier can
			// hand over: if the pane walked the list rather than searching it,
			// this cost would scale with the file.
			auto spans = application::token_span_list{ };
			spans.reserve(aLines);
			for (auto line = std::size_t{ 0U }; line < aLines; ++line) {
				spans.push_back({
					.Line   = static_cast<std::uint32_t>(line),
					.Column = 0U,
					.Length = 7U,
					.Role   = role::syntax_keyword,
				});
			}
			expect(
				workbench.adopt_token_spans(
					core::diff_side::left,
					std::make_shared<const application::token_span_list>(
						std::move(spans))),
				"the per-line classification is adopted");
			process_events();

			// Measured at the far end of the file rather than at the top. At
			// line zero the visible window begins where the span list begins,
			// so a painter that scanned from the front and one that searched
			// would be indistinguishable — which is exactly how a scan-shaped
			// cost hides from a measurement taken on arrival.
			auto* const bar = workbench.shared_scrollbar();
			bar->setValue(bar->maximum());
			process_events();

			// One repaint, forced by a palette the panes do not already carry.
			// A second scroll would be a weaker instrument: the window barely
			// moves, and blocks whose formats are unchanged are deliberately
			// left alone.
			const auto before = workbench.statistics();
			auto       other  = qt::appearance_profile{ };
			static_cast<void>(other.select_mode(core::appearance_mode::dark));
			workbench.adopt_appearance(other);
			process_events();
			const auto after = workbench.statistics();
			return measurement{
				.Blocks = after.SyntaxBlocksFormatted - before.SyntaxBlocksFormatted,
				.Spans  = after.SyntaxSpansVisited - before.SyntaxSpansVisited,
				.Segments
				= after.AlignmentSegmentsVisited - before.AlignmentSegmentsVisited,
			};
		};

		const auto small = measure(200U);
		const auto large = measure(2'000U);
		expect(
			small.Blocks > 0U && small.Spans > 0U && small.Segments > 0U,
			"the bounds below are not satisfied by doing nothing");
		expect(
			large.Blocks <= 4U * small.Blocks,
			"a file ten times longer formats no more than a constant factor "
			"more blocks");
		// The block count alone would be satisfied by a painter that walked
		// every span in the file to find the visible ones. This is the half of
		// the bound that a block count cannot see.
		expect(
			large.Spans <= 4U * small.Spans,
			"a file ten times longer visits no more than a constant factor "
			"more classified spans");
		// And the same again for the alignment. BPR-0196 bounded the number of
		// bands it emitted and left the scan that chose them proportional to
		// the file; the selection count could not see the difference, and
		// neither could the two assertions above.
		expect(
			large.Segments <= 4U * small.Segments,
			"a file ten times longer visits no more than a constant factor "
			"more alignment segments");
	}
	/// \brief Moved code must not read as an unrelated delete plus add.
	auto test_moved_blocks_carry_the_moved_role() -> void
	{
		auto moveOptions         = core::diff_options{ };
		moveOptions.Moves        = core::move_detection_policy::deterministic_exact;
		const auto [left, right] = dense_move_text();
		const auto diff          = computed(left, right, moveOptions, 46U);
		expect(
			!diff.Alignment.Moves.empty(),
			"the fixture really does produce move relations, so the assertions "
			"below are about detected moves and not about their absence");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 92U, 92U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		expect(
			workbench.statistics().MovedBandsPainted > 0U,
			"a comparison the core reports moves for paints moved bands");

		// Read off the pane, and compared against the model's own answer: the
		// left side of a move is a deletion segment, so a surface that ignored
		// `Moves` would paint it with the removed colour and still look
		// plausible.
		const auto movedColor
			= profile.color(core::appearance_role::diff_moved_background);
		const auto removedColor
			= profile.color(core::appearance_role::diff_removed_background);
		expect(
			movedColor != removedColor,
			"the palette distinguishes the two, so the assertion below can");

		const auto& source = segment_by_id(diff, diff.Alignment.Moves.front().SourceDeletion);
		expect(
			source.Kind == core::alignment_kind::left_only,
			"the move's source really is a deletion, which is what would be "
			"painted as an ordinary removal by a surface that read no moves");
		expect(
			band_color(*workbench.left_pane(), static_cast<int>(source.Left.Begin))
				== movedColor,
			"the source of a move is painted as moved rather than as removed");

		const auto& destination
			= segment_by_id(diff, diff.Alignment.Moves.front().DestinationInsertion);
		expect(
			band_color(
				*workbench.right_pane(),
				static_cast<int>(destination.Right.Begin))
				== movedColor,
			"the destination of a move is painted as moved rather than as added");

		// An unrelated deletion must keep the removed colour, or "moved" would
		// mean nothing.
		const auto& plain = segment_of_kind(diff, core::alignment_kind::equal);
		expect(
			!band_color(*workbench.left_pane(), static_cast<int>(plain.Left.Begin))
				 .isValid(),
			"an unchanged line stays untinted, so the moved colour is a mark "
			"and not a wash");
	}

	/// \brief Painted intraline ranges must be the ranges the core computed.
	auto test_intraline_ranges_match_the_model() -> void
	{
		// A two-byte character sits ahead of the edit on the changed line, so a
		// byte offset used as a character index is wrong by exactly one here
		// and right on every ASCII line around it.
		const auto left = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(alpha);\n"
			"context-after\n"
		};
		const auto right = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(omega);\n"
			"context-after\n"
		};
		const auto diff = computed(left, right, { }, 47U);
		expect(
			!diff.Intraline.empty(),
			"the fixture produces intraline edits, so the comparison below is "
			"against something");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 93U, 93U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		const auto emphasis
			= profile.color(core::appearance_role::diff_intraline_added_background);
		auto wash = workbench.right_pane()->palette().color(QPalette::Highlight);
		wash.setAlpha(core::selection_wash_alpha);
		auto compared = 0U;
		for (const auto& edit : diff.Intraline) {
			const auto pair = std::ranges::find_if(
				diff.Alignment.Pairs,
				[&edit](const core::line_pair& aPair) {
					return aPair.Id == edit.Pair;
				});
			expect(
				pair != diff.Alignment.Pairs.end(),
				"every intraline edit resolves through the model's own pairs");
			if (edit.Right.Begin >= edit.Right.End) {
				continue;
			}
			// The expectation is computed from the model and the document
			// bytes, not from the widget: this is the comparison the card asks
			// for, and reading the widget for both sides would compare the
			// painter with itself.
			const auto& document = diff.RightDocument;
			const auto& decoded  = document->Lines[pair->RightLine];
			const auto  prefix   = QString::fromUtf8(
				reinterpret_cast<const char*>(
					document->Bytes.data() + decoded.Content.Begin),
				static_cast<qsizetype>(edit.Right.Begin));
			const auto through = QString::fromUtf8(
				reinterpret_cast<const char*>(
					document->Bytes.data() + decoded.Content.Begin),
				static_cast<qsizetype>(edit.Right.End));
			const auto expectedStart  = prefix.size();
			const auto expectedLength = through.size() - prefix.size();
			expect(
				expectedStart
					!= static_cast<qsizetype>(edit.Right.Begin),
				"the multi-byte character really does put the character index "
				"and the byte offset apart, so this comparison can fail");

			const auto painted = emphasis_range(
				*workbench.right_pane(),
				static_cast<int>(pair->RightLine),
				emphasis);
			expect(
				painted.has_value(),
				"the changed line carries painted intraline emphasis");
			expect(
				painted->first == expectedStart
					&& painted->second == expectedLength,
				"the painted character range is the range the core computed, "
				"converted from bytes rather than assumed equal to them");
			const auto& selections    = workbench.right_pane()->extraSelections();
			const auto  washSelection = std::ranges::find_if(
				selections,
				[&wash, &pair](const QTextEdit::ExtraSelection& aSelection) {
					return aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& aSelection.cursor.blockNumber()
						== static_cast<int>(pair->RightLine)
						&& aSelection.format.background().color() == wash;
				});
			const auto emphasisSelection = std::ranges::find_if(
				selections,
				[&emphasis, &pair](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& aSelection.cursor.blockNumber()
						== static_cast<int>(pair->RightLine)
						&& aSelection.format.background().color() == emphasis;
				});
			expect(
				washSelection != selections.end()
					&& emphasisSelection != selections.end()
					&& std::distance(selections.begin(), washSelection)
						< std::distance(selections.begin(), emphasisSelection),
				"the opaque intraline role is painted after the selected-row wash");
			++compared;
		}
		expect(compared > 0U, "at least one intraline edit was compared");
	}

	/// \brief The removed side must be emphasised exactly as the added side is.
	///
	/// \details The check above reads only `right_pane`, only
	/// `diff_intraline_added_background`, and only `edit.Right`. That is how a
	/// palette in which the removed emphasis was invisible survived to an
	/// attended review: nothing ever asked the left pane whether it had been
	/// marked. BPR-0320 fixed the palette; this keeps the coverage gap from
	/// reopening.
	auto test_intraline_ranges_are_painted_on_the_removed_side() -> void
	{
		const auto left = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(alpha);\n"
			"context-after\n"
		};
		const auto right = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(omega);\n"
			"context-after\n"
		};
		const auto diff = computed(left, right, { }, 47U);
		expect(
			!diff.Intraline.empty(),
			"the fixture produces intraline edits, so the comparison below is "
			"against something");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 93U, 93U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		const auto emphasis
			= profile.color(core::appearance_role::diff_intraline_removed_background);
		const QList<QTextEdit::ExtraSelection> selections
			= workbench.left_pane()->extraSelections();
		auto compared = 0U;
		for (const auto& edit : diff.Intraline) {
			const auto pair = std::ranges::find_if(
				diff.Alignment.Pairs,
				[&edit](const core::line_pair& aPair) {
					return aPair.Id == edit.Pair;
				});
			if (pair == diff.Alignment.Pairs.end()
				|| edit.Left.Begin >= edit.Left.End) {
				continue;
			}
			const auto painted = std::ranges::any_of(
				selections,
				[&emphasis, &pair](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& aSelection.cursor.blockNumber()
						== static_cast<int>(pair->LeftLine)
						&& aSelection.format.background().color() == emphasis;
				});
			expect(
				painted,
				"every removed-side intraline range carries the removed "
				"emphasis colour");
			++compared;
		}
		expect(compared > 0U, "at least one removed-side intraline edit exists");
	}

	/// \brief Selecting a change group must wash the whole group.
	auto test_group_selection_washes_every_line() -> void
	{
		const auto [left, right] = representative_text();
		const auto diff          = computed(left, right, { }, 48U);

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 94U, 94U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		// Selecting through the ribbon, because that is the gesture that
		// produces a group selection; setting one directly would prove the
		// painter reads a field rather than that a reviewer can reach it.
		const auto visible = workbench.link_map()->visible_links();
		expect(!visible.empty(), "a ribbon is available to select");
		const auto& ribbon = visible.front();
		send_click(
			*workbench.link_map(), ribbon.Path.boundingRect().center());
		process_events();

		const auto selection = workbench.selection();
		expect(
			selection.has_value()
				&& selection->Kind == core::semantic_selection_kind::group
				&& selection->Group.has_value(),
			"clicking a ribbon selects a change group, not a line");

		const auto group = std::ranges::find(
			diff.Alignment.Groups, *selection->Group, &core::change_group::Id);
		expect(group != diff.Alignment.Groups.end(), "the group is in the alignment");
		const auto span = group->Left.End - group->Left.Begin;
		expect(
			span > 1U,
			"the selected group covers more than one line on the left, so a "
			"single-line wash and a group wash can be told apart");

		// Counted from the pane's own selections rather than from the model:
		// the claim is that the wash reaches the lines, and a model that knew
		// the span while the pane painted one row is exactly the defect.
		//
		// Matched by colour, not merely by being full width. The added and
		// removed bands are full-width selections over these same lines, so a
		// count that ignored colour would tally them and pass with the wash
		// collapsed to a single row — which an earlier draft of this test did.
		auto wash = workbench.left_pane()->palette().color(QPalette::Highlight);
		wash.setAlpha(core::selection_wash_alpha);
		const auto washed = std::ranges::count_if(
			workbench.left_pane()->extraSelections(),
			[&](const QTextEdit::ExtraSelection& aSelection) {
				const auto line = aSelection.cursor.blockNumber();
				return aSelection.format.boolProperty(
						   QTextFormat::FullWidthSelection)
					&& aSelection.format.background().color() == wash
					&& line >= static_cast<int>(group->Left.Begin)
					&& line < static_cast<int>(group->Left.End);
			});
		expect(
			static_cast<std::uint64_t>(washed) >= span,
			"every line of the selected group carries a selection band");
	}

	/// \brief A ribbon must meet the pane at the block it names.
	///
	/// \details Checked scrolled, which is where BPR-0208 lived. Two separate
	/// errors put ribbons out of place: the map drew at its own y though it is
	/// a sibling of the panes and does not share their band, and the scroll
	/// origin was a fraction of the scrollbar's travel rather than a document
	/// measurement. Both are exercised here — the first by comparing in the
	/// parent's coordinates, the second by asserting after a reveal has
	/// scrolled the panes well down the file.
	auto test_link_map_endpoints_meet_their_blocks() -> void
	{
		const auto [left, right] = representative_text();
		const auto diff          = computed(left, right, { }, 48U);

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		const auto selected = context(diff, 94U, 94U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		auto* const map    = workbench.link_map();
		auto* const pane   = workbench.left_pane();
		auto* const parent = map->parentWidget();
		expect(
			map != nullptr && pane != nullptr && parent != nullptr,
			"the composed workbench has a link map, a pane and a parent");
		expect(
			pane->verticalScrollBar()->value()
				> pane->verticalScrollBar()->minimum(),
			"the pane really is scrolled, so a document-origin error can show");

		const auto links = map->visible_links();
		expect(!links.empty(), "the comparison shows at least one ribbon");

		const auto mapTop  = map->mapTo(parent, QPoint{ 0, 0 }).y();
		const auto paneTop = pane->viewport()->mapTo(parent, QPoint{ 0, 0 }).y();
		expect(
			mapTop != paneTop,
			"the map and the pane viewport really do start at different y, so "
			"the band correction can fail");

		auto checked = 0U;
		for (const auto& link : links) {
			const auto segment = std::ranges::find_if(
				diff.Alignment.Segments,
				[&link](const core::alignment_segment& aSegment) {
					return aSegment.Id == link.Identity.Segment;
				});
			if (segment == diff.Alignment.Segments.end()) {
				continue;
			}
			const auto block = pane->document()->findBlockByNumber(
				static_cast<int>(segment->Left.Begin));
			if (!block.isValid()) {
				continue;
			}
			const auto blockTop = pane->cursorRect(QTextCursor{ block }).top()
				+ paneTop;
			if (blockTop < paneTop
				|| blockTop > paneTop + pane->viewport()->height()) {
				continue;
			}
			const auto ribbonTop = qRound(link.LeftTop) + mapTop;
			// Exact, not approximate. Both quantities are integers derived
			// from the same per-line measurement, and measured at 100, 125 and
			// 200 per cent the difference is zero rather than small. A
			// tolerance would have to be two pixels wide to admit the band
			// error this test exists to catch, which would make it admit the
			// error rather than catch it.
			expect(
				ribbonTop == blockTop,
				"a ribbon meets the pane at the top of the block it names");
			++checked;
		}
		expect(checked > 0U, "at least one visible ribbon was checked");
	}

	/// \brief Change-group boundaries must be marked where the model puts them.
	auto test_group_boundaries_are_painted() -> void
	{
		const auto [left, right] = representative_text();
		const auto diff          = computed(left, right, { }, 48U);
		expect(
			!diff.Alignment.Groups.empty(),
			"the fixture produces change groups");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 94U, 94U);
		workbench.set_loading(selected);
		workbench.publish({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		});
		process_events();

		auto& leftPane = *workbench.left_pane();

		// The boundary is a drawn rule, so the pane has to have painted for
		// there to be anything to assert. Grabbing the viewport is what makes
		// that happen, and it is also the only place the rule exists — it is
		// deliberately not an extra selection.
		const auto pixels = leftPane.viewport()->grab().toImage();
		expect(
			workbench.statistics().GroupBoundariesPainted > 0U,
			"a comparison with change groups draws at least one boundary rule");

		const auto rule
			= profile.color(core::appearance_role::diff_group_boundary_rule);
		expect(rule.isValid(), "the profile supplies a boundary rule colour");

		// A boundary belongs on the edge above the group's first line. Find a
		// group the viewport is actually showing and check the pixels there.
		auto checked = false;
		for (const auto& group : diff.Alignment.Groups) {
			if (group.First.Left == 0U) {
				continue;
			}
			const auto block = leftPane.document()->findBlockByNumber(
				static_cast<int>(group.First.Left));
			if (!block.isValid()) {
				continue;
			}
			// cursorRect is in logical pixels and the grab is in device ones,
			// so the two only agree at a device pixel ratio of one. The scaled
			// courses run at 1.25 and 2.0, where reading the image at a logical
			// y lands on a different line entirely.
			const auto ratio = pixels.devicePixelRatio();
			const auto top   = qRound(
				leftPane.cursorRect(QTextCursor{ block }).top() * ratio);
			if (top <= 0 || top >= pixels.height()) {
				continue;
			}
			// The pen is BoundaryRuleWidth logical pixels, so the run is that
			// many device rows thick and the search widens with the ratio.
			const auto reach = qRound(2.0 * ratio) + 1;
			auto       found = false;
			for (auto offset = -reach; offset <= reach && !found; ++offset) {
				const auto y = top + offset;
				if (y < 0 || y >= pixels.height()) {
					continue;
				}
				auto run = 0;
				for (auto x = 0; x < pixels.width(); ++x) {
					if (QColor{ pixels.pixelColor(x, y) } == rule) {
						++run;
					}
				}
				found = run > pixels.width() / 2;
			}
			expect(
				found,
				"the edge above a change group's first line carries a rule in "
				"the boundary colour, spanning the pane");

			// The defect BPR-0202 repaired: the unchanged line above the group
			// used to be filled edge to edge, in the same vocabulary the added
			// and removed bands use, to say something that was not true of it.
			expect(
				!band_color(leftPane, static_cast<int>(group.First.Left - 1U))
					 .isValid(),
				"the unchanged line above a change group carries no band of "
				"its own");
			checked = true;
			break;
		}
		expect(checked, "at least one change group boundary was visible to check");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(QStringLiteral("BetterPullReviewDiffWorkbenchTests"));
	test_selected_composition();
	test_semantic_selection_callback_tracks_both_panes_and_clear();
	test_scale_and_high_frequency_evidence();
	test_line_painting_stays_bounded_by_the_viewport();
	test_syntax_spans_colour_the_panes();
	test_syntax_formatting_stays_bounded_by_the_viewport();
	test_moved_blocks_carry_the_moved_role();
	test_intraline_ranges_match_the_model();
	test_intraline_ranges_are_painted_on_the_removed_side();
	test_group_boundaries_are_painted();
	test_link_map_endpoints_meet_their_blocks();
	test_group_selection_washes_every_line();
	std::cout << "diff-workbench-tests:pass\n";
	return EXIT_SUCCESS;
}
