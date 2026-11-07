#include "bpr/app/github_application.hpp"
#include "bpr/adapters/process_support/process_supervision.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/storage_diagnostics.hpp"
#include "bpr/adapters/workspace/clone_folder_scan.hpp"
#include "bpr/app/diagnostic_controls.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/release_maintenance_controls.hpp"
#include "bpr/application/credential_vault_diagnostics.hpp"
#include "bpr/application/host_operation_diagnostics.hpp"
#include "bpr/application/insights_host.hpp"
#include "bpr/application/markdown_blob_snippet_host.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/application/markdown_image_service.hpp"
#include "bpr/application/remembered_connection_host.hpp"
#include "bpr/application/review_mutation_diagnostics.hpp"
#include "bpr/application/review_tour_host.hpp"
#include "bpr/application/review_tour_import.hpp"
#include "bpr/core/agent_comment_suggestion.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/conversation_mutation_lifecycle.hpp"
#include "bpr/core/conversation_reply.hpp"
#include "bpr/core/conversation_timeline.hpp"
#include "bpr/core/description_edit.hpp"
#include "bpr/core/draft_state.hpp"
#include "bpr/core/hex.hpp"
#include "bpr/core/pull_request_suggestion.hpp"
#include "bpr/core/review_role.hpp"
#include "bpr/core/reviewer_request.hpp"
#include "bpr/logging/quill_log.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/event_loop_metrics.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/insights_window.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/markdown_image_decoder.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/suggestion_mark.hpp"
#include "github_application/agent_composition.hpp"
#include "github_application/blob_sources.hpp"
#include "github_application/cli_accounts.hpp"
#include "github_application/credential_prompt.hpp"
#include "github_application/defect_identity.hpp"
#include "github_application/device_authorization_composition.hpp"
#include "github_application/fixture_course_support.hpp"
#include "github_application/fixture_uiforetw_observation.hpp"
#include "github_application/github_presentation_projections.hpp"
#include "github_application/insights_metrics.hpp"
#include "github_application/profile_hosts.hpp"
#include "github_application/profile_runtime.hpp"
#include "github_application/public_preview_wire_transport.hpp"
#include "github_application/queue_notifications.hpp"
#include "github_application/review_mutation_selection.hpp"
#include "github_application/session_settings.hpp"
#include "github_application/startup_failures.hpp"
#include "github_application/theme_document_file.hpp"
#include "github_application/window_composition.hpp"
#include "github_application/window_probe_states.hpp"
#include "github_application/window_review_lanes.hpp"
#include "github_application/workbench_composition.hpp"
#include "github_cli_runtime.hpp"
#include "github_release_update.hpp"
#include "pane_find_controller.hpp"
#include "platform_credential_vault.hpp"
#include "pr_search_controller.hpp"
#include "profile_window_layout_port.hpp"
#include "terminal_diagnostics.hpp"
#include "tour_observation.hpp"
#include <QCryptographicHash>

#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "live_review_submission_provider.hpp"
#include "private_review_workflow.hpp"
#include "review_cycle_desktop.hpp"
#include "review_mutation_execution_authority.hpp"
#include "review_submission_execution_authority.hpp"
#include "review_submission_workflow.hpp"
#include "system_browser_opener.hpp"

#include "bpr/app/worker_executor.hpp"
#include "bpr/application/github_read_admission.hpp"
#include "bpr/application/home_open_host.hpp"

#include "designated_review_public_observer.hpp"
#include "gh_cli_git_smart_transport.hpp"
#include "gh_cli_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_device_authorization_transport.hpp"
#include "github_host_profile.hpp"
#include "github_provider.hpp"
#include "live_review_mutation_provider.hpp"
#include "native_https_transport.hpp"
#include "platform_body_digest.hpp"
#include "review_comment_permalink.hpp"

#if defined(_WIN32)
	#include "windows_title_row_host.hpp"
#endif

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/appearance_preferences_application_host.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/profile_epoch_actor_registry.hpp"
	#include "bpr/application/workbench_preferences_host.hpp"
	#include "bpr/qt/destructive_question.hpp"
	#include "bpr/qt/settings_window.hpp"
	#include "github_public_path_fixture.hpp"
	#include "github_read_contract.hpp"
	#include "github_wire_transport.hpp"
	#include "review_submission_fixture.hpp"
	#include "startup_diagnostic.hpp"
	#include <QMenuBar>
	#include <QPainter>
	#include <QStatusBar>
	#include <glaze/json.hpp>
	#include <qpa/qwindowsysteminterface.h>
#endif

// [[BPR-1179]]. BPR_AGENT_INTEGRATIONS and BPR_LOCAL_AUTOMATION are defined
// only in a graph that builds those component families. Every block they
// guard here composes, drives or reports one of them; without it the window
// withdraws the agent controls and no owner command endpoint is bound, and
// nothing else in this composition changes.
#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/adapters/agent/agent_process_launcher.hpp"
	#include "bpr/adapters/mcp/mcp_context_helper.hpp"
	#include "bpr/application/agent_conversation_host.hpp"
	#include "bpr/application/agent_program_resolution.hpp"
	#include "bpr/application/agent_setup_host.hpp"
	#include "bpr/application/claude_process_wrapper.hpp"
	#include "bpr/application/clone_link_host.hpp"
	#include "bpr/application/review_brief.hpp"
	#include "bpr/application/review_skill_host.hpp"
	#include "bpr/qt/agent_launch_panel.hpp"
	#include "bpr/qt/agent_tab_view.hpp"
#endif
#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/profile_activation.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/profile_startup_admission.hpp"
#include "bpr/application/pull_request_link_location.hpp"
#include "bpr/application/setup_destination.hpp"
#include "bpr/application/tour_agent_drafting.hpp"
#include "bpr/application/window_review_restoration_host.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#if defined(BPR_LOCAL_AUTOMATION)
	#include "cycle_owner_service.hpp"
#endif
#include "tour_owner_service.hpp"

#include "bpr/adapters/sqlite/cached_review_overview_store.hpp"
#include "bpr/adapters/sqlite/markdown_image_disk_cache.hpp"
#include "bpr/adapters/sqlite/pull_request_activity_store.hpp"
#include "bpr/adapters/sqlite/view_filter_memory_store.hpp"
#include "bpr/application/pull_request_activity_host.hpp"
#include "bpr/application/view_filter_memory_host.hpp"
#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/agent_chat_session_host.hpp"
	#include "bpr/application/agent_comment_workshop_host.hpp"
	#include "bpr/application/agent_finding_host.hpp"
	#include "bpr/application/agent_invocation_host.hpp"
	#include "bpr/application/agent_realignment.hpp"
	#include "bpr/application/agent_run_diagnosis.hpp"
	#include "bpr/application/mcp_context_helper_supervisor.hpp"
#endif
#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/commit_filter_model.hpp"
#include "bpr/application/defect_correlation.hpp"
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/document_tab_host.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/feedback_counters.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/observing_review_ledger_store.hpp"
#include "bpr/application/overview_feedback.hpp"
#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/application/pull_request_overview.hpp"
#include "bpr/application/reference_page.hpp"
#include "bpr/application/reply_thread_observation.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/review_queue_facts.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/application/workbench_snapshot_observation.hpp"
#include "bpr/core/github_transition.hpp"
#include "bpr/core/home_open.hpp"
#include "bpr/core/markdown_code.hpp"
#include "bpr/core/pull_request_activity.hpp"
#include "bpr/core/refresh_feedback.hpp"
#include "bpr/core/reply_draft_standing.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/event_loop_device_timer.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/motion_observer.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/pull_request_activity_notifications.hpp"
#include "bpr/qt/sensitive_text_input.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainterPathStroker>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFormat>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::app {
	// [[BPR-0802]]. From the environment rather than a flag because a flag is
	// something a course can forget, and the thing it would forget is the one
	// that keeps an unattended suite from opening a browser on the reviewer's
	// machine. `bpr_add_network_isolated_test` injects it into every
	// registered row on all three platforms, and a child the course starts
	// inherits it.
	//
	// Any non-empty value means inside. The failure mode has to be the safe
	// one: an unrecognised value that meant "outside" would be a suite allowed
	// to start a program, while one that means "inside" is at worst a person's
	// own run composing a recorder, which they would see immediately.
	//
	// [[BPR-0983]]. Read only in the tests-enabled graph. The name is in no
	// tests-disabled binary, and the tests-off Release scan in
	// `tests/end_to_end/composition_contract_tests.py` looks for it.
	auto inside_automated_test_boundary() -> bool
	{
#if defined(BPR_BUILD_TESTING)
		return !qEnvironmentVariableIsEmpty("BPR_AUTOMATED_TEST_BOUNDARY");
#else
		return false;
#endif
	}

	namespace {
		using adapters::github::github_host_profile_candidate;
		using adapters::github::github_profile_provenance;
		using adapters::github::make_live_review_mutation_provider;
		using adapters::github::validate_github_host_profile;
		using adapters::github::validated_github_host_profile;
		using adapters::github::detail::github_provider;
		using adapters::github::detail::github_wire_transport_factory_port;
		using adapters::github::detail::make_github_provider;
		using adapters::github::detail::native_github_wire_transport_factory;

	}

#if defined(BPR_AGENT_INTEGRATIONS)
	static_assert(
		qt::finding_queue_action_count == application::agent_finding_action_count,
		"the composed finding vocabulary and the queue's must stay one list");
