#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/operation_diagnostics.hpp"
#include "bpr/application/testing.hpp"
#include "bpr/core/comparison_preparation.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/core/filtered_comment.hpp"
#include "bpr/core/git_objects.hpp"
#include "epoch_host_implementation.hpp"
#include "epoch_host_support.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <expected>
#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <ranges>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	auto epoch_application_host::mint_selected_diff_endpoint(
		workbench_selected_diff_request aRequest,
		const core::diff_side           aSide,
		core::git_blob_oid              aBlob,
		std::filesystem::path           aPath)
		-> workbench_selected_diff_endpoint
	{
		return { std::move(aRequest), aSide, std::move(aBlob), std::move(aPath) };
	}

#if defined(BPR_APPLICATION_TESTING)
	auto make_workbench_selected_diff_endpoint_for_testing(
		workbench_selected_diff_request aRequest,
		const core::diff_side           aSide,
		core::git_blob_oid              aBlob,
		std::filesystem::path           aPath)
		-> workbench_selected_diff_endpoint
	{
		return {
			std::move(aRequest), aSide, std::move(aBlob), std::move(aPath)
		};
	}
#endif
} // namespace bpr::application

#if defined(BPR_APPLICATION_TESTING)
namespace bpr::application {
} // namespace bpr::application

namespace bpr::application::detail::testing {
	void fail_next_epoch_job_admission(
		const epoch_admission_allocation_stage aStage) noexcept
	{
		ArmedAdmissionStage.store(
			static_cast<int>(aStage), std::memory_order_release);
	}

	void clear_epoch_job_admission_failure() noexcept
	{
		ArmedAdmissionStage.store(-1, std::memory_order_release);
	}
} // namespace bpr::application::detail::testing
#endif

namespace bpr::application {

	epoch_application_host::epoch_application_host(
		core::epoch_state       aInitialState,
		git_cache_port&         aCaches,
		epoch_catalog_port&     aCatalogs,
		epoch_entropy_port&     aEntropy,
		epoch_clock_port&       aClock,
		effect_executor&        aEffects,
		decision_executor&      aDecisionLane,
		epoch_projection_port&  aProjections,
		const epoch_host_limits aLimits, std::shared_ptr<diagnostic_log_port> aDiagnosticLog)
		: OwnedRegistry{ new profile_epoch_actor_registry{ aCaches, aEffects } }
		, Implementation{ std::make_unique<implementation>(
			  std::move(aInitialState),
			  aCaches,
			  aCatalogs,
			  aEntropy,
			  aClock,
			  aEffects,
			  aDecisionLane,
			  aProjections,
			  aLimits, std::move(aDiagnosticLog), OwnedRegistry->attach_legacy(), true) }
	{
	}

	epoch_application_host::epoch_application_host(
		core::epoch_state aInitialState, profile_epoch_actor_registry& aRegistry,
		const core::bpr_window_context aWindow,
		epoch_catalog_port& aCatalogs, epoch_entropy_port& aEntropy,
		epoch_clock_port& aClock, effect_executor& aEffects,
		decision_executor& aDecisionLane, epoch_projection_port& aProjections,
		const epoch_host_limits aLimits, std::shared_ptr<diagnostic_log_port> aDiagnosticLog)
	{
		auto consumer = aRegistry.attach(aWindow);
		if (!consumer) {
			throw std::invalid_argument{ "Epoch window attachment refused" };
		}
		Implementation = std::make_unique<implementation>(std::move(aInitialState),
														  aRegistry.caches(), aCatalogs, aEntropy, aClock, aEffects, aDecisionLane, aProjections,
														  aLimits, std::move(aDiagnosticLog), std::move(*consumer));
	}

	epoch_application_host::~epoch_application_host()
	{
		shutdown();
	}

	void epoch_application_host::accept(accepted_pull_request_handoff aHandoff)
	{
		Implementation->accept(std::move(aHandoff));
	}

	void epoch_application_host::prepare_runtime()
	{
		Implementation->prepare_runtime();
	}

	void epoch_application_host::invalidate(
		pull_request_authority_invalidation aInvalidation)
	{
		Implementation->invalidate(std::move(aInvalidation));
	}

	void epoch_application_host::dispatch(epoch_intent aIntent)
	{
		Implementation->dispatch(std::move(aIntent));
	}

