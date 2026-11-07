#ifndef BPR_SRC_APPLICATION_SRC_EPOCH_HOST_IMPLEMENTATION_HPP
#define BPR_SRC_APPLICATION_SRC_EPOCH_HOST_IMPLEMENTATION_HPP
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/cached_review_source_port.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/managed_clone.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/profile_epoch_actor_registry.hpp"
#include "bpr/application/source_corpus_port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/document_tabs.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/filtered_comment.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/source_search.hpp"
#include "epoch_host_support.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// The epoch application host's state and its effect, completion and actor
// lanes (epoch_application_host::implementation).
namespace bpr::application {
	struct epoch_application_host::implementation
	{
		enum class prepared_kind : std::uint8_t {
			observation_only,
			append,
		};

		struct prepared_operation final
		{
			prepared_kind                          Kind = prepared_kind::observation_only;
			core::epoch_operation_id               Operation;
			std::optional<append_operation_record> Record;
			std::optional<git_prepared_revision>   Git;
		};

		struct blob_authority final
		{
			core::application_session_id ApplicationSession;
			core::pull_request_authority Authority;

			auto operator<=>(const blob_authority&) const = default;
		};

		struct selected_diff_authority final
		{
			workbench_pull_request_authority Workbench;
			workbench_selected_diff_context  Diff;

			auto operator<=>(const selected_diff_authority&) const = default;
		};

		// One cache-scoped storage actor is retained across revision and pull
		// request context replacement. Its FIFO serializes catalog/cache effects
		// that share the lifetime-exclusive cache session, while each context
		// remains independently discardable.
		struct authority_context;

		/// [[BPR-1327]]. Verified bytes of a blob a sparse cache does not
		/// hold, kept so a second read of it (the classifier's, a revisit) does
		/// not cross the network again.
		struct retained_sparse_blob final
		{
			core::git_blob_oid        Blob;
			std::vector<std::uint8_t> Bytes;
			bool                      Background = false;
		};

		static constexpr std::size_t   maximum_retained_sparse_blobs      = 256U;
		static constexpr std::uint64_t maximum_retained_sparse_blob_bytes = 16U * 1024U * 1024U;

		[[nodiscard]] static auto still_needed(const source_corpus_read_needed& aNeeded) noexcept -> bool;

		struct corpus_read_waiter
		{
			std::mutex                         Mutex;
			std::condition_variable            Changed;
			source_corpus_material             Material;
			bool                               Done = false;
			std::stop_source                   Stop;
			source_corpus_read_needed          Needed;
			core::git_blob_oid                 Blob;
			std::weak_ptr<const source_corpus> Corpus;
		};

		struct authority_effect_actor final
		{
			explicit authority_effect_actor(std::shared_ptr<profile_epoch_actor> aShared);
			std::shared_ptr<profile_epoch_actor>     Shared;
			std::unique_ptr<git_cache_session>&      Cache;
			std::mutex                               BlobAuthorityMutex;
			std::optional<core::source_search_scope> SearchScope;
			/// RFC-BPR-0089 ([[BPR-1688]]). The files a filtered scope names.
			std::vector<core::source_search_filtered_file> SearchFiltered;
			std::shared_ptr<const source_corpus>           SearchCorpus;
			std::shared_ptr<corpus_read_waiter>            SearchRead;
			std::uint64_t                                  SearchGeneration = 0U;
			std::stop_source                               SearchScanStop;
			std::optional<blob_authority>                  BlobAuthority;
			std::weak_ptr<authority_context>               BlobContext;
			std::optional<selected_diff_authority>         SelectedDiffAuthority;
			/// BPR-0532. The one older pair a continued turn may re-align,
			/// held beside the workbench's own authorization rather than in
			/// place of it: a re-alignment and the reviewer's own selection
			/// are two different comparisons of one review, and serving the
			/// first must not stop serving the second. Minted only by the
			/// host, replaced by the next re-alignment, and dropped whenever
			/// the review's context is replaced or revoked.
			std::optional<selected_diff_authority>             RealignmentAuthority;
			std::optional<workbench_selected_diff_observation> SelectedBlobGrant;
			std::uint64_t                                      SelectedBlobGrantSequence = 0U;
			std::uint64_t                                      BlobAuthorityGeneration   = 0U;
			/// [[BPR-1327]]. Resolutions prepared for the files the reviewer is
			/// about to open, under the same selected comparison authority as
			/// SelectedBlobGrant and dropped with it. Each authorizes exactly
			/// the blobs its own tree lookup found, as the selected grant does.
			std::vector<workbench_selected_diff_observation> PreparedGrants;
			std::uint64_t                                    PreparedGrantSequence = 0U;
			std::deque<retained_sparse_blob>                 RetainedBlobs;
			std::uint64_t                                    RetainedBlobBytes = 0U;
			/// [[BPR-1359]]. The stop of preparation's exact read now on this
			/// actor, if one is. Reviewer work stops it before queueing, and
			/// none starts while `ReviewerWork` is non-zero: the reviewer's
			/// own resolve or read is never queued behind a prepared one.
			std::optional<std::stop_source> PreparedRead;
			std::size_t                     ReviewerWork = 0U;
			/// [[BPR-1359]]. The last batched read under this authority failed,
			/// so prepared blobs are not read one at a time either: a transport
			/// whose batch fails reads nothing ahead. A batch that succeeds,
			/// or a new authority, lifts it.
			bool PreparedReadsSuspended = false;
			/// RFC-BPR-0089. The commits a commit-filtered diff may be read at:
			/// every chain commit, and the parent it was diffed against, whose
			/// tree diff a touches job answered for this exact workbench
			/// authority. Sorted. Dropped with the blob authority, so a filtered
			/// request can never name a commit this review did not diff.
			std::optional<workbench_pull_request_authority> FilterAuthority;
			std::vector<core::git_commit_oid>               FilterCommits;
			/// RFC-BPR-0089. A sparse review's chain trees, acquired into a
			/// transient quarantined store for the same authority, and read
			/// only at the filter commits above.
			std::shared_ptr<git_chain_tree_store> ChainTrees;
		};

		static void forget_prepared_grants(authority_effect_actor& aActor) noexcept;

		/// \brief Reviewer work on one actor, from before it is queued until
		/// it has run. [[BPR-1359]]
		///
		/// \details Owned by the queued closure, so it is released when the
		/// work has run or is dropped unrun. Constructing it stops the
		/// prepared exact read in flight, whose `gh` process is then killed,
		/// as `cancel_preparation` kills the batch's.
		class reviewer_work final
		{
		public:
			explicit reviewer_work(std::shared_ptr<authority_effect_actor> aActor);

			~reviewer_work();

			reviewer_work(const reviewer_work&)                    = delete;
			auto operator=(const reviewer_work&) -> reviewer_work& = delete;
			reviewer_work(reviewer_work&&)                         = delete;
			auto operator=(reviewer_work&&) -> reviewer_work&      = delete;

		private:
			std::shared_ptr<authority_effect_actor> Actor;
		};