#endif

	void github_application::implementation::compose_review_mutation(
		const application::accepted_pull_request_handoff& aHandoff)
	{
		// A fixture retry waiting for a draining host may already have been
		// superseded by another accepted read. Never compose that old context.
		if (!LastAcceptedHandoff || workbench_authority(aHandoff) != workbench_authority(*LastAcceptedHandoff)) {
			return;
		}
		const auto binding = WindowRead && WindowRead->accepted_snapshot()
			? select_observed_review_mutation(aHandoff, *WindowRead->accepted_snapshot(), true)
			: std::nullopt;
		const auto scope   = core::conversation_mutation_scope{
			.Session            = aHandoff.ApplicationSession,
			.Profile            = aHandoff.Profile,
			.ProfileRevision    = aHandoff.ProfileRevision,
			.Host               = aHandoff.Host,
			.HostAuthority      = aHandoff.HostAuthority,
			.Account            = aHandoff.Account,
			.Viewer             = aHandoff.Viewer,
			.IdentityGeneration = aHandoff.IdentityGeneration,
			.Repository         = aHandoff.Repository,
			.PullRequest        = aHandoff.PullRequest,
			.Number             = aHandoff.Number,
			.Base               = application::recorded_base(aHandoff),
			.Head               = aHandoff.Head,
			.Mode               = aHandoff.Mode,
		};
		const auto bindingCurrent = binding && ConversationMutationBinding
			&& binding->Target == ConversationMutationBinding->Target
			&& binding->ActingIdentity == ConversationMutationBinding->ActingIdentity;
		const auto retirement       = core::adopt_conversation_mutations(ConversationMutations, ConversationOverlay, scope, bindingCurrent);
		ConversationMutationBinding = binding;
		// [[BPR-1477]]. Retired with the composition below, and needed to
		// say again why a held comment's outcome is unknown.
		const auto heldRetry = RetryableReviewDraft;
		retire_review_mutation_host(retirement);
		InlineReviewBinding.reset();
		if (Window != nullptr && Window->review_editor() != nullptr) {
			Window->review_editor()->present(
				qt::review_draft_editor_clear_reason::composition_retired);
		}
		// [[BPR-1374]]. The reply being written is held again once its
		// line is chosen under the new composition; before, a Refresh
		// left it saved but shown nowhere.
		if (ReviewDraft) {
			ResumeReply = ReviewDraft->ReplyToThread && ReviewDraft->State == core::review_draft_state::drafting ? std::optional{ ReviewDraft->Id } : std::nullopt;
			// [[BPR-1475]]. So is a new comment being written: the window
			// rereads by itself after publishing, and that takes no note away.
			// [[BPR-1477]]. And one whose publication answer was lost, with
			// its retry: before, a Refresh left it saved, locked and shown
			// nowhere, and Retry was gone.
			ResumeComment        = core::resume_comment_after_refresh(std::span{ &*ReviewDraft, 1U }, ReviewDraft->Id);
			ResumeCommentOpen    = ResumeComment && DraftOpenAtRead;
			ResumeCommentFailure = heldRetry && heldRetry->Draft.Id == ReviewDraft->Id ? heldRetry->Failure : application::review_mutation_failure::unavailable;
		}
		DraftOpenAtRead = false;
		ReviewDraft.reset();
		ReviewDraftRevision = 0U;
		SelectedSemanticSelection.reset();
		refresh_agent_private_context_choices();
		publish_preflight_selection();
		ReviewProjection.clear();
		// [[BPR-1824]]. Until a write connection is composed below the
		// cards say why Resolve cannot go, not that it is "unavailable".
		if (Window != nullptr) {
			Window->set_thread_resolution_connection(thread_resolution_connection_refusal(), false);
		}
#if defined(BPR_BUILD_TESTING)
		if (Fixture == nullptr || Window == nullptr
			|| Window->review_editor() == nullptr || WorkbenchHost == nullptr
			|| Host == nullptr || !WindowRead->accepted_snapshot().has_value()) {
			return;
		}
		if (!RetiredReviewMutationHosts.empty()) {
			auto retry = std::make_shared<application::accepted_pull_request_handoff>(
				clone_without_smart_git(aHandoff));
			QTimer::singleShot(0, Window.get(), [this, retry] {
				if (!Closing) {
					compose_review_mutation(*retry);
				}
			});
			return;
		}
		auto selection = select_observed_review_mutation(
			aHandoff, *WindowRead->accepted_snapshot(), true);
		if (!selection.has_value()) {
			return;
		}
		InlineReviewBinding = *selection;
		const auto target   = selection->Target.Semantic;
		Fixture->prepare_review_mutation(target);
		Fixture->set_private_lineage(PrivateReview);
		if (ReviewMutationDecisionGate == nullptr) {
			ReviewMutationDecisionGate = std::make_unique<
				gated_review_mutation_decision_executor>(DecisionLane);
		}
		ReviewMutationHost = std::make_unique<
			application::review_mutation_application_host>(
			Fixture->review_mutation_provider(),
			selection->Target,
			selection->ActingIdentity,
			aHandoff.ApplicationSession,
			WorkbenchHost->generation(),
			application::review_mutation_execution_grant{ },
			ReviewMutationEffects,
			*ReviewMutationDecisionGate,
			*this,
			this, insights_capture(aHandoff));
		FixtureReviewMutationHostComposed = true;
#else
		if (Host == nullptr || WorkbenchHost == nullptr
			|| !WindowRead->accepted_snapshot().has_value()) {
			return;
		}
		auto selection = AttendedReviewMutationMode
			? select_designated_review_mutation(aHandoff, *WindowRead->accepted_snapshot())
			: select_observed_review_mutation(aHandoff, *WindowRead->accepted_snapshot(), true);
		if (!selection.has_value()) {
			LiveReviewMutationGrant.reset();
			return;
		}
		if (AttendedReviewMutationMode && LiveReviewMutationGrant == nullptr) {
			return;
		}
		auto lease = WindowRead->mutation_transport();
		if (lease == nullptr) {
			return;
		}
		const auto repository = selection->Target.Repository.Owner + "/"
			+ selection->Target.Repository.Name;
		auto recovery = adapters::sqlite::make_review_mutation_recovery_store({
			.StorageRoot = ApplicationDataRoot,
			.Host        = std::string{ selection->Target.Hostname.value() },
			.Account     = std::to_string(selection->ActingIdentity.Account.value()),
			.Repository  = repository,
			.PullRequest = static_cast<std::int64_t>(
				selection->Target.Number.value()),
		});
		if (!recovery.has_value()) {
			LiveReviewMutationGrant.reset();
			return;
		}
		auto provider = make_live_review_mutation_provider(
			std::move(lease),
			selection->Target,
			selection->ActingIdentity,
			std::move(*recovery), PrivateReview.get(), DiagnosticLog);
		if (provider.has_value()) {
			InlineReviewBinding = *selection;
			auto erased         = std::unique_ptr<application::review_mutation_port>{
				std::move(*provider)
			};
			// [[BPR-1828]]. Every resolve, reaction, edit, reviewer
			// request and publication leaves a github.review_mutation record.
			erased             = application::with_review_mutation_diagnostics(std::move(erased), DiagnosticLog);
			ReviewMutationHost = std::make_unique<
				application::review_mutation_application_host>(
				std::move(erased),
				std::move(selection->Target),
				std::move(selection->ActingIdentity),
				aHandoff.ApplicationSession,
				WorkbenchHost->generation(),
				std::move(LiveReviewMutationGrant),
				ReviewMutationEffects,
				DecisionLane,
				*this,
				this, insights_capture(aHandoff));
			ReviewMutationHost->repair_pending_publications();
		} else {
			LiveReviewMutationGrant.reset();
		}
#endif
		if (ReviewMutationHost == nullptr || Window == nullptr
			|| Window->review_editor() == nullptr) {
			return;
		}
		// [[BPR-1826]]. A closed or merged pull request's connection
		// resolves and unresolves threads and nothing else: its host
		// offers no other capability, and no draft is published from it.
		if (InlineReviewBinding && !InlineReviewBinding->Target.Open) {
			Window->set_thread_resolution_provider(ReviewMutationHost->capabilities().ResolvesThreads);
			present_reviewer_offer();
			present_description_offer();
			present_comment_edit_offer();
			present_draft_state_offer();
			return;
		}
		if (!ensure_shared_drafts(aHandoff)) {
			return;
		}
		auto* const editor = Window->review_editor();
		editor->describe_target(QStringLiteral("%1 · %2/%3#%4 at %5 as %6")
									.arg(QString::fromStdString(std::string{ aHandoff.Hostname.value() }))
									.arg(QString::fromStdString(aHandoff.Locator.Owner))
									.arg(QString::fromStdString(aHandoff.Locator.Name))
									.arg(aHandoff.Number.value())
									.arg(QString::fromUtf8(
										core::canonical_hex(aHandoff.Head).view()))
									.arg(QString::fromStdString(WindowRead->accepted_snapshot()->Viewer.Login)));
		editor->present(ReviewMutationHost->capabilities());
		// [[BPR-1741]]. The GitHub CLI connection resolves threads
		// too, through `gh api graphql`.
		Window->set_thread_resolution_provider(ReviewMutationHost->capabilities().ResolvesThreads);
		present_reviewer_offer();
		present_description_offer();
		present_comment_edit_offer();
		present_draft_state_offer();
	}

	/// [[BPR-1741]]. GitHub's word on what the viewer may do to a
	/// thread, from the read; unknown is not a refusal.
	auto github_application::implementation::thread_permission(const application::github_review_thread_record& aThread) noexcept -> core::thread_resolution_permission
	{
		return {
			.MayResolve   = aThread.ViewerResolvePermission != application::github_permission_state::denied,
			.MayUnresolve = aThread.ViewerUnresolvePermission != application::github_permission_state::denied,
		};
	}

	/// [[BPR-1741]]. The posts' reactions and the threads'
	/// resolutions as the viewer should see them: the accepted read, with
	/// the viewer's own changes the core keeps until GitHub and a later
	/// read agree with them.
	void github_application::implementation::present_conversation_state()
	{
		if (Closing || Window == nullptr || !WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto& snapshot  = *WindowRead->accepted_snapshot();
		const auto  viewer    = snapshot.Viewer.Login;
		auto        reactions = std::map<std::string, core::reaction_summary>{ };
		const auto  read      = snapshot.ReactionsAvailability == application::github_reaction_availability::available
			|| snapshot.ReactionsAvailability == application::github_reaction_availability::partial;
		if (read) {
			for (const auto& record : snapshot.Reactions) {
				if (auto shown = core::project_reaction_summary(&record.Reactions, ConversationOverlay, record.Subject, viewer)) {
					reactions.insert_or_assign(record.Subject, std::move(*shown));
				}
			}
		}
		for (const auto& [subject, answered] : ConversationOverlay.Answered) {
			if (!reactions.contains(subject)) {
				if (auto shown = core::project_reaction_summary(nullptr, ConversationOverlay, subject, viewer)) {
					reactions.insert_or_assign(subject, std::move(*shown));
				}
			}
		}
		auto resolutions = std::map<std::string, qt::thread_resolution_presentation>{ };
		for (const auto& thread : snapshot.ReviewThreads) {
			const auto identity  = thread_identity(thread);
			const auto projected = core::project_thread_resolution(thread.Resolved, ConversationOverlay, identity);
			// [[BPR-1841]]. No re-read follows a resolve, so the read's
			// permission is projected with its state.
			const auto permitted = core::project_thread_resolution_permission(thread.Resolved, projected.Resolved, thread_permission(thread));
			resolutions.insert_or_assign(identity, qt::thread_resolution_presentation{
													   .Resolved     = projected.Resolved,
													   .Pending      = projected.Pending,
													   .CanResolve   = permitted.MayResolve,
													   .CanUnresolve = permitted.MayUnresolve,
												   });
		}
		Window->present_conversation_state(std::move(reactions), std::move(resolutions));
		present_overview_resolutions();
	}

	/// [[BPR-1741]]. A new accepted read reconciles the viewer's
	/// changes with what GitHub now says; another pull request drops them.
	void github_application::implementation::observe_conversation_state()
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto& snapshot          = *WindowRead->accepted_snapshot();
		auto        observation       = core::conversation_read_observation{ };
		observation.ReactionsObserved = snapshot.ReactionsAvailability == application::github_reaction_availability::available;
		for (const auto& record : snapshot.Reactions) {
			observation.Reactions.insert_or_assign(record.Subject, record.Reactions);
		}
		for (const auto& thread : snapshot.ReviewThreads) {
			observation.Resolved.insert_or_assign(thread_identity(thread), thread.Resolved);
		}
		core::observe_conversation(ConversationOverlay, observation);
	}

	/// [[BPR-1741]]. A post's reaction: the core decides from the
	/// session's anonymity, what was read of the post and GitHub's
	/// permission, and whether the same change is still unanswered; the
	/// count changes at once, and the host writes it under a one-shot grant.
	void github_application::implementation::react_to_post(const qt::reaction_gesture& aGesture)
	{
		if (Closing || Window == nullptr || aGesture.Subject.empty()) {
			return;
		}
		auto* const panel  = Window->conversation_panel();
		const auto  notice = [panel](const QString& aText) {
			if (panel != nullptr) {
				panel->present_notice(aText);
			}
		};
		const auto anonymous = Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
		const auto refusal = core::decide_reaction(core::reaction_input{
			.AnonymousRead  = anonymous,
			.Observed       = aGesture.Observed,
			.ViewerCanReact = aGesture.ViewerCanReact,
			.InFlight       = core::reaction_in_flight(ConversationOverlay, aGesture.Subject, aGesture.Content),
		});
		++ReactionGestures;
		switch (refusal) {
		case core::reaction_refusal::anonymous_read:
			notice(QStringLiteral("Sign in to react; a public preview only reads."));
			return;
		case core::reaction_refusal::unobserved:
			notice(QStringLiteral("Reactions on this comment have not been read yet; refresh and try again."));
			return;
		case core::reaction_refusal::not_permitted:
			notice(QStringLiteral("GitHub does not let you react to this comment."));
			return;
		case core::reaction_refusal::in_flight:
			notice(QStringLiteral("Waiting for GitHub to answer the last change to this reaction."));
			return;
		case core::reaction_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().Reacts) {
			notice(QStringLiteral("Reactions are unavailable for this pull request."));
			return;
		}
		if (!core::begin_reaction_change(ConversationOverlay, aGesture.Subject, aGesture.Content, aGesture.Add)) {
			return;
		}
		present_conversation_state();
		auto request = application::review_reaction_publication_request{
			.Subject = aGesture.Subject,
			.Content = aGesture.Content,
			.Add     = aGesture.Add,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_reaction(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		const auto context = ReviewMutationHost->react(std::move(request), std::move(grant));
		if (!core::retain_conversation_mutation(ConversationMutations, { context.Request, context.Session, context.Generation },
												{ core::conversation_mutation_kind::reaction, aGesture.Subject, aGesture.Content, aGesture.Add })) {
			core::abandon_reaction_change(ConversationOverlay, aGesture.Subject, aGesture.Content);
			present_conversation_state();
		}
	}

	void github_application::implementation::publish_reaction(const application::review_reaction_terminal& aTerminal)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (!core::settle_conversation_mutation(ConversationMutations, ConversationOverlay,
												{ aTerminal.Context.Request, aTerminal.Context.Session, aTerminal.Context.Generation },
												{ core::conversation_mutation_kind::reaction, aTerminal.Request.Subject, aTerminal.Request.Content, aTerminal.Request.Add },
												aTerminal.Result.has_value(), aTerminal.Result ? std::optional{ aTerminal.Result->Reactions } : std::nullopt)) {
			return;
		}
		if (aTerminal.Result.has_value()) {
			++ReactionCompletions;
		} else {
			++ReactionFailures;
			if (auto* const panel = Window->conversation_panel(); panel != nullptr) {
				const auto words = QString::fromUtf8(core::reaction_label(aTerminal.Request.Content).data(),
													 static_cast<qsizetype>(core::reaction_label(aTerminal.Request.Content).size()));
				panel->present_notice(aTerminal.Result.error() == application::review_mutation_failure::not_confirmed
										  ? QStringLiteral("GitHub does not show the %1 reaction; nothing was sent twice. Try again.").arg(words)
										  : QStringLiteral("The %1 reaction could not be %2: %3.")
												.arg(words, aTerminal.Request.Add ? QStringLiteral("added") : QStringLiteral("removed"),
													 QString::fromUtf8(application::name(aTerminal.Result.error()))));
			}
		}
		present_conversation_state();
	}

	/// [[BPR-1774]]. The signed-in login and the pull request's author
	/// as the accepted read gave them; empty when either is unknown or the
	/// session reads anonymously.
	auto github_application::implementation::description_edit_logins() const -> std::pair<std::string, std::string>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return { };
		}
		const auto& snapshot  = *WindowRead->accepted_snapshot();
		const auto  anonymous = Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
		if (anonymous || !snapshot.PullRequest.Author.has_value()) {
			return { };
		}
		return { snapshot.PullRequest.Author->Login, snapshot.Viewer.Login };
	}

	/// [[BPR-1774]]. Edit on the description is offered to the pull
	/// request's author, the core decides, when the connection in use can
	/// write it; everyone else sees nothing new.
	void github_application::implementation::present_description_offer()
	{
		if (Closing || Window == nullptr || Window->overview_page() == nullptr) {
			return;
		}
		const auto [author, viewer] = description_edit_logins();
		Window->overview_page()->set_description_editable(core::description_edit_offered(author, viewer) && ReviewMutationHost != nullptr
														  && ReviewMutationHost->capabilities().EditsDescription);
	}

	/// [[BPR-1774]]. Save in the description editor: the core decides on
	/// the gesture (the author, a change, within GitHub's length), the host
	/// writes once under a one-shot grant after GitHub, asked first, still
	/// has the original. Nothing is sent before Save, and a refusal is said
	/// beside the editor.
	void github_application::implementation::save_description(const qt::overview_description_save& aSave)
	{
		auto* const page = Window != nullptr ? Window->overview_page() : nullptr;
		if (Closing || page == nullptr) {
			return;
		}
		++DescriptionEditGestures;
		const auto [author, viewer] = description_edit_logins();
		const auto refusal          = core::decide_description_edit({ .Author = author, .Viewer = viewer, .Original = aSave.Original, .Edited = aSave.Body });
		switch (refusal) {
		case core::description_edit_refusal::not_author:
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("Only the pull request's author can edit its description here."));
			return;
		case core::description_edit_refusal::unchanged:
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("Nothing has changed, so there is nothing to save."));
			return;
		case core::description_edit_refusal::too_long:
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("GitHub keeps at most 65,536 characters; this description has %1.")
												 .arg(QLocale{ }.toString(static_cast<qulonglong>(core::description_characters(aSave.Body)))));
			return;
		case core::description_edit_refusal::not_permitted:
		case core::description_edit_refusal::conflict:
		case core::description_edit_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().EditsDescription) {
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("This connection cannot edit the description."));
			return;
		}
		if (PendingDescriptionEdit.has_value()) {
			page->present_description_status(QStringLiteral("Waiting for GitHub to answer the last save."), true);
			return;
		}
		auto request = application::review_description_publication_request{ .Original = aSave.Original, .Body = aSave.Body };
		auto grant   = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_description(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		page->present_description_status(QStringLiteral("Saving the description to GitHub…"), true);
		PendingDescriptionEdit = ReviewMutationHost->edit_description(std::move(request), std::move(grant));
	}

	void github_application::implementation::publish_description(const application::review_description_terminal& aTerminal)
	{
		if (Closing || Window == nullptr || PendingDescriptionEdit != std::optional{ aTerminal.Context }) {
			return;
		}
		PendingDescriptionEdit.reset();
		auto* const page = Window->overview_page();
		if (page == nullptr) {
			return;
		}
		if (!aTerminal.Result.has_value()) {
			++DescriptionEditFailures;
			auto words = QString{ };
			switch (aTerminal.Result.error()) {
			case application::review_mutation_failure::refused_by_server:
				words = QStringLiteral("GitHub does not let you edit this description. Nothing was saved.");
				break;
			case application::review_mutation_failure::not_confirmed:
				words = QStringLiteral("GitHub does not show the new description; it was not sent twice. Check the pull request, then save again.");
				break;
			case application::review_mutation_failure::incompatible_body:
				words = QStringLiteral("GitHub cannot take this text. Nothing was saved.");
				break;
			case application::review_mutation_failure::target_moved:
			case application::review_mutation_failure::undesignated_target:
			case application::review_mutation_failure::target_unavailable:
				words = QStringLiteral("The pull request changed on GitHub, so nothing was saved. Refresh and try again.");
				break;
			case application::review_mutation_failure::cancelled:
				words = QStringLiteral("Saving was stopped. Nothing more is sent.");
				break;
			default:
				words = QStringLiteral("The connection could not save the description (%1). Your text is kept.")
							.arg(QString::fromUtf8(application::name(aTerminal.Result.error())));
				break;
			}
			page->present_description_status(words);
			return;
		}
		if (aTerminal.Result->Conflict) {
			// Never overwritten silently: the editor says so, keeps the typed
			// text, and offers GitHub's newer text to go on from.
			++DescriptionEditConflicts;
			page->present_description_status(QStringLiteral("Someone changed the description on GitHub since you began. Nothing was saved."));
			page->offer_newer_description(aTerminal.Result->Body);
			return;
		}
		++DescriptionEditCompletions;
		page->close_description_editor();
		// The description shows GitHub's answer the way every other fact
		// comes back: the next read.
		if (auto* const refresh = Window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
			Window->run_automatic_refresh([refresh] { refresh->trigger(); });
		}
	}

	/// [[BPR-1775]]. The conversation's cards, where my comments are edited.
	auto github_application::implementation::comment_edit_cards() const -> qt::conversation_cards_view*
	{
		return Window != nullptr && Window->conversation_panel() != nullptr ? Window->conversation_panel()->cards() : nullptr;
	}

	/// [[BPR-1775]]. Whether the session reads anonymously, as a reaction
	/// asks it.
	auto github_application::implementation::reads_anonymously() const -> bool
	{
		return Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
	}

	/// [[BPR-1775]]. Edit on my own published comments is offered in a
	/// signed-in read whose connection edits comments; the core decides
	/// each post from the read's "Mine".
	void github_application::implementation::present_comment_edit_offer()
	{
		auto* const cards = comment_edit_cards();
		if (Closing || cards == nullptr) {
			return;
		}
		const auto signedIn = WindowRead && WindowRead->accepted_snapshot() && !WindowRead->accepted_snapshot()->Viewer.Login.empty();
		cards->set_comment_edit_offered(signedIn && !reads_anonymously() && ReviewMutationHost != nullptr && ReviewMutationHost->capabilities().EditsComments);
	}

	/// [[BPR-1775]]. The post as the accepted read has it: whether the
	/// viewer wrote it, and its body; nothing when the read no longer
	/// shows it.
	auto github_application::implementation::accepted_comment(const std::string& aPost) const -> std::optional<std::pair<bool, std::string>>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto  mine     = [&snapshot](const auto& aRecord) {
			return aRecord.Author.has_value() && !snapshot.Viewer.Login.empty() && core::same_login(aRecord.Author->Login, snapshot.Viewer.Login);
		};
		for (const auto& thread : snapshot.ReviewThreads) {
			for (const auto& comment : thread.Comments) {
				if (!comment.Deleted && comment_projection_id(comment) == aPost) {
					return std::pair{ mine(comment), comment.Body };
				}
			}
		}
		for (const auto& comment : snapshot.IssueComments) {
			if (!comment.Deleted && comment_projection_id(comment) == aPost) {
				return std::pair{ mine(comment), comment.Body };
			}
		}
		for (const auto& review : snapshot.Reviews) {
			if (comment_projection_id(review) == aPost) {
				return std::pair{ mine(review), review.Body };
			}
		}
		return std::nullopt;
	}

	/// [[BPR-1775]]. Save while editing one of my comments: the core
	/// decides on the gesture (mine, a change, not blank, within GitHub's
	/// length), the host writes once under a one-shot grant after GitHub,
	/// asked first, still has the original. Nothing is sent before Save,
	/// and a refusal is said beside the editor.
	void github_application::implementation::save_comment_edit(const qt::comment_edit_save& aSave)
	{
		auto* const cards = comment_edit_cards();
		if (Closing || cards == nullptr) {
			return;
		}
		++CommentEditGestures;
		const auto subject  = core::comment_edit_subject_of(aSave.Post);
		const auto accepted = accepted_comment(aSave.Post);
		if (!subject.has_value() || !accepted.has_value()) {
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("This comment is no longer in the pull request as read. Refresh and try again."));
			return;
		}
		const auto refusal = core::decide_comment_edit({ .Mine = accepted->first, .Anonymous = reads_anonymously(), .Original = aSave.Original, .Edited = aSave.Body });
		switch (refusal) {
		case core::comment_edit_refusal::not_mine:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("Only your own comments can be edited here."));
			return;
		case core::comment_edit_refusal::unchanged:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("Nothing has changed, so there is nothing to save."));
			return;
		case core::comment_edit_refusal::empty:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("A comment needs some text. Deleting a comment is not offered here."));
			return;
		case core::comment_edit_refusal::too_long:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("GitHub keeps at most 65,536 characters; this comment has %1.")
												   .arg(QLocale{ }.toString(static_cast<qulonglong>(core::description_characters(aSave.Body)))));
			return;
		case core::comment_edit_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().EditsComments) {
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("This connection cannot edit comments."));
			return;
		}
		if (PendingCommentEdit.has_value()) {
			cards->present_comment_edit_status(QStringLiteral("Waiting for GitHub to answer the last save."), true);
			return;
		}
		auto request = application::review_comment_edit_publication_request{
			.Kind     = subject->Kind,
			.Subject  = std::string{ subject->Node },
			.Original = aSave.Original,
			.Body     = aSave.Body,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_comment_edit(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		cards->present_comment_edit_status(QStringLiteral("Saving your comment to GitHub…"), true);
		PendingCommentEdit = ReviewMutationHost->edit_comment(std::move(request), std::move(grant));
	}

	void github_application::implementation::publish_comment_edit(const application::review_comment_edit_terminal& aTerminal)
	{
		if (Closing || Window == nullptr || PendingCommentEdit != std::optional{ aTerminal.Context }) {
			return;
		}
		PendingCommentEdit.reset();
		auto* const cards = comment_edit_cards();
		if (cards == nullptr) {
			return;
		}
		if (!aTerminal.Result.has_value()) {
			++CommentEditFailures;
			auto words = QString{ };
			switch (aTerminal.Result.error()) {
			case application::review_mutation_failure::refused_by_server:
				words = QStringLiteral("GitHub does not let you edit this comment. Nothing was saved.");
				break;
			case application::review_mutation_failure::not_confirmed:
				words = QStringLiteral("GitHub does not show the new text; it was not sent twice. Check the comment, then save again.");
				break;
			case application::review_mutation_failure::incompatible_body:
				words = QStringLiteral("GitHub cannot take this text. Nothing was saved.");
				break;
			case application::review_mutation_failure::target_moved:
			case application::review_mutation_failure::undesignated_target:
			case application::review_mutation_failure::target_unavailable:
				words = QStringLiteral("The comment or the pull request changed on GitHub, so nothing was saved. Refresh and try again.");
				break;
			case application::review_mutation_failure::cancelled:
				words = QStringLiteral("Saving was stopped. Nothing more is sent.");
				break;
			default:
				words = QStringLiteral("The connection could not save the comment (%1). Your text is kept.")
							.arg(QString::fromUtf8(application::name(aTerminal.Result.error())));
				break;
			}
			cards->present_comment_edit_status(words);
			return;
		}
		if (aTerminal.Result->Conflict) {
			// Never overwritten silently: the editor says so, keeps the typed
			// text, and offers GitHub's newer text to go on from.
			++CommentEditConflicts;
			cards->present_comment_edit_status(QStringLiteral("Someone changed this comment on GitHub since you began. Nothing was saved."));
			cards->offer_newer_comment(aTerminal.Result->Body);
			return;
		}
		++CommentEditCompletions;
		cards->close_comment_edit();
		// The comment shows GitHub's answer the way every other fact comes
		// back: the next read.
		if (auto* const refresh = Window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
			Window->run_automatic_refresh([refresh] { refresh->trigger(); });
		}
	}

	/// [[BPR-1804]]. Request review is offered in People when the
	/// connection in use requests reviewers and the session is signed in.
	void github_application::implementation::present_reviewer_offer()
	{
		if (Closing || Window == nullptr || Window->overview_page() == nullptr) {
			return;
		}
		const auto anonymous = Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
		const auto offered = !anonymous && WindowRead && WindowRead->accepted_snapshot() && ReviewMutationHost != nullptr
			&& ReviewMutationHost->capabilities().RequestsReviewers;
		Window->overview_page()->set_reviewer_requests_offered(offered);
		if (!offered && ReviewerPicker.Open) {
			ReviewerPicker = core::close_reviewer_picker(std::move(ReviewerPicker));
		}
	}

	void github_application::implementation::present_reviewer_picker()
	{
		if (!Closing && Window != nullptr && Window->overview_page() != nullptr) {
			Window->overview_page()->present_reviewer_picker(ReviewerPicker);
		}
	}

	/// [[BPR-1804]]. The core's transition, then its one effect: a search
	/// typed waits 250 ms for the next keystroke (only the last is asked),
	/// opening asks at once, and a choice asks GitHub once under a one-shot
	/// grant for that person.
	void github_application::implementation::reviewer_gesture(const qt::reviewer_picker_gesture& aGesture)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		auto transition = core::reviewer_picker_transition{ .State = ReviewerPicker, .Effect = std::monostate{ } };
		switch (aGesture.Kind) {
		case qt::reviewer_picker_gesture::kind::open:
			if (!Window->overview_page()->reviewer_requests_offered()) {
				return;
			}
			transition = core::open_reviewer_picker(ReviewerPicker);
			break;
		case qt::reviewer_picker_gesture::kind::query:
			transition = core::type_reviewer_query(ReviewerPicker, aGesture.Text);
			break;
		case qt::reviewer_picker_gesture::kind::choose:
			++ReviewerChoices;
			transition = core::choose_reviewer(ReviewerPicker, aGesture.Text);
			break;
		case qt::reviewer_picker_gesture::kind::close:
			ReviewerPicker = core::close_reviewer_picker(std::move(ReviewerPicker));
			ReviewerSearchDebounce.reset();
			return;
		}
		ReviewerPicker = std::move(transition.State);
		if (const auto* const search = std::get_if<core::reviewer_search_effect>(&transition.Effect); search != nullptr) {
			if (aGesture.Kind == qt::reviewer_picker_gesture::kind::open) {
				search_reviewers(*search);
			} else {
				debounce_reviewer_search(*search);
			}
		} else if (const auto* const request = std::get_if<core::reviewer_request_effect>(&transition.Effect); request != nullptr) {
			request_reviewer(*request);
		}
		present_reviewer_picker();
	}

	void github_application::implementation::debounce_reviewer_search(core::reviewer_search_effect aSearch)
	{
		if (!ReviewerSearchDebounce) {
			ReviewerSearchDebounce = std::make_unique<QTimer>();
			ReviewerSearchDebounce->setSingleShot(true);
			ReviewerSearchDebounce->setInterval(reviewer_search_delay);
			QObject::connect(ReviewerSearchDebounce.get(), &QTimer::timeout, Window.get(), [this] {
				if (DebouncedReviewerSearch) {
					auto search = std::move(*DebouncedReviewerSearch);
					DebouncedReviewerSearch.reset();
					search_reviewers(search);
				}
			});
		}
		DebouncedReviewerSearch = std::move(aSearch);
		ReviewerSearchDebounce->start();
	}

	auto github_application::implementation::reviewer_failure(const application::review_mutation_failure aFailure) -> core::reviewer_request_failure
	{
		switch (aFailure) {
		case application::review_mutation_failure::refused_by_server:
		case application::review_mutation_failure::unsupported:
			return core::reviewer_request_failure::refused;
		case application::review_mutation_failure::target_moved:
		case application::review_mutation_failure::undesignated_target:
		case application::review_mutation_failure::target_unavailable:
		case application::review_mutation_failure::stale_head:
			return core::reviewer_request_failure::target_changed;
		case application::review_mutation_failure::cancelled:
			return core::reviewer_request_failure::cancelled;
		default:
			return core::reviewer_request_failure::connection;
		}
	}

	void github_application::implementation::search_reviewers(const core::reviewer_search_effect& aSearch)
	{
		if (Closing || !ReviewerPicker.Open || aSearch.Search != ReviewerPicker.Search) {
			return;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().RequestsReviewers) {
			ReviewerPicker = core::fail_reviewer_search(std::move(ReviewerPicker), aSearch.Search, core::reviewer_request_failure::refused);
			present_reviewer_picker();
			return;
		}
		++ReviewerSearches;
		const auto context                               = ReviewMutationHost->reviewers({ .Operation = application::review_reviewer_operation::search, .Query = aSearch.Query });
		PendingReviewerSearches[context.Request.value()] = aSearch.Search;
	}

	void github_application::implementation::request_reviewer(const core::reviewer_request_effect& aRequest)
	{
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().RequestsReviewers) {
			ReviewerPicker = core::fail_reviewer_request(std::move(ReviewerPicker), aRequest.Request, core::reviewer_request_failure::refused);
			return;
		}
		auto request = application::review_reviewer_publication_request{
			.Operation = application::review_reviewer_operation::request,
			.User      = aRequest.NodeId,
			.Login     = aRequest.Login,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_reviewer(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		// [[BPR-1849]]. People shows the person at once, pending, as the
		// core projects the request; GitHub's answer settles it.
		if (WindowRead && WindowRead->accepted_snapshot()) {
			const auto key = reviewer_overlay_key(*WindowRead->accepted_snapshot());
			if (ReviewerOverlayPullRequest != key) {
				ReviewerOverlay            = { };
				ReviewerOverlayPullRequest = key;
			}
			core::begin_reviewer_request_change(ReviewerOverlay, aRequest);
			present_overview_review_requests();
		}
		++ReviewerRequestsAsked;
		const auto context                               = ReviewMutationHost->reviewers(std::move(request), std::move(grant));
		PendingReviewerRequests[context.Request.value()] = aRequest.Request;
	}

	/// \brief [[BPR-1849]]. Which pull request a snapshot reads, for the
	/// review requests asked on it.
	auto github_application::implementation::reviewer_overlay_key(const application::github_pull_request_snapshot& aSnapshot) -> std::string
	{
		return aSnapshot.Repository.Owner + "/" + aSnapshot.Repository.Name + "#" + std::to_string(aSnapshot.PullRequest.Number.value());
	}

	/// \brief [[BPR-1849]]. A new accepted read reconciles the requests
	/// the viewer asked for with the ones GitHub lists; another pull
	/// request drops them.
	void github_application::implementation::observe_reviewer_request_state()
	{
		if (!WindowRead || !WindowRead->accepted_snapshot() || !ReviewerOverlayPullRequest) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		if (reviewer_overlay_key(snapshot) != *ReviewerOverlayPullRequest) {
			ReviewerOverlay = { };
			ReviewerOverlayPullRequest.reset();
			return;
		}
		core::observe_reviewer_requests(ReviewerOverlay, application::requested_reviewer_logins(snapshot));
	}

	/// \brief [[BPR-1849]]. The requests People adds to the read's.
	auto github_application::implementation::projected_overview_review_requests(const application::github_pull_request_snapshot& aSnapshot) const
		-> std::vector<core::projected_reviewer_request>
	{
		if (ReviewerOverlay.Changes.empty() || ReviewerOverlayPullRequest != std::optional{ reviewer_overlay_key(aSnapshot) }) {
			return { };
		}
		return core::project_reviewer_requests(application::requested_reviewer_logins(aSnapshot), ReviewerOverlay);
	}

	/// \brief [[BPR-1849]]. A request asked, answered or withdrawn moves
	/// People and the status in place; nothing is read and no other part
	/// of the Overview is visited. A read the Overview has not been
	/// projected from yet is left to that projection.
	void github_application::implementation::present_overview_review_requests()
	{
		if (Closing || Window == nullptr || !PublishedOverview || !WindowRead || !WindowRead->accepted_snapshot()
			|| OverviewResolutionsSnapshot != std::optional{ WindowRead->accepted_snapshot()->Snapshot }) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		auto        now      = projected_overview_review_requests(snapshot);
		if (now == OverviewReviewRequests) {
			return;
		}
		const auto changed     = application::apply_review_requests(*PublishedOverview, snapshot, now);
		OverviewReviewRequests = std::move(now);
		if (changed) {
			++OverviewReviewRequestUpdates;
			Window->publish_review_requests(*PublishedOverview);
		}
	}

	/// \brief [[BPR-1852]]. The draft state the window shows over a read:
	/// the viewer's own change while the overlay holds one for this pull
	/// request, the read's otherwise.
	auto github_application::implementation::projected_draft_state(const application::github_pull_request_snapshot& aSnapshot) const -> std::optional<core::projected_draft_state>
	{
		if (!DraftOverlay.held() || DraftOverlayPullRequest != std::optional{ reviewer_overlay_key(aSnapshot) }) {
			return std::nullopt;
		}
		return core::project_draft_state(aSnapshot.PullRequest.Draft, DraftOverlay);
	}

	auto github_application::implementation::draft_state_reading_now() const -> std::optional<draft_state_reading>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot  = *WindowRead->accepted_snapshot();
		const auto  shown     = projected_draft_state(snapshot).value_or(core::projected_draft_state{ .Draft = snapshot.PullRequest.Draft });
		auto [author, viewer] = description_edit_logins();
		return draft_state_reading{ .Author = std::move(author), .Viewer = std::move(viewer), .Open = snapshot.PullRequest.State == application::github_pull_request_state::open, .Draft = shown.Draft };
	}

	/// \brief [[BPR-1852]]. Ready for review or Convert to draft is
	/// offered to the pull request's author, the core decides, when the
	/// connection in use can change the draft state; everyone else sees the
	/// chip and the banner alone.
	void github_application::implementation::present_draft_state_offer()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto reading = draft_state_reading_now();
		auto       offered = std::optional<core::draft_state_change>{ };
		if (reading.has_value() && ReviewMutationHost != nullptr && ReviewMutationHost->capabilities().ChangesDraftState) {
			offered = core::draft_state_offered(reading->facts());
		}
		Window->set_draft_state_offer(offered);
	}

	/// \brief [[BPR-1852]]. A new accepted read reconciles the viewer's
	/// change with GitHub's draft state; another pull request drops it.
	void github_application::implementation::observe_draft_state_change()
	{
		if (!WindowRead || !WindowRead->accepted_snapshot() || !DraftOverlayPullRequest) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		if (reviewer_overlay_key(snapshot) != *DraftOverlayPullRequest) {
			DraftOverlay = { };
			DraftOverlayPullRequest.reset();
			return;
		}
		core::observe_draft_state(DraftOverlay, snapshot.PullRequest.Draft);
	}

	/// \brief [[BPR-1852]]. A change asked, answered or withdrawn moves the
	/// title row's chip, Overview's chip and banner, and Merge in place;
	/// nothing is read and no other part of the window is visited. A read
	/// the Overview has not been projected from yet is left to that
	/// projection.
	void github_application::implementation::present_overview_draft_state()
	{
		if (Closing || Window == nullptr || !WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto  now      = projected_draft_state(snapshot);
		Window->present_strip_draft(now.value_or(core::projected_draft_state{ .Draft = snapshot.PullRequest.Draft }).Draft
									&& snapshot.PullRequest.State == application::github_pull_request_state::open);
		present_draft_state_offer();
		if (!PublishedOverview || OverviewResolutionsSnapshot != std::optional{ snapshot.Snapshot }) {
			return;
		}
		if (application::apply_draft_state(*PublishedOverview, snapshot, now)) {
			++OverviewDraftStateUpdates;
			Window->publish_draft_state(*PublishedOverview);
		}
	}

	/// \brief [[BPR-1852]]. Ready for review, or Convert to draft, from
	/// Overview or the Pull Request menu: the core decides on the gesture
	/// (the author, an open pull request, not already in that state), the
	/// window shows the change at once, and the host writes it once under a
	/// one-shot grant after GitHub, asked first, says it still applies.
	void github_application::implementation::change_draft_state(const core::draft_state_change aChange)
	{
		auto* const page = Window != nullptr ? Window->overview_page() : nullptr;
		if (Closing || page == nullptr) {
			return;
		}
		++DraftStateGestures;
		const auto reading = draft_state_reading_now();
		if (!reading.has_value()) {
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("The pull request is still loading. Try again when it has."));
			return;
		}
		switch (core::decide_draft_state_change(reading->facts(), aChange)) {
		case core::draft_state_refusal::not_author:
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("Only the pull request's author can change whether it is a draft here."));
			return;
		case core::draft_state_refusal::not_open:
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("This pull request is closed, so its draft state cannot change."));
			return;
		case core::draft_state_refusal::already:
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, aChange == core::draft_state_change::ready_for_review ? QStringLiteral("This pull request is already ready for review.") : QStringLiteral("This pull request is already a draft."));
			return;
		case core::draft_state_refusal::not_permitted:
		case core::draft_state_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().ChangesDraftState) {
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("This connection cannot change whether the pull request is a draft."));
			return;
		}
		if (PendingDraftState.has_value()) {
			page->present_draft_state_status(aChange, QStringLiteral("Waiting for GitHub to answer the last change."), true);
			return;
		}
		auto request = application::review_draft_state_publication_request{ .Change = aChange };
		auto grant   = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_draft_state(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		// The window shows the change at once, pending, as the core
		// projects it; GitHub's answer settles it.
		const auto& snapshot    = *WindowRead->accepted_snapshot();
		DraftOverlayPullRequest = reviewer_overlay_key(snapshot);
		core::begin_draft_state_change(DraftOverlay, ++DraftStateRequests, aChange);
		PendingDraftState = pending_draft_state{ .Request = DraftStateRequests, .Change = aChange };
		page->present_draft_state_status(aChange, QString{ }, true);
		present_overview_draft_state();
		PendingDraftState->Context = ReviewMutationHost->change_draft_state(request, std::move(grant));
	}

	void github_application::implementation::publish_draft_state(const application::review_draft_state_terminal& aTerminal)
	{
		if (Closing || Window == nullptr || !PendingDraftState.has_value() || PendingDraftState->Context != aTerminal.Context) {
			return;
		}
		const auto request = PendingDraftState->Request;
		PendingDraftState.reset();
		auto* const page   = Window->overview_page();
		const auto  change = aTerminal.Request.Change;
		if (!aTerminal.Result.has_value()) {
			++DraftStateFailures;
			// The window shows the read again; the words say why.
			core::abandon_draft_state_change(DraftOverlay, request);
			auto words = QString{ };
			switch (aTerminal.Result.error()) {
			case application::review_mutation_failure::refused_by_server:
				words = QStringLiteral("GitHub did not let you change whether this pull request is a draft. Nothing changed.");
				break;
			case application::review_mutation_failure::not_confirmed:
				words = QStringLiteral("GitHub does not show the change; it was not sent twice. Check the pull request, then try again.");
				break;
			case application::review_mutation_failure::target_moved:
			case application::review_mutation_failure::undesignated_target:
			case application::review_mutation_failure::target_unavailable:
				words = QStringLiteral("The pull request changed on GitHub, so nothing was sent. Refresh and try again.");
				break;
			case application::review_mutation_failure::cancelled:
				words = QStringLiteral("The change was stopped. Nothing more is sent.");
				break;
			default:
				words = QStringLiteral("The connection could not change the draft state (%1). Try again.").arg(QString::fromUtf8(application::name(aTerminal.Result.error())));
				break;
			}
			if (page != nullptr) {
				page->present_draft_state_status(change, words);
			}
			present_overview_draft_state();
			return;
		}
		++DraftStateCompletions;
		// GitHub's answer confirms what the window already shows, or
		// withdraws it; nothing re-reads the pull request, and the next
		// read reconciles.
		core::settle_draft_state_change(DraftOverlay, request, aTerminal.Result->Draft);
		if (page != nullptr) {
			page->present_draft_state_status(change, aTerminal.Result->Draft == core::draft_after(change) ? QString{ } : QStringLiteral("GitHub still shows the pull request as it was. Nothing changed."));
		}
		present_overview_draft_state();
	}

	/// \brief [[BPR-1852]]. A retired host answers nothing more: a change
	/// still waiting is withdrawn and said to have gone unanswered (it is
	/// never sent again).
	void github_application::implementation::retire_draft_state_change()
	{
		if (!PendingDraftState.has_value()) {
			return;
		}
		const auto change = PendingDraftState->Change;
		core::abandon_draft_state_change(DraftOverlay, PendingDraftState->Request);
		PendingDraftState.reset();
		if (Window != nullptr && Window->overview_page() != nullptr) {
			Window->overview_page()->present_draft_state_status(change, QStringLiteral("The connection closed before GitHub answered. Check the pull request, then try again."));
		}
		present_overview_draft_state();
	}

	void github_application::implementation::publish_reviewers(const application::review_reviewer_terminal& aTerminal)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto key = aTerminal.Context.Request.value();
		if (aTerminal.Request.Operation == application::review_reviewer_operation::search) {
			const auto found = PendingReviewerSearches.find(key);
			if (found == PendingReviewerSearches.end()) {
				return;
			}
			const auto search = found->second;
			PendingReviewerSearches.erase(found);
			if (aTerminal.Result.has_value()) {
				ReviewerPicker = core::accept_reviewer_search(std::move(ReviewerPicker), search, aTerminal.Result->Answer);
			} else {
				ReviewerPicker = core::fail_reviewer_search(std::move(ReviewerPicker), search, reviewer_failure(aTerminal.Result.error()));
			}
			present_reviewer_picker();
			return;
		}
		const auto found = PendingReviewerRequests.find(key);
		if (found == PendingReviewerRequests.end()) {
			return;
		}
		const auto request = found->second;
		PendingReviewerRequests.erase(found);
		if (!aTerminal.Result.has_value()) {
			++ReviewerRequestFailures;
			ReviewerPicker = core::fail_reviewer_request(std::move(ReviewerPicker), request, reviewer_failure(aTerminal.Result.error()));
			// [[BPR-1849]]. People shows the read again; the picker says why.
			core::abandon_reviewer_request_change(ReviewerOverlay, request);
			present_reviewer_picker();
			present_overview_review_requests();
			return;
		}
		++ReviewerRequestCompletions;
		const auto answer = core::reviewer_request_answer{ .Requested = aTerminal.Result->Answer.Requested, .Written = aTerminal.Result->Written };
		ReviewerPicker    = core::finish_reviewer_request(std::move(ReviewerPicker), request, answer);
		// [[BPR-1849]]. GitHub's answer confirms the person People already
		// shows, or withdraws them; nothing re-reads the pull request, and
		// the next read reconciles.
		core::settle_reviewer_request_change(ReviewerOverlay, request, answer);
		present_reviewer_picker();
		present_overview_review_requests();
	}

	/// [[BPR-1804]]. A retired host answers nothing more: searches are
	/// forgotten, the picker closes, and a request still waiting is said to
	/// have gone unanswered (it is never sent again).
	void github_application::implementation::retire_reviewer_picker()
	{
		PendingReviewerSearches.clear();
		for (const auto& [context, request] : PendingReviewerRequests) {
			static_cast<void>(context);
			ReviewerPicker = core::fail_reviewer_request(std::move(ReviewerPicker), request, core::reviewer_request_failure::connection);
			core::abandon_reviewer_request_change(ReviewerOverlay, request);
		}
		const auto withdrawn = !PendingReviewerRequests.empty();
		PendingReviewerRequests.clear();
		if (withdrawn) {
			present_overview_review_requests();
		}
		DebouncedReviewerSearch.reset();
		if (ReviewerSearchDebounce) {
			ReviewerSearchDebounce->stop();
		}
		if (ReviewerPicker.Open) {
			ReviewerPicker = core::close_reviewer_picker(std::move(ReviewerPicker));
			present_reviewer_picker();
		}
	}

	// [[BPR-0657]]. A card's Resolve or Unresolve: the core decides from
	// the session's anonymity, the server's capability and the thread's
	// state, before any provider is asked; a refusal is named on the
	// panel. An allowed gesture goes to the mutation host under the same
	// grant a publication gets. [[BPR-1841]]: its terminal settles the
	// core's overlay and the window projects that one thread's change;
	// nothing re-reads the pull request, and the next read reconciles.
	void github_application::implementation::resolve_review_thread(const qt::thread_resolution_gesture& aGesture)
	{
		constexpr auto prefix = std::string_view{ "review-thread:" };
		if (Closing || Window == nullptr || !aGesture.Identity.starts_with(prefix)) {
			return;
		}
		auto* const panel  = Window->conversation_panel();
		const auto  notice = [panel](const QString& aText) {
			if (panel != nullptr) {
				panel->present_notice(aText);
			}
		};
		const auto anonymous = Host != nullptr

			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(
				aGesture.Resolve ? core::anonymous_read_intent::resolve_review_thread
								 : core::anonymous_read_intent::unresolve_review_thread));
		// [[BPR-1824]]. Whether a connection writes to this pull request;
		// a selected diff is not asked for, so Discuss resolves with no
		// file open.
		const auto connection = thread_resolution_connection_refusal();
		// [[BPR-1741]]. GitHub's own word on whether this viewer may
		// resolve (or reopen) the thread, from the accepted read.
		// [[BPR-1841]]. As projected: a thread I resolved since the read
		// may be reopened without reading it again.
		const auto* const thread    = accepted_thread(aGesture.Identity);
		auto              permitted = true;
		if (thread != nullptr) {
			const auto projected = core::project_thread_resolution(thread->Resolved, ConversationOverlay, aGesture.Identity);
			const auto may       = core::project_thread_resolution_permission(thread->Resolved, projected.Resolved, thread_permission(*thread));
			permitted            = aGesture.Resolve ? may.MayResolve : may.MayUnresolve;
		}
		const auto refusal = core::decide_thread_resolution(core::thread_resolution_input{
			.Capability      = aGesture.Capability,
			.AnonymousRead   = anonymous,
			.Resolved        = aGesture.Resolved,
			.Resolve         = aGesture.Resolve,
			.ViewerPermitted = permitted,
			.WriteConnection = connection != core::thread_resolution_refusal::no_write_connection && ReviewMutationHost != nullptr,
		});

		++ThreadResolutionGestures;
		if (refusal != core::thread_resolution_refusal::none) {
			++ThreadResolutionRefusals;
			notice(qt::thread_resolution_refusal_text(refusal, aGesture.Resolve));
			return;
		}
		if (core::resolution_in_flight(ConversationOverlay, aGesture.Identity)) {
			++ThreadResolutionRefusals;
			notice(QStringLiteral("Waiting for GitHub to answer the last change to this conversation."));
			return;
		}
		auto request = application::review_thread_resolution_publication_request{

			.Thread  = std::string{ aGesture.Identity.substr(prefix.size()) },
			.Resolve = aGesture.Resolve,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_thread(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		notice(aGesture.Resolve ? QStringLiteral("Resolving the conversation…")
								: QStringLiteral("Reopening the conversation…"));
		// [[BPR-1741]]. The thread shows what was asked at once.
		static_cast<void>(core::begin_resolution_change(ConversationOverlay, aGesture.Identity, aGesture.Resolve));
		present_conversation_state();
		const auto context = ReviewMutationHost->resolve(std::move(request), std::move(grant));
		if (!core::retain_conversation_mutation(ConversationMutations, { context.Request, context.Session, context.Generation },
												{ .Subject = aGesture.Identity, .Add = aGesture.Resolve })) {
			core::abandon_resolution_change(ConversationOverlay, aGesture.Identity);
			present_conversation_state();
		}
	}

	/// [[BPR-1824]]. What stopped a resolution GitHub was asked for, or
	/// that the host refused before asking, and what to do about it.
	auto github_application::implementation::thread_resolution_failure_text(const application::review_mutation_failure aFailure, const bool aResolve) -> QString
	{
		using failure   = application::review_mutation_failure;
		const auto verb = aResolve ? QStringLiteral("resolved") : QStringLiteral("reopened");
		switch (aFailure) {
		case failure::refused_by_server:
		case failure::target_unavailable:
			return QStringLiteral("GitHub refused, so the thread was not %1. Your account may lack write access to the repository, or the thread is gone.").arg(verb);
		case failure::target_moved:
		case failure::undesignated_target:
		case failure::stale_head:
			return QStringLiteral("The pull request changed since it was read, so the thread was not %1. Refresh, then try again.").arg(verb);
		case failure::unsupported:
			return QStringLiteral("This connection cannot change threads, so the thread was not %1. Use the pull request's page on GitHub.").arg(verb);
		case failure::credential_unavailable:
		case failure::credential_tool_failed:
			return QStringLiteral("No GitHub sign-in was available, so the thread was not %1. Sign in again, then try again.").arg(verb);
		case failure::cancelled:
			return QStringLiteral("Stopped before GitHub answered; the thread was not %1.").arg(verb);
		case failure::not_confirmed:
			return QStringLiteral("GitHub does not show the thread %1, and the change was not sent twice. Refresh to see its state.").arg(verb);
		case failure::malformed_response:
			return QStringLiteral("GitHub's answer could not be read, so it is not known whether the thread was %1. Refresh to see its state.").arg(verb);
		case failure::unavailable:
			return QStringLiteral("GitHub could not be reached, or another change to this pull request is still being sent, so the thread was not %1. Try again.").arg(verb);
		default:
			return QStringLiteral("The thread was not %1 (%2).").arg(verb, QString::fromUtf8(application::name(aFailure)));
		}
	}

	void github_application::implementation::publish_resolution(const application::review_thread_resolution_terminal& aTerminal)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (!core::settle_conversation_mutation(ConversationMutations, ConversationOverlay,
												{ aTerminal.Context.Request, aTerminal.Context.Session, aTerminal.Context.Generation },
												{ .Subject = "review-thread:" + aTerminal.Request.Thread, .Add = aTerminal.Request.Resolve }, aTerminal.Result.has_value())) {
			return;
		}
		present_conversation_state();
		auto* const panel = Window->conversation_panel();
		if (!aTerminal.Result.has_value()) {
			++ThreadResolutionFailures;

			if (panel != nullptr) {
				panel->present_notice(thread_resolution_failure_text(aTerminal.Result.error(), aTerminal.Request.Resolve));
			}
			return;
		}
		++ThreadResolutionCompletions;
		if (panel != nullptr) {
			panel->present_notice(aTerminal.Result->Resolved ? QStringLiteral("Conversation resolved.")
															 : QStringLiteral("Conversation reopened."));
		}
		// [[BPR-1841]]. GitHub's answer is the thread's new state: the core
		// keeps the confirmed change until a later read agrees, and the
		// window has already shown it. Re-reading the whole pull request
		// here made every resolve wait on a full refresh.
	}

	void github_application::implementation::refresh_review_projection(
		const application::review_mutation_terminal& aTerminal)
	{
#if defined(BPR_BUILD_TESTING)
		static_cast<void>(aTerminal);
		if (Fixture == nullptr) {
			return;
		}
		const auto observed  = Fixture->review_comments();
		ReviewReconciliation = application::reconcile(
			ReviewProjection, observed, { });
		ReviewProjection = ReviewReconciliation.Projection;
#else
		if (!aTerminal.Reconciliation.has_value()) {
			return;
		}
		ReviewReconciliation = application::reconcile(
			ReviewProjection, *aTerminal.Reconciliation, { });
		ReviewProjection = ReviewReconciliation.Projection;
#endif
	}
	auto initialize_or_reset_designated_live_replay_state(
		const std::filesystem::path& aApplicationDataRoot) noexcept -> bool
	{
		try {
			if (aApplicationDataRoot.empty() || !aApplicationDataRoot.is_absolute()
				|| aApplicationDataRoot.lexically_normal() != aApplicationDataRoot) {
				return false;
			}
			return application::detail::designated_review_replay_initializer::initialize_or_reset(
					   designated_review_replay_namespace_path(aApplicationDataRoot))
				.has_value();
		} catch (...) {
			return false;
		}
	}

	[[nodiscard]] auto compatibility_window_context(core::bpr_profile_id aProfile) -> core::bpr_window_context
	{
		auto state = core::profile_window_state{ };
		if (aProfile != core::default_bpr_profile_id) {
			state.Profiles.push_back({ { aProfile, "Profile" }, { }, { } });
			state.LastProfileId = aProfile.value();
		}
		auto catalog = core::profile_window_catalog::restore(std::move(state));
		if (!catalog) {
			throw std::runtime_error{ "compatibility window admission unavailable" };
		}
		auto launch = catalog->launch(aProfile, core::profile_launch_intent::activate);
		if (!launch) {
			throw std::runtime_error{ "compatibility window admission unavailable" };
		}
		return launch->Activated;
	}

	class github_application_startup::implementation final
		: public application::profile_registry_port,
		  public application::profile_startup_root_port,
		  public application::profile_startup_projection_port,
		  public std::enable_shared_from_this<implementation>
	{
	public:
		implementation(github_application_config aConfig, github_application_startup_completion aCompletion
#if defined(BPR_BUILD_TESTING)
					   ,
					   std::optional<detail::github_public_path_fixture> aFixture = std::nullopt,
					   detail::github_public_path_fixture_options        aOptions = { }
#endif
					   )
			: Config{ std::move(aConfig) }
			, Completion{ std::move(aCompletion) }
			, Host{ *this, *this, Effects, Decisions, *this, core::application_session_id{ 1U } }
		{
#if defined(BPR_BUILD_TESTING)
			Fixture = aFixture;
			Options = std::move(aOptions);
			if (Fixture) {
				Config.Profile             = Options.Profile;
				Config.ProfileLaunchIntent = Options.ProfileLaunchIntent;
				Config.ApplicationDataRoot = Options.ApplicationDataRoot;
				Config.DataReset           = Options.DataReset;
				Config.DifCatalog          = Options.DifCatalog;
				Config.StatusItem          = Options.StatusItem;
				if (!Config.ApplicationDataRoot) {
					auto temporary             = std::make_shared<QTemporaryDir>(QDir::temp().filePath(QStringLiteral("bpr-public-path-XXXXXX")));
					Config.ApplicationDataRoot = temporary->isValid() ? native_filesystem_path(QFileInfo{ temporary->path() }.canonicalFilePath()) / "data" : std::filesystem::path{ };
					Scratch                    = std::move(temporary);
				}
			}
#endif
			if (!Config.MotionObserver) {
				Config.MotionObserver = std::make_shared<qt::motion_observer>(qt::make_native_motion_source());
			}
			if (!Config.DifCatalog) {
				Config.DifCatalog = std::make_shared<qt::dif_catalog_provider>();
			}
			// [[BPR-1868]]. A root typed in another letter case than its folders
			// have on a case-insensitive volume names the same folders; the
			// stores admit only a root spelled as the file system spells it.
			RegistryRoot = adapters::sqlite::storage_root_on_disk_spelling(native_application_data_root(Config));
			// [[BPR-1870]]. A named folder that holds other programs' files is
			// never narrowed; the stores keep their data in a private folder
			// the application makes inside it instead.
			const auto placed  = adapters::sqlite::storage_root_placement(RegistryRoot);
			RegistryRootNested = placed != RegistryRoot;
			RegistryRoot       = placed;
		}
		~implementation() override
		{
			Host.shutdown();
			Effects.close_and_drain();
		}
		void begin()
		{
			const auto started = Host.start(Config.Profile);
			if (!started) {
				Decisions.post([self = shared_from_this()] { self->finish(std::unexpected(github_application_startup_failure::invalid_profile)); });
			}
		}
		void cancel()
		{
			Cancelled = true;
			Host.cancel();
		}
		auto is_quiescent() const noexcept -> bool { return Terminal && Host.is_quiescent() && Effects.idle(); }
		auto load(std::stop_token aStop) -> application::profile_registry_result<application::profile_registry_snapshot> override
		{
			if (aStop.stop_requested()) {
				return std::unexpected(application::profile_registry_failure::cancelled);
			}
			// [[BPR-1868]]. Each refusal below names its own cause on the
			// startup-failure-source line: every one of them reaches a reader as
			// `registry_unavailable`, and a root that was only spelled in another
			// letter case, and one that already existed with a shared mode, were
			// told apart by nothing. The names are fixed text with no path.
			const auto refuse = [](const std::string_view aSource) {
				detail::note_startup_failure(aSource);
				return std::unexpected(application::profile_registry_failure::unavailable);
			};
			if (RegistryRoot.empty() || !RegistryRoot.is_absolute() || RegistryRoot.lexically_normal() != RegistryRoot) {
				return refuse("registry-root-path: the data root is not an absolute, normal path");
			}
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(RegistryRoot, error);
			if (!std::filesystem::exists(status)) {
				auto ancestor = RegistryRoot.parent_path();
				while (!ancestor.empty() && !std::filesystem::exists(ancestor, error) && !error) {
					ancestor = ancestor.parent_path();
				}
				if (error || ancestor.empty() || std::filesystem::canonical(ancestor, error) != ancestor || error) {
					return refuse("registry-root-ancestor: the data root's nearest existing folder resolves through a link or another spelling");
				}
				if (!adapters::sqlite::create_storage_root_owner_private(RegistryRoot)) {
					return refuse("registry-root-create: the data root could not be created owner-private");
				}
			} else if (std::filesystem::is_symlink(status)) {
				return refuse("registry-root-link: the data root is a symbolic link");
			} else if (!std::filesystem::is_directory(status)) {
				return refuse("registry-root-not-directory: the data root exists and is not a folder");
			} else {
				// [[BPR-1870]]. Owner, 2026-11-04: "I want the app to just work".
				// A data root that is the application's own (empty, or holding
				// its `state/v1`) and this user's is narrowed in place rather
				// than refused, and says so on its own line.
				switch (adapters::sqlite::tighten_existing_storage_root(RegistryRoot)) {
				case adapters::sqlite::storage_root_tightening::already_private:
					break;
				case adapters::sqlite::storage_root_tightening::tightened:
					detail::record_terminal_diagnostic("STORAGE_PRIVATE_REPAIRED:registry-root: the data root folder was not owner-private and was made so\n");
					break;
				case adapters::sqlite::storage_root_tightening::not_owned:
					return refuse("registry-root-owner: the data root folder belongs to another account; name a folder you own, or one that does not exist yet");
				case adapters::sqlite::storage_root_tightening::foreign_content:
					return refuse("registry-root-foreign-content: other files appeared in the data root folder while it was being prepared; start again");
				case adapters::sqlite::storage_root_tightening::refused:
					break;
				}
			}
			if (RegistryRootNested) {
				detail::record_terminal_diagnostic("STORAGE_PRIVATE_NESTED:registry-root: the named data folder holds other files or is another account's, and is left unchanged; data is kept in its BetterPullReview/data folder\n");
			}
			// This only validates the registry base; it does not admit Default for runtime use.
			const auto baseAuthority = application::profile_storage_authority::select(application::profile_registry_snapshot{ }, core::default_bpr_profile_id);
			const auto base          = adapters::sqlite::resolve_profile_storage_root(RegistryRoot, *baseAuthority, aStop);
			if (!base) {
				if (aStop.stop_requested()) {
					return std::unexpected(application::profile_registry_failure::cancelled);
				}
				switch (base.error()) {
				case adapters::sqlite::profile_storage_root_failure::invalid_permissions:
					// [[BPR-1870]]. The root itself was narrowed above when it
					// could be; what is left could not be made private here.
					return refuse("registry-root-permissions: a data folder is not owner-private and could not be made so (it belongs to another account, or its mode cannot be changed); name a folder you own, or one that does not exist yet");
				case adapters::sqlite::profile_storage_root_failure::invalid_path:
					return refuse("registry-root-spelling: the data root resolves through a link or another spelling");
				case adapters::sqlite::profile_storage_root_failure::cancelled:
					return std::unexpected(application::profile_registry_failure::cancelled);
				case adapters::sqlite::profile_storage_root_failure::unavailable:
					break;
				}
				return refuse("registry-root-unavailable: the data root could not be opened, or is not on a supported local file system");
			}
			auto registryStore = adapters::sqlite::make_profile_registry_store(*base);
			return registryStore->load(aStop);
		}
		auto save(const application::profile_registry_snapshot&, std::uint64_t, std::stop_token) -> application::profile_registry_result<application::profile_registry_snapshot> override
		{
			return std::unexpected(application::profile_registry_failure::unavailable);
		}
		auto resolve(const application::profile_storage_authority& aAuthority, std::stop_token aStop) -> application::profile_startup_result<std::filesystem::path> override
		{
			const auto root = adapters::sqlite::resolve_profile_storage_root(RegistryRoot, aAuthority, aStop);
			if (!root) {
				return std::unexpected(aStop.stop_requested() ? application::profile_startup_failure::cancelled : application::profile_startup_failure::root_unavailable);
			}
			auto lease = adapters::sqlite::acquire_profile_runtime_lease(*root, aStop);
			if (!lease) {
				if (OwnerOnly && lease.error() == application::profile_runtime_lease_failure::busy) {
					const auto routed  = adapters::sqlite::send_profile_activation(*root, Config.Profile, Config.ProfileLaunchIntent, aStop);
					const auto outcome = routed ? *routed : routed.error();
					if (outcome == application::profile_activation_outcome::queued) {
						Handoff = github_application_startup_failure::activation_queued;
					} else if (outcome == application::profile_activation_outcome::refused) {
						Handoff = github_application_startup_failure::activation_refused;
					} else if (outcome == application::profile_activation_outcome::indeterminate) {
						Handoff = github_application_startup_failure::activation_indeterminate;
					}
				}
				return std::unexpected(lease.error() == application::profile_runtime_lease_failure::busy
										   ? application::profile_startup_failure::owner_busy
										   : aStop.stop_requested() ? application::profile_startup_failure::cancelled
																	: application::profile_startup_failure::root_unavailable);
			}
			RuntimeLease = std::move(*lease);
			// Recovery precedes every profile store and producer. The registry has a
			// separate owner and is explicitly excluded from the purge inventory.
			if (!recover_release_profile(*root, RuntimeLease, Config.Profile)) {
				return std::unexpected(application::profile_startup_failure::root_unavailable);
			}
			// [[BPR-1602]]. A confirmed reset runs here, under the lease and before
			// any store opens, so a damaged store is removed without being read.
			// One that stopped part-way leaves its journal for the next start,
			// which finishes it; until then the profile does not open over it.
			if (Config.DataReset) {
				// [[BPR-1858]]. A reset that signs out reaches this profile's own
				// vault on a worker it owns, drained before the vault goes; a
				// course reaches its fixture vault instead, never the keychain.
				auto vaultEffects = std::optional<worker_executor>{ };
				auto vault        = std::unique_ptr<application::github_credential_vault_port>{ };
				if (core::profile_data_reset_signs_out(Config.DataReset->Reset)) {
					vaultEffects.emplace();
#if defined(BPR_BUILD_TESTING)
					if (Fixture) {
						vault = detail::make_github_fixture_reset_vault(*vaultEffects, Options);
					} else
#endif
					{
						vault = make_platform_vault(*vaultEffects, Config.Profile);
					}
				}
				DataResetReport = run_profile_data_reset(*root, RuntimeLease, Config.Profile, *Config.DataReset, vault.get(), aStop);
				if (vaultEffects) {
					vaultEffects->close_and_drain();
				}
				vault.reset();
				if (DataResetReport->Outcome.Reason == core::release_maintenance_reason::recovery_required) {
					detail::note_startup_failure("profile-data-reset-unfinished");
					return std::unexpected(application::profile_startup_failure::root_unavailable);
				}
			}
			if (OwnerOnly) {
				const auto endpoint = adapters::sqlite::make_profile_activation_server(*root, Config.Profile, RuntimeLease);
				if (endpoint) {
					ActivationServer = *endpoint;
				} else {
					// [[BPR-0881]]. A profile with no endpoint answers its own second launch with
					// owner-busy forever, and skipping the refusal silently is how that survived.
					const auto resolved = adapters::sqlite::profile_activation_endpoint(*root);
					bpr::app::detail::terminal_text_stream{ } << "profile-activation-endpoint-unavailable "
															  << (resolved ? "bind-refused" : adapters::sqlite::describe_profile_activation_endpoint_failure(resolved.error())) << '\n';
				}
				// The owner command endpoint (tour and, since BPR-1168, cycle commands) is separate:
				// without it the owner keeps working and a CLI caller is told the owner has no
				// channel, never given a second writer. Windows binds a named pipe (BPR-1168).
#if defined(BPR_LOCAL_AUTOMATION)
				const auto tour = adapters::sqlite::make_tour_owner_server(*root, Config.Profile, RuntimeLease);
				if (tour) {
					TourOwnerServer   = *tour;
					TourOwnerService  = std::make_shared<const tour_owner_service>(*root, Config.Profile);
					CycleOwnerService = std::make_shared<cycle_owner_service>(*root, Config.Profile);
				} else {
					bpr::app::detail::terminal_text_stream{ } << "tour-owner-endpoint-unavailable\n";
				}
#else
				// [[BPR-1179]]. Without local automation no endpoint is bound and
				// no cycle command is served. The tour service stays: the desktop
				// routes its own tour edits (a note promoted into a pull request's
				// tour) through it in process, with no channel involved.
				TourOwnerService = std::make_shared<const tour_owner_service>(*root, Config.Profile);
#endif
			}
			auto store  = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = *root });
			auto loaded = store ? store->load(aStop) : std::unexpected(application::queue_store_failure::unavailable);
			if (aStop.stop_requested()) {
				return std::unexpected(application::profile_startup_failure::cancelled);
			}
			if (loaded) {
				SavedQueue = std::move(*loaded);
			} else {
				SavedQueue.reset();
			}
			ConnectionStore  = adapters::sqlite::make_remembered_connection_store(*root, Config.Profile);
			SavedConnections = ConnectionStore->load(aStop);
			return *root;
		}
		bool OwnerOnly = false;
		void publish(const application::profile_startup_terminal& aTerminal) override
		{
			if (Cancelled || (!aTerminal.Result && aTerminal.Result.error() == application::profile_startup_failure::cancelled)) {
				finish(std::unexpected(github_application_startup_failure::cancelled));
				return;
			}
			if (!aTerminal.Result) {
				const auto failure = aTerminal.Result.error();
				// [[BPR-0877]]. `registry` below is the catch-all arm of this
				// mapping, so five different profile refusals arrive at a reader
				// wearing one name. Record the one that actually fired, first
				// refusal wins, so a run says which rather than needing a bisect.
				note_profile_startup_refusal(failure);
				if (failure == application::profile_startup_failure::owner_busy && Handoff) {
					finish(std::unexpected(*Handoff));
					return;
				}
				finish(std::unexpected(failure == application::profile_startup_failure::owner_busy             ? github_application_startup_failure::owner_busy
										   : failure == application::profile_startup_failure::unknown_profile  ? github_application_startup_failure::invalid_profile
										   : failure == application::profile_startup_failure::root_unavailable ? github_application_startup_failure::root
																											   : github_application_startup_failure::registry));
				return;
			}
			// [[BPR-0882]]. Three different steps fail into the one catch below,
			// so each names itself before it runs and the catch reports which.
			auto step = std::string_view{ "profile-runtime-construction" };
			try {
				auto profile = std::make_shared<github_profile_runtime>(std::move(Config), *aTerminal.Result, RegistryRoot, std::move(SavedQueue), std::move(ConnectionStore), std::move(SavedConnections), std::move(Scratch), OwnerOnly, std::move(RuntimeLease)
#if defined(BPR_BUILD_TESTING)
																																																											   ,
																		Fixture, std::move(Options)
#endif
				);
				profile->ActivationServer = std::move(ActivationServer);
				profile->DataResetReport  = std::move(DataResetReport);
				profile->TourOwnerServer  = std::move(TourOwnerServer);
				profile->TourOwnerService = std::move(TourOwnerService);
#if defined(BPR_LOCAL_AUTOMATION)
				profile->CycleOwnerService = std::move(CycleOwnerService);
				if (profile->CycleOwnerService) {
					// [[BPR-1830]]. Forwarded cycle commands record their rounds in the profile's log.
					profile->CycleOwnerService->set_diagnostics(profile->DiagnosticLog.get());
				}
#endif
				auto runtime = std::unique_ptr<github_application>{ new github_application{ std::move(profile) } };
				step         = "profile-services-start";
				runtime->Profile->start_services();
				if (!OwnerOnly) {
					step               = "compatibility-window-open";
					const auto context = compatibility_window_context(aTerminal.Result->Authority.profile());
					if (!runtime->open_window(context)) {
						throw std::runtime_error{ "initial window unavailable" };
					}
					runtime->Profile->Focused = context;
				}
				finish(std::move(runtime));
			} catch (...) {
				// A throw from the completion `finish` already ran is not this
				// startup refusing, so only an unfinished startup is named.
				if (!Terminal) {
					detail::note_startup_failure(step, std::current_exception());
				}
				finish(std::unexpected(github_application_startup_failure::unavailable));
			}
		}

	private:
		void finish(github_application_startup_result aResult)
		{
			if (Terminal) {
				return;
			}
			auto keepAlive = shared_from_this();
			Terminal       = true;
			RuntimeLease.reset();
			auto completion = std::move(Completion);
			if (completion) {
				completion(std::move(aResult));
			}
		}
		github_application_config                               Config;
		std::shared_ptr<application::profile_runtime_lease>     RuntimeLease;
		std::shared_ptr<application::profile_activation_server> ActivationServer;
		std::shared_ptr<application::tour_owner_server>         TourOwnerServer;
		std::shared_ptr<const tour_owner_service>               TourOwnerService;
