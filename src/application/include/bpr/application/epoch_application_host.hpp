#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP

#include "bpr/application/cached_review_source_port.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/profile_epoch_actor_registry.hpp"
#include "bpr/application/source_corpus_port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace bpr::application {
	/// \brief Provides the epoch application host contract.
	/// \brief What one preparation of a window found. [[BPR-1327]]
	struct epoch_prepared_comparisons
	{
		/// The available resolutions, in request order.
		std::vector<workbench_selected_diff_observation> Observations;
		/// Blobs read and retained, and the batched reads that carried them.
		std::size_t RemoteBlobs   = 0U;
		std::size_t RemoteBatches = 0U;
	};

	class epoch_application_host final
		: public github_epoch_handoff_port,
		  public cached_review_source_port,
		  public diff_blob_port,
		  public workbench_selected_diff_port,
		  public agent_workspace_port,
		  public source_corpus_port
	{
	public:
		// Semantic ingress and state/quiescence inspection are confined to the
		// serialized decision lane supplied at construction. materialize() and
		// materialize_both() are the sole concurrent ingress seam: diff workers
		// enter through a close-aware call gate and the bounded job ledger.
		// Worker effects otherwise communicate only by posting completions to
		// the decision lane.
		// shutdown() closes ingress without waiting. Composition keeps every
		// referenced port, executor, and projection alive until is_quiescent().
		epoch_application_host(
			core::epoch_state                    aInitialState,
			git_cache_port&                      aCaches,
			epoch_catalog_port&                  aCatalogs,
			epoch_entropy_port&                  aEntropy,
			epoch_clock_port&                    aClock,
			effect_executor&                     aEffects,
			decision_executor&                   aDecisionLane,
			epoch_projection_port&               aProjections,
			epoch_host_limits                    aLimits        = { },
			std::shared_ptr<diagnostic_log_port> aDiagnosticLog = { });
		/// \brief Attaches a window to its externally owned profile cache actors.
		/// \details Construction refuses stale/foreign contexts. Shutdown retires
		/// concurrent ingress before catalog removal; all referenced ports and the
		/// registry must outlive host quiescence. Catalog sessions remain window-local.
		epoch_application_host(
			core::epoch_state                    aInitialState,
			profile_epoch_actor_registry&        aRegistry,
			core::bpr_window_context             aWindow,
			epoch_catalog_port&                  aCatalogs,
			epoch_entropy_port&                  aEntropy,
			epoch_clock_port&                    aClock,
			effect_executor&                     aEffects,
			decision_executor&                   aDecisionLane,
			epoch_projection_port&               aProjections,
			epoch_host_limits                    aLimits        = { },
			std::shared_ptr<diagnostic_log_port> aDiagnosticLog = { });

		~epoch_application_host() override;

		epoch_application_host(const epoch_application_host&) = delete;
		auto operator=(const epoch_application_host&)
			-> epoch_application_host&                   = delete;
		epoch_application_host(epoch_application_host&&) = delete;
		auto operator=(epoch_application_host&&)
			-> epoch_application_host& = delete;

		[[nodiscard]] auto read_cached_source(cached_review_source_request aRequest, std::stop_token aStop, cached_review_source_completion aCompletion) -> bool override;
		void               accept(accepted_pull_request_handoff aHandoff) override;
		// The issuer revokes its lease before notification. The epoch host also
		// revokes its retained handle idempotently before dispatching core
		// invalidation and canceling old-generation work.
		void invalidate(
			pull_request_authority_invalidation aInvalidation) override;
		void dispatch(epoch_intent aIntent);
		// The diff worker calls this synchronous concurrent seam. It is serialized
		// onto the same retained authority/cache actor used by epoch work, so no
		// competing lifetime-exclusive Git cache session is opened. shutdown()
		// closes new calls, cancels admitted work, and quiescence includes calls
		// that entered before closure.
		[[nodiscard]] auto materialize(
			const diff_blob_request& aRequest,
			std::stop_token          aStop) -> diff_blob_observation override;
		/// [[BPR-1360]]. Both sides of the reviewer's selected file in one
		/// actor job, their network reads on the wire together; absent for
		/// any other pair, which is read one side at a time.
		[[nodiscard]] auto materialize_both(
			const diff_blob_request& aLeft,
			const diff_blob_request& aRight,
			std::stop_token          aStop) -> std::optional<std::pair<diff_blob_observation, diff_blob_observation>> override;
		[[nodiscard]] auto materialize_agent_workspace(
			const agent_workspace_materialization_request& aRequest,
			std::stop_token                                aStop)
			-> std::expected<owned_agent_workspace, agent_workspace_error> override;
		[[nodiscard]] auto cleanup_agent_workspace(
			const owned_agent_workspace& aWorkspace,
			std::stop_token              aStop)
			-> std::expected<void, agent_workspace_error> override;
		[[nodiscard]] auto context(
			const workbench_pull_request_authority& aAuthority) const
			-> std::optional<workbench_selected_diff_context> override;
		[[nodiscard]] auto resolve(
			const workbench_selected_diff_request& aRequest,
			std::stop_token                        aStop)
			-> workbench_selected_diff_observation override;
		/// \brief Reads one sealed pinned-source endpoint without a selected-file grant.
		/// \details The observation must belong to the currently authorized identical-commit
		/// re-alignment. The call alone authorizes its exact tree-proved blob, at most 256 KiB;
		/// it creates no grant usable by ordinary diff reads. Runs on a source worker.
		[[nodiscard]] auto materialize_pinned_source_blob(
			const workbench_selected_diff_observation& aSource,
			const diff_blob_request& aRequest, std::stop_token aStop) -> diff_blob_observation;
		/// \brief Prepares the files the reviewer is about to open.
		///
		/// \details [[BPR-1327]]. A synchronous concurrent seam like
		/// `resolve`, for one bounded window of requests sharing one held
		/// comparison authority, in the order given. Each file is resolved on
		/// the repository actor exactly as an open would resolve it, and kept
		/// as a prepared grant, so opening it later runs no Git and its blobs
		/// may be materialized for preparation. For a sparse review, the
		/// grants' missing blobs are then read in batches through a transport
		/// that supports them, off the actor, and their verified bytes kept for
		/// the authority. Replacement, invalidation and shutdown drop both.
		/// [[BPR-1359]]. A prepared blob read on its own on the actor yields to
		/// the reviewer's resolve or read there, and is not made at all while
		/// the authority's batched read is failing.
		[[nodiscard]] auto prepare(
			const std::vector<workbench_selected_diff_request>& aRequests,
			std::stop_token                                     aStop) -> epoch_prepared_comparisons;
		/// \brief Authorizes one re-alignment of an older pair of this review.
		///
		/// \details BPR-0532. `context` above answers for the comparison the
		/// reviewer is looking at; this answers for the pair an agent session
		/// the reviewer kept was started against, so a turn continued after
		/// the head moved can be resolved through the same port as a fresh
		/// one. The returned context is the caller's whole warrant: the host
		/// mints it, so no caller names a comparison the host did not decide
		/// to serve, and it is bounded to this review's own host, repository,
		/// and pull request. Authorized beside `context`'s grant rather than
		/// over it, so the reviewer's own selected diff keeps resolving.
		[[nodiscard]] auto realignment_context(
			const workbench_pull_request_authority& aAuthority,
			const core::agent_revision_binding&     aBinding) const
			-> std::optional<workbench_selected_diff_context>;
		/// \brief Resolves which of a review's files differ between two exact commits.
		///
		/// \details [[BPR-0654]]. Admitted only for the accepted review with
		/// `Right` its accepted head and no ingestion in flight, so the head's
		/// objects are retained; `Left` must be a commit the retained cache
		/// verifies. The lookups run on the review's cache actor and the
		/// completion is delivered on the decision lane like every job's, so the
		/// reviewer's selected comparison is neither replaced nor waited on.
		/// Returns false when the request is refused before admission; an
		/// admitted request always completes, with `stale`, `cancelled` or
		/// `failure` when it cannot be answered.
		[[nodiscard]] auto resolve_changed_paths(
			changed_paths_request    aRequest,
			changed_paths_completion aCompletion) -> bool;
		/// \brief RFC-BPR-0089. Answers which paths each first-parent commit
		/// changes, from local tree diffs of the retained objects.
		/// \details Never fetches. A step whose commit or parent tree is not
		/// retained answers `objects_unavailable` for the whole request. A
		/// resolved answer also authorizes filtered diffs of this exact review
		/// to be read at those commits, and only those. Runs on the review's
		/// cache actor like `resolve_changed_paths`, with the same admission and
		/// completion.
		[[nodiscard]] auto resolve_commit_filter_touches(
			commit_filter_touches_request    aRequest,
			commit_filter_touches_completion aCompletion) -> bool;
		/// \brief RFC-BPR-0089 M2 ([[BPR-1692]]). Reads the head's
		/// `.gitattributes` files in the requested directories, for
		/// `linguist-generated`.
		/// \details Looks each up at the accepted head by literal path; reads
		/// at most `core::maximum_generated_attribute_files` blobs of at most
		/// `core::maximum_generated_attribute_file_bytes` and says which bound
		/// stopped it. A sparse review's missing blob is read exactly through
		/// its own lease and checked against its OID; nothing else is fetched.
		/// Runs on the review's cache actor like `resolve_changed_paths`, with
		/// the same admission and completion.
		[[nodiscard]] auto resolve_generated_attributes(
			generated_attributes_request    aRequest,
			generated_attributes_completion aCompletion) -> bool;
		/// \brief [[BPR-1212]]. Answers whether a comparison's left document is
		/// the merge base's version of its file, which GitHub numbers `LEFT`
		/// lines in.
		/// \details Derives the unique merge base of `BaseTip` and `Head` from
		/// the retained objects, never fetching, and compares the file's blob
		/// there with the one at `Left`. Runs on the review's cache actor like
		/// `resolve_changed_paths`, with the same admission and completion.
		[[nodiscard]] auto resolve_github_base_numbering(
			github_base_numbering_request    aRequest,
			github_base_numbering_completion aCompletion) -> bool;
		/// \brief Mints one exact search scope beside the selected-file grant, including older pairs.
		[[nodiscard]] auto bind_source_search(const workbench_pull_request_authority& aAuthority, const core::comparison_pair& aComparison, std::optional<core::document_tab_id> aTab = { }) -> std::optional<core::source_search_scope>;
		[[nodiscard]] auto catalogue(const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation override;
		[[nodiscard]] auto read(const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop, source_corpus_read_needed aStillNeeded) -> source_corpus_material override;
		void               demand_changed() override;
		void               yield_to_source_navigation() override;
		[[nodiscard]] auto scan_stop_token(const core::source_search_scope& aScope) -> std::stop_token override;
		void               shutdown();

		[[nodiscard]] auto state() const noexcept -> const core::epoch_state&;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		/// \brief Returns true only after `shutdown()` once nothing is owed.
		///
		/// \details Requires closing, so it never becomes true while the host
		/// runs ([[BPR-1302]]).
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		/// \brief [[BPR-1302]] Returns true while the host runs and owes nothing.
		///
		/// \details No job, blob call, actor work, turn or deferred ingress is
		/// outstanding. False once closing. A caller that drives the decision
		/// lane waits on this after `accept`, with the lane drained.
		[[nodiscard]] auto is_running_idle() const noexcept -> bool;
		[[nodiscard]] auto outstanding_jobs() const noexcept -> std::size_t;
		/// \brief [[BPR-1344]] The part of `outstanding_jobs` that preparation
		/// admitted: files resolved, and their blobs read, before the reviewer
		/// opens them.
		[[nodiscard]] auto outstanding_preparation_jobs() const noexcept -> std::size_t;
		/// \brief [[BPR-1344]] `outstanding_jobs` less preparation's, read at once.
		[[nodiscard]] auto outstanding_reviewer_jobs() const noexcept -> std::size_t;
		/// \brief [[BPR-1344]] The reviewer's epoch jobs still to run or to
		/// answer. Unlike `outstanding_reviewer_jobs`, finished actor work that
		/// only awaits its retirement is not counted: its completion has
		/// already been posted.
		[[nodiscard]] auto active_reviewer_jobs() const noexcept -> std::size_t;
		[[nodiscard]] auto queued_actor_work() const noexcept -> std::size_t;
		[[nodiscard]] auto deferred_ordinary_ingress() const noexcept
			-> std::size_t;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		[[nodiscard]] static auto mint_selected_diff_endpoint(
			workbench_selected_diff_request aRequest,
			core::diff_side                 aSide,
			core::git_blob_oid              aBlob,
			std::filesystem::path           aPath)
			-> workbench_selected_diff_endpoint;

		struct implementation;
		std::unique_ptr<profile_epoch_actor_registry> OwnedRegistry;
		std::unique_ptr<implementation>               Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_APPLICATION_HOST_HPP