		/// \brief Preparation's exact read of one blob, on the actor, for as
		/// long as it runs. [[BPR-1359]]
		///
		/// \details Admitted only when no reviewer work is queued on the actor
		/// and the authority's batched read is not failing; while admitted,
		/// its stop is where reviewer work finds it.
		class prepared_read_registration final
		{
		public:
			prepared_read_registration(authority_effect_actor& aActor, const std::stop_source& aStop, const bool aHonorSuspension = true);

			~prepared_read_registration();

			prepared_read_registration(const prepared_read_registration&)                    = delete;
			auto operator=(const prepared_read_registration&) -> prepared_read_registration& = delete;
			prepared_read_registration(prepared_read_registration&&)                         = delete;
			auto operator=(prepared_read_registration&&) -> prepared_read_registration&      = delete;

			[[nodiscard]] auto admitted() const noexcept -> bool;

			[[nodiscard]] auto suspended() const noexcept -> bool;

		private:
			authority_effect_actor& Actor;
			bool                    Admitted  = false;
			bool                    Suspended = false;
		};

		static void forget_retained_blobs(authority_effect_actor& aActor) noexcept;

		static void retain_blob(authority_effect_actor& aActor, const core::git_blob_oid& aBlob,
								const std::vector<std::uint8_t>& aBytes, const bool aBackground = false);

		[[nodiscard]] static auto retained_blob(const authority_effect_actor& aActor, const core::git_blob_oid& aBlob)
			-> std::optional<std::vector<std::uint8_t>>;

		struct blob_waiter final
		{
			std::mutex                           Mutex;
			std::condition_variable              Completed;
			std::optional<diff_blob_observation> Result;
			// Set when the caller's stop token fires. The per-job stop source is
			// wired inside the enqueued closure, so a stop requested while the
			// work is still queued behind the actor's FIFO could not reach it and
			// the wait was deaf to cancellation. BPR-0390.
			bool Abandoned = false;
		};

		/// \brief Where a cold open's caller waits for both sides. [[BPR-1360]]
		///
		/// \details Besides the result, the actor may lend the caller one
		/// side's network read to make while it waits, so the two reads are on
		/// the wire together without another thread. A read offered and not
		/// yet taken when the caller stops waiting goes back to the actor.
		struct blob_pair_waiter final
		{
			enum class lent_read : std::uint8_t {
				none,
				offered,
				taken,
				returned,
			};

			std::mutex                                                             Mutex;
			std::condition_variable                                                Changed;
			std::optional<std::pair<diff_blob_observation, diff_blob_observation>> Result;
			std::function<void()>                                                  Lent;
			lent_read                                                              Lend = lent_read::none;
			/// The caller's stop token fired, as `blob_waiter::Abandoned`.
			bool Abandoned = false;
			/// The caller stopped waiting, stopped or at its bound.
			bool Gone = false;
		};

		/// \brief One blob read on a repository actor, in three steps.
		/// [[BPR-1360]]
		///
		/// \details `begin_actor_blob_read` does the actor's part up to the
		/// network: the cache, the grants, retained bytes, the durable sparse
		/// proof and the transport. `read_remote_blob` makes the one exact
		/// network read still owed, if any; it touches no cache session, so a
		/// cold open may lend it to the diff worker waiting for the result
		/// while the actor reads the other side. `finish_actor_blob_read`
		/// rechecks the authority under the actor's lock and publishes.
		struct actor_blob_read final
		{
			explicit actor_blob_read(const diff_blob_request& aRequest);

			actor_blob_read(const actor_blob_read&)                    = delete;
			auto operator=(const actor_blob_read&) -> actor_blob_read& = delete;
			actor_blob_read(actor_blob_read&&)                         = delete;
			auto operator=(actor_blob_read&&) -> actor_blob_read&      = delete;
			~actor_blob_read()                                         = default;

			const diff_blob_request&                                   Request;
			diff_blob_observation                                      Observation;
			bool                                                       Opened = false;
			std::optional<git_cache_result<std::vector<std::uint8_t>>> Cached;
			std::optional<std::vector<std::uint8_t>>                   Bytes;
			std::optional<std::uint64_t>                               SelectedGrantSequence;
			std::optional<std::uint64_t>                               PreparedGrantSequence;
			std::optional<workbench_selected_diff_observation>         PinnedSource;
			bool                                                       FetchedRemotely   = false;
			bool                                                       RetainedOverLimit = false;
			/// [[BPR-1359]]. Held from admission until the network read ends.
			std::optional<prepared_read_registration>   PreparedRead;
			git_smart_transport                         Transport;
			std::optional<git_smart_exact_blob_request> Remote;
		};

		struct selected_diff_waiter final
		{
			std::mutex                                         Mutex;
			std::condition_variable                            Completed;
			std::optional<workbench_selected_diff_observation> Result;
			// See `blob_waiter::Abandoned`. This wait carried the identical
			// unbounded shape until BPR-0397.
			bool Abandoned = false;
		};

		struct workspace_waiter final
		{
			std::mutex              Mutex;
			std::condition_variable Completed;
			std::optional<
				std::expected<owned_agent_workspace, agent_workspace_error>>
				Result;
			// See `blob_waiter::Abandoned`. Abandoning this one is not free the
			// way abandoning a read is: if the work later succeeds it has written
			// a workspace to disk that nobody owns, so the closure consults this
			// flag under the mutex and cleans up rather than leaking. BPR-0397.
			bool Abandoned = false;
			// [[BPR-1772]]. The work was asked for the linked clone's head
			// only, and the clone does not hold it.
			bool CloneLacksHead = false;
		};

		struct workspace_cleanup_waiter final
		{
			std::mutex                                                Mutex;
			std::condition_variable                                   Completed;
			std::optional<std::expected<void, agent_workspace_error>> Result;
			// See `blob_waiter::Abandoned`. Abandoning a cleanup is safe: the
			// removal still runs, and only its outcome goes unread. BPR-0397.
			bool Abandoned = false;
		};

		struct authority_context final
		{
			authority_context(
				accepted_pull_request_handoff                 aAccepted,
				core::pull_request_authority                  aAuthority,
				core::accepted_pull_request_revision          aRevision,
				std::unique_ptr<epoch_catalog_session>        aCatalog,
				std::shared_ptr<authority_effect_actor>       aActor,
				std::optional<core::epoch_occurrence_history> aOccurrenceHistory,
				std::optional<core::review_epoch_summary>     aCurrentEpoch);

			~authority_context();

			[[nodiscard]] auto store(prepared_operation aOperation) -> bool;

			[[nodiscard]] auto take(const core::epoch_operation_id aOperation)
				-> std::optional<prepared_operation>;

			[[nodiscard]] auto requires_cache_cleanup(
				const std::optional<core::epoch_operation_id> aExpected = std::nullopt)
				const noexcept -> bool;