#if defined(BPR_LOCAL_AUTOMATION)
		std::shared_ptr<cycle_owner_service> CycleOwnerService;
#endif
		std::optional<github_application_startup_failure>                                  Handoff;
		std::optional<profile_data_reset_report>                                           DataResetReport;
		github_application_startup_completion                                              Completion;
		std::filesystem::path                                                              RegistryRoot;
		bool                                                                               RegistryRootNested = false;
		std::shared_ptr<void>                                                              Scratch;
		std::optional<application::durable_queue_state>                                    SavedQueue;
		std::unique_ptr<application::remembered_connection_store_port>                     ConnectionStore;
		application::remembered_connection_store_result<core::remembered_connection_state> SavedConnections = std::unexpected(application::remembered_connection_store_failure::unavailable);
#if defined(BPR_BUILD_TESTING)
		std::optional<detail::github_public_path_fixture> Fixture;
		detail::github_public_path_fixture_options        Options;
#endif
		worker_executor                   Effects;
		qt::event_loop_executor           Decisions;
		application::profile_startup_host Host;
		bool                              Cancelled = false;
		bool                              Terminal  = false;
	};
	github_application_startup::github_application_startup(std::shared_ptr<implementation> aImplementation)
		: Implementation{ std::move(aImplementation) }
	{ }
	github_application_startup::~github_application_startup() { Implementation->cancel(); }
	void github_application_startup::cancel() { Implementation->cancel(); }
	auto github_application_startup::is_quiescent() const noexcept -> bool { return Implementation->is_quiescent(); }
	auto github_application::start(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state  = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion));
		auto handle = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->begin();
		return handle;
	}
	auto github_application::start_profile(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state       = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion));
		auto handle      = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->OwnerOnly = true;
		state->begin();
		return handle;
	}

	github_application::github_application(std::shared_ptr<github_profile_runtime> aProfile) noexcept
		: Profile{ std::move(aProfile) }
	{ }
	github_application::~github_application()
	{
		// [[BPR-0827]]. First, before shutdown begins and before the drain below
		// pumps events, so a poller tick that the drain delivers finds the token
		// expired rather than re-entering windows that are already shutting down.
		// BPR-0705 cleared it after the drain, and every such tick still found
		// the application usable.
		Liveness.reset();
		begin_shutdown();
		// The existing fallback drain now belongs to the profile facade, after all
		// window sessions have been told to stop. Normal desktop close is already quiescent.
		while (!profile_shutdown_complete()) {
			QCoreApplication::processEvents(QEventLoop::AllEvents);
			std::this_thread::yield();
		}
		SessionCloseLifetime.reset();
	}
	auto github_application::liveness() const noexcept -> std::weak_ptr<const void> { return Liveness; }
	void github_application::show()
	{
		if (!InitialRetired && Implementation) {
			Implementation->Window->show();
		}
	}
	auto github_application::desktop_registry_root() const -> std::filesystem::path { return Profile->RegistryRoot; }
	auto github_application::desktop_update_recovery_root() const -> std::filesystem::path { return Profile->ApplicationDataRoot; }
	auto github_application::desktop_update_ready() const noexcept -> bool
	{
		return (!Profile->ReviewRestoration || Profile->ReviewRestoration->durable())
			&& (!Profile->AppearancePreferences || Profile->AppearancePreferences->durable())
			&& (!Profile->WorkbenchPreferences || (!Profile->WorkbenchPreferences->is_busy() && Profile->WorkbenchPreferences->is_quiescent() && !Profile->WorkbenchPreferences->delivery_failed()));
	}
	void github_application::desktop_update_freeze(bool aFreeze)
	{
		Profile->UpdateShutdown = aFreeze;
		if (Profile->Settings) {
			Profile->Settings->setEnabled(!aFreeze);
		}
		for (const auto& [context, session] : Sessions) {
			static_cast<void>(context);
			if (session->Window) {
				session->Window->setEnabled(!aFreeze);
			}
		}
	}
	auto github_application::desktop_update_drafts() const -> std::vector<application::release_update_saved_draft>
	{
		std::vector<application::release_update_saved_draft> result;
		for (const auto& owner : Profile->DraftOwners) {
			const auto& authority = owner.Host->authority();
			for (const auto& saved : owner.Host->recovery_snapshot()) {
				const auto& draft = saved.Draft;
				const auto  scope = std::string{ authority.ProfileAuthority.Hostname.value() } + " · account " + std::to_string(authority.Account.value())
					+ " · repository " + std::to_string(authority.Target.Repository.value()) + " · pull request " + std::to_string(authority.Target.PullRequest.value())
					+ " · head " + std::string{ core::canonical_hex(authority.Target.ExpectedHead).view() }
					+ " · " + (draft.Anchor.Side == core::diff_side::right ? "right" : "left")
					+ (draft.StartLine ? " lines " + std::to_string(*draft.StartLine) + "-" + std::to_string(draft.Anchor.Line) : " line " + std::to_string(draft.Anchor.Line));
				result.push_back({ "inline:" + std::to_string(owner.Scope.value()) + ":" + std::to_string(draft.Id.value()), scope, draft.Body,
								   draft.State == core::review_draft_state::publishing });
			}
		}
		for (const auto& [context, session] : Sessions) {
			if (!session->Window) {
				continue;
			}
			const auto identity = "window:" + std::to_string(context.Window.value());
			if (const auto* editor = session->Window->review_editor(); editor && editor->body_editor() && !editor->body_editor()->toPlainText().isEmpty()) {
				result.push_back({ identity + ":editor", "Unsubmitted inline editor text (including any conflicting edit)", editor->body_editor()->toPlainText().toStdString(),
								   session->PublishingDraft.has_value() });
			}
			if (const auto* editor = session->Window->review_submission_editor_view(); editor && editor->shown_draft() && editor->body_editor()) {
				const auto& draft = *editor->shown_draft();
				const auto  body  = editor->body_editor()->toPlainText().toStdString();
				if (!body.empty()) {
					result.push_back({ identity + ":review", "Whole review · account " + std::to_string(draft.ActingAccount.value()) + " · pull request " + std::to_string(draft.Target.PullRequest.value()) + " · head " + std::string{ core::canonical_hex(draft.Target.ExpectedHead).view() },
									   body, draft.State != core::review_submission_state::drafting });
				}
			}
		}
		return result;
	}
	auto github_application::desktop_release_account() -> std::optional<release_update_account> { return Profile->release_account(); }
	auto github_application::desktop_diagnostic_log() const -> std::shared_ptr<application::diagnostic_log_port> { return Profile->DiagnosticLog; }
	auto github_application::desktop_activation_server() const -> std::shared_ptr<application::profile_activation_server> { return Profile->ActivationServer; }
	auto github_application::desktop_tour_owner_server() const -> std::shared_ptr<application::tour_owner_server> { return Profile->TourOwnerServer; }
	namespace {
		auto tour_owner_error(std::string aCode, std::string aMessage = { }) -> application::tour_channel_bytes
		{
			return adapters::sqlite::encode_tour_response({ .Error = std::move(aCode), .Json = std::move(aMessage) });
		}
		auto assist_refusal_code(std::string_view aReason) -> std::string { return std::string{ aReason }; }
	}
	void github_application::desktop_tour_owner_route(application::tour_channel_bytes aRequest, std::function<void(application::tour_channel_bytes)> aRespond)
	{
		auto& profile = *Profile;
		if (Profile->Closing || !profile.TourOwnerService) {
			aRespond(tour_owner_error("owner_unavailable"));
			return;
		}
		const auto request = adapters::sqlite::decode_tour_request(aRequest);
		if (!request) {
			aRespond(tour_owner_error("invalid_request"));
			return;
		}
		if (request->Operation == application::tour_owner_operation::cycle) {
			desktop_cycle_owner_route(std::move(aRequest), request->Cycle, std::move(aRespond));
			return;
		}
		// The draft is read or written where its author is: an open window's host is the one committed
		// history, so a forwarded command never runs beside it on a second copy read from the store.
		const auto found = [&]() -> implementation::live_tour_view {
			implementation::live_tour_view result;
			for (const auto& [context, session] : Sessions) {
				static_cast<void>(context);
				const auto seen = session->live_tour(request->Draft);
				if (seen.Host && (!result.Host || (seen.Current && !result.Current))) {
					result = seen;
				}
			}
			return result;
		}();
		const auto serve = [service = profile.TourOwnerService, &effects = profile.WorkbenchEffects](application::tour_channel_bytes aRead, std::function<void(application::tour_channel_bytes)> aReply) {
			effects.post([service, read = std::move(aRead), reply = std::move(aReply)] { reply(service->serve(read)); });
		};
		if (request->Operation == application::tour_owner_operation::source) {
			// [[BPR-1227]]. Source is read where the pull request is open, by that window's own provider
			// composition: no second process reads the provider or opens a profile store, and nothing
			// is written. A draft no window shows is refused by the service, provider-free.
			if (request->Source.LastLine - request->Source.FirstLine >= 200) {
				aRespond(tour_owner_error("source_too_large"));
				return;
			}
			for (const auto& [context, session] : Sessions) {
				static_cast<void>(context);
				if (session->read_tour_source(request->Draft, request->Source, aRespond)) {
					return;
				}
			}
			serve(std::move(aRequest), std::move(aRespond));
			return;
		}
		if (!found.Host) {
			serve(std::move(aRequest), std::move(aRespond));
			return;
		}
		if (found.Composing) {
			// Preedit text is not committed text. It is neither flushed nor read past.
			aRespond(tour_owner_error("editor_busy"));
			return;
		}
		const auto  accepted    = profile.WorkbenchPreferences ? profile.WorkbenchPreferences->accepted() : std::optional<application::workbench_profile_preferences>{ };
		const auto* preferences = accepted ? &*accepted : nullptr;
		const auto  isMutation  = request->Operation == application::tour_owner_operation::mutate;
		const bool  assisting   = isMutation && (request->Edit.Operation == application::tour_service_operation::assist_complete || request->Edit.Operation == application::tour_service_operation::outline_use);
		std::string outcomeJson;
		auto        read = application::tour_owner_request{ .Operation = isMutation ? application::tour_owner_operation::request_status : (request->Operation == application::tour_owner_operation::assist_issue || request->Operation == application::tour_owner_operation::assist_cancel) ? application::tour_owner_operation::load
																																																																							: request->Operation,
															.Draft     = request->Draft };
		if (isMutation) {
			auto edit = request->Edit;
			if (edit.RequestId.empty()) {
				aRespond(tour_owner_error("request_id_required"));
				return;
			}
			const auto draft = found.Host->presentation().Draft;
			if (!draft) {
				aRespond(tour_owner_error("owner_unavailable"));
				return;
			}
			edit.Command.Owner = draft->Owner;
			edit.Command.Draft = draft->Id;
			if (assisting) {
				// The receiver's own clock and hook revision; the caller supplied neither.
				if (!preferences) {
					aRespond(tour_owner_error("owner_busy"));
					return;
				}
				edit.Assist.Now                 = tour_assist_now();
				edit.Assist.CurrentHookRevision = preferences->WritingHooks.Revision;
			}
			const auto result = found.Host->execute(edit);
			if (result.Status == application::tour_service_status::busy) {
				aRespond(tour_owner_error("owner_busy"));
				return;
			}
			if (result.Status == application::tour_service_status::refused) {
				if (assisting && !result.Reason.empty()) {
					aRespond(tour_owner_error(assist_refusal_code(result.Reason), result.Detail));
					return;
				}
				aRespond(tour_owner_error(result.Refusal == core::tour_refusal::conflict ? "revision_conflict" : "command_refused"));
				return;
			}
			if (assisting) {
				outcomeJson = tour_assist_outcome_json(result, *found.Host);
			}
			read.Edit.RequestId = edit.RequestId;
		} else if (request->Operation == application::tour_owner_operation::assist_issue) {
			if (!preferences) {
				aRespond(tour_owner_error("owner_busy"));
				return;
			}
			const auto&       q = request->Assist;
			tour_assist_query query{ .Hook = q.Hook, .Stop = q.Stop, .Range = q.HasRange ? std::optional{ std::pair{ q.Begin, q.End } } : std::nullopt, .Quote = q.HasQuote ? std::optional{ q.Quote } : std::nullopt, .Session = q.Session };
			auto              issued = tour_assist_issue(*found.Host, preferences->WritingHooks, query, { }, tour_assist_now());
			if (!issued) {
				aRespond(tour_owner_error(issued.error().Code, issued.error().Message));
				return;
			}
			outcomeJson = issued->ContextJson;
		} else if (request->Operation == application::tour_owner_operation::assist_cancel) {
			found.Host->assist_cancel();
		} else {
			read.Edit.RequestId = request->Edit.RequestId;
		}
		profile.WaitingTourRequests.push_back({ request->Draft, std::move(outcomeJson), adapters::sqlite::encode_tour_request(read), std::move(aRespond), std::chrono::steady_clock::now() + std::chrono::seconds{ 5 } });
		desktop_tour_owner_tick();
	}
	void github_application::desktop_cycle_owner_route(application::tour_channel_bytes aRequest, application::cycle_owner_request aCycle, std::function<void(application::tour_channel_bytes)> aRespond)
	{
#if defined(BPR_LOCAL_AUTOMATION)
		auto& profile = *Profile;
		if (Profile->Closing || !profile.CycleOwnerService) {
			aRespond(tour_owner_error("owner_unavailable"));
			return;
		}
		const auto liveness  = std::weak_ptr<const int>{ Liveness };
		auto&      decisions = profile.DecisionLane;
		const auto stop      = profile.CycleOwnerStop.get_token();
		// Before the command reads or changes a round draft a window may hold, the
		// core decides from what the windows hold; the windows answer on their lane.
		auto guard = [this, liveness, &decisions, stop](const core::cycle_command aCommand, const core::review_cycle_id aTarget) -> cycle_result<void> {
			const auto ask = [&](const bool aSave) -> std::optional<core::cycle_draft_editor> {
				auto promise = std::make_shared<std::promise<core::cycle_draft_editor>>();
				auto future  = promise->get_future();
				decisions.post([this, liveness, promise, aTarget, aSave] {
					promise->set_value(liveness.expired() ? core::cycle_draft_editor::settled : cycle_draft_editors(aTarget, aSave));
				});
				if (future.wait_for(std::chrono::seconds{ 1 }) != std::future_status::ready) {
					return std::nullopt;
				}
				return future.get();
			};
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
			auto       saved    = false;
			while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) {
				const auto editor = ask(!saved);
				if (!editor) {
					continue;
				}
				const auto admission = core::admit_owner_command(aCommand, *editor);
				if (admission == core::cycle_owner_admission::serve) {
					return { };
				}
				if (admission == core::cycle_owner_admission::busy) {
					return std::unexpected(cycle_failure{ "draft_busy", "The desktop is composing text in this round's draft (an input method is open). Finish or cancel it, then retry; nothing was changed.", { }, { } });
				}
				// The window saves what it holds; this command then reads what was saved.
				saved = true;
				std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
			}
			return std::unexpected(cycle_failure{ "draft_busy", "The desktop is still saving this round's draft. Retry shortly; nothing was changed.", { }, { } });
		};
		profile.CycleOwnerEffects.post([service = profile.CycleOwnerService, payload = std::move(aRequest), cycle = std::move(aCycle), admit = std::move(guard), respond = std::move(aRespond), stop, this, liveness, &decisions] {
			auto answer = service->serve(payload, cycle, admit, [stop] { return stop.stop_requested(); });
			respond(std::move(answer.Bytes));
			if (answer.Changed) {
				decisions.post([this, liveness, changed = *answer.Changed] {
					if (!liveness.expired()) {
						cycle_written(changed);
					}
				});
			}
		});
