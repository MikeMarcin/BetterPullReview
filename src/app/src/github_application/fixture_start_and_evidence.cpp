#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/device_binding_store.hpp"
	#include "bpr/application/diff_application_host.hpp"
	#include "bpr/application/document_tab_host.hpp"
	#include "bpr/application/epoch_application_host.hpp"
	#include "bpr/application/feedback_counters.hpp"
	#include "bpr/application/github_application_host.hpp"
	#include "bpr/application/github_credential_vault.hpp"
	#include "bpr/application/github_host_authority.hpp"
	#include "bpr/application/github_provider_port.hpp"
	#include "bpr/application/github_publication_port.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/review_ledger_application_host.hpp"
	#include "bpr/application/review_queue_enrollment.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/core/diff_transition.hpp"
	#include "bpr/core/document_tabs.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/profile_window.hpp"
	#include "bpr/core/review_queue.hpp"
	#include "bpr/core/window_review_restoration.hpp"
	#include "bpr/core/workbench_interaction.hpp"
	#include "github_application/blob_sources.hpp"
	#include "github_application/device_authorization_composition.hpp"
	#include "github_application/profile_hosts.hpp"
	#include "github_application/review_mutation_selection.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_review_lanes.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_host_profile.hpp"
	#include "github_public_path_fixture.hpp"
	#include "github_read_contract.hpp"
	#include "github_wire_transport.hpp"
	#include "native_https_transport.hpp"
	#include "review_cycle_desktop.hpp"
	#include <atomic>
	#include <cstddef>
	#include <expected>
	#include <functional>
	#include <map>
	#include <span>
	#include <stop_token>
	#include <string>
	#include <string_view>
	#include <utility>
	#include <variant>
	#include <vector>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::network_isolation_probe()
		-> bool
	{
		using namespace adapters::github::detail;
		const auto profile = adapters::github::validate_github_host_profile(native_profile_candidate(
			core::host_profile_id{ 901U },
			core::git_host_id{ 902U },
			application::github_profile_revision{ 903U },
			application::github_server_contract::ghes_3_18_2022_11_28,
			application::github_authentication_transport::native_https,
			"example.com",
			adapters::github::github_profile_provenance::managed));
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
#endif
}