			accepted_pull_request_handoff                 Handoff;
			core::pull_request_authority                  Authority;
			core::accepted_pull_request_revision          Revision;
			std::unique_ptr<epoch_catalog_session>        Catalog;
			std::shared_ptr<authority_effect_actor>       Actor;
			std::optional<core::epoch_occurrence_history> OccurrenceHistory;
			std::optional<core::review_epoch_summary>     CurrentEpoch;
			std::vector<prepared_operation>               Preparations;
			bool                                          HistoricalImportPending = false;
			std::uint8_t                                  HistoricalImports       = 0U;
			std::vector<core::epoch_occurrence_id>        HistoricalSkipped;
		};

		struct completion_gate final
		{
			std::recursive_mutex         Barrier;
			std::atomic<implementation*> Host           = nullptr;
			std::atomic_bool             DeliveryFailed = false;
		};

		/// [[BPR-1344]]. Preparation is true for a job preparation admitted, not
		/// the reviewer; it is set before the job is published and never changes.
		struct job_control final
		{
			std::stop_source                           Stop;
			std::optional<core::epoch_request_id>      Request;
			std::optional<core::epoch_generation>      EpochGeneration;
			std::optional<core::comparison_generation> ComparisonGeneration;
			std::optional<std::uint64_t>               OfferToken;
			bool                                       MandatoryCleanup   = false;
			bool                                       CancellationIssued = false;
			bool                                       Preparation        = false;
			std::function<void(implementation&)>       Completion;
			std::function<void()>                      DecisionTask;
		};

		struct pending_context_cleanup final
		{
			std::shared_ptr<authority_context> Context;
			std::shared_ptr<job_control>       Control;
		};

		struct deferred_accept final
		{
			std::shared_ptr<accepted_pull_request_handoff> Handoff;
		};
		struct deferred_dispatch final
		{
			epoch_intent Intent;
		};
		using deferred_ingress  = std::variant<deferred_accept, deferred_dispatch>;
		using worker_completion = std::function<void(implementation&)>;
		using worker_task       = std::function<worker_completion(std::stop_token)>;
		using admission_failure = std::function<void(implementation&, core::epoch_failure)>;

		implementation(
			core::epoch_state       aInitialState,
			git_cache_port&         aCaches,
			epoch_catalog_port&     aCatalogs,
			epoch_entropy_port&     aEntropy,
			epoch_clock_port&       aClock,
			effect_executor&        aEffects,
			decision_executor&      aDecisionLane,
			epoch_projection_port&  aProjections,
			const epoch_host_limits aLimits, std::shared_ptr<diagnostic_log_port> aDiagnosticLog,
			std::shared_ptr<profile_epoch_actor_consumer> aConsumer, const bool aLegacy = false);

		~implementation();

		[[nodiscard]] static auto blob_authority_for(
			const authority_context& aContext) noexcept -> blob_authority;

		[[nodiscard]] static auto blob_authority_for(
			const workbench_selected_diff_request& aRequest) noexcept
			-> blob_authority;

		[[nodiscard]] static auto blob_authority_for(
			const diff_blob_request& aRequest) noexcept -> blob_authority;

		[[nodiscard]] static auto blob_authority_for(
			const agent_workspace_materialization_request& aRequest) noexcept
			-> blob_authority;

		[[nodiscard]] static auto exact_selected_blob_request(
			const workbench_selected_diff_observation& aGrant,
			const diff_blob_request&                   aRequest) noexcept -> bool;

		static void authorize_blobs(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const blob_authority&                          aAuthority,
			const std::shared_ptr<authority_context>&      aContext) noexcept;

		static void authorize_selected_diff(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const selected_diff_authority&                 aAuthority) noexcept;

		static void authorize_realignment(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const selected_diff_authority&                 aAuthority) noexcept;

		static void revoke_blob_authority(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const blob_authority&                          aAuthority) noexcept;

		[[nodiscard]] static auto selected_diff_authorization_generation(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const workbench_selected_diff_request&         aRequest) noexcept
			-> std::optional<std::uint64_t>;

		[[nodiscard]] static auto blob_authorization_generation(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const blob_authority&                          aAuthority) noexcept
			-> std::optional<std::uint64_t>;

		static void revoke_deferred(deferred_ingress& aIngress) noexcept;

		void clear_deferred_ingress() noexcept;

		void defer_diagnostic(const core::epoch_reason_code aReason) noexcept;

		[[nodiscard]] auto defer_ordinary(deferred_ingress aIngress) noexcept
			-> bool;

		[[nodiscard]] auto defer_invalidation(
			const pull_request_authority_invalidation& aInvalidation) noexcept -> bool;

		[[nodiscard]] auto deferred_invalidations_empty() const noexcept -> bool;

		[[nodiscard]] auto deferred_work_pending() const noexcept -> bool;

		void clear_deferred_invalidations() noexcept;

		void defer_shutdown() noexcept;

		template <typename Action>
		void run_turn(Action&& aAction)
		{
			TurnActive = true;
			try {
				std::forward<Action>(aAction)();
				const auto count = DeferredIngress.size();
				for (auto& pending : DeferredInvalidations) {
					if (DeferredShutdown || !pending) {
						continue;
					}
					const auto invalidation = std::exchange(pending, std::nullopt);
					invalidate_now(*invalidation);
				}
				for (
					std::size_t index = 0U;
					index < count && !DeferredShutdown;
					++index) {
					auto ingress = std::move(DeferredIngress.front());
					DeferredIngress.pop_front();
					std::visit(
						[this](auto&& aValue) {
							using value_type = std::decay_t<decltype(aValue)>;
							if constexpr (
								std::is_same_v<value_type, deferred_accept>) {
								accept_now(std::move(*aValue.Handoff));
							} else {
								dispatch_now(aValue.Intent);
							}
						},
						std::move(ingress));
				}
				if (DeferredShutdown) {
					DeferredShutdown = false;
					clear_deferred_invalidations();
					clear_deferred_ingress();
					shutdown_now();
				}
				if (DeferredDiagnostic) {
					const auto reason = std::exchange(DeferredDiagnostic, std::nullopt);
					publish_diagnostic(*reason);
				}
				TurnActive = false;
				schedule_deferred_ingress();
			} catch (...) {
				clear_deferred_ingress();
				DeferredDiagnostic.reset();
				TurnActive = false;
				throw;
			}
		}

		void schedule_deferred_ingress() noexcept;

		static void enqueue_completion(
			const std::weak_ptr<completion_gate>& aGate,
			decision_executor* const              aDecisionLane,
			std::shared_ptr<job_control>          aControl,
			worker_completion                     aCompletion) noexcept;

		void enqueue_actor_work(
			const std::shared_ptr<authority_effect_actor>& aActor,
			std::function<void()>                          aWork,
			const profile_epoch_actor_priority             aPriority = profile_epoch_actor_priority::reviewer);