	auto epoch_application_host::materialize(
		const diff_blob_request& aRequest,
		const std::stop_token    aStop) -> diff_blob_observation
	{
		return Implementation->materialize(aRequest, aStop);
	}

	auto epoch_application_host::materialize_both(
		const diff_blob_request& aLeft,
		const diff_blob_request& aRight,
		const std::stop_token    aStop) -> std::optional<std::pair<diff_blob_observation, diff_blob_observation>>
	{
		return Implementation->materialize_both(aLeft, aRight, aStop);
	}

	auto epoch_application_host::prepare(
		const std::vector<workbench_selected_diff_request>& aRequests,
		const std::stop_token                               aStop) -> epoch_prepared_comparisons
	{
		return Implementation->prepare_selected_diffs(aRequests, aStop);
	}

	auto epoch_application_host::materialize_agent_workspace(
		const agent_workspace_materialization_request& aRequest,
		const std::stop_token                          aStop)
		-> std::expected<owned_agent_workspace, agent_workspace_error>
	{
		// [[BPR-1828]]. "Setting up a workspace" leaves a start and an end.
		auto* const log   = Implementation->DiagnosticLog.get();
		const auto  watch = operation_stopwatch{ };
		record_operation(log, { .Action = core::operation_action::workspace_materialize }, true);
		auto result = Implementation->materialize_agent_workspace(aRequest, aStop);
		record_operation(log, { .Action = core::operation_action::workspace_materialize, .Failure = result ? core::operation_failure::none : operation_failure_of(result.error()), .DurationMilliseconds = watch.milliseconds() });
		return result;
	}

	auto epoch_application_host::keep_managed_clone(
		const managed_clone_request&    aRequest,
		const git_smart_clone_progress& aProgress,
		const std::stop_token           aStop) -> std::expected<void, managed_clone_error>
	{
		return Implementation->keep_managed_clone(aRequest, aProgress, aStop);
	}

	auto epoch_application_host::cleanup_agent_workspace(
		const owned_agent_workspace& aWorkspace,
		const std::stop_token        aStop)
		-> std::expected<void, agent_workspace_error>
	{
		const auto watch  = operation_stopwatch{ };
		auto       result = Implementation->cleanup_agent_workspace(aWorkspace, aStop);
		record_operation(Implementation->DiagnosticLog.get(), { .Action = core::operation_action::workspace_cleanup, .Failure = result ? core::operation_failure::none : operation_failure_of(result.error()), .DurationMilliseconds = watch.milliseconds() });
		return result;
	}

	auto epoch_application_host::context(
		const workbench_pull_request_authority& aAuthority) const
		-> std::optional<workbench_selected_diff_context>
	{
		return Implementation->selected_diff_context(aAuthority);
	}

	auto epoch_application_host::realignment_context(
		const workbench_pull_request_authority& aAuthority,
		const core::agent_revision_binding&     aBinding) const
		-> std::optional<workbench_selected_diff_context>
	{
		return Implementation->authorized_selected_diff_context(
			aAuthority, aBinding);
	}

	auto epoch_application_host::resolve(
		const workbench_selected_diff_request& aRequest,
		const std::stop_token                  aStop)
		-> workbench_selected_diff_observation
	{
		return Implementation->resolve_selected_diff(aRequest, aStop);
	}

	auto epoch_application_host::materialize_pinned_source_blob(
		const workbench_selected_diff_observation& aSource,
		const diff_blob_request& aRequest, std::stop_token aStop) -> diff_blob_observation
	{
		return Implementation->materialize(aRequest, aStop, aSource);
	}

	auto epoch_application_host::read_cached_source(cached_review_source_request aRequest, std::stop_token aStop, cached_review_source_completion aCompletion) -> bool
	{
		return Implementation->read_cached_source(std::move(aRequest), aStop, std::move(aCompletion));
	}

