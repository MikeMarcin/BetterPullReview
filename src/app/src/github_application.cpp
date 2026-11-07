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
	auto detail::github_application_fixture_access::network_isolation_probe()
		-> bool
	{
		using namespace adapters::github::detail;
		const auto profile = validate_github_host_profile(native_profile_candidate(
			core::host_profile_id{ 901U },
			core::git_host_id{ 902U },
			application::github_profile_revision{ 903U },
			application::github_server_contract::ghes_3_18_2022_11_28,
			application::github_authentication_transport::native_https,
			"example.com",
			github_profile_provenance::managed));
		if (!profile.has_value()) {
			return false;
		}
		const auto wire = compile_read_request({
			.Server    = server_contract::ghes_3_18,
			.Operation = read_operation::get_viewer_identity,
			.Page      = 1U,
		});
		if (!wire.has_value()) {
			return false;
		}
		const auto context = core::github_session_request_context{
			.Request            = core::request_id{ 904U },
			.ApplicationSession = core::application_session_id{ 905U },
			.Profile            = profile->profile(),
			.IdentityGeneration = core::github_identity_generation{ 906U },
		};
		constexpr auto credentialText = std::string_view{
			"bpr-network-isolation-probe-not-a-credential"
		};
		auto credential = application::github_credential_material::create(
			std::as_bytes(std::span{ credentialText }));
		if (!credential.has_value()) {
			return false;
		}
		auto transport = make_native_https_bootstrap_transport(
			*profile, context, std::move(*credential));
		if (!transport.has_value()) {
			return false;
		}
		const auto        completion = (*transport)->perform(github_wire_request{
																 .Correlation = {
																	 .Parent          = context,
																	 .ProfileRevision = profile->revision(),
																	 .Host            = profile->host(),
																	 .Operation       = read_operation::get_viewer_identity,
																	 .Page            = 1U,
																 },
																 .Wire = *wire,
															 },
															 std::stop_token{ });
		const auto* const failure    = std::get_if<github_wire_error>(&completion);
		return failure != nullptr
			&& failure->Error.Failure
			== application::github_provider_failure::network;
	}

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

	void detail::github_application_fixture_access::hold_review_submission_terminals(github_application& aApplication, bool aHold)
	{
		auto& gate = aApplication.Implementation->ReviewSubmissionDecisionGate;
		if (gate) {
			if (aHold) {
				gate->hold();
			} else {
				gate->release();
			}
		}
	}
	auto detail::github_application_fixture_access::held_review_submission_terminals(const github_application& aApplication) -> std::size_t
	{
		const auto& gate = aApplication.Implementation->ReviewSubmissionDecisionGate;
		return gate ? gate->pending() : 0U;
	}
	void detail::github_application_fixture_access::hold_review_mutation_terminals(github_application& aApplication, bool aHold)
	{
		auto& gate = aApplication.Implementation->ReviewMutationDecisionGate;
		if (gate) {
			if (aHold) {
				gate->hold();
			} else {
				gate->release();
			}
		}
	}
	auto detail::github_application_fixture_access::held_review_mutation_terminals(const github_application& aApplication) -> std::size_t
	{
		const auto& gate = aApplication.Implementation->ReviewMutationDecisionGate;
		return gate ? gate->pending() : 0U;
	}
	void detail::github_application_fixture_access::hold_syntax_blob_reads(github_application& aApplication, const bool aHold)
	{
		if (auto& blobs = aApplication.Implementation->SyntaxBlobs; blobs) {
			blobs->hold_reads(aHold);
		}
	}
	void detail::github_application_fixture_access::activity_arrived(github_application& aApplication, const std::vector<core::pull_request_ref>& aActive)
	{
		aApplication.Implementation->hand_on_activity(aActive);
	}
	auto detail::github_application_fixture_access::reread_owed(const github_application& aApplication) noexcept -> bool
	{
		const auto& implementation = *aApplication.Implementation;
		return implementation.RereadOwed || implementation.RereadQueued || implementation.RereadRetrying;
	}
	auto detail::github_application_fixture_access::held_syntax_blob_reads(const github_application& aApplication) -> std::size_t
	{
		const auto& blobs = aApplication.Implementation->SyntaxBlobs;
		return blobs ? blobs->held_reads() : 0U;
	}

	auto detail::github_application_fixture_access::device_binding_store_idle(const github_application& aApplication) noexcept -> bool
	{
		return aApplication.Implementation->DevicePersistence && aApplication.Implementation->DevicePersistence->is_quiescent();
	}

	auto detail::github_application_fixture_access::device_binding_durable(const github_application& aApplication) noexcept -> bool
	{
		return aApplication.Implementation->Host->accepted_device_binding_durable();
	}

	auto detail::github_application_fixture_access::browser_last_address(const github_application& aApplication) -> std::string
	{
		return aApplication.Implementation->RecordingBrowser.last_address();
	}

	auto detail::github_application_fixture_access::browser_opens(const github_application& aApplication) noexcept -> std::size_t
	{
		return aApplication.Implementation->RecordingBrowser.opens();
	}

	auto detail::github_application_fixture_access::large_review_snapshot(
		const github_application& aApplication) -> std::optional<application::github_pull_request_snapshot>
	{
		return aApplication.Implementation->WindowRead->accepted_snapshot();
	}

	auto detail::github_application_fixture_access::large_review_snapshot(const github_application& aApplication, core::bpr_window_context aContext) -> std::optional<application::github_pull_request_snapshot>
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found == aApplication.Sessions.end() || found->second->Closing || !found->second->WindowRead) {
			return std::nullopt;
		}
		return found->second->WindowRead->accepted_snapshot();
	}
	void detail::github_application_fixture_access::hold_window_read_terminal(github_application& aApplication, core::bpr_window_context aContext, bool aHold)
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found != aApplication.Sessions.end() && !found->second->Closing && found->second->ReadDecision) {
			found->second->ReadDecision->hold(aHold);
		}
	}
	void detail::github_application_fixture_access::stage_server_conversation(github_application& aApplication, std::string aThread, const bool aAddedAndResolved)
	{
		if (aApplication.Implementation->Fixture) {
			aApplication.Implementation->Fixture->stage_server_conversation(std::move(aThread), aAddedAndResolved);
		}
	}
	void detail::github_application_fixture_access::stage_server_root(github_application& aApplication, std::string aThread, std::optional<std::string> aRootBody,
																	  std::optional<bool> aResolved, std::optional<std::pair<std::string, std::string>> aRootAuthor)
	{
		if (aApplication.Implementation->Fixture) {
			aApplication.Implementation->Fixture->stage_server_root(std::move(aThread), std::move(aRootBody), aResolved, std::move(aRootAuthor));
		}
	}
	void detail::github_application_fixture_access::set_cycle_publication_account(github_application& aApplication, const core::bpr_window_context aContext,
																				  std::function<std::optional<application::github_publication_account>()> aAccount)
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found != aApplication.Sessions.end() && found->second->CycleDesktop) {
			found->second->CycleDesktop->set_publication_account(std::move(aAccount));
		}
	}
	auto detail::github_application_fixture_access::held_window_read_terminals(const github_application& aApplication, core::bpr_window_context aContext) noexcept -> std::size_t
	{
		const auto found = aApplication.Sessions.find(aContext);
		return found == aApplication.Sessions.end() || !found->second->ReadDecision ? 0U : found->second->ReadDecision->held();
	}

	auto detail::github_application_fixture_access::tab_comparison_evidence(
		const github_application& aApplication) -> tab_comparison_fixture_evidence
	{
		const auto& implementation = *aApplication.Implementation;
		const auto& tabs           = implementation.TabHost->state();
		const auto* active         = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		return {
			.Accepted  = implementation.EpochHost->state().AcceptedComparison,
			.Window    = tabs.WindowComparison,
			.ActiveTab = active ? std::optional{ active->View.Comparison } : std::nullopt,
		};
	}

	auto detail::github_application_fixture_access::scaling_evidence(const github_application& aApplication, const std::optional<core::bpr_window_context> aContext) noexcept -> application_scaling_evidence
	{
		auto* selected = aApplication.Implementation.get();
		if (aContext) {
			const auto found = aApplication.Sessions.find(*aContext);
			if (found == aApplication.Sessions.end()) {
				return { };
			}
			selected = found->second.get();
		}
		const auto& owner = *selected;
		return {
			.Workbench                    = owner.WorkbenchHost ? owner.WorkbenchHost->work_statistics() : application::workbench_work_statistics{ },
			.Ledger                       = owner.ReviewLedgerHost ? owner.ReviewLedgerHost->statistics() : application::review_ledger_host_statistics{ },
			.Queue                        = owner.QueueHost ? owner.QueueHost->statistics() : application::review_queue_host_statistics{ },
			.OverviewBuilds               = owner.OverviewBuilds,
			.OverviewReceiptVisits        = owner.OverviewReceiptVisits,
			.OverviewHistoryUpdates       = owner.OverviewHistoryUpdates,
			.ConversationThreadsVisited   = owner.ConversationThreadsVisited,
			.ConversationPostsVisited     = owner.ConversationPostsVisited,
			.OverviewReceiptUpdates       = owner.OverviewReceiptUpdates,
			.OverviewReceiptRows          = owner.OverviewReceiptRows,
			.FeedbackWork                 = owner.FeedbackCounters.work(),
			.OverviewFeedbackPublications = owner.OverviewFeedbackPublications,
			.FeedbackObservations         = owner.FeedbackObservations,
			.FeedbackBodiesParsed         = owner.FeedbackBodiesParsed,
			.QueueQuiescent               = !owner.QueueHost || owner.QueueHost->is_quiescent(),
		};
	}

	void detail::github_application_fixture_access::cancel_base_numbering_answers(github_application& aApplication, const std::size_t aCount)
	{
		aApplication.Implementation->ForcedBaseNumberingCancellations = aCount;
	}

	void detail::github_application_fixture_access::stale_base_numbering_question(github_application& aApplication)
	{
		aApplication.Implementation->StaleBaseNumberingQuestion = true;
	}

	void detail::github_application_fixture_access::set_whole_diff_placement_enabled(github_application& aApplication, const bool aEnabled)
	{
		aApplication.Implementation->WholeDiffPlacementDisabled = !aEnabled;
	}

	void detail::github_application_fixture_access::set_preparation_enabled(github_application& aApplication, const bool aEnabled)
	{
		auto& implementation               = *aApplication.Implementation;
		implementation.PreparationDisabled = !aEnabled;
		if (aEnabled) {
			implementation.schedule_preparation();
		} else {
			implementation.cancel_preparation();
			if (implementation.DiffHost != nullptr) {
				implementation.DiffHost->prepare({ });
			}
		}
	}

	auto detail::github_application_fixture_access::diff_work_evidence(
		const github_application& aApplication) noexcept -> diff_work_fixture_evidence
	{
		const auto& implementation = *aApplication.Implementation;
		const auto& host           = *implementation.DiffHost;
		return {
			.OutstandingJobs                     = host.outstanding_jobs(),
			.WorkbenchOutstandingRequests        = implementation.WorkbenchHost->outstanding_requests(),
			.EpochOutstandingJobs                = implementation.EpochHost->outstanding_jobs(),
			.EpochReviewerJobs                   = implementation.EpochHost->outstanding_reviewer_jobs(),
			.EpochPreparationJobs                = implementation.EpochHost->outstanding_preparation_jobs(),
			.PreparationPending                  = implementation.PreparationScheduled || implementation.PreparationDeferred,
			.EpochMaintenancePending             = implementation.EpochHost->state().PendingMaintenance.has_value(),
			.CacheEntries                        = host.cache_entries(),
			.CacheBytes                          = host.cache_bytes(),
			.FixtureDiffHelperInvocations        = implementation.FixtureDiffHelperInvocations,
			.Pending                             = host.state().Pending.has_value(),
			.NavigationReady                     = implementation.WorkbenchHost->state().Navigation.has_value(),
			.TabPresentationPending              = implementation.TabAdvanceQueued || implementation.TabLoad.has_value() || implementation.RefreshKeptTabView,
			.Quiescent                           = host.is_quiescent(),
			.PreparedEntries                     = host.prepared_entries(),
			.PreparedBytes                       = host.prepared_bytes(),
			.Preparing                           = host.preparing(),
			.PreparationsInFlight                = implementation.PreparationsInFlight.load(std::memory_order_acquire),
			.PreparationRounds                   = implementation.PreparationRounds,
			.PreparedResolutions                 = implementation.PreparedResolutions,
			.PreparedRemoteBlobs                 = implementation.PreparedRemoteBlobs,
			.PreparedRemoteBatches               = implementation.PreparedRemoteBatches,
			.PreparedHits                        = host.prepared_hits(),
			.PreparationsPreempted               = implementation.PreparationsPreempted,
			.PreparationsStartedBeforeOpenedFile = implementation.PreparationsStartedBeforeOpenedFile,
		};
	}
	auto detail::github_application_fixture_access::diff_blob_reads(const github_application& aApplication, const core::git_blob_oid& aBlob) -> std::uint64_t
	{
		const auto& implementation = *aApplication.Implementation;
		return implementation.DiffBlobReads == nullptr ? 0U : implementation.DiffBlobReads->reads(aBlob);
	}

	auto detail::github_application_fixture_access::diff_work_evidence(
		const github_application& aApplication, core::bpr_window_context aContext) noexcept -> diff_work_fixture_evidence
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found == aApplication.Sessions.end()) {
			return { };
		}
		const auto& implementation = *found->second;
		const auto& host           = *implementation.DiffHost;
		return {
			.OutstandingJobs                     = host.outstanding_jobs(),
			.WorkbenchOutstandingRequests        = implementation.WorkbenchHost->outstanding_requests(),
			.EpochOutstandingJobs                = implementation.EpochHost->outstanding_jobs(),
			.EpochReviewerJobs                   = implementation.EpochHost->outstanding_reviewer_jobs(),
			.EpochPreparationJobs                = implementation.EpochHost->outstanding_preparation_jobs(),
			.PreparationPending                  = implementation.PreparationScheduled || implementation.PreparationDeferred,
			.EpochMaintenancePending             = implementation.EpochHost->state().PendingMaintenance.has_value(),
			.CacheEntries                        = host.cache_entries(),
			.CacheBytes                          = host.cache_bytes(),
			.FixtureDiffHelperInvocations        = implementation.FixtureDiffHelperInvocations,
			.Pending                             = host.state().Pending.has_value(),
			.NavigationReady                     = implementation.WorkbenchHost->state().Navigation.has_value(),
			.TabPresentationPending              = implementation.TabAdvanceQueued || implementation.TabLoad.has_value() || implementation.RefreshKeptTabView,
			.Quiescent                           = host.is_quiescent(),
			.PreparedEntries                     = host.prepared_entries(),
			.PreparedBytes                       = host.prepared_bytes(),
			.Preparing                           = host.preparing(),
			.PreparationsInFlight                = implementation.PreparationsInFlight.load(std::memory_order_acquire),
			.PreparationRounds                   = implementation.PreparationRounds,
			.PreparedResolutions                 = implementation.PreparedResolutions,
			.PreparedRemoteBlobs                 = implementation.PreparedRemoteBlobs,
			.PreparedRemoteBatches               = implementation.PreparedRemoteBatches,
			.PreparedHits                        = host.prepared_hits(),
			.PreparationsPreempted               = implementation.PreparationsPreempted,
			.PreparationsStartedBeforeOpenedFile = implementation.PreparationsStartedBeforeOpenedFile,
		};
	}

	namespace {
		/// \brief [[BPR-0863]]. How long a course's modality may take to settle.
		///
		/// \details The step is advanced by the automated run about every ten
		/// milliseconds and by the large-review benchmark about every
		/// millisecond, so its bound is time rather than a count of calls. Five
		/// seconds is far longer than a modality change and the drawer store
		/// behind it take on a loaded machine, and far short of any course's run
		/// budget. The floor of calls keeps one long tick from reading as a stall.
		inline constexpr auto        course_modality_wait       = std::chrono::milliseconds{ 5'000 };
		inline constexpr std::size_t course_modality_wait_floor = 16U;

		[[nodiscard]] auto course_core_modality(const qt::review_modality aModality) noexcept
			-> core::workbench_modality
		{
			switch (aModality) {
			case qt::review_modality::overview:
				return core::workbench_modality::overview;
			case qt::review_modality::review:
				return core::workbench_modality::review;
			case qt::review_modality::discuss:
				return core::workbench_modality::discuss;
			case qt::review_modality::threads:
				return core::workbench_modality::threads;
			}
			return core::workbench_modality::review;
		}

		[[nodiscard]] auto course_core_modality_name(const core::workbench_modality aModality)
			-> QString
		{
			switch (aModality) {
			case core::workbench_modality::overview:
				return QStringLiteral("Overview");
			case core::workbench_modality::review:
				return QStringLiteral("Review");
			case core::workbench_modality::discuss:
				return QStringLiteral("Discuss");
			case core::workbench_modality::threads:
				return QStringLiteral("Threads");
			}
			return QStringLiteral("unknown");
		}

		[[nodiscard]] auto course_drawer_name(const core::workbench_drawer_state aState) -> QString
		{
			return aState == core::workbench_drawer_state::expanded
				? QStringLiteral("expanded")
				: QStringLiteral("collapsed");
		}

		/// \brief [[BPR-0863]]. What the window shows a reviewer right now.
		///
		/// \details Read from the widgets, not from a table of what each
		/// modality should show, so the answer follows the product and cannot
		/// drift from it. A drawer is offered when it is shown and expanded.
		[[nodiscard]] auto course_offered_surfaces(const qt::workbench_window& aWindow)
			-> detail::course_surfaces
		{
			const auto offered = [&aWindow](const QWidget* const aSurface) {
				return !aWindow.showing_home() && aSurface != nullptr
					&& aSurface->isVisibleTo(&aWindow);
			};
			const auto expanded = [&offered](const qt::workbench_drawer_panel* const aPanel) {
				return offered(aPanel)
					&& aPanel->state() == core::workbench_drawer_state::expanded;
			};
			return {
				.Comparison          = offered(aWindow.comparison_surface()),
				.FilesDrawer         = expanded(aWindow.files_drawer()),
				.ConversationsDrawer = expanded(aWindow.conversations_drawer()),
				.RepositoriesDrawer  = expanded(aWindow.repositories_drawer()),
			};
		}

		/// \brief Names the surfaces set in `aSurfaces`, or `none`.
		[[nodiscard]] auto course_surface_names(const detail::course_surfaces& aSurfaces)
			-> QString
		{
			auto names = QStringList{ };
			if (aSurfaces.Comparison) {
				names.append(QStringLiteral("comparison"));
			}
			if (aSurfaces.FilesDrawer) {
				names.append(QStringLiteral("files_drawer"));
			}
			if (aSurfaces.ConversationsDrawer) {
				names.append(QStringLiteral("conversations_drawer"));
			}
			if (aSurfaces.RepositoriesDrawer) {
				names.append(QStringLiteral("repositories_drawer"));
			}
			return names.isEmpty() ? QStringLiteral("none") : names.join(QLatin1Char{ ',' });
		}

		/// \brief The surfaces in `aWanted` that `aOffered` does not show.
		[[nodiscard]] auto course_surfaces_missing(
			const detail::course_surfaces& aWanted,
			const detail::course_surfaces& aOffered) noexcept
			-> detail::course_surfaces
		{
			return {
				.Comparison          = aWanted.Comparison && !aOffered.Comparison,
				.FilesDrawer         = aWanted.FilesDrawer && !aOffered.FilesDrawer,
				.ConversationsDrawer = aWanted.ConversationsDrawer && !aOffered.ConversationsDrawer,
				.RepositoriesDrawer  = aWanted.RepositoriesDrawer && !aOffered.RepositoriesDrawer,
			};
		}

		[[nodiscard]] auto course_surfaces_any(const detail::course_surfaces& aSurfaces) noexcept
			-> bool
		{
			return aSurfaces.Comparison || aSurfaces.FilesDrawer
				|| aSurfaces.ConversationsDrawer || aSurfaces.RepositoriesDrawer;
		}
	} // namespace

	auto detail::github_application_fixture_access::course_modality_facts(
		const github_application& aApplication) -> QString
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr) {
			return QStringLiteral("modality=unavailable");
		}
		const auto& window  = *implementation->Window;
		const auto& state   = implementation->WorkbenchHost->state();
		const auto  offered = course_offered_surfaces(window);
		const auto  drawer  = [](const qt::workbench_drawer_panel* const aPanel) {
			return aPanel == nullptr ? QStringLiteral("absent") : course_drawer_name(aPanel->state());
		};
		const auto centre = window.showing_home() ? QStringLiteral("home")
			: window.showing_overview()           ? QStringLiteral("overview")
			: offered.Comparison                  ? QStringLiteral("comparison")
			: window.current_modality() == qt::review_modality::discuss
			? QStringLiteral("discussion")
			: window.current_modality() == qt::review_modality::threads
			? QStringLiteral("threads")
			: QStringLiteral("other");
		auto       sizes  = QStringList{ };
		if (window.central_splitter() != nullptr) {
			for (const auto size : window.central_splitter()->sizes()) {
				sizes.append(QString::number(size));
			}
		}
		const auto withheld = course_surfaces_missing(
			detail::course_surfaces{
				.Comparison          = true,
				.FilesDrawer         = true,
				.ConversationsDrawer = true,
				.RepositoriesDrawer  = true,
			},
			offered);
		return QStringList{
			QStringLiteral("modality=%1").arg(window.modality() != nullptr ? qt::review_modality_title(window.modality()->current()) : QStringLiteral("none")),
			QStringLiteral("host_modality=%1").arg(course_core_modality_name(state.Modality)),
			QStringLiteral("centre=%1").arg(centre),
			QStringLiteral("offered=%1").arg(course_surface_names(offered)),
			QStringLiteral("withheld=%1").arg(course_surface_names(withheld)),
			QStringLiteral("files_drawer=%1").arg(drawer(window.files_drawer())),
			QStringLiteral("host_files_drawer=%1").arg(course_drawer_name(state.FilesDrawer)),
			QStringLiteral("conversations_drawer=%1").arg(drawer(window.conversations_drawer())),
			QStringLiteral("host_conversations_drawer=%1").arg(course_drawer_name(state.ConversationsDrawer)),
			QStringLiteral("repositories_drawer=%1").arg(drawer(window.repositories_drawer())),
			QStringLiteral("host_repositories_drawer=%1").arg(course_drawer_name(state.RepositoriesDrawer)),
			QStringLiteral("drawer_controls=%1").arg(window.behaviour().DrawerControls == application::review_drawer_controls::shown ? QStringLiteral("shown") : QStringLiteral("hidden")),
			QStringLiteral("outstanding_requests=%1").arg(static_cast<qulonglong>(implementation->WorkbenchHost->outstanding_requests())),
			QStringLiteral("splitter_sizes=%1").arg(sizes.isEmpty() ? QStringLiteral("none") : sizes.join(QLatin1Char{ ',' })),
		}
			.join(QLatin1Char{ ' ' });
	}

	auto detail::github_application_fixture_access::enter_course_modality(
		github_application&   aApplication,
		course_modality_step& aStep) -> course_modality_result
	{
		if (aStep.Result != course_modality_result::pending) {
			return aStep.Result;
		}
		auto* const implementation = aApplication.Implementation.get();
		const auto  now            = std::chrono::steady_clock::now();
		const auto  report         = [&](const char* const aTerm, const QString& aDetail) {
			auto stream = QTextStream{ stdout };
			stream << aTerm << ":wanted=" << qt::review_modality_title(aStep.Modality)
				   << ' ' << aDetail << ' ' << course_modality_facts(aApplication) << '\n';
			stream.flush();
			aStep.Result = course_modality_result::failed;
			return aStep.Result;
		};
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->Window->modality() == nullptr) {
			return report("COURSE_MODALITY_STALLED", QStringLiteral("strip=absent"));
		}
		auto& window = *implementation->Window;
		// The reviewer's gesture: a press of the strip's segment. Triggered even
		// when the window already shows the modality, so the course starts from
		// the modality's own drawers rather than whatever an earlier gesture
		// left open.
		if (!aStep.ChosenAt.has_value()) {
			window.modality()->action(aStep.Modality)->trigger();
			aStep.ChosenAt = now;
			return course_modality_result::pending;
		}
		++aStep.Waits;
		const auto& host   = *implementation->WorkbenchHost;
		const auto& state  = host.state();
		const auto  agrees = [](const qt::workbench_drawer_panel* const aPanel,
								const core::workbench_drawer_state      aHost) {
			return aPanel != nullptr && aPanel->state() == aHost;
		};
		const auto settled = !window.showing_home()
			&& window.current_modality() == aStep.Modality
			&& state.Modality == course_core_modality(aStep.Modality)
			&& agrees(window.files_drawer(), state.FilesDrawer)
			&& agrees(window.conversations_drawer(), state.ConversationsDrawer)
			&& agrees(window.repositories_drawer(), state.RepositoriesDrawer)
			&& host.outstanding_requests() == 0U;
		// Once settled, the modality is the one the course stated and what it
		// withholds is the product's decision. A needed surface may still be a
		// layout pass behind the drawers, so it is waited for like the rest; at
		// the bound, a settled modality withholding it is named as such.
		const auto missing = settled
			? course_surfaces_missing(aStep.Needs, course_offered_surfaces(window))
			: detail::course_surfaces{ };
		if (settled && !course_surfaces_any(missing)) {
			aStep.Result = course_modality_result::entered;
			return aStep.Result;
		}
		const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(now - *aStep.ChosenAt);
		if (waited < course_modality_wait || aStep.Waits < course_modality_wait_floor) {
			return course_modality_result::pending;
		}
		const auto bound = QStringLiteral("waited_ms=%1 wait_cap_ms=%2 waits=%3")
							   .arg(static_cast<qlonglong>(waited.count()))
							   .arg(static_cast<qlonglong>(course_modality_wait.count()))
							   .arg(static_cast<qulonglong>(aStep.Waits));
		if (settled) {
			return report(
				"COURSE_MODALITY_WITHHOLDS",
				QStringLiteral("needs=%1 missing=%2 %3")
					.arg(course_surface_names(aStep.Needs), course_surface_names(missing), bound));
		}
		return report("COURSE_MODALITY_STALLED", bound);
	}

	auto detail::github_application_fixture_access::verify_pending_first_file_choice_invalidation(
		github_application& aApplication) -> bool
	{
		auto* const owner = aApplication.Implementation.get();
		if (!owner || !owner->TabHost || !owner->LastAcceptedHandoff || !owner->WorkbenchReview.exact(*owner->LastAcceptedHandoff)) {
			return false;
		}
		const auto& tabs   = owner->TabHost->state();
		const auto* active = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		if (!active || !active->View.File) {
			return false;
		}
		const auto  file    = *active->View.File;
		const auto& handoff = *owner->LastAcceptedHandoff;
		const auto  exact   = application::pull_request_authority_invalidation{
			.ApplicationSession = handoff.ApplicationSession,
			.Profile            = handoff.Profile,
			.ProfileRevision    = handoff.ProfileRevision,
			.Host               = handoff.Host,
			.HostAuthority      = handoff.HostAuthority,
			.Account            = handoff.Account,
			.Viewer             = handoff.Viewer,
			.Repository         = handoff.Repository,
			.PullRequest        = handoff.PullRequest,
			.IdentityGeneration = handoff.IdentityGeneration,
			.Mode               = handoff.Mode,
		};
		// Arrange the core state already observed between the real first-file
		// gesture and its comparison: an explicit choice, no tab or pair yet.
		owner->TabHost->reset();
		owner->TabHost->observe_file_choice(file);
		auto foreign        = exact;
		foreign.PullRequest = core::pull_request_id{ exact.PullRequest.value() == 1U ? 2U : 1U };
		owner->invalidate(foreign);
		if (tabs.Active || tabs.Chosen != std::optional{ file }) {
			return false;
		}
		owner->invalidate(exact);
		return !tabs.Active && !tabs.Pending && !tabs.Chosen;
	}

	auto detail::github_application_fixture_access::epoch_refresh_identity(
		const github_application& aApplication, const std::optional<core::bpr_window_context> aContext) -> epoch_refresh_identity_evidence
	{
		auto        result         = epoch_refresh_identity_evidence{ };
		const auto* implementation = aApplication.Implementation.get();
		if (aContext) {
			const auto found = aApplication.Sessions.find(*aContext);
			implementation   = found == aApplication.Sessions.end() ? nullptr : found->second.get();
		}
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->EpochHost == nullptr || implementation->TabHost == nullptr) {
			return result;
		}
		result.SelectedFile       = implementation->WorkbenchHost->selected();
		result.TabModality        = github_application::implementation::tab_modality(implementation->WorkbenchHost->state().Modality);
		const auto& epoch         = implementation->EpochHost->state();
		result.AcceptedComparison = epoch.AcceptedComparison;
		result.ComparisonPending  = epoch.PendingComparison.has_value();
		const auto& tabs          = implementation->TabHost->state();
		if (const auto* active = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr) {
			result.ActiveTab = *active;
		}
		result.TabPending = tabs.Pending.has_value();
		result.TabLoad    = implementation->TabLoad.has_value();
		if (implementation->PaneFind) {
			const auto counters    = implementation->PaneFind->counters();
			result.SearchCounters  = { counters.Admitted, counters.Scheduled, counters.Terminals, counters.StaleTerminals, counters.ProjectionFailures, counters.IngressRefusals };
			result.SearchQuiescent = implementation->PaneFind->is_quiescent();
		}
		if (const auto& publication = implementation->SelectedDiffPublication; publication) {
			result.DiffRequestMatches   = implementation->SelectedDiffRequest == std::optional{ publication->Request };
			result.PublishedFile        = publication->Request.File.File;
			result.PublishedComparison  = publication->Request.Diff.Comparison;
			const auto* surface         = implementation->Window->comparison_surface();
			result.PublicationDisplayed = surface->current_result() && surface->current_context()
				&& surface->current_context()->Key == publication->Key;
		}
		return result;
	}

	auto detail::github_application_fixture_access::close_review(
		const github_application& aApplication) -> close_review_evidence
	{
		auto        result         = close_review_evidence{ };
		const auto* implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->WindowRead) {
			return result;
		}
		const auto& state           = implementation->WindowRead->state();
		result.ReadHoldsPullRequest = state.ActiveLocator.has_value() || state.PendingPullRequest.has_value()
			|| state.PullRequest.has_value() || state.AcceptedSnapshot.has_value();
		result.SnapshotHeld       = implementation->WindowRead->accepted_snapshot().has_value();
		result.ReadingDestination = implementation->ReadingDestination.has_value();
		result.TabDestination     = implementation->TabDestination.has_value();
		result.LedgerComposed     = implementation->SharedDrafts != nullptr;
		if (implementation->TabHost != nullptr) {
			result.Tabs      = implementation->TabHost->state().Tabs.size();
			result.TabChosen = implementation->TabHost->state().Chosen.has_value();
		}
		for (const auto& draft : implementation->saved_draft_values()) {
			result.SavedDraftBodies.push_back(draft.Body);
		}
		return result;
	}

	auto detail::github_application_fixture_access::evidence(
		const github_application& aApplication) noexcept
		-> detail::github_public_path_fixture_evidence
	{
		return aApplication.Implementation->Fixture != nullptr
			? aApplication.Implementation->Fixture->evidence()
			: detail::github_public_path_fixture_evidence{ };
	}

	auto detail::github_application_fixture_access::review_mutation_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_mutation_fixture_evidence
	{
		return aApplication.Implementation->Fixture != nullptr
			? aApplication.Implementation->Fixture->review_mutation_evidence()
			: detail::review_mutation_fixture_evidence{ };
	}

	auto detail::github_application_fixture_access::thread_resolution_evidence(
		const github_application& aApplication) noexcept
		-> detail::thread_resolution_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		auto              evidence       = detail::thread_resolution_fixture_evidence{ };
		if (implementation == nullptr) {
			return evidence;
		}
		evidence.Gestures    = implementation->ThreadResolutionGestures;
		evidence.Refusals    = implementation->ThreadResolutionRefusals;
		evidence.Completions = implementation->ThreadResolutionCompletions;
		evidence.Failures    = implementation->ThreadResolutionFailures;
		if (implementation->ReviewMutationHost == nullptr) {
			return evidence;
		}
		const auto capabilities    = implementation->ReviewMutationHost->capabilities();
		evidence.HostComposed      = true;
		evidence.BindingOpen       = implementation->InlineReviewBinding && implementation->InlineReviewBinding->Target.Open;
		evidence.ResolvesThreads   = capabilities.ResolvesThreads;
		evidence.Reacts            = capabilities.Reacts;
		evidence.EditsDescription  = capabilities.EditsDescription;
		evidence.EditsComments     = capabilities.EditsComments;
		evidence.RequestsReviewers = capabilities.RequestsReviewers;
		return evidence;
	}

	auto detail::github_application_fixture_access::notification_tray(
		github_application& aApplication) noexcept -> QSystemTrayIcon*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->SystemNotificationSink != nullptr
			? implementation->SystemNotificationSink->tray_icon()
			: nullptr;
	}

	auto detail::github_application_fixture_access::personal_discovery_evidence(
		const github_application& aApplication) noexcept
		-> detail::personal_discovery_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Discovery == nullptr) {
			return { };
		}
		const auto& discovery = *implementation->Discovery;
		return {
			.Composed   = true,
			.Statistics = discovery.statistics(),
			.Status     = discovery.status(),
			.Quiescent  = discovery.is_quiescent(),
		};
	}

	auto detail::github_application_fixture_access::queue_notification_evidence(
		const github_application& aApplication) noexcept
		-> detail::queue_notification_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		const auto statistics    = implementation->Notifications != nullptr
			? implementation->Notifications->statistics()
			: application::notification_delivery_statistics{ };
		auto       lastSubmitted = std::optional<application::delivered_notification>{ };
		if (implementation->FixtureNotificationSink != nullptr) {
			lastSubmitted
				= implementation->FixtureNotificationSink->submitted_payload();
		} else if (implementation->SystemNotificationSink != nullptr) {
			lastSubmitted
				= implementation->SystemNotificationSink->submitted_payload();
		}
		return {
			.RequestsAttempted  = implementation->NotificationRequestsAttempted,
			.Submitted          = statistics.Submitted,
			.DisplaySubmissions = implementation->CapturedNotificationDisplays,
			.SuppressedUnattributableActivation
			= statistics.SuppressedUnattributableActivation,
			.Activations       = statistics.Activations,
			.Selections        = implementation->NotificationSelections,
			.SelectionFailures = implementation->NotificationSelectionFailures,
			.QueueQuiescent    = implementation->QueueHost != nullptr
				&& implementation->QueueHost->is_quiescent(),
			.LastSubmitted = std::move(lastSubmitted),
		};
	}

	auto detail::github_application_fixture_access::activate_fixture_notification(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
			&& implementation->FixtureNotificationSink != nullptr
			&& implementation->FixtureNotificationSink->activate();
	}

	auto detail::github_application_fixture_access::maintenance_evidence(
		const github_application&                       aApplication,
		core::review_epoch_id                           aTarget,
		const std::optional<core::file_comparison_key>& aRetainedComparison) noexcept
		-> detail::github_epoch_maintenance_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->EpochHost == nullptr) {
			return { };
		}

		const auto& state = implementation->EpochHost->state();
		if (aRetainedComparison && (!core::valid(*aRetainedComparison) || !aRetainedComparison->LeftBlob || !core::valid(aTarget) || aTarget.Authority != aRetainedComparison->Authority.Scope || aTarget.Incarnation != aRetainedComparison->LeftAnchor.Incarnation || (state.Authority && *state.Authority != aRetainedComparison->Authority))) {
			return { };
		}
		// Shutdown clears live authority. A captured fixture comparison lets
		// this read-only inspector reopen the same catalog and retained refs.
		const auto      selectedAuthority        = state.Authority ? state.Authority
			: aRetainedComparison                                  ? std::optional{ aRetainedComparison->Authority }
																   : std::nullopt;
		const auto      root                     = implementation->Fixture->application_data_root();
		auto            retiredRepositoryPresent = false;
		auto            retiredMarkerPresent     = false;
		std::error_code directoryError;
		auto            marker = std::filesystem::directory_iterator{
			root / "quarantine" / "v1", directoryError
		};
		while (!directoryError
			   && marker != std::filesystem::directory_iterator{ }) {
			if (marker->path().extension() == ".deleted") {
				retiredMarkerPresent = true;
				auto repository      = marker->path().stem();
				repository += ".git";
				std::error_code existsError;
				retiredRepositoryPresent = retiredRepositoryPresent
					|| std::filesystem::exists(
											   root / "repositories" / "v1" / repository,
											   existsError);
			}
			marker.increment(directoryError);
		}
		// Whole-cache deletion may leave a replacement authority projected while
		// shutdown is rejecting its unfinished rehydration.  An empty target asks
		// for durable evidence about the retired cache, so derive the stable
		// catalog binding from the captured smart authority instead of sampling
		// that transient projection.
		if (!selectedAuthority.has_value()
			|| (!core::valid(aTarget) && retiredMarkerPresent)) {
			auto       catalogRevision  = std::int64_t{ 0 };
			auto       baseCacheDeleted = false;
			auto       headCacheDeleted = false;
			auto       baseAvailability = -1;
			auto       headAvailability = -1;
			const auto smartAuthority   = implementation->Fixture->last_smart_authority();
			const auto fixtureEvidence  = implementation->Fixture->evidence();
			const auto base             = core::parse_git_commit_oid(fixtureEvidence.BaseOid);
			if (smartAuthority && base) {
				auto reopened = implementation->EpochCatalogs->open(
					application::epoch_catalog_open_request{
						.ApplicationSession = state.ApplicationSession,
						.Authority          = {
							.Profile         = smartAuthority->Profile,
							.ProfileRevision = smartAuthority->ProfileRevision,
							.Host            = smartAuthority->Host,
							.HostAuthority   = smartAuthority->HostAuthority,
							.Account         = smartAuthority->Account,
							.Viewer          = smartAuthority->Viewer,
							.Repository      = smartAuthority->Repository,
							.PullRequest     = smartAuthority->PullRequest,
							.Number          = smartAuthority->Number,
							.ObjectFormat    = base->algorithm(),
						},
					},
					{ });
				if (reopened && *reopened) {
					const auto& binding = (*reopened)->binding();
					catalogRevision     = binding.Catalog.Revision.value();
					aTarget             = core::review_epoch_id{
						.Authority   = binding.Authority.Scope,
						.Incarnation = binding.Catalog.Incarnation,
						.Ordinal     = core::epoch_ordinal{ 1 },
					};
					const auto anchors = (*reopened)->exact_anchors(
						application::exact_anchor_lookup_request{
							.Catalog = binding.Catalog,
							.Anchors = {
								core::comparison_anchor_identity{
									.Authority   = aTarget.Authority,
									.Incarnation = aTarget.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_base,
									.Epoch = aTarget.Ordinal,
								},
								core::comparison_anchor_identity{
									.Authority   = aTarget.Authority,
									.Incarnation = aTarget.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_head,
									.Epoch = aTarget.Ordinal,
								},
							},
						},
						{ });
					if (anchors && anchors->Anchors.size() == 2U) {
						baseAvailability = static_cast<int>(
							anchors->Anchors[0].Availability);
						headAvailability = static_cast<int>(
							anchors->Anchors[1].Availability);
						baseCacheDeleted = anchors->Anchors[0].Availability
							== core::anchor_availability::cache_deleted;
						headCacheDeleted = anchors->Anchors[1].Availability
							== core::anchor_availability::cache_deleted;
					}
				}
			}
			return {
				.PruneIntentDispatchCount          = implementation->EpochPruneIntentDispatchCount,
				.DeleteIntentDispatchCount         = implementation->EpochDeleteIntentDispatchCount,
				.CancelIntentDispatchCount         = implementation->EpochCancelIntentDispatchCount,
				.CatalogRevision                   = catalogRevision,
				.RepositoryPresent                 = retiredRepositoryPresent,
				.CacheDeletionMarkerPresent        = retiredMarkerPresent,
				.TargetBaseCacheDeleted            = baseCacheDeleted,
				.TargetHeadCacheDeleted            = headCacheDeleted,
				.RetiredRepositoryPresent          = retiredRepositoryPresent,
				.RetiredCacheDeletionMarkerPresent = retiredMarkerPresent,
				.TargetBaseAvailability            = baseAvailability,
				.TargetHeadAvailability            = headAvailability,
			};
		}

		const auto lowerHex = [](const std::span<const std::uint8_t> aBytes) {
			return QByteArray::fromRawData(reinterpret_cast<const char*>(aBytes.data()), static_cast<qsizetype>(aBytes.size())).toHex().toStdString();
		};

		const auto& authority = *selectedAuthority;
		if (!core::valid(aTarget) && state.Catalog) {
			aTarget = core::review_epoch_id{
				.Authority   = authority.Scope,
				.Incarnation = state.Catalog->Incarnation,
				.Ordinal     = core::epoch_ordinal{ 1 },
			};
		}
		const auto cache       = lowerHex(authority.Cache.bytes());
		const auto scope       = lowerHex(aTarget.Authority.bytes());
		const auto repository  = root / "repositories" / "v1" / (cache + ".git");
		const auto ordinalText = [&lowerHex](std::int64_t aOrdinal) {
			std::array<std::uint8_t, 8U> bytes{ };
			const auto                   ordinal = static_cast<std::uint64_t>(aOrdinal);
			for (std::size_t index = 0U; index < bytes.size(); ++index) {
				bytes[index] = static_cast<std::uint8_t>(ordinal >> ((bytes.size() - index - 1U) * 8U));
			}
			return lowerHex(bytes);
		};
		// Installed Git encodes retained ref ordinals as 16 lowercase hex digits.
		const auto ordinal   = ordinalText(aTarget.Ordinal.value());
		const auto epochRoot = repository / "refs" / "betterpullreview" / "v1"
			/ "scope" / scope / "epoch"
			/ ordinal;
		const auto nonTargetOrdinal   = ordinalText(2);
		const auto nonTargetEpochRoot = repository / "refs"
			/ "betterpullreview" / "v1" / "scope" / scope / "epoch"
			/ nonTargetOrdinal;
		const auto      deletionMarker = root / "quarantine" / "v1" / (cache + ".deleted");
		std::error_code error;
		const auto      present = [&error](const std::filesystem::path& aPath) {
			error.clear();
			const auto result = std::filesystem::exists(aPath, error);
			return !error && result;
		};

		std::string baseCatalogOid, headCatalogOid;
		auto        baseCacheDeleted = false;
		auto        headCacheDeleted = false;
		auto        baseAvailability = -1;
		auto        headAvailability = -1;
		const auto& acceptedSnapshot = implementation->WindowRead->accepted_snapshot();
		const auto  acceptedBase     = acceptedSnapshot
			? core::parse_git_commit_oid(
				  acceptedSnapshot->PullRequest.BaseOid.value())
			: std::nullopt;
		const auto  objectFormat     = acceptedBase ? std::optional{ acceptedBase->algorithm() }
			: aRetainedComparison                   ? std::optional{ aRetainedComparison->LeftBlob->algorithm() }
													: std::nullopt;
		if (objectFormat && !state.PendingMaintenance.has_value()) {
			auto reopened = implementation->EpochCatalogs->open(
				application::epoch_catalog_open_request{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = {
						.Profile         = authority.Profile,
						.ProfileRevision = authority.ProfileRevision,
						.Host            = authority.Host,
						.HostAuthority   = authority.HostAuthority,
						.Account         = authority.Account,
						.Viewer          = authority.Viewer,
						.Repository      = authority.Repository,
						.PullRequest     = authority.PullRequest,
						.Number          = authority.Number,
						.ObjectFormat    = *objectFormat,
					},
				},
				{ });
			if (reopened && *reopened) {
				const auto& binding = (*reopened)->binding();
				if (!core::valid(aTarget)) {
					aTarget = core::review_epoch_id{
						.Authority   = binding.Authority.Scope,
						.Incarnation = binding.Catalog.Incarnation,
						.Ordinal     = core::epoch_ordinal{ 1 },
					};
				}
				const auto anchors = (*reopened)->exact_anchors(
					application::exact_anchor_lookup_request{
						.Catalog = binding.Catalog,
						.Anchors = {
							core::comparison_anchor_identity{
								.Authority   = aTarget.Authority,
								.Incarnation = aTarget.Incarnation,
								.Kind        = core::comparison_anchor_kind::epoch_base,
								.Epoch       = aTarget.Ordinal,
							},
							core::comparison_anchor_identity{
								.Authority   = aTarget.Authority,
								.Incarnation = aTarget.Incarnation,
								.Kind        = core::comparison_anchor_kind::epoch_head,
								.Epoch       = aTarget.Ordinal,
							},
						},
					},
					{ });
				if (anchors && anchors->Anchors.size() == 2U) {
					baseCatalogOid   = core::canonical_hex(anchors->Anchors[0].Commit).view();
					headCatalogOid   = core::canonical_hex(anchors->Anchors[1].Commit).view();
					baseAvailability = static_cast<int>(
						anchors->Anchors[0].Availability);
					headAvailability = static_cast<int>(
						anchors->Anchors[1].Availability);
					baseCacheDeleted = anchors->Anchors[0].Availability
						== core::anchor_availability::cache_deleted;
					headCacheDeleted = anchors->Anchors[1].Availability
						== core::anchor_availability::cache_deleted;
				}
			}
		}

		const auto refOid = [](const std::filesystem::path& aPath) {
			std::ifstream input{ aPath };
			std::string   oid;
			input >> oid;
			return oid;
		};
		return {
			.PruneIntentDispatchCount          = implementation->EpochPruneIntentDispatchCount,
			.DeleteIntentDispatchCount         = implementation->EpochDeleteIntentDispatchCount,
			.CancelIntentDispatchCount         = implementation->EpochCancelIntentDispatchCount,
			.CatalogRevision                   = state.Catalog
				? state.Catalog->Revision.value()
				: 0,
			.TargetBaseCatalogOid              = std::move(baseCatalogOid),
			.TargetHeadCatalogOid              = std::move(headCatalogOid),
			.TargetBaseRefOid                  = refOid(epochRoot / "base"),
			.TargetHeadRefOid                  = refOid(epochRoot / "head"),
			.TargetBaseRefPresent              = present(epochRoot / "base"),
			.TargetHeadRefPresent              = present(epochRoot / "head"),
			.NonTargetBaseRefPresent           = present(nonTargetEpochRoot / "base"),
			.NonTargetHeadRefPresent           = present(nonTargetEpochRoot / "head"),
			.RepositoryPresent                 = present(repository),
			.CacheDeletionMarkerPresent        = present(deletionMarker),
			.TargetBaseCacheDeleted            = baseCacheDeleted,
			.TargetHeadCacheDeleted            = headCacheDeleted,
			.RetiredRepositoryPresent          = retiredRepositoryPresent,
			.RetiredCacheDeletionMarkerPresent = retiredMarkerPresent,
			.TargetBaseAvailability            = baseAvailability,
			.TargetHeadAvailability            = headAvailability,
		};
	}

	namespace {
		[[nodiscard]] auto workbench_probe_invalidation(
			const application::accepted_pull_request_handoff& aHandoff,
			const core::pull_request_id                       aPullRequest)
			-> application::pull_request_authority_invalidation
		{
			return {
				.ApplicationSession = aHandoff.ApplicationSession,
				.Profile            = aHandoff.Profile,
				.ProfileRevision    = aHandoff.ProfileRevision,
				.Host               = aHandoff.Host,
				.HostAuthority      = aHandoff.HostAuthority,
				.Account            = aHandoff.Account,
				.Viewer             = aHandoff.Viewer,
				.Repository         = aHandoff.Repository,
				.PullRequest        = aPullRequest,
				.IdentityGeneration = aHandoff.IdentityGeneration,
			};
		}

		/// \brief Edits one colour through the swatch and the picker seam.
		///
		/// \details [[BPR-0790]]. The path the rebuilt Colours card offers: a swatch
		/// opens the picker, every colour the picker moves through previews in the
		/// review window, and the colour it lands on stays previewed. The picker is a
		/// settable seam exactly so this composed probe can drive it without a modal
		/// panel; the widget still performs no effect of its own.
		void pick_colour(
			qt::appearance_settings_panel& aPanel,
			const core::appearance_role    aRole,
			std::vector<QColor>            aMoves,
			const QColor&                  aAccepted)
		{
			aPanel.set_colour_picker(
				[moves = std::move(aMoves), accepted = aAccepted](
					const qt::colour_pick_request& aRequest)
					-> std::optional<QColor> {
					for (const auto& moved : moves) {
						aRequest.Changed(moved);
					}
					return accepted;
				});
			aPanel.colours()->swatch_of(aRole)->click();
		}
	} // namespace

	auto detail::github_application_fixture_access::select_other_appearance_mode(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		const auto current = implementation->AppearanceHost->settings().Mode;
		const auto other   = current == core::appearance_mode::dark
			? core::appearance_mode::light
			: core::appearance_mode::dark;
		implementation->AppearanceHost->select(other);
		return implementation->AppearanceHost->settings().Mode == other;
	}

	auto detail::github_application_fixture_access::apply_appearance_gesture(
		github_application&         aApplication,
		const core::appearance_mode aMode,
		const std::uint32_t         aTabWidth) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		auto* const panel = implementation->Window->appearance_settings();
		if (panel == nullptr) {
			return false;
		}
		// Through the controls, which is the whole claim. Choosing a theme tile and
		// clicking a tab-width segment is what a reviewer's click produces; if the
		// wiring between them and the host is missing, nothing below moves.
		panel->choose_mode(aMode);
		auto* const segment = panel->tab_width_option(aTabWidth);
		if (segment == nullptr) {
			// The segment offers the three widths the accepted design names; a
			// probe asking for another would not be driving a control that
			// exists.
			return false;
		}
		segment->click();
		panel->dif_visibility_switch()->setChecked(false);
		panel->motion_selector()->setCurrentIndex(static_cast<int>(core::motion_mode::full));
		const auto& settings = implementation->AppearanceHost->settings();
		return settings.Mode == aMode
			&& settings.Typography.Code.TabWidth == aTabWidth
			&& settings.Dif.Visibility == core::dif_visibility::hidden
			&& settings.Dif.Motion == core::motion_mode::full;
	}

	auto detail::github_application_fixture_access::refuse_appearance_gesture(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		auto* const panel = implementation->Window->appearance_settings();
		if (panel == nullptr) {
			return false;
		}
		const auto before = implementation->AppearanceHost->settings();
		// An empty family is the one illegal value a control can express: the
		// numeric controls carry the core's own bounds, so they cannot.
		panel->code_font_chooser()->setCurrentText(QString{ });
		Q_EMIT panel->code_font_chooser()->lineEdit()->editingFinished();
		const auto& after = implementation->AppearanceHost->settings();
		return after == before
			&& !implementation->AppearanceHost->last_rejections().empty()
			&& panel->status_text().startsWith(QStringLiteral("Refused"))
			&& panel->code_font_chooser()->currentText().toStdString()
			== before.Typography.Code.Font.Family;
	}

	namespace {
		/// \brief [[BPR-0860]], [[BPR-0864]], [[BPR-1037]]. How long one phase
		/// of a fixture probe may wait for an effect it does not control.
		///
		/// \details A phase's bounded wait names its outstanding condition.
		/// Five seconds remains the default; a course may supply its own finite
		/// functional watchdog without changing the outer run budget. The bound
		/// uses time rather than ticks because the event loop stretches under
		/// load. The call floor keeps one long tick from reading as a stall
		/// ([[BPR-0863]]).
		inline constexpr auto        fixture_probe_wait_cap   = std::chrono::milliseconds{ 5'000 };
		inline constexpr std::size_t fixture_probe_wait_floor = 16U;

		/// \brief [[BPR-0864]], [[BPR-1037]]. Counts one call of a probe phase
		/// that is still waiting.
		///
		/// \details A refusal on one tick means "not yet" (`src/app/AGENTS.md`),
		/// so a phase keeps waiting until its bound. Then this prints one line,
		/// `TERM:waited_ms=... wait_cap_ms=... waits=... <facts> <modality facts>`.
		/// `aFacts` writes the inputs to the condition the phase waits on. The
		/// line ends with the modality facts every course's failure ends with
		/// ([[BPR-0863]]), which include the host's outstanding requests.
		///
		/// \returns Whether the phase may keep waiting. `false` means it
		/// stalled, and the caller fails the probe.
		template <typename TFacts>
		[[nodiscard]] auto keep_waiting(
			const github_application&       aApplication,
			detail::fixture_probe_wait&     aWait,
			const char* const               aTerm,
			const TFacts&                   aFacts,
			const std::chrono::milliseconds aWaitCap = fixture_probe_wait_cap) -> bool
		{
			const auto now = std::chrono::steady_clock::now();
			if (!aWait.Since.has_value()) {
				aWait.Since = now;
			}
			++aWait.Calls;
			const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(now - *aWait.Since);
			if (waited < aWaitCap || aWait.Calls < fixture_probe_wait_floor) {
				return true;
			}
			auto stream = QTextStream{ stdout };
			stream << aTerm << ":waited_ms=" << static_cast<qlonglong>(waited.count())
				   << " wait_cap_ms=" << static_cast<qlonglong>(aWaitCap.count())
				   << " waits=" << static_cast<qulonglong>(aWait.Calls) << ' ';
			aFacts(stream);
			stream << ' ' << detail::github_application_fixture_access::course_modality_facts(aApplication) << '\n';
			stream.flush();
			return false;
		}

		/// \brief [[BPR-0864]], [[BPR-1037]]. Moves a probe to another phase.
		///
		/// \details The only thing that restarts a phase's wait, so a wait
		/// cannot restart itself on a tick ([[BPR-0859]]).
		template <typename TPhase>
		void enter_phase(TPhase& aPhase, detail::fixture_probe_wait& aWait, const TPhase aNext) noexcept
		{
			aPhase = aNext;
			aWait  = { };
		}
	} // namespace

	auto detail::github_application_fixture_access::
		exercise_appearance_theme_customization(
			github_application&    aApplication,
			const std::string_view aThemePath)
			-> detail::appearance_theme_customization_probe_result
	{
		using result               = detail::appearance_theme_customization_probe_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr
			|| implementation->AppearanceProfile == nullptr) {
			return result::failed;
		}
		auto* const panel     = implementation->Window->appearance_settings();
		auto* const workbench = implementation->Window->comparison_surface();
		if (panel == nullptr || workbench == nullptr) {
			return result::failed;
		}
		const auto reportStage = [](const char* const aStage) {
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_STAGE:" << aStage << '\n';
			stream.flush();
		};

		using state_type = appearance_theme_probe_state;
		if (!implementation->AppearanceThemeProbe.has_value()) {
			implementation->AppearanceThemeProbe.emplace();
			implementation->AppearanceThemeProbe->Path = std::string{ aThemePath };
		}

		auto& probe = *implementation->AppearanceThemeProbe;
		if (probe.Path != aThemePath || probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		const auto unchanged = [&]() {
			const auto statistics = workbench->statistics();
			return workbench->current_result() == probe.Result
				&& workbench->current_context() == probe.Context
				&& workbench->selection() == probe.Selection
				&& workbench->scroll_controller()->position() == probe.Position
				&& workbench->token_spans(core::diff_side::left) == probe.LeftSpans
				&& workbench->token_spans(core::diff_side::right) == probe.RightSpans
				&& statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds
				&& statistics.SemanticIndexAdoptions == probe.Statistics.SemanticIndexAdoptions
				&& statistics.SelectionChanges == probe.Statistics.SelectionChanges
				&& statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions
				&& implementation->AppearanceHost->classification_submissions()
				== probe.ClassificationSubmissions;
		};
		const auto reportIdentity = [&]() {
			const auto  statistics = workbench->statistics();
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_IDENTITY_FACTS:"
				   << (workbench->current_result() == probe.Result)
				   << (workbench->current_context() == probe.Context)
				   << (workbench->selection() == probe.Selection)
				   << (workbench->scroll_controller()->position() == probe.Position)
				   << (workbench->token_spans(core::diff_side::left) == probe.LeftSpans)
				   << (workbench->token_spans(core::diff_side::right) == probe.RightSpans)
				   << (statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds)
				   << (statistics.SemanticIndexAdoptions == probe.Statistics.SemanticIndexAdoptions)
				   << (statistics.SelectionChanges == probe.Statistics.SelectionChanges)
				   << (statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions)
				   << (implementation->AppearanceHost->classification_submissions()
					   == probe.ClassificationSubmissions)
				   << '\n';
			stream.flush();
		};
		const auto fail = [&]() {
			reportStage("failed");
			probe.Phase = appearance_theme_probe_state::phase::failed;
			return result::failed;
		};
		// [[BPR-1037]]. Every phase that waits is bounded and names what it
		// was waiting for; see `keep_waiting`.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		const auto phaseName = [](const state_type::phase aPhase) {
			switch (aPhase) {
			case state_type::phase::awaiting_comparison:
				return "awaiting_comparison";
			case state_type::phase::awaiting_stable_comparison:
				return "awaiting_stable_comparison";
			case state_type::phase::awaiting_manual_preview:
				return "awaiting_manual_preview";
			case state_type::phase::awaiting_preview_adoption:
				return "awaiting_preview_adoption";
			case state_type::phase::awaiting_preview_restoration:
				return "awaiting_preview_restoration";
			case state_type::phase::awaiting_first_preview:
				return "awaiting_first_preview";
			case state_type::phase::awaiting_second_preview:
				return "awaiting_second_preview";
			case state_type::phase::awaiting_store:
				return "awaiting_store";
			case state_type::phase::awaiting_stored_preview:
				return "awaiting_stored_preview";
			case state_type::phase::awaiting_stored_preview_adoption:
				return "awaiting_stored_preview_adoption";
			case state_type::phase::awaiting_stored_preview_restoration:
				return "awaiting_stored_preview_restoration";
			case state_type::phase::awaiting_equal_stored_preview:
				return "awaiting_equal_stored_preview";
			case state_type::phase::awaiting_equal_stored_commit:
				return "awaiting_equal_stored_commit";
			case state_type::phase::awaiting_export:
				return "awaiting_export";
			case state_type::phase::awaiting_export_refusal:
				return "awaiting_export_refusal";
			case state_type::phase::passed:
				return "passed";
			case state_type::phase::failed:
				return "failed";
			}
			return "unknown";
		};
		// What an imported theme's preview depends on.
		const auto previewFacts = [&](QTextStream& aStream) {
			aStream << "previewing=" << truth(implementation->AppearanceProfile->previewing_palette())
					<< " panel_preview=" << present(panel->preview_palette().has_value())
					<< " profile_generation=" << static_cast<qulonglong>(implementation->AppearanceProfile->generation())
					<< " baseline_generation=" << static_cast<qulonglong>(probe.ProfileGeneration)
					<< " status=\"" << panel->status_text() << '"';
		};

		// Every phase reads the comparison and its spans, and none may run while
		// the appearance host has a request outstanding.
		if (workbench->current_result() == nullptr
			|| !workbench->current_context().has_value()
			|| !workbench->selection().has_value()
			|| !workbench->token_spans(core::diff_side::left)
			|| !workbench->token_spans(core::diff_side::right)
			|| implementation->AppearanceHost->outstanding_requests() != 0U) {
			return wait("APPEARANCE_THEME_COMPARISON_STALLED", [&](QTextStream& aStream) {
				aStream << "phase=" << phaseName(probe.Phase)
						<< " comparison_result=" << present(workbench->current_result() != nullptr)
						<< " comparison_context=" << present(workbench->current_context().has_value())
						<< " comparison_selection=" << present(workbench->selection().has_value())
						<< " left_spans=" << present(static_cast<bool>(workbench->token_spans(core::diff_side::left)))
						<< " right_spans=" << present(static_cast<bool>(workbench->token_spans(core::diff_side::right)))
						<< " appearance_outstanding=" << static_cast<qulonglong>(implementation->AppearanceHost->outstanding_requests());
			});
		}

		// The baselines every later phase compares against are taken the first
		// time the comparison is ready.
		if (probe.Phase == state_type::phase::awaiting_comparison) {
			probe.BeforeSettings    = implementation->AppearanceHost->settings();
			probe.BeforePalette     = implementation->AppearanceProfile->palette();
			probe.NativeObservation = github_application::implementation::
				native_appearance_observation();
			probe.Result            = workbench->current_result();
			probe.Context           = workbench->current_context();
			probe.Selection         = workbench->selection();
			probe.LeftSpans         = workbench->token_spans(core::diff_side::left);
			probe.RightSpans        = workbench->token_spans(core::diff_side::right);
			probe.Statistics        = workbench->statistics();
			probe.Position          = workbench->scroll_controller()->position();
			probe.ProfileGeneration = implementation->AppearanceProfile->generation();
			probe.ClassificationSubmissions
				= implementation->AppearanceHost->classification_submissions();
			probe.PersistedSettings
				= implementation->AppearanceHost->persisted_settings();
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stable_comparison);
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stable_comparison) {
			if (!unchanged()) {
				// Which of the comparison's facts moved since the last call,
				// in the order APPEARANCE_THEME_PROBE_IDENTITY_FACTS uses.
				const auto statistics = workbench->statistics();
				const auto same       = std::array{
					workbench->current_result() == probe.Result,
					workbench->current_context() == probe.Context,
					workbench->selection() == probe.Selection,
					workbench->scroll_controller()->position() == probe.Position,
					workbench->token_spans(core::diff_side::left) == probe.LeftSpans,
					workbench->token_spans(core::diff_side::right) == probe.RightSpans,
					statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds,
					statistics.SemanticIndexAdoptions == probe.Statistics.SemanticIndexAdoptions,
					statistics.SelectionChanges == probe.Statistics.SelectionChanges,
					statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions,
					implementation->AppearanceHost->classification_submissions()
						== probe.ClassificationSubmissions,
				};
				probe.Result     = workbench->current_result();
				probe.Context    = workbench->current_context();
				probe.Selection  = workbench->selection();
				probe.LeftSpans  = workbench->token_spans(core::diff_side::left);
				probe.RightSpans = workbench->token_spans(core::diff_side::right);
				probe.Statistics = workbench->statistics();
				probe.Position   = workbench->scroll_controller()->position();
				probe.ClassificationSubmissions
					= implementation->AppearanceHost->classification_submissions();
				return wait("APPEARANCE_THEME_STABLE_COMPARISON_STALLED", [same](QTextStream& aStream) {
					aStream << "unchanged=";
					for (const auto fact : same) {
						aStream << (fact ? '1' : '0');
					}
				});
			}
			const auto role       = core::appearance_role::diff_added_background;
			probe.ManualCandidate = probe.BeforePalette;
			auto& color           = probe.ManualCandidate[core::index_of(role)];
			color.Red             = color.Red == 0U ? 1U : 0U;
			// Through the swatch and the picker seam, which is the accepted path:
			// the picker reports every colour it moves through and then the one it
			// landed on, so one gesture offers the burst of candidates the three
			// preview clicks used to.
			pick_colour(
				*panel,
				role,
				{ QColor{ 0x11, 0x22, 0x33 }, QColor{ 0x44, 0x55, 0x66 } },
				qt::to_presentation_color(color));
			if (implementation->AppearanceProfile->generation() != probe.ProfileGeneration) {
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_manual_preview);
			reportStage("manual-preview-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_manual_preview) {
			const auto statistics = workbench->statistics();
			const auto profilePreviewed
				= implementation->AppearanceProfile->previewing_palette();
			const auto profileMatches = implementation->AppearanceProfile->palette()
				== probe.ManualCandidate;
			const auto panelMatches
				= panel->preview_palette() == std::optional{ probe.ManualCandidate };
			const auto settingsUnchanged
				= implementation->AppearanceHost->settings() == probe.BeforeSettings;
			const auto writesUnchanged
				= implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings;
			const auto generationChanged
				= implementation->AppearanceProfile->generation()
				== probe.ProfileGeneration + 1U;
			const auto paintChanged = statistics.AlignmentSegmentsVisited
				> probe.Statistics.AlignmentSegmentsVisited;
			const auto identityUnchanged = unchanged();
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_MANUAL_FACTS:"
					   << profilePreviewed << profileMatches << panelMatches
					   << settingsUnchanged << writesUnchanged << generationChanged
					   << paintChanged << identityUnchanged << '\n';
				stream.flush();
			}
			if (!profilePreviewed || !profileMatches || !panelMatches
				|| !settingsUnchanged || !writesUnchanged || !generationChanged
				|| !paintChanged || !identityUnchanged) {
				return fail();
			}
			reportStage("manual-preview");
			auto alternateObservation = probe.NativeObservation;
			alternateObservation.Scheme
				= alternateObservation.Scheme == core::os_color_scheme::dark
				? core::os_color_scheme::light
				: core::os_color_scheme::dark;
			implementation->AppearanceHost->observe(alternateObservation);
			implementation->AppearanceProfile->observe(alternateObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_preview_adoption);
			reportStage("unrelated-adoption-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_preview_adoption) {
			const auto adoptionFacts
				= std::array{
					  implementation->AppearanceProfile->previewing_palette(),
					  implementation->AppearanceProfile->palette()
						  == probe.ManualCandidate,
					  panel->preview_palette()
						  == std::optional{ probe.ManualCandidate },
					  panel->preview_bar() != nullptr,
					  panel->keep_changes_button() != nullptr,
					  implementation->AppearanceHost->settings()
						  == probe.BeforeSettings,
					  implementation->AppearanceHost->persisted_settings()
						  == probe.PersistedSettings,
					  unchanged(),
				  };
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_ADOPTION_FACTS:";
				for (const auto fact : adoptionFacts) {
					stream << fact;
				}
				stream << '\n';
				stream.flush();
			}
			if (!std::ranges::all_of(adoptionFacts, std::identity{ })) {
				return fail();
			}
			implementation->AppearanceHost->observe(probe.NativeObservation);
			implementation->AppearanceProfile->observe(probe.NativeObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_preview_restoration);
			reportStage("native-observation-restored");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_preview_restoration) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.ManualCandidate
				|| panel->preview_palette()
					!= std::optional{ probe.ManualCandidate }
				|| panel->preview_bar() == nullptr
				|| panel->keep_changes_button() == nullptr
				|| implementation->AppearanceHost->settings()
					!= probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			implementation->Window->settings()->close();
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette()
					!= probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			reportStage("manual-cancelled");
			implementation->Window->open_settings(qt::settings_page::appearance);
			pick_colour(
				*panel,
				core::appearance_role::diff_added_background,
				{ },
				QColor{ 0x11, 0x22, 0x33 });
			implementation->Window->settings()->close();
			QCoreApplication::sendPostedEvents(panel, QEvent::MetaCall);
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.BeforePalette) {
				return fail();
			}
			implementation->Window->open_settings(qt::settings_page::appearance);
			pick_colour(
				*panel,
				core::appearance_role::diff_added_background,
				{ },
				QColor{ 0x11, 0x22, 0x33 });
			if (implementation->AppearanceProfile->previewing_palette() || !panel->preview_palette()) {
				return fail();
			}
			auto* reset = static_cast<QPushButton*>(nullptr);
			for (auto* button : panel->findChildren<QPushButton*>()) {
				if (button->accessibleName() == QStringLiteral("Reset all colours")) {
					reset = button;
				}
			}
			if (reset == nullptr) {
				return fail();
			}
			reset->click();
			QCoreApplication::sendPostedEvents(panel, QEvent::MetaCall);
			if (implementation->AppearanceProfile->previewing_palette() || panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings() != probe.PersistedSettings) {
				return fail();
			}
			reportStage("palette-reset-cleared-preview");
			probe.Statistics = workbench->statistics();
			probe.ProfileGeneration
				= implementation->AppearanceProfile->generation();
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_first_preview);
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("import-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_first_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()) {
				if (panel->status_text().contains(QStringLiteral("refused"), Qt::CaseInsensitive)
					|| panel->status_text().contains(QStringLiteral("failed"), Qt::CaseInsensitive)) {
					return fail();
				}
				return wait("APPEARANCE_THEME_FIRST_PREVIEW_STALLED", previewFacts);
			}
			if (!panel->preview_palette().has_value()) {
				return fail();
			}
			probe.Candidate = implementation->AppearanceProfile->palette();
			reportStage("first-preview");
			const auto statistics       = workbench->statistics();
			const auto candidateChanged = probe.Candidate != probe.BeforePalette;
			const auto panelMatches     = *panel->preview_palette() == probe.Candidate;
			const auto settingsUnchanged
				= implementation->AppearanceHost->settings() == probe.BeforeSettings;
			const auto writesUnchanged
				= implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings;
			const auto generationChanged
				= implementation->AppearanceProfile->generation()
				> probe.ProfileGeneration;
			const auto paintChanged = statistics.SyntaxBlocksFormatted
				> probe.Statistics.SyntaxBlocksFormatted;
			const auto identityUnchanged = unchanged();
			reportIdentity();
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_FIRST_FACTS:"
					   << candidateChanged << panelMatches << settingsUnchanged
					   << writesUnchanged << generationChanged << paintChanged
					   << identityUnchanged << '\n';
				stream.flush();
			}
			if (!candidateChanged || !panelMatches || !settingsUnchanged
				|| !writesUnchanged || !generationChanged || !paintChanged
				|| !identityUnchanged) {
				return fail();
			}
			panel->cancel_changes_button()->click();
			reportStage("cancelled");
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette()
					!= probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_second_preview);
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("second-import-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_second_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()) {
				return wait("APPEARANCE_THEME_SECOND_PREVIEW_STALLED", previewFacts);
			}
			if (!panel->preview_palette().has_value()
				|| *panel->preview_palette() != probe.Candidate
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			reportStage("second-preview");
			probe.CustomisedVariant = implementation->AppearanceProfile->variant();
			panel->keep_changes_button()->click();
			reportStage("commit-dispatched");
			const auto committed   = application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant);
			const auto commitFacts = std::array{
				!implementation->AppearanceProfile->previewing_palette(),
				!panel->preview_palette().has_value(),
				implementation->AppearanceProfile->palette() == probe.Candidate,
				committed.Palette == probe.Candidate,
				committed.Stored,
				unchanged(),
			};
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_COMMIT_FACTS:";
				for (const auto fact : commitFacts) {
					stream << fact;
				}
				stream << '\n';
				// [[BPR-1479]]. The theme the colours were kept on, which the
				// export names as its base.
				const auto variant = core::stable_name(probe.CustomisedVariant);
				stream << "APPEARANCE_THEME_PROBE_CUSTOMISED_VARIANT:"
					   << QString::fromUtf8(variant.data(), static_cast<qsizetype>(variant.size())) << '\n';
				// A refused commit says why only in the panel, as a contrast
				// rejection naming each role.
				if (!std::ranges::all_of(commitFacts, std::identity{ })) {
					stream << "APPEARANCE_THEME_PROBE_COMMIT_STATUS:" << panel->status_text().simplified() << '\n';
				}
				stream.flush();
			}
			if (!std::ranges::all_of(commitFacts, std::identity{ })) {
				reportIdentity();
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_store);
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_store) {
			if (implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings) {
				return wait("APPEARANCE_THEME_STORE_STALLED", [&](QTextStream& aStream) {
					aStream << "persisted_settings=" << static_cast<qulonglong>(implementation->AppearanceHost->persisted_settings())
							<< " expected_persisted_settings=" << static_cast<qulonglong>(probe.PersistedSettings) + 1U
							<< " stored=" << truth(application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Stored)
							<< " previewing=" << truth(implementation->AppearanceProfile->previewing_palette())
							<< " panel_preview=" << present(panel->preview_palette().has_value());
				});
			}
			if (implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| !unchanged()) {
				return fail();
			}
			reportStage("persisted");
			probe.StoredCandidate = probe.Candidate;
			const auto role       = core::appearance_role::diff_removed_background;
			auto&      color      = probe.StoredCandidate[core::index_of(role)];
			color.Blue            = color.Blue == 0U ? 1U : 0U;
			pick_colour(*panel, role, { }, qt::to_presentation_color(color));
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stored_preview);
			reportStage("stored-palette-preview-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stored_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.StoredCandidate
				|| panel->preview_palette()
					!= std::optional{ probe.StoredCandidate }
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			auto alternateObservation = probe.NativeObservation;
			alternateObservation.Scheme
				= alternateObservation.Scheme == core::os_color_scheme::dark
				? core::os_color_scheme::light
				: core::os_color_scheme::dark;
			implementation->AppearanceHost->observe(alternateObservation);
			implementation->AppearanceProfile->observe(alternateObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stored_preview_adoption);
			reportStage("stored-palette-adoption-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stored_preview_adoption) {
			const auto adoptionFacts = std::array{
				implementation->AppearanceProfile->previewing_palette(),
				implementation->AppearanceProfile->palette()
					== probe.StoredCandidate,
				application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette
					== probe.Candidate,
				panel->preview_palette() == std::optional{ probe.StoredCandidate },
				panel->preview_bar() != nullptr,
				panel->keep_changes_button() != nullptr,
				application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					== probe.Candidate,
				implementation->AppearanceHost->persisted_settings()
					== probe.PersistedSettings + 1U,
				unchanged(),
			};
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_STORED_ADOPTION_FACTS:";
				for (const auto fact : adoptionFacts) {
					stream << fact;
				}
				stream << '\n';
				stream.flush();
			}
			if (!std::ranges::all_of(adoptionFacts, std::identity{ })) {
				return fail();
			}
			implementation->AppearanceHost->observe(probe.NativeObservation);
			implementation->AppearanceProfile->observe(probe.NativeObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stored_preview_restoration);
			reportStage("stored-palette-observation-restored");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stored_preview_restoration) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.StoredCandidate
				|| application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| panel->preview_palette()
					!= std::optional{ probe.StoredCandidate }
				|| panel->preview_bar() == nullptr
				|| panel->keep_changes_button() == nullptr
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			panel->cancel_changes_button()->click();
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.Candidate
				|| application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			reportStage("stored-palette-preview-cancelled");
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_equal_stored_preview);
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("equal-stored-import-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_equal_stored_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette() != probe.Candidate
				|| panel->preview_palette() != std::optional{ probe.Candidate }
				|| panel->preview_bar() == nullptr
				|| panel->keep_changes_button() == nullptr
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			panel->keep_changes_button()->click();
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_equal_stored_commit);
			reportStage("equal-stored-commit-dispatched");
			return result::pending;
		}

		const auto equalCommitFacts = std::array{
			!implementation->AppearanceProfile->previewing_palette(),
			implementation->AppearanceProfile->palette() == probe.Candidate,
			application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette == probe.Candidate,
			!panel->preview_palette().has_value(),
			panel->preview_bar() == nullptr,
			panel->keep_changes_button() == nullptr,
			application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette == probe.Candidate,
			implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings + 1U,
			unchanged(),
		};
		{
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_EQUAL_COMMIT_FACTS:";
			for (const auto fact : equalCommitFacts) {
				stream << fact;
			}
			stream << '\n';
			stream.flush();
		}
		if (!std::ranges::all_of(equalCommitFacts, std::identity{ })) {
			return fail();
		}
		using phase = appearance_theme_probe_state::phase;
		if (probe.Phase == phase::awaiting_equal_stored_commit) {
			enter_phase(probe.Phase, probe.Wait, phase::awaiting_export);
			panel->request_theme_export(QString::fromStdString(probe.Path + ".exported.bprtheme"));
			return result::pending;
		}
		if (probe.Phase == phase::awaiting_export) {
			if (!panel->status_text().startsWith(QStringLiteral("Saved theme exported."))) {
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, phase::awaiting_export_refusal);
			// The existing source's parent directory is a nonregular destination.
			panel->request_theme_export(QFileInfo{ QString::fromStdString(probe.Path) }.absolutePath());
			return result::pending;
		}
		if (!panel->status_text().startsWith(QStringLiteral("Theme export failed."))) {
			return fail();
		}
		reportStage("export-and-directory-refusal-acknowledged");
		probe.Phase = phase::passed;
		reportStage("equal-stored-commit-acknowledged");
		return result::passed;
	}

	auto detail::github_application_fixture_access::exercise_workbench_composition(
		github_application& aApplication)
		-> detail::workbench_composition_probe_result
	{
		using result               = detail::workbench_composition_probe_result;
		using state_type           = workbench_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return result::failed;
		}

		if (!implementation->WorkbenchCompositionProbe.has_value()) {
			implementation->WorkbenchCompositionProbe.emplace();
		}
		auto& probe = *implementation->WorkbenchCompositionProbe;
		probe.Evidence.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U;
		auto* const host       = implementation->WorkbenchHost.get();
		auto* const window     = implementation->Window.get();
		auto* const comparison = window->comparison_surface();
		const auto  fail       = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		// [[BPR-0860]], [[BPR-0864]]. Every phase that waits does so through
		// the shared bounded wait, which names the condition and prints what it
		// depends on when it gives up ([[BPR-1037]]).
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		// [[BPR-0860]]. What the drawer's room and its width store depend on.
		const auto widthFacts = [&](QTextStream& aStream) {
			aStream
				<< "dispatches=" << static_cast<qulonglong>(implementation->WorkbenchWidthDispatches)
				<< " persisted_layouts=" << static_cast<qulonglong>(host->persisted_layouts())
				<< " layout_updates=" << static_cast<qulonglong>(window->statistics().LayoutUpdates)
				<< " host_files_width=" << static_cast<qulonglong>(host->layout().FilesWidth)
				<< " splitter_widgets=" << static_cast<qulonglong>(window->central_splitter()->sizes().size())
				<< " persistence_available=" << truth(implementation->WorkbenchPersistenceAvailable)
				<< " profile_scoped="
				<< truth(implementation->WorkbenchSettings != nullptr
						 && implementation->WorkbenchSettings->profile_scoped());
			// The drag's baselines and target exist only once it is made.
			if (probe.Phase == state_type::phase::awaiting_width_store) {
				aStream
					<< " expected_dispatches=" << static_cast<qulonglong>(probe.WidthDispatches) + 1U
					<< " expected_persisted=" << static_cast<qulonglong>(probe.PersistedLayouts) + 1U
					<< " requested_files_width=" << probe.RequestedFilesWidth
					<< " initial_files_width=" << static_cast<qulonglong>(probe.InitialFilesWidth);
			}
		};

		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& handoff  = *implementation->LastAcceptedHandoff;
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| !host->state().Navigation.has_value()) {
				// [[BPR-0864]]. The accepted pull request's files reach the
				// workbench host as an observation; its navigation is what the
				// file gesture below needs.
				return wait("WORKBENCH_OBSERVATION_STALLED", [&](QTextStream& aStream) {
					aStream << "snapshot=" << present(snapshot.has_value())
							<< " snapshot_files=" << static_cast<qulonglong>(snapshot.has_value() ? snapshot->Files.size() : 0U)
							<< " window_read_quiescent=" << truth(implementation->WindowRead->is_quiescent())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< " actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " review_active=" << truth(implementation->WorkbenchReview.active());
				});
			}

			probe.Authority = workbench_authority(handoff);
			probe.ExpectedFiles.clear();
			for (const auto& file : snapshot->Files) {
				const auto identity = core::file_identity_of(file.Path);
				if (!identity.valid()) {
					return fail();
				}
				if (std::ranges::find(probe.ExpectedFiles, identity)
					== probe.ExpectedFiles.end()) {
					probe.ExpectedFiles.push_back(identity);
				}
			}

			probe.Evidence.ExactAuthority
				= application::valid(probe.Authority)
				&& implementation->WorkbenchReview.exact(handoff)
				&& probe.Authority.ApplicationSession
					== handoff.ApplicationSession
				&& probe.Authority.Profile == handoff.Profile
				&& probe.Authority.ProfileRevision == handoff.ProfileRevision
				&& probe.Authority.Host == handoff.Host
				&& probe.Authority.HostAuthority == handoff.HostAuthority
				&& probe.Authority.Account == handoff.Account
				&& probe.Authority.Viewer == handoff.Viewer
				&& probe.Authority.Repository == handoff.Repository
				&& probe.Authority.PullRequest == handoff.PullRequest
				&& probe.Authority.Number == handoff.Number
				&& probe.Authority.Snapshot == handoff.Snapshot
				&& probe.Authority.IdentityGeneration
					== handoff.IdentityGeneration
				&& probe.Authority.PullRequestGeneration
					== handoff.PullRequestGeneration;
			probe.Evidence.BoundedObservation
				= !probe.ExpectedFiles.empty()
				&& probe.ExpectedFiles.size()
					<= application::workbench_host_limits{ }.ActionableFiles
				&& std::ranges::equal(host->actionable(), probe.ExpectedFiles)
				&& host->state().Navigation.has_value();
			// [[BPR-0984]]. The session store also holds this window's observed
			// native geometry (BPR-0977): presentation the window reported, not
			// a layout storage supplied. Every stored field is still the default.
			auto sessionLayout     = host->layout();
			sessionLayout.Geometry = { };
			probe.Evidence.SessionSettingsFallback
				= !implementation->WorkbenchPersistenceAvailable
				&& implementation->WorkbenchSettings != nullptr
				&& sessionLayout == application::workbench_layout_settings{ };
			if (!probe.Evidence.ExactAuthority
				|| !probe.Evidence.BoundedObservation
				|| !probe.Evidence.SessionSettingsFallback
				|| window->files_drawer() == nullptr
				|| window->files_drawer()->contents() == nullptr
				|| window->files_drawer()->contents_view() == nullptr
				|| window->files_drawer()->contents()->rowCount() != 1) {
				return fail();
			}

			probe.IntentDispatches   = implementation->WorkbenchIntentDispatches;
			probe.WindowStatistics   = window->statistics();
			probe.SelectedDiffBegins = implementation->SelectedDiffBegins;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			auto* const view           = window->files_drawer()->contents_view();
			view->setCurrentIndex(view->model()->index(0, 0));
			return advance(state_type::phase::awaiting_file_projection);
		}

		if (probe.Phase == state_type::phase::awaiting_file_projection) {
			const auto statistics = window->statistics();
			if (implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches) {
				if (statistics.FileSelections
					== probe.WindowStatistics.FileSelections + 1U) {
					probe.Evidence.FileGesture = true;
					return fail();
				}
				// [[BPR-0864]]. Neither the files drawer's selection nor the
				// intent it dispatches has arrived.
				return wait("WORKBENCH_FILE_SELECTION_STALLED", [&](QTextStream& aStream) {
					const auto* const view = window->files_drawer()->contents_view();
					aStream << "intent_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchIntentDispatches)
							<< " expected_intent_dispatches=" << static_cast<qulonglong>(probe.IntentDispatches) + 1U
							<< " file_selections=" << static_cast<qulonglong>(statistics.FileSelections)
							<< " expected_file_selections=" << static_cast<qulonglong>(probe.WindowStatistics.FileSelections) + 1U
							<< " current_row=" << view->currentIndex().row()
							<< " drawer_rows=" << window->files_drawer()->contents()->rowCount()
							<< " host_selected=" << present(host->selected().has_value());
				});
			}
			probe.Evidence.IntentDispatch
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			probe.Evidence.FileGesture
				= statistics.FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			// [[BPR-1328]]. The fixture's only file is also its last, so the
			// delivered commands bound Next file there: the arrow stays enabled
			// and names the bound, where before BPR-1328 it was disabled.
			probe.Evidence.ProjectionDelivery
				= host->selected() == std::optional{ probe.ExpectedFiles.front() }
				&& statistics.Publications > probe.WindowStatistics.Publications
				&& statistics.CommandUpdates
					> probe.WindowStatistics.CommandUpdates
				&& window->next_file_action()->isEnabled()
				&& window->next_file_action()->statusTip()
					== QStringLiteral("Last file in this pull request.");
			if (!probe.Evidence.IntentDispatch || !probe.Evidence.FileGesture
				|| !probe.Evidence.ProjectionDelivery) {
				return fail();
			}
			return advance(state_type::phase::awaiting_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_diff) {
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (comparison == nullptr || comparison->current_result() == nullptr
				|| !comparison->selection().has_value()
				|| implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications
				|| !implementation->LastSelectedDiff.has_value()) {
				// [[BPR-0864]]. The selected file's diff is computed through
				// the Git and cache actors and published to the comparison.
				return wait("WORKBENCH_SELECTED_DIFF_STALLED", [&](QTextStream& aStream) {
					const auto work = diff_work_evidence(aApplication);
					aStream << "selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
							<< " expected_selected_diff_begins=" << static_cast<qulonglong>(probe.SelectedDiffBegins) + 1U
							<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
							<< " expected_selected_diff_publications=" << static_cast<qulonglong>(probe.SelectedDiffPublications) + 1U
							<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
							<< " last_selected_diff=" << present(implementation->LastSelectedDiff.has_value())
							<< " comparison_result=" << present(comparison != nullptr && comparison->current_result() != nullptr)
							<< " comparison_selection=" << present(comparison != nullptr && comparison->selection().has_value())
							<< " host_selected=" << present(host->selected().has_value())
							<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
							<< " diff_pending=" << truth(work.Pending)
							<< " diff_quiescent=" << truth(work.Quiescent)
							<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs);
				});
			}
			const auto& publication = *implementation->LastSelectedDiff;
			const auto& request     = publication.Request;
			const auto& key         = publication.Key;
			const auto& epoch       = implementation->EpochHost->state();
			const auto  fixture     = implementation->Fixture->evidence();
			const auto  leftBlob    = core::parse_git_blob_oid(fixture.BaseBlobOid);
			const auto  rightBlob   = core::parse_git_blob_oid(fixture.HeadOneBlobOid);
			probe.Evidence.SelectedDiffRequest
				= implementation->SelectedDiffBegins
					== probe.SelectedDiffBegins + 1U
				&& implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications + 1U
				&& request.WorkbenchAuthority == probe.Authority
				&& request.Context.Generation == host->generation()
				&& request.File.File == probe.ExpectedFiles.front()
				&& request.File.PresentationPath == "review.cpp"
				&& request.File.CurrentPath == std::filesystem::path{ "review.cpp" }
				&& !request.File.PreviousPath.has_value()
				&& request.File.Change
					== application::workbench_file_change_kind::modified;
			probe.Evidence.ExactComparison
				= epoch.Authority.has_value()
				&& epoch.AcceptedComparison.has_value()
				&& leftBlob.has_value() && rightBlob.has_value()
				&& key.Authority == *epoch.Authority
				&& key.Repository == epoch.Authority->Repository
				&& key.PullRequest == epoch.Authority->PullRequest
				&& key.LeftAnchor == epoch.AcceptedComparison->Left.Identity
				&& key.RightAnchor == epoch.AcceptedComparison->Right.Identity
				&& key.LeftBlob == leftBlob
				&& key.RightBlob == rightBlob
				&& key.File == probe.ExpectedFiles.front()
				&& key.Options == request.Diff.OptionsDigest
				&& key.Algorithm == core::supported_diff_algorithm_revision;
			if (!probe.Evidence.SelectedDiffRequest
				|| !probe.Evidence.ExactComparison) {
				QTextStream stream{ stdout };
				stream << "WORKBENCH_SELECTED_DIFF_FACTS:begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins - probe.SelectedDiffBegins)
					   << " publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications - probe.SelectedDiffPublications)
					   << " authority=" << truth(request.WorkbenchAuthority == probe.Authority)
					   << " generation=" << truth(request.Context.Generation == host->generation())
					   << " file=" << truth(request.File.File == probe.ExpectedFiles.front())
					   << " presentation_path=" << truth(request.File.PresentationPath == "review.cpp")
					   << " current_path=" << truth(request.File.CurrentPath == std::filesystem::path{ "review.cpp" })
					   << " previous_path=" << present(request.File.PreviousPath.has_value())
					   << " modified=" << truth(request.File.Change == application::workbench_file_change_kind::modified)
					   << " exact_comparison=" << truth(probe.Evidence.ExactComparison) << '\n';
				stream.flush();
				return fail();
			}
			const auto& groups   = comparison->current_result()->Alignment.Groups;
			const auto  selected = comparison->selection().has_value()
					&& comparison->selection()->Group.has_value()
				? std::ranges::find(
					  groups,
					  *comparison->selection()->Group,
					  &core::change_group::Id)
				: groups.end();
			auto*       action   = selected == groups.end()
					|| std::next(selected) != groups.end()
				? window->next_change_action()
				: window->previous_change_action();
			if (action == nullptr || !action->isEnabled()) {
				return fail();
			}

			probe.Selection        = comparison->selection();
			probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
			action->trigger();
			return advance(state_type::phase::awaiting_change_projection);
		}

		if (probe.Phase == state_type::phase::awaiting_change_projection) {
			if (implementation->WorkbenchChangeDispatches
				== probe.ChangeDispatches) {
				// [[BPR-0864]]. The change action's intent has not reached the
				// host.
				return wait("WORKBENCH_CHANGE_DISPATCH_STALLED", [&](QTextStream& aStream) {
					const auto computed = comparison == nullptr ? nullptr : comparison->current_result();
					aStream << "change_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchChangeDispatches)
							<< " expected_change_dispatches=" << static_cast<qulonglong>(probe.ChangeDispatches) + 1U
							<< " next_change_enabled=" << truth(window->next_change_action()->isEnabled())
							<< " previous_change_enabled=" << truth(window->previous_change_action()->isEnabled())
							<< " selection_moved=" << truth(comparison != nullptr && comparison->selection() != probe.Selection)
							<< " groups=" << static_cast<qulonglong>(computed == nullptr ? 0U : computed->Alignment.Groups.size());
				});
			}
			probe.Evidence.ChangeDispatch
				= implementation->WorkbenchChangeDispatches
					== probe.ChangeDispatches + 1U
				&& comparison != nullptr
				&& comparison->selection() != probe.Selection;
			if (!probe.Evidence.ChangeDispatch || window->files_drawer() == nullptr) {
				return fail();
			}

			const auto& groups = comparison->current_result()->Alignment.Groups;
			if (groups.size() < 2U) {
				return fail();
			}
			for (auto attempts = std::size_t{ 0U };
				 attempts < groups.size()
				 && (!comparison->selection().has_value()
					 || comparison->selection()->Group != groups.back().Id);
				 ++attempts) {
				if (!comparison->navigate_change(
						core::change_navigation_direction::next,
						core::change_navigation_policy::stop)) {
					return fail();
				}
			}
			if (!comparison->selection().has_value()
				|| comparison->selection()->Group != groups.back().Id) {
				return fail();
			}
			auto wrapCommands = core::workbench_command_delta{ };
			for (auto index = std::size_t{ 0U };
				 index < wrapCommands.Commands.size(); ++index) {
				wrapCommands.Commands[index] = core::workbench_command_projection{
					.Id             = static_cast<core::workbench_command_id>(index),
					.Enabled        = true,
					.DisabledReason = core::workbench_reason_code::none,
				};
			}
			wrapCommands.Policy = core::change_navigation_policy::wrap;
			window->publish(application::workbench_projection{
				.Context  = implementation->LastSelectedDiff->Request.Context,
				.Commands = wrapCommands,
			});
			probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
			window->next_change_action()->trigger();
			// [[BPR-1417]]. A press in Review is applied once the active tab's
			// exact view is on screen, and the selections made just above reach
			// the tab a lane turn later, so the press is not answered here.
			return advance(state_type::phase::awaiting_wrap_projection);
		}

		if (probe.Phase == state_type::phase::awaiting_wrap_projection) {
			if (implementation->WorkbenchChangeDispatches == probe.ChangeDispatches) {
				return wait("WORKBENCH_WRAP_DISPATCH_STALLED", [&](QTextStream& aStream) {
					aStream << "change_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchChangeDispatches)
							<< " expected_change_dispatches=" << static_cast<qulonglong>(probe.ChangeDispatches) + 1U
							<< " navigation_retained=" << truth(implementation->PendingReviewNavigation.has_value())
							<< " next_change_enabled=" << truth(window->next_change_action()->isEnabled());
				});
			}
			const auto computed = comparison->current_result();
			probe.Evidence.ChangeDispatch
				= implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 1U
				&& computed != nullptr && !computed->Alignment.Groups.empty()
				&& comparison->selection().has_value()
				&& comparison->selection()->Group == computed->Alignment.Groups.front().Id;
			if (!probe.Evidence.ChangeDispatch) {
				return fail();
			}

			// [[BPR-0860]]. The drag needs a files drawer with an edge. Since
			// [[BPR-0771]] this course names its pull request in the Home bar,
			// so the window opens on Overview -- [[BPR-0661]]'s default opening
			// modality -- and since [[BPR-0727]] Overview collapses the three
			// drawers in the core's own decision. A collapsed drawer with the
			// controls hidden takes no width and the handle beside it has
			// nothing to move, so the drag reported nothing and the course sat
			// in `awaiting_width_store` until the run budget ended. A reviewer
			// who wants the files drawer wider leaves Overview first; the
			// course states Review, the one modality whose files drawer is
			// expanded, and enters it through the shared step ([[BPR-0863]]).
			return advance(state_type::phase::awaiting_files_drawer);
		}

		if (probe.Phase == state_type::phase::awaiting_files_drawer) {
			// The step presses the strip's Review and answers once the window
			// and the host agree on it, the files drawer included, and the
			// store Review causes has landed.
			const auto modality = enter_course_modality(aApplication, probe.Modality);
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			// The drawer's room follows its state through the splitter. The
			// baselines below are taken only once it has some and nothing is
			// outstanding, so the width store the drag causes is the next one.
			// The step's own wait is bounded by the step; this one starts once
			// the step has answered.
			auto* const splitter = window->central_splitter();
			const auto  sizes    = splitter->sizes();
			if (sizes.size() != 4 || sizes[1] <= 0
				|| host->outstanding_requests() != 0U) {
				return wait("WORKBENCH_FILES_DRAWER_EXPAND_STALLED", widthFacts);
			}

			probe.WidthDispatches   = implementation->WorkbenchWidthDispatches;
			probe.PersistedLayouts  = host->persisted_layouts();
			probe.InitialFilesWidth = host->layout().FilesWidth;
			probe.WindowStatistics  = window->statistics();
			const auto target       = static_cast<int>(std::min<std::uint32_t>(
				probe.InitialFilesWidth + 41U,
				application::maximum_workbench_drawer_width));
			// [[BPR-0662]]. The reviewer's path: a drag of the handle
			// between the files drawer and the centre. A direct resize of
			// the panel is a squeeze the window answers with the persisted
			// widths and never reports.
			//
			// The handle is moved by the distance the reviewer would drag it
			// rather than to a position computed from the widths to its left:
			// a collapsed neighbour is hidden, and so is its handle, so those
			// widths are no longer what stands between the splitter's edge and
			// this one.
			splitter->move_handle(
				2, splitter->handle(2)->x() + (target - sizes[1]));
			probe.RequestedFilesWidth = target;
			return advance(state_type::phase::awaiting_width_store);
		}

		if (probe.Phase == state_type::phase::awaiting_width_store) {
			const auto statistics = window->statistics();
			if (implementation->WorkbenchWidthDispatches
					== probe.WidthDispatches
				|| host->persisted_layouts() == probe.PersistedLayouts) {
				// [[BPR-0860]]. The dispatch and the store are both effects
				// this stage does not control; the term says which of the two
				// never arrived.
				return wait(
					implementation->WorkbenchWidthDispatches
							== probe.WidthDispatches
						? "WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED"
						: "WORKBENCH_DRAWER_WIDTH_STORE_STALLED",
					widthFacts);
			}
			probe.Evidence.DrawerWidthDispatch
				= implementation->WorkbenchWidthDispatches
					== probe.WidthDispatches + 1U
				&& host->layout().FilesWidth != probe.InitialFilesWidth
				&& host->persisted_layouts() == probe.PersistedLayouts + 1U
				&& statistics.LayoutUpdates
					> probe.WindowStatistics.LayoutUpdates;
			if (!probe.Evidence.DrawerWidthDispatch) {
				return fail();
			}

			const auto& handoff  = *implementation->LastAcceptedHandoff;
			auto        mismatch = workbench_probe_invalidation(
				handoff,
				core::pull_request_id{ handoff.PullRequest.value() + 1U });
			const auto selected      = host->selected();
			const auto generation    = host->generation();
			const auto invalidations = implementation->WorkbenchInvalidations;
			implementation->invalidate(std::move(mismatch));
			probe.Evidence.MismatchPreserved
				= implementation->WorkbenchInvalidations == invalidations
				&& implementation->WorkbenchReview.active()
				&& host->selected() == selected
				&& host->generation() == generation;
			if (!probe.Evidence.MismatchPreserved) {
				return fail();
			}

			probe.Generation       = generation;
			probe.Invalidations    = invalidations;
			probe.WindowStatistics = statistics;
			implementation->invalidate(
				workbench_probe_invalidation(handoff, handoff.PullRequest));
			return advance(state_type::phase::awaiting_invalidation);
		}

		if (probe.Phase == state_type::phase::awaiting_invalidation) {
			const auto statistics = window->statistics();
			if (host->generation() == probe.Generation
				|| host->outstanding_requests() != 0U) {
				// [[BPR-0864]]. The exact invalidation supersedes the host's
				// generation and clears what the review held.
				return wait("WORKBENCH_INVALIDATION_STALLED", [&](QTextStream& aStream) {
					aStream << "generation=" << static_cast<qulonglong>(host->generation().value())
							<< " invalidated_generation=" << static_cast<qulonglong>(probe.Generation.value())
							<< " invalidations=" << static_cast<qulonglong>(implementation->WorkbenchInvalidations)
							<< " expected_invalidations=" << static_cast<qulonglong>(probe.Invalidations) + 1U
							<< " review_active=" << truth(implementation->WorkbenchReview.active())
							<< " actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " host_selected=" << present(host->selected().has_value())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< " publications=" << static_cast<qulonglong>(statistics.Publications)
							<< " invalidated_publications=" << static_cast<qulonglong>(probe.WindowStatistics.Publications);
				});
			}
			probe.Evidence.InvalidationSuperseded
				= implementation->WorkbenchInvalidations
					== probe.Invalidations + 1U
				&& !implementation->WorkbenchReview.active()
				&& host->actionable().empty()
				&& !host->selected().has_value()
				&& !host->state().Navigation.has_value()
				&& statistics.Publications > probe.WindowStatistics.Publications
				&& !window->previous_file_action()->isEnabled()
				&& !window->next_file_action()->isEnabled();
			if (!probe.Evidence.InvalidationSuperseded) {
				return fail();
			}

			probe.WindowStatistics = statistics;
			implementation->WorkbenchReview.make_unavailable(probe.Authority);
			host->observe(probe.Authority);
			return advance(state_type::phase::awaiting_failure);
		}

		if (probe.Phase == state_type::phase::awaiting_failure) {
			const auto statistics = window->statistics();
			if (statistics.Failures == probe.WindowStatistics.Failures
				|| host->outstanding_requests() != 0U) {
				// [[BPR-0864]]. An observation of an unavailable review fails,
				// and the failure reaches the window.
				return wait("WORKBENCH_FAILURE_DELIVERY_STALLED", [&](QTextStream& aStream) {
					aStream << "failures=" << static_cast<qulonglong>(statistics.Failures)
							<< " expected_failures=" << static_cast<qulonglong>(probe.WindowStatistics.Failures) + 1U
							<< " review_active=" << truth(implementation->WorkbenchReview.active())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< " publications=" << static_cast<qulonglong>(statistics.Publications)
							<< " observed_publications=" << static_cast<qulonglong>(probe.WindowStatistics.Publications);
				});
			}
			probe.Evidence.FailureDelivery
				= statistics.Failures == probe.WindowStatistics.Failures + 1U;
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			if (!probe.Evidence.FailureDelivery) {
				return fail();
			}

			const auto all = std::array{
				probe.Evidence.ExactAuthority,
				probe.Evidence.BoundedObservation,
				probe.Evidence.IntentDispatch,
				probe.Evidence.ProjectionDelivery,
				probe.Evidence.FileGesture,
				probe.Evidence.SelectedDiffRequest,
				probe.Evidence.ExactComparison,
				probe.Evidence.ChangeDispatch,
				probe.Evidence.DrawerWidthDispatch,
				probe.Evidence.SessionSettingsFallback,
				probe.Evidence.MismatchPreserved,
				probe.Evidence.InvalidationSuperseded,
				probe.Evidence.FailureDelivery,
				probe.Evidence.FixtureDiffHelperUnused,
			};
			if (!std::ranges::all_of(all, std::identity{ })) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::workbench_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::workbench_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->WorkbenchCompositionProbe.has_value()
			? implementation->WorkbenchCompositionProbe->Evidence
			: detail::workbench_composition_probe_evidence{ };
	}

	namespace {
		inline constexpr std::string_view uiforetw_base_oid
			= "8b70ee179fe55b705ee4e613ecbc89a0acebc1bc";
		inline constexpr std::string_view uiforetw_head_oid
			= "e40058be7d5fe319342dffb567ce30e42bc6f4de";
		inline constexpr std::string_view uiforetw_resource_path
			= "UIforETW/Resource.h";
		inline constexpr std::string_view uiforetw_resource_base_blob
			= "7da9aac464237ee5f1e2c9bc821598ff1f02b352";
		inline constexpr std::string_view uiforetw_resource_head_blob
			= "79a81a859f1b281c99e3c624b3263e618d7e1652";
		inline constexpr std::string_view uiforetw_support_path
			= "UIforETW/Support.cpp";
		inline constexpr std::string_view uiforetw_rc_path
			= "UIforETW/UIforETW.rc";
		inline constexpr std::string_view uiforetw_rc_base_blob
			= "b995e189fb58da146945e1fb22380cdf4a7f4e47";
		inline constexpr std::string_view uiforetw_rc_head_blob
			= "6335c1b2e14cd5e63142ce1dfc283fcf538fb134";

		[[nodiscard]] auto decoded_line(
			const core::decoded_text_document& aDocument,
			const std::uint32_t                aLine) -> std::string_view
		{
			if (aLine >= aDocument.Lines.size()) {
				return { };
			}
			const auto& span = aDocument.Lines[aLine].Content;
			if (span.End < span.Begin || span.End > aDocument.Bytes.size()) {
				return { };
			}
			return {
				reinterpret_cast<const char*>(aDocument.Bytes.data() + span.Begin),
				static_cast<std::size_t>(span.End - span.Begin),
			};
		}

		struct intraline_pair_location
		{
			std::uint32_t                        Left  = 0U;
			std::uint32_t                        Right = 0U;
			core::byte_span                      LeftSpan;
			core::byte_span                      RightSpan;
			core::alignment_segment_id           Segment;
			std::optional<core::change_group_id> Group;
		};

		[[nodiscard]] auto find_intraline_pair(
			const core::computed_file_diff& aResult,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment)
			-> std::optional<intraline_pair_location>
		{
			if (!aResult.LeftDocument.has_value()
				|| !aResult.RightDocument.has_value()) {
				return std::nullopt;
			}
			for (const auto& edit : aResult.Intraline) {
				if (edit.Left.empty() || edit.Right.empty()) {
					continue;
				}
				const auto pair = std::ranges::find_if(
					aResult.Alignment.Pairs,
					[&edit](const core::line_pair& aPair) {
						return aPair.Id == edit.Pair;
					});
				if (pair == aResult.Alignment.Pairs.end()) {
					continue;
				}
				const auto left           = decoded_line(*aResult.LeftDocument, pair->LeftLine);
				const auto right          = decoded_line(*aResult.RightDocument, pair->RightLine);
				const auto leftAt         = left.find(aLeftFragment);
				const auto rightAt        = right.find(aRightFragment);
				const auto insideFragment = [](const core::byte_span aSpan,
											   const std::size_t     aBegin,
											   const std::size_t     aExtent) {
					return aBegin != std::string_view::npos
						&& aSpan.Begin >= aBegin
						&& aSpan.End <= aBegin + aExtent;
				};
				if (insideFragment(edit.Left, leftAt, aLeftFragment.size())
					&& insideFragment(
						edit.Right, rightAt, aRightFragment.size())) {
					const auto segment = std::ranges::find_if(
						aResult.Alignment.Segments,
						[&pair](const core::alignment_segment& aSegment) {
							return aSegment.Id == pair->Segment;
						});
					if (segment == aResult.Alignment.Segments.end()) {
						continue;
					}
					return intraline_pair_location{
						.Left      = pair->LeftLine,
						.Right     = pair->RightLine,
						.LeftSpan  = edit.Left,
						.RightSpan = edit.Right,
						.Segment   = pair->Segment,
						.Group     = segment->Group,
					};
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto character_offset(
			const std::span<const std::uint8_t> aBytes,
			const std::uint64_t                 aOffset) -> int
		{
			const auto bounded = std::min<std::size_t>(
				static_cast<std::size_t>(aOffset), aBytes.size());
			return static_cast<int>(
				QString::fromUtf8(
					reinterpret_cast<const char*>(aBytes.data()),
					static_cast<qsizetype>(bounded))
					.size());
		}

		[[nodiscard]] auto intraline_selection_rect(
			QPlainTextEdit&                    aPane,
			const core::decoded_text_document& aDocument,
			const std::uint32_t                aLine,
			const core::byte_span              aSpan) -> std::optional<QRect>
		{
			if (aLine >= aDocument.Lines.size() || aSpan.empty()) {
				return std::nullopt;
			}
			const auto& decoded = aDocument.Lines[aLine];
			if (decoded.Content.End < decoded.Content.Begin
				|| decoded.Content.End > aDocument.Bytes.size()) {
				return std::nullopt;
			}
			const auto bytes = std::span<const std::uint8_t>{
				aDocument.Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(decoded.Content.End - decoded.Content.Begin)
			};
			const auto from  = character_offset(bytes, aSpan.Begin);
			const auto to    = character_offset(bytes, aSpan.End);
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid() || from >= to) {
				return std::nullopt;
			}
			auto start = QTextCursor{ block };
			start.setPosition(block.position() + from);
			auto finish = QTextCursor{ block };
			finish.setPosition(block.position() + to);
			return aPane.cursorRect(start).united(aPane.cursorRect(finish));
		}

		/// \brief Scrolls both panes until two intraline pairs are on screen.
		///
		/// \details [[BPR-0859]]. `aReport` names what stayed off screen. A
		/// caller that waits for the reveal reports only on the tick it gives
		/// up, so a run that settles on its third tick stays quiet.
		[[nodiscard]] auto reveal_intraline_pairs(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const intraline_pair_location&  aFirst,
			const intraline_pair_location&  aSecond,
			bool                            aReport = true) -> bool;

		[[nodiscard]] auto navigate_to_group(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const core::change_group_id&    aTarget) -> bool
		{
			const auto target = std::ranges::find(
				aResult.Alignment.Groups, aTarget, &core::change_group::Id);
			if (target == aResult.Alignment.Groups.end()) {
				return false;
			}
			const auto targetIndex = static_cast<std::size_t>(
				std::distance(aResult.Alignment.Groups.begin(), target));
			for (auto attempt = std::size_t{ 0U };
				 attempt <= aResult.Alignment.Groups.size(); ++attempt) {
				const auto selection = aWorkbench.selection();
				if (selection.has_value() && selection->Group == aTarget) {
					return true;
				}
				const auto current      = selection.has_value()
						&& selection->Group.has_value()
					? std::ranges::find(
						  aResult.Alignment.Groups,
						  *selection->Group,
						  &core::change_group::Id)
					: aResult.Alignment.Groups.end();
				const auto currentIndex = current != aResult.Alignment.Groups.end()
					? std::optional<std::size_t>{ static_cast<std::size_t>(
						  std::distance(aResult.Alignment.Groups.begin(), current)) }
					: std::nullopt;
				const auto direction    = !currentIndex.has_value()
						|| *currentIndex < targetIndex
					? core::change_navigation_direction::next
					: core::change_navigation_direction::previous;
				if (!aWorkbench.navigate_change(
						direction, core::change_navigation_policy::stop)) {
					return false;
				}
				QApplication::processEvents();
			}
			return false;
		}

		[[nodiscard]] auto reveal_intraline_pair(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment,
			const bool                      aReport = true) -> bool
		{
			const auto location = find_intraline_pair(
				aResult, aLeftFragment, aRightFragment);
			// [[BPR-0859]]. Each refusal names itself. A reveal that fails
			// leaves the role-paint evidence false without the role check ever
			// running, so an unnamed refusal here reads as a repaint defect.
			if (!location.has_value() || !location->Group.has_value()) {
				if (aReport) {
					bpr::app::detail::terminal_text_stream{ }
						<< "UIFORETW_REVEAL_PAIR_UNLOCATED:found="
						<< (location.has_value() ? 1 : 0) << '\n';
				}
				return false;
			}
			if (!navigate_to_group(aWorkbench, aResult, *location->Group)) {
				if (aReport) {
					bpr::app::detail::terminal_text_stream{ }
						<< "UIFORETW_REVEAL_PAIR_UNSELECTED:groups="
						<< static_cast<qulonglong>(aResult.Alignment.Groups.size())
						<< '\n';
				}
				return false;
			}
			return reveal_intraline_pairs(
				aWorkbench, aResult, *location, *location, aReport);
		}

		[[nodiscard]] auto reveal_intraline_pairs(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const intraline_pair_location&  aFirst,
			const intraline_pair_location&  aSecond,
			const bool                      aReport) -> bool
		{
			const auto reveal = [](
									QPlainTextEdit&                    aPane,
									const core::decoded_text_document& aDocument,
									const std::uint32_t                aFirstLine,
									const core::byte_span              aFirstSpan,
									const std::uint32_t                aSecondLine,
									const core::byte_span              aSecondSpan) {
				const auto first = intraline_selection_rect(
					aPane, aDocument, aFirstLine, aFirstSpan);
				const auto second = intraline_selection_rect(
					aPane, aDocument, aSecondLine, aSecondSpan);
				if (!first.has_value() || !second.has_value()) {
					return false;
				}
				auto* const horizontal   = aPane.horizontalScrollBar();
				const auto  documentLeft = std::min(first->left(), second->left())
					+ horizontal->value();
				const auto documentRight = std::max(first->right(), second->right())
					+ horizontal->value();
				const auto target = std::clamp(
					((documentLeft + documentRight) / 2)
						- (aPane.viewport()->width() / 2),
					horizontal->minimum(),
					horizontal->maximum());
				horizontal->setValue(target);
				return true;
			};
			if (aSecond.Left < aFirst.Left || aSecond.Right < aFirst.Right
				|| !reveal(
					*aWorkbench.left_pane(),
					*aResult.LeftDocument,
					aFirst.Left,
					aFirst.LeftSpan,
					aSecond.Left,
					aSecond.LeftSpan)
				|| !reveal(
					*aWorkbench.right_pane(),
					*aResult.RightDocument,
					aFirst.Right,
					aFirst.RightSpan,
					aSecond.Right,
					aSecond.RightSpan)) {
				// [[BPR-0859]]. The horizontal scroll could not be placed, so
				// no later visibility report would be meaningful.
				if (aReport) {
					bpr::app::detail::terminal_text_stream{ }
						<< "UIFORETW_REVEAL_PAIR_UNPLACED:first_left="
						<< static_cast<qulonglong>(aFirst.Left)
						<< " second_left=" << static_cast<qulonglong>(aSecond.Left)
						<< " first_right=" << static_cast<qulonglong>(aFirst.Right)
						<< " second_right="
						<< static_cast<qulonglong>(aSecond.Right) << '\n';
				}
				return false;
			}
			QApplication::processEvents();
			// [[BPR-0859]]. Every mark is weighed, not only the first, and an
			// offscreen one prints the rectangle it computed beside the
			// viewport it had to fit inside.
			const auto visible = [aReport](
									 QPlainTextEdit&                    aPane,
									 const char* const                  aName,
									 const core::decoded_text_document& aDocument,
									 const std::uint32_t                aLine,
									 const core::byte_span              aSpan) {
				const auto rect = intraline_selection_rect(
					aPane, aDocument, aLine, aSpan);
				const auto viewport
					= QRect{ QPoint{ 0, 0 }, aPane.viewport()->size() };
				const auto shown = rect.has_value() && !rect->isEmpty()
					&& viewport.contains(*rect, true);
				if (!shown && aReport) {
					const auto* const vertical = aPane.verticalScrollBar();
					bpr::app::detail::terminal_text_stream{ }
						<< "UIFORETW_REVEAL_PAIR_OFFSCREEN:" << aName
						<< " line=" << static_cast<qulonglong>(aLine)
						<< " placed=" << (rect.has_value() ? 1 : 0) << " rect="
						<< (rect.has_value()
								? QStringLiteral("%1,%2 %3x%4")
									  .arg(rect->left())
									  .arg(rect->top())
									  .arg(rect->width())
									  .arg(rect->height())
								: QStringLiteral("none"))
						<< " viewport=" << viewport.width() << 'x'
						<< viewport.height() << " vscroll=" << vertical->value()
						<< '/' << vertical->maximum()
						<< " top_block="
						<< aPane.cursorForPosition(QPoint{ 0, 0 }).blockNumber()
						<< " shown_pane=" << (aPane.isVisible() ? 1 : 0) << '\n';
				}
				return shown;
			};
			const auto firstLeft = visible(
				*aWorkbench.left_pane(),
				"left-first",
				*aResult.LeftDocument,
				aFirst.Left,
				aFirst.LeftSpan);
			const auto secondLeft = visible(
				*aWorkbench.left_pane(),
				"left-second",
				*aResult.LeftDocument,
				aSecond.Left,
				aSecond.LeftSpan);
			const auto firstRight = visible(
				*aWorkbench.right_pane(),
				"right-first",
				*aResult.RightDocument,
				aFirst.Right,
				aFirst.RightSpan);
			const auto secondRight = visible(
				*aWorkbench.right_pane(),
				"right-second",
				*aResult.RightDocument,
				aSecond.Right,
				aSecond.RightSpan);
			return firstLeft && secondLeft && firstRight && secondRight;
		}

		/// \brief The colours one intraline role can legitimately paint as.
		///
		/// \details BPR-0321 composites the row's selection wash into the
		/// emphasis, so one semantic role renders as its palette colour on an
		/// unselected row and as that colour under the wash on a selected one.
		/// Both are the role. A check that accepted only the bare colour would
		/// read a correctly washed mark as a missing one, which is exactly how
		/// this course failed when the wash first landed.
		[[nodiscard]] auto role_renderings(
			const QPlainTextEdit&          aPane,
			const QColor&                  aRole,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant) -> std::array<QColor, 3U>
		{
			const auto highlight = aPane.palette().color(QPalette::Highlight);
			const auto blended   = core::composited(
				core::appearance_color{ static_cast<std::uint8_t>(highlight.red()),
										static_cast<std::uint8_t>(highlight.green()),
										static_cast<std::uint8_t>(highlight.blue()),
										255U },
				core::appearance_color{ static_cast<std::uint8_t>(aRole.red()),
										static_cast<std::uint8_t>(aRole.green()),
										static_cast<std::uint8_t>(aRole.blue()),
										255U },
				core::selection_wash_alpha);
			// The third rendering is what a selected row shows: the band that
			// row wears, with the wash over it, strengthened in its own hue.
			auto derived = QColor{ };
			if (aRowBand.isValid()) {
				const auto washedBand = core::composited(
					core::appearance_color{
						static_cast<std::uint8_t>(highlight.red()),
						static_cast<std::uint8_t>(highlight.green()),
						static_cast<std::uint8_t>(highlight.blue()),
						255U },
					core::appearance_color{
						static_cast<std::uint8_t>(aRowBand.red()),
						static_cast<std::uint8_t>(aRowBand.green()),
						static_cast<std::uint8_t>(aRowBand.blue()),
						255U },
					core::selection_wash_alpha);
				const auto mark
					= core::derived_emphasis_fill(washedBand, aVariant);
				derived = QColor{ mark.Red, mark.Green, mark.Blue };
			}
			return { aRole,
					 QColor{ blended.Red, blended.Green, blended.Blue },
					 derived };
		}

		[[nodiscard]] auto role_paint_count(
			const QPlainTextEdit&          aPane,
			const QColor&                  aExpected,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant) -> std::size_t
		{
			// BPR-0323 marks the changed characters with a rule as well as a
			// fill, and drops the fill on a selected row because a tint there
			// read as a foreign colour. The pane already fixes which side a
			// span belongs to, so an underlined span is the role just as much
			// as a filled one; requiring the fill would read the new mark as
			// a missing one.
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			return static_cast<std::size_t>(std::ranges::count_if(
				aPane.extraSelections(),
				[&accepted](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& (std::ranges::contains(
								accepted,
								aSelection.format.background().color())
							|| aSelection.format.fontUnderline())
						&& aSelection.cursor.hasSelection();
				}));
		}

		/// \brief [[BPR-0859]]. How many intraline marks a stalled check names.
		///
		/// \details Enough to show the revealed pair and its neighbours on both
		/// sides, and short of one line per changed span on an all-changed
		/// screenful.
		inline constexpr std::size_t role_paint_report_marks = 12U;

		/// \brief [[BPR-0859]]. Ticks an intraline reveal may take to settle.
		///
		/// \details The reveal is the product's own scroll: selecting the change
		/// group asks the linked controller to bring the segment on screen, and
		/// the geometry that positions it is measured on the completion lane
		/// rather than inside the gesture. Asserting on the first tick read a
		/// scroll still in flight as a change that never came on screen, which
		/// is how this course failed. The automated run advances the probe about
		/// once every ten milliseconds, so this is roughly two and a half
		/// seconds: far longer than a measured reveal needs, far short of the
		/// forty-second run budget, and bounded so the stall names itself.
		inline constexpr std::size_t uiforetw_intraline_reveal_waits = 256U;

		/// \brief [[BPR-0859]]. Ticks both panes may take to show once Review
		/// is entered.
		///
		/// \details Entering Review is the shared step's, bounded and named
		/// there ([[BPR-0863]]). The panes are this course's own need beyond
		/// the comparison the step waits for, so they keep this bound and name
		/// their own stall rather than expiring the run budget.
		inline constexpr std::size_t uiforetw_review_panes_waits = 512U;

		/// \brief [[BPR-0859]]. Reports what one pane's role-paint check saw.
		///
		/// \details A capped wait that gives up must say what it saw, or its
		/// only evidence is that it did not see what it wanted: BPR-0859 spent
		/// two scouting passes on a stall that printed one word. The accepted
		/// renderings are named with their colour specs, because `QColor ==`
		/// compares the spec as well as the value, and every mark the pane
		/// actually holds is named beside them. A pane holding no marks at all
		/// is a pane this modality never composed, or a target outside the
		/// painted bounds; marks in an unaccepted colour are a repaint or an
		/// expectation defect. The two are indistinguishable without this.
		void report_role_paint(
			QTextStream&                   aStream,
			const QPlainTextEdit&          aPane,
			const char* const              aName,
			const QColor&                  aExpected,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant)
		{
			const auto describe = [](const QColor& aColor) {
				return aColor.isValid()
					? aColor.name(QColor::HexArgb) + QStringLiteral("/spec")
						+ QString::number(static_cast<int>(aColor.spec()))
					: QStringLiteral("invalid");
			};
			const auto accepted
				= role_renderings(aPane, aExpected, aRowBand, aVariant);
			const auto selections = aPane.extraSelections();
			const auto mark       = [](const QTextEdit::ExtraSelection& aSelection) {
				return !aSelection.format.boolProperty(
						   QTextFormat::FullWidthSelection)
					&& aSelection.cursor.hasSelection();
			};
			const auto marks = static_cast<std::size_t>(
				std::ranges::count_if(selections, mark));
			const auto height = aPane.viewport()->height();
			aStream << "UIFORETW_ROLE_PAINT_PANE:" << aName
					<< " marks=" << static_cast<qulonglong>(marks) << " matched="
					<< static_cast<qulonglong>(role_paint_count(
						   aPane, aExpected, aRowBand, aVariant))
					<< " selections=" << static_cast<qulonglong>(selections.size())
					<< " blocks=" << aPane.document()->blockCount() << " top="
					<< aPane.cursorForPosition(QPoint{ 0, 0 }).blockNumber()
					<< " bottom="
					<< aPane.cursorForPosition(QPoint{ 0, height }).blockNumber()
					<< " viewport=" << aPane.viewport()->width() << 'x' << height
					<< " shown=" << (aPane.isVisible() ? 1 : 0)
					<< " role=" << describe(accepted[0])
					<< " washed=" << describe(accepted[1])
					<< " derived=" << describe(accepted[2])
					<< " band=" << describe(aRowBand) << " highlight="
					<< describe(aPane.palette().color(QPalette::Highlight))
					<< '\n';
			auto reported = std::size_t{ 0U };
			for (const auto& selection : selections) {
				if (!mark(selection) || ++reported > role_paint_report_marks) {
					continue;
				}
				const auto background = selection.format.background().color();
				aStream << "UIFORETW_ROLE_PAINT_MARK:" << aName
						<< " block=" << selection.cursor.blockNumber()
						<< " from=" << selection.cursor.selectionStart()
						<< " to=" << selection.cursor.selectionEnd()
						<< " background=" << describe(background) << " brush="
						<< static_cast<int>(selection.format.background().style())
						<< " underline="
						<< (selection.format.fontUnderline() ? 1 : 0)
						<< " accepted="
						<< (std::ranges::contains(accepted, background) ? 1 : 0)
						<< '\n';
			}
		}

		/// \brief [[BPR-0859]]. Reports both panes' role paint at a stall.
		///
		/// \details The modality is named first. A course that samples panes the
		/// review modality on screen never composed reads a correctly painted
		/// comparison as an unpainted one, which is how BPR-0860, BPR-0823 and
		/// BPR-0858 failed in this same run.
		void report_intraline_roles(
			const qt::diff_workbench&     aWorkbench,
			const qt::appearance_profile& aProfile,
			const std::size_t             aMinimum,
			const char* const             aTerm)
		{
			auto stream = bpr::app::detail::terminal_text_stream{ };
			stream << aTerm << ":minimum=" << static_cast<qulonglong>(aMinimum)
				   << " one_pane=" << (aWorkbench.one_pane_enabled() ? 1 : 0)
				   << " variant=" << static_cast<int>(aProfile.variant()) << '\n';
			report_role_paint(
				stream,
				*aWorkbench.left_pane(),
				"left",
				aProfile.color(
					core::appearance_role::diff_intraline_removed_background),
				aProfile.color(core::appearance_role::diff_removed_background),
				aProfile.variant());
			report_role_paint(
				stream,
				*aWorkbench.right_pane(),
				"right",
				aProfile.color(
					core::appearance_role::diff_intraline_added_background),
				aProfile.color(core::appearance_role::diff_added_background),
				aProfile.variant());
		}

		/// \brief [[BPR-1036]]. The capture's layout floors, in thousandths of
		/// the window the comparison must take.
		///
		/// \details The height floors are the measured stacks'. [[BPR-1130]]:
		/// RFC-BPR-0052's Updates line folds, a new window starts folded, and
		/// open its timeline is one 34-pixel row with no caption, so the stack
		/// is taken both ways. [[BPR-1132]]: the comparison's header is the
		/// 28-pixel document tab strip, in place of the diff status row and its
		/// spacing. Measured on macOS, folded, the stack is the menu bar 20, the
		/// title row 30, the review bar 28, the tab strip 28 and the status bar
		/// 22: the comparison is 872. Open, the timeline's 34 sit under the bar:
		/// 838. On Linux [[BPR-1036]]'s stack (a 14-pixel status and 6 of
		/// spacing) gives 876 folded and 842 open, derived until the Linux
		/// station measures them. Windows has no menu bar row and keeps the
		/// owner's 850 budget ([[BPR-0246]]) for the open line, the tighter
		/// state, and so 884 folded, unmeasured until the Windows station runs
		/// this course. The folded stack is the open one less the timeline's
		/// row exactly, on every platform. Any further row that takes height
		/// fails and is named. The course script asserts the same floors.
		inline constexpr int uiforetw_capture_width_floor = 750;
	#if defined(__APPLE__)
		inline constexpr int uiforetw_capture_open_height_floor   = 838;
		inline constexpr int uiforetw_capture_folded_height_floor = 872;
	#elif defined(_WIN32)
		inline constexpr int uiforetw_capture_open_height_floor   = 850;
		inline constexpr int uiforetw_capture_folded_height_floor = 884;
	#else
		inline constexpr int uiforetw_capture_open_height_floor   = 842;
		inline constexpr int uiforetw_capture_folded_height_floor = 876;
	#endif

		/// \brief [[BPR-1036]]. Names what shares the capture window with the
		/// comparison when the comparison is under a layout floor.
		///
		/// \details The failure line used to give two ratios and no widgets, so
		/// what took the room was a guess. This prints the window and the rows
		/// the specification stacks around the comparison -- the menu bar, the
		/// strip, the epoch line and the status bar -- as `y+height`, or why
		/// one takes no room. Then, at every level from the comparison up to
		/// the window, each shown widget that sits above or below the
		/// comparison's branch (`stack=`) or beside it (`beside=`), so a row
		/// the specification does not name is named too. `unstacked` is the
		/// height neither the comparison nor a stacked row accounts for:
		/// layout margins and spacing.
		void report_capture_layout(
			QTextStream&                aStream,
			const qt::workbench_window& aWindow,
			const QWidget&              aComparison)
		{
			const auto inWindow = [&aWindow](const QWidget& aWidget) {
				return QRect{ aWidget.mapTo(&aWindow, QPoint{ 0, 0 }), aWidget.size() };
			};
			const auto row = [&inWindow](const QWidget* const aWidget) {
				if (aWidget == nullptr) {
					return QStringLiteral("absent");
				}
				if (!aWidget->isVisible()) {
					return QStringLiteral("hidden");
				}
				const auto rect = inWindow(*aWidget);
				return QStringLiteral("%1+%2").arg(rect.top()).arg(rect.height());
			};
			const auto name = [](const QWidget& aWidget) {
				return aWidget.objectName().isEmpty()
					? QString::fromLatin1(aWidget.metaObject()->className())
					: aWidget.objectName();
			};
			auto stacked = std::vector<std::pair<QRect, QString>>{ };
			auto beside  = std::vector<std::pair<QRect, QString>>{ };
			for (const auto* branch = &aComparison;
				 branch != &aWindow && branch->parentWidget() != nullptr;
				 branch = branch->parentWidget()) {
				const auto along = inWindow(*branch);
				for (const auto* const sibling :
					 branch->parentWidget()->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
					if (sibling == branch || sibling->isWindow() || !sibling->isVisible()) {
						continue;
					}
					const auto rect = inWindow(*sibling);
					const auto rows = rect.top() <= along.bottom() && along.top() <= rect.bottom();
					const auto columns
						= rect.left() <= along.right() && along.left() <= rect.right();
					if (columns && !rows) {
						stacked.emplace_back(rect, name(*sibling));
					} else if (rows && !columns) {
						beside.emplace_back(rect, name(*sibling));
					}
				}
			}
			std::ranges::sort(stacked, { }, [](const auto& aEntry) { return aEntry.first.top(); });
			std::ranges::sort(beside, { }, [](const auto& aEntry) { return aEntry.first.left(); });
			// Widgets that share a row, such as two buttons side by side, take
			// its height once.
			const auto comparison = inWindow(aComparison);
			auto       unstacked  = aWindow.height() - comparison.height();
			auto       covered    = std::numeric_limits<int>::min();
			auto       stackText  = QStringList{ };
			for (const auto& [rect, widget] : stacked) {
				unstacked -= std::max(0, rect.bottom() - std::max(rect.top() - 1, covered));
				covered = std::max(covered, rect.bottom());
				stackText.append(QStringLiteral("%1@%2+%3").arg(widget).arg(rect.top()).arg(rect.height()));
			}
			auto besideText = QStringList{ };
			for (const auto& [rect, widget] : beside) {
				besideText.append(QStringLiteral("%1@%2+%3").arg(widget).arg(rect.left()).arg(rect.width()));
			}
			aStream << " width_floor=" << uiforetw_capture_width_floor
					<< " folded_height_floor=" << uiforetw_capture_folded_height_floor
					<< " open_height_floor=" << uiforetw_capture_open_height_floor
					<< " window=" << aWindow.width() << 'x' << aWindow.height()
					<< " menu_bar=" << row(aWindow.menu_bar())
					<< " strip=" << row(aWindow.primary_toolbar())
					<< " review_bar=" << row(aWindow.review_bar())
					<< " epoch_line=" << row(aWindow.epoch_selectors())
					<< " tabs=" << row(aWindow.document_tabs())
					<< " comparison=" << comparison.left() << ',' << comparison.top() << ' '
					<< comparison.width() << 'x' << comparison.height()
					<< " status_bar=" << row(aWindow.statusBar())
					<< " stack=" << (stackText.isEmpty() ? QStringLiteral("none") : stackText.join(QLatin1Char{ ',' }))
					<< " unstacked=" << unstacked
					<< " beside=" << (besideText.isEmpty() ? QStringLiteral("none") : besideText.join(QLatin1Char{ ',' }));
		}

		[[nodiscard]] auto intraline_roles_visible(
			const qt::diff_workbench&     aWorkbench,
			const qt::appearance_profile& aProfile,
			const std::size_t             aMinimum) -> bool
		{
			return role_paint_count(
					   *aWorkbench.left_pane(),
					   aProfile.color(
						   core::appearance_role::diff_intraline_removed_background),
					   aProfile.color(core::appearance_role::diff_removed_background),
					   aProfile.variant())
				>= aMinimum
				&& role_paint_count(
					   *aWorkbench.right_pane(),
					   aProfile.color(
						   core::appearance_role::diff_intraline_added_background),
					   aProfile.color(core::appearance_role::diff_added_background),
					   aProfile.variant())
				>= aMinimum;
		}

		struct intraline_render_observation
		{
			intraline_pair_location Pair;
			QRect                   LeftRect;
			QRect                   RightRect;
			std::size_t             LeftRolePixels  = 0U;
			std::size_t             RightRolePixels = 0U;
		};

		[[nodiscard]] auto role_pixel_count(
			QPlainTextEdit&                aPane,
			const QRect&                   aRect,
			const QColor&                  aExpected,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant) -> std::size_t
		{
			const auto image = aPane.viewport()->grab(aRect).toImage().convertToFormat(
				QImage::Format_ARGB32);
			if (image.isNull() || !aExpected.isValid()) {
				return 0U;
			}
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			auto       count    = std::size_t{ 0U };
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto pixel = image.pixel(x, y);
					if (std::ranges::any_of(
							accepted,
							[pixel](const QColor& aCandidate) {
								return aCandidate.isValid()
									&& aCandidate.rgba() == pixel;
							})) {
						++count;
					}
				}
			}
			return count;
		}

		/// \brief Records the actual normal and disabled pixels of one toolbar icon.
		struct navigation_icon_contrast_observation
		{
			QColor       Background;
			QColor       NormalForeground;
			QColor       DisabledForeground;
			std::size_t  NormalForegroundPixels     = 0U;
			std::size_t  DisabledForegroundPixels   = 0U;
			std::int64_t NormalContrastHundredths   = 0;
			std::int64_t DisabledContrastHundredths = 0;
			bool         StatesDistinct             = false;
		};

		[[nodiscard]] auto exact_icon_foreground_pixels(
			const QIcon&      aIcon,
			const QSize&      aSize,
			const qreal       aScale,
			const QIcon::Mode aMode,
			const QColor&     aExpected) -> std::size_t
		{
			const auto image = aIcon.pixmap(aSize, aScale, aMode, QIcon::Off)
								   .toImage()
								   .convertToFormat(QImage::Format_ARGB32);
			if (image.isNull() || !aExpected.isValid()) {
				return 0U;
			}
			constexpr auto rgbComponentMask = QRgb{ 0x00FF'FFFFU };
			const auto     expected         = aExpected.rgb() & rgbComponentMask;
			auto           count            = std::size_t{ 0U };
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto pixel = image.pixel(x, y);
					if (qAlpha(pixel) >= 224
						&& (pixel & rgbComponentMask) == expected) {
						++count;
					}
				}
			}
			return count;
		}

		[[nodiscard]] auto observe_navigation_icon_contrast(
			const QToolButton& aButton)
			-> navigation_icon_contrast_observation
		{
			const auto background = aButton.palette().color(
				QPalette::Active, QPalette::Button);
			const auto normal = aButton.palette().color(
				QPalette::Active, QPalette::ButtonText);
			const auto disabled = aButton.palette().color(
				QPalette::Disabled, QPalette::ButtonText);
			const auto size          = aButton.iconSize().expandedTo(QSize{ 16, 16 });
			const auto scale         = aButton.devicePixelRatioF();
			const auto normalImage   = aButton.icon()
										   .pixmap(size, scale, QIcon::Normal, QIcon::Off)
										   .toImage();
			const auto disabledImage = aButton.icon()
										   .pixmap(size, scale, QIcon::Disabled, QIcon::Off)
										   .toImage();
			return {
				.Background             = background,
				.NormalForeground       = normal,
				.DisabledForeground     = disabled,
				.NormalForegroundPixels = exact_icon_foreground_pixels(
					aButton.icon(), size, scale, QIcon::Normal, normal),
				.DisabledForegroundPixels = exact_icon_foreground_pixels(
					aButton.icon(), size, scale, QIcon::Disabled, disabled),
				.NormalContrastHundredths   = static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(normal)))),
				.DisabledContrastHundredths = static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(disabled)))),
				.StatesDistinct             = !normalImage.isNull()
					&& !disabledImage.isNull() && normalImage != disabledImage,
			};
		}

		[[nodiscard]] auto exact_role_selection_present(
			const QPlainTextEdit&              aPane,
			const core::decoded_text_document& aDocument,
			const std::uint32_t                aLine,
			const core::byte_span              aSpan,
			const QColor&                      aExpected,
			const QColor&                      aRowBand,
			const core::appearance_variant     aVariant) -> bool
		{
			if (aLine >= aDocument.Lines.size()) {
				return false;
			}
			const auto& decoded = aDocument.Lines[aLine];
			if (decoded.Content.End < decoded.Content.Begin
				|| decoded.Content.End > aDocument.Bytes.size()) {
				return false;
			}
			const auto bytes = std::span<const std::uint8_t>{
				aDocument.Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(decoded.Content.End - decoded.Content.Begin)
			};
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid()) {
				return false;
			}
			const auto from     = block.position() + character_offset(bytes, aSpan.Begin);
			const auto to       = block.position() + character_offset(bytes, aSpan.End);
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			return std::ranges::any_of(
				aPane.extraSelections(),
				[&accepted, from, to](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& (std::ranges::contains(
								accepted,
								aSelection.format.background().color())
							|| aSelection.format.fontUnderline())
						&& aSelection.cursor.selectionStart() == from
						&& aSelection.cursor.selectionEnd() == to;
				});
		}

		[[nodiscard]] auto observe_intraline_render(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const qt::appearance_profile&   aProfile,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment)
			-> std::optional<intraline_render_observation>
		{
			const auto pair = find_intraline_pair(
				aResult, aLeftFragment, aRightFragment);
			if (!pair.has_value()) {
				return std::nullopt;
			}
			auto* const left     = aWorkbench.left_pane();
			auto* const right    = aWorkbench.right_pane();
			const auto  leftRect = intraline_selection_rect(
				*left, *aResult.LeftDocument, pair->Left, pair->LeftSpan);
			const auto rightRect = intraline_selection_rect(
				*right, *aResult.RightDocument, pair->Right, pair->RightSpan);
			if (!leftRect.has_value() || !rightRect.has_value()
				|| leftRect->isEmpty() || rightRect->isEmpty()
				|| !QRect{ QPoint{ 0, 0 }, left->viewport()->size() }.contains(
					*leftRect, true)
				|| !QRect{ QPoint{ 0, 0 }, right->viewport()->size() }.contains(
					*rightRect, true)) {
				return std::nullopt;
			}
			const auto removed = aProfile.color(
				core::appearance_role::diff_intraline_removed_background);
			const auto added = aProfile.color(
				core::appearance_role::diff_intraline_added_background);
			if (!exact_role_selection_present(
					*left,
					*aResult.LeftDocument,
					pair->Left,
					pair->LeftSpan,
					removed,
					aProfile.color(core::appearance_role::diff_removed_background),
					aProfile.variant())
				|| !exact_role_selection_present(
					*right,
					*aResult.RightDocument,
					pair->Right,
					pair->RightSpan,
					added,
					aProfile.color(core::appearance_role::diff_added_background),
					aProfile.variant())) {
				return std::nullopt;
			}
			return intraline_render_observation{
				.Pair           = *pair,
				.LeftRect       = *leftRect,
				.RightRect      = *rightRect,
				.LeftRolePixels = role_pixel_count(
					*left,
					*leftRect,
					removed,
					aProfile.color(core::appearance_role::diff_removed_background),
					aProfile.variant()),
				.RightRolePixels = role_pixel_count(
					*right,
					*rightRect,
					added,
					aProfile.color(core::appearance_role::diff_added_background),
					aProfile.variant()),
			};
		}

		struct connector_pixel_observation
		{
			QColor       Background;
			QColor       Dominant;
			std::size_t  SampledPixels            = 0U;
			std::size_t  ContrastPixels           = 0U;
			std::size_t  DominantPixels           = 0U;
			std::int64_t MedianContrastHundredths = 0;
		};

		struct connector_render_observation
		{
			QRectF                      Bounds;
			QPointF                     Start;
			QPointF                     End;
			connector_pixel_observation Pixels;
			bool                        Selected      = false;
			bool                        LeftEndpoint  = false;
			bool                        RightEndpoint = false;
			bool                        LeftOverlap   = false;
			bool                        RightOverlap  = false;
		};

		[[nodiscard]] auto dominant_connector_pixel(std::vector<QRgb> aPixels)
			-> std::pair<QColor, std::size_t>
		{
			if (aPixels.empty()) {
				return { QColor{ }, 0U };
			}
			std::ranges::sort(aPixels);
			auto bestPixel = aPixels.front();
			auto bestCount = std::size_t{ 0U };
			for (auto first = aPixels.begin(); first != aPixels.end();) {
				const auto last = std::ranges::find_if(
					first, aPixels.end(), [first](const QRgb aPixel) {
						return aPixel != *first;
					});
				const auto count = static_cast<std::size_t>(
					std::distance(first, last));
				if (count > bestCount) {
					bestPixel = *first;
					bestCount = count;
				}
				first = last;
			}
			return { QColor::fromRgba(bestPixel), bestCount };
		}

		[[nodiscard]] auto observe_connector_pixels(
			qt::diff_link_map&           aMap,
			const qt::visible_diff_link& aTarget)
			-> connector_pixel_observation
		{
			const auto image = aMap.grab().toImage().convertToFormat(
				QImage::Format_ARGB32);
			const auto scale = image.devicePixelRatio();
			if (image.isNull() || scale <= 0.0) {
				return { };
			}

			const auto links   = aMap.visible_links();
			auto       stroker = QPainterPathStroker{ };
			stroker.setWidth(3.0);
			const auto targetMask
				= aTarget.Path.united(stroker.createStroke(aTarget.Path));
			auto backgroundPixels = std::vector<QRgb>{ };
			auto targetPixels     = std::vector<QRgb>{ };
			backgroundPixels.reserve(
				static_cast<std::size_t>(image.sizeInBytes() / 4));
			targetPixels.reserve(backgroundPixels.capacity() / 4U);
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto point = QPointF{
						(static_cast<qreal>(x) + 0.5) / scale,
						(static_cast<qreal>(y) + 0.5) / scale,
					};
					const auto inAny = std::ranges::any_of(
						links,
						[point](const qt::visible_diff_link& aLink) {
							return aLink.Path.contains(point);
						});
					if (targetMask.contains(point)) {
						const auto overlapsAnother = std::ranges::any_of(
							links,
							[&aTarget, point](const qt::visible_diff_link& aLink) {
								return &aLink != &aTarget
									&& aLink.Path.contains(point);
							});
						if (!overlapsAnother) {
							targetPixels.push_back(image.pixel(x, y));
						}
					} else if (!inAny) {
						backgroundPixels.push_back(image.pixel(x, y));
					}
				}
			}

			const auto [background, backgroundCount]
				= dominant_connector_pixel(std::move(backgroundPixels));
			const auto [dominant, dominantCount]
				= dominant_connector_pixel(targetPixels);
			if (!background.isValid() || backgroundCount == 0U
				|| !dominant.isValid() || targetPixels.empty()) {
				return { };
			}

			auto contrasts = std::vector<std::int64_t>{ };
			contrasts.reserve(targetPixels.size());
			for (const auto pixel : targetPixels) {
				contrasts.push_back(static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(QColor::fromRgba(pixel))))));
			}
			std::ranges::sort(contrasts);
			return {
				.Background     = background,
				.Dominant       = dominant,
				.SampledPixels  = targetPixels.size(),
				.ContrastPixels = static_cast<std::size_t>(
					std::ranges::count_if(
						contrasts,
						[](const auto aContrast) { return aContrast >= 300; })),
				.DominantPixels           = dominantCount,
				.MedianContrastHundredths = contrasts[contrasts.size() / 2U],
			};
		}

		[[nodiscard]] auto observe_connector_render(
			qt::diff_workbench&                 aWorkbench,
			const intraline_render_observation& aTarget)
			-> std::optional<connector_render_observation>
		{
			auto* const map   = aWorkbench.link_map();
			const auto  found = std::ranges::find_if(
				map->visible_links(),
				[&aTarget](const qt::visible_diff_link& aLink) {
					return aLink.Identity.Segment == aTarget.Pair.Segment;
				});
			if (found == map->visible_links().end()) {
				return std::nullopt;
			}
			const auto mapTop   = map->mapToGlobal(QPoint{ 0, 0 }).y();
			const auto relative = [mapTop](
									  QPlainTextEdit& aPane,
									  const QRect&    aRect) {
				const auto top = aPane.viewport()->mapToGlobal(aRect.topLeft()).y()
					- mapTop;
				return std::pair{ static_cast<qreal>(top),
								  static_cast<qreal>(top + aRect.height()) };
			};
			const auto [leftTop, leftBottom] = relative(
				*aWorkbench.left_pane(), aTarget.LeftRect);
			const auto [rightTop, rightBottom] = relative(
				*aWorkbench.right_pane(), aTarget.RightRect);
			const auto overlaps = [](const qreal aFirstTop,
									 const qreal aFirstBottom,
									 const qreal aSecondTop,
									 const qreal aSecondBottom) {
				return std::max(aFirstTop, aSecondTop)
					<= std::min(aFirstBottom, aSecondBottom);
			};
			const auto startElement = found->Path.elementAt(0);
			const auto endElement   = found->Path.elementAt(3);
			const auto start        = QPointF{ startElement.x, startElement.y };
			const auto end          = QPointF{ endElement.x, endElement.y };
			const auto selection    = map->selected();
			const auto isSelected   = selection.has_value()
				&& (selection->Group.has_value()
							&& found->Identity.Group.has_value()
						? selection->Group == found->Identity.Group
						: selection->Segment == found->Identity.Segment);
			return connector_render_observation{
				.Bounds        = found->Path.boundingRect(),
				.Start         = start,
				.End           = end,
				.Pixels        = observe_connector_pixels(*map, *found),
				.Selected      = isSelected,
				.LeftEndpoint  = std::abs(start.x()) <= 0.5,
				.RightEndpoint = std::abs(end.x() - map->width()) <= 0.5,
				.LeftOverlap   = overlaps(
					found->LeftTop, found->LeftBottom, leftTop, leftBottom),
				.RightOverlap = overlaps(
					found->RightTop, found->RightBottom, rightTop, rightBottom),
			};
		}

		// Capture facts own every value, including strings returned by Qt.
		struct capture_json_options : glz::opts
		{
			// Glaze reads its options by these exact member names.
			// NOLINTNEXTLINE(readability-identifier-naming)
			std::uint8_t indentation_width = 4U;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool escape_control_characters = true;
			constexpr capture_json_options() { prettify = true; }
		};

		template <typename... Values>
		[[nodiscard]] auto capture_object(Values... aValues)
		{
			return glz::obj_copy<Values...>{ .value = glz::make_tuple(std::move(aValues)...) };
		}

		[[nodiscard]] auto rectangle_json(const QRect& aRect)
		{
			return capture_object(
				"x",
				aRect.x(),
				"y",
				aRect.y(),
				"width",
				aRect.width(),
				"height",
				aRect.height());
		}

		[[nodiscard]] auto rectangle_json(const QRectF& aRect)
		{
			return capture_object(
				"x",
				aRect.x(),
				"y",
				aRect.y(),
				"width",
				aRect.width(),
				"height",
				aRect.height());
		}

		[[nodiscard]] auto point_json(const QPointF& aPoint)
		{
			return capture_object(
				"x",
				aPoint.x(),
				"y",
				aPoint.y());
		}

		[[nodiscard]] auto drawer_row(
			const qt::workbench_window& aWindow,
			const std::string_view      aPath) -> QModelIndex
		{
			if (aWindow.files_drawer() == nullptr
				|| aWindow.files_drawer()->contents_view() == nullptr
				|| aWindow.files_drawer()->contents_view()->model() == nullptr) {
				return { };
			}
			const auto* const model    = aWindow.files_drawer()->contents_view()->model();
			const auto        expected = QString::fromUtf8(
				aPath.data(), static_cast<qsizetype>(aPath.size()));
			for (auto row = 0; row < model->rowCount(); ++row) {
				const auto index = model->index(row, 0);
				if (index.data(Qt::DisplayRole).toString() == expected) {
					return index;
				}
			}
			return { };
		}

		[[nodiscard]] auto selected_diff_publication_matches(
			const auto&            aImplementation,
			const std::string_view aPath,
			const std::string_view aBaseBlob,
			const std::string_view aHeadBlob) -> bool
		{
			if (!aImplementation.LastSelectedDiff.has_value()) {
				return false;
			}
			const auto  left        = core::parse_git_blob_oid(aBaseBlob);
			const auto  right       = core::parse_git_blob_oid(aHeadBlob);
			const auto& publication = *aImplementation.LastSelectedDiff;
			return left.has_value() && right.has_value()
				&& publication.Request.File.PresentationPath == aPath
				&& publication.Request.File.CurrentPath
				== std::filesystem::path{ aPath }
			&& publication.Request.File.File == core::file_identity_of(aPath)
				&& publication.Key.LeftBlob == left
				&& publication.Key.RightBlob == right
				&& publication.Key.File == core::file_identity_of(aPath);
		}

		[[nodiscard]] auto selected_diff_matches(
			const auto&            aImplementation,
			const std::string_view aPath,
			const std::string_view aBaseBlob,
			const std::string_view aHeadBlob) -> bool
		{
			return selected_diff_publication_matches(
					   aImplementation, aPath, aBaseBlob, aHeadBlob)
				&& aImplementation.Window != nullptr
				&& aImplementation.Window->comparison_surface() != nullptr
				&& aImplementation.Window->comparison_surface()->current_result()
				!= nullptr
				&& aImplementation.Window->comparison_surface()
					   ->current_result()
					   ->Alignment.Key
				== aImplementation.LastSelectedDiff->Key;
		}

		[[nodiscard]] auto appearance_mode_name(
			const core::appearance_mode aMode) -> QString
		{
			switch (aMode) {
			case core::appearance_mode::system:
				return QStringLiteral("system");
			case core::appearance_mode::light:
				return QStringLiteral("light");
			case core::appearance_mode::dark:
				return QStringLiteral("dark");
			case core::appearance_mode::high_contrast_system:
				return QStringLiteral("high-contrast-system");
			case core::appearance_mode::high_contrast_light:
				return QStringLiteral("high-contrast-light");
			case core::appearance_mode::high_contrast_dark:
				return QStringLiteral("high-contrast-dark");
			}
			return QStringLiteral("unknown");
		}
	} // namespace

	auto detail::github_application_fixture_access::
		exercise_native_appearance_selected_diff(
			github_application& aApplication)
			-> detail::native_appearance_selected_diff_probe_result
	{
		using result               = detail::native_appearance_selected_diff_probe_result;
		using reason               = detail::native_appearance_selected_diff_reason;
		using state_type           = native_appearance_selected_diff_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::refused;
		}
		if (!implementation->NativeAppearanceSelectedDiffProbe.has_value()) {
			implementation->NativeAppearanceSelectedDiffProbe.emplace();
		}
		auto&      probe   = *implementation->NativeAppearanceSelectedDiffProbe;
		const auto pending = [&probe](const reason aReason) {
			probe.Evidence.Reason = aReason;
			return result::pending;
		};
		const auto refuse = [&probe](const reason aReason) {
			probe.Evidence.Reason = aReason;
			probe.Phase           = state_type::phase::refused;
			return result::refused;
		};

		probe.Evidence.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U;
		if (!probe.Evidence.FixtureDiffHelperUnused) {
			return refuse(reason::fixture_diff_helper_invoked);
		}
		if (probe.Phase == state_type::phase::refused) {
			return result::refused;
		}
		if (probe.Phase == state_type::phase::ready) {
			return result::ready;
		}
		if (implementation->Fixture == nullptr
			|| implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return refuse(reason::composition_unavailable);
		}

		auto* const host      = implementation->WorkbenchHost.get();
		auto* const window    = implementation->Window.get();
		auto* const workbench = window->comparison_surface();
		auto* const filesView = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		const auto& fixture   = implementation->Fixture->evidence();

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| !host->state().Navigation.has_value()) {
				return pending(reason::awaiting_workbench_observation);
			}
			if (snapshot->Files.size() != 1U
				|| snapshot->Files.front().Path != "review.cpp"
				|| filesView == nullptr || filesView->model() == nullptr
				|| filesView->model()->rowCount() != 1) {
				return refuse(reason::file_projection_mismatch);
			}

			const auto& handoff     = *implementation->LastAcceptedHandoff;
			const auto& epoch       = implementation->EpochHost->state();
			probe.Authority         = workbench_authority(handoff);
			probe.File              = core::file_identity_of("review.cpp");
			const auto context      = implementation->EpochHost->context(probe.Authority);
			const auto expectedBase = core::parse_git_commit_oid(fixture.BaseOid);
			const auto expectedHead = core::parse_git_commit_oid(fixture.HeadOneOid);
			const auto expectedLeftBlob
				= core::parse_git_blob_oid(fixture.BaseBlobOid);
			const auto expectedRightBlob
				= core::parse_git_blob_oid(fixture.HeadOneBlobOid);
			const auto expectedOptions = core::diff_options{ };
			const auto expectedDigest  = core::options_digest(expectedOptions);

			probe.Evidence.ExactAuthority
				= application::valid(probe.Authority)
				&& implementation->WorkbenchReview.exact(handoff)
				&& context.has_value() && epoch.Authority.has_value()
				&& context->ApplicationSession == handoff.ApplicationSession
				&& context->Authority == *epoch.Authority
				&& probe.Authority == workbench_authority(handoff);
			probe.Evidence.ExactRevisions
				= expectedBase.has_value() && expectedHead.has_value()
				&& epoch.AcceptedRevision.has_value()
				&& epoch.AcceptedComparison.has_value()
				&& epoch.AcceptedRevision->Base == *expectedBase
				&& epoch.AcceptedRevision->Head == *expectedHead
				&& epoch.AcceptedComparison->Left.Commit == *expectedBase
				&& epoch.AcceptedComparison->Right.Commit == *expectedHead
				&& context.has_value()
				&& context->Comparison == *epoch.AcceptedComparison;
			const auto actionable    = host->actionable();
			probe.Evidence.ExactFile = probe.File.valid()
				&& actionable.size() == 1U && actionable.front() == probe.File;
			probe.Evidence.ExactBlobs
				= expectedLeftBlob.has_value() && expectedRightBlob.has_value();
			probe.Evidence.ExactOptions
				= context.has_value() && expectedDigest.has_value()
				&& context->Options == expectedOptions
				&& context->OptionsDigest == expectedDigest
				&& context->Algorithm
					== core::supported_diff_algorithm_revision;
			if (!probe.Evidence.ExactAuthority) {
				return refuse(reason::fixture_authority_mismatch);
			}
			if (!probe.Evidence.ExactRevisions || !probe.Evidence.ExactBlobs
				|| !probe.Evidence.ExactOptions) {
				return refuse(reason::fixture_material_mismatch);
			}
			if (!probe.Evidence.ExactFile) {
				return refuse(reason::file_projection_mismatch);
			}

			probe.IntentDispatches   = implementation->WorkbenchIntentDispatches;
			probe.WindowStatistics   = window->statistics();
			probe.SelectedDiffBegins = implementation->SelectedDiffBegins;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			filesView->setCurrentIndex(filesView->model()->index(0, 0));
			probe.Phase = state_type::phase::awaiting_selection_projection;
			return pending(reason::awaiting_selection_projection);
		}

		if (probe.Phase
			== state_type::phase::awaiting_selection_projection) {
			const auto statistics = window->statistics();
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return refuse(reason::selected_diff_failed);
			}
			if (implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches) {
				if (statistics.FileSelections
					== probe.WindowStatistics.FileSelections + 1U) {
					probe.Evidence.SelectionGesture = true;
					return refuse(reason::selection_wire_unavailable);
				}
				return pending(reason::awaiting_selection_projection);
			}
			probe.Evidence.SelectionGesture
				= statistics.FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			probe.Evidence.SelectionIntent
				= implementation->WorkbenchIntentDispatches
					== probe.IntentDispatches + 1U
				&& host->selected() == std::optional{ probe.File };
			if (!probe.Evidence.SelectionGesture
				|| !probe.Evidence.SelectionIntent) {
				return refuse(reason::file_projection_mismatch);
			}
			probe.Phase = state_type::phase::awaiting_diff;
			return pending(reason::awaiting_selected_diff_publication);
		}

		if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
			return refuse(reason::selected_diff_failed);
		}
		if (implementation->SelectedDiffPublications
			== probe.SelectedDiffPublications) {
			return pending(reason::awaiting_selected_diff_publication);
		}
		if (implementation->SelectedDiffBegins != probe.SelectedDiffBegins + 1U
			|| implementation->SelectedDiffPublications
				!= probe.SelectedDiffPublications + 1U
			|| !implementation->LastSelectedDiff.has_value()) {
			return refuse(reason::selected_diff_mismatch);
		}

		const auto& publication = *implementation->LastSelectedDiff;
		const auto& request     = publication.Request;
		const auto& key         = publication.Key;
		const auto& epoch       = implementation->EpochHost->state();
		const auto  expectedLeftBlob
			= core::parse_git_blob_oid(fixture.BaseBlobOid);
		const auto expectedRightBlob
			= core::parse_git_blob_oid(fixture.HeadOneBlobOid);
		const auto expectedDigest = core::options_digest(core::diff_options{ });
		probe.Evidence.ExactAuthority
			= probe.Evidence.ExactAuthority
			&& request.WorkbenchAuthority == probe.Authority
			&& epoch.Authority.has_value() && key.Authority == *epoch.Authority
			&& key.Repository == epoch.Authority->Repository
			&& key.PullRequest == epoch.Authority->PullRequest;
		probe.Evidence.ExactRevisions
			= probe.Evidence.ExactRevisions
			&& epoch.AcceptedComparison.has_value()
			&& key.LeftAnchor == epoch.AcceptedComparison->Left.Identity
			&& key.RightAnchor == epoch.AcceptedComparison->Right.Identity;
		probe.Evidence.ExactFile = probe.Evidence.ExactFile
			&& request.Context.Generation == host->generation()
			&& request.File.File == probe.File
			&& request.File.PresentationPath == "review.cpp"
			&& request.File.CurrentPath == std::filesystem::path{ "review.cpp" }
			&& !request.File.PreviousPath.has_value()
			&& request.File.Change
				== application::workbench_file_change_kind::modified
			&& key.File == probe.File;
		probe.Evidence.ExactBlobs = probe.Evidence.ExactBlobs
			&& expectedLeftBlob.has_value() && expectedRightBlob.has_value()
			&& key.LeftBlob == *expectedLeftBlob
			&& key.RightBlob == *expectedRightBlob;
		probe.Evidence.ExactOptions = probe.Evidence.ExactOptions
			&& expectedDigest.has_value()
			&& request.Diff.Options == core::diff_options{ }
			&& request.Diff.OptionsDigest == *expectedDigest
			&& key.Options == *expectedDigest
			&& key.Algorithm == core::supported_diff_algorithm_revision;
		if (!probe.Evidence.ExactAuthority || !probe.Evidence.ExactRevisions
			|| !probe.Evidence.ExactFile || !probe.Evidence.ExactBlobs
			|| !probe.Evidence.ExactOptions) {
			return refuse(reason::selected_diff_mismatch);
		}
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !workbench->selection().has_value()) {
			return pending(reason::awaiting_diff_presentation);
		}
		probe.Evidence.PresentationReady
			= workbench->current_result()->Alignment.Key == key
			&& workbench->selection()->Comparison
				== workbench->current_result()->Alignment.Digest
			&& workbench->status_text().startsWith(QStringLiteral("Diff ready"));
		if (!probe.Evidence.PresentationReady) {
			return refuse(reason::selected_diff_mismatch);
		}

		probe.Evidence.Reason = reason::none;
		probe.Phase           = state_type::phase::ready;
		return result::ready;
	}

	auto detail::github_application_fixture_access::
		native_appearance_selected_diff_evidence(
			const github_application& aApplication) noexcept
		-> detail::native_appearance_selected_diff_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return detail::native_appearance_selected_diff_probe_evidence{
				.Reason = detail::native_appearance_selected_diff_reason::
					composition_unavailable,
			};
		}
		if (implementation->NativeAppearanceSelectedDiffProbe.has_value()) {
			return implementation->NativeAppearanceSelectedDiffProbe->Evidence;
		}
		return detail::native_appearance_selected_diff_probe_evidence{
			.Reason = detail::native_appearance_selected_diff_reason::
				awaiting_workbench_observation,
			.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U,
		};
	}

	auto detail::github_application_fixture_access::exercise_uiforetw_selected_diff(
		github_application& aApplication)
		-> detail::uiforetw_selected_diff_probe_result
	{
		using result               = detail::uiforetw_selected_diff_probe_result;
		using state_type           = uiforetw_selected_diff_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->Host == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->Window == nullptr
			|| implementation->AppearanceProfile == nullptr) {
			return result::failed;
		}
		if (!implementation->UIforEtwSelectedDiffProbe.has_value()) {
			implementation->UIforEtwSelectedDiffProbe.emplace();
		}
		auto&      probe = *implementation->UIforEtwSelectedDiffProbe;
		const auto fail  = [&probe, implementation] {
			const auto* const surface = implementation->Window->comparison_surface();
			bpr::app::detail::terminal_text_stream{ }
				<< "UIFORETW_PROBE_FAILURE:phase=" << static_cast<int>(probe.Phase)
				<< " waits=" << static_cast<qulonglong>(probe.Waits)
				<< " reveal_waits=" << static_cast<qulonglong>(probe.RevealWaits)
				<< " next=" << probe.Evidence.NextChangeNavigation
				<< " previous=" << probe.Evidence.PreviousChangeNavigation
				<< " rc_revealed=" << probe.RcRevealed
				<< " dispatches=" << static_cast<qulonglong>(implementation->WorkbenchChangeDispatches)
				<< " baseline_dispatches=" << static_cast<qulonglong>(probe.ChangeDispatches)
				<< " selection=" << (surface && surface->selection().has_value())
				<< " geometry_pending=" << (surface && surface->geometry_pending())
				<< " pending_navigation=" << implementation->PendingReviewNavigation.has_value()
				<< " tab_pending=" << (implementation->TabHost && implementation->TabHost->state().Pending.has_value())
				<< " tab_queued=" << implementation->TabAdvanceQueued << '\n';
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		auto* const window             = implementation->Window.get();
		auto* const workbench          = window->comparison_surface();
		auto* const view               = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		const auto  fixture            = implementation->Fixture->evidence();
		probe.Evidence.ExactCommitPair = fixture.BaseOid == uiforetw_base_oid
			&& fixture.HeadOneOid == uiforetw_head_oid
			&& fixture.HeadTwoOid.empty();

		// [[BPR-1036]]. Every phase that waits for an effect it does not
		// control waits through the shared bounded wait ([[BPR-0864]],
		// [[BPR-1037]]): five seconds with a floor of sixteen calls, then a
		// term naming the condition, the inputs it depends on and the
		// modality facts. `advance` is the only thing that restarts it. The
		// waits this probe already bounded by ticks and named -- the panes,
		// the reveals, the role paint and the focused layout -- keep their
		// bounds.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		const auto text = [](const std::string_view aValue) {
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		};
		const auto blobText = [](const std::optional<core::git_blob_oid>& aOid) {
			if (!aOid.has_value()) {
				return QStringLiteral("absent");
			}
			const auto hex = core::canonical_hex(*aOid);
			return QString::fromLatin1(hex.view().data(), static_cast<qsizetype>(hex.view().size()));
		};
		// What a file's selection, its publication and the comparison showing
		// it depend on. The baselines the gesture is counted against print only
		// in the phase that made that gesture.
		const auto diffFacts = [&](const std::string_view aPath, const std::string_view aBaseBlob, const std::string_view aHeadBlob) {
			return [&, aPath, aBaseBlob, aHeadBlob](QTextStream& aStream) {
				const auto        work    = diff_work_evidence(aApplication);
				const auto&       last    = implementation->LastSelectedDiff;
				const auto* const current = workbench != nullptr ? workbench->current_result().get() : nullptr;
				aStream << "path=" << text(aPath)
						<< " file_selections=" << static_cast<qulonglong>(window->statistics().FileSelections);
				if (probe.Phase == state_type::phase::awaiting_resource_diff
					|| probe.Phase == state_type::phase::awaiting_rc_diff) {
					aStream << " expected_file_selections=" << static_cast<qulonglong>(probe.WindowStatistics.FileSelections) + 1U;
				} else {
					aStream << " intent_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchIntentDispatches)
							<< " expected_intent_dispatches=" << static_cast<qulonglong>(probe.IntentDispatches) + 1U;
				}
				aStream << " current_row=" << (view != nullptr ? view->currentIndex().row() : -1)
						<< " selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
						<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
						<< " expected_selected_diff_publications=" << static_cast<qulonglong>(probe.SelectedDiffPublications) + 1U
						<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
						<< " last_selected_diff="
						<< (last.has_value() ? QString::fromStdString(last->Request.File.PresentationPath) : QStringLiteral("absent"))
						<< " last_left_blob=" << (last.has_value() ? blobText(last->Key.LeftBlob) : QStringLiteral("absent"))
						<< " expected_left_blob=" << text(aBaseBlob)
						<< " last_right_blob=" << (last.has_value() ? blobText(last->Key.RightBlob) : QStringLiteral("absent"))
						<< " expected_right_blob=" << text(aHeadBlob)
						<< " comparison_result=" << present(current != nullptr)
						<< " comparison_current=" << truth(current != nullptr && last.has_value() && current->Alignment.Key == last->Key)
						<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
						<< " diff_pending=" << truth(work.Pending)
						<< " diff_quiescent=" << truth(work.Quiescent)
						<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs);
			};
		};
		const auto resourceFacts = diffFacts(uiforetw_resource_path, uiforetw_resource_base_blob, uiforetw_resource_head_blob);
		const auto rcFacts       = diffFacts(uiforetw_rc_path, uiforetw_rc_base_blob, uiforetw_rc_head_blob);
		// The support file's blobs are not this course's to pin: it is the
		// file next-file lands on, and only its path is asserted.
		const auto supportFacts = diffFacts(uiforetw_support_path, "unpinned", "unpinned");

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			const auto  resource = drawer_row(*window, uiforetw_resource_path);
			const auto  rc       = drawer_row(*window, uiforetw_rc_path);
			if (!snapshot.has_value()
				|| implementation->WorkbenchHost->outstanding_requests() != 0U
				|| !implementation->WorkbenchHost->state().Navigation.has_value()
				|| !resource.isValid() || !rc.isValid() || view == nullptr) {
				// [[BPR-1036]]. The accepted pull request reaches the workbench
				// host as an observation, and its files reach the drawer rows
				// both gestures below select.
				return wait("UIFORETW_OBSERVATION_STALLED", [&](QTextStream& aStream) {
					aStream << "snapshot=" << present(snapshot.has_value())
							<< " snapshot_files=" << static_cast<qulonglong>(snapshot.has_value() ? snapshot->Files.size() : 0U)
							<< " window_read_quiescent=" << truth(implementation->WindowRead->is_quiescent())
							<< " navigation=" << present(implementation->WorkbenchHost->state().Navigation.has_value())
							<< " actionable=" << static_cast<qulonglong>(implementation->WorkbenchHost->actionable().size())
							<< " files_view=" << present(view != nullptr)
							<< " drawer_rows=" << (view != nullptr && view->model() != nullptr ? view->model()->rowCount() : 0)
							<< " resource_row=" << present(resource.isValid())
							<< " rc_row=" << present(rc.isValid());
				});
			}
			if (!probe.Evidence.ExactCommitPair) {
				return fail();
			}
			window->resize(1'280, 1'000);
			QApplication::processEvents();
			// [[BPR-0859]]. Leave Overview before reading the comparison.
			// Since [[BPR-0771]] this course names its pull request in the Home
			// bar, so the window opens on [[BPR-0661]]'s default opening
			// modality, Overview, which shows no comparison at all. The panes
			// still hold the diff's text, so every marker up to the reveal
			// passed while the surface carrying them was hidden: unmeasured,
			// never scrolled to the selected change, and therefore never
			// painting an intraline mark where this course looked for one. A
			// reviewer who wants to read a file's changes leaves Overview
			// first, through the strip; the course states Review and enters it
			// through the step every course shares ([[BPR-0863]]).
			probe.Waits = 0U;
			return advance(state_type::phase::awaiting_review_modality);
		}

		if (probe.Phase == state_type::phase::awaiting_review_modality) {
			const auto modality = enter_course_modality(aApplication, probe.Modality);
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			// The comparison is shown; this course reads both of its panes.
			// [[BPR-1287]]. With no file chosen yet the comparison shows its
			// direction to open one in the panes' place; the panes return
			// with the file this course selects next.
			auto* const left  = workbench == nullptr ? nullptr
													 : workbench->left_pane();
			auto* const right = workbench == nullptr ? nullptr
													 : workbench->right_pane();
			const auto  idle  = workbench != nullptr && workbench->idle() && workbench->idle_page()->isVisible();
			if (left == nullptr || right == nullptr || (!idle && (!left->isVisible() || !right->isVisible()))) {
				if (++probe.Waits <= uiforetw_review_panes_waits) {
					return result::pending;
				}
				bpr::app::detail::terminal_text_stream{ }
					<< "UIFORETW_REVIEW_PANES_HIDDEN:left_shown="
					<< (left != nullptr && left->isVisible())
					<< " right_shown=" << (right != nullptr && right->isVisible())
					<< ' ' << course_modality_facts(aApplication) << '\n';
				return fail();
			}
			// Taken here, after the modality has settled, so the file-selection
			// gesture below is measured against a window that is not still
			// changing what it shows.
			probe.WindowStatistics = window->statistics();
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			const auto resourceRow     = drawer_row(*window, uiforetw_resource_path);
			if (!resourceRow.isValid() || view == nullptr) {
				return fail();
			}
			view->setCurrentIndex(resourceRow);
			return advance(state_type::phase::awaiting_resource_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_resource_diff) {
			probe.Evidence.ResourceGesture
				= window->statistics().FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				// [[BPR-1036]]. Bounded whether or not the drawer's selection
				// registered: a gesture that never lands is named rather than
				// waited on until the run budget expires.
				return wait(
					probe.Evidence.ResourceGesture ? "UIFORETW_RESOURCE_PUBLICATION_STALLED"
												   : "UIFORETW_RESOURCE_SELECTION_STALLED",
					resourceFacts);
			}
			const auto resourcePublication
				= implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications + 1U;
			if (!resourcePublication
				|| !selected_diff_publication_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				return fail();
			}
			probe.Evidence.ResourcePublication = true;
			if (!selected_diff_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				return wait("UIFORETW_RESOURCE_BLOBS_STALLED", resourceFacts);
			}
			// [[BPR-0859]]. Reset once, on the tick the blobs first match.
			// Resetting on every tick made the role-paint cap below
			// unreachable: the count could never pass and never give up.
			if (!probe.Evidence.ResourceExactBlobs) {
				probe.Evidence.ResourceExactBlobs = true;
				probe.Waits                       = 0U;
			}
			const auto& computed                 = *workbench->current_result();
			probe.Evidence.ResourceIntralinePair = find_intraline_pair(
													   computed,
													   "_APS_NEXT_RESOURCE_VALUE        136",
													   "_APS_NEXT_RESOURCE_VALUE        137")
													   .has_value();
			if (!probe.Evidence.ResourceGesture
				|| !probe.Evidence.ResourcePublication
				|| !probe.Evidence.ResourceExactBlobs
				|| !probe.Evidence.ResourceIntralinePair) {
				return fail();
			}
			// [[BPR-0859]]. The reveal is a condition this stage waits for, not
			// one it controls: the scroll that brings the selected change on
			// screen is positioned from geometry measured on the completion
			// lane. One tick cannot tell "not yet" from "wrong".
			if (!probe.ResourceRevealed) {
				if (!reveal_intraline_pair(
						*workbench,
						computed,
						"_APS_NEXT_RESOURCE_VALUE        136",
						"_APS_NEXT_RESOURCE_VALUE        137",
						probe.RevealWaits >= uiforetw_intraline_reveal_waits)) {
					if (++probe.RevealWaits > uiforetw_intraline_reveal_waits) {
						return fail();
					}
					return result::pending;
				}
				probe.ResourceRevealed = true;
				probe.RevealWaits      = 0U;
				probe.Waits            = 0U;
			}
			probe.Evidence.ResourceRolePaint = intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 1U);
			if (!probe.Evidence.ResourceRolePaint) {
				if (++probe.Waits > 64U) {
					report_intraline_roles(
						*workbench,
						*implementation->AppearanceProfile,
						1U,
						"UIFORETW_RESOURCE_ROLE_PAINT_STALLED");
					return fail();
				}
				return result::pending;
			}
			auto* const nextFile = window->next_file_action();
			if (nextFile == nullptr || !nextFile->isEnabled()) {
				return fail();
			}
			probe.IntentDispatches = implementation->WorkbenchIntentDispatches;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			nextFile->trigger();
			return advance(state_type::phase::awaiting_support_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_support_diff) {
			if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				return wait("UIFORETW_SUPPORT_PUBLICATION_STALLED", supportFacts);
			}
			const auto exactPublication
				= implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications + 1U
				&& implementation->LastSelectedDiff.has_value()
				&& implementation->LastSelectedDiff->Request.File.PresentationPath
					== uiforetw_support_path;
			if (!exactPublication) {
				return fail();
			}
			const auto exactSupport = workbench->current_result() != nullptr
				&& workbench->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key;
			if (!exactSupport) {
				return wait("UIFORETW_SUPPORT_RESULT_STALLED", supportFacts);
			}
			probe.Evidence.NextFileNavigation
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			auto* const previousFile = window->previous_file_action();
			if (!probe.Evidence.NextFileNavigation || previousFile == nullptr
				|| !previousFile->isEnabled()) {
				return fail();
			}
			probe.IntentDispatches = implementation->WorkbenchIntentDispatches;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			previousFile->trigger();
			return advance(state_type::phase::awaiting_resource_return);
		}

		if (probe.Phase == state_type::phase::awaiting_resource_return) {
			if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				return wait("UIFORETW_RESOURCE_RETURN_PUBLICATION_STALLED", resourceFacts);
			}
			if (implementation->SelectedDiffPublications
					!= probe.SelectedDiffPublications + 1U
				|| !implementation->LastSelectedDiff.has_value()
				|| implementation->LastSelectedDiff->Request.File.PresentationPath
					!= uiforetw_resource_path) {
				return fail();
			}
			if (!selected_diff_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				return wait("UIFORETW_RESOURCE_RETURN_BLOBS_STALLED", resourceFacts);
			}
			probe.Evidence.PreviousFileNavigation
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			const auto rc = drawer_row(*window, uiforetw_rc_path);
			if (!probe.Evidence.PreviousFileNavigation || !rc.isValid()
				|| view == nullptr) {
				return fail();
			}
			probe.WindowStatistics = window->statistics();
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			view->setCurrentIndex(rc);
			return advance(state_type::phase::awaiting_rc_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_rc_diff) {
			probe.Evidence.RcGesture = window->statistics().FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				return wait(
					probe.Evidence.RcGesture ? "UIFORETW_RC_PUBLICATION_STALLED"
											 : "UIFORETW_RC_SELECTION_STALLED",
					rcFacts);
			}
			const auto rcPublication = implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications + 1U;
			if (!rcPublication
				|| !selected_diff_publication_matches(
					*implementation,
					uiforetw_rc_path,
					uiforetw_rc_base_blob,
					uiforetw_rc_head_blob)) {
				return fail();
			}
			probe.Evidence.RcPublication = true;
			if (!selected_diff_matches(
					*implementation,
					uiforetw_rc_path,
					uiforetw_rc_base_blob,
					uiforetw_rc_head_blob)) {
				return wait("UIFORETW_RC_BLOBS_STALLED", rcFacts);
			}
			if (!probe.Evidence.RcExactBlobs) {
				probe.Evidence.RcExactBlobs = true;
				probe.Waits                 = 0U;
			}
			const auto& computed = *workbench->current_result();
			const auto  pairOne  = find_intraline_pair(
				computed, "WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
			const auto pairTwo = find_intraline_pair(
				computed, "WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
			probe.Evidence.RcIntralinePairs
				= pairOne.has_value() && pairTwo.has_value();
			if (!probe.Evidence.RcIntralinePairs || !pairOne->Group.has_value()) {
				return fail();
			}
			if (!probe.ChangeOrigin) {
				auto* const nextChange = window->next_change_action();
				if (!nextChange || !window->previous_change_action()
					|| !nextChange->isEnabled() || !workbench->selection()) {
					return fail();
				}
				probe.ChangeOrigin     = workbench->selection();
				probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
				nextChange->trigger();
				return result::pending;
			}
			// [[BPR-1527]]. A navigation press may be retained until its tab
			// restoration completes. Issue each gesture once, then observe its
			// exact dispatch and semantic result on the normal completion lane.
			if (!probe.Evidence.NextChangeNavigation) {
				if (implementation->WorkbenchChangeDispatches == probe.ChangeDispatches) {
					return wait("UIFORETW_RC_NEXT_CHANGE_STALLED", rcFacts);
				}
				auto* const previousChange = window->previous_change_action();
				probe.Evidence.NextChangeNavigation
					= implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 1U
					&& workbench->selection() && workbench->selection()->Group != probe.ChangeOrigin->Group
					&& previousChange && previousChange->isEnabled();
				if (!probe.Evidence.NextChangeNavigation) {
					return fail();
				}
				previousChange->trigger();
				return result::pending;
			}
			if (!probe.Evidence.PreviousChangeNavigation) {
				if (implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 1U) {
					return wait("UIFORETW_RC_PREVIOUS_CHANGE_STALLED", rcFacts);
				}
				probe.Evidence.PreviousChangeNavigation
					= implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 2U
					&& workbench->selection() == probe.ChangeOrigin;
				if (!probe.Evidence.PreviousChangeNavigation) {
					return fail();
				}
			}
			if (!workbench->selection().has_value()
				|| workbench->selection()->Group != pairOne->Group) {
				// [[BPR-1036]]. The walk to the first pair's group is bounded by
				// the groups there are, and either end of it names itself with
				// where it stands: how far it walked, and which group is selected
				// against the one it is walking to.
				const auto walkFailed = [&](const char* const aTerm) {
					const auto groupIndex = [&computed](const auto& aGroup) {
						const auto found = std::ranges::find(computed.Alignment.Groups, aGroup, &core::change_group::Id);
						return found == computed.Alignment.Groups.end()
							? qlonglong{ -1 }
							: static_cast<qlonglong>(std::distance(computed.Alignment.Groups.begin(), found));
					};
					const auto selection = workbench->selection();
					bpr::app::detail::terminal_text_stream{ }
						<< aTerm << ":walked=" << static_cast<qulonglong>(probe.Waits)
						<< " groups=" << static_cast<qulonglong>(computed.Alignment.Groups.size())
						<< " target_group=" << groupIndex(*pairOne->Group)
						<< " selection=" << present(selection.has_value())
						<< " selected_group="
						<< (selection.has_value() && selection->Group.has_value() ? groupIndex(*selection->Group) : qlonglong{ -1 })
						<< " next_change_enabled=" << truth(window->next_change_action() != nullptr && window->next_change_action()->isEnabled())
						<< ' ' << course_modality_facts(aApplication) << '\n';
					return fail();
				};
				if (probe.Waits++ > computed.Alignment.Groups.size()) {
					return walkFailed("UIFORETW_RC_CHANGE_WALK_EXHAUSTED");
				}
				if (!workbench->navigate_change(
						core::change_navigation_direction::next,
						core::change_navigation_policy::stop)) {
					return walkFailed("UIFORETW_RC_CHANGE_WALK_REFUSED");
				}
				return result::pending;
			}
			if (!probe.Evidence.RcGesture || !probe.Evidence.RcPublication
				|| !probe.Evidence.RcExactBlobs
				|| !probe.Evidence.RcIntralinePairs) {
				return fail();
			}
			// [[BPR-0859]]. As on the resource file: the reveal is waited for
			// and bounded, and its wait is not the paint's wait.
			if (!probe.RcRevealed) {
				if (!reveal_intraline_pair(
						*workbench,
						computed,
						"WS_TABSTOP,464,28,67,10",
						"WS_TABSTOP,464,24,67,10",
						probe.RevealWaits >= uiforetw_intraline_reveal_waits)) {
					if (++probe.RevealWaits > uiforetw_intraline_reveal_waits) {
						return fail();
					}
					return result::pending;
				}
				probe.RcRevealed  = true;
				probe.RevealWaits = 0U;
				probe.Waits       = 0U;
			}
			probe.Evidence.RcRolePaint = intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 2U);
			if (!probe.Evidence.RcRolePaint) {
				if (++probe.Waits > 64U) {
					report_intraline_roles(
						*workbench,
						*implementation->AppearanceProfile,
						2U,
						"UIFORETW_RC_ROLE_PAINT_STALLED");
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			const auto semantic = std::array{
				probe.Evidence.ExactCommitPair,
				probe.Evidence.ResourceGesture,
				probe.Evidence.ResourceExactBlobs,
				probe.Evidence.ResourceIntralinePair,
				probe.Evidence.ResourceRolePaint,
				probe.Evidence.ResourcePublication,
				probe.Evidence.RcGesture,
				probe.Evidence.RcExactBlobs,
				probe.Evidence.RcIntralinePairs,
				probe.Evidence.RcRolePaint,
				probe.Evidence.RcPublication,
				probe.Evidence.PreviousFileNavigation,
				probe.Evidence.NextFileNavigation,
				probe.Evidence.PreviousChangeNavigation,
				probe.Evidence.NextChangeNavigation,
				probe.Evidence.FixtureDiffHelperUnused,
			};
			if (!std::ranges::all_of(semantic, std::identity{ })) {
				return fail();
			}
			auto* const files = window->files_drawer_toggle_action();
			auto* const conversations
				= window->conversations_drawer_toggle_action();
			auto* const repositories = window->repositories_drawer_toggle_action();
			auto* const details      = window->review_details_toggle_action();
			if (files == nullptr || conversations == nullptr || details == nullptr
				|| repositories == nullptr || !files->isEnabled()
				|| !conversations->isEnabled() || !repositories->isEnabled()) {
				return fail();
			}
			// [[BPR-0651]]. The third drawer collapses to its rail like the
			// other two, so the focused layout is the comparison alone.
			//
			// [[BPR-0859]]. Each toggle proves its wire whatever the drawer
			// starts as. Review expands the files drawer and leaves the other
			// two on their rails, so an evidence bit set only inside
			// `if (isChecked())` asserted the opening modality rather than the
			// toggle, and stayed false for a drawer that was already down. A
			// drawer on its rail is now expanded and collapsed again -- the two
			// presses a reviewer would make -- so every press still travels the
			// intent wire and the layout still ends focused.
			const auto press = [implementation](QAction* const aAction) {
				const auto dispatches = implementation->WorkbenchIntentDispatches;
				const auto checked    = aAction->isChecked();
				aAction->trigger();
				return implementation->WorkbenchIntentDispatches == dispatches + 1U
					&& aAction->isChecked() != checked;
			};
			const auto collapse = [&press](QAction* const aAction) {
				if (!aAction->isChecked() && !press(aAction)) {
					return false;
				}
				return press(aAction) && !aAction->isChecked();
			};
			probe.Evidence.RepositoriesDrawerToggle  = collapse(repositories);
			probe.Evidence.FilesDrawerToggle         = collapse(files);
			probe.Evidence.ConversationsDrawerToggle = collapse(conversations);
			if (details->isChecked()) {
				details->trigger();
			}
			// [[BPR-1334]]. Line details is on by default and is not a review
			// detail; the focused review hides it through View > Line Details,
			// as a reader giving the panes all the room would. The comparison's
			// height floors measure that focused layout.
			if (auto* const lineDetails = window->line_details_action(); lineDetails == nullptr) {
				return fail();
			} else if (lineDetails->isChecked()) {
				lineDetails->trigger();
			}
			if (!probe.Evidence.FilesDrawerToggle
				|| !probe.Evidence.ConversationsDrawerToggle
				|| !probe.Evidence.RepositoriesDrawerToggle) {
				bpr::app::detail::terminal_text_stream{ }
					<< "UIFORETW_DRAWER_TOGGLE_REFUSED:files="
					<< probe.Evidence.FilesDrawerToggle << " conversations="
					<< probe.Evidence.ConversationsDrawerToggle
					<< " repositories="
					<< probe.Evidence.RepositoriesDrawerToggle
					<< " files_checked=" << files->isChecked()
					<< " conversations_checked=" << conversations->isChecked()
					<< " repositories_checked=" << repositories->isChecked()
					<< '\n';
				return fail();
			}
			probe.Waits = 0U;
			return advance(state_type::phase::awaiting_focused_layout);
		}

		if (probe.Phase == state_type::phase::awaiting_focused_layout) {
			auto* const detail = workbench->detail_pane();
			// [[BPR-0646]]. A collapsed drawer is its rail, so focused means
			// both drawers are down to their rails; there is no review header
			// above the comparison any more.
			const auto focused = window->files_drawer()->width()
					<= qt::workbench_drawer_rail::width_pixels
				&& window->conversations_drawer()->width()
					<= qt::workbench_drawer_rail::width_pixels
				&& window->repositories_drawer()->width()
					<= qt::workbench_drawer_rail::width_pixels
				&& !window->files_drawer_toggle_action()->isChecked()
				&& !window->repositories_drawer_toggle_action()->isChecked()
				&& !window->conversations_drawer_toggle_action()->isChecked()
				&& !window->review_details_toggle_action()->isChecked()
				&& !window->epoch_selectors()->details_visible()
				// [[BPR-1334]]. Hidden through View > Line Details above.
				&& !window->line_details_action()->isChecked()
				&& detail != nullptr && !detail->isVisible();
			if (!focused) {
				if (++probe.Waits <= 64U) {
					return result::pending;
				}
				// [[BPR-0859]]. Name the part of the focused layout that did
				// not arrive, rather than the stage it did not leave.
				bpr::app::detail::terminal_text_stream{ }
					<< "UIFORETW_FOCUSED_LAYOUT_STALLED:rail="
					<< qt::workbench_drawer_rail::width_pixels
					<< " files=" << window->files_drawer()->width()
					<< " conversations=" << window->conversations_drawer()->width()
					<< " repositories=" << window->repositories_drawer()->width()
					<< " files_checked="
					<< window->files_drawer_toggle_action()->isChecked()
					<< " repositories_checked="
					<< window->repositories_drawer_toggle_action()->isChecked()
					<< " conversations_checked="
					<< window->conversations_drawer_toggle_action()->isChecked()
					<< " details_checked="
					<< window->review_details_toggle_action()->isChecked()
					<< " selector_details="
					<< window->epoch_selectors()->details_visible()
					<< " line_details_checked="
					<< window->line_details_action()->isChecked()
					<< " detail_shown="
					<< (detail != nullptr && detail->isVisible()) << '\n';
				return fail();
			}
			probe.Evidence.FocusedReviewLayout = true;
			probe.Phase                        = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::uiforetw_selected_diff_evidence(
		const github_application& aApplication) noexcept
		-> detail::uiforetw_selected_diff_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->UIforEtwSelectedDiffProbe.has_value()
			? implementation->UIforEtwSelectedDiffProbe->Evidence
			: detail::uiforetw_selected_diff_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::
		capture_uiforetw_selected_diff_evidence(
			github_application&         aApplication,
			const std::string_view      aDirectory,
			const core::appearance_mode aMode) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr
			|| implementation->AppearanceProfile == nullptr
			|| !implementation->UIforEtwSelectedDiffProbe.has_value()
			|| implementation->UIforEtwSelectedDiffProbe->Phase
				!= uiforetw_selected_diff_probe_state::phase::passed
			|| implementation->AppearanceHost->settings().Mode != aMode
			|| aDirectory.empty()) {
			return false;
		}
		auto* const       window     = implementation->Window.get();
		auto* const       workbench  = window->comparison_surface();
		const auto* const result     = workbench != nullptr
			? workbench->current_result().get()
			: nullptr;
		const auto        firstPair  = result != nullptr
			? find_intraline_pair(
				  *result,
				  "WS_TABSTOP,464,28,67,10",
				  "WS_TABSTOP,464,24,67,10")
			: std::optional<intraline_pair_location>{ };
		const auto        secondPair = result != nullptr
			? find_intraline_pair(
				  *result,
				  "WS_TABSTOP,464,42,98,10",
				  "WS_TABSTOP,464,36,98,10")
			: std::optional<intraline_pair_location>{ };
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !firstPair.has_value() || !secondPair.has_value()
			|| !firstPair->Group.has_value() || !secondPair->Group.has_value()
			|| !selected_diff_matches(
				*implementation,
				uiforetw_rc_path,
				uiforetw_rc_base_blob,
				uiforetw_rc_head_blob)) {
			return false;
		}
		// [[BPR-1033]]. The panes measure their lines in batches of about 4 ms,
		// and the link map shows no link until that measurement completes, so
		// four passes alone depend on the machine. Wait for the measurement
		// itself, bounded, then drain as before.
		const auto drain = [workbench] {
			const auto started = std::chrono::steady_clock::now();
			while (workbench->geometry_pending()
				   && std::chrono::steady_clock::now() - started < std::chrono::seconds{ 5 }) {
				QApplication::processEvents();
			}
			for (auto tick = 0; tick < 4; ++tick) {
				QApplication::processEvents();
			}
		};
		const auto selected = [workbench](const intraline_pair_location& aPair) {
			return aPair.Group.has_value()
				&& workbench->selection().has_value()
				&& workbench->selection()->Segment == aPair.Segment
				&& workbench->selection()->Group == aPair.Group
				&& workbench->link_map()->selected()
				== qt::diff_link_map_identity{
					   .Segment = aPair.Segment,
					   .Group   = aPair.Group,
				   };
		};
		const auto observeTarget = [workbench, result, implementation](
									   const std::string_view aLeft,
									   const std::string_view aRight) {
			return observe_intraline_render(
				*workbench,
				*result,
				*implementation->AppearanceProfile,
				aLeft,
				aRight);
		};
		const auto connectorValid = [](const connector_render_observation& aValue) {
			return aValue.LeftEndpoint && aValue.RightEndpoint
				&& aValue.LeftOverlap && aValue.RightOverlap
				&& aValue.Pixels.Background.isValid()
				&& aValue.Pixels.Dominant.isValid()
				&& aValue.Pixels.SampledPixels >= 24U
				&& aValue.Pixels.ContrastPixels >= 24U
				&& aValue.Pixels.DominantPixels >= 24U;
		};
		const auto selectedReadable = [&connectorValid](
										  const connector_render_observation& aValue) {
			return aValue.Selected && connectorValid(aValue);
		};
		const auto unselectedReadable = [&connectorValid](
											const connector_render_observation& aValue) {
			return !aValue.Selected && connectorValid(aValue)
				&& aValue.Pixels.ContrastPixels * 2U
				>= aValue.Pixels.SampledPixels
				&& aValue.Pixels.MedianContrastHundredths >= 300;
		};
		const auto statesDistinct = [](const connector_render_observation& aSelected,
									   const connector_render_observation& aUnselected) {
			return aSelected.Pixels.Background == aUnselected.Pixels.Background
				&& aSelected.Pixels.Dominant != aUnselected.Pixels.Dominant;
		};

		if (!navigate_to_group(*workbench, *result, *firstPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto firstSelected = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto secondUnselected = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstSelectedConnector    = firstSelected.has_value()
			? observe_connector_render(*workbench, *firstSelected)
			: std::optional<connector_render_observation>{ };
		const auto secondUnselectedConnector = secondUnselected.has_value()
			? observe_connector_render(*workbench, *secondUnselected)
			: std::optional<connector_render_observation>{ };
		if (!firstSelected.has_value() || !selected(*firstPair)
			|| firstSelected->LeftRolePixels == 0U
			|| firstSelected->RightRolePixels == 0U
			|| !secondUnselected.has_value()
			|| !firstSelectedConnector.has_value()
			|| !secondUnselectedConnector.has_value()
			|| !selectedReadable(*firstSelectedConnector)
			|| !unselectedReadable(*secondUnselectedConnector)) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_CAPTURE_FIRST_TARGET_FAILED\n";
			// [[BPR-1033]]. One term per clause of the condition above, in its
			// order, with the values each clause reads, so a failure names the
			// clause instead of the capture. After the marker, so every
			// earlier line keeps its place.
			auto       facts     = bpr::app::detail::terminal_text_stream{ };
			const auto rectangle = [&facts](const QRect& aRect) {
				facts << aRect.x() << ',' << aRect.y() << ',' << aRect.width() << ',' << aRect.height();
			};
			const auto target = [&facts, &rectangle](const char* const aName, const std::optional<intraline_render_observation>& aValue) {
				facts << ' ' << aName << "_observed=" << aValue.has_value();
				if (aValue.has_value()) {
					facts << ' ' << aName << "_left_rect=";
					rectangle(aValue->LeftRect);
					facts << ' ' << aName << "_right_rect=";
					rectangle(aValue->RightRect);
					facts << ' ' << aName << "_left_role_pixels=" << aValue->LeftRolePixels << ' ' << aName
						  << "_right_role_pixels=" << aValue->RightRolePixels;
				}
			};
			const auto color = [](const QColor& aColor) {
				return aColor.isValid() ? aColor.name(QColor::HexArgb) : QStringLiteral("invalid");
			};
			const auto connector = [&facts, &color](const char* const aName, const std::optional<connector_render_observation>& aValue) {
				facts << ' ' << aName << "_connector_observed=" << aValue.has_value();
				if (aValue.has_value()) {
					facts << ' ' << aName << "_connector=selected:" << aValue->Selected << ",endpoints:" << aValue->LeftEndpoint
						  << aValue->RightEndpoint << ",overlaps:" << aValue->LeftOverlap << aValue->RightOverlap
						  << ",background:" << color(aValue->Pixels.Background) << ",dominant:" << color(aValue->Pixels.Dominant)
						  << ",sampled:" << aValue->Pixels.SampledPixels << ",contrast:" << aValue->Pixels.ContrastPixels
						  << ",dominant_pixels:" << aValue->Pixels.DominantPixels
						  << ",median_contrast_hundredths:" << aValue->Pixels.MedianContrastHundredths;
				}
			};
			const auto workbenchSelection = workbench->selection().has_value()
				&& workbench->selection()->Segment == firstPair->Segment
				&& workbench->selection()->Group == firstPair->Group;
			facts << "UIFORETW_CAPTURE_FIRST_TARGET_FACTS:";
			target("first", firstSelected);
			facts << " first_selected=" << selected(*firstPair) << " workbench_selection=" << workbenchSelection
				  << " link_map_selection="
				  << (workbench->link_map()->selected()
					  == qt::diff_link_map_identity{
						  .Segment = firstPair->Segment,
						  .Group   = firstPair->Group,
					  });
			target("second", secondUnselected);
			// The connector clauses read the link map's visible links; a target
			// with no connector means the map shows no link for its segment.
			const auto* const map      = workbench->link_map();
			const auto        segments = [map](const intraline_pair_location& aPair) {
				return std::ranges::count_if(map->visible_links(), [&aPair](const qt::visible_diff_link& aLink) {
					return aLink.Identity.Segment == aPair.Segment;
				});
			};
			facts << " geometry_pending=" << workbench->geometry_pending() << " link_map=visible:" << map->isVisible()
				  << ",size:" << map->width() << 'x' << map->height()
				  << ",links:" << map->visible_links().size() << ",first_segment_links:" << segments(*firstPair)
				  << ",second_segment_links:" << segments(*secondPair) << ",selected:" << map->selected().has_value();
			connector("first", firstSelectedConnector);
			connector("second", secondUnselectedConnector);
			facts << " selected_readable="
				  << (firstSelectedConnector.has_value() && selectedReadable(*firstSelectedConnector))
				  << " unselected_readable="
				  << (secondUnselectedConnector.has_value() && unselectedReadable(*secondUnselectedConnector)) << '\n';
			return false;
		}

		if (!navigate_to_group(*workbench, *result, *secondPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto secondSelected = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstUnselected = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto secondSelectedConnector  = secondSelected.has_value()
			? observe_connector_render(*workbench, *secondSelected)
			: std::optional<connector_render_observation>{ };
		const auto firstUnselectedConnector = firstUnselected.has_value()
			? observe_connector_render(*workbench, *firstUnselected)
			: std::optional<connector_render_observation>{ };
		if (!secondSelected.has_value() || !selected(*secondPair)
			|| secondSelected->LeftRolePixels == 0U
			|| secondSelected->RightRolePixels == 0U
			|| !firstUnselected.has_value()
			|| !secondSelectedConnector.has_value()
			|| !firstUnselectedConnector.has_value()
			|| !selectedReadable(*secondSelectedConnector)
			|| !unselectedReadable(*firstUnselectedConnector)
			|| !statesDistinct(
				*firstSelectedConnector, *firstUnselectedConnector)
			|| !statesDistinct(
				*secondSelectedConnector, *secondUnselectedConnector)) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_CAPTURE_SECOND_TARGET_FAILED\n";
			return false;
		}

		if (!navigate_to_group(*workbench, *result, *firstPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto first = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto second = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstConnector  = first.has_value()
			? observe_connector_render(*workbench, *first)
			: std::optional<connector_render_observation>{ };
		const auto secondConnector = second.has_value()
			? observe_connector_render(*workbench, *second)
			: std::optional<connector_render_observation>{ };
		if (!intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 2U)
			|| !first.has_value() || !second.has_value()
			|| first->LeftRolePixels == 0U || first->RightRolePixels == 0U
			|| second->LeftRolePixels == 0U || second->RightRolePixels == 0U
			|| !firstConnector.has_value() || !secondConnector.has_value()
			|| !selectedReadable(*firstConnector)
			|| !unselectedReadable(*secondConnector) || !selected(*firstPair)) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_CAPTURE_FINAL_TARGET_FAILED\n";
			return false;
		}

		auto* const detail             = workbench->detail_pane();
		auto* const comparisonSurface  = workbench->comparison_splitter();
		auto* const linkMap            = workbench->link_map();
		auto* const toolbar            = window->primary_toolbar();
		auto* const reviewBar          = window->review_bar();
		auto* const filesAction        = window->files_drawer_toggle_action();
		auto* const repositoriesAction = window->repositories_drawer_toggle_action();
		auto* const previousFile       = window->previous_file_action();
		auto* const nextFile           = window->next_file_action();
		auto* const previousChange     = window->previous_change_action();
		auto* const nextChange         = window->next_change_action();
		auto* const conversationsAction
			= window->conversations_drawer_toggle_action();
		auto* const reviewDetails  = window->review_details_toggle_action();
		auto* const reviewControls = window->review_controls_button();
		if (window->epoch_selectors()->details_visible()
			|| detail == nullptr || detail->isVisible()
			|| window->files_drawer()->width()
				> qt::workbench_drawer_rail::width_pixels
			|| window->conversations_drawer()->width()
				> qt::workbench_drawer_rail::width_pixels
			|| comparisonSurface == nullptr || linkMap == nullptr
			|| toolbar == nullptr || reviewBar == nullptr || filesAction == nullptr || repositoriesAction == nullptr
			|| previousFile == nullptr || nextFile == nullptr
			|| previousChange == nullptr || nextChange == nullptr
			|| conversationsAction == nullptr || reviewDetails == nullptr
			|| reviewControls == nullptr) {
			return false;
		}
		const auto requiredActions = std::array{
			filesAction,
			previousFile,
			nextFile,
			previousChange,
			nextChange,
			conversationsAction,
		};
		// [[BPR-1129]]. The drawer toggles are the title row's; the navigation
		// is the review bar's, under it.
		auto requiredPresenters = std::array<QToolButton*, 6U>{ };
		for (std::size_t index = 0U; index < requiredActions.size(); ++index) {
			const auto* const row     = index == 0U || index + 1U == requiredActions.size()
				? static_cast<const QToolBar*>(toolbar)
				: reviewBar;
			requiredPresenters[index] = qobject_cast<QToolButton*>(
				row->widgetForAction(requiredActions[index]));
			if (requiredPresenters[index] == nullptr) {
				return false;
			}
		}
		const auto navigationIconObservations = std::array{
			observe_navigation_icon_contrast(*requiredPresenters[1]),
			observe_navigation_icon_contrast(*requiredPresenters[2]),
			observe_navigation_icon_contrast(*requiredPresenters[3]),
			observe_navigation_icon_contrast(*requiredPresenters[4]),
		};
		const auto normalIconContrastFloor
			= aMode == core::appearance_mode::high_contrast_light
				|| aMode == core::appearance_mode::high_contrast_dark
			? std::int64_t{ 700 }
			: std::int64_t{ 450 };
		const auto navigationIconsPaletteAware = std::ranges::all_of(
			navigationIconObservations,
			[normalIconContrastFloor](
				const navigation_icon_contrast_observation& aObservation) {
				return aObservation.NormalForegroundPixels > 0U
					&& aObservation.DisabledForegroundPixels > 0U
					&& aObservation.NormalContrastHundredths
					>= normalIconContrastFloor
					&& aObservation.DisabledContrastHundredths >= 300
					&& aObservation.StatesDistinct;
			});
		const auto toolbarActions   = toolbar->actions();
		const auto reviewBarActions = reviewBar->actions();
		const auto widgetActionNamed
			= [](const QList<QAction*>& aActions, const qsizetype aIndex, const char* const aName) {
				  return aIndex >= 0 && aIndex < aActions.size()
					  && aActions[aIndex]->objectName() == QLatin1String{ aName };
			  };
		// [[BPR-1129]]. RFC-BPR-0052's title row, left to right: the platform
		// inset, Home, repositories, files, the name, Open on GitHub right
		// after it ([[BPR-1338]]), a spacer, the modality
		// control, Home's View ([[BPR-1580]], hidden outside Home), refresh,
		// Home's discovery refresh (hidden outside Home),
		// comments, Hide Drawers ([[BPR-1295]]), a rule, account, settings.
		// A hidden action is still one of the row's actions: the row pins order,
		// not what is shown.
		//
		// [[BPR-0653]]. On Windows the title row carries two more actions: a
		// compact menu control takes index 1, right after the inset, which
		// shifts every later action along by one, and the caption buttons end
		// the row after settings. Hence the lead and trailing offsets rather
		// than a second copy of the row: the row stays asserted whole, in order
		// and exactly, on every platform.
	#if defined(_WIN32)
		constexpr auto lead     = qsizetype{ 1 };
		constexpr auto trailing = qsizetype{ 1 };
	#else
		constexpr auto lead     = qsizetype{ 0 };
		constexpr auto trailing = qsizetype{ 0 };
	#endif
		const auto at
			= [](const qsizetype aIndex) { return aIndex + lead; };
		const auto named = [&widgetActionNamed, &toolbarActions](
							   const qsizetype aIndex, const char* const aName) {
			return widgetActionNamed(toolbarActions, aIndex + lead, aName);
		};
		const auto exactRowOrder = toolbarActions.size() == 17 + lead + trailing
			&& widgetActionNamed(toolbarActions, 0, "workbenchStripInsetAction")
	#if defined(_WIN32)
			&& widgetActionNamed(toolbarActions, 1, "workbenchStripMenuAction")
	#endif
			&& toolbarActions[at(1)] == window->home_action()
			&& toolbarActions[at(2)] == repositoriesAction
			&& toolbarActions[at(3)] == filesAction
			&& named(4, "workbenchStripIdentityAction")
			// [[BPR-1852]]. A draft's chip, hidden unless the pull request is one.
			&& named(5, "workbenchStripDraftChipAction")
			&& named(6, "workbenchOpenOnGithubAction")
			&& named(7, "workbenchNavigationSpacerAction")
			&& named(8, "workbenchModalityAction")
			&& named(9, "homeViewAction")
			&& toolbarActions[at(10)] == window->github_refresh_action()
			&& toolbarActions[at(11)] == window->queue_refresh_action()
			&& toolbarActions[at(12)] == conversationsAction
			&& toolbarActions[at(13)] == window->toggle_drawers_action()
			&& named(14, "workbenchStripRightDividerAction")
			&& toolbarActions[at(15)] == window->account_action()
			&& toolbarActions[at(16)] == window->settings_action()
	#if defined(_WIN32)
			&& named(17, "workbenchCaptionButtonsAction")
	#endif
			;
		// [[BPR-1129]]. The review bar, left to right: an inset, Updates, the
		// comparison's menu and a rule, the file pair with its position, the
		// change pair with its position, a gap, the progress track holding
		// Seen, Read, Accept file ([[BPR-1507]]) and private progress on one
		// rail ([[BPR-1578]]), a spacer, the
		// review chip ([[BPR-1345]], hidden with no review in progress),
		// Compose review and Comment. Review details is the comparison's
		// menu's.
		const auto* const acceptFile          = window->findChild<QToolButton*>(QStringLiteral("workbenchAcceptFileButton"));
		const auto        exactReviewBarOrder = reviewBarActions.size() == 19
			&& widgetActionNamed(reviewBarActions, 0, "reviewBarInsetAction")
			&& widgetActionNamed(reviewBarActions, 1, "reviewBarUpdatesAction")
			&& widgetActionNamed(reviewBarActions, 2, "reviewBarComparingAction")
			&& widgetActionNamed(reviewBarActions, 3, "reviewBarComparisonDividerAction")
			&& widgetActionNamed(reviewBarActions, 4, "workbenchStripFilePositionAction")
			&& reviewBarActions[5] == previousFile
			&& reviewBarActions[6] == nextFile
			&& widgetActionNamed(reviewBarActions, 7, "workbenchStripChangePositionAction")
			&& reviewBarActions[8] == previousChange
			&& reviewBarActions[9] == nextChange
			&& widgetActionNamed(reviewBarActions, 10, "reviewBarProgressGapAction")
			&& widgetActionNamed(reviewBarActions, 11, "workbenchProgressTrackAction")
			&& acceptFile != nullptr && acceptFile->parentWidget() == reviewBar->widgetForAction(reviewBarActions[11])
			&& acceptFile->defaultAction() == window->accept_action()
			&& reviewControls->parentWidget() == reviewBar->widgetForAction(reviewBarActions[11])
			// [[BPR-1815]]. A rule, the layout toggle and Go to header/source.
			&& widgetActionNamed(reviewBarActions, 12, "reviewBarViewDividerAction")
			&& reviewBarActions[13] == window->one_pane_action()
			&& reviewBarActions[14] == window->header_source_action()
			&& widgetActionNamed(reviewBarActions, 15, "reviewBarSpacerAction")
			&& reviewBarActions[16] == window->review_chip_action()
			&& reviewBarActions[17] == window->compose_review_action()
			&& reviewBarActions[18] == window->comment_action()
			&& window->epoch_selectors()->comparing_button()->menu()->actions().contains(reviewDetails);
		if (!exactRowOrder || !exactReviewBarOrder) {
			// [[BPR-0859]]. An exact row order that no longer holds names the
			// rows it did find, so the difference is read from the run rather
			// than from some thirty conjuncts in a debugger.
			auto stream = bpr::app::detail::terminal_text_stream{ };
			stream << "UIFORETW_NAVIGATION_ROW_UNEXPECTED:actions="
				   << toolbarActions.size() << ",review_bar=" << reviewBarActions.size() << '\n';
			for (auto index = qsizetype{ 0 }; index < toolbarActions.size();
				 ++index) {
				stream << "UIFORETW_NAVIGATION_ROW_ACTION:" << index << '='
					   << (toolbarActions[index]->objectName().isEmpty()
							   ? QStringLiteral("<unnamed>")
							   : toolbarActions[index]->objectName())
					   << '\n';
			}
			for (auto index = qsizetype{ 0 }; index < reviewBarActions.size(); ++index) {
				stream << "UIFORETW_REVIEW_BAR_ACTION:" << index << '='
					   << (reviewBarActions[index]->objectName().isEmpty()
							   ? QStringLiteral("<unnamed>")
							   : reviewBarActions[index]->objectName())
					   << '\n';
			}
		}
		// [[BPR-1033]]. On Windows the caption buttons end the row, after
		// settings, so the row's right edge holds them too. The contract takes
		// their width off its right-edge gap, which keeps one bound for what
		// every platform puts there. Empty where the row has no window controls
		// at its right.
	#if defined(_WIN32)
		const auto windowControlsInRow = exactRowOrder
			? toolbar->actionGeometry(toolbarActions[at(16)])
			: QRect{ };
	#else
		const auto windowControlsInRow = QRect{ };
	#endif
		auto iconOnly    = toolbar->toolButtonStyle() == Qt::ToolButtonIconOnly && reviewBar->toolButtonStyle() == Qt::ToolButtonIconOnly;
		auto nativeIcons = true;
		auto accessible  = true;
		auto uniqueNames = true;
		for (std::size_t index = 0U; index < requiredActions.size(); ++index) {
			const auto* const action = requiredActions[index];
			const auto* const button = requiredPresenters[index];
			iconOnly                 = iconOnly
				&& button->toolButtonStyle() == Qt::ToolButtonIconOnly;
			nativeIcons = nativeIcons && !action->icon().isNull()
				&& !button->icon().isNull();
			accessible = accessible && !button->accessibleName().isEmpty()
				&& !button->accessibleDescription().isEmpty()
				&& !action->toolTip().isEmpty() && !action->shortcut().isEmpty()
				&& button->focusPolicy() == Qt::StrongFocus;
			for (std::size_t other = index + 1U;
				 other < requiredActions.size(); ++other) {
				uniqueNames = uniqueNames
					&& requiredPresenters[index]->accessibleName()
						!= requiredPresenters[other]->accessibleName()
					&& requiredActions[index]->objectName()
						!= requiredActions[other]->objectName();
			}
		}
		const auto inWindow = [window](const QWidget& aWidget) {
			return QRect{
				aWidget.mapTo(window, QPoint{ 0, 0 }),
				aWidget.size(),
			};
		};
		// [[BPR-1130]]. A new window starts with the Updates line folded, and
		// RFC-BPR-0052 AC-11 takes the stack both ways. The line opens through
		// the window's own command, so the core decides it and the host
		// publishes it; it is measured open and folded again before anything
		// below is captured.
		const auto initiallyFolded = !window->updates_line_open() && !window->epoch_selectors()->isVisible();
		const auto settleFold      = [window, &drain](const bool aOpen) {
			const auto started = std::chrono::steady_clock::now();
			while (window->updates_line_open() != aOpen
				   && std::chrono::steady_clock::now() - started < std::chrono::seconds{ 5 }) {
				QApplication::processEvents();
			}
			drain();
			return window->updates_line_open() == aOpen && window->epoch_selectors()->isVisible() == aOpen;
		};
		window->updates_line_action()->trigger();
		const auto opened         = settleFold(true);
		const auto openLineRect   = inWindow(*window->epoch_selectors());
		const auto openComparison = inWindow(*comparisonSurface);
		auto       openStack      = QString{ };
		{
			auto stream = QTextStream{ &openStack };
			report_capture_layout(stream, *window, *comparisonSurface);
		}
		window->updates_line_action()->trigger();
		const auto refolded = settleFold(false);
		if (!initiallyFolded || !opened || !refolded) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_UPDATES_LINE_FOLD_FAILED:initially_folded=" << initiallyFolded
													  << " opened=" << opened << " refolded=" << refolded << '\n';
			return false;
		}
		// [[BPR-1413]]. A pane moves its viewport up by the within-line
		// offset and makes it a line taller, masked to the pane's content
		// area. The pane the reviewer sees is that mask, as BPR-1413's own
		// tests read it; the widget itself reaches past the comparison.
		const auto visiblePane = [&inWindow, window](const QWidget& aViewport) {
			const auto mask = aViewport.mask();
			if (mask.isEmpty()) {
				return inWindow(aViewport);
			}
			const auto area = mask.boundingRect();
			return QRect{ aViewport.mapTo(window, area.topLeft()), area.size() };
		};
		const auto comparisonRect = inWindow(*comparisonSurface);
		const auto leftPaneRect   = visiblePane(*workbench->left_pane()->viewport());
		const auto rightPaneRect  = visiblePane(*workbench->right_pane()->viewport());
		const auto gutterRect     = inWindow(*linkMap);
		const auto widthRatio     = (1'000 * comparisonRect.width())
			/ std::max(1, window->width());
		const auto heightRatio = (1'000 * comparisonRect.height())
			/ std::max(1, window->height());
		const auto openHeightRatio = (1'000 * openComparison.height())
			/ std::max(1, window->height());
		if (widthRatio < uiforetw_capture_width_floor
			|| heightRatio < uiforetw_capture_folded_height_floor
			|| openHeightRatio < uiforetw_capture_open_height_floor
			|| comparisonRect.height() - openComparison.height() != openLineRect.height()) {
			// [[BPR-1036]]. Name what took the room, not only how much is left:
			// the folded stack, then the open one.
			auto stream = bpr::app::detail::terminal_text_stream{ };
			stream << "UIFORETW_CAPTURE_LAYOUT_FAILED:" << widthRatio << ':'
				   << heightRatio << ":open=" << openHeightRatio;
			report_capture_layout(stream, *window, *comparisonSurface);
			stream << " open_line=" << openLineRect.top() << '+' << openLineRect.height()
				   << " open:" << openStack << ' ' << course_modality_facts(aApplication) << '\n';
			return false;
		}

		const auto path = QString::fromUtf8(
			aDirectory.data(), static_cast<qsizetype>(aDirectory.size()));
		if (!QDir{ }.mkpath(path)) {
			return false;
		}
		const auto directory  = QDir{ path };
		const auto full       = window->grab();
		const auto comparison = comparisonSurface->grab();
		if (full.isNull() || comparison.isNull()
			|| !full.save(directory.filePath(QStringLiteral("full-window.png")), "PNG")
			|| !comparison.save(
				directory.filePath(QStringLiteral("intraline-comparison.png")),
				"PNG")) {
			return false;
		}

		const auto removed = implementation->AppearanceProfile->color(
			core::appearance_role::diff_intraline_removed_background);
		const auto added = implementation->AppearanceProfile->color(
			core::appearance_role::diff_intraline_added_background);
		const auto& key      = implementation->LastSelectedDiff->Key;
		const auto  blobText = [](const core::git_blob_oid& aOid) {
			const auto text = core::canonical_hex(aOid);
			return std::string{ text.view() };
		};
		const auto identityText = []<typename Identity>(const Identity& aIdentity) {
			const auto& bytes = aIdentity.bytes();
			return QString::fromLatin1(
					   QByteArray{
						   reinterpret_cast<const char*>(bytes.data()),
						   static_cast<qsizetype>(bytes.size()) }
						   .toHex())
				.toStdString();
		};
		const auto connectorJson = [](const connector_render_observation& aValue) {
			return capture_object(
				"state",
				aValue.Selected ? "selected"
								: "unselected",
				"gutter_background",
				aValue.Pixels.Background.name(QColor::HexArgb).toStdString(),
				"dominant_path_pixel",
				aValue.Pixels.Dominant.name(QColor::HexArgb).toStdString(),
				"sampled_path_pixels",
				static_cast<qint64>(aValue.Pixels.SampledPixels),
				"contrast_pixels",
				static_cast<qint64>(aValue.Pixels.ContrastPixels),
				"dominant_path_pixels",
				static_cast<qint64>(aValue.Pixels.DominantPixels),
				"contrast_floor_hundredths",
				300,
				"median_contrast_hundredths",
				static_cast<qint64>(aValue.Pixels.MedianContrastHundredths),
				"bounds",
				rectangle_json(aValue.Bounds),
				"start",
				point_json(aValue.Start),
				"end",
				point_json(aValue.End),
				"left_endpoint_at_pane_edge",
				aValue.LeftEndpoint,
				"right_endpoint_at_pane_edge",
				aValue.RightEndpoint,
				"left_target_row_overlap",
				aValue.LeftOverlap,
				"right_target_row_overlap",
				aValue.RightOverlap);
		};
		const auto firstGroup = std::ranges::find(
			result->Alignment.Groups, *firstPair->Group, &core::change_group::Id);
		const auto secondGroup = std::ranges::find(
			result->Alignment.Groups, *secondPair->Group, &core::change_group::Id);
		if (firstGroup == result->Alignment.Groups.end()
			|| secondGroup == result->Alignment.Groups.end()) {
			return false;
		}
		const auto targetJson = [&identityText, &connectorJson](
									const intraline_render_observation& aTarget,
									const intraline_render_observation& aSelectedTarget,
									const connector_render_observation& aConnector,
									const connector_render_observation& aSelectedConnector,
									const connector_render_observation& aUnselectedConnector,
									const qint64                        aChangeIndex) {
			return capture_object(
				"segment",
				identityText(aTarget.Pair.Segment),
				"group",
				aTarget.Pair.Group.has_value()
					? identityText(*aTarget.Pair.Group)
					: std::string{ },
				"left_line",
				static_cast<qint64>(aTarget.Pair.Left),
				"right_line",
				static_cast<qint64>(aTarget.Pair.Right),
				"left_rect_in_pane",
				rectangle_json(aTarget.LeftRect),
				"right_rect_in_pane",
				rectangle_json(aTarget.RightRect),
				"left_role_pixels",
				static_cast<qint64>(aTarget.LeftRolePixels),
				"right_role_pixels",
				static_cast<qint64>(aTarget.RightRolePixels),
				"selected_observed",
				true,
				"selected_left_role_pixels",
				static_cast<qint64>(aSelectedTarget.LeftRolePixels),
				"selected_right_role_pixels",
				static_cast<qint64>(aSelectedTarget.RightRolePixels),
				"change_index",
				aChangeIndex,
				"connector",
				connectorJson(aConnector),
				"selected_connector",
				connectorJson(aSelectedConnector),
				"unselected_connector",
				connectorJson(aUnselectedConnector));
		};
		const auto firstChangeIndex = static_cast<qint64>(
			std::distance(result->Alignment.Groups.begin(), firstGroup));
		const auto secondChangeIndex = static_cast<qint64>(
			std::distance(result->Alignment.Groups.begin(), secondGroup));
		const auto iconContrastJson = [](
										  const navigation_icon_contrast_observation& aValue) {
			return capture_object(
				"background",
				aValue.Background.name(QColor::HexArgb).toStdString(),
				"normal_foreground",
				aValue.NormalForeground.name(QColor::HexArgb).toStdString(),
				"disabled_foreground",
				aValue.DisabledForeground.name(QColor::HexArgb).toStdString(),
				"normal_foreground_pixels",
				static_cast<qint64>(aValue.NormalForegroundPixels),
				"disabled_foreground_pixels",
				static_cast<qint64>(aValue.DisabledForegroundPixels),
				"normal_contrast_hundredths",
				static_cast<qint64>(aValue.NormalContrastHundredths),
				"disabled_contrast_hundredths",
				static_cast<qint64>(aValue.DisabledContrastHundredths),
				"states_distinct",
				aValue.StatesDistinct);
		};
		auto facts = capture_object(
			"schema",
			"bpr.uiforetw_intraline_capture.v7",
			"appearance",
			appearance_mode_name(aMode).toStdString(),
			"qt_platform",
			QGuiApplication::platformName().toStdString(),
			"qt_version",
			std::string{ qVersion() },
			"style",
			std::string{ window->style()->metaObject()->className() },
			"device_pixel_ratio",
			full.devicePixelRatio(),
			"logical_width",
			window->width(),
			"logical_height",
			window->height(),
			"pixel_width",
			full.width(),
			"pixel_height",
			full.height(),
			"comparison_pixel_width",
			comparison.width(),
			"comparison_pixel_height",
			comparison.height(),
			"selected_path",
			std::string{ uiforetw_rc_path },
			"base_commit",
			std::string{ uiforetw_base_oid },
			"head_commit",
			std::string{ uiforetw_head_oid },
			"base_blob",
			blobText(*key.LeftBlob),
			"head_blob",
			blobText(*key.RightBlob),
			"removed_role",
			removed.name(QColor::HexArgb).toStdString(),
			"added_role",
			added.name(QColor::HexArgb).toStdString(),
			"intraline_edit_count",
			static_cast<qint64>(workbench->current_result()->Intraline.size()),
			"fixture_diff_helper_invocations",
			static_cast<qint64>(implementation->FixtureDiffHelperInvocations),
			"previous_file_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.PreviousFileNavigation,
			"next_file_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.NextFileNavigation,
			"previous_change_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.PreviousChangeNavigation,
			"next_change_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.NextChangeNavigation,
			"files_drawer_toggle_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.FilesDrawerToggle,
			"conversations_drawer_toggle_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.ConversationsDrawerToggle,
			"repositories_drawer_toggle_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.RepositoriesDrawerToggle,
			"title_row_exact_order",
			exactRowOrder,
			"review_bar_exact_order",
			exactReviewBarOrder,
			"navigation_actions_icon_only",
			iconOnly,
			"navigation_actions_have_native_icons",
			nativeIcons,
			"navigation_icons_palette_aware",
			navigationIconsPaletteAware,
			"navigation_icon_contrast",
			capture_object(
				"previous_file",
				iconContrastJson(navigationIconObservations[0]),
				"next_file",
				iconContrastJson(navigationIconObservations[1]),
				"previous_change",
				iconContrastJson(navigationIconObservations[2]),
				"next_change",
				iconContrastJson(navigationIconObservations[3])),
			"navigation_actions_accessible",
			accessible,
			"navigation_accessible_names_unique",
			uniqueNames,
			"files_drawer_visible",
			false,
			"conversations_drawer_visible",
			false,
			"review_details_visible",
			false,
			"epoch_controls_visible",
			false,
			"selected_change_detail_visible",
			false,
			"full_window_rect",
			rectangle_json(window->rect()),
			"title_row_rect_in_window",
			rectangle_json(inWindow(*toolbar)),
			"review_bar_rect_in_window",
			rectangle_json(inWindow(*reviewBar)),
			"files_toggle_rect_in_title_row",
			rectangle_json(requiredPresenters[0]->geometry()),
			"conversations_toggle_rect_in_title_row",
			rectangle_json(requiredPresenters[5]->geometry()),
			"hide_drawers_rect_in_title_row",
			rectangle_json(toolbar->actionGeometry(window->toggle_drawers_action())),
			"window_controls_rect_in_title_row",
			rectangle_json(windowControlsInRow),
			"comparing_rect_in_bar",
			rectangle_json(window->epoch_selectors()->comparing_button()->geometry()),
			"previous_change_rect_in_bar",
			rectangle_json(requiredPresenters[3]->geometry()),
			"next_change_rect_in_bar",
			rectangle_json(requiredPresenters[4]->geometry()),
			"previous_file_rect_in_bar",
			rectangle_json(requiredPresenters[1]->geometry()),
			"next_file_rect_in_bar",
			rectangle_json(requiredPresenters[2]->geometry()),
			"private_progress_rect_in_bar",
			// [[BPR-1578]]. Private progress is the progress track's last cell,
			// so its rectangle is mapped from the track into the bar.
			rectangle_json(QRect{ reviewControls->mapTo(reviewBar, QPoint{ }), reviewControls->size() }),
			"compose_review_rect_in_bar",
			rectangle_json(reviewBar->actionGeometry(window->compose_review_action())),
			"comment_rect_in_bar",
			rectangle_json(reviewBar->actionGeometry(window->comment_action())),
			"review_details_in_comparison_menu",
			window->epoch_selectors()->comparing_button()->menu()->actions().contains(reviewDetails),
			"comparison_rect_in_window",
			rectangle_json(comparisonRect),
			"left_pane_rect_in_window",
			rectangle_json(leftPaneRect),
			"right_pane_rect_in_window",
			rectangle_json(rightPaneRect),
			"gutter_rect_in_window",
			rectangle_json(gutterRect),
			"files_drawer_rect_in_splitter",
			rectangle_json(window->files_drawer()->geometry()),
			"conversations_drawer_rect_in_splitter",
			rectangle_json(window->conversations_drawer()->geometry()),
			"repositories_drawer_rect_in_splitter",
			rectangle_json(window->repositories_drawer()->geometry()),
			"review_details_rect_in_window",
			rectangle_json(inWindow(*window->epoch_selectors())),
			"comparison_width_ratio_permille",
			widthRatio,
			"comparison_height_ratio_permille",
			heightRatio,
			"updates_line_initially_folded",
			initiallyFolded,
			"updates_line_open_rect_in_window",
			rectangle_json(openLineRect),
			"updates_line_open_comparison_height",
			openComparison.height(),
			"updates_line_open_height_ratio_permille",
			openHeightRatio,
			"document_tabs_rect_in_window",
			rectangle_json(inWindow(*window->document_tabs())),
			"left_horizontal_offset",
			workbench->left_pane()->horizontalScrollBar()->value(),
			"right_horizontal_offset",
			workbench->right_pane()->horizontalScrollBar()->value(),
			"selected_change_index",
			firstChangeIndex,
			"selected_segment",
			identityText(first->Pair.Segment),
			"selected_group",
			identityText(*first->Pair.Group),
			"replacement_28_to_24",
			targetJson(*first, *firstSelected, *firstConnector, *firstSelectedConnector, *firstUnselectedConnector, firstChangeIndex),
			"replacement_42_to_36",
			targetJson(*second, *secondSelected, *secondConnector, *secondSelectedConnector, *secondUnselectedConnector, secondChangeIndex));
		auto file = QFile{ directory.filePath("capture-facts.json") };
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		std::string bytes;
		if (glz::write<capture_json_options{ }>(facts, bytes)) {
			return false;
		}
		bytes.push_back('\n');
		return file.write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size()) && file.flush();
	}

	namespace {
		[[nodiscard]] auto review_ledger_probe_note(
			const core::file_identity          aFile,
			const core::file_comparison_digest aComparison)
			-> application::local_note_record
		{
			return {
				.Id         = application::local_note_id{ 228U },
				.Attachment = application::local_note_attachment{
					.File       = aFile,
					.Comparison = aComparison,
				},
				.Note = { .Text = "BPR-0228 host-only composition probe." },
			};
		}

		[[nodiscard]] constexpr auto review_ledger_probe_thread_identity() noexcept
			-> std::string_view
		{
			return "review-thread:PRRT_fixture_1";
		}

		[[nodiscard]] auto review_ledger_probe_thread_matches_file(
			const core::file_identity                       aFile,
			const application::github_review_thread_record& aThread) -> bool
		{
			const auto acceptedIdentity
				= "review-thread:" + std::string{ aThread.Thread.value() };
			return acceptedIdentity == review_ledger_probe_thread_identity()
				&& aThread.Subject
				== application::github_review_thread_subject::line
				&& !aThread.Outdated
				&& (aThread.DiffSide == application::github_diff_side::left
					|| aThread.DiffSide == application::github_diff_side::right)
				&& aThread.Line.has_value() && *aThread.Line != 0U
				&& !aThread.Path.empty()
				&& core::file_identity_of(aThread.Path) == aFile;
		}

		[[nodiscard]] auto review_ledger_probe_defect(
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread)
			-> std::optional<application::durable_defect_record>
		{
			if (!review_ledger_probe_thread_matches_file(aFile, aThread)) {
				return std::nullopt;
			}
			const auto identity = defect_identity(review_ledger_probe_thread_identity());
			const auto side     = aThread.DiffSide == application::github_diff_side::left
				? core::diff_side::left
				: core::diff_side::right;
			const auto anchor   = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = side,
				.Line       = *aThread.Line - 1U,
			};
			return application::durable_defect_record{
				.File         = aFile,
				.Conversation = core::review_conversation_id{ identity.Conversation },
				.History      = {
					.Defect = {
						.Id       = core::defect_id{ identity.Defect },
						.Anchor   = anchor,
						.Severity = core::defect_severity::major,
						.Category = core::defect_category::correctness,
						.State    = core::defect_state::needs_verification,
					},
					.OriginalAnchor = anchor,
					.Availability   = core::defect_anchor_availability::available,
				},
			};
		}

		[[nodiscard]] auto review_ledger_probe_defect_matches_thread(
			const application::durable_defect_record&       aDefect,
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread) -> bool
		{
			if (!review_ledger_probe_thread_matches_file(aFile, aThread)) {
				return false;
			}
			const auto identity = defect_identity(review_ledger_probe_thread_identity());
			const auto side     = aThread.DiffSide == application::github_diff_side::left
				? core::diff_side::left
				: core::diff_side::right;
			const auto anchor   = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = side,
				.Line       = *aThread.Line - 1U,
			};
			return aDefect.File == aFile
				&& aDefect.Conversation.value() == identity.Conversation
				&& aDefect.History.Defect.Id.value() == identity.Defect
				&& aDefect.History.Defect.Anchor == anchor
				&& aDefect.History.OriginalAnchor == anchor;
		}

		[[nodiscard]] auto review_ledger_probe_defect_match_is_non_vacuous(
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread) -> bool
		{
			const auto expected = review_ledger_probe_defect(
				aFile, aComparison, aThread);
			if (!expected.has_value()) {
				return false;
			}
			auto       wrongLine = *expected;
			const auto counterexampleLine
				= wrongLine.History.Defect.Anchor.Line == 1U ? 2U : 1U;
			wrongLine.History.Defect.Anchor.Line  = counterexampleLine;
			wrongLine.History.OriginalAnchor.Line = counterexampleLine;
			auto wrongSide                        = *expected;
			wrongSide.History.Defect.Anchor.Side  = aThread.DiffSide
					== application::github_diff_side::left
				? core::diff_side::right
				: core::diff_side::left;
			wrongSide.History.OriginalAnchor.Side
				= wrongSide.History.Defect.Anchor.Side;
			return review_ledger_probe_defect_matches_thread(
					   *expected, aFile, aComparison, aThread)
				&& !review_ledger_probe_defect_matches_thread(
					   wrongLine, aFile, aComparison, aThread)
				&& !review_ledger_probe_defect_matches_thread(
					   wrongSide, aFile, aComparison, aThread);
		}
	} // namespace

	auto detail::github_application_fixture_access::exercise_review_ledger_composition(
		github_application&                   aApplication,
		const review_ledger_composition_probe aCourse)
		-> detail::review_ledger_composition_probe_result
	{
		using result               = detail::review_ledger_composition_probe_result;
		using state_type           = review_ledger_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return result::failed;
		}

		if (!implementation->ReviewLedgerCompositionProbe.has_value()) {
			implementation->ReviewLedgerCompositionProbe.emplace();
			implementation->ReviewLedgerCompositionProbe->Course = aCourse;
		}
		auto& probe = *implementation->ReviewLedgerCompositionProbe;
		if (probe.Course != aCourse
			|| probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		auto* const window = implementation->Window.get();
		auto* const host   = implementation->WorkbenchHost.get();
		auto* const ledger = implementation->ReviewLedgerHost.get();
		const auto  fail   = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		// [[BPR-1037]]. Every phase that waits is bounded and names what it
		// was waiting for; see `keep_waiting`.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		// [[BPR-1331]]. What the review bar's private progress button says.
		const auto progressMark = [window] {
			const auto* const button = window->findChild<QWidget*>(QStringLiteral("workbenchReviewControlsButton"));
			return button != nullptr ? button->accessibleName().toStdString() : std::string{ "absent" };
		};
		// What the ledger's own counters say, for the phases that wait on it.
		const auto ledgerFacts = [&](QTextStream& aStream) {
			const auto statistics = ledger->statistics();
			aStream << "ledger_commands=" << static_cast<qulonglong>(statistics.Commands)
					<< " ledger_persistence_requests=" << static_cast<qulonglong>(statistics.PersistenceRequests)
					<< " ledger_completions=" << static_cast<qulonglong>(statistics.Completions)
					<< " ledger_store_failures=" << static_cast<qulonglong>(statistics.StoreFailures)
					<< " ledger_material_observations=" << static_cast<qulonglong>(statistics.MaterialObservations);
		};

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& handoff  = *implementation->LastAcceptedHandoff;
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| host->actionable().empty()) {
				// The accepted pull request's files reach the workbench host as
				// an observation, and the ledger's gestures need one of them.
				return wait("REVIEW_LEDGER_OBSERVATION_STALLED", [&](QTextStream& aStream) {
					aStream << "snapshot=" << present(snapshot.has_value())
							<< " snapshot_files=" << static_cast<qulonglong>(snapshot.has_value() ? snapshot->Files.size() : 0U)
							<< " window_read_quiescent=" << truth(implementation->WindowRead->is_quiescent())
							<< " actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< ' ';
					ledgerFacts(aStream);
				});
			}
			probe.Evidence.ExactAuthority
				= implementation->ReviewLedgerAuthority
				== std::optional{ workbench_authority(handoff) };
			probe.Evidence.HostComposed = ledger != nullptr;
			probe.Evidence.PublicPathAvailable
				= window->github_status_text() == "Pull request ready.";
			probe.Evidence.HostOnlyProbe = true;
			probe.Evidence.ReadStateObserved
				= ledger != nullptr && ledger->statistics().ConversationLoads >= 1U;
			probe.InitialStatistics = ledger->statistics();
			const auto modality     = enter_course_modality(aApplication, probe.Overview);
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			auto* const view = window->files_drawer()->contents_view();
			if (!probe.Evidence.ExactAuthority || !probe.Evidence.HostComposed
				|| !probe.Evidence.PublicPathAvailable || view == nullptr
				|| view->model() == nullptr || view->model()->rowCount() == 0) {
				return fail();
			}
			view->setCurrentIndex(view->model()->index(0, 0));
			return advance(state_type::phase::awaiting_hidden_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_hidden_diff) {
			const auto* surface = window->comparison_surface();
			if (!surface->current_result() || !implementation->SelectedDiffPublication || !ledger->is_quiescent()) {
				return wait("REVIEW_LEDGER_HIDDEN_DIFF_STALLED", ledgerFacts);
			}
			// Loading a selected file underneath Overview must not claim it was viewed.
			probe.SeenOnEntry = ledger->retained().Read == core::file_read_state::seen;
			if (!window->showing_overview() || surface->isVisibleTo(window)
				|| ledger->statistics().Commands != 0U
				|| (aCourse != review_ledger_composition_probe::unavailable
					&& ledger->retained().Read != core::file_read_state::unread
					&& !(aCourse == review_ledger_composition_probe::reopen && probe.SeenOnEntry))) {
				return fail();
			}
			return advance(state_type::phase::awaiting_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_diff) {
			const auto modality = enter_course_modality(aApplication, probe.Review);
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			const auto* const comparison = window->comparison_surface();
			if (comparison == nullptr || comparison->current_result() == nullptr
				|| !implementation->LastSelectedDiff.has_value() || !ledger->is_quiescent()) {
				// The file gesture's diff is computed through the Git and cache
				// actors and published to the comparison.
				return wait("REVIEW_LEDGER_SELECTED_DIFF_STALLED", [&](QTextStream& aStream) {
					const auto work = diff_work_evidence(aApplication);
					aStream << "file_selections=" << static_cast<qulonglong>(window->statistics().FileSelections)
							<< " selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
							<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
							<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
							<< " last_selected_diff=" << present(implementation->LastSelectedDiff.has_value())
							<< " comparison_result=" << present(comparison != nullptr && comparison->current_result() != nullptr)
							<< " host_selected=" << present(host->selected().has_value())
							<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
							<< " diff_pending=" << truth(work.Pending)
							<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs)
							<< " ledger_outstanding=" << static_cast<qulonglong>(ledger->outstanding());
				});
			}
			const auto& selected   = *implementation->LastSelectedDiff;
			const auto  material   = core::file_material(selected.Key);
			const auto  retained   = ledger->retained();
			const auto  statistics = ledger->statistics();
			probe.File             = selected.Request.File.File;
			probe.Material         = material;
			probe.Comparison       = core::comparison_digest(selected.Key);
			probe.Evidence.RealFileGesture
				= window->statistics().FileSelections > 0U;
			probe.Evidence.RealSelectedDiff
				= selected.Request.File.File.valid()
				&& selected.Key == comparison->current_result()->Alignment.Key;
			probe.Evidence.ExactMaterial = material.valid()
				&& (aCourse == review_ledger_composition_probe::unavailable
					|| (retained.File == selected.Request.File.File
						&& retained.Material == material));
			probe.Evidence.ExpectedPreGesturePersistence
				= aCourse == review_ledger_composition_probe::seed
				? statistics.Commands == 1U
					&& statistics.PersistenceRequests == 1U
					&& statistics.Completions == 1U
				: aCourse == review_ledger_composition_probe::reopen
				? statistics.Commands == (probe.SeenOnEntry ? 0U : 1U)
					&& statistics.PersistenceRequests == (probe.SeenOnEntry ? 0U : 2U)
					&& statistics.Completions == (probe.SeenOnEntry ? 0U : 2U)
				: statistics.Commands == 0U
					&& statistics.PersistenceRequests == 0U
					&& statistics.Completions == 0U
					&& statistics.StoreFailures > 0U;
			// [[BPR-0824]]. Not `ReceiptLoadHits`: this window's queued refresh_shared_ledger re-reads its own carry-over commit, racing this tick.
			probe.Evidence.ReceiptLoadedOnEntry
				= statistics.MaterialObservations == 1U
				&& statistics.MaterialReceiptHits == 1U;
			implementation->ReviewLedgerWasPresentOnEntry
				= probe.Evidence.ReceiptLoadedOnEntry;

			if (!probe.Evidence.RealFileGesture
				|| !probe.Evidence.RealSelectedDiff
				|| !probe.Evidence.ExactMaterial
				|| !probe.Evidence.ExpectedPreGesturePersistence) {
				return fail();
			}

			if (aCourse == review_ledger_composition_probe::unavailable) {
				probe.Evidence.UnavailableVisible
					= window->status_text().contains(
						  QStringLiteral("Private review progress is unavailable."))
					&& !window->seen_action()->isEnabled()
					&& !window->read_action()->isEnabled()
					&& !window->accept_action()->isEnabled()
					&& !window->exclude_action()->isEnabled()
					&& statistics.StoreFailures > 0U;
				probe.Evidence.DurableReceipt          = true;
				probe.Evidence.PrivateRecords          = true;
				probe.Evidence.GestureSequence         = true;
				probe.Evidence.ActionabilityProjection = true;
				if (!probe.Evidence.UnavailableVisible) {
					return fail();
				}
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			// [[BPR-1506]]. The ordinary file gesture made Seen durable before
			// any receipt action. A late receipt for another file, material or
			// request generation must not submit a command, even if unread.
			// [[BPR-1701]]. Seen reached is checked and offers Mark unseen.
			const auto durable = ledger->cached_receipt(probe.File);
			if (retained.Read != core::file_read_state::seen || retained.Accepted || retained.Excluded
				|| !window->seen_action()->isChecked() || progressMark() != "Private progress: seen"
				|| !durable || !*durable || **durable != retained) {
				return fail();
			}
			auto unread = retained;
			unread.Read = core::file_read_state::unread;
			implementation->AutoSeenPresentedRequest.reset();
			auto stale                       = selected;
			stale.Request.Context.Generation = core::request_generation{ selected.Request.Context.Generation.value() + 1U };
			implementation->mark_presented_file_seen(stale, unread);
			stale                   = selected;
			stale.Request.File.File = core::file_identity_of("not-the-presented-file.cpp");
			implementation->mark_presented_file_seen(stale, unread);
			auto wrongMaterial     = unread;
			wrongMaterial.Material = core::file_material_id{ };
			implementation->mark_presented_file_seen(selected, wrongMaterial);
			implementation->observe_selected_material(application::diff_publication{ .Context = *comparison->current_context() });
			implementation->mark_presented_file_seen(selected, retained);
			if (ledger->outstanding() != 0U || ledger->statistics().Commands != statistics.Commands) {
				return fail();
			}

			const auto* const defectThread = implementation->accepted_thread(
				review_ledger_probe_thread_identity());
			if (defectThread == nullptr
				|| !review_ledger_probe_thread_matches_file(
					probe.File, *defectThread)) {
				return fail();
			}

			if (aCourse == review_ledger_composition_probe::seed) {
				// [[BPR-1211]]. Each gesture is decided and made durable off the
				// decision lane; the course gives each one a quiescent ledger,
				// as a reviewer waiting for the button to change would.
				probe.GesturesBefore = ledger->statistics();
				probe.Gesture        = 0U;
				return advance(state_type::phase::awaiting_gestures);
			} else {
				const auto receipt      = ledger->retained();
				const auto privateState = ledger->private_observation();
				probe.Evidence.GestureSequence
					= ledger->statistics().Commands == (probe.SeenOnEntry ? 0U : 1U);
				probe.Evidence.DurableReceipt
					= probe.Evidence.ReceiptLoadedOnEntry
					&& receipt.File == probe.File
					&& receipt.Material == probe.Material
					&& receipt.Read == core::file_read_state::seen
					&& !receipt.Accepted && !receipt.Excluded;
				probe.Evidence.ProgressMark = progressMark();
				const auto priorHead        = implementation->Fixture != nullptr
					? core::parse_git_commit_oid(
						  implementation->Fixture->evidence().HeadOneOid)
					: std::optional<core::git_commit_oid>{ };
				if (privateState.has_value() && privateState->Notes.size() == 1U
					&& privateState->Notes.front().Attachment.has_value()) {
					const auto storedComparison
						= privateState->Notes.front().Attachment->Comparison;
					auto expectedNote     = review_ledger_probe_note(probe.File, storedComparison);
					expectedNote.Revision = application::local_note_revision{ 1U };
					auto defect           = review_ledger_probe_defect(
						probe.File, storedComparison, *defectThread);
					// The reuse course advances to head two. The conversation-backed
					// defect from head one must survive, but its old comparison can no
					// longer prove an exact placement.
					if (defect.has_value()) {
						defect->History.Availability
							= core::defect_anchor_availability::outdated;
					}
					probe.Evidence.PrivateRecords
						= defect.has_value()
						&& review_ledger_probe_defect_match_is_non_vacuous(
							  probe.File, storedComparison, *defectThread)
						&& storedComparison.valid()
						&& storedComparison != probe.Comparison
						&& privateState->Notes
							== std::vector{ expectedNote }
						&& priorHead.has_value()
						&& privateState->LastReviewedHead == priorHead
						&& privateState->Defects
							== std::vector{ *defect }
						&& review_ledger_probe_defect_matches_thread(
							  privateState->Defects.front(),
							  probe.File,
							  storedComparison,
							  *defectThread)
						&& ledger->statistics().PrivateWrites == (probe.SeenOnEntry ? 0U : 1U);
				}
				implementation->ReviewLedgerRoundTripPassed
					= probe.Evidence.DurableReceipt;
			}
			if (!probe.Evidence.DurableReceipt
				|| !probe.Evidence.PrivateRecords) {
				return fail();
			}
			return advance(state_type::phase::awaiting_actionability);
		}

		if (probe.Phase == state_type::phase::awaiting_gestures) {
			if (!ledger->is_quiescent()) {
				return wait("REVIEW_LEDGER_GESTURE_STALLED", [&](QTextStream& aStream) {
					aStream << "gesture=" << static_cast<qulonglong>(probe.Gesture)
							<< " ledger_outstanding=" << static_cast<qulonglong>(ledger->outstanding()) << ' ';
					ledgerFacts(aStream);
				});
			}
			const auto actions = std::array{
				window->read_action(),
				window->accept_action(),
				window->accept_action(),
				window->exclude_action(),
				window->exclude_action(),
				window->accept_action(),
			};
			if (probe.Gesture < actions.size()) {
				auto* const action = actions[probe.Gesture];
				if (action == nullptr || !action->isEnabled()) {
					return fail();
				}
				++probe.Gesture;
				if (action == window->accept_action()) {
					// [[BPR-1507]]. Exercise the normal one-click presenter through
					// the same exact command, durable host and published receipt.
					auto* const button = window->findChild<QToolButton*>(QStringLiteral("workbenchAcceptFileButton"));
					if (button == nullptr || !button->isVisible() || !button->isEnabled()
						|| button->defaultAction() != action || button->menu() != nullptr) {
						return fail();
					}
					// [[BPR-1515]]. Accept uses the actual composed window's key;
					// the inverse stays a deliberate one-click gesture.
					// [[BPR-1531]]. The key is Option-A (Alt+A). On macOS Cocoa
					// offers it to the shortcut map as the A key with Option and
					// the text it types, "å", before the text input system.
					window->activateWindow();
					button->setFocus();
					QCoreApplication::processEvents();
					const auto acceptKey = [window] {
	#if defined(Q_OS_MACOS)
						const auto typed = QStringLiteral("å");
						if (QWindowSystemInterface::handleShortcutEvent(window->windowHandle(), 0UL, Qt::Key_A, Qt::AltModifier, 0U, 0U, 0U, typed)) {
							QCoreApplication::processEvents();
							return;
						}
	#else
						const auto typed = QString{ };
	#endif
						for (const auto type : { QEvent::KeyPress, QEvent::KeyRelease }) {
							QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(
								window->windowHandle(), type, Qt::Key_A, Qt::AltModifier, typed);
							QCoreApplication::processEvents();
						}
					};
					if (ledger->retained().Accepted) {
						const auto commands = ledger->statistics().Commands;
						acceptKey();
						acceptKey();
						if (ledger->statistics().Commands != commands || !ledger->retained().Accepted || !action->shortcut().isEmpty()) {
							return fail();
						}
						QTextStream{ stdout } << "REVIEW_LEDGER_ACCEPT_SHORTCUT_REPEAT_OK\n";
						button->click();
					} else {
						if (action->shortcut() != QKeySequence{ QStringLiteral("Alt+A") }) {
							return fail();
						}
						acceptKey();
					}
				} else {
					action->trigger();
				}
				return result::pending;
			}
			const auto  after   = ledger->statistics();
			const auto  receipt = ledger->retained();
			const auto& before  = probe.GesturesBefore;
			probe.Evidence.GestureSequence
				= after.Commands == before.Commands + actions.size()
				&& after.PersistenceRequests
					== before.PersistenceRequests + actions.size()
				&& after.Completions == before.Completions + actions.size()
				&& after.StoreFailures == before.StoreFailures
				&& receipt.File == probe.File
				&& receipt.Material == probe.Material
				&& receipt.Read == core::file_read_state::read
				&& receipt.Accepted && !receipt.Excluded;
			probe.Evidence.ProgressMark    = progressMark();
			const auto* const defectThread = implementation->accepted_thread(
				review_ledger_probe_thread_identity());
			if (!probe.Evidence.GestureSequence || defectThread == nullptr) {
				return fail();
			}
			const auto note = review_ledger_probe_note(
				probe.File, probe.Comparison);
			probe.PrivateDefect = review_ledger_probe_defect(
				probe.File, probe.Comparison, *defectThread);
			if (!probe.PrivateDefect.has_value()
				|| !review_ledger_probe_defect_match_is_non_vacuous(
					probe.File, probe.Comparison, *defectThread)) {
				return fail();
			}
			const auto answered = [&probe](const bool aAccepted) {
				++probe.PrivateAnswers;
				probe.PrivateAccepted = probe.PrivateAccepted && aAccepted;
			};
			ledger->persist(note, std::nullopt, implementation->ledger_answer([&probe, answered](const std::expected<application::local_note_record, application::local_note_failure>& aStored) {
				if (aStored.has_value()) {
					probe.AcceptedNote = *aStored;
				}
				answered(aStored.has_value());
			}));
			ledger->persist_last_reviewed_head(implementation->LastAcceptedHandoff->Head, implementation->ledger_answer([answered](const std::expected<void, application::ledger_store_failure>& aStored) {
				answered(aStored.has_value());
			}));
			ledger->persist(*probe.PrivateDefect, implementation->ledger_answer([answered](const std::expected<void, application::ledger_store_failure>& aStored) {
				answered(aStored.has_value());
			}));
			return advance(state_type::phase::awaiting_private_records);
		}

		if (probe.Phase == state_type::phase::awaiting_private_records) {
			if (probe.PrivateAnswers < 3U || !ledger->is_quiescent()) {
				return wait("REVIEW_LEDGER_PRIVATE_RECORDS_STALLED", [&](QTextStream& aStream) {
					aStream << "private_answers=" << static_cast<qulonglong>(probe.PrivateAnswers) << ' ';
					ledgerFacts(aStream);
				});
			}
			const auto* const defectThread = implementation->accepted_thread(
				review_ledger_probe_thread_identity());
			const auto& head              = implementation->LastAcceptedHandoff->Head;
			const auto  privateState      = ledger->private_observation();
			probe.Evidence.PrivateRecords = probe.PrivateAccepted && probe.AcceptedNote.has_value()
				&& defectThread != nullptr && privateState.has_value()
				&& privateState->Notes == std::vector{ *probe.AcceptedNote }
				&& privateState->LastReviewedHead == std::optional{ head }
				&& privateState->Defects == std::vector{ *probe.PrivateDefect }
				&& review_ledger_probe_defect_matches_thread(
												privateState->Defects.front(),
												probe.File,
												probe.Comparison,
												*defectThread);
			probe.Evidence.DurableReceipt               = true;
			implementation->ReviewLedgerRoundTripPassed = true;
			if (!probe.Evidence.PrivateRecords) {
				return fail();
			}
			return advance(state_type::phase::awaiting_actionability);
		}

		if (probe.Phase == state_type::phase::awaiting_actionability) {
			if (host->outstanding_requests() != 0U || !ledger->is_quiescent()) {
				// The gestures' persistence and the actionability it projects
				// reach the host as requests; the tail prints how many remain.
				return wait("REVIEW_LEDGER_ACTIONABILITY_STALLED", [&](QTextStream& aStream) {
					aStream << "actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " host_selected=" << present(host->selected().has_value())
							<< ' ';
					ledgerFacts(aStream);
				});
			}
			const auto&       actionable        = host->actionable();
			const auto* const comparison        = window->comparison_surface();
			const auto        selectionRetained = host->selected() == probe.File
				&& implementation->LastSelectedDiff.has_value()
				&& comparison != nullptr && comparison->current_result() != nullptr
				&& comparison->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key;
			if (aCourse == review_ledger_composition_probe::seed) {
				if (!actionable.empty() || !selectionRetained) {
					return fail();
				}
				auto materialBytes   = probe.Material.bytes();
				materialBytes.back() = static_cast<std::uint8_t>(
					materialBytes.back() ^ 0x01U);
				probe.ReopenedMaterial = core::file_material_id{ materialBytes };
				ledger->observe_material(core::file_material_observation{
					.File     = probe.File,
					.Material = probe.ReopenedMaterial,
				});
				return advance(state_type::phase::awaiting_material_actionability);
			}
			probe.Evidence.ActionabilityProjection = actionable.size() == 1U
				&& actionable.front() == probe.File && selectionRetained;
			if (!probe.Evidence.ActionabilityProjection) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		if (probe.Phase == state_type::phase::awaiting_material_actionability) {
			if (host->outstanding_requests() != 0U || !ledger->is_quiescent()) {
				// The new material's observation makes the file actionable
				// again through the host.
				return wait("REVIEW_LEDGER_MATERIAL_ACTIONABILITY_STALLED", [&](QTextStream& aStream) {
					const auto& receipt = ledger->retained();
					aStream << "actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " host_selected=" << present(host->selected().has_value())
							<< " receipt_material_reopened=" << truth(receipt.Material == probe.ReopenedMaterial)
							<< ' ';
					ledgerFacts(aStream);
				});
			}
			const auto& actionable = host->actionable();
			const auto& receipt    = ledger->retained();
			const auto* comparison = window->comparison_surface();
			probe.Evidence.ActionabilityProjection
				= actionable.size() == 1U && actionable.front() == probe.File
				&& host->selected() == probe.File
				&& implementation->LastSelectedDiff.has_value()
				&& comparison != nullptr && comparison->current_result() != nullptr
				&& comparison->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key
				&& receipt.File == probe.File
				&& receipt.Material == probe.ReopenedMaterial
				&& receipt.Read == core::file_read_state::unread
				&& !receipt.Accepted && !receipt.Excluded;
			if (!probe.Evidence.ActionabilityProjection) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::review_ledger_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_ledger_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->ReviewLedgerCompositionProbe.has_value()
			? implementation->ReviewLedgerCompositionProbe->Evidence
			: detail::review_ledger_composition_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::exercise_review_round_trip_composition(
		github_application& aApplication)
		-> detail::review_round_trip_composition_probe_result
	{
		using result               = detail::review_round_trip_composition_probe_result;
		using state_type           = review_round_trip_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReviewRoundTripCompositionProbe.has_value()) {
			implementation->ReviewRoundTripCompositionProbe.emplace();
			// [[BPR-1735]]. This course's off-hunk line exercises RFC-BPR-0022's
			// native attempt and fallback, its subject, so it places as a build
			// before the hunk rule did; the BPR-1735 courses cover the rule.
			implementation->WholeDiffPlacementDisabled = true;
		}
		auto& probe = *implementation->ReviewRoundTripCompositionProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		// [[BPR-1037]]. A failure gives back the decision gate the course may
		// be holding. A held gate keeps the lost publication outstanding, so
		// the run's shutdown would wait for it until the harness killed the
		// process, and the verdict would never print.
		const auto fail = [&probe, implementation] {
			const auto counts = implementation->Fixture ? implementation->Fixture->review_mutation_evidence() : detail::review_mutation_fixture_evidence{ };
			QTextStream{ stdout } << "SEMANTIC_COMMENT_PROBE_FAILED phase=" << static_cast<int>(probe.Phase)
								  << " submissions=" << counts.Submissions << " native=" << counts.NativePosts << " file=" << counts.FilePosts
								  << " stored=" << counts.StoredComments << " duplicate=" << counts.DuplicateReturned << " durable=" << counts.DurableJournalReads
								  << " sessions=" << counts.SessionConstructions << " draft=" << (implementation->ReviewDraft ? static_cast<int>(implementation->ReviewDraft->State) : -1) << '\n';

			probe.Phase = state_type::phase::failed;
			if (implementation->ReviewMutationDecisionGate != nullptr) {
				implementation->ReviewMutationDecisionGate->release();
			}
			return result::failed;
		};
		// [[BPR-1037]]. Every phase that waits is bounded and names what it
		// was waiting for; see `keep_waiting`.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			// [[BPR-1854]]. Native Ubuntu exceeded five seconds in distinct actor waits.
			// This functional course keeps its outer 40s
			// budget and all semantic predicates; only its phase watchdog grows.
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts, std::chrono::milliseconds{ 15'000 }) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto draftState = [implementation] {
			if (!implementation->ReviewDraft.has_value()) {
				return QStringLiteral("absent");
			}
			const auto state = core::name(implementation->ReviewDraft->State);
			return QString::fromUtf8(state.data(), static_cast<qsizetype>(state.size()));
		};
		const auto phaseName = [](const state_type::phase aPhase) {
			switch (aPhase) {
			case state_type::phase::awaiting_edited_body:    return "awaiting_edited_body";
			case state_type::phase::awaiting_fallback_draft: return "awaiting_fallback_draft";
			case state_type::phase::awaiting_fallback_body:  return "awaiting_fallback_body";
			case state_type::phase::awaiting_editor:
				return "awaiting_editor";
			case state_type::phase::awaiting_stale_refusal:
				return "awaiting_stale_refusal";
			case state_type::phase::awaiting_lost_effect:
				return "awaiting_lost_effect";
			case state_type::phase::awaiting_deferred_terminal:
				return "awaiting_deferred_terminal";
			case state_type::phase::awaiting_retry:
				return "awaiting_retry";
			case state_type::phase::awaiting_native_conversation:
				return "awaiting_native_conversation";
			case state_type::phase::awaiting_fallback_conversation:
				return "awaiting_fallback_conversation";
			case state_type::phase::passed:
				return "passed";
			case state_type::phase::failed:
				return "failed";
			}
			return "unknown";
		};
		// The file gesture composes the editor's draft once the selected
		// diff is published; every phase needs both.
		const auto draftFacts = [&](QTextStream& aStream) {
			const auto* const view = implementation->Window == nullptr || implementation->Window->files_drawer() == nullptr
				? nullptr
				: implementation->Window->files_drawer()->contents_view();
			aStream << "phase=" << phaseName(probe.Phase)
					<< " fixture=" << present(implementation->Fixture != nullptr)
					<< " window=" << present(implementation->Window != nullptr)
					<< " workbench_host=" << present(implementation->WorkbenchHost != nullptr)
					<< " mutation_host=" << present(implementation->ReviewMutationHost != nullptr)
					<< " review_draft=" << draftState()
					<< " off_hunk_anchor=" << present(probe.OffHunkAnchor.has_value())
					<< " private_ready=" << present(implementation->PrivateDraftsReady)
					<< " private_selection_pending=" << present(implementation->PrivateDraftSelectionPending)
					<< " semantic_selection=" << present(implementation->SelectedSemanticSelection.has_value())
					<< " pending_reply=" << present(implementation->PendingPrivateReply.has_value())
					<< " accepted_handoff=" << present(implementation->LastAcceptedHandoff.has_value())
					<< " last_selected_diff=" << present(implementation->LastSelectedDiff.has_value())
					<< " review_editor=" << present(implementation->Window != nullptr && implementation->Window->review_editor() != nullptr)
					<< " file_rows=" << (view == nullptr || view->model() == nullptr ? 0 : view->model()->rowCount())
					<< " current_row=" << (view == nullptr ? -1 : view->currentIndex().row())
					<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
					<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures);
		};
		if (probe.Phase == state_type::phase::awaiting_editor && implementation->Window && implementation->LastSelectedDiff) {
			implementation->Window->choose_modality(qt::review_modality::review);
			auto* comparison = implementation->Window->comparison_surface();
			if (comparison->current_semantic_index() && !comparison->geometry_pending()
				&& (!comparison->selection() || comparison->selection()->PreferredLine.Side != core::diff_side::right)) {
				(void)comparison->select_conversation_line(core::diff_side::right, 0);
			}
		}
		// A successful retry can retire the selected draft while the server refresh
		// is still reaching this probe. Conversation checks use the accepted server
		// snapshot; they do not need the editor's former draft selection.
		const auto needsDraft = probe.Phase != state_type::phase::awaiting_retry
			&& probe.Phase != state_type::phase::awaiting_native_conversation
			&& probe.Phase != state_type::phase::awaiting_fallback_draft
			&& probe.Phase != state_type::phase::awaiting_fallback_conversation;
		if (implementation->Fixture != nullptr && implementation->Window != nullptr
			&& implementation->WorkbenchHost != nullptr
			&& implementation->ReviewMutationHost != nullptr
			&& needsDraft && !implementation->ReviewDraft.has_value()
			&& implementation->WorkbenchHost->outstanding_requests() == 0U) {
			auto* const view = implementation->Window->files_drawer()->contents_view();
			if (view != nullptr && view->model() != nullptr
				&& view->model()->rowCount() > 0
				&& !view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
			}
			return wait("REVIEW_ROUND_TRIP_DRAFT_STALLED", draftFacts);
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->ReviewMutationHost == nullptr
			|| (needsDraft && !implementation->ReviewDraft.has_value())
			|| !implementation->LastAcceptedHandoff.has_value()
			|| !implementation->LastSelectedDiff.has_value()
			|| implementation->Window->review_editor() == nullptr) {
			return wait("REVIEW_ROUND_TRIP_DRAFT_STALLED", draftFacts);
		}
		auto* const editor     = implementation->Window->review_editor();
		const auto  designated = github_application::implementation::review_mutation_target(
			*implementation->LastAcceptedHandoff);
		const auto clickRightLine = [implementation](const std::uint32_t aLine) {
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || comparison->right_pane() == nullptr) {
				return false;
			}
			auto* const pane  = comparison->right_pane();
			const auto  block = pane->document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid()) {
				return false;
			}
			pane->setTextCursor(QTextCursor{ block });
			pane->ensureCursorVisible();
			const auto point = pane->cursorRect(QTextCursor{ block }).center();
			auto       press = QMouseEvent{
				QEvent::MouseButtonPress,
				QPointF{ point },
				QPointF{ point },
				QPointF{ point },
				Qt::LeftButton,
				Qt::LeftButton,
				Qt::NoModifier,
			};
			auto release = QMouseEvent{
				QEvent::MouseButtonRelease,
				QPointF{ point },
				QPointF{ point },
				QPointF{ point },
				Qt::LeftButton,
				Qt::NoButton,
				Qt::NoModifier,
			};
			QCoreApplication::sendEvent(pane->viewport(), &press);
			QCoreApplication::sendEvent(pane->viewport(), &release);
			return true;
		};

		if (probe.Phase == state_type::phase::awaiting_native_conversation
			|| probe.Phase == state_type::phase::awaiting_fallback_conversation) {
			const bool fallback = probe.Phase == state_type::phase::awaiting_fallback_conversation;
			const auto id       = fallback ? "review-comment:PRRC_composed_2" : "review-comment:PRRC_composed_1";
			const auto index    = implementation->Window->conversation_model()->index_for_id(id);
			const auto evidence = implementation->Fixture->review_mutation_evidence();
			if (!index.isValid() || implementation->Window->conversation_model()->id_for_index(implementation->Window->conversation_view()->currentIndex()) != id
				|| implementation->PendingPublishedConversation) {
				return wait("SEMANTIC_COMMENT_SERVER_REFRESH_STALLED", [&](QTextStream& aStream) {
					aStream << "fallback=" << fallback << " post=" << index.isValid() << " pending_selection=" << implementation->PendingPublishedConversation.has_value()
							<< " native_posts=" << evidence.NativePosts << " file_posts=" << evidence.FilePosts << " stored=" << evidence.StoredComments
							<< " snapshot=" << implementation->WindowRead->accepted_snapshot().has_value();
				});
			}
			if (fallback) {
				const auto  anchor   = implementation->Window->conversation_model()->semantic_anchor(index);
				const auto& snapshot = *implementation->WindowRead->accepted_snapshot();
				const auto  root     = std::ranges::find_if(snapshot.ReviewThreads, [](const auto& aThread) { return aThread.Thread.value() == "PRRT_composed_2"; });
				QTextStream{ stdout } << "SEMANTIC_COMMENT_READBACK anchor=" << anchor.has_value() << " exact=" << (anchor == probe.OffHunkAnchor)
									  << " source=" << implementation->conversation_source().has_value() << " root=" << (root != snapshot.ReviewThreads.end())
									  << " body=" << (index.data(qt::github_conversation_model::body_role).toString() == QStringLiteral("BPR-0571 off-hunk publication.")) << '\n';
				if (evidence.NativePosts != 2U || evidence.FilePosts != 1U || evidence.StoredComments != 2U
					|| evidence.SessionConstructions != 2U || evidence.DurableJournalReads != 3U
					|| anchor != probe.OffHunkAnchor || root == snapshot.ReviewThreads.end()
					|| root->Subject != application::github_review_thread_subject::file || root->Line
					|| index.data(qt::github_conversation_model::body_role).toString() != QStringLiteral("BPR-0571 off-hunk publication.")) {
					return fail();
				}
				QTextStream{ stdout } << "SEMANTIC_COMMENT_COMPOSITION_OK:native-posts=2,file-posts=1,stored=2,journal-reopens=3,session-constructions=2\n";
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			if (evidence.NativePosts != 1U || evidence.FilePosts != 0U || evidence.DurableJournalReads != 2U
				|| evidence.SessionConstructions != 2U || implementation->Window->conversation_model()->semantic_anchor(index)) {
				return fail();
			}
			probe.Evidence.RetryIdempotent = evidence.Submissions == 2U && evidence.StoredComments == 1U && evidence.DuplicateReturned;
			const auto& observed           = *implementation->WindowRead->accepted_snapshot();
			const auto  native             = std::ranges::find_if(observed.ReviewThreads, [](const auto& aThread) { return aThread.Thread.value() == "PRRT_composed_1"; });
			probe.Evidence.ServerAuthority = native != observed.ReviewThreads.end()
				&& native->Subject == application::github_review_thread_subject::line && native->Comments.size() == 1U
				&& native->Comments.front().Body == "BPR-0229 fixture publication.";
			probe.Evidence.TypedQtProjection = index.data(qt::github_conversation_model::body_role).toString() == QStringLiteral("BPR-0229 fixture publication.")
				&& !implementation->Window->conversation_model()->semantic_anchor(index);
			if (!probe.Evidence.RetryIdempotent || !probe.Evidence.ServerAuthority || !probe.Evidence.TypedQtProjection) {
				return fail();
			}
			implementation->Window->choose_modality(qt::review_modality::review);
			auto* const       comparison = implementation->Window->comparison_surface();
			const auto* const left       = comparison->left_pane()->document();
			const auto* const right      = comparison->right_pane()->document();
			// An unchanged line with four unchanged neighbours on each side is
			// outside the synthetic provider's three-line patch context.
			for (int line = 4; line + 4 < std::min(left->blockCount(), right->blockCount()); ++line) {
				bool unchanged = true;
				for (int neighbour = line - 4; neighbour <= line + 4; ++neighbour) {
					unchanged = unchanged && left->findBlockByNumber(neighbour).text() == right->findBlockByNumber(neighbour).text();
				}
				if (!unchanged || right->findBlockByNumber(line).text().isEmpty()) {
					continue;
				}
				if (!clickRightLine(static_cast<std::uint32_t>(line))
					|| !comparison->select_conversation_line(core::diff_side::right, static_cast<std::uint32_t>(line))
					|| !comparison->selection()
					|| comparison->selection()->PreferredLine.Side != core::diff_side::right) {
					return fail();
				}
				probe.OffHunkAnchor = comparison->selection()->PreferredLine;
				break;
			}
			// A refreshed comparison can still be arriving after its conversation.
			// Stay here until the public gesture actually selected an off-hunk line.
			if (!probe.OffHunkAnchor) {
				return wait("REVIEW_ROUND_TRIP_OFF_HUNK_LINE_STALLED", [&](QTextStream& aStream) {
					aStream << "left_blocks=" << left->blockCount() << " right_blocks=" << right->blockCount()
							<< " result=" << present(comparison->current_result() != nullptr)
							<< " geometry_pending=" << comparison->geometry_pending();
				});
			}
			return advance(state_type::phase::awaiting_fallback_draft);
		}
		if (probe.Phase == state_type::phase::awaiting_fallback_draft) {
			auto* const files = implementation->Window->files_drawer()->contents_view();
			if (files && files->model() && files->model()->rowCount() > 0 && !files->currentIndex().isValid()) {
				files->setCurrentIndex(files->model()->index(0, 0));
				return wait("REVIEW_ROUND_TRIP_FALLBACK_DRAFT_STALLED", draftFacts);
			}
			auto* const comparison = implementation->Window->comparison_surface();
			if (probe.OffHunkAnchor && (!comparison->selection() || comparison->selection()->PreferredLine != *probe.OffHunkAnchor)) {
				(void)comparison->select_conversation_line(core::diff_side::right, probe.OffHunkAnchor->Line);
			}
			if (!probe.OffHunkAnchor || !editor->shown_draft() || editor->shown_draft()->Anchor != probe.OffHunkAnchor
				|| !implementation->ReviewDraft || implementation->ReviewDraft->State != core::review_draft_state::drafting) {
				return wait("REVIEW_ROUND_TRIP_FALLBACK_DRAFT_STALLED", draftFacts);
			}
			editor->body_editor()->setPlainText(QStringLiteral("BPR-0571 off-hunk publication."));
			return advance(state_type::phase::awaiting_fallback_body);
		}
		if (probe.Phase == state_type::phase::awaiting_fallback_body) {
			if (implementation->ReviewDraft->Body != "BPR-0571 off-hunk publication." || !editor->publish_action()->isEnabled()) {
				return wait("REVIEW_ROUND_TRIP_FALLBACK_BODY_STALLED", draftFacts);
			}
			// [[BPR-1277]]. An epoch refresh asks again for the selected file
			// while the reviewer writes; the draft, its body and its line stay.
			if (!probe.SameDiffAskedAgain) {
				probe.SameDiffAskedAgain   = true;
				probe.SameDiffBegins       = implementation->SelectedDiffBegins;
				probe.SameDiffPublications = implementation->SelectedDiffPublications;
				implementation->WorkbenchHost->refresh_selected_diff();
				return wait("REVIEW_ROUND_TRIP_SAME_DIFF_STALLED", draftFacts);
			}
			if (implementation->SelectedDiffBegins == probe.SameDiffBegins
				|| implementation->SelectedDiffPublications == probe.SameDiffPublications
				|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
				return wait("REVIEW_ROUND_TRIP_SAME_DIFF_STALLED", draftFacts);
			}
			const auto* const kept = implementation->Window->comparison_surface();
			if (!editor->shown_draft() || editor->shown_draft()->Anchor != probe.OffHunkAnchor
				|| !kept->selection() || kept->selection()->PreferredLine != *probe.OffHunkAnchor) {
				QTextStream{ stdout } << "REVIEW_ROUND_TRIP_SAME_DIFF_DROPPED_DRAFT\n";
				return fail();
			}
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_fallback_conversation);
		}

		// The draft is anchored where the comparison's selection is, and the
		// course needs it on the right side.
		const auto selectionFacts = [&](QTextStream& aStream) {
			const auto* const comparison = implementation->Window->comparison_surface();
			const auto        selection  = comparison == nullptr ? std::optional<core::semantic_selection>{ } : comparison->selection();
			aStream << "comparison=" << present(comparison != nullptr)
					<< " selection=" << present(selection.has_value())
					<< " selection_side="
					<< (!selection.has_value()                                        ? "none"
							: selection->PreferredLine.Side == core::diff_side::right ? "right"
																					  : "left")
					<< " right_pane=" << present(comparison != nullptr && comparison->right_pane() != nullptr)
					<< " shown_draft=" << present(editor->shown_draft().has_value())
					<< " review_draft=" << draftState();
		};

		if (probe.Phase == state_type::phase::awaiting_editor) {
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || !comparison->selection().has_value()) {
				return wait("REVIEW_ROUND_TRIP_EDITOR_SELECTION_STALLED", selectionFacts);
			}
			if (comparison->selection()->PreferredLine.Side
				!= core::diff_side::right) {
				auto* const pane  = comparison->right_pane();
				const auto  block = pane->document()->findBlockByNumber(0);
				const auto  point = pane->cursorRect(QTextCursor{ block }).center();
				auto        press = QMouseEvent{
					QEvent::MouseButtonPress,
					QPointF{ point },
					QPointF{ point },
					QPointF{ point },
					Qt::LeftButton,
					Qt::LeftButton,
					Qt::NoModifier,
				};
				auto release = QMouseEvent{
					QEvent::MouseButtonRelease,
					QPointF{ point },
					QPointF{ point },
					QPointF{ point },
					Qt::LeftButton,
					Qt::NoButton,
					Qt::NoModifier,
				};
				QCoreApplication::sendEvent(pane->viewport(), &press);
				QCoreApplication::sendEvent(pane->viewport(), &release);
				return wait("REVIEW_ROUND_TRIP_EDITOR_SELECTION_STALLED", selectionFacts);
			}
			if (!probe.NativeLineChosen) {
				const auto* left  = comparison->left_pane()->document();
				const auto* right = comparison->right_pane()->document();
				for (int line = 0; line < std::min(left->blockCount(), right->blockCount()); ++line) {
					if (left->findBlockByNumber(line).text() != right->findBlockByNumber(line).text()
						&& !right->findBlockByNumber(line).text().isEmpty()) {
						probe.NativeLineChosen = clickRightLine(static_cast<std::uint32_t>(line));
						break;
					}
				}
				if (!probe.NativeLineChosen) {
					return fail();
				}
			}
			if (!editor->shown_draft() || editor->shown_draft()->Anchor != comparison->selection()->PreferredLine) {
				return wait("REVIEW_ROUND_TRIP_SELECTED_DRAFT_STALLED", draftFacts);
			}
			probe.Evidence.ExactDesignatedTarget
				= implementation->ReviewMutationHost->designated() == designated;
			probe.Evidence.EditorComposed = editor->shown_draft().has_value()
				&& editor->shown_draft()->Anchor
					== comparison->selection()->PreferredLine
				&& editor->shown_draft()->Anchor.Side == core::diff_side::right
				&& editor->shown_draft()->Anchor.Comparison
					== core::comparison_digest(
						implementation->LastSelectedDiff->Key)
				&& editor->target_label()->text().contains(
					QString::number(implementation->LastAcceptedHandoff->Number.value()));
			probe.Evidence.CapabilityFallback
				= !implementation->ReviewMutationHost->capabilities().ResolvesThreads
				&& !implementation->ReviewMutationHost->capabilities()
						.RecordsViewedState
				&& editor->mutation_status_text().contains(
					QStringLiteral("unavailable"));
			if (!probe.Evidence.ExactDesignatedTarget
				|| !probe.Evidence.EditorComposed
				|| !probe.Evidence.CapabilityFallback) {
				return fail();
			}

			probe.DraftIntents       = editor->statistics().DraftIntents;
			probe.PriorDraftRevision = implementation->ReviewDraftRevision;
			editor->body_editor()->setPlainText(
				QStringLiteral("BPR-0229 fixture publication."));
			return advance(state_type::phase::awaiting_edited_body);
		}
		if (probe.Phase == state_type::phase::awaiting_edited_body) {
			if (implementation->ReviewDraft->Body != "BPR-0229 fixture publication." || !editor->publish_action()->isEnabled()) {
				return wait("REVIEW_ROUND_TRIP_EDIT_ACK_STALLED", draftFacts);
			}
			probe.Evidence.SemanticEdit
				= editor->statistics().DraftIntents == probe.DraftIntents + 1U
				&& implementation->ReviewDraft->Body
					== "BPR-0229 fixture publication.";
			if (!probe.Evidence.SemanticEdit) {
				return fail();
			}

			// Stale revision and foreign scope stop at draft admission, before
			// the real mutation host receives any publication request.
			const auto beforeDraftRefusals = implementation->ReviewMutationHost->statistics();
			const auto acceptedRevision    = implementation->ReviewDraftRevision;
			// [[BPR-0822]]. Named nested types, as in the shared draft edit.
			implementation->dispatch_review_draft(core::revisioned_review_draft_command{
				.Command          = core::review_draft_command{ .Id = implementation->ReviewDraft->Id, .Kind = core::review_draft_command_kind::begin_publish },
				.ExpectedRevision = probe.PriorDraftRevision,
				.Scope            = implementation->DraftScope });
			implementation->dispatch_review_draft(core::revisioned_review_draft_command{
				.Command          = core::review_draft_command{ .Id = implementation->ReviewDraft->Id, .Kind = core::review_draft_command_kind::begin_publish },
				.ExpectedRevision = acceptedRevision,
				.Scope            = core::review_draft_scope_id{ implementation->DraftScope.value() + 1U } });
			if (implementation->ReviewMutationHost->statistics() != beforeDraftRefusals
				|| implementation->ReviewDraftRevision != acceptedRevision
				|| implementation->ReviewDraft->State != core::review_draft_state::drafting
				|| implementation->ReviewDraft->Body != "BPR-0229 fixture publication.") {
				return fail();
			}

			auto       moved      = designated;
			const auto secondHead = core::parse_git_commit_oid(
				implementation->Fixture->evidence().HeadTwoOid);
			if (!secondHead.has_value() || *secondHead == designated.ExpectedHead) {
				return fail();
			}
			moved.ExpectedHead = *secondHead;
			implementation->Fixture->report_review_mutation_target(moved);
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_stale_refusal);
		}

		if (probe.Phase == state_type::phase::awaiting_stale_refusal) {
			if (implementation->PendingReviewMutation || implementation->PrivateDraftCommandPending) {
				return wait("REVIEW_ROUND_TRIP_STALE_TERMINAL_STALLED", draftFacts);
			}
			const auto provider = implementation->Fixture->review_mutation_evidence();
			probe.Evidence.StaleHeadRefused
				= implementation->ReviewDraft->State
					== core::review_draft_state::drafting
				&& provider.Submissions == 0U
				&& editor->mutation_status_text().contains(QStringLiteral("head moved"));
			if (!probe.Evidence.StaleHeadRefused) {
				return fail();
			}
			const auto* const comparison
				= implementation->Window->comparison_surface();
			if (comparison == nullptr || !comparison->selection().has_value()
				|| implementation->ReviewMutationDecisionGate == nullptr) {
				return fail();
			}
			probe.PublishingSelection = comparison->selection();
			probe.PublishingDraft     = implementation->ReviewDraft;
			implementation->Fixture->report_review_mutation_target(designated);
			implementation->ReviewMutationDecisionGate->hold();
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_lost_effect);
		}

		if (probe.Phase == state_type::phase::awaiting_lost_effect) {
			const auto lost = implementation->Fixture->review_mutation_evidence();
			if (!lost.LostResult || lost.StoredComments != 1U
				|| implementation->ReviewMutationDecisionGate == nullptr
				|| implementation->ReviewMutationDecisionGate->pending() == 0U) {
				// The provider stores the comment and loses its answer, and the
				// held decision gate keeps the terminal back.
				return wait("REVIEW_ROUND_TRIP_LOST_EFFECT_STALLED", [&](QTextStream& aStream) {
					const auto* const gate = implementation->ReviewMutationDecisionGate.get();
					aStream << "lost_result=" << (lost.LostResult ? "true" : "false")
							<< " stored_comments=" << static_cast<qulonglong>(lost.StoredComments)
							<< " expected_stored_comments=1"
							<< " submissions=" << static_cast<qulonglong>(lost.Submissions)
							<< " decision_gate=" << present(gate != nullptr)
							<< " gate_pending=" << static_cast<qulonglong>(gate == nullptr ? 0U : gate->pending())
							<< " review_draft=" << draftState()
							<< " pending_mutation=" << present(implementation->PendingReviewMutation.has_value())
							<< " mutation_outstanding=" << static_cast<qulonglong>(implementation->ReviewMutationHost->outstanding_requests());
				});
			}
			if (!probe.PublishingSelection.has_value()
				|| !implementation->PendingReviewMutation.has_value()
				|| !implementation->ReviewDraft.has_value()
				|| implementation->ReviewDraft->State
					!= core::review_draft_state::publishing) {
				return fail();
			}
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || comparison->right_pane() == nullptr) {
				return fail();
			}
			for (auto line = 0; line < comparison->right_pane()->document()->blockCount();
				 ++line) {
				if (static_cast<std::uint32_t>(line)
					== probe.PublishingSelection->PreferredLine.Line) {
					continue;
				}
				if (!clickRightLine(static_cast<std::uint32_t>(line))) {
					continue;
				}
				if (comparison->selection().has_value()
					&& comparison->selection()->PreferredLine.Side
						== core::diff_side::right
					&& comparison->selection()->PreferredLine
						!= probe.PublishingSelection->PreferredLine) {
					probe.DeferredSelection = comparison->selection();
					break;
				}
			}
			const auto intermediateDeferred
				= probe.DeferredSelection.has_value()
				&& implementation->DeferredReviewSelection.has_value()
				&& *implementation->DeferredReviewSelection
					== probe.DeferredSelection
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& editor->shown_draft() == implementation->ReviewDraft;
			if (!intermediateDeferred
				|| !clickRightLine(probe.PublishingSelection->PreferredLine.Line)) {
				return fail();
			}
			probe.Evidence.SelectionDeferred = true;
			probe.Evidence.LatestSelectionWins
				= comparison->selection() == probe.PublishingSelection
				&& implementation->DeferredReviewSelection.has_value()
				&& *implementation->DeferredReviewSelection
					== probe.PublishingSelection
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& implementation->PendingReviewMutation.has_value()
				&& probe.PublishingDraft.has_value()
				&& implementation->ReviewDraft->Id == probe.PublishingDraft->Id
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& implementation->ReviewDraft->State
					== core::review_draft_state::publishing;
			if (!probe.Evidence.LatestSelectionWins) {
				return fail();
			}
			implementation->ReviewMutationDecisionGate->release();
			return advance(state_type::phase::awaiting_deferred_terminal);
		}

		if (implementation->ReviewMutationHost->outstanding_requests() != 0U
			|| implementation->PendingReviewMutation || implementation->PrivateDraftCommandPending) {
			// The released or retried publication reaches its terminal.
			return wait(probe.Phase == state_type::phase::awaiting_retry
							? "REVIEW_ROUND_TRIP_RETRY_STALLED"
							: "REVIEW_ROUND_TRIP_DEFERRED_TERMINAL_STALLED",
						[&](QTextStream& aStream) {
							const auto provider   = implementation->Fixture->review_mutation_evidence();
							const auto statistics = implementation->ReviewMutationHost->statistics();
							aStream << "phase=" << phaseName(probe.Phase)
									<< " mutation_outstanding=" << static_cast<qulonglong>(implementation->ReviewMutationHost->outstanding_requests())
									<< " review_draft=" << draftState()
									<< " pending_mutation=" << present(implementation->PendingReviewMutation.has_value())
									<< " effects_queued=" << static_cast<qulonglong>(statistics.EffectsQueued)
									<< " terminals_published=" << static_cast<qulonglong>(statistics.TerminalsPublished)
									<< " submissions=" << static_cast<qulonglong>(provider.Submissions)
									<< " stored_comments=" << static_cast<qulonglong>(provider.StoredComments);
						});
		}

		if (probe.Phase == state_type::phase::awaiting_deferred_terminal) {
			const auto        lost = implementation->Fixture->review_mutation_evidence();
			const auto* const comparison
				= implementation->Window->comparison_surface();
			auto retryIsolation = false;
			if (probe.PublishingSelection.has_value()
				&& probe.DeferredSelection.has_value()
				&& implementation->RetryableReviewDraft.has_value()
				&& implementation->SelectedDiffPublication.has_value()) {
				auto foreign     = implementation->RetryableReviewDraft->Authority;
				foreign.Snapshot = core::provider_snapshot_id{
					foreign.Snapshot.value() + 1U
				};
				retryIsolation
					= !github_application::implementation::exact_retryable_review_draft(
						  *implementation->RetryableReviewDraft,
						  *probe.DeferredSelection,
						  implementation->SelectedDiffPublication->Request.WorkbenchAuthority)
					&& !github_application::implementation::exact_retryable_review_draft(
						*implementation->RetryableReviewDraft,
						*probe.PublishingSelection,
						foreign);
			}
			probe.Evidence.LostResultRefreshed = lost.LostResult
				&& lost.StoredComments == 1U
				&& implementation->ReviewProjection
					== implementation->Fixture->review_comments();
			probe.Evidence.PendingCleared
				= !implementation->PendingReviewMutation.has_value();
			probe.Evidence.DeferredDraftExact
				= comparison != nullptr && probe.PublishingDraft.has_value()
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& comparison->selection() == probe.PublishingSelection
				&& implementation->ReviewDraft->State
					== core::review_draft_state::publishing
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& implementation->ReviewDraft->Id == probe.PublishingDraft->Id
				&& implementation->ReviewDraft->Body == probe.PublishingDraft->Body
				&& editor->shown_draft() == implementation->ReviewDraft
				&& implementation->RetryableReviewDraft.has_value()
				&& implementation->ReviewDraft
					== std::optional{ implementation->RetryableReviewDraft->Draft }
				&& retryIsolation;
			probe.Evidence.StaleTerminalWithheld
				= editor->mutation_status_text().contains(
					  QStringLiteral("unavailable"))
				&& !editor->mutation_status_text().contains(
					QStringLiteral("retry is safe"))
				&& !editor->mutation_status_text().contains(
					QStringLiteral("cancelled"));
			if (!probe.Evidence.LostResultRefreshed
				|| !probe.Evidence.PendingCleared
				|| !probe.Evidence.DeferredDraftExact
				|| !probe.Evidence.StaleTerminalWithheld
				|| !probe.PublishingSelection.has_value()) {
				return fail();
			}
			if (comparison == nullptr || !comparison->selection().has_value()
				|| comparison->selection()->PreferredLine
					!= probe.PublishingSelection->PreferredLine
				|| !implementation->ReviewDraft.has_value()
				|| !implementation->RetryableReviewDraft.has_value()
				|| *implementation->ReviewDraft
					!= implementation->RetryableReviewDraft->Draft) {
				return fail();
			}
			editor->body_editor()->setPlainText(
				QStringLiteral("BPR-0229 fixture publication."));
			if (implementation->ReviewDraft->Body
				!= "BPR-0229 fixture publication.") {
				return fail();
			}
			editor->publish_action()->trigger();
			return advance(state_type::phase::awaiting_retry);
		}

		// The receipt initiates a server refresh. That refresh can retire the
		// editor's publication state before this observer runs, so the result
		// is checked on the selected accepted conversation below.
		const auto provider = implementation->Fixture->review_mutation_evidence();
		if (provider.Submissions != 2U || provider.StoredComments != 1U || !provider.DuplicateReturned
			|| provider.NativePosts != 1U || provider.FilePosts != 0U) {
			return fail();
		}

		return advance(state_type::phase::awaiting_native_conversation);
	}

	namespace {
		/// [[BPR-1741]]. One post of the open thread, by identity.
		[[nodiscard]] auto post_view_of(const qt::conversation_cards_view& aCards, const std::string& aPost) -> QWidget*
		{
			if (aCards.open_card() == nullptr) {
				return nullptr;
			}
			for (auto* const post : aCards.open_card()->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
				if (post->property("postIdentity").toString().toStdString() == aPost) {
					return post;
				}
			}
			return nullptr;
		}

		/// [[BPR-1741]]. One reaction's chip under a post of the open thread.
		[[nodiscard]] auto reaction_chip_of(const qt::conversation_cards_view& aCards, const std::string& aPost, const core::reaction_content aContent) -> QAbstractButton*
		{
			auto* const post = post_view_of(aCards, aPost);
			if (post == nullptr) {
				return nullptr;
			}
			for (auto* const chip : post->findChildren<QAbstractButton*>(QStringLiteral("reactionChip"))) {
				if (chip->isVisible() && chip->property("reactionContent").toInt() == static_cast<int>(aContent)) {
					return chip;
				}
			}
			return nullptr;
		}
	} // namespace

	auto detail::github_application_fixture_access::exercise_threaded_reply(
		github_application& aApplication) -> detail::threaded_reply_probe_result
	{

		using result               = detail::threaded_reply_probe_result;
		using state_type           = threaded_reply_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ThreadedReplyProbe.has_value()) {
			implementation->ThreadedReplyProbe.emplace();
		}
		auto& probe = *implementation->ThreadedReplyProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr
			? window->conversation_panel()->cards()
			: nullptr;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (cards == nullptr || editor == nullptr || model == nullptr) {
			return result::pending;
		}
		// [[BPR-1804]]. Where each course ends: with reviewers served, it goes
		// on to request one from People.
		// [[BPR-1852]]. With the draft state served, every course ends there.
		const auto goOnToDraftState = [&probe, implementation] {
			if (implementation->ReviewMutationHost->capabilities().ChangesDraftState) {
				probe.Phase = state_type::phase::awaiting_draft_offered;
				return result::pending;
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		};
		const auto goOnToReviewers = [&probe, implementation, &goOnToDraftState] {
			if (implementation->ReviewMutationHost->capabilities().RequestsReviewers) {
				probe.Phase = state_type::phase::awaiting_reviewer_offered;
				return result::pending;
			}
			return goOnToDraftState();
		};
		const auto fail = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		// One line per phase, the latest facts of each: a stalled course says
		// where it stalled and what it last saw before.
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		// [[BPR-1841]]. The Overview's unresolved threads, over its files.
		const auto overviewUnresolved = [implementation]() {
			auto total = std::size_t{ 0U };
			if (implementation->PublishedOverview) {
				for (const auto& file : implementation->PublishedOverview->Files) {
					total += file.UnresolvedThreadCount;
				}
			}
			return total;
		};
		// [[BPR-1841]]. What resolving has cost since the Resolve press; a
		// refresh still running when GitHub's answer arrived counts against it.
		const auto measureResolveCost = [&probe, implementation, window, model, cards]() {
			if (!probe.ResolveBaseline.has_value()) {
				return;
			}
			const auto& base                = *probe.ResolveBaseline;
			auto&       evidence            = probe.Evidence;
			auto* const refresh             = window->github_refresh_action();
			evidence.ResolveCostMeasured    = true;
			evidence.ResolveReads           = implementation->Fixture->evidence().AccountReadCount - base.Reads;
			evidence.ResolveProviderCalls   = implementation->Fixture->review_mutation_evidence().Resolutions - base.Resolutions;
			evidence.ResolveModelResets     = model->statistics().ModelResets - base.ModelResets;
			evidence.ResolveRowsChanged     = model->statistics().RowsChanged - base.RowsChanged;
			evidence.ResolveFinderRebuilds  = static_cast<std::size_t>((window->finder() != nullptr ? window->finder()->rebuilds() : 0U) - base.FinderRebuilds);
			evidence.ResolveCardLayouts     = static_cast<std::size_t>(cards->layouts() - base.CardLayouts);
			evidence.ResolveRefreshInFlight = evidence.ResolveRefreshInFlight || (refresh != nullptr && !refresh->isEnabled());
			evidence.ResolveOverviewBuilds  = static_cast<std::size_t>(implementation->OverviewBuilds - base.OverviewBuilds);
			evidence.ResolveOverviewUpdates = implementation->OverviewResolutionUpdates - base.OverviewMoves;
		};
		// [[BPR-1849]]. Whether People's Waiting on lists a login, and if so
		// whether as pending; and the time since the reviewer choice.
		const auto reviewerRow = [window](const std::string_view aLogin) -> std::optional<bool> {
			for (const auto* const row : window->overview_page()->request_rows()) {
				if (row->request().Name == aLogin) {
					return row->request().Pending;
				}
			}
			return std::nullopt;
		};
		const auto microsSinceChoice = [&probe]() -> std::int64_t {
			return probe.ReviewerBaseline
				? std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - probe.ReviewerBaseline->Chosen).count()
				: -1;
		};
		const auto measureReviewerCost = [&probe, implementation, window]() {
			if (!probe.ReviewerBaseline) {
				return;
			}
			auto* const refresh                    = window->github_refresh_action();
			probe.Evidence.ReviewerCostMeasured    = true;
			probe.Evidence.ReviewerReads           = implementation->Fixture->evidence().AccountReadCount - probe.ReviewerBaseline->Reads;
			probe.Evidence.ReviewerOverviewBuilds  = static_cast<std::size_t>(implementation->OverviewBuilds - probe.ReviewerBaseline->OverviewBuilds);
			probe.Evidence.ReviewerOverviewUpdates = implementation->OverviewReviewRequestUpdates - probe.ReviewerBaseline->OverviewMoves;
			probe.Evidence.ReviewerRefreshInFlight = probe.Evidence.ReviewerRefreshInFlight || (refresh != nullptr && !refresh->isEnabled());
		};
		constexpr auto threadIdentity = std::string_view{ "review-thread:PRRT_fixture_1" };
		constexpr auto replyBody      = std::string_view{ "Agreed, will do." };
		constexpr auto discardedBody  = std::string_view{ "Local text that is discarded." };
		// The first current-head line of review.cpp that no thread is on.
		const auto firstThreadlessRightLine = [window, model]() -> std::optional<std::uint32_t> {
			for (auto line = std::uint32_t{ 0U }; line < 64U; ++line) {
				if (window->comparison_surface()->select_conversation_line(core::diff_side::right, line)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					return line;
				}
			}
			return std::nullopt;
		};
		auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
		auto* const loaded     = window->conversation_panel()->findChild<QWidget*>(QStringLiteral("githubConversationLoaded"));
		if (inlinePage == nullptr || loaded == nullptr) {
			return result::pending;
		}
		// The drawer's cards are on its loaded page, with the thread's card.
		const auto drawerShowsThread = [window, cards, loaded, threadIdentity] {
			return loaded->isVisibleTo(window->conversation_panel()) && cards->isVisibleTo(window->conversation_panel())
				&& std::ranges::find(cards->cards(), std::string{ threadIdentity }, &qt::thread_card::Id) != cards->cards().end();
		};
		// A press on one of the thread's actions, as a reviewer's: the thread
		// opens in place ([[BPR-1290]]) and its button is clicked.
		const auto press = [cards, threadIdentity](const qt::card_action aAction) {
			cards->open_thread(std::string{ threadIdentity });
			auto* const card = cards->open_card();
			if (card == nullptr) {
				return false;
			}
			const auto  name   = aAction == qt::card_action::reply ? QStringLiteral("threadReplyField") : QStringLiteral("threadResolve");
			auto* const button = card->findChild<QAbstractButton*>(name);
			if (button == nullptr || !button->isEnabled()) {
				return false;
			}
			button->click();
			return true;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_ready: {
			note("outstanding=" + std::to_string(implementation->WorkbenchHost->outstanding_requests())
				 + " diff=" + std::to_string(implementation->LastSelectedDiff.has_value() ? 1 : 0)
				 + " thread=" + std::to_string(model->index_for_id(threadIdentity).isValid() ? 1 : 0));
			if (implementation->WorkbenchHost->outstanding_requests() != 0U) {
				return result::pending;
			}
			auto* const view = window->files_drawer()->contents_view();
			if (view == nullptr || view->model() == nullptr || view->model()->rowCount() == 0) {
				return result::pending;
			}
			if (!view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
				return result::pending;
			}
			if (!implementation->LastSelectedDiff.has_value()
				|| !model->index_for_id(threadIdentity).isValid()) {
				return result::pending;
			}
			// [[BPR-1214]]. First a comment on a line no thread is on, which
			// is discarded, then Back: the drawer keeps the file's threads.
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_line_comparison;
			return result::pending;
		}
		case state_type::phase::awaiting_line_comparison: {
			note("comparison=" + std::to_string(window->comparison_surface()->current_result() != nullptr ? 1 : 0));
			if (window->comparison_surface()->current_result() == nullptr) {
				return result::pending;
			}
			probe.ThreadlessLine = firstThreadlessRightLine();
			if (!probe.ThreadlessLine) {
				return fail("no-threadless-line rows=" + std::to_string(model->rowCount()));
			}
			probe.Phase = state_type::phase::awaiting_line_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_line_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + std::to_string(draft.has_value() ? 1 : 0)
				 + " anchorLine=" + std::to_string(draft ? draft->Anchor.Line : 0U)
				 + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0)
				 + " rows=" + std::to_string(model->rowCount()));
			if (!draft || draft->Anchor.Side != core::diff_side::right || draft->Anchor.Line != probe.ThreadlessLine
				|| draft->ReplyToThread || !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			if (model->rowCount() != 0 || !drawerShowsThread()) {
				return fail("threadless-line-hid-drawer rows=" + std::to_string(model->rowCount()));
			}
			// [[BPR-1212]]. Comment opens the draft in the drawer at its line.
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (!cards->writing_draft() || cards->draft_editor() == nullptr || cards->draft_editor()->isReadOnly() || inlinePage->isVisibleTo(window)) {
				return fail("comment-drawer-draft-not-editable");
			}
			cards->draft_editor()->setPlainText(QString::fromUtf8(discardedBody.data(), static_cast<qsizetype>(discardedBody.size())));
			probe.Phase = state_type::phase::awaiting_line_body;
			return result::pending;
		}
		case state_type::phase::awaiting_line_body: {
			note("body=" + (implementation->ReviewDraft ? implementation->ReviewDraft->Body : std::string{ "-" }));
			if (!implementation->ReviewDraft || implementation->ReviewDraft->Body != discardedBody) {
				return result::pending;
			}
			// The draft's own Discard, in the drawer.
			auto* const discard = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadDeleteDraft")) : nullptr;
			if (discard == nullptr) {
				return fail("no-drawer-discard");
			}
			discard->click();
			QCoreApplication::processEvents();
			// [[BPR-1345]] v002: Delete asks first when the draft has words.
			auto* const question = window->delete_draft_confirmation();
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("deleteDraftConfirmationConfirm")) : nullptr;
			if (yes == nullptr) {
				return fail("drawer-delete-asked-nothing");
			}
			yes->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_line_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_line_discarded: {
			const auto& draft = implementation->ReviewDraft;
			note("state=" + (draft ? std::string{ core::name(draft->State) } : std::string{ "-" }));
			if (draft && draft->State != core::review_draft_state::discarded && draft->Body == discardedBody) {
				return result::pending;
			}
			QCoreApplication::processEvents();
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			if (inlinePage->isVisibleTo(window) || cards->writing_draft() || !drawerShowsThread() || mutations.Submissions != 0U || mutations.Replies != 0U) {
				return fail("comments-drawer-blank-after-discard-and-back page=" + std::to_string(inlinePage->isVisibleTo(window) ? 1 : 0)
							+ " submissions=" + std::to_string(mutations.Submissions) + " replies=" + std::to_string(mutations.Replies));
			}
			window->choose_modality(qt::review_modality::discuss);
			QCoreApplication::processEvents();
			if (!press(qt::card_action::reply)) {
				return fail("no-reply-action");
			}
			probe.Phase = state_type::phase::awaiting_reply_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_draft: {
			note("draft=" + std::to_string(implementation->ReviewDraft.has_value() ? 1 : 0) + " reply="
				 + (implementation->ReviewDraft && implementation->ReviewDraft->ReplyToThread ? *implementation->ReviewDraft->ReplyToThread : std::string{ "-" })
				 + " anchorSide=" + std::to_string(implementation->ReviewDraft ? static_cast<int>(implementation->ReviewDraft->Anchor.Side) : -1)
				 + " anchorLine=" + std::to_string(implementation->ReviewDraft ? implementation->ReviewDraft->Anchor.Line : 0U)
				 + " surfaceSide=" + std::to_string(window->comparison_surface()->selection() ? static_cast<int>(window->comparison_surface()->selection()->PreferredLine.Side) : -1)
				 + " surfaceLine=" + std::to_string(window->comparison_surface()->selection() ? window->comparison_surface()->selection()->PreferredLine.Line : 0U)
				 + " availability=" + std::to_string(model->index_for_id(threadIdentity).data(qt::github_conversation_model::anchor_availability_role).toInt())
				 + " discuss=" + std::to_string(window->conversation_panel()->discuss() ? 1 : 0));
			if (!implementation->ReviewDraft.has_value()
				|| implementation->ReviewDraft->ReplyToThread
					!= std::optional<std::string>{ "PRRT_fixture_1" }) {
				return result::pending;
			}
			probe.Evidence.ReplyDrafted = !editor->reply_target_label()->isHidden();
			editor->body_editor()->setPlainText(QString::fromUtf8(replyBody.data(), static_cast<qsizetype>(replyBody.size())));
			probe.Phase = state_type::phase::awaiting_reply_body;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_body: {
			note("body=" + (implementation->ReviewDraft ? implementation->ReviewDraft->Body : std::string{ "-" })
				 + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0));
			if (!implementation->ReviewDraft.has_value() || implementation->ReviewDraft->Body != replyBody
				|| !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_reply_published;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_published: {
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0)
				 + " replies=" + std::to_string(implementation->Fixture->review_mutation_evidence().Replies)
				 + " submissions=" + std::to_string(implementation->Fixture->review_mutation_evidence().Submissions)
				 + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value()
				|| implementation->Fixture->review_mutation_evidence().Replies == 0U) {
				return result::pending;
			}
			probe.Evidence.ReplyPublished = implementation->Fixture->review_mutation_evidence().Replies == 1U;
			auto* const refresh           = window->github_refresh_action();
			if (refresh == nullptr) {
				return fail("refresh-unavailable");
			}
			// A refresh the application started after publishing holds the
			// action until it answers; the run budget bounds the wait.
			if (!refresh->isEnabled()) {
				note("refresh=busy");
				return result::pending;
			}
			refresh->trigger();
			probe.Phase = state_type::phase::awaiting_reply_seen;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_seen: {
			const auto reply = model->index_for_id("review-comment:fixture-comment-1");
			note("replyRow=" + std::to_string(reply.isValid() ? 1 : 0) + " rows=" + std::to_string(model->rowCount())
				 + " threadChildren=" + std::to_string(model->rowCount(model->index_for_id(threadIdentity)))
				 + " reads=" + std::to_string(implementation->Fixture->evidence().AccountReadCount)
				 + " lastRead=" + std::to_string(implementation->Fixture->evidence().LastReadOperation));
			if (!reply.isValid()) {
				return result::pending;
			}
			const auto parent = reply.data(qt::github_conversation_model::parent_identity_role).toString();
			probe.Evidence.ReplyUnderThread
				= (parent == QString::fromUtf8(threadIdentity.data(), static_cast<qsizetype>(threadIdentity.size()))
				   || parent == QStringLiteral("review-comment:PRRC_fixture_1"))
				&& reply.data(qt::github_conversation_model::body_role).toString()
					== QString::fromUtf8(replyBody.data(), static_cast<qsizetype>(replyBody.size()));
			if (!probe.Evidence.ReplyUnderThread) {
				return fail("reply-not-under-thread parent=" + parent.toStdString());
			}
			// [[BPR-1824]]. Resolve in Discuss with no file open: the diff
			// selection goes first, and GitHub is still asked. Before, the
			// grant waited on a selected diff and the thread "could not be
			// resolved: unavailable".
			implementation->clear_diff_selection();
			if (implementation->SelectedDiffPublication.has_value()) {
				return fail("diff-selection-kept");
			}
			window->choose_modality(qt::review_modality::discuss);
			QCoreApplication::processEvents();
			// The thread opens before the counters are taken: opening marks it
			// read, and the receipt that writes may project the Overview again.
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			probe.ResolveBaseline = state_type::resolve_baseline{
				.Reads          = implementation->Fixture->evidence().AccountReadCount,
				.Resolutions    = implementation->Fixture->review_mutation_evidence().Resolutions,
				.ModelResets    = model->statistics().ModelResets,
				.RowsChanged    = model->statistics().RowsChanged,
				.FinderRebuilds = window->finder() != nullptr ? window->finder()->rebuilds() : 0U,
				.CardLayouts    = cards->layouts(),
				.OverviewBuilds = implementation->OverviewBuilds,
				.OverviewMoves  = implementation->OverviewResolutionUpdates,
				.Unresolved     = overviewUnresolved(),
			};
			if (!press(qt::card_action::resolve)) {
				return fail("no-resolve-action refusal=" + std::to_string(static_cast<int>(cards->resolution_refusal())));
			}
			probe.Evidence.ResolveAsked = true;
			probe.Phase                 = state_type::phase::awaiting_resolved;
			return result::pending;
		}
		case state_type::phase::awaiting_resolved: {
			note("completions=" + std::to_string(implementation->ThreadResolutionCompletions)
				 + " refusals=" + std::to_string(implementation->ThreadResolutionRefusals)
				 + " failures=" + std::to_string(implementation->ThreadResolutionFailures)
				 + " resolutions=" + std::to_string(implementation->Fixture->review_mutation_evidence().Resolutions));
			if (implementation->ThreadResolutionFailures != 0U || implementation->ThreadResolutionRefusals != 0U) {
				return fail("resolution refused-or-failed notice=" + (window->conversation_panel() != nullptr ? window->conversation_panel()->notice_label()->text().toStdString() : std::string{ }));
			}
			if (implementation->ThreadResolutionCompletions == 0U) {
				return result::pending;
			}
			const auto thread = model->index_for_id(threadIdentity);
			if (!thread.isValid() || !thread.data(qt::github_conversation_model::resolved_role).toBool()) {
				return result::pending;
			}
			probe.Evidence.ThreadResolved = true;
			measureResolveCost();
			probe.UnresolvedWhenResolved = overviewUnresolved();
			// [[BPR-1741]]. With reactions served, the course goes on:
			// Unresolve conversation at the thread's end, then reactions.
			if (!implementation->ReviewMutationHost->capabilities().Reacts) {
				// [[BPR-1774]]. With the description served, it goes on there.
				if (implementation->ReviewMutationHost->capabilities().EditsDescription) {
					probe.Phase = state_type::phase::awaiting_description_offered;
					return result::pending;
				}
				// [[BPR-1775]]. With comments edited, it goes on to my reply.
				if (implementation->ReviewMutationHost->capabilities().EditsComments) {
					probe.Phase = state_type::phase::awaiting_comment_edit_offered;
					return result::pending;
				}
				return goOnToReviewers();
			}
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			auto* const reopen = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadResolveConversation")) : nullptr;
			if (reopen == nullptr || !reopen->isEnabled() || reopen->text() != QStringLiteral("Unresolve conversation")) {
				return fail("no-unresolve-conversation");
			}
			auto* const gate = implementation->ReviewMutationDecisionGate.get();
			if (gate == nullptr) {
				return fail("no-reopen-decision-gate");
			}
			gate->hold();
			const auto release = qScopeGuard([gate] { gate->release(); });
			reopen->click();
			// Observe the synchronous gesture before its native terminal can
			// clear Pending. The next phase still requires confirmed reopening.
			const auto shown                 = std::ranges::find(cards->cards(), std::string{ threadIdentity }, &qt::thread_card::Id);
			probe.Evidence.ReopenShownAtOnce = shown != cards->cards().end() && !shown->Resolved && shown->ResolutionPending;
			if (!probe.Evidence.ReopenShownAtOnce) {
				return fail("reopen-not-shown-at-once");
			}
			probe.Phase = state_type::phase::awaiting_reopened;
			return result::pending;
		}
		case state_type::phase::awaiting_reopened: {
			note("completions=" + std::to_string(implementation->ThreadResolutionCompletions) + " failures=" + std::to_string(implementation->ThreadResolutionFailures));
			if (implementation->ThreadResolutionFailures != 0U || implementation->ThreadResolutionRefusals != 0U) {
				return fail("reopen-refused-or-failed");
			}
			const auto thread = model->index_for_id(threadIdentity);
			const auto card   = std::ranges::find(cards->cards(), std::string{ threadIdentity }, &qt::thread_card::Id);
			if (implementation->ThreadResolutionCompletions < 2U || !thread.isValid() || thread.data(qt::github_conversation_model::resolved_role).toBool()
				|| card == cards->cards().end() || card->Resolved || card->ResolutionPending) {
				return result::pending;
			}
			probe.Evidence.Reopened = true;
			measureResolveCost();
			if (probe.ResolveBaseline && probe.UnresolvedWhenResolved) {
				const auto before                     = probe.ResolveBaseline->Unresolved;
				probe.Evidence.ResolveOverviewCounted = before > 0U && *probe.UnresolvedWhenResolved == before - 1U && overviewUnresolved() == before;
			}
			note("overviewUnresolved=" + std::to_string(probe.ResolveBaseline ? probe.ResolveBaseline->Unresolved : 0U) + "/"
				 + std::to_string(probe.UnresolvedWhenResolved.value_or(0U)) + "/" + std::to_string(overviewUnresolved()));
			probe.Evidence.ResolveMinimal = probe.Evidence.ResolveReads == 0 && probe.Evidence.ResolveProviderCalls == 2U
				&& probe.Evidence.ResolveModelResets == 0U && !probe.Evidence.ResolveRefreshInFlight && probe.Evidence.ResolveOverviewCounted;
			probe.Phase = state_type::phase::awaiting_reactions_read;
			return result::pending;
		}
		case state_type::phase::awaiting_reactions_read: {
			const auto post  = std::string{ "review-comment:PRRC_fixture_1" };
			const auto shown = cards->reactions_of(post);
			note("read=" + std::to_string(shown.has_value() ? 1 : 0) + " reads=" + std::to_string(implementation->Fixture->evidence().AccountReadCount));
			if (!shown.has_value()) {
				return result::pending;
			}
			const auto* const thumbs     = core::find_reaction_group(*shown, core::reaction_content::thumbs_up);
			probe.Evidence.ReactionsRead = thumbs != nullptr && thumbs->Count == 2U && !thumbs->ViewerReacted;
			if (!probe.Evidence.ReactionsRead) {
				return fail("reactions-not-as-recorded");
			}
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			auto* const chip = reaction_chip_of(*cards, post, core::reaction_content::thumbs_up);
			if (chip == nullptr || !chip->isEnabled()) {
				return fail("no-thumbs-up-chip");
			}
			auto* const gate = implementation->ReviewMutationDecisionGate.get();
			if (gate == nullptr) {
				return fail("no-reaction-decision-gate");
			}
			gate->hold();
			const auto release = qScopeGuard([gate] { gate->release(); });
			chip->click();
			// Keep the native answer behind this immediate observation. The
			// next phase still requires the confirmed reaction and no Pending.
			const auto        now        = cards->reactions_of(post);
			const auto* const pending    = now ? core::find_reaction_group(*now, core::reaction_content::thumbs_up) : nullptr;
			probe.Evidence.ReactionShown = pending != nullptr && pending->Count == 3U && pending->ViewerReacted && pending->Pending;
			if (!probe.Evidence.ReactionShown) {
				return fail("reaction-not-shown-at-once");
			}
			probe.Phase = state_type::phase::awaiting_reaction_confirmed;
			return result::pending;
		}
		case state_type::phase::awaiting_reaction_confirmed:
		case state_type::phase::awaiting_reaction_removed:
		case state_type::phase::awaiting_picker_reaction:    {
			const auto post = std::string{ "review-comment:PRRC_fixture_1" };
			note("completions=" + std::to_string(implementation->ReactionCompletions) + " failures=" + std::to_string(implementation->ReactionFailures)
				 + " asked=" + std::to_string(implementation->Fixture->review_mutation_evidence().Reactions));
			if (implementation->ReactionFailures != 0U) {
				return fail("reaction-failed");
			}
			const auto shown = cards->reactions_of(post);
			if (!shown.has_value()) {
				return result::pending;
			}
			const auto* const thumbs = core::find_reaction_group(*shown, core::reaction_content::thumbs_up);
			const auto* const heart  = core::find_reaction_group(*shown, core::reaction_content::heart);
			if (probe.Phase == state_type::phase::awaiting_reaction_confirmed) {
				if (implementation->ReactionCompletions < 1U || thumbs == nullptr || thumbs->Pending) {
					return result::pending;
				}
				probe.Evidence.ReactionConfirmed = thumbs->Count == 3U && thumbs->ViewerReacted;
				if (!probe.Evidence.ReactionConfirmed) {
					return fail("reaction-not-confirmed");
				}
				cards->open_thread(std::string{ threadIdentity });
				QCoreApplication::processEvents();
				auto* const chip = reaction_chip_of(*cards, post, core::reaction_content::thumbs_up);
				if (chip == nullptr) {
					return fail("no-chip-to-remove");
				}
				chip->click();
				probe.Phase = state_type::phase::awaiting_reaction_removed;
				return result::pending;
			}
			if (probe.Phase == state_type::phase::awaiting_reaction_removed) {
				if (implementation->ReactionCompletions < 2U || thumbs == nullptr || thumbs->Pending) {
					return result::pending;
				}
				probe.Evidence.ReactionRemoved = thumbs->Count == 2U && !thumbs->ViewerReacted;
				if (!probe.Evidence.ReactionRemoved) {
					return fail("reaction-not-removed");
				}
				// The picker, by keyboard alone: five steps right is heart.
				cards->open_thread(std::string{ threadIdentity });
				QCoreApplication::processEvents();
				auto* const view  = post_view_of(*cards, post);
				auto* const react = view != nullptr ? view->findChild<QAbstractButton*>(QStringLiteral("threadPostReact")) : nullptr;
				if (react == nullptr || react->isHidden()) {
					return fail("no-add-reaction");
				}
				react->click();
				QCoreApplication::processEvents();
				auto* picker = static_cast<QWidget*>(nullptr);
				for (auto* const widget : QApplication::topLevelWidgets()) {
					if (widget->objectName() == QStringLiteral("reactionPicker") && widget->isVisible()) {
						picker = widget;
					}
				}
				if (picker == nullptr || picker->focusWidget() == nullptr) {
					return fail("no-picker");
				}
				const auto typeKey = [picker](const int aKey) {
					auto event = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
					QCoreApplication::sendEvent(picker->focusWidget() != nullptr ? picker->focusWidget() : picker, &event);
				};
				for (auto step = 0; step < 5; ++step) {
					typeKey(Qt::Key_Right);
				}
				typeKey(Qt::Key_Return);
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::awaiting_picker_reaction;
				return result::pending;
			}
			if (implementation->ReactionCompletions < 3U || heart == nullptr || heart->Pending) {
				return result::pending;
			}
			probe.Evidence.PickerReacted = heart->Count == 1U && heart->ViewerReacted;
			if (!probe.Evidence.PickerReacted) {
				return fail("picker-reaction-not-heart");
			}
			probe.ReadsBeforeRefresh = implementation->Fixture->evidence().AccountReadCount;
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
				refresh->trigger();
			} else {
				return fail("no-refresh");
			}
			probe.Phase = state_type::phase::awaiting_reaction_reconciled;
			return result::pending;
		}
		case state_type::phase::awaiting_reaction_reconciled: {
			const auto post  = std::string{ "review-comment:PRRC_fixture_1" };
			const auto shown = cards->reactions_of(post);
			note("reads=" + std::to_string(implementation->Fixture->evidence().AccountReadCount) + " overlay=" + std::to_string(implementation->ConversationOverlay.Reactions.size()));
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh || !implementation->ConversationOverlay.Reactions.empty()
				|| !shown.has_value()) {
				return result::pending;
			}
			const auto* const thumbs          = core::find_reaction_group(*shown, core::reaction_content::thumbs_up);
			const auto* const heart           = core::find_reaction_group(*shown, core::reaction_content::heart);
			probe.Evidence.ReactionReconciled = thumbs != nullptr && thumbs->Count == 2U && !thumbs->ViewerReacted && heart != nullptr && heart->Count == 1U && heart->ViewerReacted
				&& !heart->Pending;
			if (!probe.Evidence.ReactionReconciled) {
				return fail("reaction-not-reconciled");
			}
			if (implementation->ReviewMutationHost->capabilities().EditsDescription) {
				probe.Phase = state_type::phase::awaiting_description_offered;
				return result::pending;
			}
			if (implementation->ReviewMutationHost->capabilities().EditsComments) {
				probe.Phase = state_type::phase::awaiting_comment_edit_offered;
				return result::pending;
			}
			return goOnToReviewers();
		}
		// [[BPR-1774]]. Overview offers the author Edit; Save performs exactly
		// one write, and the next read shows GitHub's body. A second edit
		// meets a body another client changed: nothing is written, the typed
		// text stays, and editing goes on from GitHub's newer text.
		case state_type::phase::awaiting_description_offered: {
			auto* const page = window->overview_page();
			window->choose_modality(qt::review_modality::overview);
			QCoreApplication::processEvents();
			note("editable=" + std::to_string(page->description_editable() ? 1 : 0) + " visible=" + std::to_string(page->description_edit_button()->isVisible() ? 1 : 0));
			if (!page->description_editable() || !page->projection().has_value()) {
				return result::pending;
			}
			probe.Evidence.DescriptionOffered = page->description_edit_button()->isVisible() || page->description_add_button()->isVisible();
			if (!probe.Evidence.DescriptionOffered) {
				return fail("edit-not-shown");
			}
			(page->description_edit_button()->isVisible() ? static_cast<QAbstractButton*>(page->description_edit_button()) : page->description_add_button())->click();
			QCoreApplication::processEvents();
			auto* const text = page->description_editor();
			if (!page->description_editing() || text->raw() != page->projection()->Body) {
				return fail("editor-not-on-body");
			}
			probe.ReadsBeforeRefresh = implementation->Fixture->evidence().AccountReadCount;
			text->selectAll();
			text->insertPlainText(QStringLiteral("Saved from the Overview editor.\n\n- one write"));
			QCoreApplication::processEvents();
			page->description_save_button()->click();
			probe.Phase = state_type::phase::awaiting_description_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_description_saved: {
			auto* const page     = window->overview_page();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->DescriptionEditCompletions) + " failures=" + std::to_string(implementation->DescriptionEditFailures)
				 + " refusals=" + std::to_string(implementation->DescriptionEditRefusals) + " edits=" + std::to_string(evidence.DescriptionEdits)
				 + " writes=" + std::to_string(evidence.DescriptionWrites) + " status=" + page->description_status()->text().toStdString());
			if (implementation->DescriptionEditFailures != 0U || implementation->DescriptionEditRefusals != 0U || implementation->DescriptionEditConflicts != 0U) {
				return fail("save-refused-or-failed");
			}
			if (implementation->DescriptionEditCompletions < 1U || page->description_editing()) {
				return result::pending;
			}
			probe.Evidence.DescriptionSavedOnce = evidence.DescriptionEdits == 1U && evidence.DescriptionWrites == 1U;
			if (!probe.Evidence.DescriptionSavedOnce) {
				return fail("not-exactly-one-write");
			}
			constexpr auto saved = std::string_view{ "Saved from the Overview editor.\n\n- one write" };
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh || !page->projection().has_value() || page->projection()->Body != saved) {
				return result::pending;
			}
			probe.Evidence.DescriptionShownAfterRead = page->body_view()->original_body() == saved;
			if (!probe.Evidence.DescriptionShownAfterRead) {
				return fail("read-not-shown");
			}
			// Another client changes the description while this one edits.
			if (!page->begin_description_edit()) {
				return fail("no-second-edit");
			}
			auto* const text = page->description_editor();
			auto        end  = text->textCursor();
			end.movePosition(QTextCursor::End);
			text->setTextCursor(end);
			text->insertPlainText(QStringLiteral("\n- mine, typed second"));
			QCoreApplication::processEvents();
			implementation->Fixture->stage_server_description("Changed on GitHub by someone else.");
			page->description_save_button()->click();
			probe.Phase = state_type::phase::awaiting_description_conflict;
			return result::pending;
		}
		case state_type::phase::awaiting_description_conflict: {
			auto* const page     = window->overview_page();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("conflicts=" + std::to_string(implementation->DescriptionEditConflicts) + " edits=" + std::to_string(evidence.DescriptionEdits)
				 + " writes=" + std::to_string(evidence.DescriptionWrites) + " status=" + page->description_status()->text().toStdString());
			if (implementation->DescriptionEditFailures != 0U || implementation->DescriptionEditCompletions != 1U) {
				return fail("conflict-written-or-failed");
			}
			if (implementation->DescriptionEditConflicts < 1U) {
				return result::pending;
			}
			auto* const text                          = page->description_editor();
			probe.Evidence.DescriptionConflictRefused = evidence.DescriptionEdits == 2U && evidence.DescriptionWrites == 1U && page->description_editing()
				&& text->raw().ends_with("- mine, typed second") && page->description_newer_button()->isVisible()
				&& page->description_status()->text().contains(QStringLiteral("changed the description on GitHub"));
			if (!probe.Evidence.DescriptionConflictRefused) {
				return fail("conflict-not-refused");
			}
			page->description_newer_button()->click();
			QCoreApplication::processEvents();
			probe.Evidence.DescriptionNewerLoaded = text->raw() == "Changed on GitHub by someone else." && page->description_original() == text->raw();
			if (!probe.Evidence.DescriptionNewerLoaded) {
				return fail("newer-not-loaded");
			}
			page->description_cancel_button()->click();
			QCoreApplication::processEvents();
			if (implementation->ReviewMutationHost->capabilities().EditsComments) {
				probe.Phase = state_type::phase::awaiting_comment_edit_offered;
				return result::pending;
			}
			return goOnToReviewers();
		}
		// [[BPR-1775]]. My published reply offers Edit in Discuss and another's
		// comment does not; Save performs exactly one write, and the next read
		// shows GitHub's body. A second edit meets a body another client
		// changed: nothing is written, the typed text stays, and editing goes
		// on from GitHub's newer text.
		case state_type::phase::awaiting_comment_edit_offered: {
			constexpr auto mine = std::string_view{ "review-comment:fixture-comment-1" };
			// Every post in the recording is the viewer's: the thread's first
			// post becomes another account's on the next read.
			if (!probe.CommentRootRestaged) {
				auto* const refresh = window->github_refresh_action();
				if (refresh == nullptr || !refresh->isEnabled()) {
					return result::pending;
				}
				implementation->Fixture->stage_server_root("PRRT_fixture_1", std::nullopt, std::nullopt, std::pair<std::string, std::string>{ "U_fixture_other", "octo-other" });
				probe.CommentRootRestaged = true;
				probe.ReadsBeforeRefresh  = implementation->Fixture->evidence().AccountReadCount;
				refresh->trigger();
				return result::pending;
			}
			// A refresh the window was already running may have answered from
			// before the change; ask again until a later read arrives.
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh) {
				if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
					refresh->trigger();
				}
				return result::pending;
			}
			window->choose_modality(qt::review_modality::discuss);
			cards->open_thread(std::string{ threadIdentity });
			QCoreApplication::processEvents();
			const auto others = cards->comment_editable("review-comment:PRRC_fixture_1");
			note("offered=" + std::to_string(cards->comment_edit_offered() ? 1 : 0) + " mine=" + std::to_string(cards->comment_editable(mine) ? 1 : 0)
				 + " others=" + std::to_string(others ? 1 : 0) + " open=" + cards->open_thread_id());
			// Until the read naming another author arrives, the first post is
			// still the viewer's.
			if (!cards->comment_edit_offered() || cards->open_card() == nullptr || others || !cards->comment_editable(mine)) {
				return result::pending;
			}
			for (auto* const post : cards->open_card()->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
				if (post->property("postIdentity").toString() == QStringLiteral("review-comment:PRRC_fixture_1")
					&& (post->findChild<QAbstractButton*>(QStringLiteral("threadPostEdit")) == nullptr || !post->findChild<QAbstractButton*>(QStringLiteral("threadPostEdit"))->isHidden())) {
					return fail("edit-shown-on-another's-comment");
				}
			}
			auto* button = static_cast<QAbstractButton*>(nullptr);
			for (auto* const post : cards->open_card()->findChildren<QWidget*>(QStringLiteral("threadPost"))) {
				if (post->property("postIdentity").toString() == QString::fromUtf8(mine.data(), static_cast<qsizetype>(mine.size()))) {
					button = post->findChild<QAbstractButton*>(QStringLiteral("threadPostEdit"));
				}
			}
			probe.Evidence.CommentEditOffered = button != nullptr && !button->isHidden();
			if (!probe.Evidence.CommentEditOffered) {
				return fail("edit-not-shown-on-my-reply");
			}
			button->click();
			QCoreApplication::processEvents();
			auto* const text = cards->comment_editor();
			if (text == nullptr || text->raw() != replyBody) {
				return fail("editor-not-on-reply");
			}
			probe.ReadsBeforeRefresh = implementation->Fixture->evidence().AccountReadCount;
			text->selectAll();
			text->insertPlainText(QStringLiteral("Edited from the conversation."));
			QCoreApplication::processEvents();
			cards->comment_edit_save_button()->click();
			probe.Phase = state_type::phase::awaiting_comment_edit_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_comment_edit_saved: {
			constexpr auto mine     = std::string_view{ "review-comment:fixture-comment-1" };
			constexpr auto saved    = std::string_view{ "Edited from the conversation." };
			const auto     evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->CommentEditCompletions) + " failures=" + std::to_string(implementation->CommentEditFailures)
				 + " refusals=" + std::to_string(implementation->CommentEditRefusals) + " edits=" + std::to_string(evidence.CommentEdits)
				 + " writes=" + std::to_string(evidence.CommentEditWrites) + " status=" + (cards->comment_edit_status() != nullptr ? cards->comment_edit_status()->text().toStdString() : std::string{ }));
			if (implementation->CommentEditFailures != 0U || implementation->CommentEditRefusals != 0U || implementation->CommentEditConflicts != 0U) {
				return fail("save-refused-or-failed");
			}
			if (implementation->CommentEditCompletions < 1U || cards->comment_editing().has_value()) {
				return result::pending;
			}
			probe.Evidence.CommentEditSavedOnce = evidence.CommentEdits == 1U && evidence.CommentEditWrites == 1U;
			if (!probe.Evidence.CommentEditSavedOnce) {
				return fail("not-exactly-one-write");
			}
			const auto reply = model->index_for_id(std::string{ mine });
			if (implementation->Fixture->evidence().AccountReadCount <= probe.ReadsBeforeRefresh || !reply.isValid()
				|| reply.data(qt::github_conversation_model::body_role).toString() != QString::fromUtf8(saved.data(), static_cast<qsizetype>(saved.size()))) {
				return result::pending;
			}
			probe.Evidence.CommentEditShownAfterRead = true;
			// Another client changes the reply while this one edits it.
			if (!cards->begin_comment_edit(std::string{ mine }) || cards->comment_editor() == nullptr) {
				return fail("no-second-edit");
			}
			auto* const text = cards->comment_editor();
			auto        end  = text->textCursor();
			end.movePosition(QTextCursor::End);
			text->setTextCursor(end);
			text->insertPlainText(QStringLiteral(" Typed second."));
			QCoreApplication::processEvents();
			implementation->Fixture->stage_server_comment("fixture-comment-1", "Changed on GitHub by someone else.");
			cards->comment_edit_save_button()->click();
			probe.Phase = state_type::phase::awaiting_comment_edit_conflict;
			return result::pending;
		}
		case state_type::phase::awaiting_comment_edit_conflict: {
			const auto evidence = implementation->Fixture->review_mutation_evidence();
			note("conflicts=" + std::to_string(implementation->CommentEditConflicts) + " edits=" + std::to_string(evidence.CommentEdits)
				 + " writes=" + std::to_string(evidence.CommentEditWrites));
			if (implementation->CommentEditFailures != 0U || implementation->CommentEditCompletions != 1U) {
				return fail("conflict-written-or-failed");
			}
			if (implementation->CommentEditConflicts < 1U) {
				return result::pending;
			}
			auto* const text                          = cards->comment_editor();
			probe.Evidence.CommentEditConflictRefused = evidence.CommentEdits == 2U && evidence.CommentEditWrites == 1U && text != nullptr
				&& text->raw().ends_with(" Typed second.") && cards->comment_edit_newer_button()->isVisible()
				&& cards->comment_edit_status()->text().contains(QStringLiteral("changed this comment on GitHub"));
			if (!probe.Evidence.CommentEditConflictRefused) {
				return fail("conflict-not-refused");
			}
			cards->comment_edit_newer_button()->click();
			QCoreApplication::processEvents();
			probe.Evidence.CommentEditNewerLoaded = text->raw() == "Changed on GitHub by someone else." && cards->comment_edit_original() == text->raw();
			if (!probe.Evidence.CommentEditNewerLoaded) {
				return fail("newer-not-loaded");
			}
			cards->comment_edit_cancel_button()->click();
			QCoreApplication::processEvents();
			return goOnToReviewers();
		}
		// [[BPR-1804]]. People offers Request review; the picker lists GitHub's
		// suggestion, a typed query narrows it to one person, and choosing them
		// by keyboard writes exactly once; choosing them again sends nothing.
		case state_type::phase::awaiting_reviewer_offered: {
			auto* const page = window->overview_page();
			window->choose_modality(qt::review_modality::overview);
			QCoreApplication::processEvents();
			if (!page->reviewer_requests_offered()) {
				return result::pending;
			}
			probe.Evidence.ReviewerOffered = page->request_review_button()->isVisibleTo(page);
			if (!probe.Evidence.ReviewerOffered) {
				return fail("request-review-not-shown");
			}
			page->request_review_button()->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_reviewer_listed;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_listed: {
			auto* const picker = window->overview_page()->reviewer_picker_popup();
			if (picker == nullptr || implementation->ReviewerPicker.Searching) {
				return result::pending;
			}
			if (picker->listed_logins() != QStringList{ "fixture-other-reviewer", "octo-maintainer", "octo-outsider" }) {
				return fail("picker-not-listed");
			}
			for (const auto character : QStringLiteral("maint")) {
				auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString{ character } };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			probe.Phase = state_type::phase::awaiting_reviewer_filtered;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_filtered: {
			auto* const picker   = window->overview_page()->reviewer_picker_popup();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("searches=" + std::to_string(evidence.ReviewerSearches) + " listed=" + picker->listed_logins().join(QLatin1Char{ ',' }).toStdString());
			if (implementation->ReviewerPicker.Searching || evidence.ReviewerSearches < 2U) {
				return result::pending;
			}
			probe.Evidence.ReviewerSearched = picker->listed_logins() == QStringList{ "octo-maintainer" } && implementation->ReviewerPicker.Query == "maint";
			if (!probe.Evidence.ReviewerSearched) {
				return fail("search-not-narrowed");
			}
			// [[BPR-1849]]. What requesting costs is counted from the choice.
			probe.ReviewerBaseline = state_type::reviewer_baseline{
				.Reads          = implementation->Fixture->evidence().AccountReadCount,
				.OverviewBuilds = implementation->OverviewBuilds,
				.OverviewMoves  = implementation->OverviewReviewRequestUpdates,
				.Chosen         = std::chrono::steady_clock::now(),
			};
			for (const auto key : { Qt::Key_Down, Qt::Key_Return }) {
				auto down = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			// The choice dispatched synchronously: People lists them now, pending.
			if (reviewerRow("octo-maintainer") == std::optional{ true }) {
				probe.Evidence.ReviewerShownAtOnce = true;
				probe.Evidence.ReviewerShownMicros = microsSinceChoice();
			}
			probe.Phase = state_type::phase::awaiting_reviewer_requested;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_requested: {
			auto* const picker   = window->overview_page()->reviewer_picker_popup();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->ReviewerRequestCompletions) + " failures=" + std::to_string(implementation->ReviewerRequestFailures)
				 + " writes=" + std::to_string(evidence.ReviewerWrites) + " status=" + picker->status()->text().toStdString());
			if (implementation->ReviewerRequestFailures != 0U) {
				return fail("request-failed");
			}
			if (implementation->ReviewerRequestCompletions < 1U) {
				return result::pending;
			}
			if (!probe.Evidence.ReviewerRequestedOnce) {
				probe.Evidence.ReviewerRequestedOnce = evidence.ReviewerWrites == 1U
					&& picker->status()->text() == QStringLiteral("Requested a review from octo-maintainer.");
				if (!probe.Evidence.ReviewerRequestedOnce) {
					return fail("not-requested-once");
				}
				// [[BPR-1849]]. GitHub's answer confirms the row People shows.
				probe.Evidence.ReviewerAnsweredMicros = microsSinceChoice();
				probe.Evidence.ReviewerConfirmed      = reviewerRow("octo-maintainer") == std::optional{ false };
				if (probe.Evidence.ReviewerShownMicros < 0 && reviewerRow("octo-maintainer").has_value()) {
					probe.Evidence.ReviewerShownMicros = probe.Evidence.ReviewerAnsweredMicros;
				}
				measureReviewerCost();
			}
			measureReviewerCost();
			// Whatever the answer began settles before the course goes on.
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && !refresh->isEnabled()) {
				return result::pending;
			}
			if (probe.Evidence.ReviewerShownMicros < 0 && reviewerRow("octo-maintainer").has_value()) {
				probe.Evidence.ReviewerShownMicros = microsSinceChoice();
			}
			// [[BPR-1818]]. A native popup may have been dismissed while the
			// first request and its refresh completed. A closed picker correctly
			// refuses a choice. Reopen through People and narrow the public list
			// before exercising the already-requested row again.
			if (!implementation->ReviewerPicker.Open || !picker->isVisible()) {
				window->overview_page()->request_review_button()->click();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (implementation->ReviewerPicker.Searching) {
				return result::pending;
			}
			if (implementation->ReviewerPicker.Query != "maint") {
				for (const auto character : QStringLiteral("maint")) {
					auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString{ character } };
					QCoreApplication::sendEvent(picker->search_field(), &down);
				}
				return result::pending;
			}
			if (picker->listed_logins() != QStringList{ "octo-maintainer" }) {
				return fail("repeat-reviewer-not-listed");
			}
			const auto asked   = implementation->ReviewerRequestsAsked;
			const auto context = [&] {
				const auto* const selected = picker->people()->currentItem();
				return "open=" + std::to_string(implementation->ReviewerPicker.Open)
					+ " visible=" + std::to_string(picker->isVisible())
					+ " selected=" + (selected != nullptr ? selected->data(Qt::AccessibleTextRole).toString().toStdString() : "<none>")
					+ " asked=" + std::to_string(implementation->ReviewerRequestsAsked)
					+ " writes=" + std::to_string(implementation->Fixture->review_mutation_evidence().ReviewerWrites)
					+ " status=" + picker->status()->text().toStdString();
			};
			note("repeat-before " + context());
			for (const auto key : { Qt::Key_Down, Qt::Key_Return }) {
				auto down = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			QCoreApplication::processEvents();
			note("repeat-after " + context());
			probe.Evidence.ReviewerNotRepeated = asked == 1U && implementation->ReviewerRequestsAsked == asked && implementation->Fixture->review_mutation_evidence().ReviewerWrites == 1U
				&& picker->status()->text() == QStringLiteral("octo-maintainer already has a review request.");
			if (!probe.Evidence.ReviewerNotRepeated) {
				return fail("request-repeated");
			}
			measureReviewerCost();
			// [[BPR-1849]]. GitHub refuses the next request: People shows the
			// person at once, then withdraws them, and the picker says why.
			implementation->Fixture->stage_reviewer_request_refusal();
			for (int erase = 0; erase < 5; ++erase) {
				auto back = QKeyEvent{ QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &back);
			}
			for (const auto character : QStringLiteral("other")) {
				auto down = QKeyEvent{ QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString{ character } };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			probe.Phase = state_type::phase::awaiting_reviewer_refusal_listed;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_refusal_listed: {
			auto* const picker = window->overview_page()->reviewer_picker_popup();
			note("query=" + implementation->ReviewerPicker.Query + " listed=" + picker->listed_logins().join(QLatin1Char{ ',' }).toStdString());
			if (implementation->ReviewerPicker.Searching || implementation->ReviewerPicker.Query != "other") {
				return result::pending;
			}
			if (picker->listed_logins() != QStringList{ "fixture-other-reviewer" }) {
				return fail("refusal-reviewer-not-listed");
			}
			for (const auto key : { Qt::Key_Down, Qt::Key_Return }) {
				auto down = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
				QCoreApplication::sendEvent(picker->search_field(), &down);
			}
			probe.Evidence.ReviewerRolledBack = reviewerRow("fixture-other-reviewer") == std::optional{ true };
			probe.Phase                       = state_type::phase::awaiting_reviewer_refused;
			return result::pending;
		}
		case state_type::phase::awaiting_reviewer_refused: {
			auto* const picker = window->overview_page()->reviewer_picker_popup();
			note("failures=" + std::to_string(implementation->ReviewerRequestFailures) + " status=" + picker->status()->text().toStdString());
			if (implementation->ReviewerRequestFailures < 1U) {
				return result::pending;
			}
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && !refresh->isEnabled()) {
				return result::pending;
			}
			probe.Evidence.ReviewerRolledBack = probe.Evidence.ReviewerRolledBack && !reviewerRow("fixture-other-reviewer").has_value()
				&& reviewerRow("octo-maintainer") == std::optional{ false } && implementation->Fixture->review_mutation_evidence().ReviewerWrites == 1U
				&& picker->status()->text() == QStringLiteral("GitHub did not let you request a review from fixture-other-reviewer.");
			measureReviewerCost();
			const auto& evidence           = probe.Evidence;
			probe.Evidence.ReviewerMinimal = evidence.ReviewerReads == 0 && evidence.ReviewerOverviewBuilds == 0U && !evidence.ReviewerRefreshInFlight
				&& evidence.ReviewerShownAtOnce && evidence.ReviewerConfirmed && evidence.ReviewerRolledBack;
			picker->hide();
			QCoreApplication::processEvents();
			return goOnToDraftState();
		}
		// [[BPR-1852]]. The author of a draft sees it said in the title row and
		// in Overview, with Ready for review in the banner and the Pull
		// Request menu. Ready for review takes the chips away at once, writes
		// exactly once and reads nothing; GitHub's answer offers Convert to
		// draft. A refused Convert to draft is shown at once, then withdrawn,
		// with words beside the button.
		case state_type::phase::awaiting_draft_offered: {
			auto* const page = window->overview_page();
			window->choose_modality(qt::review_modality::overview);
			QCoreApplication::processEvents();
			note("offer=" + std::to_string(page->draft_state_offer().has_value() ? static_cast<int>(*page->draft_state_offer()) : -1) + " chip=" + std::to_string(page->draft_chip()->isVisible() ? 1 : 0));
			if (page->draft_state_offer() != core::draft_state_change::ready_for_review || !page->projection().has_value()) {
				return result::pending;
			}
			if (auto* const refresh = window->github_refresh_action(); refresh != nullptr && !refresh->isEnabled()) {
				return result::pending;
			}
			// What the courses before began settles first: the Overview is
			// projected from the accepted read and has not been projected
			// again for a few turns of the event loop, so what follows is
			// counted from the gesture alone.
			if (!implementation->WindowRead || !implementation->WindowRead->accepted_snapshot()
				|| implementation->OverviewResolutionsSnapshot != std::optional{ implementation->WindowRead->accepted_snapshot()->Snapshot }) {
				return result::pending;
			}
			if (probe.DraftQuietBuilds != std::optional{ implementation->OverviewBuilds }) {
				probe.DraftQuietBuilds = implementation->OverviewBuilds;
				probe.DraftQuietTicks  = 0U;
				return result::pending;
			}
			if (++probe.DraftQuietTicks < 5U) {
				return result::pending;
			}
			auto* const action          = window->draft_state_action();
			probe.Evidence.DraftOffered = window->strip_draft_shown() && page->draft_chip()->isVisible()
				&& page->ready_for_review_button()->isVisible() && page->ready_for_review_button()->isEnabled() && action != nullptr && action->isVisible()
				&& action->text().remove(QLatin1Char{ '&' }) == QStringLiteral("Mark Ready for Review") && !page->convert_to_draft_button()->isVisible();
			if (!probe.Evidence.DraftOffered) {
				return fail("draft-not-offered strip=" + std::to_string(window->strip_draft_shown() ? 1 : 0) + " ready=" + std::to_string(page->ready_for_review_button()->isVisible() ? 1 : 0));
			}
			probe.DraftBaseline = state_type::reviewer_baseline{
				.Reads          = implementation->Fixture->evidence().AccountReadCount,
				.OverviewBuilds = implementation->OverviewBuilds,
				.OverviewMoves  = implementation->OverviewDraftStateUpdates,
				.Chosen         = std::chrono::steady_clock::now(),
			};
			page->ready_for_review_button()->click();
			// The gesture dispatched synchronously: the chips are gone now, and
			// the button says GitHub is being asked.
			probe.Evidence.DraftShownAtOnce = !window->strip_draft_shown() && !page->draft_chip()->isVisible() && page->ready_for_review_button()->isVisible()
				&& page->ready_for_review_button()->text() == QStringLiteral("Marking ready…") && !page->ready_for_review_button()->isEnabled()
				&& page->projection().has_value() && !page->projection()->Draft && page->projection()->DraftPending;
			note("shown strip=" + std::to_string(window->strip_draft_shown() ? 1 : 0) + " chip=" + std::to_string(page->draft_chip()->isVisible() ? 1 : 0)
				 + " button=" + page->ready_for_review_button()->text().toStdString()
				 + " enabled=" + std::to_string(page->ready_for_review_button()->isEnabled() ? 1 : 0)
				 + " draft=" + std::to_string(page->projection().has_value() && page->projection()->Draft ? 1 : 0)
				 + " pending=" + std::to_string(page->projection().has_value() && page->projection()->DraftPending ? 1 : 0));
			probe.Phase = state_type::phase::awaiting_draft_ready;
			return result::pending;
		}
		case state_type::phase::awaiting_draft_ready: {
			auto* const page     = window->overview_page();
			const auto  evidence = implementation->Fixture->review_mutation_evidence();
			note("completions=" + std::to_string(implementation->DraftStateCompletions) + " failures=" + std::to_string(implementation->DraftStateFailures)
				 + " writes=" + std::to_string(evidence.DraftStateWrites) + " status=" + page->draft_state_status(core::draft_state_change::ready_for_review)->text().toStdString());
			if (implementation->DraftStateFailures != 0U || implementation->DraftStateRefusals != 0U) {
				return fail("ready-refused-or-failed");
			}
			if (implementation->DraftStateCompletions < 1U) {
				return result::pending;
			}
			auto* const refresh = window->github_refresh_action();
			if (probe.Evidence.DraftAnsweredMicros < 0) {
				probe.Evidence.DraftAnsweredMicros  = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - probe.DraftBaseline->Chosen).count();
				probe.Evidence.DraftRefreshInFlight = refresh != nullptr && !refresh->isEnabled();
			}
			probe.Evidence.DraftReads           = implementation->Fixture->evidence().AccountReadCount - probe.DraftBaseline->Reads;
			probe.Evidence.DraftOverviewBuilds  = static_cast<std::size_t>(implementation->OverviewBuilds - probe.DraftBaseline->OverviewBuilds);
			probe.Evidence.DraftOverviewUpdates = implementation->OverviewDraftStateUpdates - probe.DraftBaseline->OverviewMoves;
			probe.Evidence.DraftReadyOnce       = evidence.DraftStateChanges == 1U && evidence.DraftStateWrites == 1U && !page->ready_for_review_button()->isVisible()
				&& !page->draft_chip()->isVisible() && !window->strip_draft_shown() && page->projection().has_value() && !page->projection()->Draft
				&& !page->projection()->DraftPending && page->draft_state_offer() == core::draft_state_change::convert_to_draft && page->convert_to_draft_button()->isVisible()
				&& window->draft_state_action()->text().remove(QLatin1Char{ '&' }) == QStringLiteral("Convert to Draft");
			if (!probe.Evidence.DraftReadyOnce) {
				return fail("ready-not-confirmed");
			}
			// GitHub refuses the next change, asked from the menu: the chips
			// come back at once, then go, and the words say why.
			implementation->Fixture->stage_draft_state_refusal();
			window->draft_state_action()->trigger();
			probe.Evidence.DraftRolledBack = window->strip_draft_shown() && page->draft_chip()->isVisible() && page->projection()->DraftPending;
			note("converting strip=" + std::to_string(window->strip_draft_shown() ? 1 : 0) + " chip=" + std::to_string(page->draft_chip()->isVisible() ? 1 : 0)
				 + " pending=" + std::to_string(page->projection()->DraftPending ? 1 : 0) + " draft=" + std::to_string(page->projection()->Draft ? 1 : 0));
			probe.Phase = state_type::phase::awaiting_draft_refused;
			return result::pending;
		}
		case state_type::phase::awaiting_draft_refused: {
			auto* const page   = window->overview_page();
			auto* const status = page->draft_state_status(core::draft_state_change::convert_to_draft);
			note("failures=" + std::to_string(implementation->DraftStateFailures) + " status=" + status->text().toStdString());
			if (implementation->DraftStateFailures < 1U) {
				return result::pending;
			}
			probe.Evidence.DraftRolledBack = probe.Evidence.DraftRolledBack && !window->strip_draft_shown() && !page->draft_chip()->isVisible()
				&& !page->projection()->Draft && implementation->Fixture->review_mutation_evidence().DraftStateWrites == 1U && status->isVisible()
				&& status->text() == QStringLiteral("GitHub did not let you change whether this pull request is a draft. Nothing changed.")
				&& page->convert_to_draft_button()->isVisible() && page->convert_to_draft_button()->isEnabled();
			const auto& evidence        = probe.Evidence;
			probe.Evidence.DraftMinimal = evidence.DraftReads == 0 && evidence.DraftOverviewBuilds == 0U && !evidence.DraftRefreshInFlight && evidence.DraftShownAtOnce
				&& evidence.DraftReadyOnce && evidence.DraftRolledBack;
			if (!probe.Evidence.DraftRolledBack) {
				return fail("refusal-not-withdrawn");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed:
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::mark_fixture_read_through(
		github_application& aApplication, const std::string& aThread, const std::string& aComment) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->mark_conversation_read_through(aThread, aComment);
	}

	auto detail::github_application_fixture_access::apply_fixture_read_state(github_application& aApplication, const std::string& aFixture)
		-> std::optional<std::size_t>
	{
		auto file = QFile{ QString::fromStdString((std::filesystem::path{ BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT } / aFixture / "read_state.json").string()) };
		if (!file.open(QIODevice::ReadOnly)) {
			return std::nullopt;
		}
		const auto threads = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("threads")).toObject();
		auto       marked  = std::size_t{ 0U };
		for (auto entry = threads.begin(); entry != threads.end(); ++entry) {
			if (entry.value().isNull()) {
				continue;
			}
			if (!mark_fixture_read_through(aApplication, entry.key().toStdString(), entry.value().toString().toStdString())) {
				return std::nullopt;
			}
			++marked;
		}
		return marked;
	}

	auto detail::github_application_fixture_access::reveal_fixture_file(
		github_application& aApplication, const std::string_view aPath) -> detail::fixture_file_reveal_result
	{
		using result               = detail::fixture_file_reveal_result;
		using state_type           = fixture_file_reveal_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->FileReveal.has_value() || implementation->FileReveal->Path != aPath) {
			implementation->FileReveal = state_type{ .Path = std::string{ aPath } };
		}
		auto& reveal = *implementation->FileReveal;
		if (reveal.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (reveal.Phase == state_type::phase::shown) {
			return result::shown;
		}
		if (implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const view   = window->files_drawer() != nullptr ? window->files_drawer()->contents_view() : nullptr;
		if (reveal.Phase == state_type::phase::awaiting_files) {
			const auto row = drawer_row(*window, aPath);
			if (view == nullptr || !row.isValid() || window->showing_home()) {
				return result::pending;
			}
			// The reviewer's way in: Review on the strip, then the file's row.
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			// [[BPR-1345]]. Choosing Review can re-project the drawer, so the
			// row is found again: a stale index selects nothing and the reveal
			// would wait for a diff no one asked for.
			const auto shown = drawer_row(*window, aPath);
			if (!shown.isValid()) {
				return result::pending;
			}
			view->setCurrentIndex(shown);
			reveal.Phase = state_type::phase::awaiting_diff;
			return result::pending;
		}
		const auto& last = implementation->LastSelectedDiff;
		if (!last.has_value() || last->Request.File.PresentationPath != aPath
			|| window->comparison_surface()->current_result() == nullptr
			|| window->current_modality() != qt::review_modality::review) {
			return result::pending;
		}
		reveal.Phase = state_type::phase::shown;
		return result::shown;
	}

	auto detail::github_application_fixture_access::accepted_head(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->LastAcceptedHandoff.has_value()) {
			return { };
		}
		return std::string{ core::canonical_hex(implementation->LastAcceptedHandoff->Head).view() };
	}

	auto detail::github_application_fixture_access::window_read_progress(const github_application& aApplication) -> window_read_progress_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->WindowRead) {
			return { };
		}
		const auto& snapshot = implementation->WindowRead->accepted_snapshot();
		return {
			.Snapshot = snapshot ? std::optional{ snapshot->Snapshot.value() } : std::nullopt,
			.Reading  = implementation->WindowRead->state().PendingPullRequest.has_value() || implementation->RereadOwed || implementation->RereadQueued,
		};
	}

	auto detail::github_application_fixture_access::seed_attended_comment_draft(
		github_application& aApplication, const core::diff_side aSide, const std::string& aBody) -> detail::fixture_file_reveal_result
	{
		using result               = detail::fixture_file_reveal_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr) {
			return result::failed;
		}
		auto* const window = &*implementation->Window;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (editor == nullptr || model == nullptr) {
			return result::pending;
		}
		const auto& draft = implementation->ReviewDraft;
		if (draft && draft->Body == aBody && draft->State == core::review_draft_state::drafting && editor->body_editor()->toPlainText().toStdString() == aBody) {
			return result::shown;
		}
		if (window->current_modality() != qt::review_modality::review) {
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			return result::pending;
		}
		// Chosen once and then kept selected, as the label-authoring course does.
		auto& line = implementation->AttendedDraftLine;
		if (!line) {
			// [[BPR-1735]]. A line GitHub places in its own diff, once that
			// diff is read where GitHub names a merge base: a line outside
			// every hunk publishes as a file comment, not the line comment
			// these courses write.
			const auto lines = implementation->shown_github_diff_lines();
			if (!lines) {
				return result::pending;
			}
			for (auto candidate = std::uint32_t{ 0U }; candidate < 64U && !line; ++candidate) {
				if (core::place_on_github_diff(*lines, aSide, candidate, std::nullopt) != core::github_diff_placement::outside_diff
					&& window->comparison_surface()->select_conversation_line(aSide, candidate)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					line = candidate;
				}
			}
			return line ? result::pending : result::failed;
		}
		if (const auto selected = window->comparison_surface()->selection();
			!selected || selected->PreferredLine.Side != aSide || selected->PreferredLine.Line != *line) {
			static_cast<void>(window->comparison_surface()->select_conversation_line(aSide, *line));
		}
		if (!draft || draft->ReplyToThread || draft->Anchor.Line != *line || draft->Anchor.Side != aSide
			|| draft->State != core::review_draft_state::drafting || !window->comment_action()->isEnabled()) {
			return result::pending;
		}
		window->comment_action()->trigger();
		QCoreApplication::processEvents();
		if (editor->body_editor()->isReadOnly()) {
			return result::failed;
		}
		editor->body_editor()->setPlainText(QString::fromStdString(aBody));
		return result::pending;
	}

	auto detail::github_application_fixture_access::exercise_label_authoring(
		github_application& aApplication, const std::optional<std::string>& aEvidenceDirectory, const bool aLineDraftOnly) -> detail::label_authoring_probe_result
	{
		using result               = detail::label_authoring_probe_result;
		using state_type           = label_authoring_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->LabelAuthoringProbe.has_value()) {
			implementation->LabelAuthoringProbe.emplace();
			implementation->LabelAuthoringProbe->LineDraftOnly = aLineDraftOnly;
			// [[BPR-1735]]. This course's lines lie outside GitHub's diff of its
			// fixture and it publishes them natively; its subject is the
			// labels, so it places as a build before the hunk rule did.
			implementation->WholeDiffPlacementDisabled = true;
		}
		auto& probe = *implementation->LabelAuthoringProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr || implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (cards == nullptr || editor == nullptr || model == nullptr || editor->label_controls() == nullptr) {
			return result::pending;
		}
		auto* const controls = editor->label_controls();
		auto* const body     = editor->body_editor();
		const auto  fail     = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		// Layout evidence for the implementer's own look, never a substitute
		// for the owner's.
		const auto render = [&probe, window, &aEvidenceDirectory](const char* const aName) {
			if (!aEvidenceDirectory.has_value()) {
				return;
			}
			++probe.Renders;
			static_cast<void>(window->grab().save(QDir{ QString::fromStdString(*aEvidenceDirectory) }.filePath(QStringLiteral("%1-%2.png").arg(probe.Renders).arg(QString::fromLatin1(aName)))));
		};
		// One key as a keyboard delivers it: the shortcut pass first, then the press.
		const auto key = [body](const int aKey, const Qt::KeyboardModifiers aModifiers, const QString& aText) {
			auto overrideEvent = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers, aText };
			QCoreApplication::sendEvent(body, &overrideEvent);
			auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers, aText };
			QCoreApplication::sendEvent(body, &press);
			auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers, aText };
			QCoreApplication::sendEvent(body, &release);
			QCoreApplication::processEvents();
		};
		// [[BPR-1212]]. The same, to a code pane.
		const auto paneKey = [](QWidget& aPane, const int aKey, const Qt::KeyboardModifiers aModifiers) {
			auto overrideEvent = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers };
			QCoreApplication::sendEvent(&aPane, &overrideEvent);
			auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
			QCoreApplication::sendEvent(&aPane, &press);
			auto release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers };
			QCoreApplication::sendEvent(&aPane, &release);
			QCoreApplication::processEvents();
		};
		const auto text = [body] { return body->toPlainText(); };
		// Types the next queued character once the draft holds what the body
		// shows; true when nothing is left to type and the two agree.
		const auto typed = [&probe, &key, &text, implementation] {
			const auto& draft  = implementation->ReviewDraft;
			const auto  caught = draft.has_value() && QString::fromStdString(draft->Body) == text();
			if (!caught) {
				return false;
			}
			if (probe.Typing.isEmpty()) {
				return true;
			}
			const auto character = probe.Typing.front();
			probe.Typing.remove(0, 1);
			key(static_cast<int>(character.toUpper().unicode()), Qt::NoModifier, QString{ character });
			return false;
		};
		const auto cardFor = [cards](const std::string_view aId) -> const qt::thread_card* {
			const auto found = std::ranges::find(cards->cards(), std::string{ aId }, &qt::thread_card::Id);
			return found == cards->cards().end() ? nullptr : &*found;
		};
		const auto rootLabel = [](const qt::thread_card* const aCard) -> std::optional<core::feedback_label> {
			return aCard == nullptr || aCard->Posts.empty() ? std::nullopt : aCard->Posts.front().Label;
		};
		constexpr auto path      = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto subject   = std::string_view{ "Should the width be clamped to one pixel?" };
		constexpr auto published = std::string_view{ "issue: Should the width be clamped to one pixel?" };
		// [[BPR-1084]]. The suggested-change bodies: a multiline replacement
		// with tabs, prose with no patch, a patch the fixture's GitHub refuses
		// an inline comment for, and a reply with a patch.
		const auto patchBody   = QStringLiteral("issue: Clamp the width to one pixel.\n\n```suggestion\n\tconst auto width = std::max(1, requested);\n\treturn width;\n```\n");
		const auto proseBody   = QStringLiteral("suggestion: Consider clamping the width to one pixel.");
		const auto refusedBody = QStringLiteral("issue: BPR-1084 off-hunk suggestion.\n\n```suggestion\n\tclamp(width, 1);\n```\n");
		const auto replyBody   = QStringLiteral("question: Would this read better?\n\n```suggestion:-1+0\n\tdraw_scene(view);\n```\n");
		const auto markState   = [editor]() -> std::string {
			const auto* const mark = editor->suggestion_mark_widget();
			if (mark == nullptr) {
				return "-";
			}
			return mark->isVisibleTo(editor) ? mark->property("suggestionApplicability").toString().toStdString() : std::string{ "hidden" };
		};
		// A root comment on the next current-head line no thread is on.
		const auto rootDraft = [&](const QString& aBody, const state_type::phase aNext) -> result {
			// Back from the last comment's page to the code, in Review.
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				auto* const back = inlinePage->findChild<QPushButton*>(QStringLiteral("composeInlineDraftPageBack"));
				if (back == nullptr) {
					return fail("no-back");
				}
				back->click();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return result::pending;
			}
			// [[BPR-1305]]. Its refreshed comparison must answer before the line is selected.
			const auto& publication = implementation->SelectedDiffPublication;
			const auto& comparison  = window->comparison_surface()->current_result();
			if (!publication || !comparison || comparison->Alignment.Key != publication->Key
				|| implementation->WorkbenchHost->generation() != publication->Request.Context.Generation
				|| implementation->WorkbenchHost->selected() != std::optional{ publication->Request.File.File }
				|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
				note("awaiting-current-comparison");
				return result::pending;
			}
			// Chosen once and then kept selected: choosing again each tick
			// would move the selection away and back before its draft arrives.
			if (!probe.Line) {
				for (auto candidate = std::uint32_t{ 0U }; candidate < 64U && !probe.Line; ++candidate) {
					if (window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate)
						&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
						probe.Line = candidate;
					}
				}
			} else if (const auto selected = window->comparison_surface()->selection();
					   !selected || selected->PreferredLine.Side != core::diff_side::right || selected->PreferredLine.Line != *probe.Line) {
				static_cast<void>(window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Line));
			}
			const auto  line  = probe.Line;
			const auto& draft = implementation->ReviewDraft;
			note("line=" + std::to_string(line.value_or(0U)) + " draft=" + (draft ? std::to_string(draft->Anchor.Line) + ":" + std::string{ core::name(draft->State) } : std::string{ "-" })
				 + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0));
			if (!line || !draft || draft->ReplyToThread || draft->Anchor.Line != *line || draft->State != core::review_draft_state::drafting
				|| !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (body->isReadOnly()) {
				return fail("suggestion-page-not-editable");
			}
			probe.Anchor = draft->Anchor;
			probe.Line.reset();
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			body->setPlainText(aBody);
			probe.Phase = aNext;
			return result::pending;
		};
		// The draft holds the body; the mark beside Publish is checked before
		// Publish is pressed.
		const auto rootBody = [&](const QString& aBody, const std::string_view aMark, bool& aEvidence, const char* const aRender, const state_type::phase aNext) -> result {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + (draft ? draft->Body : std::string{ "-" }) + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0) + " mark=" + markState());
			if (!draft || draft->Body != aBody.toStdString() || !editor->publish_action()->isEnabled() || text() != aBody
				|| implementation->PrivateDraftCommandPending || implementation->PrivateDraftSelectionPending
				|| !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready.") {
				return result::pending;
			}
			aEvidence = markState() == aMark && probe.Anchor && draft->Anchor == *probe.Anchor;
			render(aRender);
			if (!aEvidence) {
				return fail("mark-before-publish expected=" + std::string{ aMark } + " shown=" + markState());
			}
			editor->publish_action()->trigger();
			probe.Phase = aNext;
			return result::pending;
		};
		// One native post, received exactly as drafted on the drafted line,
		// and its thread back from the application's own refresh.
		const auto rootPublished = [&](const QString& aBody, const core::feedback_label aLabel, bool& aEvidence, const bool aCombine, const state_type::phase aNext) -> result {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto found     = std::ranges::find_if(cards->cards(), [&](const qt::thread_card& aCard) { return !aCard.Posts.empty() && aCard.Posts.front().Body == aBody; });
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts)
				 + " lastLine=" + std::to_string(mutations.LastLine.value_or(0U)) + " anchor=" + std::to_string(probe.Anchor ? probe.Anchor->Line : 0U)
				 + " side=" + mutations.LastSide + " card=" + std::to_string(found != cards->cards().end() ? 1 : 0) + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || found == cards->cards().end()
				|| !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U) {
				return result::pending;
			}
			const auto bytes = aBody.toStdString();
			// The prose case also carries its before-Publish check.
			aEvidence = (!aCombine || aEvidence) && mutations.NativePosts == probe.NativeBefore + 1U && mutations.FilePosts == 0U && !mutations.LastFile
				&& mutations.LastBody == bytes && mutations.LastPath == path && mutations.LastSide == "RIGHT" && probe.Anchor && mutations.LastLine == probe.Anchor->Line + 1U
				&& found->Posts.front().Label == aLabel;
			if (!aEvidence) {
				return fail("published-not-exact body=" + mutations.LastBody + " path=" + mutations.LastPath);
			}
			probe.Phase = aNext;
			return result::pending;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_review_empty: {
			// [[BPR-1287]]. Review before any file is chosen.
			auto* const surface = window->comparison_surface();
			note("files=" + std::to_string(drawer_row(*window, path).isValid() ? 1 : 0) + " idle=" + std::to_string(surface->idle() ? 1 : 0));
			if (implementation->WorkbenchHost->outstanding_requests() != 0U || !drawer_row(*window, path).isValid() || window->showing_home()) {
				return result::pending;
			}
			window->choose_modality(qt::review_modality::review);
			QCoreApplication::processEvents();
			probe.Evidence.ReviewEmptyDirected = window->current_modality() == qt::review_modality::review && surface->idle()
				&& surface->idle_page()->isVisibleTo(window) && !surface->left_pane()->isVisibleTo(window)
				&& surface->idle_open_first_button()->isEnabled();
			render("review-no-file");
			probe.Phase = state_type::phase::awaiting_file;
			return result::pending;
		}
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::pending) {
				return result::pending;
			}
			probe.Evidence.FileShown = true;
			probe.Phase              = state_type::phase::awaiting_marks;
			return result::pending;
		}
		case state_type::phase::awaiting_marks: {
			const auto* const first  = cardFor("review-thread:PRRT_cc_1");
			const auto* const second = cardFor("review-thread:PRRT_cc_2");
			note("cards=" + std::to_string(cards->cards().size()) + " first=" + std::to_string(first != nullptr ? 1 : 0)
				 + " second=" + std::to_string(second != nullptr ? 1 : 0) + " visible=" + std::to_string(cards->isVisibleTo(window) ? 1 : 0));
			if (first == nullptr || second == nullptr || !cards->isVisibleTo(window)) {
				return result::pending;
			}
			// renderer.cpp's two threads: issue (blocking), question
			// (non-blocking), praise and a plain reply; suggestion
			// (non-blocking, if-minor), todo (blocking, if-minor, perf) and
			// nitpick (style). Several and custom decorations are kept, shown
			// as written in the text, and styled by weight and the if-minor
			// corner (owner, decorations-v003).
			using weight      = qt::feedback_mark_weight;
			const auto marked = rootLabel(first) == core::feedback_label::issue && first->Posts.front().Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::blocking }
				&& first->Posts.size() == 4U && first->Posts[1].Label == core::feedback_label::question && first->Posts[1].Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::non_blocking }
				&& first->Posts[2].Label == core::feedback_label::praise && !first->Posts[3].Label.has_value()
				&& rootLabel(second) == core::feedback_label::suggestion && second->Posts.front().Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::non_blocking, .IfMinor = true }
				&& second->Posts.front().Body.startsWith(QStringLiteral("suggestion (non-blocking, if-minor): ")) && second->Posts.size() == 3U
				&& second->Posts[1].Emphasis == qt::feedback_mark_emphasis{ .Weight = weight::blocking, .IfMinor = true } && second->Posts[1].Name.endsWith(QStringLiteral("todo (blocking, if-minor, perf)"))
				&& second->Posts[2].Label == core::feedback_label::nitpick && second->Posts[2].Emphasis == qt::feedback_mark_emphasis{ } && second->Posts[2].Body.startsWith(QStringLiteral("nitpick (style): "))
				// The drawer's notes show their root's mark under its avatar
				// (owner, thread-marks-v002: "Marks don't show on inline
				// comments").
				&& !cards->mark_rect(*first).isEmpty() && !cards->mark_rect(*second).isEmpty();
			if (!marked) {
				return fail("thread-marks-not-drawn");
			}
			probe.Evidence.ThreadMarksDrawn = true;
			render("read-renderer-marks");
			// [[BPR-1212]]. `--line-draft-course` goes straight to the code's
			// keys, before any publication.
			probe.Phase = probe.LineDraftOnly ? state_type::phase::awaiting_left_refused : state_type::phase::awaiting_line_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_line_draft: {
			// The first current-head line no thread is on, chosen in Review.
			auto line = std::optional<std::uint32_t>{ };
			for (auto candidate = std::uint32_t{ 0U }; candidate < 32U && !line; ++candidate) {
				if (window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					line = candidate;
				}
			}
			const auto& draft = implementation->ReviewDraft;
			note("line=" + std::to_string(line.value_or(0U)) + " draft=" + std::to_string(draft.has_value() ? 1 : 0)
				 + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0));
			if (!line || !draft || draft->ReplyToThread || !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			// [[BPR-1212]]. Comment opens the draft in the drawer; the label
			// authoring here is the full composer's, which Maximize opens in
			// Discuss with the same draft.
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (cards == nullptr || !cards->writing_draft() || cards->draft_editor() == nullptr) {
				return fail("comment-drawer-draft-not-open");
			}
			window->open_draft_in_discuss();
			QCoreApplication::processEvents();
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage == nullptr || !inlinePage->isVisibleTo(window) || body->isReadOnly() || !controls->isVisibleTo(window)) {
				return fail("comment-page-not-editable");
			}
			probe.Evidence.CommentEditable = true;
			body->setFocus();
			// [[BPR-1565]]. The `:xx:` codes are gone (owner, 2026-10-16): a
			// typed `:qu:` stays the text it is, with the helper on.
			probe.Typing = QStringLiteral(":qu: ") + QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size()));
			probe.Phase  = state_type::phase::awaiting_literal_code;
			return result::pending;
		}
		case state_type::phase::awaiting_literal_code: {
			const auto subjectText = QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size()));
			const auto literal     = QStringLiteral(":qu: ") + subjectText;
			note("body=" + text().toStdString() + " draft=" + (implementation->ReviewDraft ? implementation->ReviewDraft->Body : std::string{ "-" })
				 + " typing=" + probe.Typing.toStdString() + " status=" + editor->mutation_status_label()->text().toStdString());
			if (!typed()) {
				return result::pending;
			}
			if (text() == literal) {
				probe.Evidence.CodeStaysLiteral = !controls->state().Label.has_value() && controls->statistics().Edits == 0U;
				render("editor-literal-code");
				// The reviewer takes the code out by hand: one selection and
				// Backspace, so the draft follows as typing does.
				auto cursor = body->textCursor();
				cursor.setPosition(0);
				cursor.setPosition(5, QTextCursor::KeepAnchor);
				body->setTextCursor(cursor);
				key(Qt::Key_Backspace, Qt::NoModifier, QString{ });
				return result::pending;
			}
			if (text() != subjectText) {
				return fail("literal-code-changed body=" + text().toStdString());
			}
			controls->chip()->click();
			QCoreApplication::processEvents();
			auto* const choice = controls->menu() != nullptr ? controls->menu()->label_button(core::feedback_label::suggestion) : nullptr;
			if (choice == nullptr || !controls->menu()->isVisible()) {
				return fail("chip-menu-not-open");
			}
			choice->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_chip;
			return result::pending;
		}
		case state_type::phase::awaiting_chip: {
			note("body=" + text().toStdString() + " menu=" + std::to_string(controls->menu()->isVisible() ? 1 : 0));
			if (!text().startsWith(QStringLiteral("suggestion: "))) {
				return result::pending;
			}
			probe.Evidence.ChipApplied = controls->state().Label == core::feedback_label::suggestion && !controls->menu()->isVisible()
				&& text() == QStringLiteral("suggestion: ") + QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size()));
			const auto opens = controls->statistics().MenuOpens;
			key(Qt::Key_L, Qt::ControlModifier, QString{ });
			auto* const choice = controls->menu()->label_button(core::feedback_label::issue);
			if (controls->statistics().MenuOpens != opens + 1U || !controls->menu()->isVisible() || choice == nullptr) {
				return fail("command-l-did-not-open-the-menu");
			}
			render("label-menu");
			choice->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_shortcut;
			return result::pending;
		}
		case state_type::phase::awaiting_shortcut: {
			const auto& draft = implementation->ReviewDraft;
			note("body=" + text().toStdString() + " draft=" + (draft ? draft->Body : std::string{ "-" })
				 + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0));
			if (text().toStdString() != published || !draft || draft->Body != published || !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			probe.Evidence.ShortcutApplied = controls->state().Label == core::feedback_label::issue;
			render("editor-labelled");
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_published;
			return result::pending;
		}
		case state_type::phase::awaiting_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0)
				 + " native=" + std::to_string(mutations.NativePosts) + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == 0U) {
				return result::pending;
			}
			probe.Evidence.Published = mutations.NativePosts == 1U && mutations.StoredComments == 1U;
			auto* const refresh      = window->github_refresh_action();
			if (refresh == nullptr) {
				return fail("refresh-unavailable");
			}
			// The application refreshes once after publishing, and that
			// refresh holds the action until it answers.
			if (!refresh->isEnabled()) {
				return result::pending;
			}
			refresh->trigger();
			// [[BPR-1292]]. The core takes the refresh on the dispatch: the
			// action is held and the status says so before trigger returns.
			// A refresh it refused would leave nothing to wait for.
			if (refresh->isEnabled() || window->github_status_text() != "Refreshing the pull request.") {
				return fail("refresh-not-taken status=" + window->github_status_text());
			}
			probe.Phase = state_type::phase::awaiting_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_refreshed: {
			const auto  thread = model->index_for_id("review-thread:PRRT_composed_1");
			const auto* card   = cardFor("review-thread:PRRT_composed_1");
			const auto  ready  = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			note("thread=" + std::to_string(thread.isValid() ? 1 : 0) + " card=" + std::to_string(card != nullptr ? 1 : 0)
				 + " cards=" + std::to_string(cards->cards().size()) + " ready=" + std::to_string(ready ? 1 : 0));
			// [[BPR-1292]]. The card is already there from the application's
			// own refresh, so it says nothing about this one. The course's
			// refresh has answered when the pull request is ready again and
			// the action is released; until then its reads are in flight, and
			// ending the run there cut the fixture's script short.
			if (!ready || card == nullptr) {
				return result::pending;
			}
			probe.Evidence.PublishedMarked = rootLabel(card) == core::feedback_label::issue && card->Posts.front().Emphasis == qt::feedback_mark_emphasis{ }
				&& card->Posts.front().Name.endsWith(QStringLiteral(", issue")) && !cards->mark_rect(*card).isEmpty();
			render("published-marked");
			if (!probe.Evidence.PublishedMarked) {
				return fail("published-thread-unmarked name=" + card->Posts.front().Name.toStdString());
			}
			probe.Phase = state_type::phase::awaiting_patch_draft;
			return result::pending;
		}
		// [[BPR-1084]]. Suggested changes through the same public path: an
		// issue with a patch, a suggestion with none, a patch on a line GitHub
		// refuses an inline comment on, and a reply in place with a patch.
		case state_type::phase::awaiting_patch_draft:
			return rootDraft(patchBody, state_type::phase::awaiting_patch_body);
		case state_type::phase::awaiting_patch_body:
			return rootBody(patchBody, "native", probe.Evidence.PatchMarkedNative, "editor-patch-native", state_type::phase::awaiting_patch_published);
		case state_type::phase::awaiting_patch_published:
			return rootPublished(patchBody, core::feedback_label::issue, probe.Evidence.PatchPublishedExact, false, state_type::phase::awaiting_prose_draft);
		case state_type::phase::awaiting_prose_draft:
			return rootDraft(proseBody, state_type::phase::awaiting_prose_body);
		case state_type::phase::awaiting_prose_body:
			return rootBody(proseBody, "hidden", probe.Evidence.ProseUnmarkedExact, "editor-prose-unmarked", state_type::phase::awaiting_prose_published);
		case state_type::phase::awaiting_prose_published:
			return rootPublished(proseBody, core::feedback_label::suggestion, probe.Evidence.ProseUnmarkedExact, true, state_type::phase::awaiting_refused_draft);
		case state_type::phase::awaiting_refused_draft:
			return rootDraft(refusedBody, state_type::phase::awaiting_refused_body);
		case state_type::phase::awaiting_refused_body: {
			// Before the first attempt nothing says GitHub will refuse this
			// line, so the mark says native: GitHub is asked for an inline
			// comment, and a refused suggestion never becomes a file comment.
			auto asked = false;
			return rootBody(refusedBody, "native", asked, "editor-refused-before", state_type::phase::awaiting_refused);
		}
		case state_type::phase::awaiting_refused: {
			const auto  mutations = implementation->Fixture->review_mutation_evidence();
			const auto& draft     = implementation->ReviewDraft;
			const auto  status    = editor->mutation_status_label()->text();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts)
				 + " file=" + std::to_string(mutations.FilePosts) + " lastLine=" + std::to_string(mutations.LastLine.value_or(0U)) + " anchor=" + std::to_string(probe.Anchor ? probe.Anchor->Line : 0U)
				 + " state=" + (draft ? std::string{ core::name(draft->State) } : std::string{ "-" }) + " mark=" + markState() + " status=" + status.toStdString());
			if (implementation->PendingReviewMutation.has_value() || !status.contains(QStringLiteral("needs a supported inline line anchor"))
				|| !draft || draft->State != core::review_draft_state::drafting) {
				return result::pending;
			}
			probe.Evidence.RefusedTextKept = mutations.NativePosts == probe.NativeBefore + 1U && mutations.FilePosts == 0U && !mutations.LastFile
				&& mutations.LastBody == refusedBody.toStdString() && probe.Anchor && mutations.LastLine == probe.Anchor->Line + 1U && mutations.LastSide == "RIGHT"
				&& draft->Anchor == *probe.Anchor && draft->Body == refusedBody.toStdString() && text() == refusedBody
				// [[BPR-1326]]. No thread holds it; my kept draft is a note of its own.
				&& std::ranges::none_of(cards->cards(), [&](const qt::thread_card& aCard) { return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == refusedBody; });
			const auto* const mark               = editor->suggestion_mark_widget();
			probe.Evidence.RefusedMarkedTextOnly = markState() == "textual_only" && mark->decision().Reason == core::suggestion_text_reason::inline_comment_unsupported;
			render("editor-refused-text-only");
			if (!probe.Evidence.RefusedTextKept || !probe.Evidence.RefusedMarkedTextOnly) {
				return fail("refused-patch kept=" + std::to_string(probe.Evidence.RefusedTextKept ? 1 : 0) + " marked=" + std::to_string(probe.Evidence.RefusedMarkedTextOnly ? 1 : 0));
			}
			probe.Phase = state_type::phase::awaiting_reply_open;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_open: {
			// A reply written in place in renderer.cpp's first thread, beside
			// the code in Review, where the drawer writes replies in place.
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			note("page=" + std::to_string(inlinePage != nullptr && inlinePage->isVisibleTo(window) ? 1 : 0) + " modality=" + std::to_string(static_cast<int>(window->current_modality())));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				auto* const back = inlinePage->findChild<QPushButton*>(QStringLiteral("composeInlineDraftPageBack"));
				if (back == nullptr) {
					return fail("no-back");
				}
				back->click();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return result::pending;
			}
			cards->open_thread("review-thread:PRRT_cc_1");
			auto* const card  = cards->open_card();
			auto* const field = card != nullptr ? card->findChild<QAbstractButton*>(QStringLiteral("threadReplyField")) : nullptr;
			if (field == nullptr || !field->isEnabled()) {
				return fail("no-reply-field");
			}
			field->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_reply_draft;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("reply=" + (draft && draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) + " editor=" + std::to_string(cards->reply_editor() != nullptr ? 1 : 0));
			if (!draft || draft->ReplyToThread != std::optional<std::string>{ "PRRT_cc_1" } || cards->reply_editor() == nullptr) {
				return result::pending;
			}
			cards->reply_editor()->setPlainText(replyBody);
			probe.Phase = state_type::phase::awaiting_reply_body;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_body: {
			const auto& draft   = implementation->ReviewDraft;
			auto* const card    = cards->open_card();
			auto* const mark    = card != nullptr ? dynamic_cast<qt::suggestion_mark*>(card->findChild<QWidget*>(QStringLiteral("threadReplySuggestionMark"))) : nullptr;
			const auto  inPlace = mark == nullptr ? std::string{ "-" } : !mark->isVisibleTo(card) ? std::string{ "hidden" }
																								  : mark->property("suggestionApplicability").toString().toStdString();
			note("body=" + (draft ? draft->Body : std::string{ "-" }) + " inPlace=" + inPlace + " editorMark=" + markState());
			if (!draft || draft->Body != replyBody.toStdString()) {
				return result::pending;
			}
			// The draft editor reads the thread's projected facts through the
			// window, the reply editor from its own card: both say native.
			const auto* const editorMark     = editor->suggestion_mark_widget();
			probe.Evidence.ReplyMarkedNative = inPlace == "native" && editorMark->decision().Applicability == core::suggestion_applicability::native
				&& cards->reply_editor() != nullptr && cards->reply_editor()->toPlainText() == replyBody;
			render("reply-in-place-native");
			const auto& evidence = probe.Evidence;
			const auto  all      = evidence.ReviewEmptyDirected && evidence.FileShown && evidence.ThreadMarksDrawn && evidence.CommentEditable && evidence.CodeStaysLiteral
				&& evidence.ChipApplied && evidence.ShortcutApplied && evidence.Published && evidence.PublishedMarked && evidence.PatchMarkedNative
				&& evidence.PatchPublishedExact && evidence.ProseUnmarkedExact && evidence.RefusedTextKept && evidence.RefusedMarkedTextOnly && evidence.ReplyMarkedNative;
			if (!all) {
				return fail("reply-mark inPlace=" + inPlace + " editorMark=" + markState());
			}
			probe.Phase = state_type::phase::awaiting_defect_offer;
			return result::pending;
		}
		// [[BPR-1086]]. Private tracking beside public labels (RFC-BPR-0048 R16):
		// the issue root offers Classify as defect, only that gesture tracks
		// it, and the website's label edit and resolution reach the defect as
		// evidence at most. Then a tour stop with a labelled narrative (R19).
		case state_type::phase::awaiting_defect_offer: {
			auto* const ledger = implementation->ReviewLedgerHost.get();
			auto        menu   = std::unique_ptr<QMenu>{ cards->thread_menu("review-thread:PRRT_cc_1") };
			auto* const offer  = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("threadClassifyDefect")) : nullptr;
			const auto  issues = std::ranges::count_if(cards->cards(), [&](const qt::thread_card& aCard) { return rootLabel(&aCard) == core::feedback_label::issue; });
			note("ledger=" + std::to_string(ledger != nullptr && ledger->is_quiescent() ? 1 : 0) + " defects=" + std::to_string(implementation->DurableDefects.size())
				 + " issues=" + std::to_string(issues) + " offer=" + std::to_string(offer == nullptr ? -1 : offer->isEnabled() ? 1
																															   : 0));
			if (ledger == nullptr || !ledger->is_quiescent() || offer == nullptr || !offer->isEnabled()) {
				return result::pending;
			}
			// Several issue roots were read, written and published above; none
			// became a defect.
			probe.Evidence.IssueOffersTracking = implementation->DurableDefects.empty() && issues >= 2 && rootLabel(cardFor("review-thread:PRRT_cc_1")) == core::feedback_label::issue;
			if (!probe.Evidence.IssueOffersTracking) {
				return fail("issue-read-made-a-defect defects=" + std::to_string(implementation->DurableDefects.size()));
			}
			const auto mutations    = implementation->Fixture->review_mutation_evidence();
			probe.SubmissionsBefore = mutations.Submissions;
			probe.NativeBefore      = mutations.NativePosts;
			offer->trigger();
			probe.Phase = state_type::phase::awaiting_defect_tracked;
			return result::pending;
		}
		case state_type::phase::awaiting_defect_tracked: {
			const auto identity = defect_identity("review-thread:PRRT_cc_1");
			const auto found    = std::ranges::find_if(implementation->DurableDefects, [identity](const application::durable_defect_record& aDefect) {
				return aDefect.Conversation.value() == identity.Conversation && aDefect.History.Defect.Id.value() == identity.Defect;
			});
			const auto quiet    = implementation->ReviewLedgerHost->is_quiescent();
			note("defects=" + std::to_string(implementation->DurableDefects.size()) + " tracked="
				 + (found != implementation->DurableDefects.end() ? std::string{ core::name(found->History.Defect.State) } : std::string{ "-" }) + " quiet=" + std::to_string(quiet ? 1 : 0));
			if (found == implementation->DurableDefects.end() || !quiet || !window->github_refresh_action()->isEnabled()) {
				return result::pending;
			}
			const auto mutations                = implementation->Fixture->review_mutation_evidence();
			probe.Tracked                       = found->History.Defect;
			probe.Evidence.TrackedOnlyByGesture = implementation->DurableDefects.size() == 1U && core::outstanding(probe.Tracked->State)
				&& mutations.Submissions == probe.SubmissionsBefore && mutations.NativePosts == probe.NativeBefore;
			if (!probe.Evidence.TrackedOnlyByGesture) {
				return fail("tracking state=" + std::string{ core::name(probe.Tracked->State) });
			}
			// The website edits only the root's label away, without a commit.
			const auto* const root    = cardFor("review-thread:PRRT_cc_1");
			const auto        written = root != nullptr ? root->Posts.front().Body.toStdString() : std::string{ };
			const auto        read    = core::recognize_feedback_prefix(written);
			if (!read.Subject) {
				return fail("issue-root-unreadable");
			}
			probe.EditedRoot = written.substr(read.Subject->Begin);
			stage_server_root(aApplication, "PRRT_cc_1", probe.EditedRoot, std::nullopt);
			window->github_refresh_action()->trigger();
			probe.Phase = state_type::phase::awaiting_relabel_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_relabel_refreshed: {
			const auto* const root    = cardFor("review-thread:PRRT_cc_1");
			const auto        ready   = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			const auto        edited  = root != nullptr && root->Posts.front().Body.toStdString() == probe.EditedRoot;
			const auto        quiet   = implementation->ReviewLedgerHost->is_quiescent();
			const auto        tracked = std::ranges::find(implementation->DurableDefects, probe.Tracked->Id, [](const application::durable_defect_record& aDefect) { return aDefect.History.Defect.Id; });
			note("ready=" + std::to_string(ready ? 1 : 0) + " edited=" + std::to_string(edited ? 1 : 0) + " quiet=" + std::to_string(quiet ? 1 : 0)
				 + " tracked=" + (tracked != implementation->DurableDefects.end() ? std::string{ core::name(tracked->History.Defect.State) } : std::string{ "-" }));
			if (!ready || !edited || !quiet) {
				return result::pending;
			}
			if (tracked == implementation->DurableDefects.end()) {
				return fail("relabel-removed-the-defect");
			}
			const auto& after                = tracked->History.Defect;
			probe.Evidence.RelabelKeptDefect = !rootLabel(root).has_value() && after == *probe.Tracked && implementation->DurableDefects.size() == 1U;
			if (!probe.Evidence.RelabelKeptDefect) {
				return fail("relabel-moved-the-defect state=" + std::string{ core::name(after.State) });
			}
			stage_server_root(aApplication, "PRRT_cc_1", std::nullopt, true);
			window->github_refresh_action()->trigger();
			probe.Phase = state_type::phase::awaiting_resolve_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_resolve_refreshed: {
			const auto* const thread   = implementation->accepted_thread("review-thread:PRRT_cc_1");
			const auto        ready    = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			const auto        resolved = thread != nullptr && thread->Resolved;
			const auto        quiet    = implementation->ReviewLedgerHost->is_quiescent();
			const auto        tracked  = std::ranges::find(implementation->DurableDefects, probe.Tracked->Id, [](const application::durable_defect_record& aDefect) { return aDefect.History.Defect.Id; });
			note("ready=" + std::to_string(ready ? 1 : 0) + " resolved=" + std::to_string(resolved ? 1 : 0) + " quiet=" + std::to_string(quiet ? 1 : 0)
				 + " tracked=" + (tracked != implementation->DurableDefects.end() ? std::string{ core::name(tracked->History.Defect.State) } : std::string{ "-" }));
			if (!ready || !resolved || !quiet) {
				return result::pending;
			}
			if (tracked == implementation->DurableDefects.end()) {
				return fail("resolution-removed-the-defect");
			}
			const auto& after                  = tracked->History.Defect;
			auto        menu                   = std::unique_ptr<QMenu>{ cards->thread_menu("review-thread:PRRT_cc_1") };
			auto* const verify                 = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("defectVerify")) : nullptr;
			const auto  mutations              = implementation->Fixture->review_mutation_evidence();
			probe.Evidence.ResolveOnlyEvidence = core::outstanding(after.State) && after.State != core::defect_state::verified && after.State != core::defect_state::withdrawn
				&& after.Severity == probe.Tracked->Severity && after.Category == probe.Tracked->Category && verify != nullptr && verify->isEnabled()
				&& mutations.Submissions == probe.SubmissionsBefore && mutations.NativePosts == probe.NativeBefore;
			if (!probe.Evidence.ResolveOnlyEvidence) {
				return fail("resolution-settled-the-defect state=" + std::string{ core::name(after.State) } + " verify=" + std::to_string(verify != nullptr && verify->isEnabled() ? 1 : 0));
			}
			// The website shows a tour stop on renderer.cpp's other thread,
			// its narrative deliberately labelled.
			const std::array navigation{ core::tour_markdown_link{ "Tour overview", "https://github.com/octo-org/nested-folders/pull/7#pullrequestreview-9" } };
			const auto       stop = core::render_tour_stop("Why the width is clamped", 2, "question: Should the clamp live in the caller?\n\nThe renderer trusts it today.", navigation);
			if (!stop) {
				return fail("tour-stop-unrendered");
			}
			probe.TourStop = *stop;
			stage_server_root(aApplication, "PRRT_cc_2", probe.TourStop, std::nullopt);
			window->github_refresh_action()->trigger();
			probe.Phase = state_type::phase::awaiting_tour_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_tour_refreshed: {
			const auto* const stop  = cardFor("review-thread:PRRT_cc_2");
			const auto        ready = window->github_status_text() == "Pull request ready." && window->github_refresh_action()->isEnabled();
			const auto        shown = stop != nullptr && stop->Posts.front().Body.toStdString() == probe.TourStop;
			note("ready=" + std::to_string(ready ? 1 : 0) + " shown=" + std::to_string(shown ? 1 : 0));
			if (!ready || !shown) {
				return result::pending;
			}
			// [[BPR-1086]] owner look. Its note reads from the narrative, so the
			// label, set as the header, sits beside the mark.
			const auto& post                = stop->Posts.front();
			probe.Evidence.TourStopLabelled = rootLabel(stop) == core::feedback_label::question && post.Lead > 0 && post.Body.mid(post.Lead).startsWith(QStringLiteral("question: Should the clamp"))
				&& post.HeaderLength == QStringLiteral("question: ").size() && !cards->mark_rect(*stop).isEmpty();
			render("tour-stop-labelled");
			if (!probe.Evidence.TourStopLabelled) {
				return fail("tour-stop-unmarked name=" + stop->Posts.front().Name.toStdString());
			}
			probe.Phase = state_type::phase::awaiting_left_refused;
			return result::pending;
		}
		// [[BPR-1212]]. The keyboard line cursor and Return in the code, by
		// keys delivered to the focused pane as a keyboard delivers them. The
		// draft opens in the drawer (slice 2's begin_new_comment), the one
		// path every new comment takes.
		case state_type::phase::awaiting_left_refused: {
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				window->close_review_composition();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return result::pending;
			}
			auto* const surface = window->comparison_surface();
			auto* const left    = surface->left_pane();
			if (!surface->select_conversation_line(core::diff_side::left, 0U)) {
				return result::pending;
			}
			window->statusBar()->clearMessage();
			left->setFocus(Qt::OtherFocusReason);
			const auto natives = implementation->Fixture->review_mutation_evidence().NativePosts;
			paneKey(*left, Qt::Key_Return, Qt::NoModifier);
			const auto message = window->statusBar()->currentMessage();
			note("message=" + message.toStdString());
			// [[BPR-1212]] v002. Line 1 of the base is outside every hunk, or
			// its numbering is not yet known to be GitHub's: refused either way.
			probe.Evidence.LeftLineRefused = (message.contains(QStringLiteral("inside a changed region")) || message.contains(QStringLiteral("not known to match"))) && !cards->writing_draft()
				&& (inlinePage == nullptr || !inlinePage->isVisibleTo(window)) && implementation->Fixture->review_mutation_evidence().NativePosts == natives;
			if (!probe.Evidence.LeftLineRefused) {
				return fail("left-line-not-refused");
			}
			probe.Phase = state_type::phase::awaiting_single_line;
			return result::pending;
		}
		case state_type::phase::awaiting_single_line: {
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			auto* const surface    = window->comparison_surface();
			if (!probe.KeySent) {
				if (!probe.Line) {
					for (auto candidate = std::uint32_t{ 0U }; candidate < 64U && !probe.Line; ++candidate) {
						if (surface->select_conversation_line(core::diff_side::right, candidate) && window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
							probe.Line = candidate;
						}
					}
					if (!probe.Line) {
						return fail("no-free-line");
					}
				}
				if (const auto selected = surface->selection(); !selected || selected->PreferredLine.Side != core::diff_side::right || selected->PreferredLine.Line != *probe.Line) {
					static_cast<void>(surface->select_conversation_line(core::diff_side::right, *probe.Line));
				}
				const auto& draft  = implementation->ReviewDraft;
				const auto* active = implementation->TabHost && implementation->TabHost->state().Active
					? core::find_tab(implementation->TabHost->state(), *implementation->TabHost->state().Active)
					: nullptr;
				note("line=" + std::to_string(*probe.Line) + " draft=" + (draft ? std::to_string(draft->Anchor.Line) : std::string{ "-" })
					 + " selected_file=" + std::to_string(implementation->WorkbenchHost->selected().has_value())
					 + " active_tab=" + std::to_string(active != nullptr)
					 + " tab_file=" + std::to_string(active && active->View.File.has_value())
					 + " chosen=" + std::to_string(implementation->TabHost && implementation->TabHost->state().Chosen.has_value())
					 + " material=" + std::to_string(implementation->ReviewSourceMaterial.has_value())
					 + " tab_queued=" + std::to_string(implementation->TabAdvanceQueued));
				if (!draft || draft->Anchor.Line != *probe.Line || draft->Anchor.Side != core::diff_side::right || draft->State != core::review_draft_state::drafting) {
					return result::pending;
				}
				auto* const right = surface->right_pane();
				right->setFocus(Qt::OtherFocusReason);
				paneKey(*right, Qt::Key_Return, Qt::NoModifier);
				probe.KeySent = true;
			}
			const auto expected = QStringLiteral("Your draft on line %1").arg(*probe.Line + 1U);
			note("writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" }));
			if (!cards->writing_draft() || cards->open_card() == nullptr) {
				return result::pending;
			}
			probe.Evidence.SingleLineOpenedPlain = cards->draft_editor() != nullptr && !cards->draft_editor()->isReadOnly() && !body->isReadOnly()
				&& (inlinePage == nullptr || !inlinePage->isVisibleTo(window)) && implementation->ReviewDraft && !implementation->ReviewDraft->StartLine
				&& implementation->ReviewDraft->Anchor.Line == *probe.Line && cards->open_card()->accessibleName() == expected;
			render("drawer-one-line");
			if (!probe.Evidence.SingleLineOpenedPlain) {
				return fail("return-did-not-open-one-line");
			}
			probe.Line.reset();
			probe.KeySent = false;
			// Esc with nothing written drops the draft and returns to the code.
			paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
			probe.Phase = state_type::phase::awaiting_keyboard_range;
			return result::pending;
		}
		case state_type::phase::awaiting_keyboard_range: {
			auto* const surface  = window->comparison_surface();
			const auto& compared = surface->current_result();
			if (!compared || !compared->RightDocument) {
				return fail("no-comparison");
			}
			if (cards->writing_draft()) {
				return result::pending;
			}
			// Three right lines in one hunk with no thread on any of them,
			// from the last hunk up, away from the lines used above.
			if (!probe.RangeStart) {
				const auto hunks = core::github_review_hunks(compared->Alignment, core::diff_side::right);
				for (auto hunk = hunks.rbegin(); hunk != hunks.rend() && !probe.RangeStart; ++hunk) {
					for (auto start = hunk->End >= 3U ? hunk->End - 3U : 0U; start >= hunk->Begin && start + 2U < hunk->End && !probe.RangeStart; --start) {
						auto unthreaded = true;
						for (auto line = start; line < start + 3U && unthreaded; ++line) {
							unthreaded = surface->select_conversation_line(core::diff_side::right, line) && window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0;
						}
						if (unthreaded) {
							probe.RangeStart = start;
						}
						if (start == 0U) {
							break;
						}
					}
				}
				if (!probe.RangeStart) {
					return fail("no-free-range");
				}
			}
			const auto start = *probe.RangeStart;
			if (!surface->select_conversation_line(core::diff_side::right, start)) {
				return result::pending;
			}
			auto* const right = surface->right_pane();
			right->setFocus(Qt::OtherFocusReason);
			paneKey(*right, Qt::Key_Down, Qt::ShiftModifier);
			paneKey(*right, Qt::Key_Down, Qt::ShiftModifier);
			const auto range = surface->line_draft_range();
			note("start=" + std::to_string(start) + " range=" + (range ? std::to_string(range->first.Line) + "-" + std::to_string(range->second.Line) : std::string{ "-" }));
			probe.Evidence.KeyboardRangeSelected = range && range->first.Line == start && range->second.Line == start + 2U && range->first.Side == core::diff_side::right
				&& range->second.Side == core::diff_side::right && surface->selection() && surface->selection()->PreferredLine.Line == start + 2U
				&& core::decide_comment_range(compared->Alignment, range->first, range->second).StartLine == start;
			if (!probe.Evidence.KeyboardRangeSelected) {
				return fail("keyboard-range-not-selected");
			}
			auto lines = core::suggestion_source_lines(*compared->RightDocument, start, start + 2U);
			if (!lines) {
				return fail("no-prefill");
			}
			// [[RFC-BPR-0090]]. The change typed at the end of the last line,
			// as the core composes a suggestion with no words.
			lines->back() += " // probe";
			probe.Prefill      = core::compose_suggestion_body("", *lines);
			probe.RangeTyped   = false;
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			probe.Anchor       = core::source_line_ref{ compared->Alignment.Digest, core::diff_side::right, start + 2U };
			paneKey(*right, Qt::Key_Return, Qt::ShiftModifier);
			probe.Phase = state_type::phase::awaiting_range_prefilled;
			return result::pending;
		}
		case state_type::phase::awaiting_range_prefilled: {
			const auto& draft  = implementation->ReviewDraft;
			const auto  start  = probe.RangeStart.value_or(0U);
			auto* const writer = cards->writing_draft() ? cards->draft_editor() : nullptr;
			auto* const head   = window->comparison_surface()->head_editor();
			note("draft=" + (draft ? std::to_string(draft->Anchor.Line) + ":" + (draft->StartLine ? std::to_string(*draft->StartLine) : std::string{ "-" }) : std::string{ "-" })
				 + " writing=" + std::to_string(writer != nullptr ? 1 : 0) + " pane=" + std::to_string(head != nullptr ? 1 : 0) + " typed=" + std::to_string(probe.RangeTyped ? 1 : 0)
				 + " status=" + window->statusBar()->currentMessage().toStdString());
			// [[RFC-BPR-0090]]. Shift+Return opens the three lines for writing
			// in the head pane, with the caret at the end of the last, and the
			// drawer holds the words, none yet. The change is typed there.
			if (!probe.RangeTyped) {
				if (!draft || !probe.Anchor || draft->Anchor != *probe.Anchor || draft->StartLine != std::optional{ start } || writer == nullptr || head == nullptr
					|| head->model().editable_range() != std::optional{ std::pair<std::uint64_t, std::uint64_t>{ start, start + 2U } }) {
					return result::pending;
				}
				if (!writer->toPlainText().isEmpty() || !head->apply(core::insert_text{ .Text = " // probe" })) {
					return fail("suggestion-not-in-the-pane words=" + writer->toPlainText().toStdString());
				}
				probe.RangeTyped = true;
				return result::pending;
			}
			if (!draft || draft->Anchor != *probe.Anchor || draft->Body != probe.Prefill || writer == nullptr || head == nullptr || !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			// The drawer names the lines, the gutter's blue bracket spans them,
			// the drawer holds no block, and the body is the core's composition.
			const auto& marks                  = window->comparison_surface()->line_marks(core::diff_side::right);
			const auto  bracket                = std::ranges::any_of(marks, [start](const qt::diff_line_mark& aMark) {
				return aMark.Writing && aMark.Line == start + 2U && aMark.FirstLine == std::optional<std::uint32_t>{ start };
			});
			probe.Evidence.SuggestionPrefilled = draft->StartLine == start && !draft->ReplyToThread && cards->open_card() != nullptr
				&& cards->open_card()->accessibleName() == QStringLiteral("Your draft on lines %1–%2").arg(start + 1U).arg(start + 3U) && bracket && writer->toPlainText().isEmpty()
				&& markState() == "native";
			render("drawer-range-suggestion");
			if (!probe.Evidence.SuggestionPrefilled) {
				return fail("suggestion-not-in-the-pane words=" + writer->toPlainText().toStdString() + " mark=" + markState() + " bracket=" + std::to_string(bracket ? 1 : 0)
							+ " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" }));
			}
			// The drawer draft's own Publish.
			auto* const publish = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
			if (publish == nullptr || !publish->isEnabled()) {
				return fail("no-drawer-publish");
			}
			publish->click();
			probe.Phase = state_type::phase::awaiting_range_published;
			return result::pending;
		}
		case state_type::phase::awaiting_range_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto prefill   = QString::fromStdString(probe.Prefill);
			const auto found     = std::ranges::find_if(cards->cards(), [&](const qt::thread_card& aCard) { return !aCard.Posts.empty() && aCard.Posts.front().Body == prefill; });
			const auto start     = probe.RangeStart.value_or(0U);
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts) + " line="
				 + std::to_string(mutations.LastLine.value_or(0U)) + " start=" + std::to_string(mutations.LastStartLine.value_or(0U)) + " startSide=" + mutations.LastStartSide.value_or("-")
				 + " card=" + std::to_string(found != cards->cards().end() ? 1 : 0) + " status=" + editor->mutation_status_label()->text().toStdString());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || found == cards->cards().end()) {
				return result::pending;
			}
			probe.Evidence.RangePublishedExact = mutations.NativePosts == probe.NativeBefore + 1U && !mutations.LastFile && mutations.LastBody == probe.Prefill && mutations.LastPath == path
				&& mutations.LastSide == "RIGHT" && mutations.LastLine == start + 3U && mutations.LastStartLine == start + 1U && mutations.LastStartSide == std::optional<std::string>{ "RIGHT" };
			if (!probe.Evidence.RangePublishedExact) {
				return fail("range-not-published-exact");
			}
			probe.Phase = state_type::phase::awaiting_mouse_range;
			return result::pending;
		}
		// [[BPR-1721]]. The owner's course: several right lines selected
		// by a drag, a right-click inside the selection, and Add comment on
		// the menu the window opens, each delivered to the window as the
		// platform delivers it, with the event loop turning between them.
		// The draft takes all the lines, and its Publish sends them.
		case state_type::phase::awaiting_mouse_range: {
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			// The range's publication refreshes the conversations first, and the
			// kept tab revalidates; a line chosen before that lands is replaced
			// by the tab's remembered one ([[BPR-1713]]).
			if (!window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value() || implementation->TabAdvanceQueued || implementation->TabLoad.has_value()
				|| implementation->RefreshKeptTabView) {
				return result::pending;
			}
			auto* const surface  = window->comparison_surface();
			const auto& compared = surface->current_result();
			if (!compared || !compared->RightDocument) {
				return fail("no-comparison");
			}
			// Three right lines in one hunk with no thread on any, the last
			// with text for the drag to end in.
			if (!probe.MouseRangeStart) {
				const auto hunks = core::github_review_hunks(compared->Alignment, core::diff_side::right);
				for (const auto& hunk : hunks) {
					for (auto start = hunk.Begin; start + 2U < hunk.End && !probe.MouseRangeStart; ++start) {
						auto unthreaded = !surface->right_pane()->document()->findBlockByNumber(static_cast<int>(start + 2U)).text().isEmpty();
						for (auto line = start; line < start + 3U && unthreaded; ++line) {
							unthreaded = surface->select_conversation_line(core::diff_side::right, line) && window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0;
						}
						if (unthreaded) {
							probe.MouseRangeStart = start;
						}
					}
				}
				if (!probe.MouseRangeStart) {
					return fail("no-free-mouse-range");
				}
				// Somewhere else first, so the drag is what selects the lines.
				static_cast<void>(surface->select_conversation_line(core::diff_side::right, *probe.MouseRangeStart + 3U));
				return result::pending;
			}
			const auto  start = *probe.MouseRangeStart;
			auto* const right = surface->right_pane();
			// A point in the window on a line's code, or at its end.
			const auto point = [window, surface, right](const std::uint32_t aLine, const bool aEnd) {
				const auto row    = surface->line_rect(core::diff_side::right, aLine);
				const auto inPane = right->viewport()->mapFrom(surface, row.center());
				return right->viewport()->mapTo(window, QPoint{ aEnd ? right->viewport()->width() - 4 : 40, inPane.y() });
			};
			const auto mouse = [window](const QPoint aAt, const QEvent::Type aType, const Qt::MouseButton aButton, const Qt::MouseButtons aHeld) {
				QCursor::setPos(window->mapToGlobal(aAt));
				QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
					window->windowHandle(), QPointF{ aAt }, QPointF{ window->mapToGlobal(aAt) }, aHeld, aButton, aType, Qt::NoModifier);
				QCoreApplication::processEvents();
			};
			if (!probe.MouseDragged) {
				const auto from = point(start, false);
				const auto to   = point(start + 2U, true);
				mouse(from, QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
				mouse(from, QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
				mouse((from + to) / 2, QEvent::MouseMove, Qt::NoButton, Qt::LeftButton);
				mouse(to, QEvent::MouseMove, Qt::NoButton, Qt::LeftButton);
				mouse(to, QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
				probe.MouseDragged = true;
				return result::pending;
			}
			const auto range = surface->line_draft_range();
			note("start=" + std::to_string(start) + " range=" + (range ? std::to_string(range->first.Line) + "-" + std::to_string(range->second.Line) : std::string{ "-" })
				 + " selected=" + (surface->selection() ? std::to_string(surface->selection()->PreferredLine.Line) : std::string{ "-" }));
			if (!range || range->first.Line != start || range->second.Line != start + 2U) {
				return fail("drag-did-not-select-the-lines");
			}
			// The right-click: the window opens its menu as a popup.
			const auto inside = point(start + 1U, false);
			mouse(inside, QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
			mouse(inside, QEvent::MouseButtonPress, Qt::RightButton, Qt::RightButton);
			auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (menu == nullptr || menu->objectName() != QStringLiteral("codeContextMenu") || menu->windowHandle() == nullptr) {
				return fail("right-click-opened-no-code-menu");
			}
			const auto corner = QPointF{ 1.0, 1.0 };
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
				menu->windowHandle(), corner, QPointF{ menu->mapToGlobal(corner) }, Qt::NoButton, Qt::RightButton, QEvent::MouseButtonRelease, Qt::NoModifier);
			QCoreApplication::processEvents();
			const auto actions = menu->actions();
			const auto found   = std::ranges::find_if(actions, [](const QAction* aAction) { return aAction->objectName() == QStringLiteral("addCommentAction"); });
			const auto label   = QStringLiteral("Add comment on lines %1–%2").arg(start + 1U).arg(start + 3U);
			note("start=" + std::to_string(start) + " item=" + (found != actions.end() ? (*found)->text().toStdString() : std::string{ "-" })
				 + " kept=" + std::to_string(right->textCursor().hasSelection() ? 1 : 0));
			if (found == actions.end() || !(*found)->isEnabled() || (*found)->text() != label) {
				return fail("add-comment-not-on-the-lines");
			}
			// The item, clicked on the popup as a hand does.
			const auto centre = QPointF{ menu->actionGeometry(*found).center() };
			const auto global = QPointF{ menu->mapToGlobal(centre) };
			QCursor::setPos(global.toPoint());
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(menu->windowHandle(), centre, global, Qt::NoButton, Qt::NoButton, QEvent::MouseMove, Qt::NoModifier);
			QCoreApplication::processEvents();
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(menu->windowHandle(), centre, global, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress, Qt::NoModifier);
			QCoreApplication::processEvents();
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(menu->windowHandle(), centre, global, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease, Qt::NoModifier);
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_mouse_range_opened;
			return result::pending;
		}
		case state_type::phase::awaiting_mouse_range_opened: {
			const auto  start  = probe.MouseRangeStart.value_or(0U);
			const auto& draft  = implementation->ReviewDraft;
			auto* const writer = cards->writing_draft() ? cards->draft_editor() : nullptr;
			const auto  words  = QStringLiteral("question: Do these three lines need the same guard?");
			note("draft=" + (draft ? std::to_string(draft->Anchor.Line) + ":" + (draft->StartLine ? std::to_string(*draft->StartLine) : std::string{ "-" }) : std::string{ "-" })
				 + " writing=" + std::to_string(writer != nullptr ? 1 : 0) + " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" })
				 + " status=" + window->statusBar()->currentMessage().toStdString());
			if (writer == nullptr || !draft || draft->Anchor.Line != start + 2U) {
				return ++probe.Waited < 2'000U ? result::pending : fail("mouse-range-not-opened");
			}
			if (writer->toPlainText() != words) {
				writer->setPlainText(words);
				return result::pending;
			}
			if (draft->Body != words.toStdString() || !editor->publish_action()->isEnabled()) {
				return result::pending;
			}
			probe.Waited                    = 0U;
			probe.Evidence.MouseRangeOpened = draft->StartLine == std::optional<std::uint32_t>{ start } && draft->Anchor.Side == core::diff_side::right && !draft->ReplyToThread
				&& cards->open_card() != nullptr && cards->open_card()->accessibleName() == QStringLiteral("Your draft on lines %1–%2").arg(start + 1U).arg(start + 3U);
			render("drawer-mouse-range");
			if (!probe.Evidence.MouseRangeOpened) {
				return fail("mouse-range-draft-not-on-the-lines");
			}
			auto* const publish = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
			if (publish == nullptr || !publish->isEnabled()) {
				return fail("no-drawer-publish");
			}
			probe.Prefill      = words.toStdString();
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			publish->click();
			probe.Phase = state_type::phase::awaiting_mouse_range_published;
			return result::pending;
		}
		case state_type::phase::awaiting_mouse_range_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto start     = probe.MouseRangeStart.value_or(0U);
			note("native=" + std::to_string(mutations.NativePosts) + " line=" + std::to_string(mutations.LastLine.value_or(0U)) + " start=" + std::to_string(mutations.LastStartLine.value_or(0U)));
			// The thread it makes, once the conversations are read again, is on
			// all three lines, as GitHub has it.
			const auto words  = QString::fromStdString(probe.Prefill);
			const auto thread = std::ranges::find_if(cards->cards(), [&words](const qt::thread_card& aCard) { return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == words; });
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || thread == cards->cards().end()) {
				return result::pending;
			}
			probe.Evidence.MouseRangePublishedExact = mutations.NativePosts == probe.NativeBefore + 1U && !mutations.LastFile && mutations.LastBody == probe.Prefill && mutations.LastPath == path
				&& mutations.LastSide == "RIGHT" && mutations.LastLine == start + 3U && mutations.LastStartLine == start + 1U && mutations.LastStartSide == std::optional<std::string>{ "RIGHT" }
				&& thread->Line == std::optional<std::uint32_t>{ start + 3U } && qt::thread_first_line(*thread) == std::optional<std::uint32_t>{ start + 1U };
			if (!probe.Evidence.MouseRangePublishedExact) {
				return fail("mouse-range-not-published-exact");
			}
			probe.Phase = state_type::phase::awaiting_base_numbering;
			return result::pending;
		}
		// [[BPR-1212]] v002. The epoch host has told whether this file's base is
		// the merge base's version GitHub numbers `LEFT` lines in; the
		// fixture's base is its branch point, so it is.
		case state_type::phase::awaiting_base_numbering: {
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			// The range's publication refreshes the conversations; the next
			// publication waits for that read to finish, as a reviewer's
			// would, so the fixture's recorded reads stay in order.
			// [[BPR-1570]]. Its owed reread of the pull request too, which
			// starts only once a pending read settles.
			if (!window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value()) {
				return result::pending;
			}
			note("left-current=" + std::to_string(window->left_conversation_endpoint_current() ? 1 : 0)
				 + " numbered=" + std::to_string(implementation->ReviewSourceMaterial && implementation->ReviewSourceMaterial->BaseNumberedAsGitHub ? 1 : 0));
			probe.Evidence.BaseNumberedKnown = window->left_conversation_endpoint_current() && implementation->ReviewSourceMaterial
				&& implementation->ReviewSourceMaterial->BaseNumberedAsGitHub;
			if (!probe.Evidence.BaseNumberedKnown) {
				return result::pending;
			}
			probe.RefreshedFrom.reset();
			probe.Phase = state_type::phase::awaiting_kept_refresh;
			return result::pending;
		}
		// [[BPR-1570]]. A refresh that keeps the file adopts it under a new
		// workbench authority without publishing it again, as the range's
		// publication refresh does at a time of its own. Forced here before the
		// base line is chosen, so the course always meets that order: the
		// numbering holds across it and the base line still takes its draft.
		case state_type::phase::awaiting_kept_refresh: {
			const auto& selected = implementation->SelectedDiffPublication;
			if (!selected) {
				return fail("no-selected-diff");
			}
			if (!probe.RefreshedFrom) {
				if (!window->github_refresh_action()->isEnabled()) {
					return result::pending;
				}
				probe.RefreshedFrom = selected->Request.WorkbenchAuthority;
				window->github_refresh_action()->trigger();
				return result::pending;
			}
			const auto adopted = selected->Request.WorkbenchAuthority != *probe.RefreshedFrom;
			note("adopted=" + std::to_string(adopted ? 1 : 0) + " numbered=" + std::to_string(implementation->base_numbered_as_github() ? 1 : 0)
				 + " left-current=" + std::to_string(window->left_conversation_endpoint_current() ? 1 : 0));
			if (!adopted || !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value()) {
				return result::pending;
			}
			probe.Evidence.BaseNumberingKept = implementation->base_numbered_as_github() && window->left_conversation_endpoint_current()
				&& implementation->ReviewSourceMaterial && implementation->ReviewSourceMaterial->BaseNumberedAsGitHub;
			if (!probe.Evidence.BaseNumberingKept) {
				return fail("base-numbering-lost-on-kept-refresh");
			}
			// `--stale-base-numbering`: the kept answer becomes one about
			// another path with the window's left side still placed, the state
			// such a refresh left before [[BPR-1570]]. The base line's own
			// question is then answered at once, and must still take a draft.
			if (std::exchange(implementation->StaleBaseNumberingQuestion, false) && implementation->BaseNumbering) {
				implementation->BaseNumbering->Request.LeftPath = std::filesystem::path{ "bpr-1570/another-path" };
			}
			probe.Line.reset();
			probe.KeySent = false;
			probe.Phase   = state_type::phase::awaiting_base_line;
			return result::pending;
		}
		case state_type::phase::awaiting_base_line: {
			auto* const surface  = window->comparison_surface();
			const auto& compared = surface->current_result();
			if (!compared || !compared->LeftDocument) {
				return fail("no-base-document");
			}
			if (!probe.KeySent) {
				// A base line in a hunk with no thread on it.
				if (!probe.Line) {
					for (const auto& hunk : core::github_review_hunks(compared->Alignment, core::diff_side::left)) {
						for (auto line = hunk.Begin; line < hunk.End && !probe.Line; ++line) {
							if (surface->select_conversation_line(core::diff_side::left, line) && model->rowCount() == 0) {
								probe.Line = line;
							}
						}
					}
					if (!probe.Line) {
						return fail("no-free-base-line");
					}
				}
				if (const auto selected = surface->selection(); !selected || selected->PreferredLine.Side != core::diff_side::left || selected->PreferredLine.Line != *probe.Line) {
					static_cast<void>(surface->select_conversation_line(core::diff_side::left, *probe.Line));
				}
				const auto& draft = implementation->ReviewDraft;
				note("base-line=" + std::to_string(*probe.Line) + " draft=" + (draft ? std::to_string(draft->Anchor.Line) + (draft->Anchor.Side == core::diff_side::left ? "L" : "R") : std::string{ "-" }));
				if (!draft || draft->Anchor.Line != *probe.Line || draft->Anchor.Side != core::diff_side::left || draft->State != core::review_draft_state::drafting) {
					return result::pending;
				}
				auto* const left = surface->left_pane();
				left->setFocus(Qt::OtherFocusReason);
				paneKey(*left, Qt::Key_Return, Qt::NoModifier);
				probe.KeySent = true;
			}
			const auto expected = QStringLiteral("Your draft on base line %1").arg(*probe.Line + 1U);
			note("writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " card=" + (cards->open_card() != nullptr ? cards->open_card()->accessibleName().toStdString() : std::string{ "-" }));
			if (!cards->writing_draft() || cards->open_card() == nullptr || cards->draft_editor() == nullptr) {
				return result::pending;
			}
			if (cards->draft_editor()->toPlainText().isEmpty()) {
				cards->draft_editor()->setPlainText(QStringLiteral("question: BPR-1212 base-side comment, why was this removed?"));
				return result::pending;
			}
			auto* const publish = cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
			const auto& draft   = implementation->ReviewDraft;
			if (!draft || draft->Body.empty() || publish == nullptr || !publish->isEnabled()) {
				return result::pending;
			}
			probe.Evidence.BaseLineOpened = cards->open_card()->accessibleName() == expected && draft->Anchor.Side == core::diff_side::left
				&& draft->Anchor.Line == *probe.Line && !draft->StartLine && !draft->ReplyToThread;
			render("drawer-base-line");
			if (!probe.Evidence.BaseLineOpened) {
				return fail("base-line-not-opened card=" + cards->open_card()->accessibleName().toStdString());
			}
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			publish->click();
			probe.Phase = state_type::phase::awaiting_base_published;
			return result::pending;
		}
		case state_type::phase::awaiting_base_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts) + " side="
				 + mutations.LastSide + " line=" + std::to_string(mutations.LastLine.value_or(0U)) + " status=" + editor->mutation_status_label()->text().toStdString());
			// [[BPR-1570]]. The publication's owed reread finishes before the
			// course does, or the run ends with it reading and the fixture's
			// recording unconsumed.
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore
				|| !window->github_refresh_action()->isEnabled() || window->github_status_text() != "Pull request ready."
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->RereadOwed || implementation->RereadQueued
				|| implementation->WindowRead->state().PendingPullRequest.has_value()) {
				return result::pending;
			}
			probe.Evidence.BasePublishedLeft = mutations.NativePosts == probe.NativeBefore + 1U && !mutations.LastFile && mutations.LastPath == path
				&& mutations.LastSide == "LEFT" && mutations.LastLine == probe.Line.value_or(0U) + 1U && !mutations.LastStartLine && !mutations.LastStartSide
				&& mutations.LastBody == "question: BPR-1212 base-side comment, why was this removed?";
			if (!probe.Evidence.BasePublishedLeft) {
				return fail("base-line-not-published-left");
			}
			probe.Line.reset();
			probe.Phase = state_type::phase::awaiting_no_line;
			return result::pending;
		}
		case state_type::phase::awaiting_no_line: {
			auto* const inlinePage = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			if (inlinePage != nullptr && inlinePage->isVisibleTo(window)) {
				window->close_review_composition();
				QCoreApplication::processEvents();
				return result::pending;
			}
			if (cards->writing_draft() && cards->draft_editor() != nullptr) {
				paneKey(*cards->draft_editor(), Qt::Key_Escape, Qt::NoModifier);
				return result::pending;
			}
			// The comparison emptied, as when no file is chosen: the pane
			// still takes Return, and nothing opens.
			auto* const surface = window->comparison_surface();
			surface->clear();
			QCoreApplication::processEvents();
			window->statusBar()->clearMessage();
			surface->right_pane()->setFocus(Qt::OtherFocusReason);
			paneKey(*surface->right_pane(), Qt::Key_Return, Qt::NoModifier);
			const auto message = window->statusBar()->currentMessage();
			note("message=" + message.toStdString());
			probe.Evidence.NoLineExplained = message.contains(QStringLiteral("Select a line first")) && message.contains(QStringLiteral("Up and Down")) && !cards->writing_draft()
				&& (inlinePage == nullptr || !inlinePage->isVisibleTo(window));
			if (!probe.Evidence.NoLineExplained) {
				return fail("no-line-not-explained");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed:
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::label_authoring_evidence(
		const github_application& aApplication) -> detail::label_authoring_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->LabelAuthoringProbe.has_value()) {
			return { };
		}
		return implementation->LabelAuthoringProbe->Evidence;
	}

	auto detail::github_application_fixture_access::label_authoring_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->LabelAuthoringProbe.has_value()) {
			return { };
		}
		return implementation->LabelAuthoringProbe->Diagnostic;
	}

	auto detail::github_application_fixture_access::exercise_published_file_switch(
		github_application& aApplication, const detail::published_file_switch_timing aTiming) -> detail::published_file_switch_probe_result
	{
		using result               = detail::published_file_switch_probe_result;
		using state_type           = published_file_switch_probe_state;
		using timing               = detail::published_file_switch_timing;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->PublishedFileSwitchProbe.has_value()) {
			implementation->PublishedFileSwitchProbe.emplace();
		}
		auto& probe = *implementation->PublishedFileSwitchProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr || implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (cards == nullptr || editor == nullptr || model == nullptr) {
			return result::pending;
		}
		auto* const body = editor->body_editor();
		const auto  fail = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		constexpr auto path    = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto other   = std::string_view{ "src/engine/renderer.h" };
		const auto     written = QStringLiteral("BPR-1326: this comment stays after a file switch.");
		const auto     ready   = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value();
		};
		const auto byBody = [&](const qt::thread_card& aCard) { return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == written; };
		// What shows of the comment on the open file: the thread in the
		// accepted pull request or the draft still saved; its note, measured,
		// placed at its line and in the shown drawer; its gutter marker and
		// its scroll-bar tick.
		struct shown_facts
		{
			bool Kept   = false;
			bool Note   = false;
			bool Drawn  = false;
			bool Marker = false;
			bool Tick   = false;

			[[nodiscard]] auto all() const noexcept -> bool { return Kept && Note && Drawn && Marker && Tick; }
			[[nodiscard]] auto text() const -> std::string
			{
				return "kept=" + std::to_string(Kept ? 1 : 0) + " note=" + std::to_string(Note ? 1 : 0) + " drawn=" + std::to_string(Drawn ? 1 : 0)
					+ " marker=" + std::to_string(Marker ? 1 : 0) + " tick=" + std::to_string(Tick ? 1 : 0);
			}
		};
		const auto shown = [&]() -> shown_facts {
			auto facts = shown_facts{ };
			if (!probe.Anchor) {
				return facts;
			}
			const auto line = probe.Anchor->Line;
			const auto card = aTiming == timing::draft
				? std::ranges::find_if(cards->cards(), [line](const qt::thread_card& aCard) { return aCard.Draft && aCard.Line == std::optional{ line + 1U }; })
				: std::ranges::find_if(cards->cards(), byBody);
			if (aTiming == timing::draft) {
				facts.Kept = implementation->SharedDrafts && std::ranges::any_of(implementation->SharedDrafts->recovery_snapshot(), [&](const core::revisioned_review_draft& aSaved) {
								 return aSaved.Draft.Anchor == *probe.Anchor && aSaved.Draft.Body == written.toStdString() && aSaved.Draft.State == core::review_draft_state::drafting;
							 });
			} else if (card != cards->cards().end()) {
				facts.Kept = implementation->WindowRead && implementation->WindowRead->accepted_snapshot()
					&& std::ranges::any_of(implementation->WindowRead->accepted_snapshot()->ReviewThreads, [&](const application::github_review_thread_record& aThread) {
								 return "review-thread:" + std::string{ aThread.Thread.value() } == card->Id;
							 });
			}
			facts.Note   = card != cards->cards().end() && card->Line == std::optional{ line + 1U };
			facts.Drawn  = facts.Note && card->Placed && card->Rect.height() > 0 && cards->isVisibleTo(window);
			facts.Marker = std::ranges::any_of(window->comparison_surface()->line_marks(core::diff_side::right), [line](const qt::diff_line_mark& aMark) { return aMark.Line == line; })
				&& std::ranges::any_of(cards->line_marks(core::diff_side::right), [line](const qt::thread_line_mark& aMark) { return aMark.Line == line; });
			facts.Tick = std::ranges::any_of(window->comparison_surface()->thread_ticks(), [line](const qt::diff_thread_tick& aTick) { return aTick.Side == core::diff_side::right && aTick.Line == line; });
			return facts;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Phase = state_type::phase::awaiting_line_draft;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_line_draft: {
			// The first current-head line no thread is on, chosen in Review.
			auto line = std::optional<std::uint32_t>{ };
			for (auto candidate = std::uint32_t{ 0U }; candidate < 32U && !line; ++candidate) {
				if (window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					line = candidate;
				}
			}
			const auto& draft = implementation->ReviewDraft;
			note("line=" + std::to_string(line.value_or(0U)) + " draft=" + std::to_string(draft.has_value() ? 1 : 0)
				 + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0));
			if (!line || !draft || draft->ReplyToThread || !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			window->comment_action()->trigger();
			QCoreApplication::processEvents();
			if (body->isReadOnly()) {
				return fail("comment-page-not-editable");
			}
			probe.Anchor       = draft->Anchor;
			probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
			body->setPlainText(written);
			probe.Phase = state_type::phase::awaiting_body;
			return result::pending;
		}
		case state_type::phase::awaiting_body: {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + (draft ? draft->Body : std::string{ "-" }) + " publish=" + std::to_string(editor->publish_action()->isEnabled() ? 1 : 0));
			if (!draft || draft->Body != written.toStdString() || !editor->publish_action()->isEnabled()
				|| implementation->PrivateDraftCommandPending || implementation->PrivateDraftSelectionPending || !ready()) {
				return result::pending;
			}
			if (!probe.Anchor || draft->Anchor != *probe.Anchor) {
				return fail("draft-moved");
			}
			if (aTiming == timing::draft) {
				// Not published: back from the comment's page to the code,
				// where the draft is my note at its line.
				if (auto* const page = window->findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage")); page != nullptr && page->isVisibleTo(window)) {
					auto* const back = page->findChild<QPushButton*>(QStringLiteral("composeInlineDraftPageBack"));
					if (back == nullptr) {
						return fail("no-back");
					}
					back->click();
					QCoreApplication::processEvents();
					return result::pending;
				}
				const auto facts = shown();
				note("draft=" + draft->Body + " " + facts.text());
				if (!facts.all()) {
					return result::pending;
				}
				probe.Evidence.Written     = implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore;
				probe.Evidence.ShownBefore = true;
				probe.Phase                = state_type::phase::awaiting_other_file;
				return result::pending;
			}
			// A slow server: the application's own refresh after Publish
			// answers only once the other file shows.
			if (aTiming == timing::late) {
				window->github_refresh_action()->blockSignals(true);
			}
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_published;
			return result::pending;
		}
		case state_type::phase::awaiting_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			const auto facts     = shown();
			note("pending=" + std::to_string(implementation->PendingReviewMutation.has_value() ? 1 : 0) + " native=" + std::to_string(mutations.NativePosts) + " " + facts.text());
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore) {
				return result::pending;
			}
			probe.Evidence.Written = mutations.NativePosts == probe.NativeBefore + 1U && mutations.LastPath == path
				&& probe.Anchor && mutations.LastLine == probe.Anchor->Line + 1U;
			if (!probe.Evidence.Written) {
				return fail("published-not-exact");
			}
			// The late course leaves as soon as Publish answers.
			if (aTiming == timing::settled) {
				if (!facts.all() || !ready()) {
					return result::pending;
				}
				probe.Evidence.ShownBefore = true;
			}
			probe.Phase = state_type::phase::awaiting_other_file;
			return result::pending;
		}
		case state_type::phase::awaiting_other_file: {
			const auto revealed = reveal_fixture_file(aApplication, other);
			note("reveal=" + std::to_string(static_cast<int>(revealed)) + " ready=" + std::to_string(ready() ? 1 : 0));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("other-file-not-revealed");
			}
			if (revealed != detail::fixture_file_reveal_result::shown) {
				return result::pending;
			}
			probe.Evidence.OtherFileShown = true;
			if (window->github_refresh_action()->signalsBlocked()) {
				window->github_refresh_action()->blockSignals(false);
				window->github_refresh_action()->trigger();
				return result::pending;
			}
			if (!ready()) {
				return result::pending;
			}
			probe.Phase = state_type::phase::awaiting_return;
			return result::pending;
		}
		case state_type::phase::awaiting_return: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed-again");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Waited = 0U;
				probe.Phase  = state_type::phase::awaiting_settled;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_settled: {
			// Settled: nothing is being read, and the comment has had forty
			// ticks (two seconds) to show.
			const auto facts     = shown();
			const auto selection = window->comparison_surface()->selection();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " waited=" + std::to_string(probe.Waited) + " " + facts.text()
				 + " selected=" + (selection ? std::to_string(selection->PreferredLine.Line) : std::string{ "-" }) + " notes=" + std::to_string(cards->cards().size()));
			if (!ready() || (!facts.all() && ++probe.Waited < 40U)) {
				return result::pending;
			}
			probe.Evidence.Kept             = facts.Kept;
			probe.Evidence.ShownAfterReturn = facts.all();
			if (probe.Evidence.ShownAfterReturn) {
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			// Gone: say whether a later refresh brings it back, then fail.
			window->github_refresh_action()->trigger();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_refreshed: {
			const auto facts = shown();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " waited=" + std::to_string(probe.Waited) + " " + facts.text());
			if ((!ready() || !facts.all()) && ++probe.Waited < 80U) {
				return result::pending;
			}
			return fail(facts.all() ? "gone-after-return refresh-restored=1" : "gone-after-return refresh-restored=0");
		}
		case state_type::phase::passed:
			return result::passed;
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::exercise_reply_draft(
		github_application& aApplication, const detail::reply_draft_course aCourse) -> detail::reply_draft_probe_result
	{
		using result     = detail::reply_draft_probe_result;
		using course     = detail::reply_draft_course;
		using state_type = reply_draft_probe_state;
		if (aCourse == course::outdated_publish || aCourse == course::outdated_write || aCourse == course::outdated_restart) {
			return exercise_outdated_reply(aApplication, aCourse);
		}
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReplyDraftProbe.has_value()) {
			implementation->ReplyDraftProbe.emplace();
		}
		auto& probe = *implementation->ReplyDraftProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor = window->review_editor();
		if (cards == nullptr || editor == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe](const std::string_view aWhy) {
			probe.PrivateEffectsHold.reset();
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		constexpr auto path     = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto threadId = std::string_view{ "PRRT_cc_1" };
		const auto     identity = "review-thread:" + std::string{ threadId };
		const auto     words    = QStringLiteral("question: Is this still needed once the clear runs first? (BPR-1374 unsent reply)");
		const auto     more     = words + QStringLiteral(" Typed after the Refresh.");
		const auto     edited   = std::string{ "issue (blocking): draw_scene() runs before the clear. (edited on GitHub)" };
		const auto     ready    = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto sent = [&] {
			const auto now = implementation->Fixture->review_mutation_evidence();
			return std::tie(now.Submissions, now.NativePosts, now.FilePosts, now.Replies, now.Resolutions)
				!= std::tie(probe.Before.Submissions, probe.Before.NativePosts, probe.Before.FilePosts, probe.Before.Replies, probe.Before.Resolutions);
		};
		const auto saved = [&]() -> std::optional<core::revisioned_review_draft> {
			if (!implementation->SharedDrafts || !probe.Draft) {
				return std::nullopt;
			}
			return implementation->SharedDrafts->read(implementation->DraftConsumer, *probe.Draft).Accepted;
		};
		const auto cardFor = [&](const std::string& aId) -> const qt::thread_card* {
			const auto found = std::ranges::find(cards->cards(), aId, &qt::thread_card::Id);
			return found == cards->cards().end() ? nullptr : &*found;
		};
		const auto keptId = [&] { return probe.Draft ? qt::conversation_cards_view::kept_reply_identity(*probe.Draft) : std::string{ }; };
		const auto button = [&](const char* aName) -> QAbstractButton* {
			auto* const open = cards->open_card();
			return open != nullptr ? open->findChild<QAbstractButton*>(QString::fromLatin1(aName)) : nullptr;
		};
		// [[BPR-1374]]. With BPR_REPLY_DRAFT_RENDER_DIRECTORY set, the window
		// as the owner will see it, in light and dark, for the look.
		const auto renders = qEnvironmentVariable("BPR_REPLY_DRAFT_RENDER_DIRECTORY");
		const auto render  = [&](const QString& aName) {
			if (renders.isEmpty() || window->appearance_settings() == nullptr) {
				return;
			}
			QDir{ }.mkpath(renders);
			for (const auto& [mode, name] : std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				static_cast<void>(window->grab().save(renders + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + name + QStringLiteral(".png"), "PNG"));
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		const auto openReply = [&]() -> bool {
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return false;
			}
			if (cards->open_thread_id() != identity) {
				cards->open_thread(identity);
				QCoreApplication::processEvents();
			}
			auto* const field = button("threadReplyField");
			if (field == nullptr || !field->isEnabled()) {
				return false;
			}
			field->click();
			QCoreApplication::processEvents();
			return true;
		};
		const auto held = [&](const QString& aWords) {
			const auto& draft = implementation->ReviewDraft;
			return draft && probe.Draft && draft->Id == *probe.Draft && draft->ReplyToThread == std::optional<std::string>{ std::string{ threadId } }
			&& draft->Body == aWords.toStdString() && draft->State == core::review_draft_state::drafting;
		};
		const auto finish = [&] {
			if (aCourse == course::deleted && !probe.HeldDiscard) {
				return fail("discard-not-held");
			}
			probe.Evidence.NothingSent = !sent();
			const auto& evidence       = probe.Evidence;
			if (!(evidence.Written && evidence.ServerShown && evidence.Kept && evidence.Presented && evidence.Flagged && evidence.Acted && evidence.NothingSent)) {
				return fail("evidence");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Before = implementation->Fixture->review_mutation_evidence();
				probe.Phase  = aCourse == course::restart ? state_type::phase::awaiting_restored : state_type::phase::awaiting_reply_open;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_reply_open:
			note("ready=" + std::to_string(ready() ? 1 : 0) + " open=" + cards->open_thread_id());
			if (!ready() || !openReply()) {
				return result::pending;
			}
			probe.Phase = state_type::phase::awaiting_reply_draft;
			return result::pending;
		case state_type::phase::awaiting_reply_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("reply=" + (draft && draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) + " editor=" + std::to_string(cards->reply_editor() != nullptr ? 1 : 0));
			if (!draft || draft->ReplyToThread != std::optional<std::string>{ std::string{ threadId } } || cards->reply_editor() == nullptr) {
				return result::pending;
			}
			probe.Draft = draft->Id;
			cards->reply_editor()->setPlainText(words);
			probe.Phase = state_type::phase::awaiting_reply_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_saved: {
			const auto stored = saved();
			note("held=" + std::to_string(held(words) ? 1 : 0) + " saved=" + std::to_string(stored && stored->Draft.Body == words.toStdString() ? 1 : 0) + " ready=" + std::to_string(ready() ? 1 : 0));
			if (!held(words) || !stored || stored->Draft.Body != words.toStdString() || !ready()) {
				return result::pending;
			}
			probe.Evidence.Written = !sent();
			render(QStringLiteral("written"));
			if (aCourse == course::write) {
				// Quit with the reply saved; `restart` reads it back.
				probe.Evidence.ServerShown = probe.Evidence.Kept = probe.Evidence.Presented = probe.Evidence.Flagged = probe.Evidence.Acted = true;
				return finish();
			}
			auto& fixture = *implementation->Fixture;
			if (aCourse == course::edited) {
				fixture.stage_server_root(std::string{ threadId }, edited, std::nullopt);
			} else if (aCourse == course::resolved) {
				fixture.stage_server_root(std::string{ threadId }, std::nullopt, true);
			} else if (aCourse == course::deleted || aCourse == course::kept) {
				fixture.stage_server_thread_deleted(std::string{ threadId });
			}
			if (aCourse == course::deleted) {
				// Hold only private storage, while the reread and visible note
				// complete normally. Confirm must retain this exact discard.
				probe.PrivateEffectsHold.emplace(hold_private_review_effects(aApplication));
			}
			window->github_refresh_action()->trigger();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_refreshed;
			return result::pending;
		}
		case state_type::phase::awaiting_refreshed: {
			const auto* const thread   = cardFor(identity);
			const auto* const kept     = cardFor(keptId());
			const auto&       snapshot = implementation->WindowRead->accepted_snapshot();
			const auto        onServer = snapshot && std::ranges::any_of(snapshot->ReviewThreads, [&](const application::github_review_thread_record& aThread) { return aThread.Thread.value() == threadId; });
			const auto        stored   = saved();
			const auto        gone     = aCourse == course::deleted || aCourse == course::kept;
			switch (aCourse) {
			case course::refresh:          probe.Evidence.ServerShown = onServer && thread != nullptr; break;
			case course::edited:           probe.Evidence.ServerShown = onServer && thread != nullptr && !thread->Posts.empty() && thread->Posts.front().Body.toStdString() == edited; break;
			case course::resolved:         probe.Evidence.ServerShown = onServer && thread != nullptr && thread->Resolved; break;
			case course::deleted:
			case course::kept:             probe.Evidence.ServerShown = snapshot && !onServer && thread == nullptr; break;
			case course::write:
			case course::restart:
			case course::outdated_publish:
			case course::outdated_write:
			case course::outdated_restart: break;
			}
			probe.Evidence.Kept = stored && stored->Draft.Body == words.toStdString() && stored->Draft.State == core::review_draft_state::drafting
				&& stored->Draft.ReplyToThread == std::optional<std::string>{ std::string{ threadId } };
			if (gone) {
				probe.Evidence.Presented = kept != nullptr && kept->Draft && kept->Placed && kept->Line == std::optional<std::uint32_t>{ 6U } && !kept->Posts.empty() && kept->Posts.front().Body == words;
				probe.Evidence.Flagged   = kept != nullptr && kept->UnsentReply == std::optional{ core::reply_draft_standing::thread_deleted }
					&& kept->Notice == QStringLiteral("The thread this replies to was deleted on GitHub.");
			} else {
				probe.Evidence.Presented = held(words) && thread != nullptr && thread->UnsentReply.has_value();
				probe.Evidence.Flagged   = thread != nullptr
					&& (aCourse == course::resolved ? thread->UnsentReply == std::optional{ core::reply_draft_standing::thread_resolved } && thread->Notice == QStringLiteral("The thread this replies to was resolved on GitHub.")
													: thread->UnsentReply == std::optional{ core::reply_draft_standing::answerable } && thread->Notice.isEmpty());
			}
			const auto& draft = implementation->ReviewDraft;
			note("ready=" + std::to_string(ready() ? 1 : 0) + " waited=" + std::to_string(probe.Waited) + " server=" + std::to_string(probe.Evidence.ServerShown ? 1 : 0)
				 + " kept=" + std::to_string(probe.Evidence.Kept ? 1 : 0) + " presented=" + std::to_string(probe.Evidence.Presented ? 1 : 0) + " flagged=" + std::to_string(probe.Evidence.Flagged ? 1 : 0)
				 + " editor=" + (draft ? std::to_string(draft->Id.value()) + ":" + (draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) : std::string{ "none" })
				 + " thread=" + std::to_string(thread != nullptr ? 1 : 0) + " keptNote=" + std::to_string(kept != nullptr ? 1 : 0) + " saved=" + (stored ? std::string{ core::name(stored->Draft.State) } : std::string{ "none" }));
			if (!ready() || (!(probe.Evidence.ServerShown && probe.Evidence.Kept && probe.Evidence.Presented && probe.Evidence.Flagged) && ++probe.Waited < 80U)) {
				return result::pending;
			}
			if (!probe.Evidence.ServerShown || !probe.Evidence.Kept || !probe.Evidence.Presented || !probe.Evidence.Flagged) {
				return fail("after-refresh");
			}
			render(QStringLiteral("refreshed-") + (aCourse == course::refresh ? QStringLiteral("refresh") : aCourse == course::edited ? QStringLiteral("edited")
													   : aCourse == course::resolved                                                  ? QStringLiteral("resolved")
																																	  : QStringLiteral("deleted")));
			switch (aCourse) {
			case course::refresh:
				probe.Phase = state_type::phase::awaiting_typed;
				return result::pending;
			case course::edited:
				probe.Evidence.Acted = true;
				return finish();
			case course::resolved:
				probe.Evidence.Acted = editor->publish_action()->isEnabled();
				return finish();
			case course::deleted:
			case course::kept:
				cards->open_thread(keptId());
				QCoreApplication::processEvents();
				render(QStringLiteral("kept-open"));
				probe.Phase = aCourse == course::deleted ? state_type::phase::awaiting_copied : state_type::phase::awaiting_kept;
				return result::pending;
			case course::write:
			case course::restart:
			case course::outdated_publish:
			case course::outdated_write:
			case course::outdated_restart: break;
			}
			return fail("course");
		}
		case state_type::phase::awaiting_typed: {
			// The reply in place is still the same draft: what is typed after
			// the Refresh is saved into it.
			if (cards->reply_editor() == nullptr) {
				note("reopen");
				static_cast<void>(openReply());
				return result::pending;
			}
			if (cards->reply_editor()->toPlainText() != more) {
				cards->reply_editor()->setPlainText(more);
				return result::pending;
			}
			const auto stored = saved();
			note("held=" + std::to_string(held(more) ? 1 : 0) + " saved=" + std::to_string(stored && stored->Draft.Body == more.toStdString() ? 1 : 0));
			if ((!held(more) || !stored || stored->Draft.Body != more.toStdString() || !ready()) && ++probe.Waited < 160U) {
				return result::pending;
			}
			probe.Evidence.Acted = held(more) && stored && stored->Draft.Body == more.toStdString();
			return finish();
		}
		case state_type::phase::awaiting_copied: {
			auto* const copy = button("threadCopyReply");
			note("copy=" + std::to_string(copy != nullptr ? 1 : 0) + " open=" + cards->open_thread_id());
			if (copy == nullptr) {
				return ++probe.Waited < 200U ? result::pending : fail("no-copy");
			}
			QGuiApplication::clipboard()->clear();
			copy->click();
			QCoreApplication::processEvents();
			if (QGuiApplication::clipboard()->text() != words) {
				return fail("copy-text clipboard=" + QGuiApplication::clipboard()->text().toStdString());
			}
			auto* const discard = button("threadDiscardReply");
			if (discard == nullptr) {
				return fail("no-discard");
			}
			discard->click();
			QCoreApplication::processEvents();
			auto* const question = window->findChild<QDialog*>(QStringLiteral("discardReplyConfirmation"));
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("discardReplyConfirmationConfirm")) : nullptr;
			if (yes == nullptr) {
				return fail("no-question");
			}
			// Asked first: nothing is discarded until the question is answered.
			if (const auto stored = saved(); !stored || stored->Draft.State != core::review_draft_state::drafting) {
				return fail("discarded-before-asked");
			}
			if (!probe.PrivateEffectsHold || implementation->PrivateDraftsReady || (implementation->PrivateReview && implementation->PrivateReview->binding())) {
				return fail("discard-store-not-held");
			}
			yes->click();
			QCoreApplication::processEvents();
			const auto retained = saved();
			probe.HeldDiscard   = retained && retained->Draft.State == core::review_draft_state::drafting;
			probe.PrivateEffectsHold.reset();
			if (!probe.HeldDiscard) {
				return fail("discard-before-store-resumes");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_discarded: {
			const auto stored = saved();
			const auto gone   = (!stored || stored->Draft.State == core::review_draft_state::discarded) && cardFor(keptId()) == nullptr;
			note("saved=" + (stored ? std::string{ core::name(stored->Draft.State) } : std::string{ "none" }) + " note=" + std::to_string(cardFor(keptId()) != nullptr ? 1 : 0));
			if (!gone && ++probe.Waited < 160U) {
				return result::pending;
			}
			probe.Evidence.Acted = gone;
			return finish();
		}
		case state_type::phase::awaiting_kept: {
			// Pressed once the window is settled, and again if a press made
			// while the drafts were still being restored changed nothing.
			if (ready() && implementation->PrivateDraftsReady && probe.Waited % 40U == 0U) {
				if (auto* const keep = button("threadKeepReplyAsComment"); keep != nullptr) {
					keep->click();
					QCoreApplication::processEvents();
				}
			}
			const auto stored = saved();
			const auto line   = std::ranges::find_if(cards->cards(), [&](const qt::thread_card& aCard) { return aCard.Draft && !aCard.UnsentReply.has_value() && aCard.Line == std::optional<std::uint32_t>{ 6U }; });
			const auto kept   = stored && !stored->Draft.ReplyToThread && stored->Draft.Body == words.toStdString() && stored->Draft.State == core::review_draft_state::drafting
				&& stored->Draft.Anchor.Line == 5U && line != cards->cards().end() && line->Placed && cardFor(keptId()) == nullptr;
			note("comment=" + std::to_string(stored && !stored->Draft.ReplyToThread ? 1 : 0) + " note=" + std::to_string(line != cards->cards().end() ? 1 : 0));
			if (!kept && ++probe.Waited < 160U) {
				return result::pending;
			}
			render(QStringLiteral("kept-as-comment"));
			probe.Evidence.Acted = kept;
			return finish();
		}
		case state_type::phase::awaiting_restored: {
			// After the restart: the saved reply, found among the restored
			// drafts, marked on its thread's note.
			if (!probe.Draft && implementation->SharedDrafts) {
				for (const auto& draft : implementation->SharedDrafts->recovery_snapshot()) {
					if (draft.Draft.ReplyToThread == std::optional<std::string>{ std::string{ threadId } } && draft.Draft.Body == words.toStdString()
						&& draft.Draft.State == core::review_draft_state::drafting) {
						probe.Draft = draft.Draft.Id;
					}
				}
			}
			const auto* const thread   = cardFor(identity);
			probe.Evidence.Written     = true;
			probe.Evidence.Kept        = probe.Draft.has_value();
			probe.Evidence.ServerShown = thread != nullptr;
			probe.Evidence.Flagged     = thread != nullptr && thread->UnsentReply == std::optional{ core::reply_draft_standing::answerable } && thread->Notice.isEmpty();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " drafts=" + std::to_string(implementation->PrivateDraftsReady ? 1 : 0) + " saved=" + std::to_string(probe.Draft ? 1 : 0)
				 + " thread=" + std::to_string(thread != nullptr ? 1 : 0) + " marked=" + std::to_string(probe.Evidence.Flagged ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!(probe.Evidence.Kept && probe.Evidence.Flagged) && ++probe.Waited < 120U)) {
				return result::pending;
			}
			if (!probe.Evidence.Kept || !probe.Evidence.Flagged) {
				return fail("not-restored");
			}
			render(QStringLiteral("restarted"));
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reopened;
			return result::pending;
		}
		case state_type::phase::awaiting_reopened: {
			// Reply on the thread opens the saved reply, with its words.
			if (!held(words)) {
				note("opening editor=" + (implementation->ReviewDraft ? std::to_string(implementation->ReviewDraft->Id.value()) : std::string{ "none" }));
				if (ready() && probe.Waited++ % 10U == 0U) {
					static_cast<void>(openReply());
				}
				return probe.Waited < 200U ? result::pending : fail("reply-not-reopened");
			}
			if (cards->reply_editor() == nullptr) {
				static_cast<void>(openReply());
				return ++probe.Waited < 200U ? result::pending : fail("no-reply-editor");
			}
			probe.Evidence.Presented = true;
			probe.Evidence.Acted     = cards->reply_editor()->toPlainText() == words;
			render(QStringLiteral("restarted-reply"));
			return finish();
		}
		case state_type::phase::passed:
			return result::passed;
		case state_type::phase::awaiting_outdated:
		case state_type::phase::awaiting_published:
		case state_type::phase::failed:
			break;
		}
		return result::failed;
	}

	/// [[BPR-1745]]. The owner, 2026-10-22: "Allow replying to an
	/// outdated thread from the drawer, like GitHub does". Over
	/// conventional-comments-v1, `PRRT_cc_1` on src/engine/renderer.cpp is
	/// made outdated by a push (its line left the diff; every post was written
	/// on an earlier commit), the public Refresh reads it so, and its Reply in
	/// the Comments drawer opens a reply bound to the thread, with no refusal
	/// and one quiet line saying the thread's line has changed. Before the
	/// change the Reply was refused with "This thread's line isn't in the code
	/// shown, so it can't take a reply here." and no draft was made.
	auto detail::github_application_fixture_access::exercise_outdated_reply(
		github_application& aApplication, const detail::reply_draft_course aCourse) -> detail::reply_draft_probe_result
	{
		using result               = detail::reply_draft_probe_result;
		using course               = detail::reply_draft_course;
		using state_type           = reply_draft_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReplyDraftProbe.has_value()) {
			implementation->ReplyDraftProbe.emplace();
		}
		auto& probe = *implementation->ReplyDraftProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		if (cards == nullptr || window->review_editor() == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		constexpr auto path      = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto threadId  = std::string_view{ "PRRT_cc_1" };
		constexpr auto writtenOn = std::string_view{ "3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c" };
		const auto     identity  = "review-thread:" + std::string{ threadId };
		const auto     words     = QStringLiteral("question: Is this still true after the push? (outdated-thread reply)");
		const auto     ready     = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto now      = [&] { return implementation->Fixture->review_mutation_evidence(); };
		const auto sentNone = [&] {
			const auto after = now();
			return std::tie(after.Submissions, after.NativePosts, after.FilePosts, after.Replies, after.Resolutions)
				== std::tie(probe.Before.Submissions, probe.Before.NativePosts, probe.Before.FilePosts, probe.Before.Replies, probe.Before.Resolutions);
		};
		const auto saved = [&]() -> std::optional<core::revisioned_review_draft> {
			if (!implementation->SharedDrafts || !probe.Draft) {
				return std::nullopt;
			}
			return implementation->SharedDrafts->read(implementation->DraftConsumer, *probe.Draft).Accepted;
		};
		const auto cardFor = [&](const std::string& aId) -> const qt::thread_card* {
			const auto found = std::ranges::find(cards->cards(), aId, &qt::thread_card::Id);
			return found == cards->cards().end() ? nullptr : &*found;
		};
		const auto button = [&](const char* aName) -> QAbstractButton* {
			auto* const open = cards->open_card();
			return open != nullptr ? open->findChild<QAbstractButton*>(QString::fromLatin1(aName)) : nullptr;
		};
		const auto refused = [&] {
			auto* const notice = window->conversation_panel()->notice_label();
			return notice != nullptr && notice->isVisibleTo(window) && !notice->text().isEmpty();
		};
		const auto outdatedNote = [&] {
			auto* const open  = cards->open_card();
			auto* const label = open != nullptr ? open->findChild<QLabel*>(QStringLiteral("threadReplyOutdatedNote")) : nullptr;
			return label != nullptr && label->isVisibleTo(window) && label->text() == QStringLiteral("Outdated: this thread's line has changed since it was written");
		};
		const auto renders = qEnvironmentVariable("BPR_REPLY_DRAFT_RENDER_DIRECTORY");
		const auto render  = [&](const QString& aName) {
			if (renders.isEmpty() || window->appearance_settings() == nullptr) {
				return;
			}
			QDir{ }.mkpath(renders);
			for (const auto& [mode, name] : std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				static_cast<void>(window->grab().save(renders + QStringLiteral("/outdated-") + aName + QLatin1Char{ '-' } + name + QStringLiteral(".png"), "PNG"));
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		const auto openReply = [&]() -> bool {
			if (window->current_modality() != qt::review_modality::review) {
				window->choose_modality(qt::review_modality::review);
				QCoreApplication::processEvents();
				return false;
			}
			if (cards->open_thread_id() != identity) {
				cards->open_thread(identity);
				QCoreApplication::processEvents();
			}
			auto* const field = button("threadReplyField");
			if (field == nullptr || !field->isEnabled() || !field->isVisibleTo(window)) {
				return false;
			}
			field->click();
			QCoreApplication::processEvents();
			return true;
		};
		const auto held = [&](const QString& aWords) {
			const auto& draft = implementation->ReviewDraft;
			return draft && probe.Draft && draft->Id == *probe.Draft && core::thread_bound_reply(*draft) && draft->ReplyToThread == std::optional<std::string>{ std::string{ threadId } }
			&& draft->Body == aWords.toStdString() && draft->State == core::review_draft_state::drafting;
		};
		const auto finish = [&] {
			const auto& evidence = probe.Evidence;
			if (!(evidence.Written && evidence.ServerShown && evidence.Kept && evidence.Presented && evidence.Flagged && evidence.Acted && evidence.NothingSent)) {
				return fail("evidence");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed != detail::fixture_file_reveal_result::shown || !ready()) {
				return result::pending;
			}
			// The push: the thread's line leaves the diff, and the public
			// Refresh reads it outdated.
			probe.Before = now();
			implementation->Fixture->stage_server_thread_outdated(std::string{ threadId }, std::string{ writtenOn });
			window->github_refresh_action()->trigger();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_outdated;
			return result::pending;
		}
		case state_type::phase::awaiting_outdated: {
			const auto* const thread   = implementation->accepted_thread(identity);
			const auto* const card     = cardFor(identity);
			probe.Evidence.ServerShown = thread != nullptr && thread->Outdated && !thread->Line && card != nullptr && card->ReplyOnThread;
			note("ready=" + std::to_string(ready() ? 1 : 0) + " outdated=" + std::to_string(thread != nullptr && thread->Outdated ? 1 : 0) + " card=" + std::to_string(card != nullptr ? 1 : 0)
				 + " onThread=" + std::to_string(card != nullptr && card->ReplyOnThread ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!probe.Evidence.ServerShown && ++probe.Waited < 160U)) {
				return result::pending;
			}
			if (!probe.Evidence.ServerShown) {
				return fail("not-outdated");
			}
			probe.Waited = 0U;
			probe.Phase  = aCourse == course::outdated_restart ? state_type::phase::awaiting_restored : state_type::phase::awaiting_reply_open;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_open:
			note("ready=" + std::to_string(ready() ? 1 : 0) + " open=" + cards->open_thread_id());
			if (!ready() || !openReply()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-reply-field");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reply_draft;
			return result::pending;
		case state_type::phase::awaiting_reply_draft: {
			const auto& draft = implementation->ReviewDraft;
			note("reply=" + (draft && draft->ReplyToThread ? *draft->ReplyToThread : std::string{ "-" }) + " bound=" + std::to_string(draft && core::thread_bound_reply(*draft) ? 1 : 0)
				 + " draft=" + (draft ? std::to_string(draft->Id.value()) + ":" + std::to_string(draft->Anchor.Line) : std::string{ "none" })
				 + " private=" + std::to_string(implementation->PrivateReview ? 1 : 0) + std::to_string(implementation->PrivateDraftsReady ? 1 : 0)
				 + std::to_string(implementation->PrivateDraftSelectionPending ? 1 : 0) + std::to_string(implementation->PendingThreadReply ? 1 : 0)
				 + " selected=" + std::to_string(implementation->SelectedSemanticSelection ? 1 : 0) + " diff=" + std::to_string(implementation->SelectedDiffPublication ? 1 : 0)
				 + " editor=" + std::to_string(cards->reply_editor() != nullptr ? 1 : 0) + " refused=" + std::to_string(refused() ? 1 : 0)
				 + " notice=" + window->conversation_panel()->notice_label()->text().toStdString());
			if (refused()) {
				return fail("refused");
			}
			if (!draft || !core::thread_bound_reply(*draft) || draft->ReplyToThread != std::optional<std::string>{ std::string{ threadId } } || cards->reply_editor() == nullptr) {
				return ++probe.Waited < 200U ? result::pending : fail("no-thread-bound-reply");
			}
			probe.Evidence.Flagged = outdatedNote();
			if (!probe.Evidence.Flagged) {
				return fail("no-outdated-note");
			}
			probe.Draft = draft->Id;
			cards->reply_editor()->setPlainText(words);
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reply_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_reply_saved: {
			const auto stored = saved();
			note("held=" + std::to_string(held(words) ? 1 : 0) + " saved=" + std::to_string(stored && stored->Draft.Body == words.toStdString() ? 1 : 0) + " ready=" + std::to_string(ready() ? 1 : 0));
			if (!held(words) || !stored || stored->Draft.Body != words.toStdString() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-saved");
			}
			probe.Evidence.Written   = sentNone();
			probe.Evidence.Kept      = core::thread_bound_reply(stored->Draft);
			probe.Evidence.Presented = cards->reply_editor() != nullptr && cards->reply_editor()->toPlainText() == words;
			render(QStringLiteral("written"));
			if (aCourse == course::outdated_write) {
				// Quit with the reply saved; `outdated-restart` reads it back.
				probe.Evidence.Acted       = true;
				probe.Evidence.NothingSent = sentNone();
				return finish();
			}
			auto* const publish = button("threadReplyPublish");
			if (publish == nullptr || !publish->isEnabled()) {
				return ++probe.Waited < 400U ? result::pending : fail("publish-not-offered");
			}
			publish->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_published;
			return result::pending;
		}
		case state_type::phase::awaiting_published: {
			const auto        after  = now();
			const auto        stored = saved();
			const auto* const card   = cardFor(identity);
			const auto        landed = card != nullptr && std::ranges::any_of(card->Posts, [&](const qt::card_post& aPost) { return aPost.Body == words; });
			const auto        bound  = after.Replies == probe.Before.Replies + 1U && after.LastReplyThread == threadId && after.LastReplyThreadBound && after.LastReplyCommit == writtenOn
				&& after.NativePosts == probe.Before.NativePosts && after.FilePosts == probe.Before.FilePosts && after.Resolutions == probe.Before.Resolutions;
			note("ready=" + std::to_string(ready() ? 1 : 0) + " replies=" + std::to_string(after.Replies - probe.Before.Replies) + " bound=" + std::to_string(after.LastReplyThreadBound ? 1 : 0)
				 + " commit=" + after.LastReplyCommit + " saved=" + (stored ? std::string{ core::name(stored->Draft.State) } : std::string{ "none" }) + " landed=" + std::to_string(landed ? 1 : 0)
				 + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!(bound && stored && stored->Draft.State == core::review_draft_state::published && landed) && ++probe.Waited < 400U)) {
				return result::pending;
			}
			// Exactly one write: the reply, bound to the thread and its commit.
			probe.Evidence.NothingSent = bound;
			probe.Evidence.Acted       = stored && stored->Draft.State == core::review_draft_state::published && landed;
			render(QStringLiteral("published"));
			return finish();
		}
		case state_type::phase::awaiting_restored: {
			// After the restart: the saved reply bound to the outdated thread,
			// found among the restored drafts, marked on its thread's note.
			if (!probe.Draft && implementation->SharedDrafts) {
				for (const auto& draft : implementation->SharedDrafts->recovery_snapshot()) {
					if (core::thread_bound_reply(draft.Draft) && draft.Draft.ReplyToThread == std::optional<std::string>{ std::string{ threadId } } && draft.Draft.Body == words.toStdString()
						&& draft.Draft.State == core::review_draft_state::drafting) {
						probe.Draft = draft.Draft.Id;
					}
				}
			}
			const auto* const thread = cardFor(identity);
			probe.Evidence.Written   = true;
			probe.Evidence.Kept      = probe.Draft.has_value();
			const auto marked        = thread != nullptr && thread->UnsentReply == std::optional{ core::reply_draft_standing::answerable } && thread->Notice.isEmpty();
			note("ready=" + std::to_string(ready() ? 1 : 0) + " drafts=" + std::to_string(implementation->PrivateDraftsReady ? 1 : 0) + " saved=" + std::to_string(probe.Draft ? 1 : 0)
				 + " marked=" + std::to_string(marked ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (!ready() || (!(probe.Evidence.Kept && marked) && ++probe.Waited < 160U)) {
				return result::pending;
			}
			if (!probe.Evidence.Kept || !marked) {
				return fail("not-restored");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reopened;
			return result::pending;
		}
		case state_type::phase::awaiting_reopened: {
			if (refused()) {
				return fail("refused");
			}
			if (!held(words) || cards->reply_editor() == nullptr) {
				note("opening editor=" + (implementation->ReviewDraft ? std::to_string(implementation->ReviewDraft->Id.value()) : std::string{ "none" }));
				if (ready() && probe.Waited++ % 10U == 0U) {
					static_cast<void>(openReply());
				}
				return probe.Waited < 200U ? result::pending : fail("reply-not-reopened");
			}
			probe.Evidence.Flagged     = outdatedNote();
			probe.Evidence.Presented   = cards->reply_editor()->toPlainText() == words;
			probe.Evidence.Acted       = held(words);
			probe.Evidence.NothingSent = sentNone();
			render(QStringLiteral("restarted"));
			return finish();
		}
		case state_type::phase::passed:
			return result::passed;
		default:
			break;
		}
		return fail("phase");
	}

	auto detail::github_application_fixture_access::reply_draft_evidence(
		const github_application& aApplication) -> detail::reply_draft_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReplyDraftProbe.has_value() ? implementation->ReplyDraftProbe->Evidence : detail::reply_draft_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::reply_draft_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReplyDraftProbe.has_value() ? implementation->ReplyDraftProbe->Diagnostic : std::string{ };
	}

	auto detail::github_application_fixture_access::exercise_refresh_pick(
		github_application& aApplication, const detail::refresh_pick_course aCourse) -> detail::refresh_pick_probe_result
	{
		using result               = detail::refresh_pick_probe_result;
		using state_type           = refresh_pick_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->RefreshPickProbe.has_value()) {
			implementation->RefreshPickProbe.emplace();
		}
		auto& probe = *implementation->RefreshPickProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->TabHost == nullptr || !implementation->WindowRead || !implementation->ReadDecision) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const view   = window->files_drawer() != nullptr ? window->files_drawer()->contents_view() : nullptr;
		const auto  fail   = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		constexpr auto path   = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto other  = std::string_view{ "src/engine/renderer.h" };
		const auto     shown  = core::file_identity_of(path);
		const auto     picked = core::file_identity_of(other);
		const auto&    last   = implementation->LastSelectedDiff;
		const auto     ready  = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->WindowRead->state().PendingPullRequest.has_value();
		};
		const auto active = [&]() -> const core::document_tab* {
			const auto& tabs = implementation->TabHost->state();
			return tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		};
		const auto activeFile = [&]() -> std::optional<core::file_identity> {
			const auto* const tab = active();
			return tab != nullptr ? tab->View.File : std::nullopt;
		};
		const auto name = [&](const std::optional<core::file_identity>& aFile) -> std::string {
			if (!aFile) {
				return "-";
			}
			if (*aFile == shown) {
				return "cpp";
			}
			return *aFile == picked ? "h" : "other";
		};
		const auto facts = [&] {
			return "selected=" + name(implementation->WorkbenchHost->selected()) + " tab=" + name(activeFile())
				+ " diff=" + (last ? std::string{ last->Request.File.PresentationPath } : std::string{ "-" })
				+ " review=" + std::to_string(window->current_modality() == qt::review_modality::review ? 1 : 0)
				+ " ready=" + std::to_string(ready() ? 1 : 0) + " load=" + std::to_string(implementation->TabLoad ? 1 : 0);
		};
		// Review shows the picked file: chosen, in the active tab, its diff on screen.
		const auto showsPick = [&] {
			const auto chosen = implementation->WorkbenchHost->selected() == std::optional{ picked } && activeFile() == std::optional{ picked };
			const auto drawn  = last && last->Request.File.PresentationPath == other && window->comparison_surface()->current_result() != nullptr;
			return window->current_modality() == qt::review_modality::review && chosen && drawn;
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Phase = state_type::phase::awaiting_line;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_line: {
			// A line the reviewer chooses in renderer.cpp, which its tab keeps:
			// the place the window holds before the read.
			auto* const surface = window->comparison_surface();
			if (!probe.Line) {
				const auto opened = surface->selection() ? std::optional{ surface->selection()->PreferredLine.Line } : std::nullopt;
				for (auto candidate = std::uint32_t{ 1U }; candidate < 32U && !probe.Line; ++candidate) {
					if (candidate != opened && surface->select_conversation_line(core::diff_side::right, candidate)) {
						probe.Line = candidate;
					}
				}
			}
			const auto* const tab  = active();
			const auto        kept = probe.Line && surface->selection() && surface->selection()->PreferredLine.Line == *probe.Line && tab != nullptr
				&& tab->View.File == std::optional{ shown } && tab->View.Selection == surface->selection();
			note(facts() + " line=" + (probe.Line ? std::to_string(*probe.Line) : std::string{ "-" }) + " kept=" + std::to_string(kept ? 1 : 0));
			if (!ready() || !kept || implementation->TabLoad || implementation->TabAdvanceQueued) {
				return result::pending;
			}
			const auto& accepted = implementation->WindowRead->accepted_snapshot();
			probe.ShownBefore    = accepted ? std::optional{ accepted->Snapshot } : std::nullopt;
			// The read's answer waits on the window's read lane until the pick.
			implementation->ReadDecision->hold(true);
			if (aCourse == detail::refresh_pick_course::refresh) {
				window->github_refresh_action()->trigger();
			} else {
				implementation->reread_after_publication();
			}
			probe.Phase = state_type::phase::awaiting_read;
			return result::pending;
		}
		case state_type::phase::awaiting_read: {
			const auto row = drawer_row(*window, other);
			note(facts() + " reading=" + std::to_string(implementation->WindowRead->state().PendingPullRequest ? 1 : 0) + " row=" + std::to_string(row.isValid() ? 1 : 0));
			if (!implementation->WindowRead->state().PendingPullRequest || view == nullptr || !row.isValid()
				|| implementation->WorkbenchHost->outstanding_requests() != 0U || implementation->TabLoad || implementation->TabAdvanceQueued) {
				return result::pending;
			}
			// The window takes a picked file into the active tab on a later
			// turn. Holding that turn until the read lands gives the order a
			// busy event loop can give: the pick, the read, then the tab.
			implementation->TabAdvanceQueued = true;
			view->setCurrentIndex(row);
			implementation->ReadDecision->hold(false);
			if (implementation->WorkbenchHost->selected() != std::optional{ picked }) {
				implementation->TabAdvanceQueued = false;
				return fail("pick-refused");
			}
			probe.Phase = state_type::phase::awaiting_landed;
			return result::pending;
		}
		case state_type::phase::awaiting_landed: {
			const auto& accepted = implementation->WindowRead->accepted_snapshot();
			const auto  landed   = accepted && std::optional{ accepted->Snapshot } != probe.ShownBefore && !implementation->WindowRead->state().PendingPullRequest;
			note(facts() + " landed=" + std::to_string(landed ? 1 : 0));
			if (!landed) {
				return result::pending;
			}
			// The read retired the workbench's selection while the active tab
			// still named renderer.cpp: the pick shows nowhere.
			probe.Evidence.Raced             = activeFile() == std::optional{ shown } && !implementation->WorkbenchHost->selected().has_value();
			implementation->TabAdvanceQueued = false;
			if (!probe.Evidence.Raced) {
				return fail("read-not-between-pick-and-tab");
			}
			implementation->schedule_tab_presentation();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_settled;
			return result::pending;
		}
		case state_type::phase::awaiting_settled: {
			note(facts() + " waited=" + std::to_string(probe.Waited));
			const auto settled = ready() && !implementation->TabLoad && implementation->WorkbenchHost->selected().has_value() && last
				&& window->comparison_surface()->current_result() != nullptr;
			if (!probe.Evidence.Kept) {
				if (!settled) {
					return ++probe.Waited < 400U ? result::pending : fail("not-settled");
				}
				if (!showsPick()) {
					return fail("pick-lost");
				}
				probe.Evidence.Kept = true;
				probe.Waited        = 0U;
				return result::pending;
			}
			if (!settled || !showsPick()) {
				return fail("pick-lost-later");
			}
			if (++probe.Waited < 20U) {
				return result::pending;
			}
			probe.Evidence.Stayed = true;
			probe.Phase           = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed: return result::passed;
		case state_type::phase::failed: return result::failed;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::refresh_pick_evidence(const github_application& aApplication) -> detail::refresh_pick_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->RefreshPickProbe.has_value() ? implementation->RefreshPickProbe->Evidence : detail::refresh_pick_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::refresh_pick_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->RefreshPickProbe.has_value() ? implementation->RefreshPickProbe->Diagnostic : std::string{ };
	}

	auto detail::github_application_fixture_access::exercise_draft_delete(
		github_application& aApplication, const detail::draft_delete_course aCourse) -> detail::draft_delete_probe_result
	{
		using result               = detail::draft_delete_probe_result;
		using course               = detail::draft_delete_course;
		using state_type           = draft_delete_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->DraftDeleteProbe.has_value()) {
			implementation->DraftDeleteProbe.emplace();
		}
		auto& probe = *implementation->DraftDeleteProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr || implementation->ReviewMutationHost == nullptr) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const cards  = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor = window->review_editor();
		auto* const model  = window->conversation_model();
		if (cards == nullptr || editor == nullptr || model == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		constexpr auto path  = std::string_view{ "src/engine/renderer.cpp" };
		const auto     words = std::array{
			std::string{ "issue: this draft on a changed line is deleted. (BPR-1862 line)" },
			std::string{ "issue: this draft outside every hunk is deleted. (BPR-1862 off-hunk)" },
			std::string{ "The agent's answer, drafted outside every hunk, is deleted. (BPR-1862 agent)" },
			std::string{ "The agent's answer, drafted in a review, is deleted from Finish your review. (BPR-1862 agent review)" },
		};
		// Drafted again from the same turn, and kept: `reopened` finds it.
		const auto again = std::string{ "The agent's answer, drafted again outside every hunk, is kept. (BPR-1862 agent again)" };
		// The draft an agent turn holds, while it says Go to draft.
		const auto turnHolds = [&](const std::optional<std::string>& aKey) -> std::optional<core::review_draft_id> {
	#if defined(BPR_AGENT_INTEGRATIONS)
			if (aKey) {
				if (const auto found = implementation->AgentAnswerDrafts.find(*aKey); found != implementation->AgentAnswerDrafts.end()) {
					return found->second.Drafted;
				}
			}
	#else
			static_cast<void>(aKey);
	#endif
			return std::nullopt;
		};
		const auto  index = aCourse == course::line ? 0U : aCourse == course::off_hunk ? 1U
																					   : 2U;
		const auto& mine  = words[index];
		const auto  ready = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto saved = [&] {
			return implementation->SharedDrafts ? implementation->SharedDrafts->recovery_snapshot() : std::vector<core::revisioned_review_draft>{ };
		};
		// An active draft in the store with these words.
		const auto stored = [&](const std::string& aWords) {
			const auto all = saved();
			return std::ranges::any_of(all, [&aWords](const core::revisioned_review_draft& aDraft) {
				return aDraft.Draft.Body == aWords && (aDraft.Draft.State == core::review_draft_state::drafting || aDraft.Draft.State == core::review_draft_state::publishing);
			});
		};
		// The words anywhere the window shows a draft: the drawer's field, the
		// editor, or a note's post.
		const auto shown = [&](const std::string& aWords) {
			const auto text = QString::fromStdString(aWords);
			if (cards->draft_editor() != nullptr && cards->draft_editor()->toPlainText() == text) {
				return true;
			}
			if (editor->body_editor()->toPlainText() == text) {
				return true;
			}
			return std::ranges::any_of(cards->cards(), [&text](const qt::thread_card& aCard) {
				return aCard.Draft && std::ranges::any_of(aCard.Posts, [&text](const qt::card_post& aPost) { return aPost.Body == text; });
			});
		};
		const auto facts = [&] {
			const auto& draft = implementation->ReviewDraft;
			return "ready=" + std::to_string(ready() ? 1 : 0) + " draft=" + (draft ? std::to_string(draft->Id.value()) : std::string{ "-" })
				+ " state=" + (draft ? std::string{ core::name(draft->State) } : std::string{ "-" }) + " body=" + (draft ? draft->Body : std::string{ "-" })
				+ " stored=" + std::to_string(stored(mine) ? 1 : 0) + " shown=" + std::to_string(shown(mine) ? 1 : 0)
				+ " field=" + std::to_string(cards->draft_editor() != nullptr && cards->draft_editor()->toPlainText().toStdString() == mine ? 1 : 0)
				+ " editor=" + std::to_string(editor->body_editor()->toPlainText().toStdString() == mine ? 1 : 0) + " cards=" + [&] {
					  auto ids = std::string{ };
					  for (const auto& card : cards->cards()) {
						  if (card.Draft) {
							  ids += card.Id + "[" + (card.Posts.empty() ? std::string{ "-" } : card.Posts.front().Body.toStdString()) + "],";
						  }
					  }
					  return ids;
				  }()
				+ " writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " status=" + editor->mutation_status_label()->text().toStdString() + " bar=" + window->statusBar()->currentMessage().toStdString();
		};
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				if (window->current_modality() != qt::review_modality::review) {
					window->choose_modality(qt::review_modality::review);
					QCoreApplication::processEvents();
				}
				probe.Phase = state_type::phase::awaiting_lines;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_lines: {
			note(facts() + " drafts=" + std::to_string(implementation->PrivateDraftsReady ? 1 : 0));
			if (!ready() || !implementation->PrivateDraftsReady || window->comparison_surface()->current_result() == nullptr) {
				return result::pending;
			}
			if (aCourse == course::reopened) {
				// After the restart: none of the deleted drafts is kept, and
				// none shows.
				for (const auto& written : words) {
					if (stored(written)) {
						return fail("reopened-store-kept:" + written);
					}
					if (shown(written)) {
						return fail("reopened-view-shows:" + written);
					}
				}
				// The agent turn's second draft, never deleted, is kept.
				if (!stored(again)) {
					return fail("reopened-kept-draft-lost");
				}
				probe.Evidence.Written = probe.Evidence.Asked = probe.Evidence.ViewGone = probe.Evidence.StoreGone = true;
				probe.Waited                                                                                       = 0U;
				probe.Phase                                                                                        = state_type::phase::awaiting_stayed;
				return result::pending;
			}
			// [[BPR-1735]]. GitHub's own diff of the file decides which lines
			// are inside a hunk.
			const auto lines = implementation->shown_github_diff_lines();
			if (!lines) {
				return result::pending;
			}
			if (!*lines) {
				return fail("github-diff-unknown");
			}
			const auto all = saved();
			for (auto candidate = std::uint32_t{ 0U }; candidate < 96U && !probe.Line; ++candidate) {
				const auto outside = core::place_on_github_diff(*lines, core::diff_side::right, candidate, std::nullopt) == core::github_diff_placement::outside_diff;
				if (outside != (aCourse != course::line)) {
					continue;
				}
				if (std::ranges::any_of(all, [candidate](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == candidate && !aDraft.Draft.Body.empty(); })) {
					continue;
				}
				if (window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate) && window->conversation_panel()->anchor_line().has_value()
					&& model->rowCount() == 0) {
					probe.Line = candidate;
				}
			}
			if (!probe.Line) {
				return fail("no-line");
			}
			probe.Phase = state_type::phase::awaiting_line;
			return result::pending;
		}
		case state_type::phase::awaiting_line: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " line=" + std::to_string(*probe.Line));
			if (!ready() || !draft || draft->ReplyToThread || draft->Anchor.Line != *probe.Line || draft->Anchor.Side != core::diff_side::right
				|| draft->State != core::review_draft_state::drafting || !window->comment_action()->isEnabled()) {
				return result::pending;
			}
			if (aCourse == course::agent) {
	#if defined(BPR_AGENT_INTEGRATIONS)
				// [[BPR-1773]]. The Draft comment an agent's answer offers,
				// for a question asked with this line selected.
				const auto target = implementation->decide_answer_draft();
				const auto found  = implementation->AgentAnswerDrafts.find(target.Key);
				if (found == implementation->AgentAnswerDrafts.end() || found->second.Place != core::answer_draft_place::line_comment || found->second.First != *probe.Line) {
					return fail("agent-target-not-the-line");
				}
				probe.AgentKey = target.Key;
				implementation->draft_agent_answer(target.Key, QString::fromStdString(mine), [&probe](const bool aDrafted, const QString& aWhy) {
					probe.AgentAsked = true;
					if (!aDrafted) {
						probe.Diagnostic += " | agent-refused=" + aWhy.toStdString();
					}
				});
	#else
				return fail("agent-integrations-off");
	#endif
			} else {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
				if (cards->draft_editor() == nullptr || cards->draft_editor()->isReadOnly()) {
					return fail("drawer-draft-not-editable");
				}
				cards->draft_editor()->setPlainText(QString::fromStdString(mine));
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_saved: {
			const auto& draft = implementation->ReviewDraft;
			note(facts());
			if (!ready() || !draft || draft->Body != mine || !stored(mine)) {
				return ++probe.Waited < 2000U ? result::pending : fail("not-saved");
			}
			if (aCourse == course::agent) {
				// The turn says Go to draft, on this draft.
				if (!probe.AgentAsked) {
					return ++probe.Waited < 2000U ? result::pending : fail("agent-not-answered");
				}
				if (turnHolds(probe.AgentKey) != std::optional{ draft->Id }) {
					return fail("agent-turn-not-on-its-draft");
				}
			}
			probe.Draft            = draft->Id;
			probe.Evidence.Written = true;
			// The draft opens in the drawer, where its Delete is.
			if (!cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			auto* const open   = cards->open_card();
			auto* const remove = open != nullptr ? open->findChild<QAbstractButton*>(QStringLiteral("threadDeleteDraft")) : nullptr;
			if (remove == nullptr || !remove->isEnabled() || !remove->isVisibleTo(window)) {
				return ++probe.Waited < 2000U ? result::pending : fail("no-delete");
			}
			remove->click();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_question;
			return result::pending;
		}
		case state_type::phase::awaiting_question: {
			auto* const question = window->delete_draft_confirmation();
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("deleteDraftConfirmationConfirm")) : nullptr;
			note(facts());
			if (yes == nullptr || !question->isVisible()) {
				return fail("delete-asked-nothing");
			}
			yes->click();
			QCoreApplication::processEvents();
			probe.Evidence.Asked = true;
			probe.Waited         = 0U;
			probe.Phase          = state_type::phase::awaiting_deleted;
			return result::pending;
		}
		case state_type::phase::awaiting_deleted: {
			note(facts() + " waited=" + std::to_string(probe.Waited));
			if (!ready() || stored(mine) || shown(mine)) {
				return ++probe.Waited < 400U ? result::pending : fail("not-deleted");
			}
			probe.Evidence.StoreGone = true;
			probe.Evidence.ViewGone  = true;
			// The line chosen again reopens nothing with these words.
			static_cast<void>(window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Line == 0U ? 1U : *probe.Line - 1U));
			QCoreApplication::processEvents();
			static_cast<void>(window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Line));
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_stayed;
			return result::pending;
		}
		case state_type::phase::awaiting_stayed: {
			note(facts() + " waited=" + std::to_string(probe.Waited));
			for (const auto& written : words) {
				if (stored(written) || shown(written)) {
					return fail("came-back:" + written);
				}
			}
			if (!ready() || ++probe.Waited < 20U) {
				return probe.Waited < 2000U ? result::pending : fail("not-settled");
			}
			probe.Evidence.Stayed = true;
			if (aCourse != course::agent) {
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			// The turn let its deleted draft go, and Draft comment pressed
			// again makes a new one.
			probe.Evidence.Released = !turnHolds(probe.AgentKey).has_value();
			if (!probe.Evidence.Released) {
				return fail("agent-turn-still-holds-deleted-draft");
			}
			const auto& draft = implementation->ReviewDraft;
			if (!draft || draft->Anchor.Line != *probe.Line || draft->State != core::review_draft_state::drafting || !window->comment_action()->isEnabled()) {
				return probe.Waited < 2000U ? result::pending : fail("line-draft-not-back");
			}
	#if defined(BPR_AGENT_INTEGRATIONS)
			probe.AgentAsked = false;
			implementation->draft_agent_answer(*probe.AgentKey, QString::fromStdString(again), [&probe](const bool aDrafted, const QString& aWhy) {
				probe.AgentAsked = true;
				if (!aDrafted) {
					probe.Diagnostic += " | agent-again-refused=" + aWhy.toStdString();
				}
			});
	#endif
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_again_saved: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " again=" + std::to_string(stored(again) ? 1 : 0));
			if (!ready() || !probe.AgentAsked || !draft || draft->Body != again || !stored(again)) {
				return ++probe.Waited < 2000U ? result::pending : fail("again-not-saved");
			}
			if (draft->Id == probe.Draft || turnHolds(probe.AgentKey) != std::optional{ draft->Id }) {
				return fail("again-not-a-fresh-draft");
			}
			probe.Again = draft->Id;
			// Then a second turn's draft on a line GitHub's diff holds, which a
			// review can carry; it leaves the review from Finish your review.
			const auto lines = implementation->shown_github_diff_lines();
			if (!lines || !*lines) {
				return fail("github-diff-unknown");
			}
			if (cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			for (auto candidate = std::uint32_t{ 0U }; candidate < 96U && !probe.ReviewLine; ++candidate) {
				if (candidate != *probe.Line && core::place_on_github_diff(*lines, core::diff_side::right, candidate, std::nullopt) != core::github_diff_placement::outside_diff
					&& window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate) && model->rowCount() == 0) {
					probe.ReviewLine = candidate;
				}
			}
			if (!probe.ReviewLine) {
				return fail("no-review-line");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_line;
			return result::pending;
		}
		case state_type::phase::awaiting_review_line: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " reviewLine=" + std::to_string(*probe.ReviewLine));
			if (!ready() || !draft || draft->ReplyToThread || draft->Anchor.Line != *probe.ReviewLine || draft->State != core::review_draft_state::drafting
				|| !window->comment_action()->isEnabled()) {
				return ++probe.Waited < 2000U ? result::pending : fail("review-line-draft");
			}
	#if defined(BPR_AGENT_INTEGRATIONS)
			const auto target = implementation->decide_answer_draft();
			probe.ReviewKey   = target.Key;
			probe.AgentAsked  = false;
			implementation->draft_agent_answer(target.Key, QString::fromStdString(words[3]), [&probe](const bool aDrafted, const QString& aWhy) {
				probe.AgentAsked = true;
				if (!aDrafted) {
					probe.Diagnostic += " | agent-review-refused=" + aWhy.toStdString();
				}
			});
	#endif
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_saved;
			return result::pending;
		}
		case state_type::phase::awaiting_review_saved: {
			const auto& draft = implementation->ReviewDraft;
			note(facts() + " review=" + std::to_string(stored(words[3]) ? 1 : 0));
			if (!ready() || !probe.AgentAsked || !draft || draft->Body != words[3] || !stored(words[3])) {
				return ++probe.Waited < 2000U ? result::pending : fail("review-not-saved");
			}
			if (turnHolds(probe.ReviewKey) != std::optional{ draft->Id }) {
				return fail("review-turn-not-on-its-draft");
			}
			probe.Draft = draft->Id;
			// Start a review takes it into a review.
			if (!cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			auto* const open   = cards->open_card();
			auto* const review = open != nullptr ? open->findChild<QAbstractButton*>(QStringLiteral("threadReviewDraft")) : nullptr;
			if (review == nullptr || !review->isEnabled()) {
				return ++probe.Waited < 2000U ? result::pending : fail("no-start-a-review");
			}
			review->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_pending;
			return result::pending;
		}
		case state_type::phase::awaiting_review_pending: {
			const auto all     = saved();
			const auto pending = std::ranges::any_of(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Id == probe.Draft && core::pending_in_review(aDraft.Draft); });
			note(facts() + " pending=" + std::to_string(pending ? 1 : 0));
			if (!ready() || !pending || cards->writing_draft()) {
				return ++probe.Waited < 2000U ? result::pending : fail("review-not-pending");
			}
			if (!window->review_sheet_open()) {
				window->finish_review_action()->trigger();
				QCoreApplication::processEvents();
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_finish;
			return result::pending;
		}
		case state_type::phase::awaiting_review_finish: {
			auto* const summary = window->review_submission_editor_view();
			auto        visible = std::vector<QToolButton*>{ };
			for (auto* const button : summary->pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"))) {
				if (button->isVisibleTo(window)) {
					visible.push_back(button);
				}
			}
			note(facts() + " sheet=" + std::to_string(window->review_sheet_open() ? 1 : 0) + " rows=" + std::to_string(visible.size()));
			if (!window->review_sheet_open() || visible.size() != 1U) {
				return ++probe.Waited < 2000U ? result::pending : fail("finish-review-row");
			}
			visible.front()->click();
			QCoreApplication::processEvents();
			auto* const question = summary->delete_confirmation();
			auto* const yes      = question != nullptr ? question->findChild<QAbstractButton*>(QStringLiteral("deleteCommentConfirmationConfirm")) : nullptr;
			if (yes == nullptr) {
				return fail("row-delete-asked-nothing");
			}
			yes->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_deleted;
			return result::pending;
		}
		case state_type::phase::awaiting_review_deleted: {
			note(facts() + " review=" + std::to_string(stored(words[3]) ? 1 : 0) + " held=" + std::to_string(turnHolds(probe.ReviewKey) ? 1 : 0));
			if (!ready() || stored(words[3]) || shown(words[3]) || turnHolds(probe.ReviewKey).has_value()) {
				return ++probe.Waited < 400U ? result::pending : fail("review-comment-not-let-go");
			}
			// The first turn's second draft, never deleted, is kept.
			if (!stored(again) || turnHolds(probe.AgentKey) != std::optional{ probe.Again }) {
				return fail("kept-draft-lost");
			}
			probe.Evidence.Fresh = true;
			probe.Phase          = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed: return result::passed;
		case state_type::phase::failed: return result::failed;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::draft_delete_evidence(const github_application& aApplication) -> detail::draft_delete_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->DraftDeleteProbe.has_value() ? implementation->DraftDeleteProbe->Evidence : detail::draft_delete_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::draft_delete_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->DraftDeleteProbe.has_value() ? implementation->DraftDeleteProbe->Diagnostic : std::string{ };
	}

	auto detail::github_application_fixture_access::exercise_start_review(
		github_application& aApplication, const detail::start_review_course aCourse) -> detail::start_review_probe_result
	{
		using result               = detail::start_review_probe_result;
		using course               = detail::start_review_course;
		using state_type           = start_review_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->StartReviewProbe.has_value()) {
			implementation->StartReviewProbe.emplace();
			// [[BPR-1735]]. This course's lines lie outside GitHub's diff of its
			// fixture; its subject is the review, so it places as a build before
			// the hunk rule did. The BPR-1735 courses cover placement.
			implementation->WholeDiffPlacementDisabled = true;
		}
		auto& probe = *implementation->StartReviewProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewMutationHost == nullptr || !implementation->ReviewSubmissionServer) {
			return result::pending;
		}
		auto* const window    = &*implementation->Window;
		auto* const cards     = window->conversation_panel() != nullptr ? window->conversation_panel()->cards() : nullptr;
		auto* const editor    = window->review_editor();
		auto* const model     = window->conversation_model();
		auto* const summary   = window->review_submission_editor_view();
		auto* const chip      = static_cast<qt::review_progress_chip*>(window->review_chip());
		auto* const chipShown = window->review_chip_action();
		if (cards == nullptr || editor == nullptr || model == nullptr || summary == nullptr || chip == nullptr || chipShown == nullptr) {
			return result::pending;
		}
		auto&      server = *implementation->ReviewSubmissionServer;
		const auto fail   = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " | phase=" + std::to_string(static_cast<int>(probe.Phase)) + " failed=" + std::string{ aWhy };
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto note = [&probe](std::string aFacts) {
			const auto key   = "phase=" + std::to_string(static_cast<int>(probe.Phase)) + " ";
			const auto start = probe.Diagnostic.find(key);
			if (start == std::string::npos) {
				probe.Diagnostic += (probe.Diagnostic.empty() ? "" : " | ") + key + std::move(aFacts);
				return;
			}
			const auto end = probe.Diagnostic.find(" | ", start);
			probe.Diagnostic.replace(start, end == std::string::npos ? std::string::npos : end - start, key + std::move(aFacts));
		};
		constexpr auto path  = std::string_view{ "src/engine/renderer.cpp" };
		constexpr auto other = std::string_view{ "src/engine/renderer.h" };
		// [[BPR-1541]]. The lines the page shows for an unreachable list and
		// for a server error.
		const auto unknownLine        = [](const core::review_submission_unknown_reason aReason) { return qt::review_submission_unknown_text(core::review_submission_unknown_notice{ .Reason = aReason }); };
		const auto unknownServerError = qt::review_submission_unknown_text(core::review_submission_unknown_notice{ .Reason = core::review_submission_unknown_reason::http_status, .StatusClass = 5U });
		const auto words              = std::array{
			QStringLiteral("question: Where does this value come from? (BPR-1345 first)"),
			QStringLiteral("nit: these two lines could be one. (BPR-1345 range)"),
			QStringLiteral("BPR-1345: this one goes on its own."),
		};
		// [[BPR-1345]] v002, `again`: the older draft and a summary that
		// is discarded.
		const auto older      = QStringLiteral("suggestion: an older draft, kept out of the review. (BPR-1345 again)");
		const auto summarised = QStringLiteral("A summary to discard. (BPR-1345 again)");
		const auto ready      = [&] {
			return window->github_refresh_action()->isEnabled() && window->github_status_text() == "Pull request ready."
				&& implementation->WorkbenchHost->outstanding_requests() == 0U && !implementation->PendingReviewMutation.has_value()
				&& !implementation->PrivateDraftCommandPending && !implementation->PrivateDraftSelectionPending;
		};
		const auto saved = [&] {
			return implementation->SharedDrafts ? implementation->SharedDrafts->recovery_snapshot() : std::vector<core::revisioned_review_draft>{ };
		};
		const auto pending = [&] {
			auto drafts = std::vector<core::revisioned_review_draft>{ };
			for (auto& draft : saved()) {
				if (core::pending_in_review(draft.Draft)) {
					drafts.push_back(std::move(draft));
				}
			}
			std::ranges::sort(drafts, { }, [](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line; });
			return drafts;
		};
		// Published drafts leave the saved list; each carried one is read by
		// its identity.
		const auto countPublished = [&] {
			return static_cast<std::size_t>(std::ranges::count_if(probe.Drafts, [&](const core::review_draft_id aDraft) {
				if (!implementation->SharedDrafts) {
					return false;
				}
				const auto read = implementation->SharedDrafts->read(implementation->DraftConsumer, aDraft);
				return read.Accepted && read.Accepted->Draft.State == core::review_draft_state::published && read.Accepted->Draft.InReview;
			}));
		};
		const auto countHeld = [&] {
			const auto all = saved();
			return static_cast<std::size_t>(std::ranges::count_if(all, [](const core::revisioned_review_draft& aDraft) { return core::held_by_review(aDraft.Draft); }));
		};
		const auto posts = [&server] {
			const auto lock = std::scoped_lock{ server.Mutex };
			return server.ReviewPosts;
		};
		const auto chipSays = [&](const std::size_t aCount) {
			return chipShown->isVisible() && chip->pending() == aCount && !chip->submitting()
				&& chip->summary() == QStringLiteral("Review · %1 pending").arg(aCount);
		};
		const auto composerButton = [&](const char* aName) -> QAbstractButton* {
			auto* const open = cards->open_card();
			return open != nullptr ? open->findChild<QAbstractButton*>(QString::fromLatin1(aName)) : nullptr;
		};
		// The first current-head line no thread or draft is on, chosen in Review.
		const auto pick = [&]() -> std::optional<std::uint32_t> {
			for (auto candidate = std::uint32_t{ 0U }; candidate < 64U; ++candidate) {
				if (std::ranges::find(probe.Lines, candidate) != probe.Lines.end() || probe.Third == std::optional{ candidate }
					|| (!probe.Lines.empty() && candidate == probe.Lines.front() + 1U)) {
					continue;
				}
				// A saved draft's line is passed over before it is selected, so
				// the selection does not move every tick (v002, `again`).
				if (std::ranges::none_of(saved(), [candidate](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == candidate && !aDraft.Draft.Body.empty(); })
					&& window->comparison_surface()->select_conversation_line(core::diff_side::right, candidate)
					&& window->conversation_panel()->anchor_line().has_value() && model->rowCount() == 0) {
					return candidate;
				}
			}
			return std::nullopt;
		};
		// Opens the selected line's draft in the drawer and writes its words.
		const auto write = [&](const QString& aWords) -> bool {
			const auto& draft = implementation->ReviewDraft;
			if (!draft || draft->ReplyToThread || !window->comment_action()->isEnabled()) {
				return false;
			}
			if (!cards->writing_draft()) {
				window->comment_action()->trigger();
				QCoreApplication::processEvents();
			}
			auto* const field = cards->draft_editor();
			if (field == nullptr) {
				return false;
			}
			if (field->toPlainText() != aWords) {
				field->setPlainText(aWords);
				QCoreApplication::processEvents();
			}
			return true;
		};
		const auto written = [&](const QString& aWords) {
			const auto& draft = implementation->ReviewDraft;
			return draft && draft->Body == aWords.toStdString() && draft->State == core::review_draft_state::drafting && ready();
		};
		// Each pending draft is a note at its line, marked pending, with its
		// gutter marker.
		const auto drawn = [&](const std::vector<core::revisioned_review_draft>& aPending) {
			return !aPending.empty() && std::ranges::all_of(aPending, [&](const core::revisioned_review_draft& aDraft) {
				const auto line = aDraft.Draft.Anchor.Line;
				const auto card = std::ranges::find_if(cards->cards(), [line](const qt::thread_card& aCard) { return aCard.Draft && aCard.Line == std::optional{ line + 1U }; });
				return card != cards->cards().end() && card->Pending && card->Placed
					&& std::ranges::any_of(window->comparison_surface()->line_marks(core::diff_side::right), [line](const qt::diff_line_mark& aMark) { return aMark.Line == line && aMark.Draft; });
			});
		};
		const auto finishPage = [&]() -> QWidget* {
			// [[BPR-1590]]. Finish your review is a sheet, or its expanded page.
			if (window->review_sheet_open()) {
				return window->review_sheet();
			}
			auto* const page = window->findChild<QScrollArea*>(QStringLiteral("composeReviewPage"));
			return page != nullptr && page->isVisibleTo(window) ? page : nullptr;
		};
		const auto openFinish = [&]() -> bool {
			if (finishPage() == nullptr) {
				chip->click();
				QCoreApplication::processEvents();
			}
			return finishPage() != nullptr;
		};
		const auto shown = [&](const std::size_t aCount) {
			return summary->title_label()->text() == QStringLiteral("Finish your review") && summary->pending_rows().size() == static_cast<qsizetype>(aCount)
				&& summary->carries_label()->isVisibleTo(window)
				&& summary->carries_label()->text() == (aCount == 1U ? QStringLiteral("1 pending comment") : QStringLiteral("%1 pending comments").arg(aCount));
		};
		// [[BPR-1345]] v002. A key in the drawer draft's field, as typed.
		const auto press = [&](const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers) -> bool {
			auto* const field = cards->draft_editor();
			if (field == nullptr) {
				return false;
			}
			auto down = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
			auto up   = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers };
			QCoreApplication::sendEvent(field, &down);
			QCoreApplication::sendEvent(field, &up);
			QCoreApplication::processEvents();
			return true;
		};
		// [[BPR-1345]] v002. With BPR_START_REVIEW_RENDER_DIRECTORY set, the
		// window as the owner will see it, in each theme, for the look.
		const auto renders = qEnvironmentVariable("BPR_START_REVIEW_RENDER_DIRECTORY");
		const auto render  = [&](const QString& aName, QWidget* aWidget = nullptr) {
			if (renders.isEmpty()) {
				return;
			}
			QDir{ }.mkpath(renders);
			static_cast<void>((aWidget != nullptr ? aWidget : static_cast<QWidget*>(window))->grab().save(renders + QLatin1Char{ '/' } + aName + QStringLiteral(".png"), "PNG"));
		};
		const auto renderThemes = [&](const QString& aName) {
			if (renders.isEmpty() || window->appearance_settings() == nullptr) {
				return;
			}
			const auto modes = std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") },
										   std::pair{ core::appearance_mode::high_contrast_light, QStringLiteral("high-contrast-light") },
										   std::pair{ core::appearance_mode::high_contrast_dark, QStringLiteral("high-contrast-dark") } };
			for (const auto& [mode, name] : modes) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				render(aName + QLatin1Char{ '-' } + name);
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		// [[BPR-1345]] v003. A question in the window's every theme, drawn
		// over the window it asks in, as the owner sees it.
		const auto renderQuestion = [&](const QString& aName, QWidget* const aQuestion) {
			if (renders.isEmpty() || aQuestion == nullptr || window->appearance_settings() == nullptr) {
				return;
			}
			QDir{ }.mkpath(renders);
			const auto modes = std::array{ std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") },
										   std::pair{ core::appearance_mode::high_contrast_light, QStringLiteral("high-contrast-light") },
										   std::pair{ core::appearance_mode::high_contrast_dark, QStringLiteral("high-contrast-dark") } };
			for (const auto& [mode, name] : modes) {
				window->appearance_settings()->choose_mode(mode);
				for (auto turn = 0; turn < 6; ++turn) {
					QCoreApplication::processEvents();
				}
				auto       scene    = window->grab();
				const auto question = aQuestion->grab();
				auto       painter  = QPainter{ &scene };
				painter.fillRect(scene.rect(), QColor{ 0, 0, 0, 90 });
				const auto ratio = scene.devicePixelRatio();
				const auto size  = question.deviceIndependentSize();
				painter.drawPixmap(QPointF{ (scene.width() / ratio - size.width()) / 2.0, (scene.height() / ratio - size.height()) / 2.0 }, question);
				painter.end();
				static_cast<void>(scene.save(renders + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + name + QStringLiteral(".png"), "PNG"));
				static_cast<void>(question.save(renders + QLatin1Char{ '/' } + aName + QLatin1Char{ '-' } + name + QStringLiteral("-alone.png"), "PNG"));
			}
			window->appearance_settings()->choose_mode(core::appearance_mode::light);
			for (auto turn = 0; turn < 6; ++turn) {
				QCoreApplication::processEvents();
			}
		};
		// [[BPR-1345]] v002. Presses a question's deleting button.
		const auto confirm = [](QWidget* const aBox, const char* aButton) -> bool {
			auto* const yes = aBox != nullptr ? aBox->findChild<QAbstractButton*>(QString::fromLatin1(aButton)) : nullptr;
			if (yes == nullptr) {
				return false;
			}
			yes->click();
			QCoreApplication::processEvents();
			return true;
		};
		// [[BPR-1345]] v002. The page as the owner asked for it: Comment
		// offered, the details closed, and what is published said once.
		// [[BPR-1853]]. The fixture's viewer, fixture-author, opened this pull
		// request, and GitHub takes only a comment from its author: Approve
		// and Request changes are off and each says why.
		const auto calm = [&] {
			auto* const comment = summary->disposition_button(core::review_disposition::comment);
			auto* const changes = summary->disposition_button(core::review_disposition::request_changes);
			auto* const approve = summary->disposition_button(core::review_disposition::approve);
			return comment->isEnabled() && !approve->isEnabled() && approve->toolTip().startsWith(QStringLiteral("You can't approve your own pull request")) && !changes->isEnabled()
				&& changes->toolTip().startsWith(QStringLiteral("You can't request changes on your own pull request"))
				&& !summary->details_toggle()->isChecked() && !summary->target_editor()->isVisibleTo(window)
				&& summary->note_label()->isVisibleTo(window) && summary->note_label()->text() == QStringLiteral("Nothing is published until you submit.");
		};
		const auto startOne = [&](const state_type::phase aAfter) -> result {
			switch (probe.Phase) {
			case state_type::phase::awaiting_first_line: {
				const auto line = pick();
				note("line=" + std::to_string(line.value_or(0U)) + " ready=" + std::to_string(ready() ? 1 : 0));
				if (!line || !ready() || !write(words[0])) {
					return result::pending;
				}
				probe.Lines        = { *line };
				probe.NativeBefore = implementation->Fixture->review_mutation_evidence().NativePosts;
				probe.Phase        = state_type::phase::awaiting_first_text;
				return result::pending;
			}
			case state_type::phase::awaiting_first_text:
				note("written=" + std::to_string(written(words[0]) ? 1 : 0));
				if (!written(words[0])) {
					return result::pending;
				}
				probe.Phase = state_type::phase::awaiting_first_offer;
				return result::pending;
			case state_type::phase::awaiting_first_offer: {
				auto* const single = composerButton("threadPublishDraft");
				auto* const review = composerButton("threadReviewDraft");
				note("single=" + (single ? single->text().toStdString() : std::string{ "-" }) + " review=" + (review ? review->text().toStdString() : std::string{ "-" }));
				if (single == nullptr || review == nullptr || !review->isEnabled()) {
					return result::pending;
				}
				if (single->text() != QStringLiteral("Add single comment") || review->text() != QStringLiteral("Start a review") || chipShown->isVisible()) {
					return fail("first-offer-not-start-a-review");
				}
				review->click();
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::awaiting_first_pending;
				return result::pending;
			}
			case state_type::phase::awaiting_first_pending: {
				const auto now = pending();
				note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString() + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
				if (now.size() != 1U || !chipSays(1U) || cards->writing_draft() || !ready() || !drawn(now)) {
					return result::pending;
				}
				const auto& mark       = now.front().Draft.InReview;
				probe.Evidence.Started = now.front().Draft.Anchor.Line == probe.Lines.front() && now.front().Draft.Body == words[0].toStdString()
					&& mark->Path == path && mark->Submission.empty() && implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore
					&& posts().empty();
				if (!probe.Evidence.Started) {
					return fail("first-not-pending-exactly");
				}
				probe.Drafts = { now.front().Draft.Id };
				probe.Phase  = aAfter;
				return result::pending;
			}
			default:
				return result::pending;
			}
		};
		// [[BPR-1590]] v002. The owner's look: real-looking pending comments,
		// each written on a free line through the drawer's own Start a review
		// and Add review comment, then a summary in Finish review.
		if (aCourse == course::showcase) {
			const auto showcase = std::array{
				QStringLiteral("issue (blocking): The retry budget resets every frame, so a view that keeps failing never stops retrying. Move the counter onto the view."),
				QStringLiteral("suggestion: Compute the scale once per frame and pass it in.\n```suggestion\n    const auto scale = device_scale(view);\n```"),
				QStringLiteral("question: Does the second view share the depth buffer with the first, or does each view own one?"),
				// [[BPR-1590]] v003. A very long comment, whose row stays one line.
				QStringLiteral("thought: The clear and the draw race on the second view.\n\nA fence between them would make the order explicit, and it costs one wait per frame on the views that need it. "
							   "The first view never sees the race because its clear is recorded on the same queue as its draw, but the second view's clear goes through the transfer queue when "
							   "the depth buffer is shared, and nothing orders that queue against the graphics queue.\n\nI tried three orders locally. Recording the clear on the graphics queue "
							   "fixes it but costs the overlap we wanted from the transfer queue in the first place; a timeline semaphore keeps the overlap and fixes it, at one wait per frame; "
							   "a pipeline barrier alone does not help, because the barrier only orders work within one queue.\n\nSo my vote is the timeline semaphore, with a comment at the "
							   "submit saying why the second view waits, since the next person to touch this will otherwise remove the wait as redundant. Happy to pair on it if that helps."),
				QStringLiteral("nitpick (non-blocking): `std::max` reads better than the ternary here."),
			};
			const auto index = probe.Lines.size();
			switch (probe.Phase) {
			case state_type::phase::awaiting_file: {
				const auto revealed = reveal_fixture_file(aApplication, path);
				if (revealed == detail::fixture_file_reveal_result::failed) {
					return fail("file-not-revealed");
				}
				if (revealed == detail::fixture_file_reveal_result::shown) {
					probe.Phase = state_type::phase::awaiting_first_line;
				}
				return result::pending;
			}
			case state_type::phase::awaiting_first_line: {
				if (index == showcase.size()) {
					probe.Phase = state_type::phase::awaiting_finish_page;
					return result::pending;
				}
				// The line is chosen once; selecting it again every tick would
				// keep its draft's selection pending.
				if (!probe.Third) {
					probe.Third = pick();
					return probe.Third || ++probe.Waited < 600U ? result::pending : fail("showcase-no-line");
				}
				note("line=" + std::to_string(*probe.Third) + " ready=" + std::to_string(ready() ? 1 : 0) + " comment=" + std::to_string(window->comment_action()->isEnabled() ? 1 : 0));
				if (!ready() || !write(showcase[index])) {
					return ++probe.Waited < 600U ? result::pending : fail("showcase-line");
				}
				const auto line = probe.Third;
				probe.Third.reset();
				probe.Lines.push_back(*line);
				probe.Phase = state_type::phase::awaiting_first_text;
				return result::pending;
			}
			case state_type::phase::awaiting_first_text:
				if (!written(showcase[index - 1U])) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-text");
				}
				probe.Phase = state_type::phase::awaiting_first_offer;
				return result::pending;
			case state_type::phase::awaiting_first_offer: {
				auto* const review = composerButton("threadReviewDraft");
				if (review == nullptr || !review->isEnabled()) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-offer");
				}
				review->click();
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::awaiting_first_pending;
				return result::pending;
			}
			case state_type::phase::awaiting_first_pending:
				if (pending().size() != index || cards->writing_draft() || !ready()) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-pending");
				}
				probe.Waited = 0U;
				probe.Phase  = state_type::phase::awaiting_first_line;
				return result::pending;
			case state_type::phase::awaiting_finish_page:
				if (!window->review_sheet_open()) {
					window->finish_review_action()->trigger();
					QCoreApplication::processEvents();
					return result::pending;
				}
				if (summary->pending_rows().size() != static_cast<qsizetype>(showcase.size()) || summary->body_editor()->isReadOnly()) {
					return ++probe.Waited < 2'000U ? result::pending : fail("showcase-finish");
				}
				summary->body_editor()->setPlainText(QStringLiteral("Nice cleanup overall. The retry budget on the second view needs fixing before this lands; everything else is small and can follow."));
				QCoreApplication::processEvents();
				probe.Phase = state_type::phase::passed;
				return result::passed;
			default:
				return fail("showcase-phase");
			}
		}
		switch (probe.Phase) {
		case state_type::phase::awaiting_file: {
			// [[BPR-1833]]. These courses keep working on the same pull request
			// after Comment. BPR-1816 defaults a confirmed Submit to Home, so
			// choose the existing stay option through Settings and wait for the
			// accepted projection. The other verdicts retain their defaults.
			if (aCourse == course::finish || aCourse == course::again) {
				auto* const comment = window->finish_review_switch(core::review_disposition::comment);
				if (comment == nullptr) {
					return fail("finish-comment-setting-missing");
				}
				const auto& choices = window->behaviour().FinishReview;
				if (choices.Approve != core::review_finish_return::home || choices.RequestChanges != core::review_finish_return::home) {
					return fail("other-finish-defaults-changed");
				}
				if (!probe.FinishReturnChosen) {
					if (comment->isChecked()) {
						comment->click();
					}
					probe.FinishReturnChosen = true;
					return result::pending;
				}
				if (choices.Comment != core::review_finish_return::pull_request || comment->isChecked()) {
					return result::pending;
				}
			}
			const auto revealed = reveal_fixture_file(aApplication, path);
			note("reveal=" + std::to_string(static_cast<int>(revealed)));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed");
			}
			if (revealed == detail::fixture_file_reveal_result::shown) {
				probe.Phase = aCourse == course::finish ? state_type::phase::awaiting_restored
					: aCourse == course::restored       ? state_type::phase::awaiting_restored_unknown
					: aCourse == course::again          ? state_type::phase::awaiting_old_line
														: state_type::phase::awaiting_first_line;
			}
			return result::pending;
		}
		case state_type::phase::awaiting_first_line:
		case state_type::phase::awaiting_first_text:
		case state_type::phase::awaiting_first_offer:
		case state_type::phase::awaiting_first_pending:
			return startOne(aCourse == course::pending || aCourse == course::again ? state_type::phase::awaiting_second_line : state_type::phase::awaiting_finish_page);
		case state_type::phase::awaiting_second_line: {
			// The range ends on the line below its first, which is free.
			auto line = pick();
			if (line && *line == 0U) {
				line = std::nullopt;
			}
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(words[1])) {
				return result::pending;
			}
			probe.Lines.push_back(*line);
			probe.Phase = state_type::phase::awaiting_second_text;
			return result::pending;
		}
		case state_type::phase::awaiting_second_text: {
			const auto& draft = implementation->ReviewDraft;
			note("written=" + std::to_string(written(words[1]) ? 1 : 0) + " start=" + (draft && draft->StartLine ? std::to_string(*draft->StartLine) : std::string{ "-" }));
			if (!written(words[1])) {
				return result::pending;
			}
			if (!draft->StartLine) {
				static_cast<void>(editor->change_range(probe.Lines.back() - 1U));
				return result::pending;
			}
			probe.Phase = state_type::phase::awaiting_second_offer;
			return result::pending;
		}
		case state_type::phase::awaiting_second_offer: {
			auto* const single = composerButton("threadPublishDraft");
			auto* const review = composerButton("threadReviewDraft");
			auto* const field  = cards->draft_editor();
			note("single=" + (single ? single->text().toStdString() : std::string{ "-" }) + " review=" + (review ? review->text().toStdString() : std::string{ "-" }));
			if (single == nullptr || review == nullptr || field == nullptr || !review->isEnabled()) {
				return result::pending;
			}
			if (single->text() != QStringLiteral("Add single comment") || review->text() != QStringLiteral("Add review comment") || !chipSays(1U)) {
				return fail("second-offer-not-add-review-comment");
			}
			if (aCourse == course::again) {
				renderThemes(QStringLiteral("drawer-add-review-comment"));
			}
			// ⌘↩ is the main action while a review is in progress.
			static_cast<void>(press(Qt::Key_Return, Qt::ControlModifier));
			probe.Phase = state_type::phase::awaiting_second_pending;
			return result::pending;
		}
		case state_type::phase::awaiting_second_pending: {
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString());
			if (now.size() != 2U || !chipSays(2U) || cards->writing_draft() || !ready()) {
				return result::pending;
			}
			const auto& range    = now.back().Draft;
			probe.Evidence.Added = range.Anchor.Line == probe.Lines.back() && range.StartLine == std::optional{ probe.Lines.back() - 1U }
				&& range.Body == words[1].toStdString() && implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore && posts().empty();
			if (!probe.Evidence.Added) {
				return fail("second-not-pending-exactly");
			}
			if (!drawn(now)) {
				return result::pending;
			}
			probe.Evidence.Drawn = true;
			probe.Phase          = aCourse == course::again ? state_type::phase::awaiting_finish_again : state_type::phase::awaiting_third_line;
			return result::pending;
		}
		case state_type::phase::awaiting_third_line: {
			const auto line = pick();
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(words[2])) {
				return result::pending;
			}
			probe.Third = line;
			probe.Phase = state_type::phase::awaiting_third_text;
			return result::pending;
		}
		case state_type::phase::awaiting_third_text: {
			auto* const single = composerButton("threadPublishDraft");
			note("written=" + std::to_string(written(words[2]) ? 1 : 0));
			if (!written(words[2]) || single == nullptr || !single->isEnabled()) {
				return result::pending;
			}
			// v002: ⌘⇧↩ is Add single comment.
			static_cast<void>(press(Qt::Key_Return, Qt::ControlModifier | Qt::ShiftModifier));
			probe.Phase = state_type::phase::awaiting_third_published;
			return result::pending;
		}
		case state_type::phase::awaiting_third_published: {
			const auto mutations = implementation->Fixture->review_mutation_evidence();
			note("native=" + std::to_string(mutations.NativePosts) + " pending=" + std::to_string(pending().size()));
			if (implementation->PendingReviewMutation.has_value() || mutations.NativePosts == probe.NativeBefore || !ready()) {
				return result::pending;
			}
			probe.Evidence.SingleAlone = mutations.NativePosts == probe.NativeBefore + 1U && mutations.LastPath == path && probe.Third
				&& mutations.LastLine == *probe.Third + 1U && mutations.LastBody == words[2].toStdString() && pending().size() == 2U && chipSays(2U) && posts().empty();
			if (!probe.Evidence.SingleAlone) {
				return fail("single-comment-not-alone");
			}
			probe.Phase = state_type::phase::awaiting_other_file;
			return result::pending;
		}
		case state_type::phase::awaiting_other_file: {
			const auto revealed = reveal_fixture_file(aApplication, other);
			note("reveal=" + std::to_string(static_cast<int>(revealed)) + " chip=" + chip->summary().toStdString());
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("other-file-not-revealed");
			}
			if (revealed != detail::fixture_file_reveal_result::shown || !ready()) {
				return result::pending;
			}
			if (!chipSays(2U)) {
				return fail("other-file-lost-the-review");
			}
			probe.Phase = state_type::phase::awaiting_return;
			return result::pending;
		}
		case state_type::phase::awaiting_return: {
			const auto revealed = reveal_fixture_file(aApplication, path);
			const auto now      = pending();
			note("reveal=" + std::to_string(static_cast<int>(revealed)) + " pending=" + std::to_string(now.size()));
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("file-not-revealed-again");
			}
			if (revealed != detail::fixture_file_reveal_result::shown || !ready() || !drawn(now)) {
				return result::pending;
			}
			probe.Evidence.NavigationKept = now.size() == 2U && chipSays(2U) && posts().empty()
				&& implementation->Fixture->review_mutation_evidence().NativePosts == probe.NativeBefore + 1U;
			if (!probe.Evidence.NavigationKept) {
				return fail("navigation-changed-the-review");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_restored: {
			// After the restart: the two the pending course left.
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString() + " ready=" + std::to_string(ready() ? 1 : 0));
			if (now.size() != 2U || !chipSays(2U) || !ready() || !drawn(now)) {
				return ++probe.Waited < 400U ? result::pending : fail("not-restored");
			}
			probe.Evidence.Restored = now.front().Draft.Body == words[0].toStdString() && now.back().Draft.Body == words[1].toStdString()
				&& now.back().Draft.StartLine == std::optional{ now.back().Draft.Anchor.Line - 1U } && posts().empty();
			if (!probe.Evidence.Restored) {
				return fail("restored-not-exact");
			}
			for (const auto& draft : now) {
				probe.Lines.push_back(draft.Draft.Anchor.Line);
				probe.Drafts.push_back(draft.Draft.Id);
			}
			probe.Phase = state_type::phase::awaiting_finish_page;
			return result::pending;
		}
		case state_type::phase::awaiting_finish_page: {
			const auto expected = aCourse == course::finish ? 2U : 1U;
			const auto opened   = openFinish();
			note("opened=" + std::to_string(opened ? 1 : 0) + " title=" + summary->title_label()->text().toStdString() + " rows=" + std::to_string(summary->pending_rows().size())
				 + " comment=" + std::to_string(summary->disposition_button(core::review_disposition::comment)->isEnabled() ? 1 : 0) + " status=" + summary->status_label()->text().toStdString());
			if (!opened || !shown(expected) || !summary->disposition_button(core::review_disposition::comment)->isEnabled()) {
				return result::pending;
			}
			// [[BPR-1590]]. A sheet over the surface it was opened from.
			probe.Evidence.FinishOpened = window->review_sheet_open();
			// Comment, with no summary, is one of the three buttons that
			// submit (v002).
			probe.Evidence.CommentWithoutSummary = summary->shown_draft().has_value() && summary->body_editor()->toPlainText().isEmpty()
				&& summary->body_editor()->placeholderText() == QStringLiteral("Leave a summary (optional)") && calm();
			if (!probe.Evidence.FinishOpened || !probe.Evidence.CommentWithoutSummary) {
				return fail("finish-page-not-as-drawn");
			}
			{
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.RefuseNextReview = aCourse == course::refused;
				server.LoseNextReview   = aCourse == course::uncertain || aCourse == course::kept;
			}
			// [[BPR-1475]]. What the submission's own reread is measured from.
			probe.ReadsBefore     = implementation->Fixture->evidence().AccountReadCount;
			probe.ShownBefore     = implementation->WindowRead->accepted_snapshot() ? std::optional{ implementation->WindowRead->accepted_snapshot()->Snapshot } : std::nullopt;
			probe.SelectionBefore = window->comparison_surface()->selection();
			summary->disposition_button(core::review_disposition::comment)->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = aCourse == course::refused ? state_type::phase::awaiting_refused : aCourse == course::uncertain || aCourse == course::kept ? state_type::phase::awaiting_held
																																					  : state_type::phase::awaiting_submitted;
			return result::pending;
		}
		case state_type::phase::awaiting_submitted: {
			const auto sent      = posts();
			const auto remaining = pending();
			const auto published = countPublished();
			note("posts=" + std::to_string(sent.size()) + " published=" + std::to_string(published) + " pending=" + std::to_string(remaining.size()) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString());
			if (sent.size() != 1U || published != 2U || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-submitted");
			}
			// The one request as the server received it.
			const auto object             = QJsonDocument::fromJson(QByteArray::fromStdString(sent.front())).object();
			const auto comments           = object.value(QStringLiteral("comments")).toArray();
			const auto line               = [](const std::uint32_t aLine) { return static_cast<int>(aLine) + 1; };
			const auto first              = comments.size() == 2 ? comments.at(0).toObject() : QJsonObject{ };
			const auto second             = comments.size() == 2 ? comments.at(1).toObject() : QJsonObject{ };
			probe.Evidence.SubmittedExact = object.value(QStringLiteral("event")).toString() == QStringLiteral("COMMENT") && object.value(QStringLiteral("body")).toString().isEmpty()
				&& comments.size() == 2 && probe.Lines.size() == 2U
				&& first.value(QStringLiteral("path")).toString() == QString::fromLatin1(path.data(), static_cast<qsizetype>(path.size()))
				&& first.value(QStringLiteral("line")).toInt() == line(probe.Lines[0]) && first.value(QStringLiteral("side")).toString() == QStringLiteral("RIGHT")
				&& !first.contains(QStringLiteral("start_line")) && first.value(QStringLiteral("body")).toString() == words[0]
				&& second.value(QStringLiteral("path")).toString() == QString::fromLatin1(path.data(), static_cast<qsizetype>(path.size()))
				&& second.value(QStringLiteral("line")).toInt() == line(probe.Lines[1]) && second.value(QStringLiteral("side")).toString() == QStringLiteral("RIGHT")
				&& second.value(QStringLiteral("start_line")).toInt() == line(probe.Lines[1] - 1U) && second.value(QStringLiteral("start_side")).toString() == QStringLiteral("RIGHT")
				&& second.value(QStringLiteral("body")).toString() == words[1];
			// [[BPR-1345]] v003. Confirmed, the page closes and the window
			// returns to Review, where Finish review was pressed, and says so.
			probe.Evidence.MarkedPublished = remaining.empty() && finishPage() == nullptr && window->current_modality() == qt::review_modality::review
				&& window->statusBar()->currentMessage() == QStringLiteral("Review submitted with 2 comments") && summary->confirmed_submissions() == 1U
				&& summary->shown_draft().has_value() && summary->shown_draft()->State == core::review_submission_state::drafting && summary->shown_draft()->Comments.empty();
			if (!probe.Evidence.SubmittedExact || !probe.Evidence.MarkedPublished) {
				note("modality=" + std::to_string(static_cast<int>(window->current_modality())) + " message=" + window->statusBar()->currentMessage().toStdString() + " confirmed=" + std::to_string(summary->confirmed_submissions()) + " finish=" + std::to_string(finishPage() != nullptr ? 1 : 0));
				return fail("submitted-not-exact");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_publication_reread;
			return result::pending;
		}
		case state_type::phase::awaiting_publication_reread: {
			// [[BPR-1475]]. Nothing here presses Refresh: the submission's own
			// reread brings a new snapshot, and the reviewer's place stays.
			const auto& snapshot  = implementation->WindowRead->accepted_snapshot();
			const auto  reads     = implementation->Fixture->evidence().AccountReadCount;
			const auto& context   = window->comparison_surface()->current_context();
			const auto  shownFile = context.has_value() && context->Key.File == core::file_identity_of(path);
			note("reads=" + std::to_string(reads) + " before=" + std::to_string(probe.ReadsBefore) + " ready=" + std::to_string(ready() ? 1 : 0) + " file=" + std::to_string(shownFile ? 1 : 0)
				 + " selection=" + std::to_string(window->comparison_surface()->selection() == probe.SelectionBefore ? 1 : 0));
			if (reads == probe.ReadsBefore || !snapshot || std::optional{ snapshot->Snapshot } == probe.ShownBefore || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-reread-after-publish");
			}
			probe.Evidence.RereadAfterPublish = !window->showing_home() && window->current_modality() == qt::review_modality::review && shownFile
				&& probe.SelectionBefore.has_value() && window->comparison_surface()->selection() == probe.SelectionBefore && finishPage() == nullptr;
			if (!probe.Evidence.RereadAfterPublish) {
				return fail("reread-moved-the-reviewer");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_note_line;
			return result::pending;
		}
		case state_type::phase::awaiting_note_line: {
			// A new note, written and left open in the drawer.
			const auto line = pick();
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(words[2])) {
				return ++probe.Waited < 400U ? result::pending : fail("no-note-to-hold-open");
			}
			probe.Third  = line;
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_note_text;
			return result::pending;
		}
		case state_type::phase::awaiting_note_text: {
			note("written=" + std::to_string(written(words[2]) ? 1 : 0) + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			if (!written(words[2]) || !cards->writing_draft()) {
				return ++probe.Waited < 400U ? result::pending : fail("note-not-written");
			}
			// The reread a publication owes, by the same path, with the note
			// open: this window has nothing else to publish here.
			probe.ReadsBefore     = implementation->Fixture->evidence().AccountReadCount;
			probe.ShownBefore     = implementation->WindowRead->accepted_snapshot()->Snapshot;
			probe.SelectionBefore = window->comparison_surface()->selection();
			implementation->reread_after_publication();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_note_reread;
			return result::pending;
		}
		case state_type::phase::awaiting_note_reread: {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			const auto  reads    = implementation->Fixture->evidence().AccountReadCount;
			auto* const field    = cards->draft_editor();
			note("reads=" + std::to_string(reads) + " before=" + std::to_string(probe.ReadsBefore) + " ready=" + std::to_string(ready() ? 1 : 0)
				 + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0) + " field=" + std::to_string(field != nullptr ? 1 : 0) + " written=" + std::to_string(written(words[2]) ? 1 : 0)
				 + " open=" + std::to_string(cards->open_card() != nullptr ? 1 : 0) + " selection=" + std::to_string(window->comparison_surface()->selection() == probe.SelectionBefore ? 1 : 0)
				 + " modality=" + std::to_string(static_cast<int>(window->current_modality())) + " draft=" + (implementation->ReviewDraft ? std::to_string(implementation->ReviewDraft->Id.value()) + ":" + implementation->ReviewDraft->Body.substr(0, 8) : std::string{ "none" })
				 + " resume=" + std::to_string(implementation->ResumeComment ? 1 : 0) + " waited=" + std::to_string(probe.Waited));
			if (reads == probe.ReadsBefore || !snapshot || std::optional{ snapshot->Snapshot } == probe.ShownBefore || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-reread-with-the-note-open");
			}
			probe.Evidence.NoteKept = cards->writing_draft() && field != nullptr && field->toPlainText() == words[2] && written(words[2])
				&& window->current_modality() == qt::review_modality::review && window->comparison_surface()->selection() == probe.SelectionBefore;
			if (!probe.Evidence.NoteKept) {
				return ++probe.Waited < 400U ? result::pending : fail("reread-closed-the-note");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_recent_open;
			return result::pending;
		}
		case state_type::phase::awaiting_recent_open: {
			// [[BPR-1472]]. The owner's course: Home, then this pull request's
			// Recently viewed row. The shown read predates the review just
			// submitted, so the row reads the pull request again.
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			auto* const recent   = window->home_recent_list();
			if (!snapshot || recent == nullptr) {
				return fail("nothing-shown-to-reopen");
			}
			const auto name = QStringLiteral("%1/%2#%3").arg(QString::fromStdString(snapshot->Repository.Owner), QString::fromStdString(snapshot->Repository.Name)).arg(snapshot->PullRequest.Number.value());
			window->choose_home(true);
			QCoreApplication::processEvents();
			auto row = -1;
			for (auto index = 0; index < recent->count(); ++index) {
				if (recent->item(index)->text().startsWith(name)) {
					row = index;
				}
			}
			note("recent=" + std::to_string(recent->count()) + " row=" + std::to_string(row) + " visible=" + std::to_string(recent->isVisible() ? 1 : 0));
			if (row < 0 || !recent->isVisible()) {
				return ++probe.Waited < 400U ? result::pending : fail("no-recent-row");
			}
			probe.ReadsBefore = implementation->Fixture->evidence().AccountReadCount;
			probe.ShownBefore = snapshot->Snapshot;
			recent->setCurrentRow(row);
			recent->setFocus(Qt::OtherFocusReason);
			auto open = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QCoreApplication::sendEvent(recent, &open);
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_recent_read;
			return result::pending;
		}
		case state_type::phase::awaiting_recent_read: {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			const auto  reads    = implementation->Fixture->evidence().AccountReadCount;
			note("reads=" + std::to_string(reads) + " before=" + std::to_string(probe.ReadsBefore) + " ready=" + std::to_string(ready() ? 1 : 0)
				 + " home=" + std::to_string(window->showing_home() ? 1 : 0));
			if (reads == probe.ReadsBefore || !snapshot || snapshot->Snapshot == probe.ShownBefore || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("reopen-reused-the-stale-read");
			}
			probe.Evidence.ReopenedFresh = !window->showing_home() && window->current_modality() == qt::review_modality::overview;
			if (!probe.Evidence.ReopenedFresh) {
				return fail("reopen-not-on-overview");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_refused: {
			const auto sent = posts();
			const auto now  = pending();
			note("posts=" + std::to_string(sent.size()) + " pending=" + std::to_string(now.size()) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString());
			if (sent.size() != 1U || now.size() != 1U || countHeld() != 0U || !chipSays(1U) || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("refusal-not-settled");
			}
			// [[BPR-1345]] v003. A refusal keeps the page open, with its words.
			// [[BPR-1715]]. GitHub's refusal of the review is said as one.
			probe.Evidence.RefusedKept = now.front().Draft.Body == words[0].toStdString() && now.front().Draft.Anchor.Line == probe.Lines.front()
				&& now.front().Draft.InReview->Submission.empty() && finishPage() != nullptr && summary->confirmed_submissions() == 0U
				&& summary->status_label()->text().startsWith(QStringLiteral("Not published: GitHub refused this review."));
			if (!probe.Evidence.RefusedKept) {
				return fail("refusal-lost-the-draft");
			}
			if (!summary->discard_action()->isEnabled() || summary->discard_action()->text() != QStringLiteral("Discard review")) {
				return result::pending;
			}
			// v002: Discard review asks first, naming what it deletes.
			summary->discard_action()->trigger();
			QCoreApplication::processEvents();
			if (summary->discard_confirmation() == nullptr || !summary->discard_confirmation()->question().contains(QStringLiteral("1 pending comment"))) {
				return fail("discard-asked-nothing");
			}
			if (!confirm(summary->discard_confirmation(), "discardReviewConfirmationConfirm")) {
				return fail("discard-not-confirmed");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_discarded: {
			const auto all  = saved();
			const auto kept = std::ranges::find_if(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == probe.Lines.front(); });
			note("pending=" + std::to_string(pending().size()) + " chip=" + std::to_string(chipShown->isVisible() ? 1 : 0) + " page=" + std::to_string(finishPage() != nullptr ? 1 : 0));
			if (!pending().empty() || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("discard-not-settled");
			}
			// v002: deleted, not kept, and the page closed.
			probe.Evidence.DiscardDeleted = kept == all.end() && finishPage() == nullptr && posts().size() == 1U;
			if (!probe.Evidence.DiscardDeleted) {
				return fail("discard-kept-the-draft");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_held: {
			const auto all  = saved();
			const auto held = std::ranges::find_if(all, [](const core::revisioned_review_draft& aDraft) { return core::held_by_review(aDraft.Draft); });
			note("posts=" + std::to_string(posts().size()) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString()
				 + " reconcile=" + std::to_string(summary->reconcile_action()->isEnabled() ? 1 : 0));
			if (held == all.end() || !summary->reconcile_action()->isEnabled() || !chipShown->isVisible() || !chip->submitting()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-held");
			}
			probe.Evidence.UncertainHeld = posts().size() == 1U && held->Draft.Body == words[0].toStdString() && pending().empty()
				&& !editor->publication_retry_offered() && finishPage() != nullptr && summary->confirmed_submissions() == 0U
				&& summary->status_label()->text().startsWith(QStringLiteral("Review status is unknown."));
			if (!probe.Evidence.UncertainHeld) {
				return fail("uncertain-not-held-exactly");
			}
			// [[BPR-1541]]. The list could not be reached: the page says so
			// under the status, as part of its description, naming nothing.
			{
				const auto line            = summary->unknown_reason_label()->text();
				probe.Evidence.ReasonShown = !summary->unknown_reason_label()->isHidden() && line == unknownLine(core::review_submission_unknown_reason::network)
					&& summary->status_label()->accessibleDescription() == line && !line.contains(QStringLiteral("conventional")) && !line.contains(QStringLiteral("renderer"));
				render(QStringLiteral("unknown-reason"), summary);
				// [[BPR-1541]] v002. The held review in the window, in each theme.
				renderThemes(QStringLiteral("unknown-status"));
				if (!probe.Evidence.ReasonShown) {
					return fail("unknown-reason-not-shown");
				}
			}
			if (aCourse == course::kept) {
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}
			{
				// Check status again: the list answers with a server error.
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.ReviewReadStatus = 503;
			}
			summary->reconcile_action()->trigger();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_replaced;
			return result::pending;
		}
		case state_type::phase::awaiting_replaced: {
			const auto line = summary->unknown_reason_label()->text();
			note("reason=" + line.toStdString() + " reconcile=" + std::to_string(summary->reconcile_action()->isEnabled() ? 1 : 0));
			if (line != unknownServerError || !summary->reconcile_action()->isEnabled()) {
				return ++probe.Waited < 400U ? result::pending : fail("unknown-reason-not-replaced");
			}
			probe.Evidence.ReasonReplaced = posts().size() == 1U && summary->status_label()->accessibleDescription() == line;
			if (!probe.Evidence.ReasonReplaced) {
				return fail("unknown-reason-replaced-wrongly");
			}
			{
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.HideReviewReads  = false;
				server.ReviewReadStatus = 0;
			}
			summary->reconcile_action()->trigger();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_reconciled;
			return result::pending;
		}
		case state_type::phase::awaiting_restored_unknown: {
			// [[BPR-1541]]. Restarted over the kept root, with the review
			// list's reads held: before Check status answers, the restored
			// review already says what its record kept.
			const auto line = summary->unknown_reason_label()->text();
			note("reason=" + line.toStdString() + " status=" + summary->status_label()->text().toStdString());
			if (line.isEmpty() || summary->unknown_reason_label()->isHidden()) {
				return ++probe.Waited < 400U ? result::pending : fail("restored-reason-not-shown");
			}
			probe.Evidence.ReasonRestored = line == unknownLine(core::review_submission_unknown_reason::network) && summary->shown_draft()
				&& summary->shown_draft()->State == core::review_submission_state::submitting && summary->status_label()->text().startsWith(QStringLiteral("Checking the original review"));
			if (!probe.Evidence.ReasonRestored) {
				return fail("restored-reason-not-the-kept-one");
			}
			{
				const auto lock         = std::scoped_lock{ server.Mutex };
				server.HideReviewReads  = true;
				server.ReviewReadStatus = 503;
				server.StallReviewReads = false;
			}
			server.ReadGate.notify_all();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_restored_replaced;
			return result::pending;
		}
		case state_type::phase::awaiting_restored_replaced: {
			const auto line = summary->unknown_reason_label()->text();
			note("reason=" + line.toStdString() + " status=" + summary->status_label()->text().toStdString());
			if (line != unknownServerError) {
				return ++probe.Waited < 400U ? result::pending : fail("restored-reason-not-replaced");
			}
			probe.Evidence.ReasonReplaced = summary->status_label()->text().startsWith(QStringLiteral("Review status is unknown."));
			if (!probe.Evidence.ReasonReplaced) {
				return fail("restored-reason-replaced-wrongly");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_reconciled: {
			const auto published = countPublished();
			note("posts=" + std::to_string(posts().size()) + " published=" + std::to_string(published) + " held=" + std::to_string(countHeld()) + " status=" + summary->status_label()->text().toStdString());
			if (published != 1U || countHeld() != 0U || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("not-reconciled");
			}
			// Check status found it published: the page closes as on any
			// confirmation.
			probe.Evidence.Reconciled = posts().size() == 1U && finishPage() == nullptr && summary->confirmed_submissions() == 1U
				&& window->statusBar()->currentMessage() == QStringLiteral("Review submitted with 1 comment");
			if (!probe.Evidence.Reconciled) {
				return fail("reconciled-after-a-second-post");
			}
			// [[BPR-1541]]. Confirmed: no reason is shown.
			probe.Evidence.ReasonCleared = summary->unknown_reason_label()->isHidden() && summary->unknown_reason_label()->text().isEmpty();
			if (!probe.Evidence.ReasonCleared) {
				return fail("reason-kept-after-confirmation");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::awaiting_old_line: {
			// The older draft, written first and kept out of the review.
			const auto line = pick();
			note("line=" + std::to_string(line.value_or(0U)));
			if (!line || !ready() || !write(older)) {
				return result::pending;
			}
			probe.Old   = line;
			probe.Phase = state_type::phase::awaiting_old_text;
			return result::pending;
		}
		case state_type::phase::awaiting_old_text:
			note("written=" + std::to_string(written(older) ? 1 : 0));
			if (!written(older)) {
				return result::pending;
			}
			// Esc keeps a written draft at its line.
			static_cast<void>(press(Qt::Key_Escape, Qt::NoModifier));
			probe.Phase = state_type::phase::awaiting_old_kept;
			return result::pending;
		case state_type::phase::awaiting_old_kept: {
			const auto all  = saved();
			const auto kept = std::ranges::find_if(all, [&](const core::revisioned_review_draft& aDraft) { return probe.Old && aDraft.Draft.Anchor.Line == *probe.Old; });
			note("kept=" + std::to_string(kept != all.end() ? 1 : 0) + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			if (kept == all.end() || cards->writing_draft() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("older-draft-not-kept");
			}
			if (kept->Draft.Body != older.toStdString() || core::pending_in_review(kept->Draft)) {
				return fail("older-draft-not-as-written");
			}
			probe.OldDraft = kept->Draft.Id;
			probe.Waited   = 0U;
			probe.Phase    = state_type::phase::awaiting_first_line;
			return result::pending;
		}
		case state_type::phase::awaiting_finish_again: {
			// Finish review by its window action, ⌘⌥↩'s.
			auto* const finish = window->finish_review_action();
			note("finish=" + std::to_string(finish != nullptr && finish->isEnabled() ? 1 : 0) + " rows=" + std::to_string(summary->pending_rows().size()));
			if (finish == nullptr || !finish->isEnabled()) {
				return result::pending;
			}
			if (finishPage() == nullptr) {
				finish->trigger();
				QCoreApplication::processEvents();
			}
			if (finishPage() == nullptr || !shown(2U)) {
				return result::pending;
			}
			probe.Evidence.FinishOpened = window->review_sheet_open() && calm();
			if (!probe.Evidence.FinishOpened) {
				return fail("finish-again-not-as-drawn");
			}
			renderThemes(QStringLiteral("finish-your-review"));
			if (!probe.RowRevealed) {
				// A row goes to its comment: the second, the range.
				auto rows = std::vector<QAbstractButton*>{ };
				for (auto* const row : summary->pending_list()->findChildren<QAbstractButton*>(QStringLiteral("wholeReviewPendingOpen"))) {
					if (row->isVisibleTo(window)) {
						rows.push_back(row);
					}
				}
				if (rows.size() != 2U) {
					return fail("rows-not-two");
				}
				// [[BPR-1590]] v003. Threads' gesture: Return opens it in Review.
				rows.back()->setFocus(Qt::OtherFocusReason);
				auto open = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
				QCoreApplication::sendEvent(rows.back(), &open);
				QCoreApplication::processEvents();
				probe.Waited = 0U;
				probe.Phase  = state_type::phase::awaiting_row_revealed;
				return result::pending;
			}
			// Delete the second row, the range.
			auto visible = std::vector<QToolButton*>{ };
			for (auto* const button : summary->pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"))) {
				if (button->isVisibleTo(window)) {
					visible.push_back(button);
				}
			}
			if (visible.size() != 2U) {
				return fail("row-deletes-not-two");
			}
			visible.back()->click();
			QCoreApplication::processEvents();
			renderQuestion(QStringLiteral("delete-comment-question"), summary->delete_confirmation());
			if (summary->delete_confirmation() == nullptr || !confirm(summary->delete_confirmation(), "deleteCommentConfirmationConfirm")) {
				return fail("row-delete-asked-nothing");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_row_deleted;
			return result::pending;
		}
		case state_type::phase::awaiting_row_revealed: {
			const auto selected = window->comparison_surface()->selection();
			note("page=" + std::to_string(finishPage() != nullptr ? 1 : 0) + " modality=" + std::to_string(static_cast<int>(window->current_modality()))
				 + " line=" + (selected ? std::to_string(selected->PreferredLine.Line) : std::string{ "-" }) + " open=" + cards->open_thread_id()
				 + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			// [[BPR-1590]] v003. Finish your review closes, the row's line
			// shows, and its draft opens in the drawer.
			if (window->review_sheet_open() || window->current_modality() != qt::review_modality::review || !selected
				|| selected->PreferredLine.Side != core::diff_side::right || probe.Lines.size() != 2U || selected->PreferredLine.Line != probe.Lines.back() || !ready()
				|| !editor->shown_draft().has_value() || editor->shown_draft()->Anchor.Line != probe.Lines.back() || cards->open_thread_id() != "draft:unpublished"
				|| !cards->writing_draft()) {
				return ++probe.Waited < 400U ? result::pending : fail("row-not-revealed");
			}
			probe.Evidence.RowRevealed = posts().empty() && pending().size() == 2U;
			if (!probe.Evidence.RowRevealed) {
				return fail("row-reveal-changed-the-review");
			}
			probe.RowRevealed = true;
			window->finish_review_action()->trigger();
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_finish_again;
			return result::pending;
		}
		case state_type::phase::awaiting_row_deleted: {
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " rows=" + std::to_string(summary->pending_rows().size()));
			if (now.size() != 1U || !chipSays(1U) || summary->pending_rows().size() != 1 || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("row-not-deleted");
			}
			const auto all            = saved();
			probe.Evidence.RowDeleted = probe.Lines.size() == 2U && now.front().Draft.Anchor.Line == probe.Lines.front()
				&& std::ranges::none_of(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == probe.Lines.back(); })
				&& finishPage() != nullptr && posts().empty();
			if (!probe.Evidence.RowDeleted) {
				return fail("row-delete-not-exact");
			}
			summary->discard_action()->trigger();
			QCoreApplication::processEvents();
			renderQuestion(QStringLiteral("discard-review-question"), summary->discard_confirmation());
			if (summary->discard_confirmation() == nullptr || !summary->discard_confirmation()->question().contains(QStringLiteral("1 pending comment"))
				|| !confirm(summary->discard_confirmation(), "discardReviewConfirmationConfirm")) {
				return fail("discard-review-asked-nothing");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_review_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_review_discarded: {
			const auto all = saved();
			note("pending=" + std::to_string(pending().size()) + " page=" + std::to_string(finishPage() != nullptr ? 1 : 0) + " chip=" + std::to_string(chipShown->isVisible() ? 1 : 0));
			if (!pending().empty() || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("review-not-discarded");
			}
			probe.Evidence.DiscardDeleted = finishPage() == nullptr && posts().empty()
				&& std::ranges::none_of(all, [&](const core::revisioned_review_draft& aDraft) { return aDraft.Draft.Anchor.Line == probe.Lines.front(); })
				&& std::ranges::any_of(all, [&](const core::revisioned_review_draft& aDraft) { return std::optional{ aDraft.Draft.Id } == probe.OldDraft && aDraft.Draft.Body == older.toStdString(); });
			if (!probe.Evidence.DiscardDeleted) {
				return fail("discard-review-not-exact");
			}
			window->compose_review_action()->trigger();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_compose_page;
			return result::pending;
		}
		case state_type::phase::awaiting_compose_page: {
			const auto& draft = summary->shown_draft();
			note("page=" + std::to_string(finishPage() != nullptr ? 1 : 0) + " title=" + summary->title_label()->text().toStdString());
			if (finishPage() == nullptr || !draft || draft->State != core::review_submission_state::drafting || !summary->body_editor()->isVisibleTo(window)) {
				return ++probe.Waited < 400U ? result::pending : fail("compose-not-opened");
			}
			if (summary->title_label()->text() != QStringLiteral("Compose review") || !draft->Comments.empty() || !draft->Body.empty()) {
				return fail("compose-not-a-new-review");
			}
			summary->body_editor()->setPlainText(summarised);
			QCoreApplication::processEvents();
			probe.Phase = state_type::phase::awaiting_compose_written;
			return result::pending;
		}
		case state_type::phase::awaiting_compose_written: {
			const auto& draft = summary->shown_draft();
			if (!draft || draft->Body != summarised.toStdString() || !summary->discard_action()->isEnabled()) {
				return ++probe.Waited < 400U ? result::pending : fail("compose-not-written");
			}
			summary->discard_action()->trigger();
			QCoreApplication::processEvents();
			if (summary->discard_confirmation() == nullptr || !confirm(summary->discard_confirmation(), "discardReviewConfirmationConfirm")) {
				return fail("compose-discard-asked-nothing");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_compose_discarded;
			return result::pending;
		}
		case state_type::phase::awaiting_compose_discarded: {
			note("page=" + std::to_string(finishPage() != nullptr ? 1 : 0));
			if (finishPage() != nullptr || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("compose-discard-not-closed");
			}
			probe.Evidence.SecondDiscard = posts().empty() && pending().empty();
			if (!probe.Evidence.SecondDiscard) {
				return fail("compose-discard-not-exact");
			}
			// Back to the older draft, at its line.
			if (!probe.Old || !window->comparison_surface()->select_conversation_line(core::diff_side::right, *probe.Old)) {
				return fail("older-line-not-selected");
			}
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_old_again;
			return result::pending;
		}
		case state_type::phase::awaiting_old_again: {
			const auto& draft = implementation->ReviewDraft;
			note("draft=" + std::to_string(draft && std::optional{ draft->Id } == probe.OldDraft ? 1 : 0) + " writing=" + std::to_string(cards->writing_draft() ? 1 : 0));
			if (!draft || std::optional{ draft->Id } != probe.OldDraft || !ready() || !write(older) || !written(older)) {
				return ++probe.Waited < 400U ? result::pending : fail("older-draft-not-opened");
			}
			// ⌘↩: Start a review.
			static_cast<void>(press(Qt::Key_Return, Qt::ControlModifier));
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_pending;
			return result::pending;
		}
		case state_type::phase::awaiting_again_pending: {
			const auto now = pending();
			note("pending=" + std::to_string(now.size()) + " chip=" + chip->summary().toStdString());
			if (now.size() != 1U || !chipSays(1U) || cards->writing_draft() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("older-draft-not-pending");
			}
			probe.Evidence.AgainStarted = std::optional{ now.front().Draft.Id } == probe.OldDraft && now.front().Draft.Body == older.toStdString() && posts().empty();
			if (!probe.Evidence.AgainStarted) {
				return fail("older-draft-not-pending-exactly");
			}
			probe.Drafts = { now.front().Draft.Id };
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_page;
			return result::pending;
		}
		case state_type::phase::awaiting_again_page: {
			auto* const state  = summary->findChild<QLabel*>(QStringLiteral("wholeReviewDraftState"));
			const auto  opened = openFinish();
			note("opened=" + std::to_string(opened ? 1 : 0) + " title=" + summary->title_label()->text().toStdString() + " rows=" + std::to_string(summary->pending_rows().size())
				 + " state=" + (state != nullptr ? state->text().toStdString() : std::string{ }) + " status=" + summary->status_label()->text().toStdString());
			if (!opened || !shown(1U)) {
				return ++probe.Waited < 400U ? result::pending : fail("again-page-not-shown");
			}
			// The owner saw "Review draft discarded." here.
			const auto said             = summary->status_label()->text() + (state != nullptr ? state->text() : QString{ });
			probe.Evidence.AgainOffered = !said.contains(QStringLiteral("discarded"), Qt::CaseInsensitive) && calm()
				&& summary->pending_rows().front().endsWith(older);
			if (!probe.Evidence.AgainOffered) {
				return fail("again-page-stale");
			}
			summary->disposition_button(core::review_disposition::comment)->click();
			QCoreApplication::processEvents();
			probe.Waited = 0U;
			probe.Phase  = state_type::phase::awaiting_again_submitted;
			return result::pending;
		}
		case state_type::phase::awaiting_again_submitted: {
			const auto sent      = posts();
			const auto published = countPublished();
			note("posts=" + std::to_string(sent.size()) + " published=" + std::to_string(published) + " status=" + summary->status_label()->text().toStdString());
			if (sent.size() != 1U || published != 1U || chipShown->isVisible() || !ready()) {
				return ++probe.Waited < 400U ? result::pending : fail("again-not-submitted");
			}
			const auto object             = QJsonDocument::fromJson(QByteArray::fromStdString(sent.front())).object();
			const auto comments           = object.value(QStringLiteral("comments")).toArray();
			probe.Evidence.AgainSubmitted = object.value(QStringLiteral("event")).toString() == QStringLiteral("COMMENT") && comments.size() == 1
				&& comments.at(0).toObject().value(QStringLiteral("body")).toString() == older && probe.Old
				&& comments.at(0).toObject().value(QStringLiteral("line")).toInt() == static_cast<int>(*probe.Old) + 1 && pending().empty()
				&& finishPage() == nullptr && window->statusBar()->currentMessage() == QStringLiteral("Review submitted with 1 comment");
			if (probe.Evidence.AgainSubmitted) {
				renderThemes(QStringLiteral("submitted-returned"));
			}
			if (!probe.Evidence.AgainSubmitted) {
				return fail("again-submitted-not-exact");
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}
		case state_type::phase::passed:
			return result::passed;
		case state_type::phase::failed:
		default:
			break;
		}
		return result::failed;
	}

	auto detail::github_application_fixture_access::start_review_evidence(const github_application& aApplication) -> detail::start_review_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->StartReviewProbe.has_value()) {
			return { };
		}
		return implementation->StartReviewProbe->Evidence;
	}

	auto detail::github_application_fixture_access::start_review_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->StartReviewProbe.has_value()) {
			return { };
		}
		return implementation->StartReviewProbe->Diagnostic;
	}

	auto detail::github_application_fixture_access::exercise_review_progress_push(
		github_application& aApplication, const detail::review_progress_push_course aCourse) -> detail::review_progress_push_result
	{
		using result               = detail::review_progress_push_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReviewProgressPushProbe.has_value()) {
			implementation->ReviewProgressPushProbe.emplace();
		}
		auto& probe = *implementation->ReviewProgressPushProbe;
		if (probe.Failed) {
			return result::failed;
		}
		if (probe.Passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr || implementation->WorkbenchHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr || !implementation->LastAcceptedHandoff.has_value()) {
			return result::pending;
		}
		auto* const window = &*implementation->Window;
		auto* const ledger = implementation->ReviewLedgerHost.get();
		const auto  fail   = [&probe](const std::string_view aWhy) {
			probe.Diagnostic += " failed=" + std::string{ aWhy };
			probe.Failed = true;
			return result::failed;
		};
		enum class gesture : std::uint8_t {
			accept,
			exclude,
		};
		struct course_file
		{
			std::string_view Path;
			gesture          Gesture;
		};
		// renderer.cpp is the one file the push changes.
		constexpr auto files = std::array{
			course_file{ .Path = "src/engine/renderer.cpp", .Gesture = gesture::accept },
			course_file{ .Path = "src/main.cpp", .Gesture = gesture::accept },
			course_file{ .Path = "src/ui/widgets/button.cpp", .Gesture = gesture::exclude },
		};
		if (probe.File == 0U && !probe.Revealed && probe.Marks.empty()) {
			const auto& evidence = implementation->Fixture->evidence();
			const auto  expected = core::parse_git_commit_oid(aCourse == detail::review_progress_push_course::after ? evidence.HeadTwoOid : evidence.HeadOneOid);
			if (!expected.has_value() || implementation->LastAcceptedHandoff->Head != *expected) {
				return ++probe.Waited < 400U ? result::pending : fail("head");
			}
			probe.Marks  = "head=" + std::string{ core::canonical_hex(implementation->LastAcceptedHandoff->Head).view() };
			probe.Waited = 0U;
		}
		if (probe.File == files.size()) {
			probe.Passed = true;
			return result::passed;
		}
		const auto& file = files[probe.File];
		if (!probe.Revealed) {
			const auto revealed = reveal_fixture_file(aApplication, file.Path);
			if (revealed == detail::fixture_file_reveal_result::failed) {
				return fail("reveal " + std::string{ file.Path });
			}
			probe.Revealed = revealed == detail::fixture_file_reveal_result::shown;
			return result::pending;
		}
		// The file's receipt is shown once the ledger has answered the
		// observation of exactly this file's material.
		const auto& selected = implementation->LastSelectedDiff;
		const auto  retained = ledger->retained();
		const auto  shown    = ledger->is_quiescent() && selected.has_value() && selected->Request.File.PresentationPath == file.Path
			&& retained.File == selected->Request.File.File && retained.Material == core::file_material(selected->Key);
		if (!shown) {
			return ++probe.Waited < 400U ? result::pending : fail("receipt " + std::string{ file.Path });
		}
		if (aCourse == detail::review_progress_push_course::before && !probe.Acted) {
			auto* const action = file.Gesture == gesture::accept ? window->accept_action() : window->exclude_action();
			if (action == nullptr || !action->isEnabled()) {
				return fail("gesture " + std::string{ file.Path });
			}
			probe.Acted = true;
			action->trigger();
			return result::pending;
		}
		if (aCourse == detail::review_progress_push_course::before && !(file.Gesture == gesture::accept ? retained.Accepted : retained.Excluded)) {
			return ++probe.Waited < 400U ? result::pending : fail("durable " + std::string{ file.Path });
		}
		const auto* const button = window->findChild<QWidget*>(QStringLiteral("workbenchReviewControlsButton"));
		probe.Marks += ";" + std::string{ file.Path } + "=" + (button != nullptr ? button->accessibleName().toStdString() : std::string{ "absent" });
		++probe.File;
		probe.Revealed = false;
		probe.Acted    = false;
		probe.Waited   = 0U;
		return result::pending;
	}

	auto detail::github_application_fixture_access::review_progress_push_marks(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReviewProgressPushProbe.has_value() ? implementation->ReviewProgressPushProbe->Marks : std::string{ };
	}

	auto detail::github_application_fixture_access::review_progress_push_diagnostic(const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->ReviewProgressPushProbe.has_value() ? implementation->ReviewProgressPushProbe->Diagnostic : std::string{ };
	}

	auto detail::github_application_fixture_access::published_file_switch_evidence(
		const github_application& aApplication) -> detail::published_file_switch_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->PublishedFileSwitchProbe.has_value()) {
			return { };
		}
		return implementation->PublishedFileSwitchProbe->Evidence;
	}

	auto detail::github_application_fixture_access::published_file_switch_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->PublishedFileSwitchProbe.has_value()) {
			return { };
		}
		return implementation->PublishedFileSwitchProbe->Diagnostic;
	}

	auto detail::github_application_fixture_access::threaded_reply_diagnostic(
		const github_application& aApplication) -> std::string
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->ThreadedReplyProbe.has_value()) {
			return { };
		}
		return implementation->ThreadedReplyProbe->Diagnostic;
	}

	auto detail::github_application_fixture_access::threaded_reply_evidence(
		const github_application& aApplication) -> detail::threaded_reply_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->ThreadedReplyProbe.has_value()) {
			return { };
		}
		return implementation->ThreadedReplyProbe->Evidence;
	}

	auto detail::github_application_fixture_access::review_round_trip_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_round_trip_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->ReviewRoundTripCompositionProbe.has_value()
			? implementation->ReviewRoundTripCompositionProbe->Evidence
			: detail::review_round_trip_composition_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::dormant_live_provider_selection_evidence_for(
		const github_application& aApplication)
		-> detail::dormant_live_provider_selection_evidence
	{
		auto  evidence       = detail::dormant_live_provider_selection_evidence{ };
		auto* implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return evidence;
		}
		auto       handoff  = clone_without_smart_git(*implementation->LastAcceptedHandoff);
		auto       snapshot = application::github_pull_request_snapshot{ };
		const auto base     = core::parse_git_commit_oid(designated_review_base_oid);
		const auto head     = core::parse_git_commit_oid(designated_review_head_oid);
		const auto baseRef  = application::parse_git_base_branch_ref(
			designated_review_base_full_ref);
		if (!base.has_value() || !head.has_value() || !baseRef.has_value()) {
			return evidence;
		}

		handoff.Hostname = application::github_canonical_hostname{
			designated_review_hostname
		};
		handoff.ServerContract = application::github_server_contract::github_com_2022_11_28;
		handoff.Repository     = core::repository_id{ designated_review_repository_id };
		handoff.Locator        = {
			.Owner = std::string{ designated_review_owner },
			.Name  = std::string{ designated_review_name },
		};
		handoff.PullRequest  = core::pull_request_id{ designated_review_pull_request_id };
		handoff.Number       = core::pull_request_number{ designated_review_number };
		handoff.BaseRef      = *baseRef;
		handoff.Base         = *base;
		handoff.RecordedBase = *base;
		handoff.Head         = *head;

		snapshot.Profile = handoff.Profile;
		snapshot.Host    = handoff.Host;
		snapshot.Account = handoff.Account;
		snapshot.Viewer  = {
			.Actor = application::github_actor_id{ "MDQ6VXNlcjIy" },
			.Login = "fixture-reviewer",
		};
		snapshot.Repository.Repository   = handoff.Repository;
		snapshot.Repository.Node         = application::github_node_id{ std::string{ designated_review_repository_node } };
		snapshot.Repository.Owner        = std::string{ designated_review_owner };
		snapshot.Repository.Name         = std::string{ designated_review_name };
		snapshot.PullRequest.PullRequest = handoff.PullRequest;
		snapshot.PullRequest.Repository  = handoff.Repository;
		snapshot.PullRequest.Number      = handoff.Number;
		snapshot.PullRequest.Node        = application::github_node_id{ std::string{ designated_review_pull_request_node } };
		snapshot.PullRequest.State       = application::github_pull_request_state::open;
		snapshot.PullRequest.BaseOid     = application::commit_oid{ std::string{ designated_review_base_oid } };
		snapshot.PullRequest.HeadOid     = application::commit_oid{ std::string{ designated_review_head_oid } };
		snapshot.PullRequest.BaseRef     = std::string{ designated_review_base_ref };
		snapshot.PullRequest.HeadRef     = std::string{ designated_review_head_ref };

		const auto exactSelection = select_designated_review_mutation(
			handoff, snapshot);
		evidence.ExactTupleAccepted = exactSelection.has_value();
		auto rejects                = [&handoff, &snapshot, &evidence](auto aMutate) {
			auto changedHandoff  = clone_without_smart_git(handoff);
			auto changedSnapshot = snapshot;
			aMutate(changedHandoff, changedSnapshot);
			evidence.MismatchedTuplesRejected += static_cast<std::size_t>(
				!select_designated_review_mutation(
					 changedHandoff, changedSnapshot)
					 .has_value());
		};
		rejects([](auto& aHandoff, auto&) { aHandoff.Hostname = application::github_canonical_hostname{ "ghe.example.invalid" }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Locator.Owner = "Other"; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Locator.Name = "Other"; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Repository = core::repository_id{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.PullRequest = core::pull_request_id{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Number = core::pull_request_number{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Base = aHandoff.Head; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Head = aHandoff.Base; });
		rejects([](auto& aHandoff, auto&) { aHandoff.BaseRef = application::git_base_branch_ref{ }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Profile = core::host_profile_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Host = core::git_host_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Account = core::github_account_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Viewer.Actor = application::github_actor_id{ }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Viewer.Login.clear(); });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Repository = core::repository_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Node = application::github_node_id{ "other" }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Owner = "Other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Name = "Other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.PullRequest = core::pull_request_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Repository = core::repository_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Number = core::pull_request_number{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Node = application::github_node_id{ "other" }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.State = application::github_pull_request_state::closed; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.BaseOid = application::commit_oid{ std::string{ designated_review_head_oid } }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.HeadOid = application::commit_oid{ std::string{ designated_review_base_oid } }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.BaseRef = "other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.HeadRef = "other"; });
		evidence.ExpectedMismatchRejections = 28U;
		evidence.FixtureProviderSelected
			= implementation->FixtureReviewMutationHostComposed;
		evidence.LiveProviderAbsent = true;
		return evidence;
	}

	auto detail::github_application_fixture_access::review_mutation_provider_marker(
		const github_application& aApplication) noexcept -> std::string_view
	{
		const auto* implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->Fixture != nullptr
			? std::string_view{ "fixture" }
			: std::string_view{ "none" };
	}

	auto detail::github_application_fixture_access::exercise_defect_composition(
		github_application&                    aApplication,
		const detail::defect_composition_probe aCourse)
		-> detail::defect_composition_probe_result
	{
		using result               = detail::defect_composition_probe_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->DefectCompositionProbe.has_value()) {
			implementation->DefectCompositionProbe.emplace();
			implementation->DefectCompositionProbe->Course = aCourse;
		}
		auto& probe = *implementation->DefectCompositionProbe;
		if (probe.Phase
			== defect_composition_probe_state::phase::failed) {
			return result::failed;
		}
		if (probe.Phase
			== defect_composition_probe_state::phase::passed) {
			return result::passed;
		}
		using state_type = defect_composition_probe_state;
		const auto fail  = [&probe] {
			probe.Phase = defect_composition_probe_state::phase::failed;
			return result::failed;
		};
		// [[BPR-1037]]. Every phase that waits is bounded and names what it
		// was waiting for; see `keep_waiting`.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		const auto phaseName = [](const state_type::phase aPhase) {
			switch (aPhase) {
			case state_type::phase::awaiting_ready:
				return "awaiting_ready";
			case state_type::phase::awaiting_diff:
				return "awaiting_diff";
			case state_type::phase::awaiting_classified:
				return "awaiting_classified";
			case state_type::phase::awaiting_verified:
				return "awaiting_verified";
			case state_type::phase::awaiting_reclassified:
				return "awaiting_reclassified";
			case state_type::phase::awaiting_reverse_right:
				return "awaiting_reverse_right";
			case state_type::phase::awaiting_epoch_change:
				return "awaiting_epoch_change";
			case state_type::phase::awaiting_shared_ready:
				return "awaiting_shared_ready";
			case state_type::phase::passed:
				return "passed";
			case state_type::phase::failed:
				return "failed";
			}
			return "unknown";
		};
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| implementation->ReviewMutationHost == nullptr
			|| implementation->Window->defect_card() == nullptr) {
			// The course drives the composed window, ledger and mutation host.
			return wait("DEFECT_COMPOSITION_STALLED", [&](QTextStream& aStream) {
				aStream << "phase=" << phaseName(probe.Phase)
						<< " fixture=" << present(implementation->Fixture != nullptr)
						<< " window=" << present(implementation->Window != nullptr)
						<< " host=" << present(implementation->Host != nullptr)
						<< " ledger_host=" << present(implementation->ReviewLedgerHost != nullptr)
						<< " mutation_host=" << present(implementation->ReviewMutationHost != nullptr)
						<< " defect_card=" << present(implementation->Window != nullptr && implementation->Window->defect_card() != nullptr);
			});
		}
		const auto modality = enter_course_modality(aApplication, probe.Modality);
		if (modality == detail::course_modality_result::pending) {
			return result::pending;
		}
		if (modality == detail::course_modality_result::failed) {
			return fail();
		}
		// [[BPR-1211]]. A defect command is kept once its write is durable.
		if (probe.Phase == state_type::phase::awaiting_verified || probe.Phase == state_type::phase::awaiting_reclassified) {
			return defect_composition_after_verify(aApplication);
		}
		auto* const window = implementation->Window.get();
		if (!probe.CommentsRequested) {
			probe.CommentsRequested = true;
			if (window->conversations_drawer()->state() != core::workbench_drawer_state::expanded) {
				window->conversations_drawer_toggle_action()->trigger();
				return result::pending;
			}
		}
		if (probe.Phase == state_type::phase::awaiting_ready && !window->conversation_panel()->isVisible()) {
			return wait("DEFECT_COMMENTS_STALLED", [&](QTextStream& aStream) { aStream << course_modality_facts(aApplication); });
		}
		const auto firstIdentity        = std::string{ "review-thread:PRRT_fixture_1" };
		const auto secondIdentity       = std::string{ "review-thread:PRRT_fixture_3" };
		const auto fileIdentity         = std::string{ "review-thread:PRRT_fixture_file" };
		const auto firstDefectIdentity  = defect_identity(firstIdentity);
		const auto secondDefectIdentity = defect_identity(secondIdentity);
		const auto findDefect           = [implementation](const local_defect_identity aIdentity)
			-> const application::durable_defect_record* {
			const auto found = std::ranges::find_if(
				implementation->DurableDefects,
				[aIdentity](const application::durable_defect_record& aDefect) {
					return aDefect.Conversation.value() == aIdentity.Conversation
						&& aDefect.History.Defect.Id.value() == aIdentity.Defect;
				});
			return found == implementation->DurableDefects.end() ? nullptr : &*found;
		};
		const auto anchorMatchesThread = [implementation](
											 const application::durable_defect_record* const aDefect,
											 const std::string_view                          aConversation) {
			const auto* const thread = implementation->accepted_thread(aConversation);
			if (aDefect == nullptr || thread == nullptr || thread->Outdated
				|| !thread->Line.has_value() || *thread->Line == 0U) {
				return false;
			}
			const auto side = thread->DiffSide == application::github_diff_side::left
				? std::optional{ core::diff_side::left }
				: thread->DiffSide == application::github_diff_side::right
				? std::optional{ core::diff_side::right }
				: std::nullopt;
			return side.has_value()
				&& aDefect->History.OriginalAnchor.Side == *side
				&& aDefect->History.OriginalAnchor.Line == *thread->Line - 1U;
		};

		if (aCourse == detail::defect_composition_probe::reopen) {
			const auto* const first  = findDefect(firstDefectIdentity);
			const auto* const second = findDefect(secondDefectIdentity);
			if (first == nullptr || second == nullptr) {
				// The seed course's two defects come back from the ledger.
				return wait("DEFECT_REOPEN_STALLED", [&](QTextStream& aStream) {
					const auto statistics = implementation->ReviewLedgerHost->statistics();
					aStream << "durable_defects=" << static_cast<qulonglong>(implementation->DurableDefects.size())
							<< " first=" << present(first != nullptr)
							<< " second=" << present(second != nullptr)
							<< " ledger_private_loads=" << static_cast<qulonglong>(statistics.PrivateLoads)
							<< " ledger_private_writes=" << static_cast<qulonglong>(statistics.PrivateWrites)
							<< " shown_defects=" << static_cast<qulonglong>(window->defect_count());
				});
			}
			probe.Evidence.ExactThreads = anchorMatchesThread(first, firstIdentity)
				&& anchorMatchesThread(second, secondIdentity);
			probe.Evidence.LocalClassification
				= first->History.Defect.Severity == core::defect_severity::critical
				&& first->History.Defect.Category == core::defect_category::security;
			probe.Evidence.CorrelatedEvidence
				= second->History.Defect.State == core::defect_state::verified
				&& second->History.Availability
					== core::defect_anchor_availability::outdated;
			probe.Evidence.ExplicitVerification
				= first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State == core::defect_state::verified;
			probe.Evidence.DurableHistory
				= second->History.Availability
					== core::defect_anchor_availability::outdated
				&& second->History.Defect.Anchor == second->History.OriginalAnchor;
			const auto before = window->defect_card()->shown_defect();
			if (window->next_defect_action()->isEnabled()) {
				window->next_defect_action()->trigger();
			} else if (window->previous_defect_action()->isEnabled()) {
				window->previous_defect_action()->trigger();
			}
			probe.Evidence.MultiDefectNavigation = window->defect_count() == 2U
				&& before.has_value()
				&& window->defect_card()->shown_defect().has_value()
				&& window->defect_card()->shown_defect()->Id != before->Id;
			probe.Evidence.SharedActionSeparate
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			const auto statistics            = implementation->ReviewLedgerHost->statistics();
			probe.Evidence.RestartRestored   = statistics.PrivateLoads >= 1U;
			probe.Evidence.ZeroReopenWrites  = statistics.PrivateWrites == 0U;
			probe.Evidence.VisibleProjection = window->defect_count_label()->text().contains(
				QStringLiteral("/2"));
			if (probe.Evidence != detail::defect_composition_probe_evidence{ .ExactThreads = true, .LocalClassification = true, .CorrelatedEvidence = true, .ExplicitVerification = true, .DurableHistory = true, .MultiDefectNavigation = true, .SharedActionSeparate = true, .RestartRestored = true, .ZeroReopenWrites = true, .VisibleProjection = true }) {
				return fail();
			}
			probe.Phase = defect_composition_probe_state::phase::passed;
			return result::passed;
		}

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_shared_ready) {
			auto* const editor = window->review_editor();
			if (editor == nullptr || !editor->shown_draft().has_value()) {
				// The shared follow-up needs a draft shown in the editor.
				return wait("DEFECT_SHARED_READY_STALLED", [&](QTextStream& aStream) {
					aStream << "review_editor=" << present(editor != nullptr)
							<< " shown_draft=" << present(editor != nullptr && editor->shown_draft().has_value())
							<< " review_draft=" << present(implementation->ReviewDraft.has_value())
							<< " submissions=" << static_cast<qulonglong>(implementation->Fixture->review_mutation_evidence().Submissions);
				});
			}
			probe.Evidence.SharedActionSeparate
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			if (!probe.Evidence.SharedActionSeparate) {
				return fail();
			}
			probe.Evidence.RestartRestored  = false;
			probe.Evidence.ZeroReopenWrites = false;
			probe.Phase                     = defect_composition_probe_state::phase::passed;
			editor->body_editor()->setPlainText(
				QStringLiteral("Explicit shared follow-up after local classification."));
			editor->publish_action()->trigger();
			return result::passed;
		}

		auto* const model = window->conversation_model();
		auto* const view  = window->conversation_view();
		if ((probe.Phase == state_type::phase::awaiting_ready || probe.Phase == state_type::phase::awaiting_diff)
			&& (!model->index_for_id(firstIdentity).isValid()
				|| !model->index_for_id(secondIdentity).isValid()
				|| !model->index_for_id(fileIdentity).isValid())) {
			// The three fixture conversations reach the conversation model.
			return wait("DEFECT_CONVERSATIONS_STALLED", [&](QTextStream& aStream) {
				aStream << "phase=" << phaseName(probe.Phase)
						<< " first_conversation=" << present(model->index_for_id(firstIdentity).isValid())
						<< " second_conversation=" << present(model->index_for_id(secondIdentity).isValid())
						<< " file_conversation=" << present(model->index_for_id(fileIdentity).isValid())
						<< " conversation_rows=" << model->rowCount();
			});
		}
		const auto selectConversation = [window, model, view](
											const std::string_view aIdentity) {
			window->conversation_panel()->set_anchor_line(std::nullopt);
			const auto index = model->index_for_id(aIdentity);
			if (!index.isValid()) {
				return false;
			}
			view->setCurrentIndex(index);
			return true;
		};

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_ready) {
			if (!implementation->DurableDefects.empty()) {
				return fail();
			}
			auto* const files = window->files_drawer()->contents_view();
			if (implementation->WorkbenchHost == nullptr
				|| implementation->WorkbenchHost->outstanding_requests() != 0U
				|| implementation->WorkbenchHost->actionable().empty()
				|| files == nullptr || files->model() == nullptr
				|| files->model()->rowCount() == 0) {
				// The workbench host observes the pull request's files and
				// the drawer lists them.
				return wait("DEFECT_READY_STALLED", [&](QTextStream& aStream) {
					aStream << "workbench_host=" << present(implementation->WorkbenchHost != nullptr)
							<< " actionable=" << static_cast<qulonglong>(implementation->WorkbenchHost == nullptr ? 0U : implementation->WorkbenchHost->actionable().size())
							<< " files_view=" << present(files != nullptr)
							<< " file_rows=" << (files == nullptr || files->model() == nullptr ? 0 : files->model()->rowCount());
				});
			}
			files->setCurrentIndex(files->model()->index(0, 0));
			return advance(state_type::phase::awaiting_diff);
		}

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_diff) {
			if (!implementation->SelectedDiffPublication.has_value()
				|| window->comparison_surface()->current_result() == nullptr) {
				// The file gesture's diff is computed through the Git and cache
				// actors and published to the comparison.
				return wait("DEFECT_SELECTED_DIFF_STALLED", [&](QTextStream& aStream) {
					const auto work = diff_work_evidence(aApplication);
					aStream << "selected_diff_publication=" << present(implementation->SelectedDiffPublication.has_value())
							<< " comparison_result=" << present(window->comparison_surface()->current_result() != nullptr)
							<< " selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
							<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
							<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
							<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
							<< " diff_pending=" << truth(work.Pending)
							<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs);
				});
			}
			if (!selectConversation(firstIdentity)
				|| !window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			if (!selectConversation(secondIdentity)
				|| !window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			if (!selectConversation(fileIdentity)
				|| window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			// [[BPR-1211]]. Each classification is kept once its write is durable.
			return advance(state_type::phase::awaiting_classified);
		}

		if (probe.Phase == state_type::phase::awaiting_classified) {
			const auto* const first  = findDefect(firstDefectIdentity);
			const auto* const second = findDefect(secondDefectIdentity);
			if (first == nullptr || second == nullptr || !implementation->ReviewLedgerHost->is_quiescent()) {
				return wait("DEFECT_CLASSIFICATION_STALLED", [&](QTextStream& aStream) {
					aStream << "durable_defects=" << static_cast<qulonglong>(implementation->DurableDefects.size())
							<< " ledger_outstanding=" << static_cast<qulonglong>(implementation->ReviewLedgerHost->outstanding());
				});
			}
			probe.Evidence.ExactThreads = anchorMatchesThread(first, firstIdentity)
				&& anchorMatchesThread(second, secondIdentity);
			probe.Evidence.LocalClassification
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			probe.Evidence.CorrelatedEvidence = first != nullptr && second != nullptr
				&& first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State
					== core::defect_state::needs_verification;
			probe.Evidence.DurableHistory = second != nullptr
				&& second->History.Availability
					== core::defect_anchor_availability::available
				&& second->History.Defect.Anchor == second->History.OriginalAnchor;
			probe.Evidence.VisibleProjection = window->defect_count() == 2U
				&& window->defect_count_label()->text().contains(QStringLiteral("/2"));
			if (!probe.Evidence.ExactThreads || !probe.Evidence.LocalClassification
				|| !probe.Evidence.CorrelatedEvidence
				|| !probe.Evidence.DurableHistory
				|| !probe.Evidence.VisibleProjection) {
				return fail();
			}
			// [[BPR-1208]]. The same accepted defect is reachable on its own
			// thread, while the private tab contains no thread controls.
			auto        menu   = std::unique_ptr<QMenu>{ window->conversation_panel()->cards()->thread_menu(secondIdentity) };
			const auto& cards  = window->conversation_panel()->cards()->cards();
			const auto  card   = std::ranges::find(cards, secondIdentity, &qt::thread_card::Id);
			auto*       verify = menu ? menu->findChild<QAction*>(QStringLiteral("defectVerify")) : nullptr;
			if (!verify || !verify->isEnabled() || card == cards.end() || card->DefectBadge.isEmpty() || !window->conversation_panel()->cards()->isVisible()
				|| !window->comments_tab_scroll()->isAncestorOf(window->defect_count_label())
				|| window->findings_tab_scroll()->isAncestorOf(window->defect_card())) {
				return fail();
			}
			QTextStream{ stdout } << "DEFECT_THREAD_CONTROLS_OK\n";
			probe.BeforeComparison = core::comparison_digest(
				implementation->SelectedDiffPublication->Key);
			auto* const control  = window->epoch_selectors();
			const auto  leftRow  = control->left_selector()->currentIndex();
			const auto  rightRow = control->right_selector()->currentIndex();
			if (leftRow < 0 || rightRow < 0 || leftRow == rightRow) {
				return fail();
			}
			probe.ReverseLeftRow              = rightRow;
			probe.ReverseRightRow             = leftRow;
			probe.RefuseCorrelatedPersistence = true;
			control->left_selector()->setCurrentIndex(rightRow);
			return advance(state_type::phase::awaiting_reverse_right);
		}

		// What reversing the comparison depends on: the epoch core's status
		// and both selectors against the rows the course asked for.
		const auto epochFacts = [&](QTextStream& aStream) {
			auto* const control = window->epoch_selectors();
			aStream << "epoch_host=" << present(implementation->EpochHost != nullptr)
					<< " epoch_status=" << (implementation->EpochHost == nullptr ? -1 : static_cast<int>(implementation->EpochHost->state().Status))
					<< " resolving=" << truth(implementation->EpochHost != nullptr && implementation->EpochHost->state().Status == core::epoch_status::resolving)
					<< " left_row=" << control->left_selector()->currentIndex()
					<< " expected_left_row=" << probe.ReverseLeftRow
					<< " right_row=" << control->right_selector()->currentIndex()
					<< " expected_right_row=" << probe.ReverseRightRow;
		};

		if (probe.Phase
			== defect_composition_probe_state::phase::awaiting_reverse_right) {
			// [[BPR-0655]]. The control blanks the left selector the moment
			// the core starts resolving the anchor it dispatched, so reading a
			// blank selector as "not applied" and dispatching again on the
			// next 10 ms tick superseded every resolution slower than a tick:
			// each completion arrived one generation stale and the course
			// never left `resolving`. Wait while the core resolves; only a
			// settled core showing another left anchor is asked again.
			auto* const control = window->epoch_selectors();
			if (implementation->EpochHost == nullptr
				|| implementation->EpochHost->state().Status
					== core::epoch_status::resolving) {
				return wait("DEFECT_REVERSE_RIGHT_STALLED", epochFacts);
			}
			if (control->left_selector()->currentIndex() != probe.ReverseLeftRow) {
				control->left_selector()->setCurrentIndex(probe.ReverseLeftRow);
				return wait("DEFECT_REVERSE_RIGHT_STALLED", epochFacts);
			}
			control->right_selector()->setCurrentIndex(probe.ReverseRightRow);
			return advance(state_type::phase::awaiting_epoch_change);
		}

		// The reversed comparison is published and reaches the surface.
		const auto epochChangeFacts = [&](QTextStream& aStream) {
			const auto& shown = window->comparison_surface()->current_result();
			aStream << "selected_diff_publication=" << present(implementation->SelectedDiffPublication.has_value())
					<< " comparison_changed="
					<< truth(implementation->SelectedDiffPublication.has_value()
							 && core::comparison_digest(implementation->SelectedDiffPublication->Key) != probe.BeforeComparison)
					<< " comparison_result=" << present(shown != nullptr)
					<< " result_matches_publication="
					<< truth(shown != nullptr && implementation->SelectedDiffPublication.has_value()
							 && shown->Alignment.Digest == core::comparison_digest(implementation->SelectedDiffPublication->Key))
					<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
					<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
					<< " refused_persistence_attempts=" << static_cast<qulonglong>(probe.RefusedPersistenceAttempts)
					<< ' ';
			epochFacts(aStream);
		};
		if (!implementation->SelectedDiffPublication.has_value()
			|| core::comparison_digest(implementation->SelectedDiffPublication->Key)
				== probe.BeforeComparison) {
			return wait("DEFECT_EPOCH_CHANGE_STALLED", epochChangeFacts);
		}
		const auto& reversedResult
			= window->comparison_surface()->current_result();
		if (reversedResult == nullptr
			|| reversedResult->Alignment.Digest
				!= core::comparison_digest(
					implementation->SelectedDiffPublication->Key)) {
			return wait("DEFECT_EPOCH_CHANGE_STALLED", epochChangeFacts);
		}
		// BPR-1205 deliberately hides later threads on this older pair. The
		// same refusal is stronger here: there is no selectable wrong-revision
		// thread, no code anchor, and no classification command to dispatch.
		if (model->index_for_id(firstIdentity).isValid() || model->index_for_id(secondIdentity).isValid()) {
			return fail();
		}
		const auto defectCountBeforeRefusal = implementation->DurableDefects.size();
		if (window->comparison_surface()->conversation_anchor().has_value()
			|| window->classify_defect_action()->isEnabled()
			|| defectCountBeforeRefusal != 2U) {
			return fail();
		}
		window->classify_defect_action()->trigger();
		if (implementation->DurableDefects.size() != defectCountBeforeRefusal) {
			return fail();
		}
		const auto* first  = findDefect(firstDefectIdentity);
		const auto* second = findDefect(secondDefectIdentity);
		if (first == nullptr || second == nullptr
			|| first->History.Defect.State
				!= core::defect_state::open
			|| second->History.Defect.State
				!= core::defect_state::needs_verification
			|| first->History.Availability
				!= core::defect_anchor_availability::outdated
			|| second->History.Availability
				!= core::defect_anchor_availability::outdated) {
			return fail();
		}
		probe.Evidence.PersistFailureFailsClosed
			= probe.RefusedPersistenceAttempts >= 2U;
		probe.RefuseCorrelatedPersistence = false;
		if (!probe.Evidence.PersistFailureFailsClosed) {
			return fail();
		}
		while (window->defect_card()->shown_defect().has_value()
			   && window->defect_card()->shown_defect()->Id != second->History.Defect.Id) {
			if (window->previous_defect_action()->isEnabled()) {
				window->previous_defect_action()->trigger();
			} else if (window->next_defect_action()->isEnabled()) {
				window->next_defect_action()->trigger();
			} else {
				return fail();
			}
		}
		window->defect_card()->verify_action()->trigger();
		return advance(state_type::phase::awaiting_verified);
	}

	auto detail::github_application_fixture_access::defect_composition_after_verify(
		github_application& aApplication) -> detail::defect_composition_probe_result
	{
		using result               = detail::defect_composition_probe_result;
		using state_type           = defect_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		auto&       probe          = *implementation->DefectCompositionProbe;
		auto* const window         = implementation->Window.get();
		const auto  fail           = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto findDefect = [implementation](const std::string_view aThread) -> const application::durable_defect_record* {
			const auto identity = defect_identity(aThread);
			const auto found    = std::ranges::find_if(implementation->DurableDefects, [identity](const application::durable_defect_record& aDefect) {
				return aDefect.Conversation.value() == identity.Conversation && aDefect.History.Defect.Id.value() == identity.Defect;
			});
			return found == implementation->DurableDefects.end() ? nullptr : &*found;
		};
		const auto* first  = findDefect("review-thread:PRRT_fixture_1");
		const auto* second = findDefect("review-thread:PRRT_fixture_3");
		const auto  facts  = [&](QTextStream& aStream) {
			aStream << "phase=" << static_cast<int>(probe.Phase)
					<< " durable_defects=" << static_cast<qulonglong>(implementation->DurableDefects.size())
					<< " ledger_outstanding=" << static_cast<qulonglong>(implementation->ReviewLedgerHost->outstanding());
		};
		if (!implementation->ReviewLedgerHost->is_quiescent()) {
			return keep_waiting(aApplication, probe.Wait, "DEFECT_COMMAND_STALLED", facts) ? result::pending : fail();
		}
		auto* const card = window->defect_card();
		if (probe.Phase == state_type::phase::awaiting_verified) {
			probe.Evidence.ExplicitVerification = first != nullptr && second != nullptr
				&& first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State == core::defect_state::verified;
			if (!probe.Evidence.ExplicitVerification) {
				return fail();
			}
			while (card->shown_defect().has_value()
				   && card->shown_defect()->Id != first->History.Defect.Id) {
				if (window->previous_defect_action()->isEnabled()) {
					window->previous_defect_action()->trigger();
				} else if (window->next_defect_action()->isEnabled()) {
					window->next_defect_action()->trigger();
				} else {
					return fail();
				}
			}
			card->severity_selector()->setCurrentIndex(
				card->severity_selector()->findData(
					static_cast<int>(core::defect_severity::critical)));
			card->category_selector()->setCurrentIndex(
				card->category_selector()->findData(
					static_cast<int>(core::defect_category::security)));
			card->reclassify_action()->trigger();
			return advance(state_type::phase::awaiting_reclassified);
		}
		probe.Evidence.DurableHistory = probe.Evidence.DurableHistory
			&& first != nullptr
			&& first->History.Defect.Severity == core::defect_severity::critical
			&& first->History.Defect.Category == core::defect_category::security;
		const auto before = card->shown_defect()->Id;
		if (window->next_defect_action()->isEnabled()) {
			window->next_defect_action()->trigger();
		} else {
			window->previous_defect_action()->trigger();
		}
		probe.Evidence.MultiDefectNavigation
			= card->shown_defect().has_value() && card->shown_defect()->Id != before;
		// [[BPR-1238]]. The shared follow-up is a new root comment, which only a
		// right-side line takes ([[BPR-0587]]). The comparison keeps the first
		// thread's left-side line selected, so the reviewer picks a right-side
		// line, as they would before writing one.
		auto rightLineSelected = false;
		for (auto line = std::uint32_t{ 0U }; line < 64U && !rightLineSelected; ++line) {
			rightLineSelected = window->comparison_surface()->select_conversation_line(core::diff_side::right, line);
		}
		if (!rightLineSelected) {
			return fail();
		}
		return advance(state_type::phase::awaiting_shared_ready);
	}

	auto detail::github_application_fixture_access::defect_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::defect_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->DefectCompositionProbe.has_value()
			? implementation->DefectCompositionProbe->Evidence
			: detail::defect_composition_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::select_appearance_mode(
		github_application&         aApplication,
		const core::appearance_mode aMode) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		implementation->AppearanceHost->select(aMode);
		return implementation->AppearanceHost->settings().Mode == aMode;
	}

	auto detail::github_application_fixture_access::review_ledger_receipt_present(
		github_application& aApplication) -> bool
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
			&& implementation->ReviewLedgerWasPresentOnEntry;
	}

	auto detail::github_application_fixture_access::review_ledger_settled(
		github_application& aApplication, const std::optional<core::bpr_window_context> aContext) -> bool
	{
		const auto* implementation = aApplication.Implementation.get();
		if (aContext) {
			const auto found = aApplication.Sessions.find(*aContext);
			implementation   = found == aApplication.Sessions.end() ? nullptr : found->second.get();
		}
		return implementation != nullptr && !implementation->Closing && implementation->ReviewLedgerHost != nullptr
			&& implementation->ReviewLedgerHost->available().has_value()
			&& implementation->ReviewLedgerHost->is_quiescent();
	}
	auto detail::github_application_fixture_access::hold_restoration_terminals(github_application& aApplication) -> std::function<void()>
	{
		auto profile = aApplication.Profile;
		profile->RestorationDecisionGate.hold();
		return [profile] { profile->RestorationDecisionGate.release(); };
	}
	auto detail::github_application_fixture_access::hold_window_close_callbacks(github_application& aApplication) -> std::function<void()>
	{
		auto profile = aApplication.Profile;
		profile->WindowCloseDecisionGate.hold();
		return [profile] { profile->WindowCloseDecisionGate.release(); };
	}
	auto detail::github_application_fixture_access::held_restoration_and_close_callbacks(const github_application& aApplication) -> std::array<std::size_t, 2>
	{
		return { aApplication.Profile->RestorationDecisionGate.pending(), aApplication.Profile->WindowCloseDecisionGate.pending() };
	}
	auto detail::github_application_fixture_access::restoration_durable(const github_application& aApplication) -> bool
	{
		return aApplication.Profile->ReviewRestoration && aApplication.Profile->ReviewRestoration->durable();
	}

	auto detail::github_application_fixture_access::hold_review_ledger_terminals(github_application& aApplication, const core::bpr_window_context aContext, const bool aHold) -> bool
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found == aApplication.Sessions.end() || found->second->Closing || !found->second->ReviewLedgerHost || !found->second->ReviewLedgerDecisionGate) {
			return false;
		}
		auto& gate = *found->second->ReviewLedgerDecisionGate;
		if (aHold) {
			gate.hold();
		} else {
			gate.release();
		}
		return true;
	}

	auto detail::github_application_fixture_access::release_review_ledger_terminal(github_application& aApplication, const core::bpr_window_context aContext) -> bool
	{
		const auto found = aApplication.Sessions.find(aContext);
		return found != aApplication.Sessions.end() && !found->second->Closing && found->second->ReviewLedgerDecisionGate
			&& found->second->ReviewLedgerDecisionGate->release_next();
	}

	auto detail::github_application_fixture_access::held_review_ledger_terminals(const github_application& aApplication, const core::bpr_window_context aContext) -> std::size_t
	{
		const auto found = aApplication.Sessions.find(aContext);
		return found != aApplication.Sessions.end() && !found->second->Closing && found->second->ReviewLedgerDecisionGate
			? found->second->ReviewLedgerDecisionGate->pending()
			: 0U;
	}

	namespace {
		auto hold_fixture_effects(application::effect_executor& aEffects) -> std::function<void()>
		{
			struct gate
			{
				std::mutex              Mutex;
				std::condition_variable Changed;
				bool                    Open = false;
			};
			auto held = std::make_shared<gate>();
			aEffects.post([held] {
				auto lock = std::unique_lock{ held->Mutex };
				static_cast<void>(held->Changed.wait_for(lock, std::chrono::seconds{ 30 }, [&held] { return held->Open; }));
			});
			return [held] {
				{
					const auto lock = std::scoped_lock{ held->Mutex };
					held->Open      = true;
				}
				held->Changed.notify_all();
			};
		}

	}

	auto detail::github_application_fixture_access::hold_review_ledger_effects(
		github_application& aApplication) -> std::function<void()>
	{
		return hold_fixture_effects(aApplication.Implementation->ReviewLedgerEffects);
	}

	auto detail::github_application_fixture_access::hold_private_review_effects(
		github_application& aApplication) -> std::function<void()>
	{
		return hold_fixture_effects(aApplication.Implementation->ReviewMutationEffects);
	}

	auto detail::github_application_fixture_access::review_ledger_available(
		github_application& aApplication) -> std::optional<bool>
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr) {
			return std::nullopt;
		}
		return implementation->ReviewLedgerHost->available();
	}

	auto detail::github_application_fixture_access::review_ledger_file_exclusive(
		github_application& aApplication) -> std::optional<bool>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->PrivateReview == nullptr || !implementation->AgentReview.has_value()) {
			return std::nullopt;
		}
		const auto laneIdle = [implementation] {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 10 };
			while (!implementation->ReviewLedgerEffects.idle()) {
				if (std::chrono::steady_clock::now() >= deadline) {
					return false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
			}
			return true;
		};
		if (!laneIdle()) {
			return std::nullopt;
		}

		// A ledger store task running keeps the private store waiting.
		struct held_task
		{
			std::mutex              Mutex;
			std::condition_variable Changed;
			bool                    Started  = false;
			bool                    Released = false;
		};
		auto held = std::make_shared<held_task>();
		implementation->ReviewLedgerStoreEffects.post([held] {
			auto lock     = std::unique_lock{ held->Mutex };
			held->Started = true;
			held->Changed.notify_all();
			static_cast<void>(held->Changed.wait_for(lock, std::chrono::seconds{ 10 }, [&held] { return held->Released; }));
		});
		const auto release = [&held] {
			{
				const auto lock = std::scoped_lock{ held->Mutex };
				held->Released  = true;
			}
			held->Changed.notify_all();
		};
		{
			auto lock = std::unique_lock{ held->Mutex };
			if (!held->Changed.wait_for(lock, std::chrono::seconds{ 10 }, [&held] { return held->Started; })) {
				lock.unlock();
				release();
				return std::nullopt;
			}
		}
		const auto& review        = *implementation->AgentReview;
		auto        answer        = std::async(std::launch::async, [workflow = implementation->PrivateReview, target = core::review_mutation_target{ .Host = review.Host, .Repository = review.Repository, .PullRequest = review.PullRequest }, actor = review.Account] {
			static_cast<void>(workflow->pending(target, actor));
		});
		const auto  privateWaited = answer.wait_for(std::chrono::milliseconds{ 250 }) != std::future_status::ready;
		release();
		if (answer.wait_for(std::chrono::seconds{ 10 }) != std::future_status::ready || !laneIdle()) {
			return std::nullopt;
		}

		// The file exclusion held keeps a ledger store task from finishing.
		auto ledgerWaited = false;
		{
			const auto file = std::scoped_lock{ implementation->ReviewLedgerFile };
			implementation->ReviewLedgerHost->session_catalog([](const auto&) { });
			std::this_thread::sleep_for(std::chrono::milliseconds{ 250 });
			ledgerWaited = !implementation->ReviewLedgerEffects.idle();
		}
		if (!laneIdle()) {
			return std::nullopt;
		}
		return privateWaited && ledgerWaited;
	}

	auto detail::github_application_fixture_access::review_ledger_round_trip(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr) {
			return false;
		}
		if (implementation->ReviewLedgerRoundTripPassed) {
			return true;
		}

		auto* const window = implementation->Window.get();
		auto* const view   = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		if (!implementation->LastSelectedDiff.has_value()
			|| !implementation->ReviewLedgerHost->retained().Material.valid()) {
			if (view != nullptr && view->model() != nullptr
				&& view->model()->rowCount() > 0 && !view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
			}
			return false;
		}

		const auto statistics = implementation->ReviewLedgerHost->statistics();
		// [[BPR-0830]]. Not `ReceiptLoadHits`: a queued refresh_shared_ledger re-read of a receipt committed in this process counts there too.
		implementation->ReviewLedgerWasPresentOnEntry
			= statistics.MaterialReceiptHits > 0U;
		const auto& selected = *implementation->LastSelectedDiff;
		const auto  material = core::file_material(selected.Key);
		const auto  before   = implementation->ReviewLedgerHost->retained();
		if (before.File != selected.Request.File.File
			|| before.Material != material) {
			return false;
		}
		if (before.Read == core::file_read_state::unread) {
			if (window->seen_action() == nullptr
				|| !window->seen_action()->isEnabled()) {
				return false;
			}
			window->seen_action()->trigger();
		}
		const auto after = implementation->ReviewLedgerHost->retained();
		implementation->ReviewLedgerRoundTripPassed
			= after.File == before.File && after.Material == before.Material
			&& after.Read != core::file_read_state::unread;
		return implementation->ReviewLedgerRoundTripPassed;
	}

	auto detail::github_application_fixture_access::request_fixture_diff(
		github_application& aApplication,
		const bool          aUseSecondHead) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation != nullptr) {
			++implementation->FixtureDiffHelperInvocations;
		}
		if (
			implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->DiffHost == nullptr) {
			return false;
		}
		const auto& state = implementation->EpochHost->state();
		if (!state.Authority.has_value() || !state.AcceptedComparison.has_value()) {
			return false;
		}
		const auto evidence     = implementation->Fixture->evidence();
		const auto expectedHead = aUseSecondHead ? evidence.HeadTwoOid : evidence.HeadOneOid;
		const auto objectFor    = [&evidence](const core::git_commit_oid aCommit)
			-> std::optional<core::git_blob_oid> {
			const auto hex      = core::canonical_hex(aCommit);
			const auto rendered = hex.view();
			if (rendered == evidence.BaseOid) {
				return core::parse_git_blob_oid(evidence.BaseBlobOid);
			}
			if (rendered == evidence.HeadOneOid) {
				return core::parse_git_blob_oid(evidence.HeadOneBlobOid);
			}
			if (rendered == evidence.HeadTwoOid) {
				return core::parse_git_blob_oid(evidence.HeadTwoBlobOid);
			}
			return std::nullopt;
		};
		if (
			core::canonical_hex(state.AcceptedComparison->Right.Commit).view()
			!= expectedHead) {
			return false;
		}
		const auto leftBlob  = objectFor(state.AcceptedComparison->Left.Commit);
		const auto rightBlob = objectFor(state.AcceptedComparison->Right.Commit);
		const auto options   = core::diff_options{ };
		const auto digest    = core::options_digest(options);
		if (!leftBlob.has_value() || !rightBlob.has_value() || !digest.has_value()) {
			return false;
		}
		const auto file = core::file_identity_of("review.cpp");
		if (!file.valid()) {
			return false;
		}
		implementation->request_diff(
			core::file_comparison_key{
				.Authority   = *state.Authority,
				.Repository  = state.Authority->Repository,
				.PullRequest = state.Authority->PullRequest,
				.LeftAnchor  = state.AcceptedComparison->Left.Identity,
				.RightAnchor = state.AcceptedComparison->Right.Identity,
				.LeftBlob    = leftBlob,
				.RightBlob   = rightBlob,
				.File        = file,
				.Options     = *digest,
				.Algorithm   = core::supported_diff_algorithm_revision,
			},
			*state.AcceptedComparison,
			options,
			// The fixture repository's single file. Named here because the
			// comparison key carries a digest and the syntax definition needs
			// a name, and BPR-0197 made that name a source file so the
			// classifier selects C++ rather than plain text.
			"review.cpp");
		return implementation->DiffHost->state().Pending.has_value();
	}

	auto detail::github_application_fixture_access::reaccept_fixture_revision(
		github_application& aApplication,
		const bool          aChangeHead) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Fixture == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return false;
		}
		auto handoff = clone_without_smart_git(
			*implementation->LastAcceptedHandoff);
		handoff.Snapshot = core::provider_snapshot_id{
			handoff.Snapshot.value() + 1U
		};
		handoff.PullRequestGeneration = core::request_generation{
			handoff.PullRequestGeneration.value() + 1U
		};
		if (aChangeHead) {
			const auto head = core::parse_git_commit_oid(
				implementation->Fixture->evidence().HeadTwoOid);
			if (!head.has_value()) {
				return false;
			}
			handoff.Head = *head;
		}
		if (!application::valid(handoff)) {
			return false;
		}
		implementation->accept(std::move(handoff));
		return true;
	}

	namespace {
		/// \brief [[BPR-1211]]. Test access only: the ledger answers on the
		/// decision lane, so a synchronous fixture question waits for its
		/// answer, bounded, by delivering that lane's events.
		template <typename Answer>
		[[nodiscard]] auto await_ledger_answer(std::optional<Answer>& aAnswer) -> bool
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 10 };
			while (!aAnswer.has_value() && std::chrono::steady_clock::now() < deadline) {
				QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
			}
			return aAnswer.has_value();
		}

		/// \brief Test access only: after a fixture write, the reload its own
		/// change notification queued is also answered before the course goes on,
		/// as it was when the write and its reload were synchronous.
		void await_ledger_quiescence(const application::review_ledger_application_host& aLedger)
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 10 };
			for (auto quiet = 0; quiet < 3 && std::chrono::steady_clock::now() < deadline;) {
				QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
				quiet = aLedger.is_quiescent() ? quiet + 1 : 0;
			}
		}
	}

	auto detail::github_application_fixture_access::mark_head_reviewed(
		github_application&    aApplication,
		const std::string_view aHeadOid) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		const auto  head           = core::parse_git_commit_oid(aHeadOid);
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| !head.has_value()) {
			return false;
		}
		auto answer = std::make_shared<std::optional<bool>>();
		implementation->ReviewLedgerHost->persist_last_reviewed_head(*head, [answer](const std::expected<void, application::ledger_store_failure>& aStored) {
			*answer = aStored.has_value();
		});
		const auto persisted = await_ledger_answer(*answer) && **answer;
		if (persisted) {
			await_ledger_quiescence(*implementation->ReviewLedgerHost);
			implementation->publish_overview();
		}
		return persisted;
	}

	auto detail::github_application_fixture_access::seed_agent_private_context_canary(
		github_application&    aApplication,
		const std::string_view aSelectedNote,
		const std::string_view aUnselectedNote) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->EpochHost == nullptr || aSelectedNote.empty()
			|| aUnselectedNote.empty()) {
			return false;
		}
		const auto& state = implementation->EpochHost->state();
		if (!state.AcceptedRevision.has_value()) {
			return false;
		}
		const auto selected = application::local_note_record{
			.Id   = application::local_note_id{ 246U },
			.Note = { .Text = std::string{ aSelectedNote } },
		};
		const auto unselected = application::local_note_record{
			.Id   = application::local_note_id{ 247U },
			.Note = { .Text = std::string{ aUnselectedNote } },
		};
		auto       accepted = std::make_shared<std::size_t>(0U);
		auto       answered = std::make_shared<std::optional<bool>>();
		const auto count    = [accepted, answered](const bool aStored) {
			*accepted += aStored ? 1U : 0U;
			if (!aStored || *accepted == 3U) {
				*answered = aStored;
			}
		};
		auto* const ledger = implementation->ReviewLedgerHost.get();
		ledger->persist(selected, std::nullopt, [count](const auto& aStored) { count(aStored.has_value()); });
		ledger->persist(unselected, std::nullopt, [count](const auto& aStored) { count(aStored.has_value()); });
		ledger->persist_last_reviewed_head(state.AcceptedRevision->Head, [count](const auto& aStored) { count(aStored.has_value()); });
		const auto persisted = await_ledger_answer(*answered) && **answered;
		if (persisted && implementation->ReviewLedgerHost != nullptr) {
			await_ledger_quiescence(*implementation->ReviewLedgerHost);
		}
		if (persisted) {
			implementation->refresh_agent_private_context_choices();
		}
		return persisted;
	}

	auto detail::github_application_fixture_access::agent_invocation_context(
		github_application& aApplication)
		-> std::optional<detail::agent_invocation_context_evidence>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->EpochHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| implementation->Window == nullptr
			|| !implementation->AgentReview.has_value()
			|| !implementation->SelectedDiffPublication.has_value()) {
			return std::nullopt;
		}
		const auto& state    = implementation->EpochHost->state();
		const auto& selected = *implementation->SelectedDiffPublication;
		if (
			!state.Authority.has_value() || !state.AcceptedRevision.has_value()
			|| !state.AcceptedComparison.has_value()) {
			return std::nullopt;
		}
		auto receipt = std::optional<core::file_receipt>{ };
		{
			const auto lock = std::scoped_lock{
				implementation->ReviewLedgerStateMutex
			};
			receipt = implementation->ReviewLedgerReceipt;
		}
		const auto privateState
			= implementation->ReviewLedgerHost->private_observation();
		auto sessionAnswer = std::make_shared<std::optional<std::expected<application::agent_session_catalog_state, application::ledger_store_failure>>>();
		implementation->ReviewLedgerHost->session_catalog([sessionAnswer](auto aCatalog) { *sessionAnswer = std::move(aCatalog); });
		if (!await_ledger_answer(*sessionAnswer)) {
			return std::nullopt;
		}
		const auto& sessionCatalog = **sessionAnswer;
		auto* const workbench      = implementation->Window->comparison_surface();
		if (
			!receipt.has_value() || !privateState.has_value()
			|| !sessionCatalog.has_value() || !sessionCatalog->Next.has_value()
			|| receipt->File != selected.Request.File.File || workbench == nullptr
			|| !workbench->selection().has_value()
			|| workbench->current_result() == nullptr
			|| workbench->current_result()->Alignment.Key != selected.Key
			|| workbench->selection()->Comparison
				!= workbench->current_result()->Alignment.Digest) {
			return std::nullopt;
		}
		const auto& authority  = *state.Authority;
		const auto& revision   = *state.AcceptedRevision;
		const auto& comparison = *state.AcceptedComparison;
		return detail::agent_invocation_context_evidence{
			.ApplicationSession = state.ApplicationSession,
			.Authority          = authority,
			.NextSession        = *sessionCatalog->Next,
			.Binding            = {
				.Host        = authority.Host,
				.Repository  = authority.Repository,
				.PullRequest = authority.PullRequest,
				.BaseOid     = revision.Base,
				.HeadOid     = revision.Head,
				.LeftEpoch   = comparison.Left.Identity.Epoch,
				.RightEpoch  = comparison.Right.Identity.Epoch,
			},
			.PullRequestUrl = "https://" + implementation->AgentReview->Hostname + "/" + implementation->AgentReview->Locator.Owner + "/" + implementation->AgentReview->Locator.Name + "/pull/" + std::to_string(implementation->AgentReview->Number.value()),
			.Files          = {
				application::agent_file_context_input{
					.Path               = selected.Request.File.PresentationPath,
					.Receipt            = *receipt,
					.ChangedSinceReview = privateState->LastReviewedHead.has_value() && *privateState->LastReviewedHead != revision.Head,
				},
			},
			.Private   = *privateState,
			.Selection = core::supplement_semantic_context{
				.File       = selected.Request.File.File,
				.Selection  = *workbench->selection(),
				.Confidence = core::supplement_mapping_confidence::exact,
				.Segments   = workbench->current_result()->Alignment.Segments,
				.Groups     = workbench->current_result()->Alignment.Groups,
			},
			.CurrentDraft = implementation->current_agent_draft_observation(),
		};
	}

	auto detail::github_application_fixture_access::window_application_session(
		const github_application&      aApplication,
		const core::bpr_window_context aContext) noexcept
		-> std::optional<core::application_session_id>
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found == aApplication.Sessions.end() || found->second->Closing) {
			return std::nullopt;
		}
		return found->second->WindowSession;
	}

	auto detail::github_application_fixture_access::agent_session_catalog(
		github_application& aApplication)
		-> std::optional<detail::agent_session_catalog_evidence>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr) {
			return std::nullopt;
		}
		auto answer = std::make_shared<std::optional<std::expected<application::agent_session_catalog_state, application::ledger_store_failure>>>();
		implementation->ReviewLedgerHost->session_catalog([answer](auto aCatalog) { *answer = std::move(aCatalog); });
		if (!await_ledger_answer(*answer) || !answer->value().has_value()) {
			return std::nullopt;
		}
		auto catalog = std::move(answer->value());
		return detail::agent_session_catalog_evidence{
			.Catalog    = std::move(*catalog),
			.Statistics = implementation->ReviewLedgerHost->statistics(),
		};
	}

	#if defined(BPR_AGENT_INTEGRATIONS)
	auto detail::github_application_fixture_access::finding_queue(
		const github_application& aApplication) noexcept -> qt::finding_queue_view*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation == nullptr ? nullptr
										 : implementation->FindingQueue;
	}

	auto detail::github_application_fixture_access::side_chat_panel(
		const github_application& aApplication) noexcept -> qt::side_chat_panel*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation == nullptr ? nullptr
										 : implementation->SideChat;
	}

	auto detail::github_application_fixture_access::start_mcp_context_helper(
		github_application& aApplication, std::string aRequest)
		-> application::mcp_helper_refusal
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->McpHelper == nullptr) {
			return application::mcp_helper_refusal::no_helper;
		}
		return implementation->McpHelper->start(std::move(aRequest));
	}

	auto detail::github_application_fixture_access::agent_chat_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_chat_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		auto evidence = detail::agent_chat_composition_evidence{
			.ChatHostComposed      = implementation->ChatHost != nullptr,
			.SideChatPanelComposed = implementation->SideChat != nullptr,
			.Intents               = implementation->SideChatIntents,
			.Presentations         = implementation->SideChatPresentations,
			.Turns                 = implementation->SideChatTurns,
			.RefusalsPresented     = implementation->SideChatRefusals,
			.Statistics            = implementation->ChatHost != nullptr
				? implementation->ChatHost->statistics()
				: application::agent_chat_session_statistics{ },
		};
		evidence.LastTurn         = implementation->LastChatTurn;
		evidence.LastConversation = implementation->LastAgentConversation;
		if (implementation->ChatHost != nullptr) {
			evidence.Session    = implementation->ChatHost->session();
			evidence.Observed   = implementation->ChatHost->observed();
			evidence.Projection = implementation->ChatHost->projection();
		}
		if (implementation->SideChat != nullptr) {
			evidence.SendEnabled
				= implementation->SideChat->send_action() != nullptr
					&& implementation->SideChat->send_action()->isEnabled()
				? "enabled"
				: "disabled";
			evidence.SessionStateText
				= implementation->SideChat->session_state_text().toStdString();
			evidence.HeadOidText
				= implementation->SideChat->head_oid_text().toStdString();
		}
		if (implementation->Window != nullptr) {
			if (auto* const panel = implementation->Window->agent_panel();
				panel != nullptr) {
				evidence.AgentPanelStatusText
					= panel->status_text().toStdString();
			}
		}
		return evidence;
	}

	auto detail::github_application_fixture_access::dispatch_side_chat_send(
		github_application& aApplication, std::string aText) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->SideChat == nullptr
			|| implementation->ChatHost == nullptr) {
			return false;
		}
		const auto shown = implementation->ChatHost->projection();
		if (!shown.has_value()) {
			return false;
		}
		implementation->dispatch_side_chat_intent(qt::send_chat_message_intent{
			.Session = shown->Session.Id,
			.Action  = core::agent_action_intent{ shown->EntryPoint },
			.Scope   = shown->Chat.Scope,
			.Text    = std::move(aText),
		});
		return true;
	}

	auto detail::github_application_fixture_access::
		dispatch_side_chat_private_context(
			github_application&          aApplication,
			const core::agent_session_id aSession,
			const bool                   aNotes,
			const bool                   aDrafts) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ChatHost == nullptr) {
			return false;
		}
		implementation->dispatch_side_chat_intent(qt::chat_private_context_intent{
			.OptIn = {
				.Session = aSession,
				.Notes   = aNotes,
				.Drafts  = aDrafts,
			},
		});
		return true;
	}

	auto detail::github_application_fixture_access::agent_finding_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_finding_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		auto evidence = detail::agent_finding_composition_evidence{
			.FindingHostComposed = implementation->FindingHost != nullptr,
			.CommentWorkshopComposed
			= implementation->WorkshopHost != nullptr,
			.McpHelperComposed     = implementation->McpHelper != nullptr,
			.SideChatPanelComposed = implementation->SideChat != nullptr,
			.FindingQueueComposed  = implementation->FindingQueue != nullptr,
			.QueuedFindings        = implementation->FindingQueue != nullptr
				? implementation->FindingQueue->finding_count()
				: 0U,
			.WorkshopProposals     = implementation->CommentWorkshopProposals,
			.RefusalsPresented     = implementation->FindingRefusalsPresented,
			.SideChatPresentations = implementation->SideChatPresentations,
			.SideChatIntents       = implementation->SideChatIntents,
			.FrozenSnapshotWrites  = implementation->FrozenSnapshotWrites,
			.FrozenFileCount       = implementation->frozen_snapshot_files().size(),
			.FindingStatistics     = implementation->FindingHost != nullptr
				? implementation->FindingHost->statistics()
				: application::agent_finding_host_statistics{ },
			.WorkshopStatistics    = implementation->WorkshopHost != nullptr
				? implementation->WorkshopHost->statistics()
				: application::agent_comment_workshop_statistics{ },
			.McpStatistics         = implementation->McpHelper != nullptr
				? implementation->McpHelper->statistics()
				: application::mcp_helper_supervisor_statistics{ },
			.McpLaunch             = implementation->McpHelper != nullptr
				? implementation->McpHelper->describe()
				: std::nullopt,
			.McpAnswer             = implementation->McpHelper != nullptr
				? implementation->McpHelper->answer()
				: std::string{ },
			.LastContinuationRealignment
			= implementation->LastContinuationRealignment,
			.LastContinuationRealignmentRetargeted
			= implementation->LastContinuationRealignmentRetargeted,
		};
		if (implementation->FindingQueue != nullptr) {
			evidence.QueuedClaim
				= implementation->FindingQueue->claim_text().toStdString();
			evidence.QueuedAnchorState
				= implementation->FindingQueue->anchor_state_text().toStdString();
		}
		if (implementation->ReviewDraft.has_value()) {
			evidence.LocalDraftBody = implementation->ReviewDraft->Body;
		}
		if (implementation->PendingWorkshopOperation.has_value()) {
			evidence.WorkshopPendingOperation = std::string{ core::stable_name(
				implementation->PendingWorkshopOperation->Operation) };
			evidence.WorkshopPendingSessionBound
				= implementation->PendingWorkshopOperation->Session.has_value();
		}
		if (implementation->Window != nullptr) {
			if (auto* const editor = implementation->Window->review_editor();
				editor != nullptr) {
				evidence.WorkshopOriginalText
					= editor->workshop_original_text().toStdString();
				evidence.WorkshopProposalText
					= editor->workshop_proposal_text().toStdString();
				evidence.WorkshopOperationText
					= editor->workshop_operation_text().toStdString();
			}
			if (auto* const panel = implementation->Window->agent_panel();
				panel != nullptr) {
				evidence.AgentPanelStatusText
					= panel->status_text().toStdString();
			}
		}
		if (implementation->SideChat != nullptr) {
			evidence.SideChatHeadOidText
				= implementation->SideChat->head_oid_text().toStdString();
		}
		return evidence;
	}

	#endif
	auto detail::github_application_fixture_access::agent_invocation_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_invocation_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
	#if defined(BPR_AGENT_INTEGRATIONS)
		return {
			.AgentProcessLauncherComposed = implementation->AgentProcess != nullptr,
			// [[BPR-1900]]. The tour session's runs are counted with the
			// Agent tab's: a course asks whether an agent ran and finished.
			.AgentProcessLauncherInvocations
			= (implementation->AgentProcessProbe != nullptr
				   ? implementation->AgentProcessProbe->invocations()
				   : 0U)
				+ (implementation->TourProcessProbe != nullptr ? implementation->TourProcessProbe->invocations() : 0U),
			.HostComposed = implementation->AgentHost != nullptr,
			.Quiescent    = (implementation->AgentHost == nullptr
							 || implementation->AgentHost->is_quiescent())
				&& (implementation->TourSession == nullptr || implementation->TourSession->is_quiescent()),
			.Statistics    = implementation->AgentHost != nullptr
				? implementation->AgentHost->statistics()
				: application::agent_invocation_host_statistics{ },
			.WorkspaceRoot = std::filesystem::path{ implementation->LastAgentWorkspacePath }.parent_path(),
		};
	#else
		// [[BPR-1179]]. Nothing to observe: no launcher and no host are composed.
		return {
			.Quiescent     = true,
			.WorkspaceRoot = std::filesystem::path{ implementation->LastAgentWorkspacePath }.parent_path(),
		};
	#endif
	}
#endif
}