		auto submit_erased_effect(
			worker_task                                 aTask,
			worker_completion                           aExceptionCompletion,
			admission_failure                           aAdmissionFailure,
			const std::optional<core::epoch_request_id> aRequest         = std::nullopt,
			const std::optional<core::epoch_generation> aEpochGeneration = std::nullopt,
			const std::optional<core::comparison_generation>
													aComparisonGeneration = std::nullopt,
			const std::optional<std::uint64_t>      aOfferToken           = std::nullopt,
			std::shared_ptr<authority_effect_actor> aActor                = { },
			std::shared_ptr<job_control>            aAdmittedControl      = { }) -> bool;

		template <typename Task, typename ExceptionCompletion, typename AdmissionFailure>
		auto submit_effect(
			Task&&                                      aTask,
			ExceptionCompletion&&                       aExceptionCompletion,
			AdmissionFailure&&                          aAdmissionFailure,
			const std::optional<core::epoch_request_id> aRequest         = std::nullopt,
			const std::optional<core::epoch_generation> aEpochGeneration = std::nullopt,
			const std::optional<core::comparison_generation>
													aComparisonGeneration = std::nullopt,
			const std::optional<std::uint64_t>      aOfferToken           = std::nullopt,
			std::shared_ptr<authority_effect_actor> aActor                = { },
			std::shared_ptr<job_control>            aAdmittedControl      = { }) -> bool
		{
			try {
				auto admission = admission_failure{ aAdmissionFailure };
				auto task      = worker_task{ std::forward<Task>(aTask) };
				auto exception = worker_completion{
					std::forward<ExceptionCompletion>(aExceptionCompletion)
				};
				return submit_erased_effect(
					std::move(task),
					std::move(exception),
					std::move(admission),
					aRequest,
					aEpochGeneration,
					aComparisonGeneration,
					aOfferToken,
					std::move(aActor),
					std::move(aAdmittedControl));
			} catch (...) {
				if (aAdmittedControl) {
					finish_job(aAdmittedControl);
				}
				try {
					std::forward<AdmissionFailure>(aAdmissionFailure)(
						*this, core::epoch_failure::unavailable);
				} catch (...) {
					ProjectionDeliveryFailed = true;
				}
				return false;
			}
		}

		[[nodiscard]] auto admit_job(
			const std::optional<core::epoch_request_id>      aRequest,
			const std::optional<core::epoch_generation>      aEpochGeneration,
			const std::optional<core::comparison_generation> aComparisonGeneration,
			const std::optional<std::uint64_t>               aOfferToken,
			const bool                                       aUseTerminalReservation = true,
			const bool                                       aMandatoryCleanup       = false,
			const bool                                       aPreparation            = false) noexcept
			-> std::expected<std::shared_ptr<job_control>, core::epoch_failure>;

		void reject_admission(
			const admission_failure&  aFailure,
			const core::epoch_failure aReason) noexcept;

		void finish_job(const std::shared_ptr<job_control>& aControl) noexcept;

		void release_pending_cleanup(
			const std::shared_ptr<pending_context_cleanup>& aCleanup) noexcept;

		[[nodiscard]] auto retire_job_for_terminal(
			const std::shared_ptr<job_control>& aControl) noexcept -> bool;

		void release_terminal_reservation(const bool aReserved) noexcept;

		[[nodiscard]] auto retire_actor_job(
			const std::shared_ptr<job_control>& aControl) noexcept -> bool;

		void release_actor_job_reservation(
			const bool aReserved,
			const bool aPreparation = false) noexcept;

		template <typename Predicate>
		void cancel_jobs(Predicate&& aPredicate) noexcept
		{
			auto snapshot = std::array<
				std::shared_ptr<job_control>,
				maximum_epoch_outstanding_jobs>{ };
			auto count = std::size_t{ 0U };
			{
				std::scoped_lock lock{ JobsMutex };
				for (const auto& job : Jobs) {
					if (!job->CancellationIssued && aPredicate(*job)) {
						job->CancellationIssued = true;
						snapshot[count++]       = job;
					}
				}
			}
			for (auto index = std::size_t{ 0U }; index < count; ++index) {
				static_cast<void>(snapshot[index]->Stop.request_stop());
			}
		}

		void cancel_all_jobs() noexcept;

		void close_blob_ingress_and_cancel_jobs() noexcept;

		[[nodiscard]] auto actors_idle() const noexcept -> bool;

		void cancel_authority_jobs() noexcept;

		void cancel_offer_job(const std::uint64_t aToken) noexcept;

		void cancel_work(const core::cancel_epoch_work_request& aRequest) noexcept;

		[[nodiscard]] auto outstanding_work_settled() const noexcept -> bool;

		[[nodiscard]] auto job_count() const noexcept -> std::size_t;

		[[nodiscard]] auto preparation_job_count() const noexcept -> std::size_t;

		[[nodiscard]] auto reviewer_job_count() const noexcept -> std::size_t;

		[[nodiscard]] auto active_reviewer_job_count() const noexcept -> std::size_t;

		[[nodiscard]] auto active_reviewer_job_count_locked() const noexcept -> std::size_t;

		[[nodiscard]] auto preparation_job_count_locked() const noexcept -> std::size_t;

		[[nodiscard]] auto begin_blob_call() noexcept -> bool;

		[[nodiscard]] auto begin_cleanup_call() noexcept -> bool;

		void finish_blob_call() noexcept;

		[[nodiscard]] auto blob_calls_idle() const noexcept -> bool;

		[[nodiscard]] auto queued_actor_work() const noexcept -> std::size_t;