#else
		// [[BPR-1179]]. No command channel is bound without local automation, so
		// nothing reaches here; a forwarded request is still answered, never served.
		static_cast<void>(aRequest);
		static_cast<void>(aCycle);
		aRespond(tour_owner_error("owner_unavailable"));
#endif
	}
	auto github_application::cycle_draft_editors(const core::review_cycle_id aCycle, const bool aSave) -> core::cycle_draft_editor
	{
		auto held = core::cycle_draft_editor::settled;
		for (const auto& [context, session] : Sessions) {
			static_cast<void>(context);
			if (session->Closing || !session->CycleDesktop) {
				continue;
			}
			const auto editor = session->CycleDesktop->draft_editor(aCycle);
			if (editor == core::cycle_draft_editor::unsaved && aSave) {
				session->CycleDesktop->save_draft_editor(aCycle);
			}
			held = std::max(held, editor);
		}
		return held;
	}
	void github_application::cycle_written(const core::review_cycle_id aCycle)
	{
		for (const auto& [context, session] : Sessions) {
			static_cast<void>(context);
			if (!session->Closing && session->CycleDesktop) {
				session->CycleDesktop->external_change(aCycle);
			}
		}
	}
	void github_application::desktop_tour_owner_tick()
	{
		auto& profile = *Profile;
		for (auto waiting = profile.WaitingTourRequests.begin(); waiting != profile.WaitingTourRequests.end();) {
			std::optional<std::string> failure;
			bool                       ready = true;
			for (const auto& [context, session] : Sessions) {
				static_cast<void>(context);
				const auto seen = session->live_tour(waiting->Token);
				if (!seen.Host) {
					continue;
				}
				const auto shown = seen.Host->presentation();
				if (shown.Failure) {
					failure = "storage_unavailable";
				} else if (shown.Conflict) {
					failure = "revision_conflict";
				} else if (!shown.Draft || shown.Loading) {
					ready = false;
				} else {
					// A draft nobody has touched is never persisted, so there is nothing to wait for.
					const bool untouched = shown.SavedRevision == 0U && shown.Draft->Revision == 1U && shown.Draft->Problem.empty() && shown.Draft->Solution.empty() && shown.Draft->Stops.empty() && !shown.Draft->ProblemBaseline && !shown.Draft->SolutionBaseline;
					if (!untouched && (shown.Saving || shown.SavedRevision != shown.Draft->Revision || !seen.Host->is_quiescent())) {
						ready = false;
					}
				}
			}
			if (!failure && !ready && !Profile->Closing && std::chrono::steady_clock::now() < waiting->Deadline) {
				++waiting;
				continue;
			}
			// A command that was applied but is not yet durable is uncertain to its caller, who inspects
			// the exact request ID rather than replaying it.
			auto entry = std::move(*waiting);
			waiting    = profile.WaitingTourRequests.erase(waiting);
			if (failure) {
				entry.Respond(tour_owner_error(*failure));
			} else if (!ready) {
				entry.Respond(tour_owner_error(Profile->Closing ? "owner_unavailable" : "save_pending"));
			} else {
				profile.WorkbenchEffects.post([service = profile.TourOwnerService, read = std::move(entry.Read), respond = std::move(entry.Respond), json = std::move(entry.Json)] {
					auto answer = service->serve(read);
					if (!json.empty()) {
						// The durable answer, carrying what the live host said: context or the outcome of a completion.
						if (auto decoded = adapters::sqlite::decode_tour_response(answer); decoded && decoded->Error.empty()) {
							decoded->Json = json;
							answer        = adapters::sqlite::encode_tour_response(std::move(*decoded));
						}
					}
					respond(std::move(answer));
				});
			}
		}
	}
	auto github_application::desktop_scratch() const -> std::shared_ptr<void> { return Profile->StartupScratch; }
	void github_application::set_desktop_quit_handler(std::function<void(int)> aHandler) { Profile->DesktopQuit = std::move(aHandler); }
	void github_application::set_maintenance_retirement(release_maintenance_retirement aRetire) { Profile->MaintenanceRetirement = std::move(aRetire); }
	auto github_application::take_profile_data_reset_report() -> std::optional<profile_data_reset_report> { return std::exchange(Profile->DataResetReport, std::nullopt); }
	auto github_application::take_maintenance() -> std::unique_ptr<release_maintenance_controls>
	{
		if (!Profile->Maintenance || !Profile->Settings) {
			return { };
		}
		std::function<void()> unmount;
		if (Profile->SettingsOwner) {
			const auto found = Profile->Observers.find(*Profile->SettingsOwner);
			if (found != Profile->Observers.end()) {
				unmount = found->second.UnmountSettings;
			}
		}
		Profile->Maintenance->retain_settings(std::move(Profile->Settings), Profile->Admission.Label, std::move(unmount));
		Profile->SettingsOwner.reset();
		return std::move(Profile->Maintenance);
	}
	void github_application::set_profile_label(std::string aLabel)
	{
		Profile->Admission.Label = std::move(aLabel);
		const auto label         = QString::fromStdString(Profile->Admission.Label);
		if (Profile->Settings) {
			Profile->Settings->set_profile_label(label);
		}
		for (const auto& [context, session] : Sessions) {
			if (!session->Closing) {
				session->Window->set_profile_label(label);
			}
		}
	}
	void github_application::set_window_admission(std::function<bool(core::bpr_window_context)> aAdmission) { Profile->WindowAdmission = std::move(aAdmission); }
	void github_application::set_review_window_request(std::function<bool(const core::window_review_destination&)> aRequest) { Profile->ReviewWindowRequest = std::move(aRequest); }
	void github_application::open_window_review(core::bpr_window_context aContext, const core::window_review_destination& aDestination)
	{
		const auto found = Sessions.find(aContext);
		if (found == Sessions.end() || found->second->Closing) {
			return;
		}
		found->second->open_review_destination(aDestination);
	}
	void github_application::request_quit(int aExitCode)
	{
		if (Profile->DesktopQuit) {
			Profile->DesktopQuit(aExitCode);
		} else {
			QCoreApplication::exit(aExitCode);
		}
	}
	auto github_application::open_window(core::bpr_window_context aContext) -> bool
	{
		if (Profile->Closing || aContext.Profile != Profile->Admission.Authority.profile() || aContext.Window.value() == 0 || aContext.Generation.value() == 0 || Sessions.contains(aContext)) {
			return false;
		}
		if (Profile->WindowAdmission && !Profile->WindowAdmission(aContext)) {
			return false;
		}
		for (const auto& [context, session] : Sessions) {
			if (context.Window == aContext.Window) {
				return false;
			}
		}
		try {
			auto session = std::make_shared<implementation>(Profile, aContext);
			Sessions.emplace(aContext, session);
			// Not `liveness()`: this callback erases the session and starts profile
			// shutdown, so it has to run inside the destructor's own drain, after
			// that token has expired (BPR-0827).
			auto weak         = std::weak_ptr<const void>{ SessionCloseLifetime };
			session->OnClosed = [this, weak, aContext] {
				if (weak.expired()) {
					return;
				}
				const auto found = Sessions.find(aContext);
				if (found == Sessions.end()) {
					return;
				}
				if (found->second->ShutdownCleanupFailed) {
					// [[BPR-0875]]. A window's cleanup refusal becomes the
					// profile's; its name travels with it.
					Profile->note_profile_cleanup_failure(found->second->ShutdownCleanupDetail);
					Profile->CleanupFailed = true;
				}
				Sessions.erase(found);
				if (Profile->Closing && Sessions.empty()) {
					Profile->begin_shutdown();
				}
			};
			session->start();
			// [[BPR-1273]]. Add Notes to Review Tour edits a pull request's tour
			// where its draft lives, through this profile's tour owner, as
			// `bpr tour` does: the window holding that draft, or the stored one.
			if (session->CycleDesktop) {
				session->CycleDesktop->set_tour_route([this, live = std::weak_ptr<const int>{ Liveness }](application::tour_channel_bytes aRequest, std::function<void(application::tour_channel_bytes)> aRespond) {
					if (live.expired()) {
						aRespond(tour_owner_error("owner_unavailable"));
						return;
					}
					desktop_tour_owner_route(std::move(aRequest), std::move(aRespond));
				});
			}
			if (Profile->AgentWorkspaceRecoveryRefused) {
				session->present_agent_workspace_repair();
			}
			if (Profile->ReviewRestoration) {
				static_cast<void>(Profile->ReviewRestoration->attach(aContext));
				if (Profile->RestorationGlobalNotice) {
					session->restoration_notice(*Profile->RestorationGlobalNotice);
				}
				if (const auto notice = Profile->RestorationNotices.find(aContext); notice != Profile->RestorationNotices.end()) {
					session->restoration_notice(notice->second);
				}
				Profile->ReviewRestoration->mark_window_ready(aContext);
				session->publish_recent();
			}
			if (!Implementation) {
				Implementation       = std::move(session);
				Profile->FirstWindow = aContext;
			}
			return true;
		} catch (...) {
			Profile->Observers.erase(aContext);
			const auto found = Sessions.find(aContext);
			if (found != Sessions.end()) {
				found->second->begin_shutdown();
			}
			return false;
		}
	}
	auto github_application::configure_desktop_layouts(const std::vector<core::bpr_window_context>& aWindows) -> bool
	{
		if (Profile->WindowLayouts || Profile->ReviewRestoration) {
			return true;
		}
		if (Profile->FirstWindow || aWindows.empty() || aWindows.size() > core::maximum_bpr_windows_per_profile) {
			return false;
		}
		auto keep = std::vector<core::bpr_window_id>{ };
		for (const auto& context : aWindows) {
			if (context.Profile != Profile->Admission.Authority.profile() || context.Window.value() == 0 || context.Generation.value() == 0 || std::ranges::find(keep, context.Window) != keep.end()) {
				return false;
			}
			keep.push_back(context.Window);
		}
		// Per-window durable layouts only where composition found the store
		// usable. Otherwise every window takes the session defaults, as the
		// profile's own settings port already does, so a store that cannot open
		// yields the composed defaults instead of a window that never publishes
		// a layout and reports no drag (BPR-0718).
		if (Profile->WorkbenchPersistenceAvailable) {
			Profile->WindowLayouts = std::make_shared<profile_window_layout_service>(Profile->ApplicationDataRoot, std::move(keep));
		}
		auto hosts = std::vector<std::string>{ };
		for (const auto& profile : Profile->Profiles) {
			hosts.emplace_back(profile.hostname());
		}
		Profile->ReviewRestorationStore = std::make_unique<effect_window_review_store>(Profile->ApplicationDataRoot);
#if defined(BPR_BUILD_TESTING)
		auto& restorationDecisions = Profile->RestorationDecisionGate;
#else
		auto& restorationDecisions = Profile->DecisionLane;
#endif
		Profile->ReviewRestoration = std::make_unique<application::window_review_restoration_host>(Profile->Admission.Authority.profile(), aWindows, std::move(hosts), *Profile->ReviewRestorationStore, Profile->WorkbenchEffects, restorationDecisions, *Profile);
		Profile->ReviewRestoration->start();
		return true;
	}

	void github_application::close_window(core::bpr_window_context aContext, window_retirement aMode)
	{
		const auto found = Sessions.find(aContext);
		if (found == Sessions.end()) {
			return;
		}
		if (found->second == Implementation) {
			InitialRetired = true;
		}
		found->second->begin_shutdown(aMode);
	}
	void github_application::focus_window(core::bpr_window_context aContext)
	{
		auto* selected = window(aContext);
		if (!selected) {
			return;
		}
		Profile->Focused = aContext;
		selected->show();
		if (!selected->isActiveWindow()) {
			selected->raise();
			selected->activateWindow();
		}
	}
	auto github_application::window_quiescent(core::bpr_window_context aContext) const noexcept -> bool
	{
		const auto found = Sessions.find(aContext);
		// The queued close callback still folds cleanup results and retires the
		// session. Profile shutdown must not force that orderly window while
		// this ownership handoff is pending.
		return found == Sessions.end();
	}
	auto github_application::windows() const -> std::vector<core::bpr_window_context>
	{
		auto result = std::vector<core::bpr_window_context>{ };
		for (const auto& [context, session] : Sessions) {
			if (!session->Closing) {
				result.push_back(context);
			}
		}
		return result;
	}
	auto github_application::window(core::bpr_window_context aContext) noexcept -> qt::workbench_window*
	{
		const auto found = Sessions.find(aContext);
		return found == Sessions.end() || found->second->Closing || !Profile->current_window(aContext) ? nullptr : found->second->Window.get();
	}
	auto github_application::window() -> qt::workbench_window&
	{
		if (InitialRetired || !Implementation || !Implementation->Window) {
			throw std::logic_error{ "initial window retired" };
		}
		return *Implementation->Window;
	}
	void github_application::begin_shutdown()
	{
		if (Profile->Closing) {
			return;
		}
		Profile->Closing = true;
		cli_account_users().set(Profile.get(), std::nullopt);
		if (Profile->ReviewRestoration && !Profile->RestorationOrderly) {
			Profile->ReviewRestoration->shutdown();
		}
		for (const auto& [context, session] : Sessions) {
			session->begin_shutdown();
		}
		if (Sessions.empty()) {
			Profile->begin_shutdown();
		}
	}
	auto github_application::profile_shutdown_complete() const noexcept -> bool
	{
		Profile->poll_shutdown();
		return Profile->ShutdownComplete;
	}
	auto github_application::profile_shutdown_cleanup_failed() const noexcept -> bool { return Profile->CleanupFailed; }
	// [[BPR-0875]]. What the profile's flag above is about, for the desktop's
	// exit code and for a person reading a real quit.
	auto github_application::profile_shutdown_cleanup_detail() const -> std::string { return Profile->CleanupDetail; }

	// [[BPR-0875]]. The profile's name once it has one, else this window's.
	auto github_application::shutdown_cleanup_detail() const -> std::string
	{
		if (!Profile->CleanupDetail.empty()) {
			return Profile->CleanupDetail;
		}
		return Implementation ? Implementation->ShutdownCleanupDetail : std::string{ };
	}

	auto github_application::implementation::status() const noexcept -> github_application_status
	{
		// How many spans the pane on screen carries right now. Read here rather
		// than counted as it is adopted, because the question is about the
		// present state of the surface and a count kept alongside would only
		// agree with it by luck. BPR-0390.
		const auto displayedSpanCount
			= [this](const core::diff_side aSide) noexcept -> std::size_t {
			if (this->Window == nullptr) {
				return 0U;
			}
			const auto& spans
				= this->Window->comparison_surface()->token_spans(aSide);
			return spans ? spans->size() : 0U;
		};
		return {
			.Closing                       = this->Closing,
			.ShutdownComplete              = this->ShutdownComplete,
			.SourceSearchQuiescent         = !PrFind || PrFind->is_quiescent(),
			.SourceSearchScans             = PrFind ? PrFind->statistics().Scans : 0U,
			.ShutdownCleanupFailed         = this->ShutdownCleanupFailed,
			.AgentWorkspaceCleanupFailed   = this->AgentWorkspaceCleanupFailed,
			.AgentWorkspaceRecoveryRefused = this->Profile->AgentWorkspaceRecoveryRefused,
			.NativePlatformVaultSelected   = this->NativePlatformVaultSelected,
			.EpochProjectionDeliveryFailed = this->EpochProjectionDeliveryFailed,
			.EpochHandoffDeliveryFailed    = this->WindowRead && this->WindowRead->epoch_handoff_delivery_failed(),
			.WorkbenchHostComposed         = this->WorkbenchHost != nullptr,
			.AppearanceHostComposed        = this->AppearanceHost != nullptr,
#if defined(BPR_AGENT_INTEGRATIONS)
			.AgentHostComposed = this->AgentHost != nullptr,
#else
			.AgentHostComposed = false,
#endif
			.AppearancePersistenceAvailable = this->AppearancePersistenceAvailable,
			.AppearanceDurableWrites        = this->AppearanceHost != nullptr
				? this->AppearanceHost->persisted_settings()
				: 0U,
			.AppliedSelectionColor          = this->AppliedSelectionColor,
			.AppliedBaseColor               = this->AppliedBaseColor,
			.AppliedFromProfile             = this->AppliedFromProfile,
			.ObservedScheme                 = this->AppearanceProfile
				? this->AppearanceProfile->observation().Scheme
				: core::os_color_scheme::unknown,
			.ResolvedVariant                = this->AppearanceProfile
				? this->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.CustomisedRoles                = this->AppearanceProfile
				? this->AppearanceProfile->customised_roles()
				: 0U,
			.SyntaxClassifierComposed       = this->SyntaxClassification != nullptr,
			.AdoptedClassifications         = this->AdoptedClassifications,
			.ClassificationSubmissions      = this->AppearanceHost != nullptr
				? this->AppearanceHost->classification_submissions()
				: 0U,
			.ClassificationCacheHits        = this->AppearanceHost != nullptr
				? this->AppearanceHost->cache_hits()
				: 0U,
			.CoalescedClassifications       = this->AppearanceHost != nullptr
				? this->AppearanceHost->coalesced_classifications()
				: 0U,
			.DroppedClassifications         = this->DroppedClassifications,
			.ClassificationFailures         = this->AppearanceHost != nullptr
				? this->AppearanceHost->classification_failures()
				: 0U,
			.ClassificationTerminals        = this->AppearanceHost != nullptr
				? this->AppearanceHost->classification_terminals()
				: 0U,
			.DisplayedLeftClassified        = this->Window != nullptr
				&& static_cast<bool>(this->Window->comparison_surface()
										 ->token_spans(core::diff_side::left)),
			.DisplayedRightClassified = this->Window != nullptr
				&& static_cast<bool>(this->Window->comparison_surface()
										 ->token_spans(core::diff_side::right)),
			.DisplayedLeftSpans = displayedSpanCount(
				core::diff_side::left),
			.DisplayedRightSpans = displayedSpanCount(
				core::diff_side::right),
			.DisplayedComparison = this->Window != nullptr
				&& static_cast<bool>(this->Window->comparison_surface()
										 ->current_result()),
			.DisplayAdoptedClassifications
			= this->DisplayAdoptedClassifications,
			.AppearanceMode                = this->AppearanceHost != nullptr
				? std::string{ core::stable_name(
					  this->AppearanceHost->settings().Mode) }
				: std::string{ },
			.AppearanceTabWidth            = this->AppearanceHost != nullptr
				? this->AppearanceHost->settings().Typography.Code.TabWidth
				: 0U,
			.DifResolution                 = this->AppearanceHost != nullptr
				? this->AppearanceHost->dif_resolution()
				: core::dif_resolution{ },
			.DifCatalogLoadAttempts        = this->Profile->Config.DifCatalog
				? this->Profile->Config.DifCatalog->load_attempts()
				: 0U,
			.DifCatalogDecodedClips        = this->Profile->Config.DifCatalog
				? this->Profile->Config.DifCatalog->decoded_count()
				: 0U,
			.AppearanceDivergences         = this->AppearanceDivergences,
			.ProfileMode                   = this->AppearanceProfile
				? this->AppearanceProfile->mode()
				: core::appearance_mode::system,
			.ProfileVariant                = this->AppearanceProfile
				? this->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.NotificationDeliveryComposed  = this->Notifications != nullptr,
			.NotificationPermission        = this->NotificationSink != nullptr
				? this->NotificationSink->permission()
				: application::notification_permission::refused,
			.NotificationRequestsAttempted = this->NotificationRequestsAttempted,
			.NotificationRequestsSubmitted = this->Notifications != nullptr
				? this->Notifications->statistics().Submitted
				: 0U,
			.NotificationRequestsSuppressedUnattributable
			= this->Notifications != nullptr
				? this->Notifications->statistics()
					  .SuppressedUnattributableActivation
				: 0U,
			.NotificationActivations       = this->Notifications != nullptr
				? this->Notifications->statistics().Activations
				: 0U,
			.NotificationSelections        = this->NotificationSelections,
			.NotificationSelectionFailures = this->NotificationSelectionFailures,
			.EpochStatus                   = this->EpochHost->state().Status,
			.DeviceAuthorizationHostComposed
			= this->DeviceHost != nullptr,
			.DeviceAuthorizationStage
			= this->DeviceProjections.last().Stage,
			.DeviceAuthorizationCapabilityRegistered
			= this->DeviceCapabilityRegistered,
			.DeviceAuthorizationTransportReached
			= this->UnregisteredDeviceTransport.reached(),
			.DeviceAuthorizationPublications
			= this->DeviceProjections.published(),
			.DeviceAuthorizationBrowserComposed
			= this->DeviceBrowserComposed,
			.DeviceAuthorizationBrowser = this->ComposedDeviceBrowser,
			// [[BPR-0797]]. Asked of the factory this run holds, so a fixture
			// run reports the fixture's answer and a shipped-selection run
			// reports the one the account row is offering.
			.ComposedCliReadAvailable = this->TransportFactory != nullptr
				&& this->TransportFactory->supports_cli(),
#if defined(BPR_BUILD_TESTING)
			// [[BPR-0802]]. The composition's own answer, from its one reader.
			.InsideAutomatedTestBoundary
			= this->Profile->InsideAutomatedTestBoundary,
#endif
			.DeviceAuthorizationHandoff
			= this->DeviceProjections.last_attempt(),
			.DeviceAuthorizationBrowserOpens
			= this->device_browser_opens(),
			.DeviceAuthorizationVaultStores
			= this->device_vault_stores(),
			.DeviceAuthorizationDurableCopies
			= this->device_durable_copies(),
			.DeviceAuthorizationTransportSubmissions
			= this->device_transport_submissions(),
			.DeviceAuthorizationVaultStoredDigest
			= this->device_vault_stored_digest(),
			.DeviceAuthorizationVaultStoredIsGrant
			= this->device_vault_stored_is_grant(),
			.DeviceAuthorizationEnrollments
			= this->device_enrollments(),
			.DeviceAuthorizationPanelComposed
			= this->DevicePanel != nullptr,
			.DeviceAuthorizationRefreshRotations
			= this->DeviceRefreshObservation.Rotations,
			.DeviceAuthorizationRefreshState
			= this->DeviceRefreshObservation.State,
			.DeviceAuthorizationRefreshBindings
			= this->DeviceRefreshBindings,
			.DeviceAuthorizationRefreshResumes
			= this->DeviceRefreshResumes,
			.PublicPreviewComposed = this->PublicPreviewComposed,
			.PublicPreviewUnavailable
			= this->public_preview_unavailable(),
			.PublicPreviewPolicyRefused
			= this->public_preview_policy_refused(),
			.PublicPreviewSignedOut = this->public_preview_signed_out(),
			.PublicPreviewPublications
			= this->PublicPreviewProjections.published(),
			.PublicPreviewRequests = this->public_preview_requests(),
			.PublicPreviewHeaderEffects
			= this->public_preview_header_effects(),
			.PublicPreviewVaultEffects
			= this->public_preview_vault_effects(),
			.PublicPreviewSmartAcquisitions
			= this->AnonymousSmartGit == nullptr
				? 0U
				: this->AnonymousSmartGit->acquisitions(),
			.PublicPreviewSignInRequests
			= this->PublicPreviewSignInRequests,
			.PublicPreviewReviewCommentsObserved
			= this->PublicPreviewProjections.last().ReviewComments
				== application::public_preview_observation::observed,
			.PublicPreviewReviewCommentGroups
			= this->PublicPreviewProjections.last()
				  .ReviewCommentGroups.size(),
			.PublicPreviewWriteRefusals
			= this->PublicPreviewWriteRefusals,
			.PublicPreviewLiveCandidateProductionFactories
			= this->public_preview_production_factories(),
			.PublicPreviewWireRequests
			= this->public_preview_wire_requests(),
			.PublicPreviewWireCapabilityProbes
			= this->public_preview_wire_capability_probes(),
			.PublicPreviewWireCredentialedRefusals
			= this->public_preview_wire_credentialed_refusals(),
			.PublicPreviewBudgetObserved
			= this->public_preview_budget().has_value(),
			.PublicPreviewBudgetLimit
			= this->public_preview_budget().has_value()
				? this->public_preview_budget()->Limit
				: 0U,
			.PublicPreviewBudgetRemaining
			= this->public_preview_budget().has_value()
				? this->public_preview_budget()->Remaining
				: 0U,
			.PublicPreviewBudgetResetUnixSeconds
			= this->public_preview_budget().has_value()
				? this->public_preview_budget()->ResetUnixSeconds
				: 0U,
			.PublicPreviewSnapshotPublicPreview
			= this->public_preview_snapshot_signed_out(),
			.PublicPreviewSnapshotHeadOid
			= this->public_preview_snapshot_head_oid(),
		};
	}

	auto github_application::status() const noexcept -> github_application_status
	{
		if (Implementation) {
			auto result = Implementation->status();
			if (Profile->Closing) {
				result.ShutdownComplete      = profile_shutdown_complete();
				result.ShutdownCleanupFailed = Profile->CleanupFailed;
			}
			return result;
		}
		return { .Closing = true, .ShutdownComplete = true };
	}

	auto github_application::status(core::bpr_window_context aContext) const noexcept -> std::optional<github_application_status>
	{
		const auto found = Sessions.find(aContext);
		if (found == Sessions.end()) {
			return std::nullopt;
		}
		return found->second->status();
	}

	void github_application::select_device_authorization()
	{
		Implementation->select_device_authorization();
	}

	void github_application::cancel_device_authorization()
	{
		Implementation->cancel_device_authorization();
	}

	void github_application::open_device_verification_address()
	{
		Implementation->open_device_verification_address();
	}

	void github_application::activate_public_preview(const bool aEnterprise)
	{
		Implementation->activate_public_preview(aEnterprise);
	}

	void github_application::attempt_public_preview_writes()
	{
		Implementation->attempt_public_preview_writes();
	}

	auto github_application::public_preview_wire_operations() const
		-> std::vector<public_preview_wire_operation_record>
	{
		return Implementation->public_preview_wire_operations();
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::start_desktop(detail::github_public_path_fixture aFixture, detail::github_public_path_fixture_options aOptions, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>
	{
		const auto profile  = aOptions.Profile;
		const auto intent   = aOptions.ProfileLaunchIntent;
		auto       policy   = std::move(aOptions.EmptyDesktopPolicy);
		aOptions.DifCatalog = std::make_shared<qt::dif_catalog_provider>();
		// The shipped desktop's status item: a course on the cocoa platform gets
		// the menu bar's Dif exactly as the application does, and an offscreen
		// course, which has no tray, gets none.
		aOptions.StatusItem = native_menu_bar_status_item();
		auto statusItem     = aOptions.StatusItem;
		auto initialReset   = std::make_shared<std::optional<core::profile_data_reset_request>>(std::exchange(aOptions.DataReset, std::nullopt));
		return profile_desktop::start_with_factory(profile, [aFixture, options = std::move(aOptions), initialReset](core::bpr_profile_id aProfile, const std::filesystem::path& aRoot, core::profile_launch_intent aIntent, std::optional<core::profile_data_reset_request> aReset, github_application_startup_completion aDone) {
            auto next = aProfile == options.Profile || options.DistinctProfileAccounts ? options : detail::github_public_path_fixture_options{};
            next.DataReset = aReset ? aReset : aProfile == options.Profile ? std::exchange(*initialReset, std::nullopt) : std::nullopt;
			next.DifCatalog = options.DifCatalog;
			next.StatusItem = options.StatusItem;
			if (aProfile != options.Profile && options.DistinctProfileAccounts) {
				next.BeforeFilesPage = {};
				next.SharedDeviceVault.reset();
			}
            if (!aRoot.empty()) { next.ApplicationDataRoot = aRoot; next.GitExecutable = options.GitExecutable; }
            next.Profile = aProfile;
            next.ProfileLaunchIntent = aIntent;
            return detail::github_application_fixture_access::start_profile(aFixture, std::move(next), std::move(aDone)); }, std::move(aCompletion), intent, std::move(policy), false, std::move(statusItem));
	}
	auto detail::github_application_fixture_access::start_desktop_public_preview_selection_available(github_application_config aConfig, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>
	{
		const auto profile = aConfig.Profile;
		const auto intent  = aConfig.ProfileLaunchIntent;
		return profile_desktop::start_with_factory(profile, [config = std::move(aConfig)](core::bpr_profile_id aProfile, const std::filesystem::path& aRoot, core::profile_launch_intent aIntent, std::optional<core::profile_data_reset_request> aReset, github_application_startup_completion aDone) {
            auto next = profile_desktop::configuration_for_profile(config, aProfile, aRoot);
            next.ProfileLaunchIntent = aIntent;
            next.DataReset = aReset;
            return detail::github_application_fixture_access::start_profile_public_preview_selection_available(std::move(next), std::move(aDone)); }, std::move(aCompletion), intent);
	}

	auto detail::github_application_fixture_access::start(detail::github_public_path_fixture aFixture, detail::github_public_path_fixture_options aOptions, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state  = std::make_shared<github_application_startup::implementation>(github_application_config{ }, std::move(aCompletion), aFixture, std::move(aOptions));
		auto handle = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->begin();
		return handle;
	}
	auto detail::github_application_fixture_access::start_profile(detail::github_public_path_fixture aFixture, detail::github_public_path_fixture_options aOptions, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state       = std::make_shared<github_application_startup::implementation>(github_application_config{ }, std::move(aCompletion), aFixture, std::move(aOptions));
		auto handle      = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->OwnerOnly = true;
		state->begin();
		return handle;
	}
	auto detail::github_application_fixture_access::start_public_preview_selection_available(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state  = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion), std::nullopt, detail::github_public_path_fixture_options{ .PublicPreviewSelectionAvailable = true });
		auto handle = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->begin();
		return handle;
	}
	auto detail::github_application_fixture_access::start_profile_public_preview_selection_available(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state       = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion), std::nullopt, detail::github_public_path_fixture_options{ .PublicPreviewSelectionAvailable = true });
		auto handle      = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->OwnerOnly = true;
		state->begin();
		return handle;
	}

	namespace {
	#if defined(__APPLE__)
	#elif defined(_WIN32)
		inline constexpr int uiforetw_capture_open_height_floor   = 850;
		inline constexpr int uiforetw_capture_folded_height_floor = 884;
	#else
		inline constexpr int uiforetw_capture_open_height_floor   = 842;
		inline constexpr int uiforetw_capture_folded_height_floor = 876;
	#endif

	} // namespace

#endif
}