	auto epoch_application_host::resolve_changed_paths(
		changed_paths_request    aRequest,
		changed_paths_completion aCompletion) -> bool
	{
		return Implementation->resolve_changed_paths(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::resolve_commit_filter_touches(
		commit_filter_touches_request    aRequest,
		commit_filter_touches_completion aCompletion) -> bool
	{
		return Implementation->resolve_commit_filter_touches(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::resolve_generated_attributes(
		generated_attributes_request    aRequest,
		generated_attributes_completion aCompletion) -> bool
	{
		return Implementation->resolve_generated_attributes(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::resolve_head_move_lines(
		head_move_lines_request    aRequest,
		head_move_lines_completion aCompletion) -> bool
	{
		return Implementation->resolve_head_move_lines(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::resolve_permalink_source(
		permalink_source_request    aRequest,
		permalink_source_completion aCompletion) -> bool
	{
		return Implementation->resolve_permalink_source(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::resolve_github_diff_lines(
		github_diff_lines_request    aRequest,
		github_diff_lines_completion aCompletion) -> bool
	{
		return Implementation->resolve_github_diff_lines(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::resolve_filtered_comment_lines(
		filtered_comment_lines_request    aRequest,
		filtered_comment_lines_completion aCompletion) -> bool
	{
		return Implementation->resolve_filtered_comment_lines(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::resolve_github_base_numbering(
		github_base_numbering_request    aRequest,
		github_base_numbering_completion aCompletion) -> bool
	{
		return Implementation->resolve_github_base_numbering(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::bind_source_search(const workbench_pull_request_authority& aAuthority, const core::comparison_pair& aComparison, std::optional<core::document_tab_id> aTab,
													std::optional<std::vector<core::source_search_filtered_file>> aFiltered) -> std::optional<core::source_search_scope>
	{
		return Implementation->bind_source_search(aAuthority, aComparison, aTab, std::move(aFiltered));
	}
	auto epoch_application_host::catalogue(const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation
	{
		return Implementation->catalogue(aScope, aStop);
	}
	auto epoch_application_host::read(const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop, source_corpus_read_needed aStillNeeded) -> source_corpus_material
	{
		return Implementation->read_source_corpus(aCorpus, aEntry, aSide, aStop, std::move(aStillNeeded));
	}
	void epoch_application_host::demand_changed()
	{
		std::scoped_lock actorsLock{ Implementation->CacheActorsMutex };
		for (const auto& [key, actor] : Implementation->CacheActors) {
			static_cast<void>(key);
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->SearchRead && !implementation::still_needed(actor->SearchRead->Needed)) {
				actor->SearchRead->Stop.request_stop();
			}
		}
	}
	void epoch_application_host::yield_to_source_navigation()
	{
		std::scoped_lock actorsLock{ Implementation->CacheActorsMutex };
		for (const auto& [key, actor] : Implementation->CacheActors) {
			static_cast<void>(key);
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			actor->SearchScanStop.request_stop();
			if (actor->PreparedRead) {
				actor->PreparedRead->request_stop();
			}
			if (actor->SearchRead) {
				actor->SearchRead->Stop.request_stop();
			}
			if (actor->ReviewerWork == 0U) {
				actor->SearchScanStop = std::stop_source{ };
			}
		}
	}
	auto epoch_application_host::scan_stop_token(const core::source_search_scope& aScope) -> std::stop_token
	{
		const auto actor = Implementation->search_actor(aScope);
		if (!actor) {
			auto stop = std::stop_source{ };
			stop.request_stop();
			return stop.get_token();
		}
		std::scoped_lock lock{ actor->BlobAuthorityMutex };
		return actor->SearchScanStop.get_token();
	}

	void epoch_application_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto epoch_application_host::state() const noexcept -> const core::epoch_state&
	{
		return Implementation->State;
	}

	auto epoch_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto epoch_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->outstanding_work_settled();
	}

	auto epoch_application_host::is_running_idle() const noexcept -> bool
	{
		return !Implementation->Closing && Implementation->outstanding_work_settled();
	}

	auto epoch_application_host::outstanding_jobs() const noexcept -> std::size_t
	{
		return Implementation->job_count();
	}

	auto epoch_application_host::outstanding_preparation_jobs() const noexcept -> std::size_t
	{
		return Implementation->preparation_job_count();
	}

	auto epoch_application_host::outstanding_reviewer_jobs() const noexcept -> std::size_t
	{
		return Implementation->reviewer_job_count();
	}

	auto epoch_application_host::active_reviewer_jobs() const noexcept -> std::size_t
	{
		return Implementation->active_reviewer_job_count();
	}

	auto epoch_application_host::queued_actor_work() const noexcept -> std::size_t
	{
		return Implementation->queued_actor_work();
	}

	auto epoch_application_host::deferred_ordinary_ingress() const noexcept
		-> std::size_t
	{
		return Implementation->DeferredIngress.size();
	}

	auto epoch_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed
			|| Implementation->CompletionGate->DeliveryFailed.load(
				std::memory_order_acquire);
	}
} // namespace bpr::application