		[[nodiscard]] static auto advance_append(
			epoch_catalog_session&              aCatalogSession,
			const append_operation_record&      aRecord,
			const core::epoch_catalog_identity  aCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<append_operation_record>;

		[[nodiscard]] static auto advance_append(
			authority_context&                  aContext,
			const append_operation_record&      aRecord,
			const core::epoch_catalog_identity  aCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<append_operation_record>;

		[[nodiscard]] static auto advance_maintenance(
			epoch_catalog_session&              aCatalog,
			const maintenance_operation_record& aRecord,
			const core::epoch_catalog_identity  aExpectedCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<maintenance_operation_record>;

		[[nodiscard]] static auto open_actor_cache(authority_effect_actor&       aActor,
												   git_cache_port&               aCaches,
												   const git_cache_open_request& aExpected,
												   const std::stop_token         aStop)
			-> git_cache_result<git_cache_session*>;

		static void mark_append_repair(
			epoch_catalog_session&         aCatalog,
			const append_operation_record& aRecord);

		static void mark_maintenance_repair(epoch_catalog_session&              aCatalog,
											const maintenance_operation_record& aRecord);

		[[nodiscard]] static auto recover_append(
			epoch_catalog_session&  aCatalog,
			git_cache_session&      aCache,
			epoch_entropy_port&     aEntropy,
			append_operation_record aRecord,
			const std::stop_token   aStop)
			-> std::expected<void, core::epoch_failure>;

		[[nodiscard]] static auto recover_maintenance(
			epoch_catalog_session&               aCatalog,
			git_cache_port&                      aCaches,
			const accepted_pull_request_handoff& aHandoff,
			std::unique_ptr<git_cache_session>&  aCache,
			maintenance_operation_record         aRecord,
			const std::stop_token                aStop)
			-> std::expected<core::epoch_catalog_identity, core::epoch_failure>;

		[[nodiscard]] static auto recover_catalog(epoch_catalog_session&               aCatalog,
												  git_cache_port&                      aCaches,
												  epoch_entropy_port&                  aEntropy,
												  const accepted_pull_request_handoff& aHandoff,
												  std::unique_ptr<git_cache_session>&  aCache,
												  const std::stop_token                aStop)
			-> std::expected<void, core::epoch_failure>;

		[[nodiscard]] static auto pending_append(
			epoch_catalog_session&         aCatalog,
			const core::epoch_operation_id aOperation)
			-> std::expected<
				std::optional<append_operation_record>,
				core::epoch_failure>;

		[[nodiscard]] static auto abort_preparation(
			authority_context&  aContext,
			prepared_operation& aOperation, git_cache_port& aCaches) -> bool;

		[[nodiscard]] auto discard_context_preparation(
			std::shared_ptr<authority_context>            aContext,
			const std::optional<core::epoch_operation_id> aExpected        = std::nullopt,
			std::shared_ptr<job_control>                  aAdmittedControl = { })
			-> bool;

		void publish_diagnostic(const core::epoch_reason_code aReason) noexcept;

		void publish(const epoch_presentation_update& aUpdate) noexcept;

		void publish_transition(const core::epoch_transition_output& aOutput);

		void publish_occurrences(
			const core::epoch_catalog_identity               aCatalog,
			const core::epoch_occurrence_history&            aHistory,
			const std::optional<core::review_epoch_summary>& aCurrent);

		auto dispatch_core(
			const core::epoch_event& aEvent,
			const std::function<void(const core::epoch_transition_output&)>&
				aBeforeRequests = { })
			-> core::epoch_transition_output;

		void handle_request(const core::epoch_host_request& aRequest);

		[[nodiscard]] auto exact_context(
			const core::pull_request_authority&         aAuthority,
			const core::accepted_pull_request_revision& aRevision) const
			-> std::shared_ptr<authority_context>;

		[[nodiscard]] auto actor_for(
			const cache_actor_key& aKey)
			-> std::shared_ptr<authority_effect_actor>;

		[[nodiscard]] auto authorized_selected_diff_context(
			const workbench_pull_request_authority&            aAuthority,
			const std::optional<core::agent_revision_binding>& aBinding, const bool aGrant = true) const
			-> std::optional<workbench_selected_diff_context>;

		[[nodiscard]] auto selected_diff_context(
			const workbench_pull_request_authority& aAuthority) const
			-> std::optional<workbench_selected_diff_context>;

		static auto        retained_source(authority_effect_actor& aActor, git_cache_port& aCaches, epoch_catalog_port& aCatalogs,
										   const cached_review_source_request& aRequest, std::stop_token aStop) -> cached_review_source_projection;
		[[nodiscard]] auto read_cached_source(cached_review_source_request aRequest, std::stop_token aStop, cached_review_source_completion aCompletion) -> bool;

		[[nodiscard]] auto resolve_changed_paths(
			changed_paths_request    aRequest,
			changed_paths_completion aCompletion) -> bool;

		[[nodiscard]] auto resolve_commit_filter_touches(
			commit_filter_touches_request    aRequest,
			commit_filter_touches_completion aCompletion) -> bool;

		[[nodiscard]] auto commit_filter_touches(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
												 const std::shared_ptr<authority_context>& aContext, const commit_filter_touches_request& aRequest,
												 const std::stop_token aStop, std::vector<commit_filter_commit_touch>& aTouches) -> commit_filter_touches_status;

		[[nodiscard]] static auto acquire_chain_trees(const std::shared_ptr<authority_context>& aContext, const commit_filter_touches_request& aRequest,
													  const std::stop_token aStop)
			-> std::expected<std::shared_ptr<git_chain_tree_store>, commit_filter_touches_status>;

		[[nodiscard]] static auto sparse_review(const authority_context& aContext) -> bool;

		[[nodiscard]] static auto delta_failure(const git_smart_transport_error& aError) -> agent_workspace_error;

		[[nodiscard]] auto acquire_workspace_delta(const std::shared_ptr<authority_effect_actor>& aActor, const authority_context& aContext,
												   const git_cache_open_request& aExpected, const core::git_commit_oid& aHead,
												   const std::filesystem::path& aClone, const std::stop_token aStop)
			-> std::expected<std::shared_ptr<git_workspace_delta>, agent_workspace_error>;

		[[nodiscard]] auto refresh_managed_clone(const authority_context& aContext, const std::filesystem::path& aClone, const std::stop_token aStop) -> bool;

		[[nodiscard]] auto keep_managed_clone(const managed_clone_request& aRequest, const git_smart_clone_progress& aProgress, const std::stop_token aStop)
			-> std::expected<void, managed_clone_error>;

		[[nodiscard]] auto read_pull_request_files(const std::shared_ptr<authority_effect_actor>& aActor, const authority_context& aContext,
												   const git_cache_open_request& aExpected, const core::agent_revision_binding& aBinding,
												   const std::stop_token aStop) -> std::expected<std::vector<git_workspace_file>, agent_workspace_error>;

		[[nodiscard]] static auto pull_request_files_failure(const git_smart_transport_error& aError) -> agent_workspace_error;

		[[nodiscard]] auto resolve_generated_attributes(
			generated_attributes_request    aRequest,
			generated_attributes_completion aCompletion) -> bool;

		[[nodiscard]] auto generated_attributes(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
												const std::shared_ptr<authority_context>& aContext, const generated_attributes_request& aRequest,
												const std::stop_token aStop, generated_attributes_observation& aObservation) -> generated_attributes_status;

		[[nodiscard]] auto resolve_filtered_comment_lines(
			filtered_comment_lines_request    aRequest,
			filtered_comment_lines_completion aCompletion) -> bool;

		[[nodiscard]] auto resolve_head_move_lines(
			head_move_lines_request    aRequest,
			head_move_lines_completion aCompletion) -> bool;

		[[nodiscard]] auto resolve_permalink_source(
			permalink_source_request    aRequest,
			permalink_source_completion aCompletion) -> bool;

		[[nodiscard]] auto resolve_github_diff_lines(
			github_diff_lines_request    aRequest,
			github_diff_lines_completion aCompletion) -> bool;

		[[nodiscard]] auto github_diff_lines(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
											 const std::shared_ptr<authority_context>& aContext, const github_diff_lines_request& aRequest,
											 const std::stop_token aStop, std::optional<core::line_correspondence>& aLines) -> github_diff_lines_status;

		[[nodiscard]] auto filtered_comment_lines(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
												  const std::shared_ptr<authority_context>& aContext, const filtered_comment_lines_request& aRequest,
												  const std::stop_token aStop, core::filtered_comment_lines& aLines) -> filtered_comment_lines_status;

		[[nodiscard]] auto head_move_lines(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
										   const std::shared_ptr<authority_context>& aContext, const head_move_lines_request& aRequest,
										   const std::stop_token aStop, std::optional<core::line_correspondence>& aLines) -> head_move_lines_status;

		/// \brief RFC-BPR-0089 M4 and [[BPR-1733]]. Reads files by literal path
		/// at commits of the review and compares pairs of them, for `aAnswer`.
		///
		/// \details Each blob is looked up in the retained cache or, for a
		/// sparse review whose chain commits only its chain store holds, there,
		/// and read exactly as `generated_attributes` reads one. Each pair is
		/// compared by the review's own engine with exact whitespace, so an
		/// unchanged line is byte for byte the same. `aAnswer` is called with
		/// `load(path, commit)` and `compare(left, right)`.
		template <typename Answer>
		[[nodiscard]] auto read_exact_file_sides(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
												 const std::shared_ptr<authority_context>& aContext, const core::git_commit_oid& aHead,
												 const core::file_comparison_key& aKey, const std::stop_token aStop, Answer&& aAnswer) -> filtered_comment_lines_status
		{
			using status        = filtered_comment_lines_status;
			const auto statusOf = [](const git_cache_failure aFailure) {
				return aFailure == git_cache_failure::cancelled      ? status::cancelled
					: aFailure == git_cache_failure::object_mismatch ? status::objects_unavailable
					: aFailure == git_cache_failure::quota_exceeded  ? status::too_large
																	 : status::failure;
			};
			if (aStop.stop_requested() || !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
				return status::cancelled;
			}
			if (!open_actor_cache(aActor, Caches, aExpected, aStop)) {
				return status::stale;
			}
			const auto chainTrees = [&] {
				std::scoped_lock lock{ aActor.BlobAuthorityMutex };
				return aActor.ChainTrees;
			}();
			const auto sparse = aContext && object_loading_for(aContext->Handoff.Shape) == pull_request_object_loading::selected_objects
				&& aContext->Handoff.SmartGit && !aContext->Handoff.SmartGit->revoked() && aContext->Handoff.SmartGit->supports_sparse_commit_trees()
				&& aHead == aContext->Revision.Head;
			const auto lookup = [&](const std::string& aPath, const core::git_commit_oid& aCommit) -> std::expected<std::optional<core::git_blob_oid>, status> {
				if (chainTrees && chainTrees->holds(aCommit)) {
					auto held = chainTrees->find_blob_at_path(aCommit, aPath, aStop);
					if (!held) {
						return std::unexpected{ held.error().Failure == git_smart_transport_failure::cancelled ? status::cancelled : status::objects_unavailable };
					}
					return *held;
				}
				auto found = aActor.Cache->find_blob_at_path(
					git_cache_tree_entry_request{
						.Commit = aCommit,
						.Path   = std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(aPath.data()), aPath.size() } },
					},
					aStop);
				if (!found.has_value()) {
					return std::unexpected{ statusOf(found.error().Code) };
				}
				return *found;
			};
			const auto read = [&](const core::git_blob_oid& aBlob) -> std::expected<std::vector<std::uint8_t>, status> {
				auto bytes = aActor.Cache->materialize_blob({ .Blob = aBlob, .MaximumBytes = maximum_filtered_comment_side_bytes }, aStop);
				if (bytes.has_value()) {
					return std::move(*bytes);
				}
				if (bytes.error().Code != git_cache_failure::object_mismatch || !sparse) {
					return std::unexpected{ statusOf(bytes.error().Code) };
				}
				{
					std::scoped_lock lock{ aActor.BlobAuthorityMutex };
					if (auto retained = retained_blob(aActor, aBlob)) {
						return retained->size() <= maximum_filtered_comment_side_bytes ? std::expected<std::vector<std::uint8_t>, status>{ std::move(*retained) }
																					   : std::unexpected{ status::too_large };
					}
				}
				auto smart               = git_smart_request_context{ };
				smart.ApplicationSession = aContext->Handoff.ApplicationSession;
				smart.Authority          = aContext->Authority;
				smart.Revision           = aContext->Revision;
				smart.Repository         = aContext->Handoff.Locator;
				smart.BaseRef            = aContext->Handoff.BaseRef;
				const auto transport     = aContext->Handoff.SmartGit->acquire(smart);
				if (!transport || !*transport) {
					return std::unexpected{ status::objects_unavailable };
				}
				const auto fetched = (*transport)->read_exact_blob({ smart, aBlob, maximum_filtered_comment_side_bytes }, aStop);
				if (!fetched) {
					const auto failure = fetched.error().Failure;
					return std::unexpected{ failure == git_smart_transport_failure::response_limit  ? status::too_large
												: failure == git_smart_transport_failure::cancelled ? status::cancelled
																									: status::objects_unavailable };
				}
				auto exact = std::vector<std::uint8_t>(fetched->size());
				std::ranges::transform(*fetched, exact.begin(), [](const std::byte aByte) { return std::to_integer<std::uint8_t>(aByte); });
				// A mismatch or a collision detection both end here, at failure.
				if (blob_matches(aBlob, exact) != blob_check::matches) {
					return std::unexpected{ status::failure };
				}
				return exact;
			};
			struct side final
			{
				core::git_commit_oid                     Commit;
				std::optional<core::git_blob_oid>        Blob;
				std::optional<std::vector<std::uint8_t>> Bytes;
			};
			const auto load = [&](const std::string& aPath, const core::git_commit_oid& aCommit) -> std::expected<side, status> {
				auto blob = lookup(aPath, aCommit);
				if (!blob) {
					return std::unexpected{ blob.error() };
				}
				auto result = side{ .Commit = aCommit, .Blob = *blob, .Bytes = std::nullopt };
				if (*blob) {
					auto bytes = read(**blob);
					if (!bytes) {
						return std::unexpected{ bytes.error() };
					}
					if (bytes->size() > maximum_filtered_comment_side_bytes) {
						return std::unexpected{ status::too_large };
					}
					result.Bytes = std::move(*bytes);
				}
				return result;
			};
			// One comparison of two sides; none when the engine refuses it (a
			// binary side, or one past its limits).
			auto       matcher = core::make_xdiff_line_matcher();
			const auto compare = [&](const side& aLeft, const side& aRight) -> std::optional<core::line_correspondence> {
				if (!matcher) {
					return std::nullopt;
				}
				const auto options = core::diff_options{ };
				const auto digest  = core::options_digest(options);
				if (!digest) {
					return std::nullopt;
				}
				auto key         = aKey;
				key.LeftBlob     = aLeft.Blob;
				key.RightBlob    = aRight.Blob;
				key.Options      = *digest;
				key.Algorithm    = core::supported_diff_algorithm_revision;
				key.Filter       = aLeft.Commit != aRight.Commit && aLeft.Commit.algorithm() == aRight.Commit.algorithm()
					? std::optional{ core::filtered_diff_endpoints{ .Left = aLeft.Commit, .Right = aRight.Commit } }
					: std::nullopt;
				const auto input = core::diff_compute_input{
					.Key        = key,
					.Options    = options,
					.LeftBytes  = aLeft.Bytes ? std::optional{ std::span<const std::uint8_t>{ *aLeft.Bytes } } : std::nullopt,
					.RightBytes = aRight.Bytes ? std::optional{ std::span<const std::uint8_t>{ *aRight.Bytes } } : std::nullopt,
					.Limits     = { },
				};
				auto       matching = diff_line_matching{ *matcher };
				const auto computed = core::compute_file_diff(input, matching);
				return computed.Result ? core::correspondence_of(computed.Result->Alignment) : std::nullopt;
			};
			return std::forward<Answer>(aAnswer)(load, compare);
		}

		[[nodiscard]] auto review_cache_actor(
			const workbench_pull_request_authority& aAuthority,
			const core::git_commit_oid&             aHead)
			-> std::optional<std::pair<std::shared_ptr<authority_effect_actor>, git_cache_open_request>>;

		[[nodiscard]] auto resolve_github_base_numbering(
			github_base_numbering_request    aRequest,
			github_base_numbering_completion aCompletion) -> bool;

		[[nodiscard]] static auto durably_sparse_anchor(
			authority_effect_actor&        aActor,
			authority_context&             aContext,
			const core::comparison_anchor& aAnchorToProve,
			const std::stop_token          aStop) -> bool;

		[[nodiscard]] auto resolve_selected_diff(
			const workbench_selected_diff_request& aRequest,
			const std::stop_token                  aStop,
			const bool                             aPrepared = false)
			-> workbench_selected_diff_observation;

		/// \brief Runs one bounded piece of work on a repository actor and waits.
		///
		/// \details The same admission, FIFO, job ledger and `BlobWait` bound as
		/// the reviewer's own selected resolution; an absent result means the
		/// work was refused, abandoned or ran out of time.
		template <typename Result, typename Work>
		[[nodiscard]] auto run_on_actor(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const git_cache_open_request&                  aExpected,
			Work                                           aWork,
			const std::stop_token                          aStop) -> std::optional<Result>
		{
			struct waiter_type final
			{
				std::mutex              Mutex;
				std::condition_variable Completed;
				std::optional<Result>   Value;
				bool                    Done      = false;
				bool                    Abandoned = false;
			};
			auto waiter = std::shared_ptr<waiter_type>{ };
			try {
				waiter = std::make_shared<waiter_type>();
			} catch (...) {
				return std::nullopt;
			}
			auto admitted = admit_job(std::nullopt, std::nullopt, std::nullopt, std::nullopt, false, false, true);
			if (!admitted.has_value()) {
				return std::nullopt;
			}
			auto control = std::move(*admitted);
			try {
				enqueue_actor_work(
					aActor,
					[this, aActor, aExpected, aStop, waiter, control, work = std::move(aWork)] {
						auto value = std::optional<Result>{ };
						try {
							auto forwardCallerStop = std::stop_callback{
								aStop, [control] { static_cast<void>(control->Stop.request_stop()); }
							};
							const auto workStop = control->Stop.get_token();
							if (!workStop.stop_requested() && Consumer->active()
								&& !BlobIngressClosing.load(std::memory_order_acquire)
								&& open_actor_cache(*aActor, Caches, aExpected, workStop).has_value()) {
								value = work(workStop);
							}
						} catch (...) {
							value.reset();
						}
						auto waiterLock     = std::unique_lock{ waiter->Mutex };
						waiter->Value       = std::move(value);
						waiter->Done        = true;
						const auto reserved = retire_actor_job(control);
						struct actor_exit_reservation final
						{
							implementation& Host;
							bool            Reserved;
							bool            Preparation;

							~actor_exit_reservation()
							{
								Host.release_actor_job_reservation(Reserved, Preparation);
							}
						} actorExit{ *this, reserved, control->Preparation };
						waiterLock.unlock();
						waiter->Completed.notify_all();
					},
					// [[BPR-1344]]. Only preparation's blob reads run here.
					profile_epoch_actor_priority::preparation);
			} catch (...) {
				finish_job(control);
				return std::nullopt;
			}
			auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
														{
															const auto guard  = std::lock_guard{ waiter->Mutex };
															waiter->Abandoned = true;
														}
														waiter->Completed.notify_all();
													} };
			std::unique_lock lock{ waiter->Mutex };
			static_cast<void>(waiter->Completed.wait_for(
				lock, Limits.BlobWait, [&waiter] { return waiter->Done || waiter->Abandoned; }));
			return waiter->Done ? std::move(waiter->Value) : std::nullopt;
		}

		[[nodiscard]] auto search_actor(const core::source_search_scope& aScope) const -> std::shared_ptr<authority_effect_actor>;

		[[nodiscard]] auto bind_source_search(const workbench_pull_request_authority& aAuthority, const core::comparison_pair& aComparison, std::optional<core::document_tab_id> aTab,
											  std::optional<std::vector<core::source_search_filtered_file>> aFiltered) -> std::optional<core::source_search_scope>;

		[[nodiscard]] static auto exact_search_anchors(authority_context& aContext, const core::comparison_pair& aPair, std::stop_token aStop) -> bool;

		[[nodiscard]] static auto build_source_corpus(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const core::source_search_scope& aScope, std::stop_token aWorkStop) -> source_corpus_observation;

		[[nodiscard]] static auto filter_source_corpus(authority_effect_actor& aActor, const core::source_search_scope& aScope,
													   const std::vector<core::source_search_filtered_file>& aFiles, const std::stop_token aStop, source_corpus& aCorpus) -> bool;

		[[nodiscard]] static auto search_catalogue_turn(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation;
		[[nodiscard]] auto        catalogue(const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation;

		[[nodiscard]] auto acquire_source_corpus_material(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const std::shared_ptr<const source_corpus>& aCorpus, core::diff_side aSide, const std::shared_ptr<corpus_read_waiter>& aWaiter, const std::shared_ptr<job_control>& aControl) -> source_corpus_material;

		[[nodiscard]] auto read_source_corpus(const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop, source_corpus_read_needed aNeeded) -> source_corpus_material;

		[[nodiscard]] auto prepare_selected_diffs(
			const std::vector<workbench_selected_diff_request>& aRequests,
			const std::stop_token                               aStop) -> epoch_prepared_comparisons;

		void fetch_prepared_blobs(const workbench_selected_diff_request& aTemplate,
								  epoch_prepared_comparisons&            aResult,
								  const std::stop_token                  aStop);

		/// What the host's own blob check found. [[BPR-1450]]
		enum class blob_check : std::uint8_t {
			matches,
			/// Not a SHA-1 blob, or the bytes are another object.
			mismatch,
			/// SHA-1 collision detection refused the bytes; no identity exists.
			collision_detected,
		};

		[[nodiscard]] static auto blob_matches(const core::git_blob_oid&        aBlob,
											   const std::vector<std::uint8_t>& aBytes) noexcept -> blob_check;

		void begin_actor_blob_read(actor_blob_read&                               aRead,
								   const std::shared_ptr<authority_effect_actor>& aActor,
								   const std::shared_ptr<authority_context>&      aSelected,
								   const blob_authority&                          aAuthorization,
								   const std::uint64_t                            aAdmittedGeneration,
								   const git_cache_open_request&                  aExpected,
								   const std::stop_source&                        aJobStop);

		static void read_remote_blob(actor_blob_read& aRead, const std::stop_token aStop) noexcept;

		void finish_actor_blob_read(actor_blob_read&                               aRead,
									const std::shared_ptr<authority_effect_actor>& aActor,
									const std::shared_ptr<authority_context>&      aSelected,
									const blob_authority&                          aAuthorization,
									const std::uint64_t                            aAdmittedGeneration,
									const std::stop_token                          aStop,
									std::unique_lock<std::mutex>&                  aPublicationLock);

		[[nodiscard]] auto materialize(
			const diff_blob_request&                           aRequest,
			const std::stop_token                              aStop,
			std::optional<workbench_selected_diff_observation> aPinnedSource = std::nullopt) -> diff_blob_observation;

		[[nodiscard]] auto materialize_both(const diff_blob_request& aLeft,
											const diff_blob_request& aRight,
											const std::stop_token    aStop)
			-> std::optional<std::pair<diff_blob_observation, diff_blob_observation>>;

		static void read_both_remote_blobs(blob_pair_waiter&     aWaiter,
										   actor_blob_read&      aLeft,
										   actor_blob_read&      aRight,
										   const std::stop_token aStop) noexcept;

		[[nodiscard]] auto materialize_agent_workspace(
			const agent_workspace_materialization_request& aRequest,
			const std::stop_token                          aStop)
			-> std::expected<owned_agent_workspace, agent_workspace_error>;

		[[nodiscard]] auto materialize_on_actor(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const blob_authority&                          aAuthorization,
			const std::uint64_t                            aGeneration,
			const git_cache_open_request&                  aExpected,
			const agent_workspace_materialization_request& aRequest,
			std::shared_ptr<git_workspace_delta>           aDelta,
			bool&                                          aCloneLacksHead,
			const std::stop_token                          aStop,
			std::optional<std::vector<git_workspace_file>> aPullRequestFiles = std::nullopt)
			-> std::expected<owned_agent_workspace, agent_workspace_error>;

		[[nodiscard]] auto cleanup_agent_workspace(
			const owned_agent_workspace& aWorkspace,
			const std::stop_token        aStop)
			-> std::expected<void, agent_workspace_error>;

		void accept(accepted_pull_request_handoff aHandoff);

		void accept_now(accepted_pull_request_handoff aHandoff);

		[[nodiscard]] auto active_matches(
			const pull_request_authority_invalidation& aInvalidation) const noexcept
			-> bool;

		void invalidate(const pull_request_authority_invalidation aInvalidation);

		void invalidate_now(
			const pull_request_authority_invalidation& aInvalidation);

		void dispatch(const epoch_intent aIntent);

		void dispatch_now(const epoch_intent& aIntent);

		void prepare_runtime();

		void shutdown();

		void shutdown_now();

		void start_catalog_load(const core::load_epoch_catalog_request& aRequest);

		void start_revision_preparation(
			const core::prepare_epoch_revision_request& aRequest);

		void resume_pending_occurrence_import(const std::shared_ptr<authority_context>& aContext);

		void start_historical_import(const std::shared_ptr<authority_context>&                  aContext,
									 const core::epoch_request_id                               aRequest,
									 const core::epoch_generation                               aGeneration,
									 const std::optional<core::epoch_occurrence_import_context> aSelection = std::nullopt);

		void start_catalog_commit(const core::commit_epoch_catalog_request& aRequest);

		void discard_preparation(
			const core::discard_epoch_preparation_request& aRequest);

		void start_epoch_maintenance(
			const core::perform_epoch_maintenance_request& aRequest);

		void start_history_page(
			const core::load_epoch_history_page_request& aRequest);

		void start_comparison_resolution(
			const core::resolve_comparison_request& aRequest);

		core::epoch_state                             State;
		git_cache_port&                               Caches;
		epoch_catalog_port&                           Catalogs;
		epoch_entropy_port&                           Entropy;
		epoch_clock_port&                             Clock;
		effect_executor&                              Effects;
		decision_executor&                            DecisionLane;
		epoch_projection_port&                        Projections;
		epoch_host_limits                             Limits;
		std::shared_ptr<diagnostic_log_port>          DiagnosticLog;
		std::shared_ptr<completion_gate>              CompletionGate;
		std::shared_ptr<profile_epoch_actor_consumer> Consumer;
		const core::application_session_id            WindowApplicationSession;
		const bool                                    Legacy;
		struct actor_work_state
		{
			std::atomic_size_t Pending = 0U;
			std::atomic_size_t Queued  = 0U;
		};
		std::shared_ptr<actor_work_state> ActorWork = std::make_shared<actor_work_state>();
		std::vector<
			std::pair<cache_actor_key, std::shared_ptr<authority_effect_actor>>>
										   CacheActors;
		mutable std::mutex                 CacheActorsMutex;
		mutable std::mutex                 JobsMutex;
		std::shared_ptr<authority_context> Active;
		/// [[BPR-1733]]. Heads this host accepted for a pull request before the
		/// one it shows, newest last, at most `maximum_earlier_heads` of them:
		/// the commits a kept suggestion may be carried from.
		std::vector<std::pair<core::pull_request_authority, core::git_commit_oid>> EarlierHeads;
		static constexpr std::size_t                                               maximum_earlier_heads = 64U;

		void                                               remember_earlier_head(const core::pull_request_authority& aAuthority, const core::git_commit_oid& aHead);
		std::optional<pull_request_authority_invalidation> PendingAuthority;
		std::optional<std::uint64_t>                       PendingOfferToken;
		std::vector<std::shared_ptr<job_control>>          Jobs;
		std::size_t                                        ReservedTerminalAdmissions = 0U;
		std::size_t                                        RetiringActorJobs          = 0U;
		std::size_t                                        RetiringPreparationJobs    = 0U;
		std::size_t                                        InFlightBlobCalls          = 0U;
		std::deque<deferred_ingress>                       DeferredIngress;
		// Only the active and pending authority can require retained priority
		// invalidation. These fixed slots cannot be crowded out by ordinary
		// reentrant accept/dispatch work.
		std::array<std::optional<pull_request_authority_invalidation>, 2U>
											   DeferredInvalidations;
		std::optional<core::epoch_reason_code> DeferredDiagnostic;
		std::uint64_t                          OfferToken                = 0U;
		bool                                   TurnActive                = false;
		bool                                   DeferredIngressScheduled  = false;
		bool                                   DeferredShutdown          = false;
		bool                                   Closing                   = false;
		bool                                   RuntimePreparationStarted = false;
		bool                                   ProjectionDeliveryFailed  = false;
		std::atomic_bool                       BlobIngressClosing        = false;
	};
}
#endif
