#include "../support/source_regex_engine_fixture.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/review_tour_source.hpp"
#include "bpr/application/source_pr_search_execution.hpp"
#include "bpr/application/testing.hpp"
#include "bpr/core/comparison_preparation.hpp"
#include "bpr/core/sha1.hpp"
#include "product_diff.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <expected>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto entropy_bytes(const std::uint8_t aSeed)
		-> application::epoch_entropy_bytes
	{
		auto bytes    = application::epoch_entropy_bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return bytes;
	}

	template <typename Oid>
	[[nodiscard]] constexpr auto oid(const std::uint8_t aSeed) -> Oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] constexpr auto pack_oid(const std::uint8_t aSeed)
		-> application::git_pack_oid
	{
		auto digest    = std::array<std::uint8_t, 32U>{ };
		digest.front() = aSeed;
		return {
			.Algorithm = core::git_hash_algorithm::sha1,
			.Digest    = digest,
		};
	}

	[[nodiscard]] constexpr auto artifact(
		const application::git_artifact_role aRole,
		const std::uint8_t                   aSeed,
		const std::uint64_t                  aBytes) -> application::git_artifact_fact
	{
		auto identity = std::array<std::uint8_t, 32U>{ };
		identity[0]   = 1U;
		identity[1]   = 1U;
		identity[2]   = 1U;
		identity[8]   = aSeed;
		return {
			.Role     = aRole,
			.Identity = application::git_artifact_identity{ identity },
			.Bytes    = aBytes,
		};
	}

	[[nodiscard]] auto bytes(const std::string_view aValue)
		-> std::vector<std::byte>
	{
		return {
			reinterpret_cast<const std::byte*>(aValue.data()),
			reinterpret_cast<const std::byte*>(aValue.data() + aValue.size()),
		};
	}

	[[nodiscard]] auto packet(const std::string_view aPayload) -> std::string
	{
		constexpr std::string_view digits    = "0123456789abcdef";
		const auto                 total     = aPayload.size() + 4U;
		auto                       header    = std::array<char, 4U>{ };
		auto                       remaining = total;
		for (auto index = header.size(); index-- > 0U;) {
			header[index] = digits[remaining & 0x0FU];
			remaining >>= 4U;
		}
		return std::string{ header.data(), header.size() } + std::string{ aPayload };
	}

	class queued_executor final
		: public application::decision_executor,
		  public application::effect_executor
	{
	public:
		explicit queued_executor(const bool aDecision) noexcept
			: Decision{ aDecision }
		{
		}

		void post(std::function<void()> aTask) override
		{
			std::unique_lock lock{ Mutex };
			if (AcceptedPostsBeforeRejection) {
				if (*AcceptedPostsBeforeRejection == 0U) {
					AcceptedPostsBeforeRejection.reset();
					throw std::runtime_error{ "injected sequenced executor refusal" };
				}
				--*AcceptedPostsBeforeRejection;
			}
			if (BlockedRejectedPosts != 0U) {
				--BlockedRejectedPosts;
				PostBlocked = true;
				Changed.notify_all();
				Changed.wait(lock, [this] {
					return ReleaseBlockedPost;
				});
				PostBlocked = false;
				throw std::runtime_error{ "injected blocked executor refusal" };
			}
			if (RejectedPosts != 0U) {
				--RejectedPosts;
				throw std::runtime_error{ "injected executor refusal" };
			}
			Tasks.push_back(std::move(aTask));
			Changed.notify_all();
		}

		[[nodiscard]] auto pending() const -> std::size_t
		{
			std::scoped_lock lock{ Mutex };
			return Tasks.size();
		}

		void wait_for_pending()
		{
			std::unique_lock lock{ Mutex };
			Changed.wait(lock, [this] {
				return !Tasks.empty();
			});
		}

		void run(const std::size_t aIndex)
		{
			auto task = std::function<void()>{ };
			{
				std::scoped_lock lock{ Mutex };
				expect(aIndex < Tasks.size(), "executor index is in range");
				task = std::move(Tasks[aIndex]);
				Tasks.erase(Tasks.begin() + static_cast<std::ptrdiff_t>(aIndex));
				Running = true;
			}
			task();
			{
				std::scoped_lock lock{ Mutex };
				Running = false;
			}
		}

		void run_next()
		{
			run(0U);
		}

		[[nodiscard]] auto is_running() const -> bool
		{
			std::scoped_lock lock{ Mutex };
			return Running;
		}

		[[nodiscard]] auto is_decision() const noexcept -> bool
		{
			return Decision;
		}

		void reject_next()
		{
			std::scoped_lock lock{ Mutex };
			++RejectedPosts;
		}

		void accept_one_then_reject()
		{
			std::scoped_lock lock{ Mutex };
			AcceptedPostsBeforeRejection = 1U;
		}

		void block_and_reject_next()
		{
			std::scoped_lock lock{ Mutex };
			++BlockedRejectedPosts;
			ReleaseBlockedPost = false;
		}

		void wait_for_blocked_post()
		{
			std::unique_lock lock{ Mutex };
			Changed.wait(lock, [this] {
				return PostBlocked;
			});
		}

		void release_blocked_post()
		{
			std::scoped_lock lock{ Mutex };
			ReleaseBlockedPost = true;
			Changed.notify_all();
		}

	private:
		mutable std::mutex                Mutex;
		std::condition_variable           Changed;
		std::deque<std::function<void()>> Tasks;
		bool                              Decision             = false;
		bool                              Running              = false;
		std::size_t                       RejectedPosts        = 0U;
		std::size_t                       BlockedRejectedPosts = 0U;
		bool                              PostBlocked          = false;
		bool                              ReleaseBlockedPost   = false;
		std::optional<std::size_t>        AcceptedPostsBeforeRejection;
	};

	struct recording_epoch_log final : application::diagnostic_log_port
	{
		std::vector<application::diagnostic_event> Events;
		application::diagnostic_log_status         Status     = application::diagnostic_log_status::available;
		std::uint64_t                              Generation = 1U;
		auto                                       record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			return Status;
		}
		auto status() const noexcept -> application::diagnostic_log_status override { return Status; }
		auto context_generation() const noexcept -> std::uint64_t override { return Generation; }
	};

	enum class install_witness_fault {
		none,
		omitted,
		repeated,
		changed_result,
	};

	enum class receiving_witness_fault {
		none,
		omitted,
		repeated,
		foreign_operation,
	};

	struct shared_state
	{
		std::optional<application::git_smart_transport_failure> AcquireFailure;
		std::optional<application::git_smart_transport_failure> DiscoveryFailure;
		application::git_smart_transport_step                   DiscoveryStep{ };
		bool                                                    MalformedDiscovery = false;
		std::optional<application::git_pack_stream_failure>     PackFailure;
		std::vector<std::string>                                Calls;
		application::epoch_catalog_binding                      Binding;
		application::epoch_catalog_snapshot                     Snapshot;
		bool                                                    OccurrencesSupported         = false;
		bool                                                    ThrowOccurrenceWrite         = false;
		bool                                                    RefuseOccurrenceWrite        = false;
		bool                                                    RefuseOccurrenceSupersession = false;
		std::vector<core::epoch_occurrence>                     SupersededOccurrences;
		core::epoch_occurrence_history                          Occurrences;
		std::vector<core::review_epoch_summary>                 EpochRows;
		std::optional<std::int64_t>                             CommitTimeSeconds   = 1'789'000'123;
		core::head_lineage                                      PreparedHeadLineage = core::head_lineage::not_applicable;
		std::vector<core::git_commit_oid>                       CommitTimeRequests;
		std::deque<bool>                                        SourceCollisions;
		std::deque<std::optional<application::epoch_catalog_failure>>
																						 CatalogOpenFailures;
		std::deque<application::epoch_catalog_snapshot>                                  CatalogOpenSnapshots;
		std::optional<application::append_operation_record>                              Operation;
		std::optional<application::maintenance_operation_record>                         Maintenance;
		std::optional<application::git_retained_ref_pair>                                Retained;
		std::vector<application::git_retained_ref_pair>                                  RetainedPairs;
		std::vector<std::pair<core::epoch_ordinal, application::git_revision_residency>> RetainedResidencies;
		std::deque<application::epoch_recovery_page>                                     RecoveryPages;
		std::vector<std::optional<core::epoch_operation_id>>                             RecoveryPageCursors;
		std::deque<application::epoch_catalog_result<
			application::epoch_history_page>>
															 HistoryPages;
		std::vector<application::epoch_history_page_request> HistoryRequests;
		std::deque<application::git_cache_result<
			application::git_operation_recovery_observation>>
			RecoveryInspections;
		std::vector<application::git_operation_recovery_request>
			RecoveryInspectionRequests;
		std::vector<application::git_operation_recovery_request>
						  RecoveryAbortRequests;
		std::vector<bool> RecoveryAbortStopRequested;
		std::vector<application::git_operation_recovery_request>
																	RecoveryCompletionRequests;
		std::vector<application::git_cache_open_request>            CacheOpenRequests;
		std::function<void()>                                       DuringCacheDestruction;
		std::vector<application::git_cache_blob_request>            BlobRequests;
		std::vector<application::git_cache_changed_entries_request> CatalogueRequests;
		std::vector<application::git_cache_changed_entry>           CatalogueEntries;
		std::optional<application::git_cache_failure>               CatalogueFailure;
		std::optional<std::vector<std::uint8_t>>                    CachedBlobOverride;
		bool                                                        CachedBlobMissing          = false;
		bool                                                        SparseObjects              = false;
		bool                                                        SparseTreeMismatch         = false;
		bool                                                        SparseCatalogMarkerMissing = false;
		bool                                                        SupportsSparse             = false;
		std::vector<application::git_smart_exact_blob_request>      ExactBlobRequests;
		std::optional<application::git_smart_transport_failure>     ExactBlobFailure;
		/// [[BPR-1327]]. Batched reads, what a batch answers, and the
		/// simulated round trips of the single and batched reads.
		std::vector<application::git_smart_exact_blobs_request> ExactBlobBatches;
		std::vector<std::pair<core::git_blob_oid, std::string>> BatchContents;
		bool                                                    BatchUnsupported = false;
		std::chrono::milliseconds                               ExactBlobLatency{ 0 };
		std::chrono::milliseconds                               BatchLatency{ 0 };
		/// [[BPR-1359]]. A single read that ends early when stopped, as the
		/// real transport kills its gh process; how many are on the wire, and
		/// how many were stopped before their latency ran out.
		bool                     ExactBlobLatencyObservesStop = false;
		std::atomic<std::size_t> ExactBlobReadsInFlight{ 0U };
		std::atomic<std::size_t> ExactBlobStopsObserved{ 0U };
		/// [[BPR-1360]]. Single reads may now run two at once, one on the
		/// actor and one on the diff worker, so their records are guarded:
		/// the most on the wire together, and each read's blob, start and
		/// end, in the order they ended.
		struct exact_blob_span
		{
			core::git_blob_oid                    Blob;
			std::chrono::steady_clock::time_point Started;
			std::chrono::steady_clock::time_point Ended;
			bool                                  Stopped = false;
		};
		std::mutex                                             ExactBlobMutex;
		std::size_t                                            ExactBlobMostInFlight = 0U;
		std::vector<exact_blob_span>                           ExactBlobSpans;
		std::vector<application::git_cache_tree_entry_request> TreeEntryRequests;
		std::deque<std::optional<core::git_blob_oid>>          TreeEntryResults;
		// [[BPR-1212]]. The merge bases a fake cache answers, in order.
		std::vector<application::git_cache_comparison_base_request>    ComparisonBaseRequests;
		std::deque<application::git_cache_comparison_base_observation> ComparisonBaseResults;
		std::vector<application::git_cache_rename_pair_request>        RenamePairRequests;
		std::deque<application::git_cache_result<
			application::git_rename_pair_observation>>
																 RenamePairResults;
		std::vector<application::exact_anchor_lookup_request>    ExactAnchorRequests;
		std::vector<application::git_ref_prune_authorization>    PruneRequests;
		std::vector<application::git_cache_delete_authorization> DeleteRequests;
		std::vector<application::begin_prune_request>            BeginPruneRequests;
		std::vector<application::begin_cache_delete_request>
			BeginCacheDeleteRequests;
		std::deque<std::pair<std::string, application::epoch_catalog_failure>>
			MaintenanceCatalogFailures;
		std::deque<std::pair<std::string, application::git_cache_failure>>
			MaintenanceCacheFailures;
		/// Failures for the cache open itself. The fake refuses an open only
		/// when its stop token is already requested, and requesting a stop
		/// through the host also moves the comparison context a completion is
		/// matched against, so BPR-0459's question, what a cancelled open
		/// reports when its context does still match, had no way to be asked.
		std::deque<application::git_cache_failure> CacheOpenFailures;
		/// Stops the retained session answering for the exact request it was
		/// opened with, which is how `open_actor_cache` decides to reopen. One
		/// repository actor holds one cache session at a time, so a catalog
		/// commit can find a session that is not the one it needs; while the
		/// fixture's session always matched, the commit's own cache open was
		/// unreachable (BPR-0459).
		bool                                              RetainedCacheSessionIsForeign = false;
		install_witness_fault                             InstallWitnessFault           = install_witness_fault::none;
		std::optional<application::epoch_catalog_failure> InstallFailure;
		std::optional<application::epoch_catalog_failure> ReceivingFailure;
		receiving_witness_fault                           ReceivingWitnessFault = receiving_witness_fault::none;
		/// Failures for the *resolve* path rather than maintenance. A catalog
		/// retained across a change of history names commits the cache can no
		/// longer produce, and the question BPR-0200 asks is what a reviewer is
		/// told when that happens. Nothing could answer it while the only
		/// injectable failures were prune and delete.
		std::deque<std::pair<std::string, application::git_cache_failure>>
															PrepareCacheFailures;
		bool                                                PromoteAppliesThenFails           = false;
		bool                                                AssociationAppliesThenFails       = false;
		bool                                                CacheDeleteFinalizesWithoutChange = false;
		bool                                                ThrowRecoveryAbort                = false;
		bool                                                BlobMaterializationIgnoresStop    = false;
		std::function<void()>                               AfterCommitAuthorized;
		std::function<void()>                               AfterRefCommitInProgress;
		std::function<void(std::stop_token)>                DuringBlobMaterialization;
		std::function<void(std::stop_token)>                DuringTreeEntryLookup;
		std::optional<application::git_cache_error>         WorkspaceMaterializationFailure;
		std::optional<application::git_cache_error>         WorkspaceCleanupFailure;
		std::vector<std::uint64_t>                          AuthorityRevocations;
		std::vector<application::epoch_presentation_update> Updates;
		std::size_t                                         CacheMutationCalls          = 0U;
		std::size_t                                         ActiveCacheSessions         = 0U;
		std::size_t                                         MaximumActiveCacheSessions  = 0U;
		std::size_t                                         TransportCalls              = 0U;
		std::size_t                                         ProjectionCalls             = 0U;
		std::size_t                                         ProjectionOutsideDecision   = 0U;
		std::size_t                                         ProjectionFailuresRemaining = 0U;
		std::size_t                                         ProjectionDepth             = 0U;
		std::size_t                                         MaximumProjectionDepth      = 0U;
		std::size_t                                         ProjectionIngressCalls      = 0U;
		std::function<void()>                               ProjectionIngress;
	};

	template <typename Failure>
	[[nodiscard]] auto take_failure(
		std::deque<std::pair<std::string, Failure>>& aFailures,
		const std::string_view                       aCall) -> std::optional<Failure>
	{
		if (aFailures.empty() || aFailures.front().first != aCall) {
			return std::nullopt;
		}
		const auto failure = aFailures.front().second;
		aFailures.pop_front();
		return failure;
	}

	[[nodiscard]] auto authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 11U },
			.ProfileRevision = core::github_profile_revision{ 21U },
			.Host            = core::git_host_id{ 31U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(2U),
			.Account         = core::github_account_id{ 41U },
			.Viewer          = core::github_viewer_actor_id{ 51U },
			.Repository      = core::repository_id{ 61U },
			.Cache           = opaque<core::repository_cache_scope_id>(3U),
			.PullRequest     = core::pull_request_id{ 71U },
			.Number          = core::pull_request_number{ 81U },
			.Scope           = opaque<core::pull_request_authority_scope_id>(4U),
		};
	}

	[[nodiscard]] auto catalog_identity(
		const std::int64_t aRevision = 0) -> core::epoch_catalog_identity
	{
		return {
			.Incarnation = opaque<core::epoch_catalog_incarnation_id>(5U),
			.Revision    = core::epoch_catalog_revision{ aRevision },
		};
	}

	[[nodiscard]] auto summary_for(
		const core::accepted_pull_request_revision& aRevision,
		const core::epoch_catalog_identity          aCatalog,
		const core::epoch_ordinal                   aOrdinal = core::epoch_ordinal{ 1 })
		-> core::review_epoch_summary
	{
		return {
			.Id = {
				.Authority   = authority().Scope,
				.Incarnation = aCatalog.Incarnation,
				.Ordinal     = aOrdinal,
			},
			.Source     = aRevision.Source,
			.BaseBranch = aRevision.BaseBranch,
			.Base       = aRevision.Base,
			.Head       = aRevision.Head,
			.Material   = core::epoch_material_identity{
				.BaseTree       = oid<core::git_tree_oid>(60U),
				.HeadTree       = oid<core::git_tree_oid>(70U),
				.PolicyRevision = core::comparison_policy_revision{ 1U },
			},
			.Cause = {
				.Origin   = core::epoch_origin::initial_observation,
				.Base     = core::epoch_base_transition::not_applicable,
				.Head     = core::epoch_head_transition::not_applicable,
				.Material = core::epoch_material_relation::unknown,
				.Primary  = core::epoch_primary_cause::initial_observation,
			},
			.ObservedAt       = core::observed_utc_milliseconds{ 100U },
			.BaseAvailability = core::anchor_availability::available,
			.HeadAvailability = core::anchor_availability::available,
		};
	}

	[[nodiscard]] auto history_cursor_for(
		const core::epoch_catalog_identity aCatalog,
		const core::epoch_ordinal          aBefore) -> core::epoch_history_cursor
	{
		return {
			.Authority   = authority().Scope,
			.Incarnation = aCatalog.Incarnation,
			.Revision    = aCatalog.Revision,
			.Before      = aBefore,
		};
	}

	class fake_entropy final : public application::epoch_entropy_port
	{
	public:
		explicit fake_entropy(shared_state& aState) noexcept
			: State{ aState }
		{
		}

		[[nodiscard]] auto generate() noexcept
			-> std::optional<application::epoch_entropy_bytes> override
		{
			State.Calls.emplace_back("entropy");
			return entropy_bytes(Next++);
		}

	private:
		shared_state& State;
		std::uint8_t  Next = 0x20U;
	};

	class fake_clock final : public application::epoch_clock_port
	{
	public:
		explicit fake_clock(shared_state& aState) noexcept
			: State{ aState }
		{
		}

		void set_next_utc(const std::int64_t aNext) noexcept
		{
			Next = aNext;
		}

		[[nodiscard]] auto observe_utc() noexcept
			-> std::optional<core::observed_utc_milliseconds> override
		{
			State.Calls.emplace_back("clock");
			return core::observed_utc_milliseconds{ Next++ };
		}

	private:
		shared_state& State;
		std::int64_t  Next = 1'000;
	};

	class fake_projection final : public application::epoch_projection_port
	{
	public:
		fake_projection(shared_state& aState, queued_executor& aDecision) noexcept
			: State{ aState }
			, Decision{ aDecision }
		{
		}

		void publish(const application::epoch_presentation_update& aUpdate) override
		{
			++State.ProjectionCalls;
			if (!Decision.is_running()) {
				++State.ProjectionOutsideDecision;
			}
			++State.ProjectionDepth;
			State.MaximumProjectionDepth = std::max(
				State.MaximumProjectionDepth, State.ProjectionDepth);
			struct depth_guard final
			{
				std::size_t& Depth;

				~depth_guard()
				{
					--Depth;
				}
			} guard{ State.ProjectionDepth };
			if (State.ProjectionFailuresRemaining != 0U) {
				--State.ProjectionFailuresRemaining;
				throw std::runtime_error{ "injected projection failure" };
			}
			State.Updates.push_back(aUpdate);
			auto ingress = std::exchange(State.ProjectionIngress, { });
			if (ingress) {
				++State.ProjectionIngressCalls;
				ingress();
			}
		}

	private:
		shared_state&    State;
		queued_executor& Decision;
	};

	class fake_catalog_session final : public application::epoch_catalog_session
	{
	public:
		explicit fake_catalog_session(shared_state& aState) noexcept
			: State{ aState }
		{
		}

		[[nodiscard]] auto binding() const noexcept
			-> const application::epoch_catalog_binding& override
		{
			return State.Binding;
		}

		[[nodiscard]] auto snapshot(const std::stop_token aStop)
			-> application::epoch_catalog_result<
				application::epoch_catalog_snapshot> override
		{
			State.Calls.emplace_back("catalog.snapshot");
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::epoch_catalog_failure::cancelled
				};
			}
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures,
												  "catalog.snapshot")) {
				return std::unexpected{ *failure };
			}
			return State.Snapshot;
		}

		[[nodiscard]] auto retained_residency(
			const core::epoch_ordinal aOrdinal,
			const std::stop_token     aStop)
			-> application::epoch_catalog_result<application::git_revision_residency> override
		{
			State.Calls.emplace_back("catalog.retained-residency");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			if (State.SparseCatalogMarkerMissing || !State.Operation || !State.Operation->Install) {
				return std::unexpected{ application::epoch_catalog_failure::unavailable };
			}
			const auto recorded = std::ranges::find_if(State.RetainedResidencies,
													   [aOrdinal](const auto& aEntry) { return aEntry.first == aOrdinal; });
			if (recorded == State.RetainedResidencies.end()) {
				return std::unexpected{ application::epoch_catalog_failure::unavailable };
			}
			return recorded->second;
		}

		[[nodiscard]] auto occurrences(const std::stop_token aStop)
			-> application::epoch_catalog_result<core::epoch_occurrence_history> override
		{
			if (!State.OccurrencesSupported || aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::unavailable };
			}
			return State.Occurrences;
		}

		[[nodiscard]] auto record_occurrences(const core::epoch_occurrence_history& aHistory,
											  const std::stop_token                 aStop) -> application::epoch_catalog_result<void> override
		{
			if (State.RefuseOccurrenceWrite) {
				return std::unexpected{ application::epoch_catalog_failure::invalid_request };
			}
			if (State.ThrowOccurrenceWrite) {
				throw std::runtime_error{ "injected optional sidecar failure" };
			}
			if (!State.OccurrencesSupported || aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::unavailable };
			}
			expect(std::ranges::all_of(aHistory.Occurrences, [](const auto& aOccurrence) {
					   return core::valid(aOccurrence);
				   }),
				   "host persists valid occurrence facts");
			State.Occurrences = aHistory;
			return { };
		}

		[[nodiscard]] auto supersede_occurrence(const core::epoch_occurrence& aExpectedObserved,
												const core::epoch_occurrence& aReplacementEvent, const std::stop_token aStop)
			-> application::epoch_catalog_result<void> override
		{
			if (aStop.stop_requested() || State.RefuseOccurrenceSupersession) {
				return std::unexpected{ application::epoch_catalog_failure::operation_conflict };
			}
			const auto expected = std::ranges::find(State.Occurrences.Occurrences, aExpectedObserved);
			if (expected == State.Occurrences.Occurrences.end()) {
				return std::unexpected{ application::epoch_catalog_failure::operation_conflict };
			}
			expect(aExpectedObserved.Evidence.Kind == core::epoch_occurrence_kind::observed_head
					   && aReplacementEvent.Evidence.Kind == core::epoch_occurrence_kind::force_push_event
					   && aExpectedObserved.LocalEpoch == aReplacementEvent.LocalEpoch
					   && aExpectedObserved.Evidence.Head == aReplacementEvent.Evidence.Head,
				   "supersession preserves exact local binding and head while upgrading occurrence evidence");
			State.SupersededOccurrences.push_back(aExpectedObserved);
			State.Occurrences.Occurrences.erase(expected);
			std::erase_if(State.Occurrences.Occurrences, [&](const auto& aOccurrence) {
				return aOccurrence.Id == aReplacementEvent.Id;
			});
			State.Occurrences.Occurrences.push_back(aReplacementEvent);
			return { };
		}

		[[nodiscard]] auto source_observation_exists(
			const core::epoch_source_observation_id,
			const std::stop_token aStop)
			-> application::epoch_catalog_result<bool> override
		{
			State.Calls.emplace_back("catalog.source-probe");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			if (State.SourceCollisions.empty()) {
				return false;
			}
			const auto collision = State.SourceCollisions.front();
			State.SourceCollisions.pop_front();
			return collision;
		}

		[[nodiscard]] auto canonical_review(
			const application::canonical_review_request& aRequest,
			const std::stop_token                        aStop)
			-> application::epoch_catalog_result<
				core::catalog_review_association> override
		{
			State.Calls.emplace_back("catalog.canonical-review");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			return core::catalog_review_association{
				.Association  = aRequest.ProposedAssociation,
				.Review       = aRequest.Candidate.Review,
				.Actor        = aRequest.Candidate.Actor,
				.Order        = aRequest.Candidate.Order,
				.State        = aRequest.Candidate.State,
				.Commit       = aRequest.Candidate.Commit,
				.Availability = core::anchor_availability::available,
			};
		}

		[[nodiscard]] auto exact_anchors(
			const application::exact_anchor_lookup_request& aRequest,
			const std::stop_token                           aStop)
			-> application::epoch_catalog_result<
				application::exact_anchor_lookup_result> override
		{
			State.Calls.emplace_back("catalog.exact-anchors");
			State.ExactAnchorRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures,
												  "catalog.exact-anchors")) {
				return std::unexpected{ *failure };
			}
			auto result = application::exact_anchor_lookup_result{
				.Catalog = aRequest.Catalog,
			};
			for (const auto& identity : aRequest.Anchors) {
				const auto  found = std::ranges::find_if(State.EpochRows, [&identity](const auto& aEpoch) {
					return identity.Authority == aEpoch.Id.Authority
						&& identity.Incarnation == aEpoch.Id.Incarnation
						&& identity.Epoch == aEpoch.Id.Ordinal;
				});
				const auto* epoch = found != State.EpochRows.end() ? &*found
					: State.Snapshot.LatestLive && identity.Authority == State.Snapshot.LatestLive->Id.Authority
						&& identity.Incarnation == State.Snapshot.LatestLive->Id.Incarnation
						&& identity.Epoch == State.Snapshot.LatestLive->Id.Ordinal
					? &*State.Snapshot.LatestLive
					: nullptr;
				if (!epoch) {
					continue;
				}
				if (identity.Kind == core::comparison_anchor_kind::epoch_base) {
					result.Anchors.push_back(core::comparison_anchor{
						.Identity     = identity,
						.Commit       = epoch->Base,
						.Availability = epoch->BaseAvailability,
					});
				} else if (identity.Kind
						   == core::comparison_anchor_kind::epoch_head) {
					result.Anchors.push_back(core::comparison_anchor{
						.Identity     = identity,
						.Commit       = epoch->Head,
						.Availability = epoch->HeadAvailability,
					});
				}
			}
			return result;
		}

		[[nodiscard]] auto history(
			const application::epoch_history_page_request& aRequest,
			const std::stop_token                          aStop)
			-> application::epoch_catalog_result<
				application::epoch_history_page> override
		{
			State.Calls.emplace_back("catalog.history");
			State.HistoryRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::epoch_catalog_failure::cancelled
				};
			}
			if (State.HistoryPages.empty() && State.OccurrencesSupported) {
				auto page = application::epoch_history_page{
					.Authority = State.Binding.Authority.Scope,
					.Catalog   = State.Binding.Catalog,
				};
				for (auto it = State.EpochRows.rbegin(); it != State.EpochRows.rend(); ++it) {
					if (!aRequest.Cursor || it->Id.Ordinal < aRequest.Cursor->Before) {
						page.Epochs.push_back(*it);
					}
				}
				if (page.Epochs.size() > aRequest.Limit) {
					page.Epochs.resize(aRequest.Limit);
					page.Continuation = core::epoch_history_cursor{
						.Authority   = page.Authority,
						.Incarnation = page.Catalog.Incarnation,
						.Revision    = page.Catalog.Revision,
						.Before      = page.Epochs.back().Id.Ordinal,
					};
				}
				return page;
			}
			if (State.HistoryPages.empty()) {
				return std::unexpected{
					application::epoch_catalog_failure::invalid_request
				};
			}
			// [[BPR-1262]]. The value or the failure is moved out on its own.
			// Moving the whole expected and returning it read as an
			// uninitialized vector to GCC 15 at -O3.
			auto& front = State.HistoryPages.front();
			if (!front) {
				const auto failure = front.error();
				State.HistoryPages.pop_front();
				return std::unexpected{ failure };
			}
			auto page = std::move(*front);
			State.HistoryPages.pop_front();
			return page;
		}

		[[nodiscard]] auto mark_anchor_availability(
			const application::mark_anchor_availability_request&,
			const std::stop_token)
			-> application::epoch_catalog_result<
				application::catalog_commit_result> override
		{
			return std::unexpected{ application::epoch_catalog_failure::invalid_request };
		}

		[[nodiscard]] auto reserve_append(
			const application::append_reservation_request& aRequest,
			const std::stop_token                          aStop)
			-> application::epoch_catalog_result<
				application::append_operation_record> override
		{
			State.Calls.emplace_back("catalog.reserve");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			const auto owner     = State.Binding.Authority.Scope;
			const auto operation = aRequest.Context.Operation;
			State.Operation      = application::append_operation_record{
				.Context           = aRequest.Context,
				.ReservedOrdinal   = aRequest.ProposedOrdinal,
				.OperationRevision = application::epoch_catalog_operation_revision{ 1 },
				.Phase             = application::epoch_catalog_operation_phase::preparing,
				.StagingBase       = {
					.Authority = owner,
					.Ordinal   = aRequest.ProposedOrdinal,
					.Operation = operation,
					.Staging   = true,
				},
				.StagingHead = {
					.Authority = owner,
					.Ordinal   = aRequest.ProposedOrdinal,
					.Operation = operation,
					.Staging   = true,
					.Head      = true,
				},
				.FinalBase = {
					.Authority = owner,
					.Ordinal   = aRequest.ProposedOrdinal,
				},
				.FinalHead = {
					.Authority = owner,
					.Ordinal   = aRequest.ProposedOrdinal,
					.Head      = true,
				},
			};
			return *State.Operation;
		}

		[[nodiscard]] auto record_receiving(const application::record_receiving_request& aRequest, std::stop_token aStop)
			-> application::epoch_catalog_result<application::append_operation_record> override
		{
			State.Calls.emplace_back("catalog.receiving");
			if (aStop.stop_requested() || !State.Operation) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			if (State.ReceivingFailure) {
				return std::unexpected{ *State.ReceivingFailure };
			}
			State.Operation->Receiving = aRequest.Receiving;
			advance(application::epoch_catalog_operation_phase::preparing);
			return *State.Operation;
		}

		[[nodiscard]] auto record_spool(
			const application::record_spool_request& aRequest,
			const std::stop_token                    aStop)
			-> application::epoch_catalog_result<
				application::append_operation_record> override
		{
			State.Calls.emplace_back("catalog.spool-recorded");
			if (aStop.stop_requested() || !State.Operation) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			State.Operation->Spool     = aRequest.Spool;
			State.Operation->Preimport = aRequest.Preimport;
			advance(application::epoch_catalog_operation_phase::spool_recorded);
			expect(
				application::valid(*State.Operation),
				"catalog fake returns a valid spool record");
			return *State.Operation;
		}

		[[nodiscard]] auto record_install(
			const application::record_install_request& aRequest,
			const std::stop_token                      aStop)
			-> application::epoch_catalog_result<
				application::append_operation_record> override
		{
			State.Calls.emplace_back("catalog.imported");
			if (aStop.stop_requested() || !State.Operation) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			if (State.InstallFailure) {
				return std::unexpected{ *State.InstallFailure };
			}
			State.Operation->Install = aRequest.Install;
			if (aRequest.DerivedComparisonBase) {
				State.Operation->Context.Revision.Base = *aRequest.DerivedComparisonBase;
			}
			advance(application::epoch_catalog_operation_phase::imported);
			return *State.Operation;
		}

		[[nodiscard]] auto record_candidate(
			const application::record_candidate_request& aRequest,
			const std::stop_token                        aStop)
			-> application::epoch_catalog_result<
				application::append_operation_record> override
		{
			State.Calls.emplace_back("catalog.commit-authorized");
			if (aStop.stop_requested() || !State.Operation) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			State.Operation->Candidate            = aRequest.Candidate;
			State.Operation->HistoricalOccurrence = aRequest.HistoricalOccurrence;
			advance(application::epoch_catalog_operation_phase::commit_authorized);
			if (State.AfterCommitAuthorized) {
				// One shot on every standard library: a moved-from std::function
				// is unspecified, and libc++ leaves a small callable armed.
				auto callback = std::exchange(State.AfterCommitAuthorized, { });
				callback();
			}
			return *State.Operation;
		}

		[[nodiscard]] auto transition_operation(
			const application::operation_transition_request& aRequest,
			const std::stop_token                            aStop)
			-> application::epoch_catalog_result<
				application::epoch_recovery_operation> override
		{
			const auto call = "catalog.transition:" + phase_name(aRequest.Next);
			State.Calls.push_back(call);
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::epoch_catalog_failure::cancelled
				};
			}
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures, call)) {
				return std::unexpected{ *failure };
			}
			if (aRequest.Kind != application::epoch_catalog_operation_kind::append) {
				if (!State.Maintenance) {
					return std::unexpected{
						application::epoch_catalog_failure::phase_conflict
					};
				}
				expect(State.Maintenance->Phase == aRequest.Expected
						   && State.Maintenance->Operation == aRequest.Operation
						   && State.Maintenance->OperationRevision
							   == aRequest.ExpectedOperationRevision,
					   "catalog fake observes exact maintenance phase identity");
				if (aRequest.Next
					== application::epoch_catalog_operation_phase::repair_required) {
					State.Maintenance->RepairFromPhase = State.Maintenance->Phase;
				}
				State.Maintenance->Phase             = aRequest.Next;
				State.Maintenance->OperationRevision = application::epoch_catalog_operation_revision{
					State.Maintenance->OperationRevision.value() + 1
				};
				if (aRequest.Next
						== application::epoch_catalog_operation_phase::
							ref_delete_in_progress
					|| aRequest.Next
						== application::epoch_catalog_operation_phase::
							quarantine_rename_in_progress) {
					if (State.AfterRefCommitInProgress) {
						auto callback = std::exchange(State.AfterRefCommitInProgress, { });
						callback();
					}
				}
				return application::epoch_recovery_operation{ *State.Maintenance };
			}
			if (!State.Operation) {
				return std::unexpected{
					application::epoch_catalog_failure::phase_conflict
				};
			}
			if (State.Operation->Phase != aRequest.Expected) {
				std::cerr << "catalog phase mismatch: actual="
						  << static_cast<int>(State.Operation->Phase)
						  << " expected=" << static_cast<int>(aRequest.Expected)
						  << " next=" << static_cast<int>(aRequest.Next) << '\n';
				expect(
					false,
					"catalog fake observes exact expected operation phase");
			}
			if (
				aRequest.Next
				== application::epoch_catalog_operation_phase::repair_required) {
				State.Operation->RepairFromPhase = State.Operation->Phase;
			}
			advance(aRequest.Next);
			if (
				aRequest.Next
					== application::epoch_catalog_operation_phase::
						ref_commit_in_progress
				&& State.AfterRefCommitInProgress) {
				auto callback = std::exchange(State.AfterRefCommitInProgress, { });
				callback();
			}
			if (
				aRequest.Next == application::epoch_catalog_operation_phase::aborted
				|| aRequest.Next
					== application::epoch_catalog_operation_phase::complete) {
				State.Operation->RecoveryInputCompacted = true;
				State.Operation->Receiving.reset();
				State.Operation->Spool.reset();
				State.Operation->Preimport.reset();
			}
			return application::epoch_recovery_operation{ *State.Operation };
		}

		[[nodiscard]] auto finalize_append(
			const application::finalize_append_request& aRequest,
			const std::stop_token                       aStop)
			-> application::epoch_catalog_result<
				application::catalog_commit_result> override
		{
			State.Calls.emplace_back("catalog.finalize");
			if (aStop.stop_requested() || !State.Operation) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			advance(
				application::epoch_catalog_operation_phase::
					visible_cleanup_pending);
			const auto next = core::epoch_catalog_identity{
				.Incarnation = aRequest.Catalog.Incarnation,
				.Revision    = core::epoch_catalog_revision{
					aRequest.Catalog.Revision.value() + 1 },
			};
			State.Binding.Catalog          = next;
			State.Snapshot.Binding.Catalog = next;
			if (State.Operation->Install) {
				State.RetainedResidencies.emplace_back(aRequest.Mutation.Epoch.Id.Ordinal,
													   State.Operation->Install->Residency);
			}
			if (State.OccurrencesSupported) {
				State.EpochRows.push_back(aRequest.Mutation.Epoch);
				if (aRequest.HistoricalOccurrence) {
					const auto found = std::ranges::find(State.Occurrences.Occurrences,
														 *aRequest.HistoricalOccurrence, &core::epoch_occurrence::Id);
					if (found != State.Occurrences.Occurrences.end()) {
						found->LocalEpoch            = aRequest.Mutation.Epoch.Id;
						found->DerivedComparisonBase = aRequest.Mutation.Epoch.Base;
					}
				} else if (State.Snapshot.LatestLive && State.Snapshot.LatestLive->Head != aRequest.Mutation.Epoch.Head) {
					State.Snapshot.PreviousDistinctHead = State.Snapshot.LatestLive;
				}
			}
			if (!aRequest.HistoricalOccurrence) {
				if (core::historical_comparison_selection_changed(State.Operation->Context.Revision, State.Snapshot.LatestLive)) {
					for (auto& occurrence : State.Occurrences.Occurrences) {
						const auto imported = std::ranges::any_of(State.EpochRows, [&](const auto& aEpoch) {
							return occurrence.LocalEpoch == aEpoch.Id && aEpoch.Cause.Origin == core::epoch_origin::explicit_historical_import;
						});
						if (occurrence.DerivedComparisonBase || imported) {
							occurrence.LocalEpoch.reset();
							occurrence.DerivedComparisonBase.reset();
						}
					}
				}
				State.Snapshot.LatestLive = aRequest.Mutation.Epoch;
			}
			State.Snapshot.NextOrdinal = core::epoch_ordinal{
				aRequest.Mutation.Epoch.Id.Ordinal.value() + 1
			};
			return application::catalog_commit_result{
				.Catalog       = next,
				.Changed       = true,
				.InsertedEpoch = aRequest.Mutation.Epoch.Id,
			};
		}

		[[nodiscard]] auto associate_review(
			const application::associate_review_request& aRequest,
			const std::stop_token                        aStop)
			-> application::epoch_catalog_result<
				application::catalog_commit_result> override
		{
			State.Calls.emplace_back("catalog.associate-review");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			const auto next = core::epoch_catalog_identity{
				.Incarnation = aRequest.Catalog.Incarnation,
				.Revision    = core::epoch_catalog_revision{
					aRequest.Catalog.Revision.value() + 1 },
			};
			State.Binding.Catalog              = next;
			State.Snapshot.Binding.Catalog     = next;
			State.Snapshot.CurrentViewerReview = aRequest.Review;
			if (State.AssociationAppliesThenFails) {
				State.AssociationAppliesThenFails = false;
				return std::unexpected{
					application::epoch_catalog_failure::unavailable
				};
			}
			return application::catalog_commit_result{
				.Catalog = next,
				.Changed = true,
			};
		}

		[[nodiscard]] auto begin_prune(
			const application::begin_prune_request& aRequest,
			const std::stop_token                   aStop)
			-> application::epoch_catalog_result<
				application::maintenance_operation_record> override
		{
			State.Calls.emplace_back("catalog.begin-prune");
			State.BeginPruneRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::epoch_catalog_failure::cancelled
				};
			}
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures,
												  "catalog.begin-prune")) {
				return std::unexpected{ *failure };
			}
			State.Maintenance = application::maintenance_operation_record{
				.Incarnation       = aRequest.Catalog.Incarnation,
				.Operation         = aRequest.Operation,
				.Kind              = application::epoch_catalog_operation_kind::prune,
				.Phase             = application::epoch_catalog_operation_phase::prune_intent,
				.OperationRevision = application::epoch_catalog_operation_revision{ 1 },
				.Cache             = State.Binding.Authority.Cache,
				.Epochs            = aRequest.Epochs,
			};
			return *State.Maintenance;
		}

		[[nodiscard]] auto finalize_prune(
			const application::finalize_prune_request& aRequest,
			const std::stop_token)
			-> application::epoch_catalog_result<
				application::catalog_commit_result> override
		{
			State.Calls.emplace_back("catalog.finalize-prune");
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures,
												  "catalog.finalize-prune")) {
				return std::unexpected{ *failure };
			}
			if (!State.Maintenance) {
				return std::unexpected{
					application::epoch_catalog_failure::phase_conflict
				};
			}
			expect(State.Maintenance->Phase == aRequest.Transition.Expected
					   && State.Maintenance->Operation == aRequest.Transition.Operation,
				   "prune finalization matches the sealed operation");
			State.Maintenance->Phase             = aRequest.Transition.Next;
			State.Maintenance->OperationRevision = application::epoch_catalog_operation_revision{
				State.Maintenance->OperationRevision.value() + 1
			};
			const auto next = core::epoch_catalog_identity{
				.Incarnation = State.Binding.Catalog.Incarnation,
				.Revision    = core::epoch_catalog_revision{
					State.Binding.Catalog.Revision.value() + 1 },
			};
			State.Binding.Catalog          = next;
			State.Snapshot.Binding.Catalog = next;
			if (State.Snapshot.LatestLive) {
				State.Snapshot.LatestLive->BaseAvailability = core::anchor_availability::pruned;
				State.Snapshot.LatestLive->HeadAvailability = core::anchor_availability::pruned;
			}
			return application::catalog_commit_result{
				.Catalog = next,
				.Changed = true,
			};
		}

		[[nodiscard]] auto begin_cache_delete(
			const application::begin_cache_delete_request& aRequest,
			const std::stop_token                          aStop)
			-> application::epoch_catalog_result<
				application::maintenance_operation_record> override
		{
			State.Calls.emplace_back("catalog.begin-cache-delete");
			State.BeginCacheDeleteRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::epoch_catalog_failure::cancelled
				};
			}
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures,
												  "catalog.begin-cache-delete")) {
				return std::unexpected{ *failure };
			}
			State.Maintenance = application::maintenance_operation_record{
				.Incarnation = aRequest.Impact.Incarnation,
				.Operation   = aRequest.Operation,
				.Kind        = application::epoch_catalog_operation_kind::cache_delete,
				.Phase       = application::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
				.OperationRevision = application::epoch_catalog_operation_revision{ 1 },
				.Cache             = aRequest.Impact.Cache,
				.Scopes            = aRequest.Impact.Scopes,
			};
			return *State.Maintenance;
		}

		[[nodiscard]] auto cache_delete_impact(
			const core::repository_cache_scope_id aCache,
			const std::stop_token                 aStop)
			-> application::epoch_catalog_result<
				application::cache_delete_impact> override
		{
			State.Calls.emplace_back("catalog.cache-delete-impact");
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::epoch_catalog_failure::cancelled
				};
			}
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures,
												  "catalog.cache-delete-impact")) {
				return std::unexpected{ *failure };
			}
			return application::cache_delete_impact{
				.Incarnation = State.Binding.Catalog.Incarnation,
				.Cache       = aCache,
				.Scopes      = {
					application::cache_delete_scope_fact{
						.Authority        = State.Binding.Authority.Scope,
						.ExpectedRevision = State.Binding.Catalog.Revision,
					},
				},
				.RetainedEpochCount = State.Snapshot.LatestLive ? 1U : 0U,
			};
		}

		[[nodiscard]] auto finalize_cache_delete(
			const application::finalize_cache_delete_request& aRequest,
			const std::stop_token)
			-> application::epoch_catalog_result<void> override
		{
			State.Calls.emplace_back("catalog.finalize-cache-delete");
			if (const auto failure = take_failure(State.MaintenanceCatalogFailures,
												  "catalog.finalize-cache-delete")) {
				return std::unexpected{ *failure };
			}
			if (!State.Maintenance) {
				return std::unexpected{
					application::epoch_catalog_failure::phase_conflict
				};
			}
			expect(State.Maintenance->Phase == aRequest.Transition.Expected
					   && State.Maintenance->Operation == aRequest.Transition.Operation,
				   "cache-delete finalization matches the sealed operation");
			if (State.CacheDeleteFinalizesWithoutChange) {
				State.CacheDeleteFinalizesWithoutChange = false;
				return { };
			}
			State.Maintenance->Phase             = aRequest.Transition.Next;
			State.Maintenance->OperationRevision = application::epoch_catalog_operation_revision{
				State.Maintenance->OperationRevision.value() + 1
			};
			State.Binding.Catalog.Revision = core::epoch_catalog_revision{
				State.Binding.Catalog.Revision.value() + 1
			};
			State.Snapshot.Binding.Catalog = State.Binding.Catalog;
			if (State.Snapshot.LatestLive) {
				State.Snapshot.LatestLive->BaseAvailability = core::anchor_availability::cache_deleted;
				State.Snapshot.LatestLive->HeadAvailability = core::anchor_availability::cache_deleted;
			}
			return { };
		}

		[[nodiscard]] auto recovery_page(
			const std::optional<core::epoch_operation_id> aAfter,
			const std::uint16_t,
			const std::stop_token aStop)
			-> application::epoch_catalog_result<
				application::epoch_recovery_page> override
		{
			State.Calls.emplace_back("catalog.recovery-page");
			State.RecoveryPageCursors.push_back(aAfter);
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::epoch_catalog_failure::cancelled
				};
			}
			if (State.RecoveryPages.empty()) {
				return application::epoch_recovery_page{
					.Incarnation = State.Binding.Catalog.Incarnation,
				};
			}
			auto page = std::move(State.RecoveryPages.front());
			State.RecoveryPages.pop_front();
			return page;
		}

	private:
		[[nodiscard]] static auto phase_name(
			const application::epoch_catalog_operation_phase aPhase)
			-> std::string
		{
			using enum application::epoch_catalog_operation_phase;
			switch (aPhase) {
			case prune_intent:
				return "prune-intent";
			case ref_delete_in_progress:
				return "ref-delete";
			case cache_delete_intent_sealed:
				return "cache-delete-sealed";
			case quarantine_rename_in_progress:
				return "quarantine-rename";
			case quarantined:
				return "quarantined";
			case catalog_tombstoned_cleanup_pending:
				return "catalog-tombstoned";
			case staging_ready:
				return "staging-ready";
			case ref_commit_in_progress:
				return "ponr";
			case refs_committed:
				return "refs-committed";
			case visible_cleanup_pending:
				return "visible";
			case complete:
				return "cleanup";
			case abort_requested:
				return "abort-requested";
			case aborted:
				return "aborted";
			case repair_required:
				return "repair-required";
			default:
				return "other";
			}
		}

		void advance(const application::epoch_catalog_operation_phase aNext)
		{
			expect(State.Operation.has_value(), "catalog operation exists");
			State.Operation->Phase             = aNext;
			State.Operation->OperationRevision = application::epoch_catalog_operation_revision{
				State.Operation->OperationRevision.value() + 1
			};
		}

		shared_state& State;
	};

	class fake_catalog_port final : public application::epoch_catalog_port
	{
	public:
		explicit fake_catalog_port(shared_state& aState) noexcept
			: State{ aState }
		{
		}

		[[nodiscard]] auto open(
			const application::epoch_catalog_open_request&,
			const std::stop_token aStop)
			-> application::epoch_catalog_result<
				std::unique_ptr<application::epoch_catalog_session>> override
		{
			State.Calls.emplace_back("catalog.open");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::epoch_catalog_failure::cancelled };
			}
			if (!State.CatalogOpenFailures.empty()) {
				const auto failure = State.CatalogOpenFailures.front();
				State.CatalogOpenFailures.pop_front();
				if (failure) {
					return std::unexpected{ *failure };
				}
			}
			if (!State.CatalogOpenSnapshots.empty()) {
				State.Snapshot = std::move(State.CatalogOpenSnapshots.front());
				State.CatalogOpenSnapshots.pop_front();
				State.Binding = State.Snapshot.Binding;
			}
			return std::make_unique<fake_catalog_session>(State);
		}

	private:
		shared_state& State;
	};

	class fake_pack_sink final : public application::git_pack_stream_sink
	{
	public:
		fake_pack_sink(shared_state& aState, const core::epoch_operation_id aOperation)
			: State{ aState }
			, Operation{ aOperation }
		{
		}

		[[nodiscard]] auto append(
			const std::span<const std::byte> aInput,
			const std::stop_token            aStop)
			-> application::git_pack_stream_result<void> override
		{
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::git_pack_stream_failure::cancelled
				};
			}
			Received.insert(Received.end(), aInput.begin(), aInput.end());
			return { };
		}

		[[nodiscard]] auto finish(const std::stop_token aStop)
			-> application::git_pack_stream_result<
				application::completed_git_pack_receipt> override
		{
			State.Calls.emplace_back("cache.sink-finish");
			if (State.PackFailure) {
				return std::unexpected{ *State.PackFailure };
			}
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::git_pack_stream_failure::cancelled
				};
			}
			const auto pack  = pack_oid(0x42U);
			auto       token = std::array<std::uint8_t, 32U>{ };
			token.front()    = 0x71U;
			return application::completed_git_pack_receipt{
				.Operation    = Operation,
				.ExpectedPack = pack,
				.ObservedPack = pack,
				.Spool        = artifact(
					application::git_artifact_role::completed_spool,
					0x51U,
					Received.size()),
				.OpaqueToken = token,
			};
		}

		void abort() noexcept override
		{
			State.Calls.emplace_back("cache.sink-abort");
		}

	private:
		shared_state&            State;
		core::epoch_operation_id Operation;
		std::vector<std::byte>   Received;
	};

	class fake_cache_session final : public application::git_cache_session
	{
	public:
		fake_cache_session(
			shared_state&                       aState,
			application::git_cache_open_request aRequest)
			: State{ aState }
			, Request{ std::move(aRequest) }
			, Foreign{ Request }
		{
			Foreign.ObjectFormat
				= Request.ObjectFormat == core::git_hash_algorithm::sha1
				? core::git_hash_algorithm::sha256
				: core::git_hash_algorithm::sha1;
			++State.ActiveCacheSessions;
			State.MaximumActiveCacheSessions = std::max(
				State.MaximumActiveCacheSessions,
				State.ActiveCacheSessions);
			State.Calls.emplace_back(
				"cache.session-open:"
				+ std::to_string(Request.Authority.PullRequest.value()));
		}

		~fake_cache_session() override
		{
			if (State.DuringCacheDestruction) {
				State.DuringCacheDestruction();
			}
			State.Calls.emplace_back(
				"cache.session-close:"
				+ std::to_string(Request.Authority.PullRequest.value()));
			expect(
				State.ActiveCacheSessions != 0U,
				"cache session accounting cannot underflow");
			--State.ActiveCacheSessions;
		}

		[[nodiscard]] auto request() const noexcept
			-> const application::git_cache_open_request& override
		{
			return State.RetainedCacheSessionIsForeign ? Foreign : Request;
		}

		[[nodiscard]] auto read_committer_time_seconds(const core::git_commit_oid aCommit,
													   const std::stop_token      aStop) -> application::git_cache_result<std::optional<std::int64_t>> override
		{
			State.CommitTimeRequests.push_back(aCommit);
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			return State.CommitTimeSeconds;
		}

		[[nodiscard]] auto create_pack_sink(
			const core::epoch_operation_id                                      aOperation,
			const std::uint64_t                                                 aMaximumBytes,
			const std::stop_token                                               aStop,
			const std::function<bool(const application::git_spool_ownership&)>& aWitness)
			-> application::git_cache_result<
				std::unique_ptr<application::git_pack_stream_sink>> override
		{
			State.Calls.emplace_back("cache.create-sink");
			++State.CacheMutationCalls;
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			const auto ownership = application::git_spool_ownership{
				.Operation    = State.ReceivingWitnessFault == receiving_witness_fault::foreign_operation
					? opaque<core::epoch_operation_id>(0xe9U)
					: aOperation,
				.Identity     = artifact(application::git_artifact_role::completed_spool, 0x51U, 8U).Identity,
				.MaximumBytes = aMaximumBytes,
			};
			if (aWitness && State.ReceivingWitnessFault != receiving_witness_fault::omitted && !aWitness(ownership)) {
				return std::unexpected{ application::git_cache_failure::spool_failure };
			}
			if (aWitness && State.ReceivingWitnessFault == receiving_witness_fault::repeated) {
				static_cast<void>(aWitness(ownership));
			}
			return std::make_unique<fake_pack_sink>(State, aOperation);
		}

		[[nodiscard]] auto observe_preimport(
			const application::completed_git_pack_receipt& aReceipt,
			const std::stop_token                          aStop)
			-> application::git_cache_result<
				application::git_preimport_observation> override
		{
			State.Calls.emplace_back("cache.observe-preimport");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			auto token    = std::array<std::uint8_t, 32U>{ };
			token.front() = 0x72U;
			return application::git_preimport_observation{
				.Operation               = aReceipt.Operation,
				.ExpectedPack            = aReceipt.ExpectedPack,
				.RepositoryBytes         = 0U,
				.ExpectedPackPreexisting = false,
				.OpaqueToken             = token,
			};
		}

		[[nodiscard]] auto prepare_fixture_revision(
			const application::git_prepare_revision_request&                      aRequest,
			const std::stop_token                                                 aStop,
			const std::function<bool(const application::git_prepared_revision&)>& aWitness,
			const application::git_revision_residency                             aResidency)
			-> application::git_cache_result<application::git_prepared_revision>
		{
			State.Calls.emplace_back(aResidency == application::git_revision_residency::sparse_commit_trees
										 ? "cache.prepare-sparse-revision"
										 : "cache.prepare-revision");
			++State.CacheMutationCalls;
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (const auto failure = take_failure(
					State.PrepareCacheFailures, "cache.prepare-revision")) {
				return std::unexpected{ *failure };
			}
			auto prepared = application::git_prepared_revision{
				.Operation     = aRequest.Authorization.Operation,
				.Pack          = aRequest.Authorization.ExpectedPack,
				.InstalledPack = artifact(
					application::git_artifact_role::installed_pack,
					0x52U,
					8U),
				.InstalledIndex = artifact(
					application::git_artifact_role::installed_index,
					0x53U,
					4U),
				.Base          = aRequest.Base,
				.BaseTree      = oid<core::git_tree_oid>(60U),
				.Head          = aRequest.Head,
				.HeadTree      = oid<core::git_tree_oid>(70U),
				.HeadLineage   = State.PreparedHeadLineage,
				.ImportedBytes = 8U,
				.Residency     = aResidency,
			};
			if (aResidency == application::git_revision_residency::sparse_commit_trees) {
				prepared.HeadLineage = aRequest.PriorHead
					? core::head_lineage::unknown
					: core::head_lineage::not_applicable;
			}
			if (aWitness && State.InstallWitnessFault != install_witness_fault::omitted && !aWitness(prepared)) {
				return std::unexpected{ application::git_cache_failure::authorization_mismatch };
			}
			if (aWitness && State.InstallWitnessFault == install_witness_fault::repeated) {
				static_cast<void>(aWitness(prepared));
			}
			if (State.InstallWitnessFault == install_witness_fault::changed_result) {
				prepared.InstalledPack.Identity.Bytes.back() ^= 1U;
			}
			return prepared;
		}

		[[nodiscard]] auto prepare_revision(
			const application::git_prepare_revision_request&                      aRequest,
			const std::stop_token                                                 aStop,
			const std::function<bool(const application::git_prepared_revision&)>& aWitness)
			-> application::git_cache_result<application::git_prepared_revision> override
		{
			return prepare_fixture_revision(aRequest, aStop, aWitness,
											application::git_revision_residency::complete_closure);
		}

		[[nodiscard]] auto prepare_sparse_revision(
			const application::git_prepare_revision_request&                      aRequest,
			const std::stop_token                                                 aStop,
			const std::function<bool(const application::git_prepared_revision&)>& aWitness)
			-> application::git_cache_result<application::git_prepared_revision> override
		{
			return prepare_fixture_revision(aRequest, aStop, aWitness,
											application::git_revision_residency::sparse_commit_trees);
		}

		[[nodiscard]] auto promote_retained(
			const application::git_prepared_revision&           aPrepared,
			const application::git_ref_promotion_authorization& aAuthorization,
			const std::stop_token) -> application::git_cache_result<application::git_retained_ref_pair> override
		{
			State.Calls.emplace_back("cache.promote");
			++State.CacheMutationCalls;
			State.Retained = application::git_retained_ref_pair{
				.Scope     = aAuthorization.Scope,
				.Ordinal   = aAuthorization.Ordinal,
				.Base      = aAuthorization.Base,
				.Head      = aAuthorization.Head,
				.Residency = aPrepared.Residency,
			};
			State.RetainedPairs.push_back(*State.Retained);
			if (State.PromoteAppliesThenFails) {
				State.PromoteAppliesThenFails = false;
				return std::unexpected{
					application::git_cache_failure::process_failure
				};
			}
			return *State.Retained;
		}

		[[nodiscard]] auto inspect_retained(
			const core::epoch_ordinal,
			const std::stop_token) -> application::git_cache_result<application::git_retained_ref_pair> override
		{
			State.Calls.emplace_back("cache.inspect-retained");
			if (!State.Retained) {
				return std::unexpected{ application::git_cache_failure::ref_conflict };
			}
			return *State.Retained;
		}

		[[nodiscard]] auto inspect_sparse_retained(
			const core::epoch_ordinal aOrdinal,
			const std::stop_token     aStop)
			-> application::git_cache_result<application::git_retained_ref_pair> override
		{
			State.Calls.emplace_back("cache.inspect-sparse-retained");
			const auto found = std::ranges::find(State.RetainedPairs, aOrdinal,
												 &application::git_retained_ref_pair::Ordinal);
			if (aStop.stop_requested() || found == State.RetainedPairs.end()
				|| found->Residency != application::git_revision_residency::sparse_commit_trees) {
				return std::unexpected{ application::git_cache_failure::ref_conflict };
			}
			return *found;
		}

		[[nodiscard]] auto inspect_commit_object(
			const core::git_commit_oid aCommit,
			const std::stop_token) -> application::git_cache_result<application::git_cache_commit_inspection> override
		{
			State.Calls.emplace_back("cache.inspect-commit");
			if (State.SparseObjects) {
				return std::unexpected{ application::git_cache_failure::object_mismatch };
			}
			return application::git_cache_commit_inspection{
				.Commit = aCommit,
				.Tree   = oid<core::git_tree_oid>(80U),
			};
		}

		[[nodiscard]] auto inspect_sparse_commit_object(
			const core::git_commit_oid aCommit,
			const std::stop_token      aStop)
			-> application::git_cache_result<application::git_cache_commit_inspection> override
		{
			State.Calls.emplace_back("cache.inspect-sparse-commit");
			if (aStop.stop_requested() || !State.SparseObjects) {
				return std::unexpected{ application::git_cache_failure::object_mismatch };
			}
			return application::git_cache_commit_inspection{
				.Commit    = aCommit,
				.Tree      = State.SparseTreeMismatch ? oid<core::git_tree_oid>(90U)
					: aCommit == oid<core::git_commit_oid>(10U)
					? oid<core::git_tree_oid>(60U)
					: oid<core::git_tree_oid>(70U),
				.Residency = application::git_revision_residency::sparse_commit_trees,
			};
		}

		[[nodiscard]] auto materialize_agent_workspace(
			const application::agent_workspace_materialization_request&,
			const std::stop_token aStop)
			-> application::git_cache_result<application::owned_agent_workspace> override
		{
			State.Calls.emplace_back("cache.materialize-agent-workspace");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (State.WorkspaceMaterializationFailure.has_value()) {
				auto failure = std::move(*State.WorkspaceMaterializationFailure);
				State.WorkspaceMaterializationFailure.reset();
				return std::unexpected{ std::move(failure) };
			}
			return std::unexpected{ application::git_cache_failure::invalid_request };
		}

		[[nodiscard]] auto cleanup_agent_workspace(
			const application::owned_agent_workspace&,
			const std::stop_token aStop) -> application::git_cache_result<void> override
		{
			State.Calls.emplace_back("cache.cleanup-agent-workspace");
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (State.WorkspaceCleanupFailure.has_value()) {
				auto failure = std::move(*State.WorkspaceCleanupFailure);
				State.WorkspaceCleanupFailure.reset();
				return std::unexpected{ std::move(failure) };
			}
			return { };
		}

		[[nodiscard]] auto materialize_blob(
			const application::git_cache_blob_request& aRequest,
			const std::stop_token                      aStop)
			-> application::git_cache_result<std::vector<std::uint8_t>> override
		{
			State.Calls.emplace_back("cache.materialize-blob");
			State.BlobRequests.push_back(aRequest);
			if (State.DuringBlobMaterialization) {
				State.DuringBlobMaterialization(aStop);
			}
			if (aStop.stop_requested() && !State.BlobMaterializationIgnoresStop) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (State.CachedBlobMissing) {
				return std::unexpected{ application::git_cache_failure::object_mismatch };
			}
			return State.CachedBlobOverride.value_or(std::vector<std::uint8_t>{ 'b', 'l', 'o', 'b', '\n' });
		}

		[[nodiscard]] auto changed_entries(const application::git_cache_changed_entries_request& aRequest, std::stop_token aStop) -> application::git_cache_result<std::vector<application::git_cache_changed_entry>> override
		{
			State.CatalogueRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (State.CatalogueFailure) {
				return std::unexpected{ *State.CatalogueFailure };
			}
			return State.CatalogueEntries;
		}

		[[nodiscard]] auto find_blob_at_path(
			const application::git_cache_tree_entry_request& aRequest,
			const std::stop_token                            aStop)
			-> application::git_cache_result<
				std::optional<core::git_blob_oid>> override
		{
			State.Calls.emplace_back("cache.find-blob-at-path");
			State.TreeEntryRequests.push_back(aRequest);
			if (State.DuringTreeEntryLookup) {
				State.DuringTreeEntryLookup(aStop);
			}
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (!State.TreeEntryResults.empty()) {
				auto result = std::move(State.TreeEntryResults.front());
				State.TreeEntryResults.pop_front();
				return result;
			}
			return std::optional{ oid<core::git_blob_oid>(
				static_cast<std::uint8_t>(
					aRequest.Commit.bytes().front() + 1U)) };
		}

		[[nodiscard]] auto derive_comparison_base(
			const application::git_cache_comparison_base_request& aRequest,
			const std::stop_token                                 aStop)
			-> application::git_cache_result<application::git_cache_comparison_base_observation> override
		{
			State.Calls.emplace_back("cache.derive-comparison-base");
			State.ComparisonBaseRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (State.ComparisonBaseResults.empty()) {
				return application::git_cache_comparison_base_observation{ };
			}
			auto result = std::move(State.ComparisonBaseResults.front());
			State.ComparisonBaseResults.pop_front();
			return result;
		}

		[[nodiscard]] auto find_rename_pairs(
			const application::git_cache_rename_pair_request& aRequest,
			const std::stop_token                             aStop)
			-> application::git_cache_result<
				application::git_rename_pair_observation> override
		{
			State.Calls.emplace_back("cache.find-rename-pairs");
			State.RenamePairRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (!State.RenamePairResults.empty()) {
				auto result = std::move(State.RenamePairResults.front());
				State.RenamePairResults.pop_front();
				return result;
			}
			return application::git_rename_pair_observation{ };
		}

		[[nodiscard]] auto prune_retained(
			const application::git_ref_prune_authorization& aAuthorization,
			const std::stop_token) -> application::git_cache_result<void> override
		{
			State.Calls.emplace_back("cache.prune");
			State.PruneRequests.push_back(aAuthorization);
			++State.CacheMutationCalls;
			if (const auto failure = take_failure(State.MaintenanceCacheFailures, "cache.prune")) {
				return std::unexpected{ *failure };
			}
			State.Retained.reset();
			return { };
		}

		[[nodiscard]] auto abort_operation(
			const core::epoch_operation_id,
			const std::stop_token) -> application::git_cache_result<void> override
		{
			State.Calls.emplace_back("cache.abort");
			++State.CacheMutationCalls;
			return { };
		}

		[[nodiscard]] auto inspect_operation_recovery(
			const application::git_operation_recovery_request& aRequest,
			const std::stop_token) -> application::git_cache_result<application::git_operation_recovery_observation> override
		{
			State.Calls.emplace_back("cache.inspect-recovery");
			State.RecoveryInspectionRequests.push_back(aRequest);
			if (State.RecoveryInspections.empty()) {
				return std::unexpected{
					application::git_cache_failure::invalid_request
				};
			}
			auto result = std::move(State.RecoveryInspections.front());
			State.RecoveryInspections.pop_front();
			return result;
		}

		[[nodiscard]] auto abort_recovered_operation(
			const application::git_operation_recovery_request& aRequest,
			const std::stop_token                              aStop) -> application::git_cache_result<void> override
		{
			State.Calls.emplace_back("cache.abort-exact");
			State.RecoveryAbortRequests.push_back(aRequest);
			State.RecoveryAbortStopRequested.push_back(aStop.stop_requested());
			++State.CacheMutationCalls;
			if (State.ThrowRecoveryAbort) {
				throw std::runtime_error{ "injected exact-abort failure" };
			}
			return { };
		}

		[[nodiscard]] auto complete_recovered_operation(
			const application::git_operation_recovery_request& aRequest,
			const std::stop_token) -> application::git_cache_result<void> override
		{
			State.Calls.emplace_back("cache.complete-operation");
			State.RecoveryCompletionRequests.push_back(aRequest);
			return { };
		}

	private:
		shared_state&                       State;
		application::git_cache_open_request Request;
		application::git_cache_open_request Foreign;
	};

	class fake_cache_port final : public application::git_cache_port
	{
	public:
		explicit fake_cache_port(shared_state& aState) noexcept
			: State{ aState }
		{
		}

		[[nodiscard]] auto open(
			const application::git_cache_open_request& aRequest,
			const std::stop_token                      aStop) -> application::git_cache_result<std::unique_ptr<application::git_cache_session>> override
		{
			State.Calls.emplace_back("cache.open");
			State.CacheOpenRequests.push_back(aRequest);
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_cache_failure::cancelled };
			}
			if (!State.CacheOpenFailures.empty()) {
				const auto failure = State.CacheOpenFailures.front();
				State.CacheOpenFailures.pop_front();
				return std::unexpected{ failure };
			}
			return std::make_unique<fake_cache_session>(State, aRequest);
		}

		[[nodiscard]] auto delete_cache(const application::git_cache_open_request&,
										const application::git_cache_delete_authorization& aAuthorization,
										const std::stop_token) -> application::git_cache_result<void> override
		{
			State.Calls.emplace_back("cache.delete");
			State.DeleteRequests.push_back(aAuthorization);
			++State.CacheMutationCalls;
			expect(State.ActiveCacheSessions == 0U,
				   "whole-cache deletion closes the live session before quarantine");
			if (const auto failure = take_failure(State.MaintenanceCacheFailures, "cache.delete")) {
				return std::unexpected{ *failure };
			}
			return { };
		}

	private:
		shared_state& State;
	};

	class fake_transport final : public application::git_smart_transport_port
	{
	public:
		fake_transport(
			shared_state&                          aState,
			application::git_smart_request_context aContext)
			: State{ aState }
			, Context{ std::move(aContext) }
		{
		}

		void revoke() noexcept override
		{
			Revoked = true;
		}

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

		[[nodiscard]] auto read_exact_blob(
			const application::git_smart_exact_blob_request& aRequest,
			const std::stop_token                            aStop)
			-> application::git_smart_transport_result<std::vector<std::byte>> override
		{
			// Not recorded in `Calls`: a read lent to the diff worker runs
			// while the actor appends there. [[BPR-1360]]
			auto span = shared_state::exact_blob_span{ .Blob = aRequest.ExpectedBlob };
			{
				std::scoped_lock lock{ State.ExactBlobMutex };
				State.ExactBlobRequests.push_back(aRequest);
				const auto latency          = State.ExactBlobLatency;
				span.Started                = std::chrono::steady_clock::now();
				span.Ended                  = span.Started + latency;
				State.ExactBlobMostInFlight = std::max(State.ExactBlobMostInFlight, ++State.ExactBlobReadsInFlight);
			}
			if (State.ExactBlobLatencyObservesStop) {
				while (!aStop.stop_requested() && std::chrono::steady_clock::now() < span.Ended) {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
				}
				if (aStop.stop_requested()) {
					++State.ExactBlobStopsObserved;
					span.Stopped = true;
				}
			} else {
				std::this_thread::sleep_until(span.Ended);
			}
			{
				std::scoped_lock lock{ State.ExactBlobMutex };
				span.Ended = std::chrono::steady_clock::now();
				State.ExactBlobSpans.push_back(span);
				--State.ExactBlobReadsInFlight;
			}
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			if (State.ExactBlobFailure) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = *State.ExactBlobFailure,
				} };
			}
			return bytes("exact selected blob");
		}

		[[nodiscard]] auto read_exact_blobs(
			const application::git_smart_exact_blobs_request& aRequest,
			const std::stop_token                             aStop)
			-> application::git_smart_transport_result<application::git_smart_exact_blobs> override
		{
			State.Calls.emplace_back("transport.read-exact-blobs");
			State.ExactBlobBatches.push_back(aRequest);
			std::this_thread::sleep_for(State.BatchLatency);
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			if (State.BatchUnsupported) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::unavailable,
				} };
			}
			auto result = application::git_smart_exact_blobs{ };
			for (const auto& blob : aRequest.ExpectedBlobs) {
				const auto found = std::ranges::find(State.BatchContents, blob, &std::pair<core::git_blob_oid, std::string>::first);
				result.push_back(found == State.BatchContents.end() ? std::nullopt : std::optional{ bytes(found->second) });
			}
			return result;
		}

		[[nodiscard]] auto discover(
			const application::git_smart_discovery_request& aRequest,
			const std::stop_token                           aStop)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response> override
		{
			State.Calls.emplace_back("transport.discover");
			if (State.DiscoveryFailure) {
				return std::unexpected{ application::git_smart_transport_error{ .Failure = *State.DiscoveryFailure, .Step = State.DiscoveryStep } };
			}
			if (State.MalformedDiscovery) {
				return application::git_smart_discovery_response{ .Body = bytes("not-pkt-line") };
			}
			++State.TransportCalls;
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			return application::git_smart_discovery_response{
				.Body = bytes(
					packet("version 2\n") + packet("ls-refs=unborn\n")
					+ packet("fetch=shallow wait-for-done\n")
					+ packet("object-format=sha1\n")
					+ packet("server-option\n") + "0000"),
			};
		}

		[[nodiscard]] auto ls_refs(
			const application::git_smart_ls_refs_request& aRequest,
			const std::stop_token                         aStop)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response> override
		{
			State.Calls.emplace_back("transport.ls-refs");
			++State.TransportCalls;
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			const auto base = std::string{ core::canonical_hex(Context.Revision.Base).view() };
			const auto head = std::string{ core::canonical_hex(Context.Revision.Head).view() };
			return application::git_smart_ls_refs_response{
				.Body = bytes(
					packet(base + " refs/heads/main\n")
					+ packet(
						head + " refs/pull/"
						+ std::to_string(Context.Authority.Number.value())
						+ "/head\n")
					+ "0000"),
			};
		}

		[[nodiscard]] auto fetch(
			const application::git_smart_fetch_request& aRequest,
			application::git_smart_wire_consumer&       aConsumer,
			const std::stop_token                       aStop)
			-> application::git_smart_transport_result<void> override
		{
			State.Calls.emplace_back("transport.fetch");
			++State.TransportCalls;
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			auto packData = std::string(1U, static_cast<char>(1));
			packData += "PACKdata";
			const auto body = bytes(
				packet("packfile\n") + packet(packData) + "0000");
			if (!aConsumer.consume(body)) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::
						consumer_rejected,
				} };
			}
			return { };
		}

	private:
		shared_state&                          State;
		application::git_smart_request_context Context;
		bool                                   Revoked = false;
	};

	class fake_smart_authority final
		: public application::git_smart_authority_lease
	{
	public:
		fake_smart_authority(
			shared_state&                            aState,
			application::git_smart_authority_binding aBinding,
			const std::uint64_t                      aLabel)
			: State{ aState }
			, Binding{ std::move(aBinding) }
			, Label{ aLabel }
		{
		}

		[[nodiscard]] auto binding() const noexcept
			-> const application::git_smart_authority_binding& override
		{
			return Binding;
		}

		[[nodiscard]] auto supports_sparse_commit_trees() const noexcept -> bool override
		{
			return State.SupportsSparse && !Revoked;
		}

		[[nodiscard]] auto acquire(
			const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<
				application::git_smart_transport> override
		{
			State.Calls.emplace_back("transport.acquire");
			if (State.AcquireFailure) {
				return std::unexpected{ application::git_smart_transport_error{ .Failure = *State.AcquireFailure } };
			}
			if (Revoked) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			return std::make_shared<fake_transport>(State, aContext);
		}

		void revoke() noexcept override
		{
			if (Revoked) {
				return;
			}
			Revoked = true;
			State.AuthorityRevocations.push_back(Label);
			State.Calls.emplace_back("transport.revoke-authority");
		}

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

	private:
		shared_state&                            State;
		application::git_smart_authority_binding Binding;
		std::uint64_t                            Label   = 0U;
		bool                                     Revoked = false;
	};

	[[nodiscard]] auto make_handoff(
		shared_state&                                        aState,
		const std::uint64_t                                  aSnapshot = 1U,
		const std::uint8_t                                   aHead     = 20U,
		const bool                                           aSmart    = false,
		std::optional<core::current_viewer_review_candidate> aReview   = std::nullopt)
		-> application::accepted_pull_request_handoff
	{
		const auto baseRef = application::parse_git_base_branch_ref("refs/heads/main");
		expect(baseRef.has_value(), "fixture base ref is valid");
		auto handoff = application::accepted_pull_request_handoff{
			.ApplicationSession    = core::application_session_id{ 91U },
			.Profile               = core::host_profile_id{ 11U },
			.ProfileRevision       = core::github_profile_revision{ 21U },
			.Host                  = core::git_host_id{ 31U },
			.HostAuthority         = opaque<core::host_authority_fingerprint>(2U),
			.Hostname              = application::github_canonical_hostname{ "github.com" },
			.Account               = core::github_account_id{ 41U },
			.Viewer                = core::github_viewer_actor_id{ 51U },
			.Repository            = core::repository_id{ 61U },
			.Locator               = { .Owner = "octo-org", .Name = "review-repository" },
			.PullRequest           = core::pull_request_id{ 71U },
			.Number                = core::pull_request_number{ 81U },
			.Snapshot              = core::provider_snapshot_id{ aSnapshot },
			.IdentityGeneration    = core::github_identity_generation{ 3U },
			.PullRequestGeneration = core::request_generation{ aSnapshot + 3U },
			.Origin                = core::pull_request_revision_origin::live,
			.ObjectFormat          = core::git_hash_algorithm::sha1,
			.BaseBranch            = opaque<core::base_branch_id>(30U),
			.BaseRef               = *baseRef,
			.Base                  = oid<core::git_commit_oid>(10U),
			.Head                  = oid<core::git_commit_oid>(aHead),
			.CurrentViewerReview   = std::move(aReview),
		};
		if (aSmart) {
			handoff.SmartGit = std::make_unique<fake_smart_authority>(
				aState, application::authority_binding(handoff), aSnapshot);
		}
		return handoff;
	}

	[[nodiscard]] auto make_repository_handoff(
		shared_state&       aState,
		const std::uint64_t aRepository,
		const std::uint64_t aSnapshot)
		-> application::accepted_pull_request_handoff
	{
		auto handoff       = make_handoff(aState, aSnapshot, 20U);
		handoff.Repository = core::repository_id{ aRepository };
		handoff.SmartGit   = std::make_unique<fake_smart_authority>(
			aState, application::authority_binding(handoff), aSnapshot);
		return handoff;
	}

	[[nodiscard]] auto recovery_record(
		shared_state&                                    aState,
		const application::epoch_catalog_operation_phase aPhase,
		const std::uint8_t                               aOperationSeed = 0x31U)
		-> application::append_operation_record
	{
		const auto handoff   = make_handoff(aState, 1U, 20U);
		const auto operation = opaque<core::epoch_operation_id>(aOperationSeed);
		const auto revision  = core::accepted_pull_request_revision{
			.Source = {
				.Observation = opaque<core::epoch_source_observation_id>(
					static_cast<std::uint8_t>(aOperationSeed + 1U)),
				.Snapshot              = handoff.Snapshot,
				.IdentityGeneration    = handoff.IdentityGeneration,
				.PullRequestGeneration = handoff.PullRequestGeneration,
			},
			.Origin     = handoff.Origin,
			.BaseBranch = handoff.BaseBranch,
			.Base       = handoff.Base,
			.Head       = handoff.Head,
		};
		const auto pack   = pack_oid(0x42U);
		auto       record = application::append_operation_record{
			.Context = {
				.ApplicationSession = handoff.ApplicationSession,
				.Request            = core::epoch_request_id{ 101U },
				.Generation         = core::epoch_generation{ 1U },
				.Operation          = operation,
				.Revision           = revision,
				.Catalog            = aState.Binding.Catalog,
			},
			.ReservedOrdinal   = core::epoch_ordinal{ 1U },
			.OperationRevision = application::epoch_catalog_operation_revision{ 5 },
			.Phase             = aPhase,
			.StagingBase       = {
				.Authority = aState.Binding.Authority.Scope,
				.Ordinal   = core::epoch_ordinal{ 1U },
				.Operation = operation,
				.Staging   = true,
			},
			.StagingHead = {
				.Authority = aState.Binding.Authority.Scope,
				.Ordinal   = core::epoch_ordinal{ 1U },
				.Operation = operation,
				.Staging   = true,
				.Head      = true,
			},
			.FinalBase = {
				.Authority = aState.Binding.Authority.Scope,
				.Ordinal   = core::epoch_ordinal{ 1U },
			},
			.FinalHead = {
				.Authority = aState.Binding.Authority.Scope,
				.Ordinal   = core::epoch_ordinal{ 1U },
				.Head      = true,
			},
			.Spool = application::durable_git_spool_fact{
				.Operation    = operation,
				.ExpectedPack = pack,
				.ObservedPack = pack,
				.Spool        = artifact(application::git_artifact_role::completed_spool, 0x51U, 8U),
			},
			.Preimport = application::durable_git_preimport_fact{
				.Operation               = operation,
				.ExpectedPack            = pack,
				.RepositoryBytes         = 0U,
				.ExpectedPackPreexisting = false,
			},
			.Install = application::durable_git_install_fact{
				.Operation      = operation,
				.Pack           = pack,
				.InstalledPack  = artifact(application::git_artifact_role::installed_pack, 0x52U, 8U),
				.InstalledIndex = artifact(application::git_artifact_role::installed_index, 0x53U, 4U),
				.Base           = revision.Base,
				.BaseTree       = oid<core::git_tree_oid>(60U),
				.Head           = revision.Head,
				.HeadTree       = oid<core::git_tree_oid>(70U),
				.HeadLineage    = core::head_lineage::not_applicable,
			},
		};

		using enum application::epoch_catalog_operation_phase;
		if (
			aPhase == commit_authorized || aPhase == ref_commit_in_progress
			|| aPhase == refs_committed || aPhase == visible_cleanup_pending) {
			record.Candidate = core::append_epoch_mutation{
				.Epoch = summary_for(
					revision, aState.Binding.Catalog, record.ReservedOrdinal),
			};
		}
		expect(application::valid(record), "recovery operation fixture is valid");
		return record;
	}

	[[nodiscard]] auto recovery_observation(
		const application::append_operation_record&  aRecord,
		const application::git_recovery_ref_topology aRefs,
		const application::git_recovery_match        aStaging,
		const application::git_recovery_match        aFinal)
		-> application::git_operation_recovery_observation
	{
		return {
			.Operation = aRecord.Context.Operation,
			.Spool     = application::git_recovery_match::exact,
			.Installed = application::git_recovery_match::exact,
			.Staging   = aStaging,
			.Final     = aFinal,
			.Objects   = application::git_recovery_object_match::exact,
			.Refs      = aRefs,
		};
	}

	void seed_recovery(
		shared_state&                               aState,
		const application::append_operation_record& aRecord)
	{
		aState.Operation = aRecord;
		aState.RecoveryPages.push_back(application::epoch_recovery_page{
			.Incarnation = aState.Binding.Catalog.Incarnation,
			.Operations  = { aRecord },
		});
	}

	[[nodiscard]] auto maintenance_recovery_record(shared_state&                                    aState,
												   const application::epoch_catalog_operation_kind  aKind,
												   const application::epoch_catalog_operation_phase aPhase,
												   const std::uint8_t                               aOperationSeed = 0x61U)
		-> application::maintenance_operation_record
	{
		auto record = application::maintenance_operation_record{
			.Incarnation       = aState.Binding.Catalog.Incarnation,
			.Operation         = opaque<core::epoch_operation_id>(aOperationSeed),
			.Kind              = aKind,
			.Phase             = aPhase,
			.OperationRevision = application::epoch_catalog_operation_revision{ 2 },
			.Cache             = aState.Binding.Authority.Cache,
		};
		if (aKind == application::epoch_catalog_operation_kind::prune) {
			const auto epoch = core::review_epoch_id{
				.Authority   = aState.Binding.Authority.Scope,
				.Incarnation = aState.Binding.Catalog.Incarnation,
				.Ordinal     = core::epoch_ordinal{ 1 },
			};
			record.Epochs.push_back(application::prune_epoch_fact{
				.Epoch    = epoch,
				.Retained = application::git_retained_ref_pair{
					.Scope   = epoch.Authority,
					.Ordinal = epoch.Ordinal,
					.Base    = oid<core::git_commit_oid>(10U),
					.Head    = oid<core::git_commit_oid>(20U),
				},
				.BaseRef = application::epoch_catalog_ref_identity{
					.Authority = epoch.Authority,
					.Ordinal   = epoch.Ordinal,
				},
				.HeadRef = application::epoch_catalog_ref_identity{
					.Authority = epoch.Authority,
					.Ordinal   = epoch.Ordinal,
					.Head      = true,
				},
			});
		} else {
			record.Scopes.push_back(application::cache_delete_scope_fact{
				.Authority        = aState.Binding.Authority.Scope,
				.ExpectedRevision = aState.Binding.Catalog.Revision,
			});
		}
		return record;
	}

	void seed_maintenance_recovery(shared_state&                                    aState,
								   const application::maintenance_operation_record& aRecord)
	{
		aState.Maintenance = aRecord;
		aState.RecoveryPages.push_back(application::epoch_recovery_page{
			.Incarnation = aState.Binding.Catalog.Incarnation,
			.Operations  = { aRecord },
		});
	}

	struct fixture
	{
		shared_state                                         State;
		fake_cache_port                                      Caches{ State };
		fake_catalog_port                                    Catalogs{ State };
		fake_entropy                                         Entropy{ State };
		fake_clock                                           Clock{ State };
		queued_executor                                      Effects{ false };
		queued_executor                                      Decisions{ true };
		fake_projection                                      Projections{ State, Decisions };
		std::unique_ptr<application::epoch_application_host> Host;

		explicit fixture(const application::epoch_host_limits aLimits = { }, std::shared_ptr<application::diagnostic_log_port> aLog = { })
		{
			State.Binding = application::epoch_catalog_binding{
				.Authority    = authority(),
				.Catalog      = catalog_identity(),
				.ObjectFormat = core::git_hash_algorithm::sha1,
			};
			State.Snapshot = application::epoch_catalog_snapshot{
				.Binding     = State.Binding,
				.NextOrdinal = core::epoch_ordinal{ 1 },
			};
			Host = std::make_unique<application::epoch_application_host>(
				core::epoch_state{
					.ApplicationSession = core::application_session_id{ 91U },
				},
				Caches,
				Catalogs,
				Entropy,
				Clock,
				Effects,
				Decisions,
				Projections,
				aLimits, std::move(aLog));
		}

		~fixture()
		{
			Host->shutdown();
			while (Effects.pending() != 0U || Decisions.pending() != 0U) {
				if (Effects.pending() != 0U) {
					Effects.run_next();
				}
				if (Decisions.pending() != 0U) {
					Decisions.run_next();
				}
			}
		}
	};

	void run_effect_completion(fixture& aValue, const std::size_t aIndex = 0U)
	{
		aValue.Effects.run(aIndex);
		expect(
			aValue.Decisions.pending() != 0U,
			"each completed worker posts one decision-lane terminal");
		aValue.Decisions.run_next();
	}

	void drive_through_preparation(fixture& aValue)
	{
		run_effect_completion(aValue);
		run_effect_completion(aValue);
		run_effect_completion(aValue);
	}

	void drain(fixture& aValue)
	{
		while (aValue.Effects.pending() != 0U || aValue.Decisions.pending() != 0U) {
			if (aValue.Effects.pending() != 0U) {
				aValue.Effects.run_next();
			}
			if (aValue.Decisions.pending() != 0U) {
				aValue.Decisions.run_next();
			}
		}
	}

	[[nodiscard]] auto force_push(const std::uint64_t aEvent, const std::uint8_t aHead,
								  const std::int64_t aTime = 0) -> application::epoch_force_push_observation
	{
		const auto eventTime = aTime != 0 ? aTime : 1'789'000'000'000LL + (static_cast<std::int64_t>(aEvent) - 101) * 1'000;
		return { .EventId = aEvent, .Head = oid<core::git_commit_oid>(aHead), .EventUtcMilliseconds = eventTime };
	}

	void accept_history(fixture& aValue, const std::uint64_t aSnapshot, const std::uint8_t aHead,
						std::vector<application::epoch_force_push_observation> aEvents,
						const application::epoch_timeline_availability         aAvailability = application::epoch_timeline_availability::available,
						const std::optional<std::int64_t>                      aObservedUtc  = std::nullopt)
	{
		aValue.Clock.set_next_utc(aObservedUtc.value_or(1'789'000'000'500LL + (static_cast<std::int64_t>(aSnapshot) - 1) * 1'000));
		aValue.State.OccurrencesSupported = true;
		// This fake graph treats a newly introduced head as a descendant;
		// returning to an earlier node is explicitly non-fast-forward.
		aValue.State.PreparedHeadLineage = core::head_lineage::not_applicable;
		if (aValue.State.Snapshot.LatestLive && aValue.State.Snapshot.LatestLive->Head != oid<core::git_commit_oid>(aHead)) {
			aValue.State.PreparedHeadLineage = std::ranges::any_of(aValue.State.EpochRows,
																   [&](const auto& aEpoch) { return aEpoch.Head == oid<core::git_commit_oid>(aHead); })
				? core::head_lineage::non_fast_forward
				: core::head_lineage::descendant;
		}
		auto handoff                 = make_handoff(aValue.State, aSnapshot, aHead);
		handoff.Account              = aValue.State.Binding.Authority.Account;
		handoff.Viewer               = aValue.State.Binding.Authority.Viewer;
		handoff.Mode                 = aValue.State.Binding.Authority.Mode;
		handoff.ForcePushes          = std::move(aEvents);
		handoff.TimelineAvailability = aAvailability;
		handoff.SmartGit             = std::make_unique<fake_smart_authority>(aValue.State,
																			  application::authority_binding(handoff), aSnapshot);
		aValue.Host->accept(std::move(handoff));
		drain(aValue);
		expect(aValue.Host->state().AcceptedRevision
				   && aValue.Host->state().AcceptedRevision->Source.Snapshot == core::provider_snapshot_id{ aSnapshot },
			   "history read preserves the exact accepted live snapshot");
		expect(aValue.State.Snapshot.LatestLive
				   && aValue.State.Snapshot.LatestLive->Head == oid<core::git_commit_oid>(aHead),
			   "the accepted history handoff commits its actual head to the local catalogue");
	}

	void base_rewrite_replaces_projected_comparison_despite_sidecar_failure(const bool aDerivedBasePresent, const bool aFailAppend, const bool aReadFails = false)
	{
		fixture value;
		accept_history(value, 1U, 20U, { force_push(101U, 21U) });
		auto retained         = *value.State.Snapshot.LatestLive;
		retained.Id.Ordinal   = core::epoch_ordinal{ 2 };
		retained.Head         = oid<core::git_commit_oid>(21U);
		retained.Cause.Origin = core::epoch_origin::explicit_historical_import;
		value.State.EpochRows.push_back(retained);
		value.State.Snapshot.NextOrdinal = core::epoch_ordinal{ 3 };
		auto& occurrence                 = value.State.Occurrences.Occurrences.front();
		occurrence.LocalEpoch            = retained.Id;
		occurrence.DerivedComparisonBase = aDerivedBasePresent ? std::optional{ retained.Base } : std::nullopt;
		const auto original              = occurrence;
		accept_history(value, 2U, 20U, { force_push(101U, 21U) });
		expect(std::ranges::any_of(value.State.Updates, [&](const auto& aUpdate) {
				   return aUpdate.Catalog && aUpdate.Catalog->OccurrenceHistory
					   && std::ranges::find(aUpdate.Catalog->OccurrenceHistory->Occurrences, original) != aUpdate.Catalog->OccurrenceHistory->Occurrences.end();
			   }),
			   "old imported comparison is already projected before the replacement");
		value.State.Updates.clear();
		value.State.RefuseOccurrenceWrite = true;
		value.State.OccurrencesSupported  = !aReadFails;
		if (aFailAppend) {
			value.State.InstallFailure = application::epoch_catalog_failure::unavailable;
		}
		auto handoff                 = make_handoff(value.State, 3U, 20U);
		handoff.Base                 = oid<core::git_commit_oid>(88U);
		handoff.ForcePushes          = { force_push(101U, 21U) };
		handoff.TimelineAvailability = application::epoch_timeline_availability::available;
		handoff.SmartGit             = std::make_unique<fake_smart_authority>(value.State, application::authority_binding(handoff), 2U);
		value.Host->accept(std::move(handoff));
		drain(value);
		bool        projected           = false;
		std::size_t historyPublications = 0U;
		for (const auto& update : value.State.Updates) {
			if (!update.Catalog || !update.Catalog->OccurrenceHistory) {
				continue;
			}
			++historyPublications;
			if (aReadFails) {
				projected = true;
				expect(update.Catalog->OccurrenceHistory->Occurrences.empty() && update.Catalog->OccurrenceHistory->Incomplete,
					   "failed sidecar read replaces prior projected evidence with an explicit unavailable fallback");
			}
			for (const auto& row : update.Catalog->OccurrenceHistory->Occurrences) {
				if (row.Id == original.Id) {
					projected = true;
					expect(!row.LocalEpoch && !row.DerivedComparisonBase && !row.HistoricalBase,
						   "pending or failed base replacement never projects the stale imported comparison");
				}
			}
		}
		expect(projected && (aFailAppend || historyPublications >= 2U),
			   "refused persistence publishes safe acceptance history and reloads it after a successful commit");
		if (aFailAppend) {
			expect(value.State.Occurrences.Occurrences.front() == original
					   && value.State.Snapshot.LatestLive->Base == retained.Base,
				   "failed live append preserves durable history while its new selection stays safely unavailable");
		} else {
			expect(value.State.Snapshot.LatestLive->Base == oid<core::git_commit_oid>(88U)
					   && value.Host->state().AcceptedComparison,
				   "refused persistence does not refuse the committed replacement");
		}
	}

	void independent_catalogues_agree_on_evidenced_occurrences()
	{
		fixture first;
		fixture second;
		second.State.Binding.Catalog.Incarnation = opaque<core::epoch_catalog_incarnation_id>(88U);
		second.State.Binding.Authority.Account   = core::github_account_id{ };
		second.State.Binding.Authority.Viewer    = core::github_viewer_actor_id{ };
		second.State.Binding.Authority.Mode      = core::github_access_mode::public_preview;
		second.State.Snapshot.Binding            = second.State.Binding;
		accept_history(first, 1U, 20U, { force_push(103U, 20U, 1'789'000'000'000LL), force_push(101U, 20U, 1'789'000'000'000LL), force_push(102U, 21U, 1'789'000'000'000LL) });
		accept_history(second, 1U, 20U, { force_push(102U, 21U, 1'789'000'000'000LL), force_push(103U, 20U, 1'789'000'000'000LL), force_push(101U, 20U, 1'789'000'000'000LL) });
		const auto& left  = first.State.Occurrences;
		const auto& right = second.State.Occurrences;
		expect(left.Occurrences.size() == 3U && right.Occurrences.size() == 3U,
			   "two independent catalogue sessions each retain all three evidenced occurrences");
		for (std::size_t index = 0U; index < left.Occurrences.size(); ++index) {
			const auto& a = left.Occurrences[index];
			const auto& b = right.Occurrences[index];
			expect(a.Id == b.Id && a.Evidence == b.Evidence && a.DateUtcMilliseconds == b.DateUtcMilliseconds,
				   "anonymous and authenticated catalogues agree on identity and equal-date order despite input order and incarnation");
			expect(!a.HistoricalBase && !a.DerivedComparisonBase && !a.LocalEpoch,
				   "unknown historical anchors stay unknown and repeated current-head events stay unassociated");
			expect(a.DateSource == core::epoch_occurrence_date_source::github_event
					   && a.FirstSeen.value() != a.DateUtcMilliseconds,
				   "event time is labeled separately from local first-seen");
		}
		expect(left.Ambiguous && right.Ambiguous && left.Incomplete && right.Incomplete,
			   "two events returning to A stay distinct while the current snapshot's association is ambiguous");
		expect(std::ranges::count(left.Occurrences, oid<core::git_commit_oid>(20U),
								  [](const auto& aOccurrence) { return aOccurrence.Evidence.Head; })
				   == 2,
			   "A to B to A has two separate A identities");
		expect(std::ranges::any_of(first.State.Updates, [&](const auto& aUpdate) {
				   return aUpdate.Catalog && aUpdate.Catalog->OccurrenceHistory == left && aUpdate.Catalog->CurrentEpoch;
			   }),
			   "host publishes durable history and provisional current row on its decision lane");
		expect(first.State.ProjectionOutsideDecision == 0U && second.State.ProjectionOutsideDecision == 0U,
			   "portable history never publishes from an effect worker");
	}

	void live_returns_use_distinct_events_without_rebinding_old_occurrences()
	{
		fixture value;
		accept_history(value, 1U, 20U, { force_push(101U, 20U) });
		const auto initial = value.State.Occurrences.Occurrences.front();
		expect(!initial.LocalEpoch, "even a unique matching event does not prove the initial local snapshot is that occurrence");
		accept_history(value, 2U, 21U, { force_push(101U, 20U), force_push(102U, 21U) });
		expect(value.State.Snapshot.LatestLive->Cause.Primary == core::epoch_primary_cause::author_upload,
			   "changed-head second live handoff appends with author-upload cause");
		accept_history(value, 3U, 20U, { force_push(103U, 20U), force_push(101U, 20U), force_push(102U, 21U) });
		const auto history = value.State.Occurrences;
		const auto oldA    = std::ranges::find(history.Occurrences, initial.Id, &core::epoch_occurrence::Id);
		const auto newA    = std::ranges::find_if(history.Occurrences, [](const auto& aOccurrence) { return aOccurrence.Evidence.EventId == 103U; });
		expect(history.Occurrences.size() == 4U && history.Ambiguous && !oldA->LocalEpoch
				   && !newA->LocalEpoch && newA->Id != oldA->Id,
			   "A to B to A preserves distinct event identities and the independently observed B without fabricated event bindings");
		expect(value.State.Snapshot.LatestLive->Id.Ordinal == core::epoch_ordinal{ 3 }
				   && value.State.Snapshot.PreviousDistinctHead->Id.Ordinal == core::epoch_ordinal{ 2 },
			   "timeline sidecars consume no ordinals and preserve live latest and previous aliases");
		accept_history(value, 4U, 20U, { force_push(101U, 20U), force_push(102U, 21U), force_push(103U, 20U) });
		expect(value.State.Occurrences == history && value.State.Snapshot.NextOrdinal == core::epoch_ordinal{ 4 },
			   "same-head cache refresh preserves occurrence bindings and local ordinal");
	}

	void ordinary_transition_identity_survives_access_and_catalogue_changes()
	{
		fixture signedIn;
		fixture signedOut;
		fixture recreated;
		signedOut.State.Binding.Authority.Account   = core::github_account_id{ };
		signedOut.State.Binding.Authority.Viewer    = core::github_viewer_actor_id{ };
		signedOut.State.Binding.Authority.Mode      = core::github_access_mode::public_preview;
		signedOut.State.Binding.Catalog.Incarnation = opaque<core::epoch_catalog_incarnation_id>(88U);
		signedOut.State.Snapshot.Binding            = signedOut.State.Binding;
		recreated.State.Binding.Catalog.Incarnation = opaque<core::epoch_catalog_incarnation_id>(89U);
		recreated.State.Snapshot.Binding            = recreated.State.Binding;
		for (auto* value : { &signedIn, &signedOut, &recreated }) {
			accept_history(*value, 1U, 20U, { });
			accept_history(*value, 2U, 23U, { });
			expect(value->State.Occurrences.Occurrences.size() == 1U
					   && !value->State.Occurrences.Ambiguous && value->State.Occurrences.Incomplete,
				   "one ordinary observed transition yields one unambiguous occurrence while unseen push boundaries remain incomplete");
			expect(value->State.Snapshot.LatestLive->Cause.Primary == core::epoch_primary_cause::author_upload
					   && value->State.Snapshot.LatestLive->Id.Ordinal == core::epoch_ordinal{ 2 }
					   && value->State.Snapshot.PreviousDistinctHead->Id.Ordinal == core::epoch_ordinal{ 1 },
				   "ordinary live transition appends one author upload and preserves its previous distinct head");
		}
		const auto& observed = signedIn.State.Occurrences.Occurrences.front();
		for (const auto* value : { &signedOut, &recreated }) {
			const auto& other = value->State.Occurrences.Occurrences.front();
			expect(observed.Id == other.Id && observed.Evidence == other.Evidence
					   && observed.LocalEpoch != other.LocalEpoch,
				   "same evidenced ordinary update has equal portable identity across access modes and recreated catalogue with distinct local keys");
		}
		accept_history(recreated, 3U, 20U, { });
		expect(recreated.State.Occurrences.Ambiguous && recreated.State.Occurrences.Occurrences.size() == 1U,
			   "ambiguous return snapshot stays unresolved rather than receiving a fabricated portable identity");
	}

	void missing_events_do_not_invent_a_second_provisional_head()
	{
		fixture value;
		accept_history(value, 1U, 20U, { });
		expect(value.State.Occurrences.Occurrences.empty(), "first bare head remains provisional");
		accept_history(value, 2U, 21U, { });
		expect(value.State.Occurrences.Occurrences.size() == 1U, "one observed head transition produces one occurrence");
		const auto observed = value.State.Occurrences.Occurrences.front();
		expect(observed.Evidence.Kind == core::epoch_occurrence_kind::observed_head
				   && observed.DateSource == core::epoch_occurrence_date_source::commit_date
				   && observed.DateUtcMilliseconds == *value.State.CommitTimeSeconds * 1'000
				   && observed.FirstSeen == value.State.Snapshot.LatestLive->ObservedAt
				   && observed.HistoricalBase == value.State.Snapshot.LatestLive->Base,
			   "ordinary transition uses exact cached committer date and preserves observed base and first-seen separately");
		accept_history(value, 3U, 20U, { });
		expect(value.State.Occurrences.Occurrences.size() == 1U && value.State.Occurrences.Ambiguous
				   && value.State.Snapshot.LatestLive->Cause.Primary == core::epoch_primary_cause::force_push_rebase,
			   "return to the unresolved first A marks a gap without minting another A or suppressing live append");
	}

	void degraded_timeline_and_sidecar_failures_preserve_live_append()
	{
		for (const auto availability : { application::epoch_timeline_availability::failed,
										 application::epoch_timeline_availability::refused, application::epoch_timeline_availability::capped }) {
			fixture value;
			accept_history(value, 1U, 20U, { force_push(101U, 20U) });
			const auto old = value.State.Occurrences.Occurrences.front();
			accept_history(value, 2U, 21U, { }, availability);
			expect(value.State.Occurrences.Incomplete && value.State.Occurrences.Occurrences.size() == 2U
					   && std::ranges::find(value.State.Occurrences.Occurrences, old.Id, &core::epoch_occurrence::Id) != value.State.Occurrences.Occurrences.end()
					   && value.State.Snapshot.LatestLive->Cause.Primary == core::epoch_primary_cause::author_upload,
				   "failed refused and capped timelines retain known history and append the exact live author update");
		}
		fixture value;
		accept_history(value, 1U, 20U, { force_push(101U, 20U) });
		value.State.ThrowOccurrenceWrite = true;
		accept_history(value, 2U, 21U, { }, application::epoch_timeline_availability::failed);
		expect(value.Host->state().Catalog == value.State.Binding.Catalog
				   && value.State.Snapshot.LatestLive->Cause.Primary == core::epoch_primary_cause::author_upload
				   && value.Host->state().AcceptedComparison,
			   "optional sidecar exceptions cannot replace a committed live success terminal or break its comparison");
	}

	void refused_timeline_reconciliation_preserves_base_rewrite()
	{
		fixture value;
		accept_history(value, 1U, 20U, { force_push(101U, 21U) });
		value.State.RefuseOccurrenceWrite = true;
		auto handoff                      = make_handoff(value.State, 2U, 20U);
		handoff.Base                      = oid<core::git_commit_oid>(88U);
		handoff.ForcePushes               = { force_push(101U, 21U) };
		handoff.TimelineAvailability      = application::epoch_timeline_availability::available;
		handoff.SmartGit                  = std::make_unique<fake_smart_authority>(value.State, application::authority_binding(handoff), 2U);
		value.Host->accept(std::move(handoff));
		drain(value);
		expect(value.Host->state().Catalog == value.State.Binding.Catalog
				   && value.State.Snapshot.LatestLive->Base == oid<core::git_commit_oid>(88U)
				   && value.Host->state().AcceptedComparison,
			   "invalid_request from supplementary reconciliation cannot refuse the live base replacement or its comparison");
	}

	void delayed_event_keeps_observed_head_separate_without_causal_equivalence()
	{
		fixture value;
		fixture fresh;
		accept_history(value, 1U, 20U, { force_push(101U, 20U) });
		accept_history(value, 2U, 21U, { }, application::epoch_timeline_availability::failed);
		const auto observed = *std::ranges::find_if(value.State.Occurrences.Occurrences,
													[](const auto& aOccurrence) { return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::observed_head; });
		const auto latest   = *value.State.Snapshot.LatestLive;
		accept_history(value, 3U, 21U, { force_push(101U, 20U), force_push(102U, 21U) });
		accept_history(fresh, 1U, 21U, { force_push(102U, 21U), force_push(101U, 20U) });
		expect(value.State.Occurrences.Occurrences.size() == 3U && fresh.State.Occurrences.Occurrences.size() == 2U
				   && value.State.Occurrences.Ambiguous,
			   "a reader that observed B retains its separate evidence when a delayed event cannot prove occurrence equivalence");
		for (const auto& event : fresh.State.Occurrences.Occurrences) {
			const auto found = std::ranges::find(value.State.Occurrences.Occurrences, event.Id, &core::epoch_occurrence::Id);
			expect(found != value.State.Occurrences.Occurrences.end() && found->Evidence == event.Evidence && !found->LocalEpoch,
				   "both readers agree on force-event identities without falsely claiming identical local observation populations");
		}
		expect(std::ranges::find(value.State.Occurrences.Occurrences, observed) != value.State.Occurrences.Occurrences.end()
				   && value.State.Snapshot.LatestLive == latest && value.State.Snapshot.NextOrdinal == core::epoch_ordinal{ 3 }
				   && value.State.SupersededOccurrences.empty(),
			   "delayed evidence neither removes observed B nor changes its local epoch or observed base");
		const auto retained = value.State.Occurrences;
		accept_history(value, 4U, 21U, { force_push(101U, 20U), force_push(102U, 21U) });
		expect(value.State.SupersededOccurrences.empty() && value.State.Occurrences == retained,
			   "repeated refresh supplies no missing causal discriminator and cannot authorize supersession");
	}

	void existing_durable_event_binding_is_preserved_without_new_supersession()
	{
		fixture value;
		accept_history(value, 1U, 20U, { force_push(101U, 20U) });
		// A trusted catalogue may contain an association established by an
		// earlier evidence source. This host preserves it, not re-derives it.
		value.State.Occurrences.Occurrences.front().LocalEpoch = value.State.Snapshot.LatestLive->Id;
		const auto bound                                       = value.State.Occurrences.Occurrences.front();
		accept_history(value, 2U, 20U, { force_push(101U, 20U) });
		expect(value.State.Occurrences.Occurrences.front() == bound && value.State.SupersededOccurrences.empty(),
			   "an existing exact durable binding survives while no unproved CAS is issued");
	}

	void old_unbound_event_cannot_identify_a_new_return_to_its_head()
	{
		for (const auto availability : { application::epoch_timeline_availability::failed,
										 application::epoch_timeline_availability::refused, application::epoch_timeline_availability::capped,
										 application::epoch_timeline_availability::available }) {
			fixture value;
			accept_history(value, 1U, 21U, { force_push(101U, 20U) });
			expect(!value.State.Occurrences.Occurrences.front().LocalEpoch,
				   "historical A event starts unbound while current B is provisional");
			accept_history(value, 2U, 20U,
						   availability == application::epoch_timeline_availability::available
							   ? std::vector{ force_push(101U, 20U) }
							   : std::vector<application::epoch_force_push_observation>{ },
						   availability);
			expect(value.State.Occurrences.Ambiguous && value.State.Occurrences.Incomplete
					   && value.State.Occurrences.Occurrences.size() == 2U
					   && !value.State.Occurrences.Occurrences.front().LocalEpoch
					   && value.State.Snapshot.LatestLive->Id.Ordinal == core::epoch_ordinal{ 2 },
				   "a newly observed return to A cannot borrow the old unbound event from stored or repeated timeline evidence");
			accept_history(value, 3U, 20U, { force_push(101U, 20U) });
			expect(value.State.Occurrences.Ambiguous && value.State.Occurrences.Occurrences.size() == 2U
					   && !value.State.Occurrences.Occurrences.front().LocalEpoch,
				   "a later same-head refresh cannot turn the previously refused stale-event match into a binding");
		}
	}

	void delayed_event_keeps_older_observation_when_current_head_changed()
	{
		fixture value;
		accept_history(value, 1U, 20U, { force_push(101U, 20U) });
		accept_history(value, 2U, 21U, { }, application::epoch_timeline_availability::failed);
		const auto observedB = *std::ranges::find_if(value.State.Occurrences.Occurrences,
													 [](const auto& aOccurrence) { return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::observed_head; });
		const auto epochB    = *value.State.Snapshot.LatestLive;
		accept_history(value, 3U, 22U, { force_push(103U, 22U), force_push(101U, 20U), force_push(102U, 21U) });
		const auto eventB = std::ranges::find_if(value.State.Occurrences.Occurrences,
												 [](const auto& aOccurrence) { return aOccurrence.Evidence.EventId == 102U; });
		expect(value.State.Occurrences.Occurrences.size() == 5U && value.State.Occurrences.Ambiguous
				   && eventB != value.State.Occurrences.Occurrences.end() && !eventB->LocalEpoch
				   && std::ranges::find(value.State.Occurrences.Occurrences, observedB) != value.State.Occurrences.Occurrences.end()
				   && value.State.SupersededOccurrences.empty(),
			   "delayed B event cannot supersede older observed B merely because A and C surround both records");
		expect(value.State.EpochRows[1U] == epochB && value.State.Snapshot.LatestLive->Id.Ordinal == core::epoch_ordinal{ 3 }
				   && value.State.Snapshot.PreviousDistinctHead->Id == epochB.Id,
			   "unresolved older B preserves its exact local row and C's live latest/previous aliases");
	}

	void newly_discovered_old_events_cannot_name_a_later_ordinary_transition()
	{
		fixture value;
		accept_history(value, 1U, 21U, { }, application::epoch_timeline_availability::failed);
		const auto firstB = *value.State.Snapshot.LatestLive;
		const auto oldA   = force_push(101U, 20U, firstB.ObservedAt.value() - 2'000);
		const auto oldB   = force_push(102U, 21U, firstB.ObservedAt.value() - 1'000);
		accept_history(value, 2U, 20U, { oldA, oldB });
		const auto currentA = value.State.Snapshot.LatestLive->Id;
		expect(value.State.Occurrences.Ambiguous && value.State.Occurrences.Incomplete
				   && value.State.Occurrences.Occurrences.size() == 3U
				   && std::ranges::none_of(value.State.Occurrences.Occurrences, [&](const auto& aOccurrence) {
						  return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event && aOccurrence.LocalEpoch == currentA;
					  }),
			   "freshly discovered historical A predating observed B cannot identify the later ordinary B-to-A transition");
		expect(value.State.Snapshot.LatestLive->Cause.Primary == core::epoch_primary_cause::author_upload,
			   "missing occurrence chronology does not suppress the real descendant live append");
		accept_history(value, 3U, 20U, { oldA, oldB });
		expect(std::ranges::none_of(value.State.Occurrences.Occurrences, [&](const auto& aOccurrence) {
				   return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event && aOccurrence.LocalEpoch == currentA;
			   }),
			   "refresh cannot supply the missing occurrence equivalence");
	}

	void local_clock_skew_cannot_associate_old_provider_event_with_new_transition()
	{
		fixture value;
		// The local clock is two seconds behind the provider throughout:
		// old A happened at provider +1000, B at +2000, then B was read
		// at provider +2500 (local +500). The later ordinary return to A
		// is read at provider +3500 (local +1500).
		constexpr auto origin = 1'789'000'000'000LL;
		accept_history(value, 1U, 21U, { }, application::epoch_timeline_availability::failed, origin + 500);
		accept_history(value, 2U, 20U,
					   { force_push(101U, 20U, origin + 1'000), force_push(102U, 21U, origin + 2'000) },
					   application::epoch_timeline_availability::available, origin + 1'500);
		const auto localA = value.State.Snapshot.LatestLive->Id;
		expect(value.State.Occurrences.Ambiguous && value.State.Occurrences.Incomplete
				   && std::ranges::none_of(value.State.Occurrences.Occurrences, [&](const auto& aOccurrence) {
						  return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event && aOccurrence.LocalEpoch == localA;
					  }),
			   "constant local clock skew cannot turn old provider A into the later ordinary return to A");
	}

	void intervening_provider_update_excludes_old_event_despite_observation_interval()
	{
		fixture        value;
		constexpr auto origin = 1'789'000'000'000LL;
		accept_history(value, 1U, 22U, { }, application::epoch_timeline_availability::available, origin + 500);
		// Synchronized clocks: C observed, event A, event B, ordinary
		// return to A, then A observed. A's event is inside the local
		// observation interval but the later B event refutes that binding.
		accept_history(value, 2U, 20U,
					   { force_push(101U, 20U, origin + 1'000), force_push(102U, 21U, origin + 2'000) },
					   application::epoch_timeline_availability::available, origin + 4'500);
		const auto localA = value.State.Snapshot.LatestLive->Id;
		expect(value.State.Occurrences.Ambiguous && value.State.Occurrences.Incomplete
				   && std::ranges::none_of(value.State.Occurrences.Occurrences, [&](const auto& aOccurrence) {
						  return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event && aOccurrence.LocalEpoch == localA;
					  }),
			   "intervening provider B refutes binding old event A to a later ordinary A even with synchronized clocks");
	}

	void uncertain_event_chronology_keeps_the_live_transition_unresolved()
	{
		for (const auto boundary : { 0, 1, 2 }) {
			fixture value;
			accept_history(value, 1U, 20U, { force_push(101U, 20U) });
			const auto lower       = value.State.Snapshot.LatestLive->ObservedAt.value();
			const auto observation = boundary == 2 ? lower - 100 : lower + 1'000;
			const auto eventTime   = boundary == 0 ? lower : (boundary == 1 ? observation + 1'000 : lower - 200);
			accept_history(value, 2U, 21U, { force_push(101U, 20U), force_push(102U, 21U, eventTime) },
						   application::epoch_timeline_availability::available, observation);
			const auto epoch = value.State.Snapshot.LatestLive->Id;
			expect(value.State.Occurrences.Ambiguous && value.State.Occurrences.Incomplete
					   && std::ranges::none_of(value.State.Occurrences.Occurrences, [&](const auto& aOccurrence) {
							  return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event && aOccurrence.LocalEpoch == epoch;
						  }),
				   "equal lower time, future event time and backwards observation bounds cannot prove event equivalence");
		}
	}

	void unavailable_commit_dates_remain_undated_and_retry_on_refresh()
	{
		fixture value;
		value.State.CommitTimeSeconds.reset();
		accept_history(value, 1U, 20U, { force_push(101U, 20U, 1'900'000'000'000LL) });
		accept_history(value, 2U, 21U, { force_push(101U, 20U, 1'900'000'000'000LL) });
		const auto undated = std::ranges::find_if(value.State.Occurrences.Occurrences,
												  [](const auto& aOccurrence) { return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::observed_head; });
		expect(undated != value.State.Occurrences.Occurrences.end()
				   && undated->DateSource == core::epoch_occurrence_date_source::unavailable && undated->DateUtcMilliseconds == 0,
			   "missing committer time remains explicitly undated");
		const auto firstSeen          = undated->FirstSeen;
		value.State.CommitTimeSeconds = 1'789'000'123;
		accept_history(value, 3U, 21U, { force_push(101U, 20U, 1'900'000'000'000LL) });
		const auto& dated = value.State.Occurrences.Occurrences.front();
		expect(dated.Evidence.Kind == core::epoch_occurrence_kind::observed_head
				   && dated.DateSource == core::epoch_occurrence_date_source::commit_date && dated.FirstSeen == firstSeen,
			   "cache refresh fills committer time without changing first-seen; mixed date order differs from live ordinal order");
	}

	/// \brief [[BPR-1340]]. An update recorded without its date, by an earlier
	/// session or a read that failed, is dated when the review is opened
	/// again at the same head, in a new process whose cache is not yet open.
	/// Before, only a new head opened the cache, so the newest update said
	/// "Date unavailable" for as long as the author did not push.
	void a_reopened_review_dates_an_undated_update_at_an_unchanged_head()
	{
		fixture value;
		value.State.CommitTimeSeconds.reset();
		accept_history(value, 1U, 20U, { });
		accept_history(value, 2U, 21U, { });
		const auto undated = std::ranges::find_if(value.State.Occurrences.Occurrences,
												  [](const auto& aOccurrence) { return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::observed_head; });
		expect(undated != value.State.Occurrences.Occurrences.end()
				   && undated->DateSource == core::epoch_occurrence_date_source::unavailable,
			   "the ordinary update is recorded undated while its committer time cannot be read");
		const auto occurrence = undated->Id;
		const auto ordinals   = value.State.Snapshot.NextOrdinal;
		// A new host, as a new process has: its actor's open cache goes with
		// it, the catalogue stays. The fixture's handoffs name session 91.
		value.Host->shutdown();
		drain(value);
		value.Host = std::make_unique<application::epoch_application_host>(
			core::epoch_state{ .ApplicationSession = core::application_session_id{ 91U } },
			value.Caches, value.Catalogs, value.Entropy, value.Clock, value.Effects, value.Decisions, value.Projections);
		value.State.CommitTimeSeconds = 1'789'000'321;
		const auto requests           = value.State.CommitTimeRequests.size();
		accept_history(value, 3U, 21U, { });
		const auto dated = std::ranges::find(value.State.Occurrences.Occurrences, occurrence, &core::epoch_occurrence::Id);
		expect(value.State.Snapshot.NextOrdinal == ordinals && value.State.CommitTimeRequests.size() > requests,
			   "reopening at the unchanged head appends no epoch and asks the cache for the committer time");
		expect(dated != value.State.Occurrences.Occurrences.end()
				   && dated->DateSource == core::epoch_occurrence_date_source::commit_date
				   && dated->DateUtcMilliseconds == 1'789'000'321'000LL,
			   "the reopened review dates the update from its commit");
	}

	void establish_ready_comparison(
		fixture&                                                aValue,
		std::optional<application::accepted_pull_request_shape> aShape = std::nullopt)
	{
		auto handoff  = make_handoff(aValue.State, 1U, 20U, true);
		handoff.Shape = aShape;
		aValue.Host->accept(std::move(handoff));
		drive_through_preparation(aValue);
		run_effect_completion(aValue);
		drain(aValue);
		expect(
			aValue.Host->state().AcceptedRevision.has_value()
				&& aValue.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 1U }
				&& aValue.Host->state().AcceptedComparison.has_value(),
			"fixture establishes one active accepted comparison");
	}

	/// A host with one job slot cannot run the settled ingestion's newest page
	/// and its comparison together (BPR-0725): the page is admitted first and
	/// the comparison is refused at admission. Re-selecting Whole PR then
	/// resolves it in the freed slot, so a capacity-one test still starts
	/// from one accepted comparison without changing its own limit.
	void establish_ready_comparison_in_one_slot(fixture& aValue)
	{
		aValue.Host->accept(make_handoff(aValue.State, 1U, 20U, true));
		drain(aValue);
		expect(
			aValue.Host->state().Catalog.has_value()
				&& !aValue.Host->state().AcceptedComparison.has_value()
				&& !aValue.Host->state().PendingHistory.has_value()
				&& aValue.Host->outstanding_jobs() == 0U,
			"with one slot the newest page is admitted and the first comparison "
			"is refused");
		aValue.Host->dispatch(core::select_named_comparison_intent{
			.Alias                   = core::named_comparison::whole_pr,
			.ExpectedCatalogRevision = aValue.Host->state().Catalog->Revision,
		});
		drain(aValue);
		expect(
			aValue.Host->state().AcceptedRevision.has_value()
				&& aValue.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 1U }
				&& aValue.Host->state().AcceptedComparison.has_value()
				&& aValue.Host->outstanding_jobs() == 0U,
			"a capacity-one fixture establishes one active accepted comparison");
	}

	[[nodiscard]] auto matching_invalidation()
		-> application::pull_request_authority_invalidation
	{
		const auto owner = authority();
		return {
			.ApplicationSession = core::application_session_id{ 91U },
			.Profile            = owner.Profile,
			.ProfileRevision    = owner.ProfileRevision,
			.Host               = owner.Host,
			.HostAuthority      = owner.HostAuthority,
			.Account            = owner.Account,
			.Viewer             = owner.Viewer,
			.Repository         = owner.Repository,
			.PullRequest        = owner.PullRequest,
			.IdentityGeneration = core::github_identity_generation{ 3U },
		};
	}

	[[nodiscard]] auto blob_request_for(
		const core::epoch_state& aState,
		const core::request_id   aRequest = core::request_id{ 701U })
		-> application::diff_blob_request
	{
		const auto options = core::options_digest(core::diff_options{ });
		const auto blob    = oid<core::git_blob_oid>(0x55U);
		expect(
			aState.Authority.has_value()
				&& aState.AcceptedComparison.has_value() && options.has_value(),
			"ready comparison supplies an exact blob request context");
		return {
			.Context = {
				.Request            = aRequest,
				.ApplicationSession = aState.ApplicationSession,
				.Key                = {
					.Authority   = *aState.Authority,
					.Repository  = aState.Authority->Repository,
					.PullRequest = aState.Authority->PullRequest,
					.LeftAnchor  = aState.AcceptedComparison->Left.Identity,
					.RightAnchor = aState.AcceptedComparison->Right.Identity,
					.LeftBlob    = blob,
					.RightBlob   = blob,
					.File        = opaque<core::file_identity>(0x56U),
					.Options     = *options,
					.Algorithm   = core::supported_diff_algorithm_revision,
				},
				.Generation = core::request_generation{ 1U },
			},
			.Side         = core::diff_side::left,
			.Blob         = blob,
			.MaximumBytes = 1'024U,
		};
	}

	[[nodiscard]] auto workbench_authority_for(const core::epoch_state& aState)
		-> application::workbench_pull_request_authority
	{
		expect(
			aState.Authority.has_value() && aState.AcceptedRevision.has_value(),
			"ready comparison supplies workbench authority facts");
		const auto& authority = *aState.Authority;
		const auto& source    = aState.AcceptedRevision->Source;
		return {
			.ApplicationSession    = aState.ApplicationSession,
			.Profile               = authority.Profile,
			.ProfileRevision       = authority.ProfileRevision,
			.Host                  = authority.Host,
			.HostAuthority         = authority.HostAuthority,
			.Account               = authority.Account,
			.Viewer                = authority.Viewer,
			.Repository            = authority.Repository,
			.PullRequest           = authority.PullRequest,
			.Number                = authority.Number,
			.Snapshot              = source.Snapshot,
			.IdentityGeneration    = source.IdentityGeneration,
			.PullRequestGeneration = source.PullRequestGeneration,
		};
	}

	[[nodiscard]] auto selected_diff_request_for(
		fixture&               aValue,
		const core::request_id aRequest = core::request_id{ 801U })
		-> application::workbench_selected_diff_request
	{
		const auto authority = workbench_authority_for(aValue.Host->state());
		const auto context   = aValue.Host->context(authority);
		const auto file      = core::file_identity_of("file.cpp");
		expect(
			context.has_value() && file.valid(),
			"ready comparison supplies selected-diff context");
		return {
			.Context = {
				.Request    = aRequest,
				.Session    = aValue.Host->state().ApplicationSession,
				.Generation = core::request_generation{ 1U },
			},
			.WorkbenchAuthority = authority,
			.Diff               = *context,
			.File               = {
				.File             = file,
				.PresentationPath = "file.cpp",
				.CurrentPath      = "file.cpp",
				.Change           = application::workbench_file_change_kind::modified,
				.ContentKind      = application::workbench_file_content_kind::text,
			},
		};
	}

	// BPR-0397. The two agent-workspace calls reach their wait only for a value
	// the host can key to a live cache actor, so these mirror the fixture's
	// accepted authority, its application session, and its SHA-1 head. That is
	// what makes the derived cache actor key the one `establish_ready_comparison`
	// installed rather than a second key with no actor behind it.
	[[nodiscard]] auto workspace_parent_directory() -> std::filesystem::path
	{
		// Absolute, because a relative parent is an invalid request. Nothing is
		// created here: the fake cache session answers without a filesystem.
		return std::filesystem::temp_directory_path() / "bpr-0397-workspaces";
	}

	[[nodiscard]] auto workspace_request_for(const core::epoch_state& aState)
		-> application::agent_workspace_materialization_request
	{
		expect(
			aState.Authority.has_value(),
			"ready comparison supplies agent workspace authority");
		const auto& owner = *aState.Authority;
		return {
			.ApplicationSession = aState.ApplicationSession,
			.Authority          = owner,
			.Session            = {
				.Id      = core::agent_session_id{ 5U },
				.Profile = core::agent_profile_id{ 6U },
				.Binding = {
					.Host        = owner.Host,
					.Repository  = owner.Repository,
					.PullRequest = owner.PullRequest,
					.BaseOid     = oid<core::git_commit_oid>(10U),
					.HeadOid     = oid<core::git_commit_oid>(20U),
					.LeftEpoch   = core::epoch_ordinal{ 1 },
					.RightEpoch  = core::epoch_ordinal{ 1 },
				},
			},
			.ParentDirectory = workspace_parent_directory(),
		};
	}

	[[nodiscard]] auto owned_workspace_for(const core::epoch_state& aState)
		-> application::owned_agent_workspace
	{
		const auto request = workspace_request_for(aState);
		return {
			.Workspace = {
				.Policy      = core::agent_workspace_policy::disposable_review_workspace,
				.PinnedHead  = request.Session.Binding.HeadOid,
				.Path        = (request.ParentDirectory / "workspace").string(),
				.Cleanliness = application::checkout_cleanliness::clean,
			},
			.ApplicationSession = request.ApplicationSession,
			.Authority          = request.Authority,
			.Session            = request.Session.Id,
			.ParentDirectory    = request.ParentDirectory,
			.OwnershipMarker    = "bpr-0397",
			.DirectoryIdentity  = { .Opaque = { 2U, 1U, 0U, 1U } },
		};
	}

	[[nodiscard]] auto diagnostic_count(
		const shared_state&           aState,
		const core::epoch_reason_code aReason) -> std::size_t
	{
		auto total = std::size_t{ 0U };
		for (const auto& update : aState.Updates) {
			if (!update.Transition) {
				continue;
			}
			total += static_cast<std::size_t>(std::ranges::count(
				update.Transition->Diagnostics,
				core::epoch_diagnostic{ .Code = aReason }));
		}
		return total;
	}

	void expect_in_order(
		const std::vector<std::string>&      aCalls,
		const std::vector<std::string_view>& aExpected)
	{
		auto cursor = aCalls.cbegin();
		for (const auto item : aExpected) {
			cursor = std::find(cursor, aCalls.cend(), item);
			expect(cursor != aCalls.cend(), item);
			++cursor;
		}
	}

	void invalid_handoff_has_no_effects()
	{
		fixture value;
		auto    invalid    = make_handoff(value.State);
		invalid.Repository = { };
		value.Host->accept(std::move(invalid));
		expect(
			value.Effects.pending() == 0U && value.Decisions.pending() == 0U
				&& value.State.Calls.empty(),
			"invalid handoff has no entropy, catalog, cache, or transport effect");
	}

	void source_collision_retries_before_core_acceptance()
	{
		fixture value;
		value.State.SourceCollisions = { true, false };
		value.Host->accept(make_handoff(value.State));
		expect(!value.Host->state().Authority, "core has not seen worker input");
		value.Effects.run_next();
		expect(
			!value.Host->state().Authority && value.State.ProjectionCalls == 0U,
			"source collision probing finishes before the core terminal");
		value.Decisions.run_next();
		expect(
			value.Host->state().Authority == authority()
				&& value.Host->state().AcceptedRevision.has_value()
				&& value.State.ProjectionCalls != 0U,
			"the second fresh source reaches core only on the decision lane");
		expect(
			std::ranges::count(value.State.Calls, "entropy") == 2
				&& std::ranges::count(
					   value.State.Calls, "catalog.source-probe")
					== 2,
			"one collision consumes one fresh replacement observation");
	}

	void epoch_preparation_diagnostics_preserve_results()
	{
		using stage  = application::diagnostic_epoch_stage;
		using status = application::diagnostic_log_status;
		for (const auto sinkStatus : { status::available, status::unavailable, status::full, status::dropped }) {
			for (unsigned mode = 0U; mode < 7U; ++mode) {
				auto log    = std::make_shared<recording_epoch_log>();
				log->Status = sinkStatus;
				fixture                               value{ { }, log };
				stage                                 expectedStage   = stage::catalog_open;
				application::diagnostic_epoch_failure expectedFailure = application::epoch_catalog_failure::invalid_permissions;
				switch (mode) {
				default: expect(false, "unknown epoch diagnostic fixture case"); break;
				case 0U: value.State.CatalogOpenFailures.emplace_back(application::epoch_catalog_failure::invalid_permissions); break;
				case 1U:
					value.State.CacheOpenFailures.push_back(application::git_cache_failure::invalid_path);
					expectedStage   = stage::cache_open;
					expectedFailure = application::git_cache_failure::invalid_path;
					break;
				case 2U:
					value.State.AcquireFailure = application::git_smart_transport_failure::authentication;
					expectedStage              = stage::smart_acquire;
					expectedFailure            = *value.State.AcquireFailure;
					break;
				case 3U:
					value.State.DiscoveryFailure = application::git_smart_transport_failure::network;
					value.State.DiscoveryStep    = application::git_smart_transport_step::identity_before;
					expectedStage                = stage::protocol_acquire;
					expectedFailure              = *value.State.DiscoveryFailure;
					break;
				case 4U:
					value.State.MalformedDiscovery = true;
					expectedStage                  = stage::protocol_acquire;
					expectedFailure                = application::git_protocol_v2_failure::malformed_packet;
					break;
				case 6U:
					value.State.PackFailure = application::git_pack_stream_failure::input_limit;
					expectedStage           = stage::protocol_acquire;
					expectedFailure         = application::git_protocol_v2_failure::pack_sink;
					break;
				case 5U:
					value.State.PrepareCacheFailures.emplace_back("cache.prepare-revision", application::git_cache_failure::object_mismatch);
					expectedStage   = stage::revision_prepare;
					expectedFailure = application::git_cache_failure::object_mismatch;
					break;
				}
				value.Host->accept(make_handoff(value.State, 1U, 20U, true));
				if (mode == 0U) {
					++log->Generation;
				}
				drain(value);
				if (mode == 0U) {
					expect(!log->Events.empty() && std::ranges::all_of(log->Events, [](const auto& aEvent) { return aEvent.ContextGeneration == 1U; }), "delayed epoch effects retain the diagnostic generation captured before revocation");
				}
				expect(!value.Host->state().AcceptedComparison && std::ranges::count(value.State.Calls, "catalog.commit-authorized") == 0, "failed preparation cannot accept comparison or commit regardless of logging availability");

				const auto expectedReason = mode == 5U ? core::epoch_reason_code::repair_required : core::epoch_reason_code::unavailable;
				expect(std::ranges::any_of(value.State.Updates, [&](const auto& aUpdate) { return aUpdate.Transition && std::ranges::any_of(aUpdate.Transition->Diagnostics, [&](const auto& aDiagnostic) { return aDiagnostic.Code == expectedReason; }); }), "logging status preserves the original core diagnostic");
				if (mode == 3U) {
					expect(std::ranges::any_of(log->Events, [](const auto& aEvent) { return aEvent.EpochStage == stage::protocol_acquire && aEvent.GitStep == application::git_smart_transport_step::identity_before; }), "protocol acquisition preserves the closed transport step without affecting the core failure");
				}
				if (mode == 6U) {
					expect(std::ranges::any_of(log->Events, [](const auto& aEvent) { return aEvent.EpochPackFailure == application::git_pack_stream_failure::input_limit; }), "protocol acquisition retains exact pack failure");
				}
				expect(!log->Events.empty() && std::ranges::all_of(log->Events, [](const auto& aEvent) { return aEvent.WindowSession == 91U; }), "every preparation stage retains the accepted window session, including failures before request allocation");
				expect(std::ranges::any_of(log->Events, [&](const auto& aEvent) { return aEvent.Operation == application::diagnostic_operation::epoch && aEvent.Outcome == application::diagnostic_outcome::failed && aEvent.EpochStage == expectedStage && aEvent.EpochFailure == expectedFailure && aEvent.Summary.text().empty(); }), "exact typed stage failure is recorded without raw details");
			}
		}
		for (const auto sinkStatus : { status::available, status::unavailable, status::full, status::dropped, status::closed }) {
			auto log    = std::make_shared<recording_epoch_log>();
			log->Status = sinkStatus;
			fixture value{ { }, log };
			establish_ready_comparison(value);
			expect(std::ranges::count(value.State.Calls, "catalog.commit-authorized") == 1, "each log status preserves the one durable catalog commit");
			expect(!log->Events.empty() && std::ranges::all_of(log->Events, [](const auto& aEvent) { return aEvent.WindowSession == 91U; }), "successful catalog and revision preparation retain the window session");
			expect(std::ranges::any_of(log->Events, [](const auto& aEvent) { return aEvent.EpochStage == stage::protocol_acquire && aEvent.Outcome == application::diagnostic_outcome::succeeded; }), "successful preparation still yields exact comparison and stage evidence");
			value.Host->shutdown();
			drain(value);
			expect(value.Host->is_quiescent(), "log refusal cannot suppress orderly terminal drainage");
		}
	}

	void git_tool_refusals_preserve_actionable_reasons_and_retry()
	{
		using step    = application::git_smart_transport_step;
		using failure = application::git_smart_transport_failure;
		for (const auto probeStep : { step::git_version, step::git_capability_probe, step::git_qualification }) {
			for (const auto probeFailure : { failure::unavailable, failure::timeout, failure::response_limit }) {
				fixture value;
				value.State.DiscoveryFailure = probeFailure;
				value.State.DiscoveryStep    = probeStep;
				value.Host->accept(make_handoff(value.State, 1U, 20U, true));
				drain(value);
				const auto expected = probeStep == step::git_qualification
					? core::epoch_reason_code::git_tool_unsupported
					: core::epoch_reason_code::git_tool_probe_failed;
				expect(diagnostic_count(value.State, expected) == 1U
						   && diagnostic_count(value.State, core::epoch_reason_code::unavailable) == 0U,
					   "Git check refusals reach presentation with the exact typed reason");
				expect(!value.Host->state().AcceptedComparison
						   && std::ranges::count(value.State.Calls, "catalog.commit-authorized") == 0,
					   "tool refusal admits no comparison or durable catalog commit");
				value.State.DiscoveryFailure.reset();
				value.State.DiscoveryStep = step::none;
				value.Host->accept(make_handoff(value.State, 2U, 21U, true));
				drain(value);
				expect(value.Host->state().AcceptedComparison.has_value(),
					   "a corrected executable can retry through the same authority");
				value.Host->shutdown();
				drain(value);
				expect(value.Host->is_quiescent(), "tool refusal and retry drain on shutdown");
			}
		}
		fixture cancelled;
		cancelled.State.DiscoveryFailure = failure::cancelled;
		cancelled.State.DiscoveryStep    = step::git_capability_probe;
		cancelled.Host->accept(make_handoff(cancelled.State, 1U, 20U, true));
		drain(cancelled);
		expect(diagnostic_count(cancelled.State, core::epoch_reason_code::git_tool_probe_failed) == 0U
				   && diagnostic_count(cancelled.State, core::epoch_reason_code::git_tool_unsupported) == 0U,
			   "cancelling a probe never becomes a tool refusal");
	}

	void gh_tool_refusals_preserve_actionable_reasons_and_retry()
	{
		using step    = application::git_smart_transport_step;
		using failure = application::git_smart_transport_failure;
		for (const auto probeStep : { step::cli_version, step::cli_capability_probe, step::cli_qualification }) {
			for (const auto probeFailure : { failure::unavailable, failure::timeout, failure::response_limit }) {
				fixture value;
				value.State.DiscoveryFailure = probeFailure;
				value.State.DiscoveryStep    = probeStep;
				value.Host->accept(make_handoff(value.State, 1U, 20U, true));
				drain(value);
				const auto expected = probeStep == step::cli_qualification
					? core::epoch_reason_code::gh_tool_unsupported
					: core::epoch_reason_code::gh_tool_probe_failed;
				expect(diagnostic_count(value.State, expected) == 1U
						   && diagnostic_count(value.State, core::epoch_reason_code::unavailable) == 0U
						   && diagnostic_count(value.State, core::epoch_reason_code::git_tool_unsupported) == 0U
						   && diagnostic_count(value.State, core::epoch_reason_code::git_tool_probe_failed) == 0U,
					   "GitHub CLI check refusals reach presentation with their own typed reason, not Git's");
				expect(!value.Host->state().AcceptedComparison
						   && std::ranges::count(value.State.Calls, "catalog.commit-authorized") == 0,
					   "GitHub CLI refusal admits no comparison or durable catalog commit");
				value.State.DiscoveryFailure.reset();
				value.State.DiscoveryStep = step::none;
				value.Host->accept(make_handoff(value.State, 2U, 21U, true));
				drain(value);
				expect(value.Host->state().AcceptedComparison.has_value(),
					   "a corrected GitHub CLI can retry through the same authority");
				value.Host->shutdown();
				drain(value);
				expect(value.Host->is_quiescent(), "GitHub CLI refusal and retry drain on shutdown");
			}
		}
		fixture cancelled;
		cancelled.State.DiscoveryFailure = failure::cancelled;
		cancelled.State.DiscoveryStep    = step::cli_capability_probe;
		cancelled.Host->accept(make_handoff(cancelled.State, 1U, 20U, true));
		drain(cancelled);
		expect(diagnostic_count(cancelled.State, core::epoch_reason_code::gh_tool_probe_failed) == 0U
				   && diagnostic_count(cancelled.State, core::epoch_reason_code::gh_tool_unsupported) == 0U,
			   "cancelling a GitHub CLI probe never becomes a tool refusal");
	}

	void successful_append_preserves_exact_order()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		expect(
			value.Host->state().PendingIngestion.has_value()
				&& value.Host->state().PendingIngestion->Phase
					== core::epoch_ingestion_phase::committing,
			"prepared append waits for exact core commit authorization");
		run_effect_completion(value);
		drain(value);

		expect(
			value.Host->state().Catalog == catalog_identity(1)
				&& !value.Host->state().PendingIngestion,
			"durable append advances the visible catalog exactly once");
		expect_in_order(
			value.State.Calls,
			{
				"catalog.reserve",
				"cache.create-sink",
				"catalog.receiving",
				"transport.acquire",
				"transport.discover",
				"transport.ls-refs",
				"transport.fetch",
				"cache.sink-finish",
				"cache.observe-preimport",
				"catalog.spool-recorded",
				"cache.prepare-revision",
				"catalog.imported",
				"catalog.transition:staging-ready",
				"catalog.commit-authorized",
				"catalog.transition:ponr",
				"cache.promote",
				"catalog.transition:refs-committed",
				"cache.inspect-retained",
				"catalog.finalize",
				"catalog.transition:cleanup",
			});
		expect(
			value.State.ProjectionOutsideDecision == 0U,
			"worker effects never publish or mutate core outside decision lane");
	}

	void projection_failure_is_contained_and_decision_progress_drains()
	{
		fixture value;
		value.State.ProjectionFailuresRemaining = 1U;
		value.Host->accept(make_handoff(value.State));

		run_effect_completion(value);
		expect(
			value.Host->projection_delivery_failed()
				&& value.State.ProjectionFailuresRemaining == 0U
				&& value.Host->state().AcceptedRevision.has_value()
				&& value.Effects.pending() == 1U,
			"a throwing projection is contained after core acceptance and cannot "
			"suppress the next host request");

		drain(value);
		expect(
			value.State.ProjectionCalls > 1U && !value.State.Updates.empty()
				&& value.State.ProjectionOutsideDecision == 0U,
			"decision-lane completions and later projections continue after a "
			"contained projection exception");

		value.Decisions.post([&value] {
			value.Host->shutdown();
		});
		value.Decisions.run_next();
		drain(value);
		expect(
			value.Host->is_closing() && value.Host->is_quiescent()
				&& value.State.ProjectionOutsideDecision == 0U,
			"a contained projection exception cannot prevent serialized shutdown "
			"quiescence");
	}

	void reentrant_projection_ingress_is_deferred_without_recursion()
	{
		fixture value;
		value.State.ProjectionIngress = [&value] {
			value.Host->dispatch(core::cancel_epoch_refresh_intent{ });
		};
		value.Host->accept(make_handoff(value.State));

		run_effect_completion(value);
		expect(
			value.State.ProjectionIngressCalls == 1U
				&& value.State.MaximumProjectionDepth == 1U
				&& value.State.ProjectionDepth == 0U
				&& value.Effects.pending() == 1U,
			"projection feedback queues semantic ingress without recursive "
			"publication or suppressing the accepted host request");

		drain(value);
		expect(
			value.Host->state().AcceptedRevision.has_value()
				&& !value.Host->state().PendingIngestion
				&& value.Host->state().Status == core::epoch_status::cancelled
				&& value.State.ProjectionIngressCalls == 1U
				&& value.State.MaximumProjectionDepth == 1U
				&& value.State.ProjectionOutsideDecision == 0U,
			"deferred projection ingress reaches one serialized terminal without "
			"corrupting accepted authority state");

		value.Decisions.post([&value] {
			value.Host->shutdown();
		});
		value.Decisions.run_next();
		drain(value);
		expect(
			value.Host->is_quiescent()
				&& value.State.MaximumProjectionDepth == 1U
				&& value.State.ProjectionOutsideDecision == 0U,
			"reentrant projection ingress leaves no recursive or deferred work at "
			"shutdown");
	}

	void epoch_host_limits_reject_invalid_capacity()
	{
		const auto rejects = [](const application::epoch_host_limits aLimits) {
			try {
				fixture value{ aLimits };
			} catch (const std::invalid_argument&) {
				return true;
			}
			return false;
		};

		expect(rejects({ .OutstandingJobs = 0U, .DeferredIngress = 2U }),
			   "zero epoch job capacity is rejected at composition");
		expect(rejects({ .OutstandingJobs = 2U, .DeferredIngress = 0U }),
			   "zero deferred-ingress capacity is rejected at composition");
		expect(rejects({ .OutstandingJobs = application::maximum_epoch_outstanding_jobs + 1U,
						 .DeferredIngress = 2U }),
			   "a job capacity above the hard maximum is rejected at "
			   "composition");
		expect(rejects({ .OutstandingJobs = 2U,
						 .DeferredIngress = application::maximum_epoch_deferred_ingress + 1U }),
			   "a deferred-ingress capacity above the hard maximum is rejected "
			   "at composition");
		expect(!rejects({ .OutstandingJobs = application::maximum_epoch_outstanding_jobs,
						  .DeferredIngress = application::maximum_epoch_deferred_ingress }),
			   "both hard maxima remain admissible at composition");

		// BPR-0390. The synchronous blob read used to wait forever; it is now
		// bounded, and a zero bound would abandon every read before the actor
		// could run, which is worse than the wait it replaced.
		expect(rejects({ .OutstandingJobs = 2U,
						 .DeferredIngress = 2U,
						 .BlobWait        = std::chrono::milliseconds::zero() }),
			   "a zero blob-read bound is rejected at composition");
		expect(rejects({ .OutstandingJobs = 2U,
						 .DeferredIngress = 2U,
						 .BlobWait        = application::maximum_epoch_blob_wait
							 + std::chrono::milliseconds{ 1 } }),
			   "a blob-read bound above the hard maximum is rejected at "
			   "composition");
		expect(!rejects({ .OutstandingJobs = 2U,
						  .DeferredIngress = 2U,
						  .BlobWait        = application::maximum_epoch_blob_wait }),
			   "the hard maximum blob-read bound remains admissible");

		// BPR-0397. The two agent-workspace waits are bounded on the same terms
		// and fail the same way at zero, but they are a separate bound because
		// writing or removing a whole tree is not a read of an object the cache
		// already holds.
		expect(rejects({ .OutstandingJobs = 2U,
						 .DeferredIngress = 2U,
						 .WorkspaceWait   = std::chrono::milliseconds::zero() }),
			   "a zero agent-workspace bound is rejected at composition");
		expect(rejects({ .OutstandingJobs = 2U,
						 .DeferredIngress = 2U,
						 .WorkspaceWait   = application::maximum_epoch_workspace_wait
							 + std::chrono::milliseconds{ 1 } }),
			   "an agent-workspace bound above the hard maximum is rejected at "
			   "composition");
		expect(!rejects({ .OutstandingJobs = 2U,
						  .DeferredIngress = 2U,
						  .WorkspaceWait   = application::maximum_epoch_workspace_wait }),
			   "the hard maximum agent-workspace bound remains admissible");
	}

	/// BPR-0397. These three waits had no deadline at all, and the only thing
	/// that ever released one was shutdown — which is how a starved call
	/// outlived the work it was waiting for. Each now returns its own failure
	/// exit when the actor never reaches the work it queued.
	///
	/// The token passed here has no stop state, so the abandonment path cannot
	/// fire and the bound is the only exit left. `queued_actor_work()` is what
	/// separates that exit from the earlier returns that report the same
	/// status: work is enqueued strictly after the key, the actor, and the
	/// authorization have been accepted, and the effect executor is never run
	/// here, so nothing can complete it.
	void every_sibling_actor_wait_releases_at_its_bound()
	{
		constexpr auto bound = std::chrono::milliseconds{ 25 };
		fixture        value{ application::epoch_host_limits{
			.BlobWait      = bound,
			.WorkspaceWait = bound,
		} };
		establish_ready_comparison(value);
		expect(
			value.Effects.pending() == 0U
				&& value.Host->queued_actor_work() == 0U,
			"the ready fixture leaves no actor work outstanding");

		const auto diff = selected_diff_request_for(value);
		expect(
			value.Host->resolve(diff, { }).Status
					== application::workbench_selected_diff_status::failure
				&& value.Host->queued_actor_work() == 1U,
			"a selected-diff resolution whose actor never runs returns at its "
			"bound with its queued work intact");

		const auto request = workspace_request_for(value.Host->state());
		const auto materialized
			= value.Host->materialize_agent_workspace(request, { });
		expect(
			!materialized.has_value()
				&& materialized.error()
					== application::agent_workspace_failure::unavailable
				&& value.Host->queued_actor_work() == 2U,
			"a workspace materialization whose actor never runs returns at its "
			"bound with its queued work intact");

		const auto workspace = owned_workspace_for(value.Host->state());
		const auto cleaned   = value.Host->cleanup_agent_workspace(workspace, { });
		expect(
			!cleaned.has_value()
				&& cleaned.error()
					== application::agent_workspace_failure::cleanup_failed
				&& value.Host->queued_actor_work() == 3U,
			"a workspace cleanup whose actor never runs returns at its bound "
			"with its queued work intact");
		expect(
			value.Effects.pending() == 1U,
			"and one scheduled actor drain still owns all three: a released "
			"wait cancels no job");
	}

	/// BPR-0397. The other exit, and the one that has to stay distinguishable
	/// from the bound: a caller that has moved on is `cancelled`. Nothing else
	/// here can produce that status — the effect executor is never run, so the
	/// queued work cannot report it, and each bound reports its own failure
	/// instead.
	///
	/// The bound is deliberately large. A short one would let a broken stop
	/// path pass by elapsing quickly, and the point of the case is that only
	/// the stop token can explain a prompt return. Ten seconds is long enough
	/// to say that and short enough that a regression fails in ten seconds
	/// rather than hanging.
	void every_sibling_actor_wait_releases_on_caller_cancellation()
	{
		constexpr auto bound = std::chrono::seconds{ 10 };
		fixture        value{ application::epoch_host_limits{
			.BlobWait      = bound,
			.WorkspaceWait = bound,
		} };
		establish_ready_comparison(value);
		auto caller = std::stop_source{ };
		static_cast<void>(caller.request_stop());

		const auto diff = selected_diff_request_for(value);
		expect(
			value.Host->resolve(diff, caller.get_token()).Status
					== application::workbench_selected_diff_status::cancelled
				&& value.Host->queued_actor_work() == 1U,
			"a stopped caller releases the selected-diff wait as cancelled "
			"without cancelling the work it queued");

		const auto request = workspace_request_for(value.Host->state());
		const auto materialized
			= value.Host->materialize_agent_workspace(request, caller.get_token());
		expect(
			!materialized.has_value()
				&& materialized.error()
					== application::agent_workspace_failure::cancelled
				&& value.Host->queued_actor_work() == 2U,
			"a stopped caller releases the materialization wait as cancelled "
			"without cancelling the work it queued");

		const auto workspace = owned_workspace_for(value.Host->state());
		const auto cleaned
			= value.Host->cleanup_agent_workspace(workspace, caller.get_token());
		expect(
			!cleaned.has_value()
				&& cleaned.error()
					== application::agent_workspace_failure::cancelled
				&& value.Host->queued_actor_work() == 3U,
			"a stopped caller releases the cleanup wait as cancelled without "
			"cancelling the removal it queued");
		expect(
			value.Effects.pending() == 1U,
			"and one scheduled actor drain still owns all three");
	}

	void admission_allocation_failure_rolls_back_every_boundary()
	{
		using application::detail::testing::epoch_admission_allocation_stage;
		using application::detail::testing::fail_next_epoch_job_admission;

		for (const auto stage : { epoch_admission_allocation_stage::job_control,
								  epoch_admission_allocation_stage::decision_task,
								  epoch_admission_allocation_stage::job_ledger }) {
			fixture value;
			fail_next_epoch_job_admission(stage);
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));

			expect(value.Host->outstanding_jobs() == 0U
					   && value.Host->queued_actor_work() == 0U
					   && value.Effects.pending() == 0U,
				   "a failed admission allocation leaves no partial job, actor "
				   "entry, or scheduled effect");
			expect(!value.Host->state().Authority
					   && value.State.AuthorityRevocations
						   == std::vector<std::uint64_t>{ 1U },
				   "a failed admission allocation revokes the rejected offer's "
				   "lease and leaves the active authority unchanged");
			expect(diagnostic_count(value.State,
									core::epoch_reason_code::unavailable)
					   == 1U,
				   "a failed admission allocation reports exactly one "
				   "unavailable diagnostic");

			value.Host->shutdown();
			expect(value.Host->outstanding_jobs() == 0U,
				   "the host still reaches quiescence after a failed admission "
				   "allocation");
		}

		fixture value;
		fail_next_epoch_job_admission(
			epoch_admission_allocation_stage::job_control);
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		expect(value.Host->outstanding_jobs() != 0U
				   || value.Effects.pending() != 0U,
			   "one armed allocation failure is consumed exactly once and does "
			   "not affect the next admission");
		value.Host->shutdown();
	}

	void executor_refusal_rolls_back_offer_admission()
	{
		fixture value;
		value.Effects.reject_next();
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));

		expect(
			value.Host->outstanding_jobs() == 0U
				&& value.Host->queued_actor_work() == 0U
				&& value.Effects.pending() == 0U
				&& !value.Host->state().Authority
				&& value.State.AuthorityRevocations
					== std::vector<std::uint64_t>{ 1U }
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::unavailable)
					== 1U,
			"effect-executor refusal retires the slot before one unavailable "
			"terminal and revokes the rejected offer");
	}

	void actor_quota_rejection_preserves_the_admitted_offer()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 1U,
			.DeferredIngress = 4U,
		} };
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		for (std::uint64_t snapshot = 2U; snapshot <= 1'000U; ++snapshot) {
			value.Host->accept(make_repository_handoff(
				value.State, snapshot + 60U, snapshot));
		}
		value.Host->accept(make_repository_handoff(value.State, 1'061U, 1'001U));

		expect(
			value.Host->outstanding_jobs() == 1U
				&& value.Effects.pending() == 1U
				&& value.State.AuthorityRevocations.size() == 1'000U
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::quota_exceeded)
					== 1'000U,
			"job and actor quota refusals do not replace the admitted offer");

		value.Host->invalidate(matching_invalidation());
		drain(value);
		expect(
			!value.Host->state().Authority
				&& value.Host->outstanding_jobs() == 0U
				&& value.State.AuthorityRevocations.size() == 1'001U
				&& std::ranges::find(value.State.AuthorityRevocations, 1U)
					!= value.State.AuthorityRevocations.end(),
			"actor quota rejection preserves the pending identity so matching "
			"invalidation cancels it and stale work cannot publish");

		value.Host->shutdown();
		expect(value.Host->is_quiescent(), "the actor-quota flood shuts down exactly");
	}

	void refresh_flood_bounds_jobs_actor_fifo_and_completions()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 3U,
			.DeferredIngress = 4U,
		} };
		for (std::uint64_t snapshot = 1U; snapshot <= 4U; ++snapshot) {
			value.Host->accept(make_handoff(
				value.State,
				snapshot,
				static_cast<std::uint8_t>(19U + snapshot),
				true));
		}

		expect(
			value.Host->outstanding_jobs() == 3U
				&& value.Host->queued_actor_work() == 3U
				&& value.Effects.pending() == 1U
				&& value.Decisions.pending() == 0U
				&& value.State.AuthorityRevocations
					== std::vector<std::uint64_t>{ 4U }
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::quota_exceeded)
					== 1U,
			"refresh offers retain exactly one bounded ledger entry per queued "
			"actor closure and reject overflow without superseding admitted work");

		value.Effects.run_next();
		expect(
			value.Host->outstanding_jobs() == 3U
				&& value.Host->queued_actor_work() == 0U
				&& value.Effects.pending() == 0U
				&& value.Decisions.pending() == 3U,
			"one actor drain preserves all admitted slots through pending serialized "
			"completions");

		value.Decisions.run_next();
		value.Decisions.run_next();
		value.Decisions.run_next();
		auto publishedSnapshots = std::vector<core::provider_snapshot_id>{ };
		for (const auto& update : value.State.Updates) {
			if (update.Transition && update.Transition->Authority
				&& update.Transition->Authority->Revision) {
				publishedSnapshots.push_back(
					update.Transition->Authority->Revision->Snapshot);
			}
		}
		expect(
			value.Host->state().AcceptedRevision
				&& value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 3U }
				&& publishedSnapshots
					== std::vector<core::provider_snapshot_id>{
						core::provider_snapshot_id{ 3U } }
				&& value.Host->outstanding_jobs() == 1U && value.Effects.pending() == 1U,
			"older stopped terminals cannot publish and terminal retirement admits "
			"the accepted refresh's next request in the same decision turn");

		value.Host->shutdown();
		drain(value);
		expect(
			value.Host->is_quiescent() && value.Host->outstanding_jobs() == 0U
				&& value.Host->queued_actor_work() == 0U,
			"shutdown reaches exact quiescence with accepted actor work pending");
	}

	void request_quota_uses_exact_typed_terminal()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 1U,
			.DeferredIngress = 4U,
		} };
		// BPR-0725: the automatic newest page takes the only slot during setup.
		establish_ready_comparison_in_one_slot(value);
		const auto request = blob_request_for(value.Host->state());
		const auto catalog = *value.Host->state().Catalog;
		value.Host->dispatch(core::request_epoch_history_page_intent{
			.Catalog = catalog,
			.Limit   = 4U,
		});
		const auto rejectedBlob = value.Host->materialize(request, { });
		expect(
			rejectedBlob.Status == application::diff_blob_status::failure
				&& value.Host->outstanding_jobs() == 1U,
			"blob admission fails without misreporting a file-size limit when the "
			"shared host ledger is full");
		drain(value);

		auto observation = std::optional<application::diff_blob_observation>{ };
		auto worker      = std::jthread{ [&] {
			observation = value.Host->materialize(request, { });
		} };
		value.Effects.wait_for_pending();
		expect(
			value.Host->outstanding_jobs() == 1U
				&& value.Host->queued_actor_work() == 1U,
			"the synchronous blob bridge consumes the same global actor ledger");

		value.Host->dispatch(core::request_epoch_history_page_intent{
			.Catalog = catalog,
			.Limit   = 4U,
		});
		expect(
			!value.Host->state().PendingHistory
				&& value.State.Updates.back().Catalog
				&& value.State.Updates.back().Catalog->HistoryPage
				&& value.State.Updates.back().Catalog->HistoryPage->Phase
					== core::epoch_history_page_phase::failed
				&& value.State.Updates.back().Catalog->HistoryPage->Reason
					== core::epoch_reason_code::quota_exceeded
				&& value.Host->outstanding_jobs() == 1U,
			"job-capacity refusal reaches the request-specific quota terminal without "
			"disturbing the admitted actor operation");

		value.Effects.run_next();
		worker.join();
		expect(
			observation
				&& observation->Status == application::diff_blob_status::available
				&& value.Host->outstanding_jobs() == 0U,
			"the admitted blob operation retires its slot before waking its caller");
	}

	void concurrent_executor_refusal_preserves_later_actor_work()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 2U,
			.DeferredIngress = 4U,
		} };
		establish_ready_comparison(value);
		const auto request = blob_request_for(value.Host->state());
		value.Effects.block_and_reject_next();

		auto first       = std::optional<application::diff_blob_observation>{ };
		auto second      = std::optional<application::diff_blob_observation>{ };
		auto firstWorker = std::jthread{ [&] {
			first = value.Host->materialize(request, { });
		} };
		value.Effects.wait_for_blocked_post();
		auto secondWorker = std::jthread{ [&] {
			second = value.Host->materialize(request, { });
		} };

		auto bothAdmitted = false;
		for (std::size_t attempt = 0U; attempt < 100'000U; ++attempt) {
			if (value.Host->outstanding_jobs() == 2U) {
				bothAdmitted = true;
				break;
			}
			std::this_thread::yield();
		}
		expect(
			bothAdmitted,
			"both actor jobs are admitted while the first executor post is blocked");

		value.Effects.release_blocked_post();
		firstWorker.join();
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		secondWorker.join();
		expect(
			first && first->Status == application::diff_blob_status::failure
				&& second
				&& second->Status == application::diff_blob_status::available
				&& value.Host->outstanding_jobs() == 0U
				&& value.Host->queued_actor_work() == 0U,
			"executor refusal removes only its own actor work while a concurrent "
			"later job remains live and completes");

		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "the concurrent refusal path quiesces");
	}

	void shutdown_cancels_an_inflight_blob_call()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto              request = blob_request_for(value.Host->state());
		std::mutex              gate;
		std::condition_variable changed;
		auto                    entered          = false;
		auto                    stopObserved     = false;
		auto                    releaseAfterStop = false;
		value.State.DuringBlobMaterialization    = [&](const std::stop_token aStop) {
			std::unique_lock lock{ gate };
			entered = true;
			changed.notify_one();
			auto notify = std::stop_callback{ aStop, [&changed] {
												 changed.notify_all();
											 } };
			changed.wait(lock, [&] {
				return aStop.stop_requested();
			});
			stopObserved = true;
			changed.notify_one();
			changed.wait(lock, [&] {
				return releaseAfterStop;
			});
		};

		auto observation = std::optional<application::diff_blob_observation>{ };
		auto caller      = std::jthread{ [&] {
			observation = value.Host->materialize(request, { });
		} };
		value.Effects.wait_for_pending();
		auto effect = std::jthread{ [&] {
			value.Effects.run_next();
		} };
		{
			std::unique_lock lock{ gate };
			changed.wait(lock, [&] {
				return entered;
			});
		}

		value.Host->shutdown();
		{
			std::unique_lock lock{ gate };
			changed.wait(lock, [&] {
				return stopObserved;
			});
		}
		expect(
			!value.Host->is_quiescent(),
			"shutdown counts the admitted synchronous blob call until it returns");
		{
			const std::lock_guard lock{ gate };
			releaseAfterStop = true;
		}
		changed.notify_one();
		effect.join();
		caller.join();
		drain(value);
		expect(
			observation
				&& observation->Status == application::diff_blob_status::cancelled
				&& value.Host->is_quiescent(),
			"epoch shutdown reaches the cache call through the combined stop token "
			"and then quiesces");
	}

	void shutdown_stop_callback_can_reenter_blob_ingress()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto              request = blob_request_for(value.Host->state());
		std::mutex              gate;
		std::condition_variable changed;
		auto                    entered          = false;
		auto                    callbackReturned = false;
		auto                    callbackDepth    = std::size_t{ 0U };
		auto                    maximumDepth     = std::size_t{ 0U };
		auto                    reentrant        = std::optional<application::diff_blob_observation>{ };
		value.State.DuringBlobMaterialization    = [&](const std::stop_token aStop) {
			auto callback = std::stop_callback{ aStop, [&] {
												   ++callbackDepth;
												   maximumDepth = std::max(maximumDepth, callbackDepth);
												   value.Host->shutdown();
												   auto rejected = value.Host->materialize(request, { });
												   --callbackDepth;
												   {
													   const std::lock_guard lock{ gate };
													   reentrant        = std::move(rejected);
													   callbackReturned = true;
												   }
												   changed.notify_one();
											   } };
			std::unique_lock lock{ gate };
			entered = true;
			changed.notify_one();
			changed.wait(lock, [&] {
				return callbackReturned;
			});
		};

		auto observation = std::optional<application::diff_blob_observation>{ };
		auto caller      = std::jthread{ [&] {
			observation = value.Host->materialize(request, { });
		} };
		value.Effects.wait_for_pending();
		auto effect = std::jthread{ [&] {
			value.Effects.run_next();
		} };
		{
			std::unique_lock lock{ gate };
			changed.wait(lock, [&] {
				return entered;
			});
		}

		value.Host->shutdown();
		expect(
			callbackReturned && maximumDepth == 1U && reentrant
				&& reentrant->Status == application::diff_blob_status::failure,
			"shutdown marks cancellation before callbacks, so recursive shutdown does "
			"not restop the job and callback reentry observes closed blob ingress");

		effect.join();
		caller.join();
		drain(value);
		expect(
			observation
				&& observation->Status == application::diff_blob_status::cancelled
				&& value.Host->is_quiescent(),
			"the outer blob call and reentrant stop callback both quiesce");
	}

	void terminal_slot_reservation_protects_mandatory_cleanup()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 1U,
			.DeferredIngress = 4U,
		} };
		// BPR-0725: the automatic newest page takes the only slot during setup.
		establish_ready_comparison_in_one_slot(value);
		const auto blobRequest = blob_request_for(value.Host->state());
		value.State.Snapshot.LatestLive.reset();
		value.State.Snapshot.PreviousDistinctHead.reset();
		value.Host->dispatch(core::refresh_epochs_intent{ });
		value.Effects.run_next();
		expect(
			value.Decisions.pending() == 1U,
			"the prepared refresh has one serialized terminal pending");
		value.Host->dispatch(core::cancel_epoch_refresh_intent{ });

		auto reentrantBlob            = std::optional<application::diff_blob_observation>{ };
		value.State.ProjectionIngress = [&] {
			reentrantBlob = value.Host->materialize(blobRequest, { });
		};
		value.Decisions.run_next();
		expect(
			reentrantBlob
				&& reentrantBlob->Status == application::diff_blob_status::failure
				&& value.Host->outstanding_jobs() == 1U
				&& value.Effects.pending() == 1U,
			"a reentrant blob cannot steal the retired terminal slot reserved for "
			"the core-emitted discard operation");

		run_effect_completion(value);
		drain(value);
		expect(
			std::ranges::find(
				value.State.Calls, "catalog.transition:aborted")
					!= value.State.Calls.end()
				&& value.Host->outstanding_jobs() == 0U,
			"mandatory pre-PONR cleanup consumes the reserved slot and retires "
			"without retained preparation work");
	}

	void replacement_reserves_required_context_cleanup()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 2U,
			.DeferredIngress = 4U,
		} };
		establish_ready_comparison(value);
		value.State.Snapshot.LatestLive.reset();
		value.State.Snapshot.PreviousDistinctHead.reset();
		value.Host->dispatch(core::refresh_epochs_intent{ });
		value.Effects.run_next();
		value.Decisions.run_next();
		expect(
			value.Host->outstanding_jobs() == 1U
				&& value.Effects.pending() == 1U,
			"the active authority retains one prepared commit job");

		auto replacement        = make_handoff(value.State, 2U, 21U, true);
		replacement.PullRequest = core::pull_request_id{ 72U };
		replacement.Number      = core::pull_request_number{ 82U };
		value.Host->accept(std::move(replacement));
		expect(
			value.Host->state().Authority == std::optional{ authority() }
				&& value.Host->outstanding_jobs() == 1U
				&& value.State.AuthorityRevocations.back() == 2U
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::quota_exceeded)
					== 1U,
			"replacement is rejected before authority mutation when the ledger "
			"cannot retain both its offer and mandatory old-context cleanup");

		drain(value);
		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "the quota-rejected replacement quiesces");
	}

	void cleanup_executor_refusal_prevents_replacement_offer()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 3U,
			.DeferredIngress = 4U,
		} };
		establish_ready_comparison(value);
		value.State.Snapshot.LatestLive.reset();
		value.State.Snapshot.PreviousDistinctHead.reset();
		value.Host->dispatch(core::refresh_epochs_intent{ });
		value.Effects.run_next();
		expect(
			value.Host->outstanding_jobs() == 1U
				&& value.Decisions.pending() == 1U,
			"the prepared active operation is retained before its decision terminal");

		value.Effects.reject_next();
		auto replacement        = make_handoff(value.State, 2U, 21U, true);
		replacement.PullRequest = core::pull_request_id{ 72U };
		replacement.Number      = core::pull_request_number{ 82U };
		value.Host->accept(std::move(replacement));
		expect(
			value.Host->state().Authority == std::optional{ authority() }
				&& value.Host->outstanding_jobs() == 1U
				&& value.State.AuthorityRevocations.back() == 2U
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::unavailable)
					== 1U,
			"cleanup executor refusal preserves the active authority and prevents "
			"the replacement offer from being queued or opened");

		drain(value);
		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "the cleanup-refused replacement quiesces");
	}

	void replacement_offer_refusal_preserves_cross_repository_authority()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 3U,
			.DeferredIngress = 4U,
		} };
		establish_ready_comparison(value);
		value.State.Snapshot.LatestLive.reset();
		value.State.Snapshot.PreviousDistinctHead.reset();
		value.Host->dispatch(core::refresh_epochs_intent{ });
		value.Effects.run_next();
		expect(
			value.Host->outstanding_jobs() == 1U
				&& value.Decisions.pending() == 1U,
			"the cross-repository refusal fixture retains prepared active work");

		value.Effects.accept_one_then_reject();
		value.Host->accept(make_repository_handoff(value.State, 62U, 2U));
		expect(
			value.Host->state().Authority == std::optional{ authority() }
				&& value.Host->outstanding_jobs() == 2U
				&& value.Effects.pending() == 1U
				&& value.State.AuthorityRevocations.back() == 2U
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::unavailable)
					== 1U,
			"a separately scheduled cleanup may remain mandatory, but refusal of the "
			"replacement repository actor preserves A and emits no invalidation");

		drain(value);
		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "cross-repository offer refusal quiesces");
	}

	void same_authority_offer_keeps_terminal_capacity_for_core_ingestion()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 1U,
			.DeferredIngress = 4U,
		} };
		// BPR-0725: the automatic newest page takes the only slot during setup.
		establish_ready_comparison_in_one_slot(value);
		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		value.Effects.run_next();
		value.Decisions.run_next();
		expect(
			value.Host->state().AcceptedRevision
				&& value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 2U }
				&& value.Host->outstanding_jobs() == 1U
				&& value.Effects.pending() == 1U,
			"an empty prior preparation set needs no cleanup job, leaving the offer "
			"terminal reservation for core-emitted catalog ingestion");

		drain(value);
		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "the capacity-one refresh quiesces");
	}

	void replacement_cancellation_does_not_stop_mandatory_cleanup()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 4U,
			.DeferredIngress = 4U,
		} };
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		expect(
			value.Effects.pending() == 1U,
			"the active same-authority context retains prepared append work");

		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		drain(value);
		expect(
			value.Host->state().AcceptedRevision
				&& value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 2U }
				&& std::ranges::find(
					   value.State.Calls, "catalog.transition:aborted")
					!= value.State.Calls.end()
				&& !value.State.RecoveryAbortStopRequested.empty()
				&& std::ranges::none_of(
					value.State.RecoveryAbortStopRequested,
					[](const auto aStopped) {
						return aStopped;
					}),
			"same-authority cancellation skips the protected cleanup control and "
			"durably aborts the prior prepared append before replacement ingestion");

		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "protected same-authority cleanup quiesces");
	}

	void reentrant_terminal_requests_close_blob_authority_before_return()
	{
		{
			fixture value;
			establish_ready_comparison(value);
			const auto request            = blob_request_for(value.Host->state());
			auto       rejected           = std::optional<application::diff_blob_observation>{ };
			value.State.ProjectionIngress = [&] {
				value.Host->invalidate(matching_invalidation());
				rejected = value.Host->materialize(request, { });
			};
			value.Host->dispatch(core::refresh_epochs_intent{ });
			expect(
				rejected
					&& rejected->Status == application::diff_blob_status::failure
					&& value.State.BlobRequests.empty(),
				"reentrant matching invalidation revokes blob authority before the "
				"invalidation call returns");
			drain(value);
			value.Host->shutdown();
			drain(value);
			expect(value.Host->is_quiescent(), "reentrant invalidation quiesces");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			const auto request            = blob_request_for(value.Host->state());
			auto       rejected           = std::optional<application::diff_blob_observation>{ };
			value.State.ProjectionIngress = [&] {
				value.Host->shutdown();
				rejected = value.Host->materialize(request, { });
			};
			value.Host->dispatch(core::refresh_epochs_intent{ });
			expect(
				value.Host->is_closing() && rejected
					&& rejected->Status == application::diff_blob_status::failure
					&& value.State.BlobRequests.empty(),
				"reentrant shutdown closes blob ingress before the shutdown call "
				"returns");
			drain(value);
			expect(value.Host->is_quiescent(), "reentrant shutdown quiesces");
		}
	}

	void reentrant_ingress_flood_preserves_terminal_priority()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 8U,
			.DeferredIngress = 3U,
		} };
		auto    maximumDeferred       = std::size_t{ 0U };
		value.State.ProjectionIngress = [&] {
			const auto observe = [&] {
				maximumDeferred = std::max(
					maximumDeferred, value.Host->deferred_ordinary_ingress());
			};
			value.Host->accept(make_handoff(value.State, 2U, 21U, true));
			observe();
			value.Host->dispatch(core::cancel_epoch_refresh_intent{ });
			observe();
			value.Host->accept(make_handoff(value.State, 3U, 22U, true));
			observe();
			value.Host->accept(make_handoff(value.State, 4U, 23U, true));
			observe();
			value.Host->invalidate(matching_invalidation());
			observe();
			value.Host->shutdown();
			observe();
		};

		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		value.Effects.run_next();
		value.Decisions.run_next();
		expect(
			value.Host->is_closing() && maximumDeferred == 3U
				&& value.State.MaximumProjectionDepth == 1U
				&& value.State.ProjectionDepth == 0U
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::quota_exceeded)
					== 1U,
			"reentrant ordinary ingress is capacity bounded with one deferred quota "
			"diagnostic and no recursive projection");

		auto revoked = value.State.AuthorityRevocations;
		std::ranges::sort(revoked);
		expect(
			revoked == std::vector<std::uint64_t>{ 1U, 2U, 3U, 4U },
			"overflow, matching invalidation, and terminal-priority shutdown revoke "
			"every rejected or retired smart authority exactly once");

		drain(value);
		expect(
			value.Host->is_quiescent()
				&& value.Host->deferred_ordinary_ingress() == 0U
				&& value.State.MaximumProjectionDepth == 1U,
			"terminal-priority shutdown survives the flood and drains exactly");
	}

	void reentrant_ordinary_ingress_retains_fifo_order()
	{
		fixture value;
		value.State.ProjectionIngress = [&] {
			value.Host->accept(make_handoff(value.State, 2U, 21U, true));
			value.Host->accept(make_handoff(value.State, 3U, 22U, true));
		};
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drain(value);

		expect(
			value.Host->state().AcceptedRevision
				&& value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 3U }
				&& value.State.MaximumProjectionDepth == 1U,
			"admitted ordinary reentrant offers retain FIFO order without recursive "
			"projection");
	}

	void reentrant_invalidation_precedes_ordinary_ingress()
	{
		fixture value{ application::epoch_host_limits{
			.OutstandingJobs = 8U,
			.DeferredIngress = 2U,
		} };
		auto    maximumDeferred       = std::size_t{ 0U };
		value.State.ProjectionIngress = [&] {
			const auto observe = [&] {
				maximumDeferred = std::max(
					maximumDeferred, value.Host->deferred_ordinary_ingress());
			};
			value.Host->accept(make_handoff(value.State, 2U, 21U, true));
			observe();
			value.Host->dispatch(core::cancel_epoch_refresh_intent{ });
			observe();
			value.Host->accept(make_handoff(value.State, 3U, 22U, true));
			observe();
			value.Host->invalidate(matching_invalidation());
			observe();
		};
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drain(value);

		auto revoked = value.State.AuthorityRevocations;
		std::ranges::sort(revoked);
		expect(
			!value.Host->state().Authority
				&& maximumDeferred == 2U
				&& revoked == std::vector<std::uint64_t>{ 1U, 2U, 3U }
				&& diagnostic_count(
					   value.State, core::epoch_reason_code::quota_exceeded)
					== 1U
				&& value.State.MaximumProjectionDepth == 1U,
			"matching invalidation survives full ordinary ingress and overflow, then "
			"cancels active and queued replacement authority before dispatch");
		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "the invalidation-overflow path quiesces");
	}

	void failed_same_authority_offer_preserves_active_comparison()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto before            = value.Host->state();
		const auto projectionsBefore = value.State.Updates.size();
		value.State.CatalogOpenFailures.emplace_back(application::epoch_catalog_failure::unavailable);

		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		run_effect_completion(value);

		expect(
			value.Host->state() == before
				&& value.Host->state().AcceptedComparison.has_value(),
			"failed same-authority offer preserves active revision and pair");
		expect(
			value.State.Updates.size() == projectionsBefore + 1U
				&& value.State.Updates.back().Transition.has_value()
				&& !value.State.Updates.back().Transition->Diagnostics.empty()
				&& value.State.Updates.back().Transition->Diagnostics.back().Code
					== core::epoch_reason_code::unavailable,
			"current failed replacement reports one bounded diagnostic");
		expect(value.State.AuthorityRevocations == std::vector<std::uint64_t>{ 2U },
			   "failed replacement revokes only its offered lease and preserves the "
			   "active lease");
	}

	void successful_same_authority_offer_swaps_on_exact_terminal()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto acceptedPair = value.Host->state().AcceptedComparison;

		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		value.Effects.run_next();
		expect(
			value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 1U }
				&& value.Host->state().AcceptedComparison == acceptedPair
				&& value.State.AuthorityRevocations.empty(),
			"worker result alone cannot swap or revoke the active context");

		value.Decisions.run_next();
		expect(
			value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 2U }
				&& value.Host->state().AcceptedComparison == acceptedPair
				&& value.State.AuthorityRevocations
					== std::vector<std::uint64_t>{ 1U },
			"exact offer token swaps on decision lane and then revokes prior lease");
	}

	void historical_origin_reaches_authority_projection()
	{
		fixture value;
		auto    historical = make_handoff(value.State, 7U);
		historical.Origin  = core::pull_request_revision_origin::explicit_historical_import;
		value.Host->accept(std::move(historical));
		run_effect_completion(value);

		const auto projected = std::ranges::any_of(
			value.State.Updates,
			[](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Transition && aUpdate.Transition->Authority
					&& aUpdate.Transition->Authority->Revision
					&& aUpdate.Transition->Authority->Revision->Origin
					== core::pull_request_revision_origin::
						explicit_historical_import;
			});
		expect(
			projected,
			"historical origin survives source minting and authority projection");
	}

	void superseded_offer_failure_emits_no_diagnostic()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto projectionsBefore = value.State.Updates.size();
		value.State.CatalogOpenFailures.emplace_back(application::epoch_catalog_failure::unavailable);

		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		value.Host->accept(make_handoff(value.State, 3U, 22U, true));
		value.Effects.run_next();
		value.Decisions.run_next();

		expect(
			value.State.Updates.size() == projectionsBefore
				&& value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 1U }
				&& value.Host->state().AcceptedComparison.has_value(),
			"superseded failed offer cannot diagnose or disturb active state");
	}

	void matching_invalidation_cancels_pending_offer_and_active_context()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto opensBefore = std::ranges::count(value.State.Calls, "catalog.open");

		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		expect(value.Effects.pending() != 0U, "same-identity offer is pending");
		value.Host->invalidate(matching_invalidation());
		expect(
			!value.Host->state().Authority
				&& !value.Host->state().AcceptedRevision
				&& !value.Host->state().AcceptedComparison
				&& value.State.AuthorityRevocations
					== std::vector<std::uint64_t>{ 1U },
			"matching invalidation immediately clears and revokes active context");

		drain(value);
		expect(
			!value.Host->state().Authority
				&& std::ranges::count(value.State.Calls, "catalog.open")
					== opensBefore,
			"cancelled matching offer never opens or resurrects its context");
	}

	void bounded_history_pages_project_incrementally()
	{
		fixture value;
		establish_ready_comparison(value);
		// BPR-0725. Settled ingestion already asked for its own newest page,
		// which this unscripted fake refused; count only the requests below.
		const auto requestsBefore = value.State.HistoryRequests.size();
		const auto catalog        = *value.Host->state().Catalog;
		const auto revision       = *value.Host->state().AcceptedRevision;
		const auto continuation   = history_cursor_for(catalog, core::epoch_ordinal{ 2 });
		value.State.HistoryPages.emplace_back(application::epoch_history_page{
			.Authority = authority().Scope,
			.Catalog   = catalog,
			.Epochs    = {
				summary_for(revision, catalog, core::epoch_ordinal{ 3 }),
				summary_for(revision, catalog, core::epoch_ordinal{ 2 }),
			},
			.Continuation = continuation,
		});

		const auto updatesBefore = value.State.Updates.size();
		value.Host->dispatch(core::request_epoch_history_page_intent{
			.Catalog = catalog,
			.Limit   = 2U,
		});
		expect(
			value.Effects.pending() == 1U
				&& value.State.Updates.size() == updatesBefore + 1U
				&& value.State.Updates.back().Catalog
				&& value.State.Updates.back().Catalog->HistoryPage
				&& value.State.Updates.back().Catalog->HistoryPage->Phase
					== core::epoch_history_page_phase::loading
				&& !value.State.Updates.back().Catalog->Replace,
			"one history intent projects loading and schedules one bounded "
			"catalog effect");

		value.Host->dispatch(core::request_epoch_history_page_intent{
			.Catalog = catalog,
			.Limit   = 2U,
		});
		expect(
			value.Effects.pending() == 1U,
			"a duplicate history intent cannot schedule a second worker");

		run_effect_completion(value);
		expect(
			value.State.HistoryRequests.size() == requestsBefore + 1U
				&& value.State.HistoryRequests.back()
					== application::epoch_history_page_request{
						.Catalog = catalog,
						.Limit   = 2U,
					}
				&& value.State.Updates.back().Catalog && !value.State.Updates.back().Catalog->Replace && value.State.Updates.back().Catalog->Changes.empty() && value.State.Updates.back().Catalog->HistoryPage && value.State.Updates.back().Catalog->HistoryPage->Phase == core::epoch_history_page_phase::loaded && value.State.Updates.back().Catalog->HistoryPage->Navigation == core::epoch_history_navigation::newest && value.State.Updates.back().Catalog->HistoryPage->Rows.size() == 2U && value.State.Updates.back().Catalog->HistoryPage->Rows[0U].Id.Ordinal == core::epoch_ordinal{ 3 } && value.State.Updates.back().Catalog->HistoryPage->Continuation == continuation && value.Host->state().HistoryContinuation == continuation,
			"a loaded page emits sanitized stable rows without a model replacement");

		value.State.HistoryPages.emplace_back(application::epoch_history_page{
			.Authority = authority().Scope,
			.Catalog   = catalog,
			.Epochs    = {
				summary_for(revision, catalog, core::epoch_ordinal{ 1 }),
			},
		});
		value.Host->dispatch(core::request_epoch_history_page_intent{
			.Catalog    = catalog,
			.Navigation = core::epoch_history_navigation::older,
			.Cursor     = continuation,
			.Limit      = 2U,
		});
		run_effect_completion(value);
		expect(
			value.State.HistoryRequests.size() == requestsBefore + 2U
				&& value.State.HistoryRequests.back().Cursor == continuation
				&& value.State.Updates.back().Catalog
				&& value.State.Updates.back().Catalog->HistoryPage
				&& value.State.Updates.back().Catalog->HistoryPage->Navigation
					== core::epoch_history_navigation::older
				&& value.State.Updates.back().Catalog->HistoryPage->Rows.size()
					== 1U
				&& value.State.Updates.back()
						.Catalog->HistoryPage->Rows.front()
						.Id.Ordinal
					== core::epoch_ordinal{ 1 }
				&& !value.State.Updates.back().Catalog->HistoryPage->Continuation
				&& !value.Host->state().HistoryContinuation,
			"the exact continuation replaces the window with a short terminal page "
			"without "
			"retaining complete history in core");

		value.State.HistoryPages.emplace_back(application::epoch_history_page{
			.Authority = authority().Scope,
			.Catalog   = catalog,
			.Epochs    = {
				summary_for(revision, catalog, core::epoch_ordinal{ 3 }),
				summary_for(revision, catalog, core::epoch_ordinal{ 2 }),
			},
			.Continuation = continuation,
		});
		value.Host->dispatch(core::request_epoch_history_page_intent{
			.Catalog = catalog,
			.Limit   = 2U,
		});
		run_effect_completion(value);
		expect(value.State.HistoryRequests.size() == requestsBefore + 3U
				   && !value.State.HistoryRequests.back().Cursor
				   && value.State.Updates.back().Catalog
				   && value.State.Updates.back().Catalog->HistoryPage
				   && value.State.Updates.back().Catalog->HistoryPage->Navigation
					   == core::epoch_history_navigation::newest
				   && value.State.Updates.back().Catalog->HistoryPage->Rows.front().Id.Ordinal
					   == core::epoch_ordinal{ 3 }
				   && value.Host->state().HistoryNavigation
					   == core::epoch_history_navigation::newest,
			   "a core-authorized newest request returns from the terminal older "
			   "page without a cursor or host-invented navigation policy");

		const auto updatesBeforeCatalogAdvance = value.State.Updates.size();
		value.Host->dispatch(core::prune_review_epochs_intent{
			.Catalog = catalog,
			.Epochs  = { value.State.Snapshot.LatestLive->Id },
		});
		run_effect_completion(value);
		const auto invalidated = std::ranges::find_if(
			value.State.Updates.begin()
				+ static_cast<std::ptrdiff_t>(updatesBeforeCatalogAdvance),
			value.State.Updates.end(),
			[&catalog](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Catalog && aUpdate.Catalog->HistoryPage
					&& aUpdate.Catalog->HistoryPage->Phase
					== core::epoch_history_page_phase::invalidated
					&& aUpdate.Catalog->HistoryPage->Catalog == catalog
					&& aUpdate.Catalog->Changes.empty();
			});
		expect(!value.Host->state().HistoryCatalog
				   && !value.Host->state().HistoryNavigation
				   && !value.Host->state().HistoryContinuation,
			   "a live catalog revision clears every core history-window binding");
		expect(invalidated != value.State.Updates.end(),
			   "a live catalog revision invalidates the accepted page and never "
			   "invents a new-epoch row in the stale resident window");
	}

	void empty_one_and_exact_bound_history_pages_reach_public_projection()
	{
		{
			fixture value;
			establish_ready_comparison(value);
			const auto catalog = *value.Host->state().Catalog;
			value.State.HistoryPages.emplace_back(application::epoch_history_page{
				.Authority = authority().Scope,
				.Catalog   = catalog,
			});
			value.Host->dispatch(core::request_epoch_history_page_intent{
				.Catalog = catalog,
				.Limit   = 2U,
			});
			run_effect_completion(value);
			expect(
				value.State.Updates.back().Catalog
					&& value.State.Updates.back().Catalog->HistoryPage
					&& value.State.Updates.back().Catalog->HistoryPage->Rows.empty()
					&& !value.State.Updates.back().Catalog->HistoryPage->Continuation,
				"an empty bounded catalog page reaches the incremental projection");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			const auto catalog  = *value.Host->state().Catalog;
			const auto revision = *value.Host->state().AcceptedRevision;
			value.State.HistoryPages.emplace_back(application::epoch_history_page{
				.Authority = authority().Scope,
				.Catalog   = catalog,
				.Epochs    = {
					summary_for(revision, catalog, core::epoch_ordinal{ 1 }),
				},
			});
			value.Host->dispatch(core::request_epoch_history_page_intent{
				.Catalog = catalog,
				.Limit   = 2U,
			});
			run_effect_completion(value);
			expect(
				value.State.Updates.back().Catalog
					&& value.State.Updates.back().Catalog->HistoryPage
					&& value.State.Updates.back().Catalog->HistoryPage->Rows.size()
						== 1U
					&& !value.State.Updates.back().Catalog->HistoryPage->Continuation,
				"a one-row page reaches the projection as a terminal page");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			const auto catalog  = *value.Host->state().Catalog;
			const auto revision = *value.Host->state().AcceptedRevision;
			value.State.HistoryPages.emplace_back(application::epoch_history_page{
				.Authority = authority().Scope,
				.Catalog   = catalog,
				.Epochs    = {
					summary_for(revision, catalog, core::epoch_ordinal{ 2 }),
					summary_for(revision, catalog, core::epoch_ordinal{ 1 }),
				},
			});
			value.Host->dispatch(core::request_epoch_history_page_intent{
				.Catalog = catalog,
				.Limit   = 2U,
			});
			run_effect_completion(value);
			expect(
				value.State.Updates.back().Catalog
					&& value.State.Updates.back().Catalog->HistoryPage
					&& value.State.Updates.back().Catalog->HistoryPage->Rows.size()
						== 2U
					&& !value.State.Updates.back()
							.Catalog->HistoryPage->Continuation,
				"an exact-bound page remains terminal when the catalog advertises "
				"no continuation");
		}
	}

	void history_page_failures_and_catalog_revision_changes_are_typed()
	{
		{
			fixture value;
			establish_ready_comparison(value);
			const auto catalog = *value.Host->state().Catalog;
			value.State.HistoryPages.emplace_back(std::unexpected{
				application::epoch_catalog_failure::unavailable });
			value.Host->dispatch(core::request_epoch_history_page_intent{
				.Catalog = catalog,
				.Limit   = 4U,
			});
			run_effect_completion(value);
			expect(
				value.State.Updates.back().Catalog
					&& value.State.Updates.back().Catalog->HistoryPage
					&& value.State.Updates.back().Catalog->HistoryPage->Phase
						== core::epoch_history_page_phase::failed
					&& value.State.Updates.back().Catalog->HistoryPage->Reason
						== core::epoch_reason_code::unavailable
					&& !value.Host->state().PendingHistory,
				"a catalog paging failure reaches one typed retryable projection");

			value.State.HistoryPages.emplace_back(std::unexpected{
				application::epoch_catalog_failure::cancelled });
			value.Host->dispatch(core::request_epoch_history_page_intent{
				.Catalog = catalog,
				.Limit   = 4U,
			});
			run_effect_completion(value);
			expect(
				value.State.Updates.back().Catalog
					&& value.State.Updates.back().Catalog->HistoryPage
					&& value.State.Updates.back().Catalog->HistoryPage->Phase
						== core::epoch_history_page_phase::cancelled
					&& value.State.Updates.back().Catalog->HistoryPage->Reason
						== core::epoch_reason_code::cancelled
					&& !value.Host->state().PendingHistory,
				"a catalog paging cancellation reaches one typed retryable projection");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			// BPR-0725: setup's automatic newest page already read history once.
			const auto historyReadsBefore = std::ranges::count(value.State.Calls, "catalog.history");
			const auto requestedCatalog   = *value.Host->state().Catalog;
			value.Host->dispatch(core::request_epoch_history_page_intent{
				.Catalog = requestedCatalog,
				.Limit   = 4U,
			});
			const auto changedCatalog = core::epoch_catalog_identity{
				.Incarnation = requestedCatalog.Incarnation,
				.Revision    = core::epoch_catalog_revision{
					requestedCatalog.Revision.value() + 1 },
			};
			value.State.Binding.Catalog          = changedCatalog;
			value.State.Snapshot.Binding.Catalog = changedCatalog;
			run_effect_completion(value);
			expect(
				std::ranges::count(value.State.Calls, "catalog.history") == historyReadsBefore
					&& !value.Host->state().Catalog
					&& value.Host->state().PendingIngestion
					&& value.Host->state().PendingIngestion->Phase
						== core::epoch_ingestion_phase::loading_catalog
					&& value.State.Updates.back().Transition
					&& std::ranges::any_of(
						value.State.Updates.back().Transition->Diagnostics,
						[](const core::epoch_diagnostic& aDiagnostic) {
							return aDiagnostic.Code
								== core::epoch_reason_code::catalog_conflict;
						}),
				"a changed live catalog binding rejects the page before SQLite "
				"access and starts an exact reload");
		}
	}

	void reordered_history_completion_cannot_cross_source_identity()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto catalog = *value.Host->state().Catalog;
		value.State.HistoryPages.emplace_back(application::epoch_history_page{
			.Authority = authority().Scope,
			.Catalog   = catalog,
		});
		value.Host->dispatch(core::request_epoch_history_page_intent{
			.Catalog = catalog,
			.Limit   = 4U,
		});
		value.Effects.run_next();
		expect(
			value.Decisions.pending() == 1U,
			"history worker completion waits on the serialized decision lane");

		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		value.Effects.run_next();
		expect(value.Decisions.pending() == 2U,
			   "a replacement source and old history terminal can be "
			   "deterministically reordered");
		value.Decisions.run(1U);
		expect(
			value.Host->state().AcceptedRevision
				&& value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 2U }
				&& !value.Host->state().PendingHistory,
			"the replacement source retires old paging before its terminal runs");
		const auto updatesBeforeStale = value.State.Updates.size();
		value.Decisions.run_next();
		expect(
			value.State.Updates.size() == updatesBeforeStale + 1U
				&& !value.State.Updates.back().Catalog
				&& value.State.Updates.back().Transition
				&& value.State.Updates.back().Transition->Diagnostics
					== std::vector{
						core::epoch_diagnostic{
							.Code = core::epoch_reason_code::stale_completion,
						},
					}
				&& value.Host->state().AcceptedRevision->Source.Snapshot == core::provider_snapshot_id{ 2U },
			"the old loaded page becomes one stale diagnostic and cannot publish "
			"rows into the new source");
	}

	void historical_source_does_not_accept_live_authority()
	{
		fixture     value;
		const auto  handoff             = make_handoff(value.State);
		const auto& identity            = value.State.Binding.Authority;
		value.State.Snapshot.LatestLive = summary_for({ .BaseBranch = handoff.BaseBranch, .Base = handoff.Base, .Head = handoff.Head }, value.State.Binding.Catalog);
		const auto original             = value.State.Snapshot;
		auto       request              = application::cached_review_source_request{
			.Catalog = { .ApplicationSession = core::application_session_id{ 91 }, .Authority = { .Profile = identity.Profile, .ProfileRevision = identity.ProfileRevision, .Host = identity.Host, .HostAuthority = identity.HostAuthority, .Account = identity.Account, .Viewer = identity.Viewer, .Repository = identity.Repository, .PullRequest = identity.PullRequest, .Number = identity.Number, .ObjectFormat = core::git_hash_algorithm::sha1 }, .ExistingOnly = true },
			.Base    = handoff.Base,
			.Head    = handoff.Head,
			.File    = { .Path = "new.cpp", .PreviousPath = "old.cpp", .Change = core::cached_file_change::renamed }
		};
		std::optional<application::cached_review_source_projection> observed;
		const auto                                                  read = [&](std::stop_token aStop = std::stop_token{ }) { observed.reset(); expect(value.Host->read_cached_source(request,aStop,[&](auto aResult) { observed=std::move(aResult); }), "historical source admitted as bounded effect"); drain(value); expect(observed.has_value(), "historical source completes"); };
		read();
		expect(observed->Status == application::cached_source_status::available && observed->BaseText == "blob\n" && observed->HeadText == "blob\n", "exact retained pair materializes bounded source");
		expect(value.State.TreeEntryRequests[0].Commit == request.Base && value.State.TreeEntryRequests[0].Path == "old.cpp" && value.State.TreeEntryRequests[1].Commit == request.Head && value.State.TreeEntryRequests[1].Path == "new.cpp", "rename reads exact endpoint paths");
		expect(value.State.CacheMutationCalls == 0 && value.State.TransportCalls == 0 && value.State.Snapshot == original && !value.Host->state().Authority && !value.Host->state().AcceptedRevision, "historical reading never ingests or changes epoch authority");
		const auto reads = value.State.TreeEntryRequests.size();
		for (const auto base : { false, true }) {
			if (base) {
				request.Base = oid<core::git_commit_oid>(99);
			} else {
				request.Head = oid<core::git_commit_oid>(99);
			}
			read();
			expect(observed->Status == application::cached_source_status::unavailable && value.State.TreeEntryRequests.size() == reads, "foreign base or head cannot borrow retained source");
			request.Base = handoff.Base;
			request.Head = handoff.Head;
		}
		value.State.Snapshot.Binding.Authority.Account = core::github_account_id{ 999 };
		read();
		expect(observed->Status == application::cached_source_status::unavailable && value.State.TreeEntryRequests.size() == reads, "foreign catalogue reader refused before Git");
		value.State.Snapshot                              = original;
		value.State.Snapshot.LatestLive->HeadAvailability = core::anchor_availability::unavailable;
		read();
		expect(observed->Status == application::cached_source_status::unavailable, "missing retained endpoint unavailable");
		value.State.Snapshot          = original;
		value.State.CachedBlobMissing = true;
		read();
		expect(observed->Status == application::cached_source_status::unavailable && !observed->BaseText && !observed->HeadText, "missing physical blob returns no partial text");
		value.State.CachedBlobMissing  = false;
		value.State.CachedBlobOverride = std::vector<std::uint8_t>{ };
		value.State.TreeEntryResults   = { std::nullopt, oid<core::git_blob_oid>(33) };
		read();
		expect(!observed->BaseText && observed->HeadText && observed->HeadText->empty(), "absent path differs from an observed empty blob");
		value.State.CachedBlobOverride = std::vector<std::uint8_t>{ 'x', 0, 'y' };
		read();
		expect(observed->Status == application::cached_source_status::binary && !observed->BaseText, "binary source is not relabeled text");
		value.State.CachedBlobOverride = std::vector<std::uint8_t>(application::maximum_cached_source_bytes + 1, 'x');
		read();
		expect(observed->Status == application::cached_source_status::too_large, "misbehaving oversized blob port is bounded again");
		value.State.CachedBlobOverride.reset();
		std::stop_source stop;
		value.State.DuringTreeEntryLookup = [&](std::stop_token) { stop.request_stop(); };
		read(stop.get_token());
		expect(observed->Status == application::cached_source_status::unavailable && !observed->BaseText && value.Host->outstanding_jobs() == 0, "cancelled source stops materialization and drains");
	}

	void unchanged_and_association_only_skip_git()
	{
		{
			fixture value;
			auto    accepted = make_handoff(value.State);
			auto    revision = core::accepted_pull_request_revision{
				.Source = {
					.Observation           = opaque<core::epoch_source_observation_id>(0x61U),
					.Snapshot              = accepted.Snapshot,
					.IdentityGeneration    = accepted.IdentityGeneration,
					.PullRequestGeneration = accepted.PullRequestGeneration,
				},
				.Origin     = accepted.Origin,
				.BaseBranch = accepted.BaseBranch,
				.Base       = accepted.Base,
				.Head       = accepted.Head,
			};
			value.State.Snapshot.LatestLive  = summary_for(revision, value.State.Binding.Catalog);
			value.State.Snapshot.NextOrdinal = core::epoch_ordinal{ 2 };
			value.Host->accept(std::move(accepted));
			drain(value);
			expect(
				value.State.CacheMutationCalls == 0U
					&& value.State.TransportCalls == 0U
					&& std::ranges::find(
						   value.State.Calls, "catalog.reserve")
						== value.State.Calls.end(),
				"unchanged observation does not reserve, fetch, or mutate cache");
		}

		{
			fixture    value;
			const auto review = core::current_viewer_review_candidate{
				.Review = core::github_review_id{ 90U },
				.Actor  = authority().Viewer,
				.Order  = core::github_review_order{ 1U },
				.State  = core::github_review_state::approved,
				.Commit = oid<core::git_commit_oid>(20U),
			};
			auto accepted = make_handoff(value.State, 1U, 20U, false, review);
			auto revision = core::accepted_pull_request_revision{
				.Source = {
					.Observation           = opaque<core::epoch_source_observation_id>(0x62U),
					.Snapshot              = accepted.Snapshot,
					.IdentityGeneration    = accepted.IdentityGeneration,
					.PullRequestGeneration = accepted.PullRequestGeneration,
				},
				.Origin              = accepted.Origin,
				.BaseBranch          = accepted.BaseBranch,
				.Base                = accepted.Base,
				.Head                = accepted.Head,
				.CurrentViewerReview = review,
			};
			value.State.Snapshot.LatestLive  = summary_for(revision, value.State.Binding.Catalog);
			value.State.Snapshot.NextOrdinal = core::epoch_ordinal{ 2 };
			value.Host->accept(std::move(accepted));
			drain(value);
			expect(
				value.Host->state().Catalog == catalog_identity(1)
					&& value.State.CacheMutationCalls == 0U
					&& value.State.TransportCalls == 0U
					&& std::ranges::find(
						   value.State.Calls, "catalog.associate-review")
						!= value.State.Calls.end(),
				"association-only update commits catalog identity without Git");
		}
	}

	void applied_association_error_reports_mandatory_success()
	{
		fixture    value;
		const auto review = core::current_viewer_review_candidate{
			.Review = core::github_review_id{ 90U },
			.Actor  = authority().Viewer,
			.Order  = core::github_review_order{ 1U },
			.State  = core::github_review_state::approved,
			.Commit = oid<core::git_commit_oid>(20U),
		};
		auto accepted = make_handoff(value.State, 1U, 20U, false, review);
		auto revision = core::accepted_pull_request_revision{
			.Source = {
				.Observation           = opaque<core::epoch_source_observation_id>(0x63U),
				.Snapshot              = accepted.Snapshot,
				.IdentityGeneration    = accepted.IdentityGeneration,
				.PullRequestGeneration = accepted.PullRequestGeneration,
			},
			.Origin              = accepted.Origin,
			.BaseBranch          = accepted.BaseBranch,
			.Base                = accepted.Base,
			.Head                = accepted.Head,
			.CurrentViewerReview = review,
		};
		value.State.Snapshot.LatestLive         = summary_for(revision, value.State.Binding.Catalog);
		value.State.Snapshot.NextOrdinal        = core::epoch_ordinal{ 2 };
		value.State.AssociationAppliesThenFails = true;

		value.Host->accept(std::move(accepted));
		drain(value);

		expect(
			value.Host->state().Catalog == catalog_identity(1)
				&& value.Host->state().Status == core::epoch_status::ready
				&& std::ranges::count(
					   value.State.Calls, "catalog.associate-review")
					== 1
				&& value.State.CacheMutationCalls == 0U
				&& value.State.TransportCalls == 0U,
			"an association committed before an adapter error is observed as "
			"mandatory success without Git work");
	}

	/// BPR-0725. A catalogue that already holds the head epoch prepares no
	/// mutation, so no epoch is appended and nothing carries a row; the owner
	/// saw "No updates loaded." beside "pair ready" on every cache hit. The
	/// line is filled only from a loaded page, so one must be published
	/// before, or with, the status that says the pair is ready.
	void a_cached_head_publishes_the_newest_history_page_before_ready()
	{
		fixture value;
		auto    accepted = make_handoff(value.State);
		auto    revision = core::accepted_pull_request_revision{
			.Source = {
				.Observation           = opaque<core::epoch_source_observation_id>(0x64U),
				.Snapshot              = accepted.Snapshot,
				.IdentityGeneration    = accepted.IdentityGeneration,
				.PullRequestGeneration = accepted.PullRequestGeneration,
			},
			.Origin     = accepted.Origin,
			.BaseBranch = accepted.BaseBranch,
			.Base       = accepted.Base,
			.Head       = accepted.Head,
		};
		const auto catalog               = value.State.Binding.Catalog;
		const auto held                  = summary_for(revision, catalog);
		value.State.Snapshot.LatestLive  = held;
		value.State.Snapshot.NextOrdinal = core::epoch_ordinal{ 2 };
		value.State.HistoryPages.emplace_back(application::epoch_history_page{
			.Authority = authority().Scope,
			.Catalog   = catalog,
			.Epochs    = { held },
		});

		value.Host->accept(std::move(accepted));
		drain(value);

		const auto& updates = value.State.Updates;
		const auto  ready   = std::ranges::find_if(
			updates,
			[](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Transition && aUpdate.Transition->Status
					&& aUpdate.Transition->Status->Status
					== core::epoch_status::ready;
			});
		const auto page = std::ranges::find_if(
			updates,
			[](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Catalog && aUpdate.Catalog->HistoryPage
					&& aUpdate.Catalog->HistoryPage->Phase
					== core::epoch_history_page_phase::loaded
					&& !aUpdate.Catalog->HistoryPage->Rows.empty();
			});
		expect(
			std::ranges::find(value.State.Calls, "catalog.reserve")
					== value.State.Calls.end()
				&& value.Host->state().Catalog == catalog
				&& ready != updates.end(),
			"a catalogue holding the head prepares no mutation and still "
			"reaches a ready comparison");
		expect(
			page != updates.end() && page <= ready
				&& page->Catalog->HistoryPage->Catalog == catalog
				&& page->Catalog->HistoryPage->Navigation
					== core::epoch_history_navigation::newest
				&& page->Catalog->HistoryPage->Rows.front().Id == held.Id,
			"a no-mutation prepare publishes the newest loaded history page, "
			"carrying the held epoch, before or with the ready status");
	}

	void stale_load_terminal_is_inert_under_reordering()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U));
		run_effect_completion(value);
		expect(value.Effects.pending() == 1U, "A catalog load is pending");

		value.Host->accept(make_handoff(value.State, 2U));
		expect(
			value.Effects.pending() == 1U,
			"the cache actor retains one FIFO worker for A load then B open");
		value.Effects.run_next();
		expect(
			value.Decisions.pending() == 2U,
			"the actor emits A cancellation before the queued B offer terminal");
		value.Decisions.run(1U);
		value.Decisions.run_next();
		value.Effects.run_next();
		while (value.Decisions.pending() != 0U) {
			value.Decisions.run_next();
		}
		expect(
			value.Host->state().AcceptedRevision->Source.Snapshot
					== core::provider_snapshot_id{ 2U }
				&& value.Host->state().PendingIngestion.has_value()
				&& value.Host->state().PendingIngestion->Phase
					== core::epoch_ingestion_phase::preparing,
			"a reordered A load cancellation leaves only B preparation active");
	}

	void stale_prepare_and_comparison_terminals_are_inert()
	{
		{
			fixture value;
			value.Host->accept(make_handoff(value.State, 1U));
			run_effect_completion(value);
			run_effect_completion(value);
			expect(value.Effects.pending() == 1U, "A preparation is pending");
			value.Host->accept(make_handoff(value.State, 2U));
			expect(
				value.Effects.pending() == 1U,
				"B offer queues behind A preparation on the cache actor");
			value.Effects.run_next();
			expect(
				value.Decisions.pending() == 2U,
				"A preparation cancellation precedes B offer completion");
			value.Decisions.run(1U);
			value.Decisions.run_next();
			expect(
				value.Host->state().AcceptedRevision->Source.Snapshot
						== core::provider_snapshot_id{ 2U }
					&& value.Host->state().PendingIngestion.has_value()
					&& value.Host->state().PendingIngestion->Phase
						== core::epoch_ingestion_phase::preparing,
				"a reordered A cancellation cannot clear B preparation");
		}

		{
			fixture value;
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drive_through_preparation(value);
			run_effect_completion(value);
			expect(value.Effects.pending() == 1U, "A comparison is pending");
			value.Host->accept(make_handoff(value.State, 2U, 21U));
			expect(
				value.Effects.pending() == 1U,
				"B offer queues behind A comparison on the cache actor");
			value.Effects.run_next();
			// BPR-0725: A's settled ingestion also queued its newest page
			// ahead of the comparison, so B's offer is the third terminal.
			expect(
				value.Decisions.pending() == 3U,
				"A newest-page and comparison terminals precede B offer completion");
			value.Decisions.run(2U);
			value.Decisions.run_next();
			value.Decisions.run_next();
			expect(
				value.Host->state().AcceptedRevision->Source.Snapshot
						== core::provider_snapshot_id{ 2U }
					&& !value.Host->state().AcceptedComparison
					&& !value.Host->state().HistoryCatalog,
				"reordered A page and comparison terminals cannot publish into B");
		}
	}

	void same_repository_actor_switches_exact_pr_cache_sessions_in_fifo_order()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		expect(
			value.Effects.pending() == 1U
				&& value.State.CacheOpenRequests.size() == 1U,
			"A retains one prepared commit and one exact cache session");

		auto secondAuthority        = authority();
		secondAuthority.PullRequest = core::pull_request_id{ 72U };
		secondAuthority.Number      = core::pull_request_number{ 82U };
		secondAuthority.Scope       = opaque<core::pull_request_authority_scope_id>(0x44U);
		const auto secondCatalog    = core::epoch_catalog_identity{
			.Incarnation = opaque<core::epoch_catalog_incarnation_id>(0x45U),
			.Revision    = core::epoch_catalog_revision{ 0 },
		};
		value.State.CatalogOpenSnapshots.push_back(
			application::epoch_catalog_snapshot{
				.Binding = {
					.Authority    = secondAuthority,
					.Catalog      = secondCatalog,
					.ObjectFormat = core::git_hash_algorithm::sha1,
				},
				.NextOrdinal = core::epoch_ordinal{ 1U },
			});
		auto second        = make_handoff(value.State, 2U, 21U);
		second.PullRequest = secondAuthority.PullRequest;
		second.Number      = secondAuthority.Number;

		value.Host->accept(std::move(second));
		drain(value);

		expect_in_order(
			value.State.Calls,
			{
				"catalog.transition:aborted",
				"cache.session-close:71",
				"cache.session-open:72",
			});
		expect(
			value.State.CacheOpenRequests.size() == 2U
				&& value.State.CacheOpenRequests[0U].Authority.PullRequest
					== core::pull_request_id{ 71U }
				&& value.State.CacheOpenRequests[1U].Authority.PullRequest
					== core::pull_request_id{ 72U }
				&& value.State.CacheOpenRequests[0U].Authority.Scope
					== authority().Scope
				&& value.State.CacheOpenRequests[1U].Authority.Scope
					== secondAuthority.Scope
				&& value.State.MaximumActiveCacheSessions == 1U
				&& value.State.ActiveCacheSessions == 1U
				&& !value.State.RecoveryAbortStopRequested.empty()
				&& std::ranges::none_of(
					value.State.RecoveryAbortStopRequested,
					[](const auto aStopped) {
						return aStopped;
					})
				&& value.Host->state().Authority == std::optional{ secondAuthority },
			"one repository actor cleans A under A, closes it, then opens and "
			"uses the exact B PR session without overlap");
	}

	void diff_blob_materialization_reuses_the_epoch_cache_actor()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto request = blob_request_for(value.Host->state());
		const auto blob    = request.Blob;
		expect(core::valid(request.Context), "blob bridge request context is valid");
		const auto materialize = [&value](
									 const application::diff_blob_request& aRequest) {
			auto observation = std::optional<application::diff_blob_observation>{ };
			auto worker      = std::jthread{ [&] {
				observation = value.Host->materialize(aRequest, { });
			} };
			value.Effects.wait_for_pending();
			value.Effects.run_next();
			worker.join();
			expect(observation.has_value(), "blob bridge returns one terminal");
			return std::move(*observation);
		};
		const auto observation = materialize(request);

		expect(
			observation.Status == application::diff_blob_status::available
				&& observation.Bytes
					== std::vector<std::uint8_t>{ 'b', 'l', 'o', 'b', '\n' }
				&& value.State.BlobRequests
					== std::vector{ application::git_cache_blob_request{
						.Blob = blob, .MaximumBytes = 1'024U } }
				&& value.State.MaximumActiveCacheSessions == 1U
				&& value.State.ActiveCacheSessions == 1U,
			"diff blob reads serialize through the retained epoch cache session");
		auto stale                              = request;
		stale.Context.Request                   = core::request_id{ 702U };
		stale.Context.Key.Authority.PullRequest = core::pull_request_id{ 72U };
		stale.Context.Key.Authority.Number      = core::pull_request_number{ 82U };
		stale.Context.Key.PullRequest           = core::pull_request_id{ 72U };
		expect(core::valid(stale.Context), "stale same-repository context is valid");
		const auto rejected = value.Host->materialize(stale, { });
		expect(
			rejected.Status == application::diff_blob_status::failure
				&& value.State.BlobRequests.size() == 1U
				&& value.State.CacheOpenRequests.size() == 1U
				&& value.State.MaximumActiveCacheSessions == 1U
				&& value.Host->outstanding_jobs() == 0U
				&& value.Host->queued_actor_work() == 0U,
			"stale same-repository authority is rejected before actor admission and "
			"cannot switch the retained cache session");

		value.Host->shutdown();
		const auto closed = value.Host->materialize(request, { });
		expect(
			closed.Status == application::diff_blob_status::failure
				&& value.State.BlobRequests.size() == 1U,
			"shutdown closes blob ingress without enqueuing cache work");
	}

	[[nodiscard]] auto resolve_selected_diff(
		fixture& aValue, const application::workbench_selected_diff_request& aRequest,
		std::stop_token aStop)
		-> application::workbench_selected_diff_observation;

	void selected_blob_reads_only_the_exact_missing_object()
	{
		const auto read = [](fixture& aValue, const application::diff_blob_request& aRequest) {
			auto result = std::optional<application::diff_blob_observation>{ };
			auto worker = std::jthread{ [&] { result = aValue.Host->materialize(aRequest, { }); } };
			aValue.Effects.wait_for_pending();
			aValue.Effects.run_next();
			worker.join();
			expect(result.has_value(), "selected blob returns one bounded terminal");
			return *result;
		};
		fixture large;
		large.State.SupportsSparse = true;
		large.State.SparseObjects  = true;
		establish_ready_comparison(large, application::accepted_pull_request_shape{ 40U, 4U, 4U });
		large.State.CachedBlobMissing = true;
		const auto fabricated         = blob_request_for(large.Host->state());
		expect(read(large, fabricated).Status == application::diff_blob_status::failure
				   && large.State.ExactBlobRequests.empty(),
			   "a same-PR blob without selected tree proof cannot be read remotely");
		const auto selected = resolve_selected_diff(large, selected_diff_request_for(large), { });
		expect(selected.Status == application::workbench_selected_diff_status::available
				   && selected.LeftBlob && selected.RightBlob,
			   "selected file resolves exact left and right tree objects");
		auto request                    = fabricated;
		request.Context.Key.LeftAnchor  = selected.Request.Diff.Comparison.Left.Identity;
		request.Context.Key.RightAnchor = selected.Request.Diff.Comparison.Right.Identity;
		request.Context.Key.LeftBlob    = selected.LeftBlob;
		request.Context.Key.RightBlob   = selected.RightBlob;
		request.Context.Key.File        = selected.Request.File.File;
		request.Context.Key.Options     = selected.Request.Diff.OptionsDigest;
		request.Context.Key.Algorithm   = selected.Request.Diff.Algorithm;
		request.Blob                    = *selected.LeftBlob;
		const auto loaded               = read(large, request);
		expect(loaded.Status == application::diff_blob_status::available
				   && loaded.Bytes == std::vector<std::uint8_t>{ 'e', 'x', 'a', 'c', 't', ' ', 's', 'e', 'l', 'e', 'c', 't', 'e', 'd', ' ', 'b', 'l', 'o', 'b' }
				   && large.State.ExactBlobRequests.size() == 1U
				   && large.State.ExactBlobRequests.front().ExpectedBlob == request.Blob
				   && large.State.ExactBlobRequests.front().Context.Authority == *large.Host->state().Authority
				   && large.State.ExactBlobRequests.front().MaximumBytes == request.MaximumBytes,
			   "large selection asks for only its exact tree-authorized missing blob");
		auto substituted                 = request;
		substituted.Blob                 = oid<core::git_blob_oid>(0x55U);
		substituted.Context.Key.LeftBlob = substituted.Blob;
		expect(read(large, substituted).Status == application::diff_blob_status::failure
				   && large.State.ExactBlobRequests.size() == 1U,
			   "a substituted same-file object cannot borrow the selected grant");
		auto sameSelection = selected_diff_request_for(large, core::request_id{ 802U });
		// [[BPR-1327]]. The verified bytes are retained for the authority, so
		// the second read of the same blob (the classifier's, a revisit) does
		// not cross the network again.
		expect(resolve_selected_diff(large, sameSelection, { }).Status
					   == application::workbench_selected_diff_status::available
				   && read(large, request).Status == application::diff_blob_status::available
				   && large.State.ExactBlobRequests.size() == 1U,
			   "same-file re-resolution preserves the exact grant across request IDs, and a read blob is not read twice");

		auto rightRequest            = request;
		rightRequest.Side            = core::diff_side::right;
		rightRequest.Blob            = *selected.RightBlob;
		large.State.ExactBlobFailure = application::git_smart_transport_failure::unavailable;
		const auto unavailable       = read(large, rightRequest);
		expect(unavailable.Status == application::diff_blob_status::failure
				   && unavailable.Bytes.empty() && large.State.ExactBlobRequests.size() == 2U,
			   "unproven remote object remains explicitly unavailable");
		auto changedSelection                  = selected_diff_request_for(large, core::request_id{ 803U });
		changedSelection.File.File             = core::file_identity_of("other.cpp");
		changedSelection.File.CurrentPath      = "other.cpp";
		changedSelection.File.PresentationPath = "other.cpp";
		expect(resolve_selected_diff(large, changedSelection, { }).Status
					   == application::workbench_selected_diff_status::available
				   && read(large, request).Status == application::diff_blob_status::failure
				   && large.State.ExactBlobRequests.size() == 2U,
			   "a different selected file revokes the former remote-object grant, retained bytes included");
		large.Host->invalidate(matching_invalidation());
		expect(large.Host->materialize(request, { }).Status == application::diff_blob_status::failure
				   && large.State.ExactBlobRequests.size() == 2U,
			   "revoked selection cannot issue another exact object read");

		fixture small;
		establish_ready_comparison(small, application::accepted_pull_request_shape{ 100U, 1U, 1U });
		small.State.CachedBlobMissing = true;
		const auto missing            = read(small, blob_request_for(small.Host->state()));
		expect(missing.Status == application::diff_blob_status::missing
				   && small.State.ExactBlobRequests.empty(),
			   "small complete reviews never turn a missing blob into a lazy remote read");
	}

	// [[BPR-1327]] ---------------------------------------------------------

	[[nodiscard]] auto sha1_blob(const std::string_view aText) -> core::git_blob_oid
	{
		auto       hash   = core::sha1_stream{ };
		const auto header = "blob " + std::to_string(aText.size());
		hash.update(std::span{ reinterpret_cast<const std::uint8_t*>(header.data()), header.size() });
		const auto terminator = std::array<std::uint8_t, 1U>{ 0U };
		hash.update(terminator);
		hash.update(std::span{ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() });
		const auto digest = hash.finish().value();
		auto       bytes  = std::array<std::uint8_t, 32U>{ };
		std::ranges::copy(digest, bytes.begin());
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto file_request(fixture& aValue, const std::string& aPath, const core::request_id aRequest = core::request_id{ 901U })
		-> application::workbench_selected_diff_request
	{
		auto request                  = selected_diff_request_for(aValue, aRequest);
		request.File.File             = core::file_identity_of(aPath);
		request.File.CurrentPath      = aPath;
		request.File.PresentationPath = aPath;
		return request;
	}

	[[nodiscard]] auto prepare_window(fixture& aValue, const std::vector<application::workbench_selected_diff_request>& aRequests,
									  const std::stop_token aStop = { }) -> application::epoch_prepared_comparisons
	{
		auto done   = std::atomic_bool{ false };
		auto result = application::epoch_prepared_comparisons{ };
		{
			auto worker = std::jthread{ [&] {
				result = aValue.Host->prepare(aRequests, aStop);
				done.store(true);
			} };
			while (!done.load()) {
				if (aValue.Effects.pending() != 0U) {
					aValue.Effects.run_next();
				} else {
					std::this_thread::yield();
				}
			}
		}
		return result;
	}

	/// Resolves an open whether or not it needs the actor: a prepared file
	/// is answered without one.
	[[nodiscard]] auto open_file(fixture& aValue, const application::workbench_selected_diff_request& aRequest)
		-> application::workbench_selected_diff_observation
	{
		auto done   = std::atomic_bool{ false };
		auto result = application::workbench_selected_diff_observation{ };
		{
			auto worker = std::jthread{ [&] {
				result = aValue.Host->resolve(aRequest, { });
				done.store(true);
			} };
			while (!done.load()) {
				if (aValue.Effects.pending() != 0U) {
					aValue.Effects.run_next();
				} else {
					std::this_thread::yield();
				}
			}
		}
		return result;
	}

	[[nodiscard]] auto read_blob(fixture& aValue, const application::diff_blob_request& aRequest)
		-> application::diff_blob_observation
	{
		auto result = std::optional<application::diff_blob_observation>{ };
		auto worker = std::jthread{ [&] { result = aValue.Host->materialize(aRequest, { }); } };
		aValue.Effects.wait_for_pending();
		aValue.Effects.run_next();
		worker.join();
		expect(result.has_value(), "blob read returns one terminal");
		return *result;
	}

	[[nodiscard]] auto prepared_blob_request(const core::epoch_state&                                aState,
											 const application::workbench_selected_diff_observation& aObservation,
											 const core::diff_side                                   aSide) -> application::diff_blob_request
	{
		const auto key = application::selected_diff_key(aObservation);
		expect(key.has_value(), "an available resolution has a diff key");
		return {
			.Context = {
				.Request            = core::request_id{ 1U },
				.ApplicationSession = aState.ApplicationSession,
				.Key                = *key,
				.Generation         = core::request_generation{ 1U },
			},
			.Side         = aSide,
			.Blob         = aSide == core::diff_side::left ? *aObservation.LeftBlob : *aObservation.RightBlob,
			.MaximumBytes = 1'024U,
		};
	}

	/// A sparse review of three modified files whose blobs are real SHA-1
	/// object IDs of known text, so a batch's answers can be verified.
	void establish_sparse_three_files(fixture& aValue)
	{
		aValue.State.SupportsSparse = true;
		aValue.State.SparseObjects  = true;
		establish_ready_comparison(aValue, application::accepted_pull_request_shape{ 3U, 1U, 1U });
		aValue.State.CachedBlobMissing = true;
		for (const auto* name : { "a", "b", "c" }) {
			for (const auto* side : { "left ", "right " }) {
				const auto text = std::string{ side } + name + "\n";
				aValue.State.TreeEntryResults.emplace_back(sha1_blob(text));
				aValue.State.BatchContents.emplace_back(sha1_blob(text), text);
			}
		}
	}

	/// The public source service must work with sparse trees and no desktop selection.
	void tour_source_materializes_sparse_tree_without_selected_file()
	{
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			fixture value;
			value.State.SupportsSparse = true;
			value.State.SparseObjects  = true;
			establish_ready_comparison(value, application::accepted_pull_request_shape{ 3U, 1U, 1U });
			value.State.CachedBlobMissing             = true;
			auto                              request = file_request(value, "pinned.cpp");
			const core::tour_code_destination source{ .Commit = side == core::diff_side::left ? request.Diff.Comparison.Left.Commit : request.Diff.Comparison.Right.Commit, .Path = "pinned.cpp", .Side = side };
			const auto                        context = application::review_tour_source_context(*value.Host, request.WorkbenchAuthority, source);
			expect(context.has_value(), "public tour service mints pinned source authority");
			request.Diff    = *context;
			const auto blob = sha1_blob("exact selected blob");
			value.State.TreeEntryResults.emplace_back(blob);
			value.State.TreeEntryResults.emplace_back(blob);
			auto done   = std::atomic_bool{ false };
			auto result = std::expected<application::review_tour_source_material, application::review_tour_source_failure>{ std::unexpected(application::review_tour_source_failure::unavailable) };
			{
				auto worker = std::jthread{ [&] {
					result = application::materialize_review_tour_source(*value.Host, *value.Host, request, source, { });
					done.store(true);
				} };
				while (!done.load()) {
					if (value.Effects.pending() != 0U) {
						value.Effects.run_next();
					} else {
						std::this_thread::yield();
					}
				}
			}
			expect(result && result->Text == "exact selected blob" && result->Source.Commit == source.Commit
					   && result->Source.Path == source.Path && result->Source.Blob == blob && result->Source.First.Side == side
					   && value.State.ExactBlobRequests.size() == 1U,
				   "public tour source reads exact pinned text from sparse trees without desktop blob grants");
		}
	}

	/// [[BPR-1377]]. Pinned reads carry sealed endpoint proof for this call only.
	void pinned_source_sparse_read_does_not_grant_ordinary_blob_reads()
	{
		fixture value;
		value.State.SupportsSparse = true;
		value.State.SparseObjects  = true;
		establish_ready_comparison(value, application::accepted_pull_request_shape{ 3U, 1U, 1U });
		value.State.CachedBlobMissing = true;
		auto       request            = file_request(value, "pinned.cpp");
		const auto authority          = request.WorkbenchAuthority;
		const auto commit             = request.Diff.Comparison.Right.Commit;
		const auto context            = value.Host->realignment_context(authority,
																		{ authority.Host, authority.Repository, authority.PullRequest, commit, commit,
																		  request.Diff.Comparison.Left.Identity.Epoch, request.Diff.Comparison.Right.Identity.Epoch });
		expect(context.has_value(), "pinned source has an authorized identical-commit context");
		request.Diff    = *context;
		const auto blob = sha1_blob("exact selected blob");
		value.State.TreeEntryResults.emplace_back(blob);
		value.State.TreeEntryResults.emplace_back(blob);
		const auto resolved = open_file(value, request);
		expect(resolved.Status == application::workbench_selected_diff_status::available,
			   "pinned source resolves its real tree endpoints without selecting a file");
		auto material = prepared_blob_request(value.Host->state(), resolved, core::diff_side::right);
		expect(read_blob(value, material).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.empty(),
			   "ordinary blob reads acquire no pinned-source authority");
		const auto read = [&](const application::workbench_selected_diff_observation& aSource,
							  const application::diff_blob_request& aRequest, std::stop_token aStop = { }) {
			auto done   = std::atomic_bool{ false };
			auto result = application::diff_blob_observation{ };
			{
				auto worker = std::jthread{ [&] {
					result = value.Host->materialize_pinned_source_blob(aSource, aRequest, aStop);
					done.store(true);
				} };
				while (!done.load()) {
					if (value.Effects.pending() != 0U) {
						value.Effects.run_next();
					} else {
						std::this_thread::yield();
					}
				}
			}
			return result;
		};
		auto forged      = resolved;
		forged.RightPath = "other.cpp";
		expect(read(forged, material).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.empty(),
			   "a forged path cannot reuse sealed source proof");
		auto foreign             = material;
		foreign.Context.Key.File = core::file_identity_of("other.cpp");
		expect(read(resolved, foreign).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.empty(),
			   "another file cannot borrow source proof");
		auto overLimit         = material;
		overLimit.MaximumBytes = 256U * 1024U + 1U;
		expect(read(resolved, overLimit).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.empty(),
			   "source reads retain their 256 KiB bound");
		std::stop_source stopped;
		stopped.request_stop();
		expect(read(resolved, material, stopped.get_token()).Status == application::diff_blob_status::cancelled
				   && value.State.ExactBlobRequests.empty(),
			   "stopped source reads spend no network call");
		const auto result = read(resolved, material);
		expect(result.Status == application::diff_blob_status::available
				   && result.Bytes == std::vector<std::uint8_t>{ 'e', 'x', 'a', 'c', 't', ' ', 's', 'e', 'l', 'e', 'c', 't', 'e', 'd', ' ', 'b', 'l', 'o', 'b' }
				   && value.State.ExactBlobRequests.size() == 1U
				   && value.State.ExactBlobRequests.front().ExpectedBlob == blob,
			   "sealed pinned source reads exactly its missing blob and verifies its bytes");
		foreign                  = material;
		foreign.Context.Key.File = core::file_identity_of("other.cpp");
		expect(read_blob(value, foreign).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.size() == 1U,
			   "source material leaves other ordinary blob authority unchanged");
		expect(read_blob(value, material).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.size() == 1U,
			   "even the same ordinary blob request gains no source grant");
		const auto moved = value.Host->realignment_context(authority,
														   { authority.Host, authority.Repository, authority.PullRequest,
															 request.Diff.Comparison.Left.Commit, oid<core::git_commit_oid>(0x32U),
															 request.Diff.Comparison.Left.Identity.Epoch, request.Diff.Comparison.Right.Identity.Epoch });
		expect(moved.has_value() && read(resolved, material).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.size() == 1U,
			   "a replaced pinned context refuses earlier sealed proof, including retained bytes");
		value.Host->invalidate(matching_invalidation());
		expect(read(resolved, material).Status == application::diff_blob_status::failure
				   && value.State.ExactBlobRequests.size() == 1U,
			   "invalidated source proof cannot start a new remote read");
	}

	void prepared_files_read_their_missing_blobs_in_one_batch()
	{
		fixture value;
		establish_sparse_three_files(value);
		const auto window   = std::vector{ file_request(value, "a.cpp"), file_request(value, "b.cpp"), file_request(value, "c.cpp") };
		const auto prepared = prepare_window(value, window);
		expect(prepared.Observations.size() == 3U && prepared.RemoteBatches == 1U && prepared.RemoteBlobs == 6U,
			   "three prepared files resolve and their six missing blobs arrive in one batch");
		expect(value.State.ExactBlobBatches.size() == 1U && value.State.ExactBlobBatches.front().ExpectedBlobs.size() == 6U
				   && value.State.ExactBlobBatches.front().MaximumBytes == application::maximum_git_exact_blob_batch_bytes
				   && value.State.ExactBlobRequests.empty(),
			   "one batched read and no single read");
		const auto lookups = value.State.TreeEntryRequests.size();
		expect(lookups == 6U, "each prepared file looked up its two endpoints once");

		// Opening a prepared file: resolution from its prepared grant, blobs
		// from the retained verified bytes; no Git, no network.
		const auto opened = open_file(value, file_request(value, "b.cpp", core::request_id{ 902U }));
		expect(opened.Status == application::workbench_selected_diff_status::available
				   && opened.LeftBlob == prepared.Observations[1].LeftBlob && opened.RightBlob == prepared.Observations[1].RightBlob
				   && value.State.TreeEntryRequests.size() == lookups,
			   "opening a prepared file reuses its resolution without a tree lookup");
		const auto left = read_blob(value, prepared_blob_request(value.Host->state(), opened, core::diff_side::left));
		expect(left.Status == application::diff_blob_status::available && left.Bytes == std::vector<std::uint8_t>{ 'l', 'e', 'f', 't', ' ', 'b', '\n' }
				   && value.State.ExactBlobRequests.empty(),
			   "the opened file's blob comes from the retained batch, not the network");
		const auto other = read_blob(value, prepared_blob_request(value.Host->state(), prepared.Observations[2], core::diff_side::right));
		expect(other.Status == application::diff_blob_status::available && value.State.ExactBlobRequests.empty(),
			   "a prepared file not yet opened may be materialized for preparation");

		auto bounded         = prepared_blob_request(value.Host->state(), prepared.Observations[2], core::diff_side::left);
		bounded.MaximumBytes = 2U;
		expect(read_blob(value, bounded).Status == application::diff_blob_status::materialization_limit
				   && value.State.ExactBlobRequests.empty(),
			   "retained bytes over the request's limit are refused, not read again");

		// A blob no grant names is still refused, prepared or not.
		auto foreign             = prepared_blob_request(value.Host->state(), prepared.Observations[0], core::diff_side::left);
		foreign.Context.Key.File = core::file_identity_of("unprepared.cpp");
		expect(read_blob(value, foreign).Status == application::diff_blob_status::failure && value.State.ExactBlobRequests.empty(),
			   "an unprepared file cannot borrow a prepared grant");

		// Invalidation drops the grants and the retained bytes.
		value.Host->invalidate(matching_invalidation());
		const auto revoked = value.Host->materialize(prepared_blob_request(value.Host->state(), prepared.Observations[2], core::diff_side::left), { });
		expect(revoked.Status == application::diff_blob_status::failure && value.State.ExactBlobRequests.empty(),
			   "after invalidation nothing prepared can be read");
	}

	/// [[BPR-1344]]. Preparation's jobs are counted apart from the reviewer's,
	/// so a caller can wait for the reviewer's alone.
	void preparation_jobs_are_counted_apart_from_the_reviewers()
	{
		fixture value;
		establish_sparse_three_files(value);
		expect(value.Host->outstanding_jobs() == 0U && value.Host->outstanding_preparation_jobs() == 0U
				   && value.Host->outstanding_reviewer_jobs() == 0U,
			   "an established comparison owes no job");
		auto prepared        = application::epoch_prepared_comparisons{ };
		auto opened          = application::workbench_selected_diff_observation{ };
		auto preparationDone = std::atomic_bool{ false };
		auto openDone        = std::atomic_bool{ false };
		auto preparation     = std::jthread{ [&] {
			prepared = value.Host->prepare({ file_request(value, "a.cpp") }, { });
			preparationDone.store(true);
		} };
		value.Effects.wait_for_pending();
		expect(value.Host->outstanding_preparation_jobs() == 1U && value.Host->outstanding_reviewer_jobs() == 0U
				   && value.Host->outstanding_jobs() == 1U,
			   "a queued preparation is preparation's job, not the reviewer's");
		auto open = std::jthread{ [&] {
			opened = value.Host->resolve(file_request(value, "b.cpp", core::request_id{ 902U }), { });
			openDone.store(true);
		} };
		while (value.Host->outstanding_reviewer_jobs() == 0U && !openDone.load()) {
			std::this_thread::yield();
		}
		expect(value.Host->outstanding_reviewer_jobs() == 1U && value.Host->outstanding_preparation_jobs() == 1U
				   && value.Host->outstanding_jobs() == 2U,
			   "the reviewer's open is counted as theirs beside the preparation");
		expect(value.Host->active_reviewer_jobs() == 1U, "[[BPR-1344]] and is still to run, so the reviewer is busy");
		while (!preparationDone.load() || !openDone.load()) {
			if (value.Effects.pending() != 0U) {
				value.Effects.run_next();
			} else {
				std::this_thread::yield();
			}
		}
		preparation.join();
		open.join();
		expect(prepared.Observations.size() == 1U && opened.Status == application::workbench_selected_diff_status::available,
			   "both the preparation and the open complete");
		expect(value.Host->outstanding_jobs() == 0U && value.Host->outstanding_preparation_jobs() == 0U
				   && value.Host->outstanding_reviewer_jobs() == 0U,
			   "both counts return to zero, the retiring actor jobs included");
		expect(value.Host->active_reviewer_jobs() == 0U, "[[BPR-1344]] nothing of the reviewer's is left to run");
	}

	/// [[BPR-1344]]. A prepared comparison's blob read is preparation's job, not
	/// the reviewer's, while it waits on the actor; the same read unmarked is
	/// the reviewer's.
	void a_prepared_blob_read_is_preparations_job()
	{
		fixture value;
		establish_sparse_three_files(value);
		const auto prepared = prepare_window(value, { file_request(value, "a.cpp"), file_request(value, "b.cpp") });
		expect(prepared.Observations.size() == 2U, "two files are prepared");
		const auto read = [&value](application::diff_blob_request aRequest, const bool aPreparation) {
			auto result = std::optional<application::diff_blob_observation>{ };
			auto worker = std::jthread{ [&] { result = value.Host->materialize(aRequest, { }); } };
			value.Effects.wait_for_pending();
			expect(value.Host->outstanding_preparation_jobs() == (aPreparation ? 1U : 0U)
					   && value.Host->outstanding_reviewer_jobs() == (aPreparation ? 0U : 1U),
				   aPreparation ? "a marked read waits as preparation's job" : "control: an unmarked read waits as the reviewer's");
			value.Effects.run_next();
			worker.join();
			expect(result.has_value() && result->Status == application::diff_blob_status::available, "the read completes");
			expect(value.Host->outstanding_jobs() == 0U, "and leaves no job behind");
		};
		auto marked     = prepared_blob_request(value.Host->state(), prepared.Observations[0], core::diff_side::left);
		marked.Prepared = true;
		read(marked, true);
		read(prepared_blob_request(value.Host->state(), prepared.Observations[1], core::diff_side::left), false);
	}

	void a_batch_answer_that_is_not_its_blob_is_not_retained()
	{
		fixture value;
		establish_sparse_three_files(value);
		value.State.BatchContents[0].second = "forged\n";
		const auto window                   = std::vector{ file_request(value, "a.cpp") };
		const auto prepared                 = prepare_window(value, window);
		expect(prepared.Observations.size() == 1U && prepared.RemoteBlobs == 1U,
			   "only the answer that hashes to its object ID is retained");
		const auto left = read_blob(value, prepared_blob_request(value.Host->state(), prepared.Observations[0], core::diff_side::left));
		expect(left.Status == application::diff_blob_status::available && value.State.ExactBlobRequests.size() == 1U,
			   "the unverified blob is read on its own, exactly, when it is needed");
	}

	void preparation_without_a_batching_transport_reads_nothing_ahead()
	{
		fixture value;
		establish_sparse_three_files(value);
		value.State.BatchUnsupported = true;
		const auto prepared          = prepare_window(value, { file_request(value, "a.cpp"), file_request(value, "b.cpp") });
		expect(prepared.Observations.size() == 2U && prepared.RemoteBlobs == 0U && value.State.ExactBlobRequests.empty(),
			   "a refused batch falls back to nothing: no per-blob reads are spent ahead of the reviewer");
	}

	/// [[BPR-1359]]. Preparation's own exact read of a blob runs on the
	/// repository actor, where the reviewer's cold selection queues too. Live,
	/// that read is five gh processes (about 2.1 s) and the reviewer waited
	/// about 1.9 s behind it.
	void a_reviewer_selection_stops_preparations_exact_read()
	{
		using application::diff_blob_status;
		using application::workbench_selected_diff_status;
		fixture value;
		establish_sparse_three_files(value);
		// The batch answers a.cpp's left blob with bytes that are not it, so
		// preparation may read that one blob on its own.
		value.State.BatchContents[0].second = "forged\n";
		const auto prepared                 = prepare_window(value, { file_request(value, "a.cpp") });
		expect(prepared.Observations.size() == 1U && prepared.RemoteBatches == 1U && prepared.RemoteBlobs == 1U,
			   "a.cpp is prepared, its left blob left to a single read");
		value.State.ExactBlobLatency             = std::chrono::seconds{ 20 };
		value.State.ExactBlobLatencyObservesStop = true;
		const auto preparedLeft                  = prepared_blob_request(value.Host->state(), prepared.Observations[0], core::diff_side::left);

		// The diff host computes the prepared comparison: its read of the
		// left blob is on the wire, on the actor.
		auto preparedRead = std::optional<application::diff_blob_observation>{ };
		auto preparer     = std::jthread{ [&] { preparedRead = value.Host->materialize(preparedLeft, { }); } };
		value.Effects.wait_for_pending();
		auto actor = std::jthread{ [&] { value.Effects.run_next(); } };
		while (value.State.ExactBlobReadsInFlight.load() == 0U) {
			std::this_thread::yield();
		}

		// The reviewer selects b.cpp, which was not prepared: its resolve
		// needs the actor.
		const auto started  = std::chrono::steady_clock::now();
		const auto selected = open_file(value, file_request(value, "b.cpp", core::request_id{ 930U }));
		const auto waited   = std::chrono::steady_clock::now() - started;
		preparer.join();
		actor.join();
		std::cout << "BPR-1359 reviewer resolve behind a prepared exact read: "
				  << std::chrono::duration<double, std::milli>(waited).count() << " ms\n";
		expect(selected.Status == workbench_selected_diff_status::available, "the reviewer's cold selection resolves");
		expect(value.State.ExactBlobStopsObserved.load() == 1U && preparedRead
				   && preparedRead->Status == diff_blob_status::cancelled && waited < value.State.ExactBlobLatency,
			   "the reviewer's selection stops preparation's exact read on the actor, as a gh process killed, "
			   "instead of waiting it out");

		// A prepared read queued on the actor ahead of reviewer work does not
		// start once the reviewer's work is queued behind it.
		const auto requests   = value.State.ExactBlobRequests.size();
		auto       queuedRead = std::optional<application::diff_blob_observation>{ };
		auto       queued     = std::jthread{ [&] { queuedRead = value.Host->materialize(preparedLeft, { }); } };
		value.Effects.wait_for_pending();
		auto reviewed = std::optional<application::workbench_selected_diff_observation>{ };
		auto reviewer = std::jthread{ [&] {
			reviewed = value.Host->resolve(file_request(value, "c.cpp", core::request_id{ 931U }), { });
		} };
		while (value.Host->queued_actor_work() < 2U) {
			std::this_thread::yield();
		}
		value.Effects.run_next();
		queued.join();
		reviewer.join();
		expect(reviewed && reviewed->Status == workbench_selected_diff_status::available && queuedRead
				   && queuedRead->Status == diff_blob_status::cancelled && value.State.ExactBlobRequests.size() == requests,
			   "a prepared read whose turn comes while reviewer work is queued is not started");

		// The reviewer's own blob is read: only preparation yields.
		value.State.ExactBlobLatency = std::chrono::milliseconds{ 0 };
		const auto own               = read_blob(value, prepared_blob_request(value.Host->state(), *reviewed, core::diff_side::left));
		expect(own.Status == diff_blob_status::available && value.State.ExactBlobRequests.size() == requests + 1U
				   && value.State.ExactBlobStopsObserved.load() == 1U,
			   "the reviewer's selected blob is read exactly and is not stopped");
	}

	/// [[BPR-1359]]. Live, a batched read that always failed left preparation
	/// reading every prepared blob one at a time: 710 gh processes against
	/// 122 with preparation off.
	void a_failed_batch_backs_off_preparations_single_reads()
	{
		using application::diff_blob_status;
		using application::workbench_selected_diff_status;
		fixture value;
		establish_sparse_three_files(value);
		value.State.BatchUnsupported = true;
		const auto prepared          = prepare_window(value, { file_request(value, "a.cpp"), file_request(value, "b.cpp") });
		expect(prepared.Observations.size() == 2U && prepared.RemoteBatches == 1U && prepared.RemoteBlobs == 0U,
			   "the batch fails");
		const auto skipped = read_blob(value, prepared_blob_request(value.Host->state(), prepared.Observations[1], core::diff_side::left));
		expect(skipped.Status == diff_blob_status::failure && value.State.ExactBlobRequests.empty(),
			   "after a failed batch, a prepared file's blob is not read on its own: preparation backs off");

		const auto opened = open_file(value, file_request(value, "a.cpp", core::request_id{ 940U }));
		const auto own    = read_blob(value, prepared_blob_request(value.Host->state(), opened, core::diff_side::left));
		expect(opened.Status == workbench_selected_diff_status::available && own.Status == diff_blob_status::available
				   && value.State.ExactBlobRequests.size() == 1U,
			   "the reviewer's own file is still read exactly");

		// A batch that works again lifts the back-off; a blob it answers
		// absent may then be read on its own.
		value.State.BatchUnsupported        = false;
		value.State.BatchContents[4].second = "forged\n";
		const auto again                    = prepare_window(value, { file_request(value, "c.cpp", core::request_id{ 941U }) });
		expect(again.Observations.size() == 1U && again.RemoteBatches == 1U && again.RemoteBlobs == 1U, "the next batch succeeds");
		const auto resumed = read_blob(value, prepared_blob_request(value.Host->state(), again.Observations[0], core::diff_side::left));
		expect(resumed.Status == diff_blob_status::available && value.State.ExactBlobRequests.size() == 2U,
			   "a successful batch lifts the back-off");
	}

	/// [[BPR-1360]]. One worker thread, as the composition's `DiffEffects`.
	class worker_thread_executor final : public application::effect_executor
	{
	public:
		worker_thread_executor()
			: Worker{ [this](const std::stop_token aStop) { work(aStop); } }
		{ }

		~worker_thread_executor() override
		{
			Worker.request_stop();
			Changed.notify_all();
		}

		worker_thread_executor(const worker_thread_executor&)                    = delete;
		auto operator=(const worker_thread_executor&) -> worker_thread_executor& = delete;
		worker_thread_executor(worker_thread_executor&&)                         = delete;
		auto operator=(worker_thread_executor&&) -> worker_thread_executor&      = delete;

		void post(std::function<void()> aTask) override
		{
			std::scoped_lock lock{ Mutex };
			Tasks.push_back(std::move(aTask));
			Changed.notify_all();
		}

	private:
		void work(const std::stop_token& aStop)
		{
			while (true) {
				auto task = std::function<void()>{ };
				{
					std::unique_lock lock{ Mutex };
					Changed.wait(lock, aStop, [this] { return !Tasks.empty(); });
					if (Tasks.empty()) {
						return;
					}
					task = std::move(Tasks.front());
					Tasks.pop_front();
				}
				task();
			}
		}

		std::mutex                        Mutex;
		std::condition_variable_any       Changed;
		std::deque<std::function<void()>> Tasks;
		std::jthread                      Worker;
	};

	/// [[BPR-1360]]. What the diff host publishes, and when.
	class diff_outcomes final : public application::diff_projection_port
	{
	public:
		void publish(const application::diff_publication& aPublication) override
		{
			Published.push_back(aPublication.Context);
			At = std::chrono::steady_clock::now();
		}

		void fail(const application::diff_failure_publication& aFailure) override
		{
			Failed.push_back(aFailure);
			At = std::chrono::steady_clock::now();
		}

		void invalidate(const core::pull_request_authority&) override { }

		std::vector<core::diff_request_context>            Published;
		std::vector<application::diff_failure_publication> Failed;
		std::chrono::steady_clock::time_point              At;
	};

	/// \brief A cold open as the composition runs it. [[BPR-1360]]
	///
	/// \details The real diff host computes the opened comparison over the
	/// real epoch host on a worker thread of its own; the repository actor's
	/// FIFO runs on another; this thread is the diff host's decision lane.
	struct cold_open_rig final
	{
		explicit cold_open_rig(fixture& aValue)
			: Value{ aValue }
			, Diff{ core::diff_state{ .ApplicationSession = aValue.Host->state().ApplicationSession },
					*aValue.Host,
					DiffEffects,
					DiffDecisions,
					Outcomes,
					bpr::tests::product_line_matcher() }
			, Actor{ [this](const std::stop_token aStop) {
				while (!aStop.stop_requested()) {
					if (Value.Effects.pending() != 0U) {
						Value.Effects.run_next();
					} else {
						std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
					}
				}
			} }
		{ }

		~cold_open_rig()
		{
			Diff.shutdown();
			static_cast<void>(pump_until([this] { return Diff.is_quiescent(); }));
			Actor.request_stop();
		}

		cold_open_rig(const cold_open_rig&)                    = delete;
		auto operator=(const cold_open_rig&) -> cold_open_rig& = delete;
		cold_open_rig(cold_open_rig&&)                         = delete;
		auto operator=(cold_open_rig&&) -> cold_open_rig&      = delete;

		/// Runs the diff host's decision lane until the condition holds or
		/// the bound runs out.
		template <typename Condition>
		[[nodiscard]] auto pump_until(Condition                       aCondition,
									  const std::chrono::milliseconds aBound = std::chrono::seconds{ 30 }) -> bool
		{
			const auto deadline = std::chrono::steady_clock::now() + aBound;
			while (!aCondition()) {
				if (std::chrono::steady_clock::now() >= deadline) {
					return false;
				}
				if (DiffDecisions.pending() != 0U) {
					DiffDecisions.run_next();
				} else {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
				}
			}
			return true;
		}

		/// Opens a resolved file and waits for its comparison's terminal.
		/// Returns the milliseconds from the request to the terminal.
		[[nodiscard]] auto open(const application::workbench_selected_diff_observation& aResolved) -> double
		{
			const auto key = application::selected_diff_key(aResolved);
			expect(key.has_value(), "an available resolution has a diff key");
			const auto terminals = Outcomes.Published.size() + Outcomes.Failed.size();
			const auto started   = std::chrono::steady_clock::now();
			Diff.request(*key, core::diff_options{ });
			expect(pump_until([&] { return Outcomes.Published.size() + Outcomes.Failed.size() > terminals; }),
				   "the opened comparison reaches a terminal");
			return std::chrono::duration<double, std::milli>(Outcomes.At - started).count();
		}

		fixture&                           Value;
		diff_outcomes                      Outcomes;
		queued_executor                    DiffDecisions{ true };
		worker_thread_executor             DiffEffects;
		application::diff_application_host Diff;
		std::jthread                       Actor;
	};

	void set_exact_blob_latency(fixture& aValue, const std::chrono::milliseconds aLatency, const bool aObservesStop)
	{
		std::scoped_lock lock{ aValue.State.ExactBlobMutex };
		aValue.State.ExactBlobLatency             = aLatency;
		aValue.State.ExactBlobLatencyObservesStop = aObservesStop;
	}

	/// [[BPR-1360]]. Live on macOS a cold open of a modified file waited for
	/// two exact reads, one per side, one after the other (about 2.1 s each).
	/// The two reads are now on the wire together: the open waits for the
	/// longer of them, not their sum. Each read takes 300 ms, or a measured
	/// live round trip passed as `--live-single-blob-ms` if that is longer.
	void a_cold_open_reads_both_sides_at_once(const std::chrono::milliseconds aSingle)
	{
		const auto delay = std::max(aSingle, std::chrono::milliseconds{ 300 });
		fixture    value;
		establish_sparse_three_files(value);
		set_exact_blob_latency(value, delay, false);
		const auto resolved = open_file(value, file_request(value, "a.cpp", core::request_id{ 950U }));
		expect(resolved.Status == application::workbench_selected_diff_status::available, "a.cpp resolves cold");

		auto rig     = std::make_unique<cold_open_rig>(value);
		auto elapsed = rig->open(resolved);
		expect(rig->Outcomes.Published.size() == 1U && rig->Outcomes.Failed.empty(), "the cold open's comparison is published");
		rig.reset();

		auto spans = std::vector<shared_state::exact_blob_span>{ };
		auto most  = std::size_t{ 0U };
		{
			std::scoped_lock lock{ value.State.ExactBlobMutex };
			spans = value.State.ExactBlobSpans;
			most  = value.State.ExactBlobMostInFlight;
		}
		expect(spans.size() == 2U && spans[0].Blob != spans[1].Blob, "each side is read exactly once");
		const auto firstEnd    = std::min(spans[0].Ended, spans[1].Ended);
		const auto secondStart = std::max(spans[0].Started, spans[1].Started);
		const auto gap         = std::chrono::duration<double, std::milli>(firstEnd - secondStart).count();
		std::cout << "BPR-1360 cold open, two exact reads of " << delay.count() << " ms: open " << elapsed
				  << " ms (sum " << 2 * delay.count() << ", larger " << delay.count() << "); most in flight " << most
				  << "; second read started " << gap << " ms before the first ended\n";
		expect(most == 2U, "both sides' reads are on the wire together");
		expect(secondStart < firstEnd, "the second side's read starts before the first side's ends");
		expect(elapsed < 1.6 * static_cast<double>(delay.count()),
			   "the cold open waits for about the longer read, well under the sum of the two");
	}

	/// [[BPR-1360]] under [[BPR-1359]]'s rule: both of a cold open's reads
	/// are the reviewer's own. A prepared read on the actor is stopped for
	/// them; preparation stopping its own work, or asking for another read
	/// while they are on the wire, neither stops nor holds either of them.
	void preparation_cannot_stop_or_delay_either_concurrent_reviewer_read()
	{
		using application::diff_blob_status;
		constexpr auto delay = std::chrono::milliseconds{ 300 };
		fixture        value;
		establish_sparse_three_files(value);
		// a.cpp's left blob is left to a single prepared read.
		value.State.BatchContents[0].second = "forged\n";
		const auto prepared                 = prepare_window(value, { file_request(value, "a.cpp") });
		expect(prepared.Observations.size() == 1U && prepared.RemoteBlobs == 1U, "a.cpp is prepared but for its left blob");
		const auto resolved = open_file(value, file_request(value, "b.cpp", core::request_id{ 960U }));
		expect(resolved.Status == application::workbench_selected_diff_status::available, "the reviewer selects b.cpp, unprepared");
		const auto preparedLeft = prepared_blob_request(value.Host->state(), prepared.Observations[0], core::diff_side::left);
		const auto laterRequest = file_request(value, "c.cpp", core::request_id{ 961U });

		auto rig = std::make_unique<cold_open_rig>(value);
		// Preparation's read is on the wire, on the actor, for 20 s.
		set_exact_blob_latency(value, std::chrono::seconds{ 20 }, true);
		auto preparedRead = std::optional<application::diff_blob_observation>{ };
		auto preparer     = std::jthread{ [&] { preparedRead = value.Host->materialize(preparedLeft, { }); } };
		expect(rig->pump_until([&] { return value.State.ExactBlobReadsInFlight.load() == 1U; }), "preparation's read is in flight");
		set_exact_blob_latency(value, delay, true);

		// While the reviewer's two reads are on the wire, preparation asks for
		// another read and for another window, and then stops its work.
		auto       preparationStop = std::stop_source{ };
		auto       laterPreparer   = std::optional<std::jthread>{ };
		auto       laterWindow     = std::optional<std::jthread>{ };
		auto       interfered      = false;
		const auto key             = application::selected_diff_key(resolved);
		expect(key.has_value(), "b.cpp has a diff key");
		const auto started = std::chrono::steady_clock::now();
		rig->Diff.request(*key, core::diff_options{ });
		expect(rig->pump_until([&] {
			if (!interfered && value.State.ExactBlobReadsInFlight.load() == 2U) {
				interfered = true;
				laterPreparer.emplace([&] { static_cast<void>(value.Host->materialize(preparedLeft, { })); });
				laterWindow.emplace([&] {
					static_cast<void>(value.Host->prepare({ laterRequest }, preparationStop.get_token()));
				});
				rig->Diff.prepare({ });
				preparationStop.request_stop();
			}
			return !rig->Outcomes.Published.empty() || !rig->Outcomes.Failed.empty();
		}),
			   "the reviewer's comparison reaches a terminal");
		const auto elapsed   = std::chrono::duration<double, std::milli>(rig->Outcomes.At - started).count();
		const auto published = rig->Outcomes.Published.size();
		preparer.join();
		if (laterPreparer) {
			laterPreparer->join();
		}
		if (laterWindow) {
			laterWindow->join();
		}
		rig.reset();

		auto spans = std::vector<shared_state::exact_blob_span>{ };
		{
			std::scoped_lock lock{ value.State.ExactBlobMutex };
			spans = value.State.ExactBlobSpans;
		}
		const auto reviewers = [&resolved](const shared_state::exact_blob_span& aSpan) {
			return aSpan.Blob == *resolved.LeftBlob || aSpan.Blob == *resolved.RightBlob;
		};
		auto reviewer = std::vector<shared_state::exact_blob_span>{ };
		std::ranges::copy_if(spans, std::back_inserter(reviewer), reviewers);
		std::cout << "BPR-1360 reviewer's cold open behind a prepared exact read: " << elapsed << " ms\n";
		expect(published == 1U, "the reviewer's comparison is published");
		expect(preparedRead && preparedRead->Status == diff_blob_status::cancelled && !spans.empty()
				   && spans.front().Blob == preparedLeft.Blob && spans.front().Stopped,
			   "the reviewer's cold open stops preparation's exact read on the actor");
		expect(interfered && reviewer.size() == 2U && std::ranges::none_of(reviewer, &shared_state::exact_blob_span::Stopped)
				   && std::max(reviewer[0].Started, reviewer[1].Started) < std::min(reviewer[0].Ended, reviewer[1].Ended),
			   "both of the reviewer's reads run together and preparation stops neither");
		const auto firstStart = std::min(reviewer[0].Started, reviewer[1].Started);
		const auto lastEnd    = std::max(reviewer[0].Ended, reviewer[1].Ended);
		expect(std::ranges::none_of(spans,
									[&](const shared_state::exact_blob_span& aSpan) {
										return !reviewers(aSpan) && aSpan.Started >= firstStart && aSpan.Started < lastEnd;
									})
				   && value.State.ExactBlobStopsObserved.load() == 1U && elapsed < 1.6 * static_cast<double>(delay.count()),
			   "preparation holds neither read: nothing of preparation's starts while they are on the wire, and the "
			   "open waits about one read, not the prepared read's 20 s");
	}

	/// [[BPR-1360]]. The reviewer moving on stops both of a cold open's reads.
	void a_superseded_cold_open_stops_both_reads()
	{
		fixture value;
		establish_sparse_three_files(value);
		const auto resolved = open_file(value, file_request(value, "a.cpp", core::request_id{ 970U }));
		expect(resolved.Status == application::workbench_selected_diff_status::available, "a.cpp resolves cold");
		set_exact_blob_latency(value, std::chrono::seconds{ 20 }, true);
		auto       rig = std::make_unique<cold_open_rig>(value);
		const auto key = application::selected_diff_key(resolved);
		expect(key.has_value(), "a.cpp has a diff key");
		rig->Diff.request(*key, core::diff_options{ });
		expect(rig->pump_until([&] { return value.State.ExactBlobReadsInFlight.load() == 2U; }, std::chrono::seconds{ 5 }),
			   "both of the open's reads are on the wire");
		const auto stopped = std::chrono::steady_clock::now();
		rig->Diff.cancel();
		expect(rig->pump_until([&] { return value.State.ExactBlobReadsInFlight.load() == 0U && rig->Diff.outstanding_jobs() == 0U; },
							   std::chrono::seconds{ 5 }),
			   "a cancelled open's reads end and its job finishes");
		const auto ended = std::chrono::steady_clock::now() - stopped;
		rig.reset();
		expect(value.State.ExactBlobStopsObserved.load() == 2U && ended < std::chrono::seconds{ 5 },
			   "the reviewer moving on stops both reads, not only the one on the actor");
	}

	void preparation_is_bounded_cancellable_and_complete_reviews_stay_local()
	{
		fixture value;
		establish_sparse_three_files(value);
		auto stopped = std::stop_source{ };
		stopped.request_stop();
		const auto cancelled = prepare_window(value, { file_request(value, "a.cpp") }, stopped.get_token());
		expect(cancelled.Observations.empty() && value.State.ExactBlobBatches.empty(), "a stopped preparation reads nothing");
		auto oversized = std::vector<application::workbench_selected_diff_request>{ };
		for (std::size_t index = 0U; index <= core::maximum_prepared_comparison_window; ++index) {
			oversized.push_back(file_request(value, "f" + std::to_string(index) + ".cpp"));
		}
		const auto refused = prepare_window(value, oversized);
		expect(refused.Observations.empty() && value.State.TreeEntryRequests.empty(), "a window over the bound is refused before any lookup");

		fixture complete;
		establish_ready_comparison(complete);
		const auto local = prepare_window(complete, { file_request(complete, "a.cpp"), file_request(complete, "b.cpp") });
		expect(local.Observations.size() == 2U && local.RemoteBatches == 0U && complete.State.ExactBlobBatches.empty(),
			   "a complete review prepares its resolutions and reads nothing remotely");
	}

	/// The live-shaped timing: the shipped per-blob read costs the measured
	/// round trip of five gh calls; a batch costs one round trip plus the
	/// account observations. The registered row uses short stand-ins; a
	/// measurement passes the measured live figures as arguments. [[BPR-1327]]
	void live_shaped_open_latency(const std::chrono::milliseconds aSingle, const std::chrono::milliseconds aBatch)
	{
		const auto single = aSingle;
		const auto batch  = aBatch;
		const auto open   = [](fixture& aValue, const std::string& aPath, const core::request_id aRequest) {
			const auto started  = std::chrono::steady_clock::now();
			const auto resolved = open_file(aValue, file_request(aValue, aPath, aRequest));
			expect(resolved.Status == application::workbench_selected_diff_status::available, "file opens");
			expect(read_blob(aValue, prepared_blob_request(aValue.Host->state(), resolved, core::diff_side::left)).Status
						   == application::diff_blob_status::available
					   && read_blob(aValue, prepared_blob_request(aValue.Host->state(), resolved, core::diff_side::right)).Status
						   == application::diff_blob_status::available,
				   "both sides read");
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
		};
		fixture before;
		establish_sparse_three_files(before);
		before.State.ExactBlobLatency = single;
		const auto beforeFirst        = open(before, "a.cpp", core::request_id{ 910U });
		const auto beforeLater        = open(before, "b.cpp", core::request_id{ 911U });
		expect(before.State.ExactBlobRequests.size() == 4U && beforeFirst >= 2.0 * static_cast<double>(single.count()),
			   "control: without preparation every open pays two network reads");

		fixture after;
		establish_sparse_three_files(after);
		after.State.ExactBlobLatency = single;
		after.State.BatchLatency     = batch;
		const auto preparedAt        = std::chrono::steady_clock::now();
		const auto prepared          = prepare_window(after, { file_request(after, "a.cpp"), file_request(after, "b.cpp"), file_request(after, "c.cpp") });
		const auto preparation       = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - preparedAt).count();
		const auto afterFirst        = open(after, "a.cpp", core::request_id{ 912U });
		const auto afterLater        = open(after, "b.cpp", core::request_id{ 913U });
		expect(prepared.RemoteBlobs == 6U && after.State.ExactBlobRequests.empty()
				   && afterFirst < static_cast<double>(single.count()) && afterLater < static_cast<double>(single.count()),
			   "prepared opens perform no network read");
		std::cout << "BPR-1327 live-shaped open ms: single_blob=" << single.count() << " batch=" << batch.count()
				  << " before_first=" << beforeFirst << " before_later=" << beforeLater << " preparation=" << preparation
				  << " after_first=" << afterFirst << " after_later=" << afterLater << '\n';
	}

	void sparse_capability_applies_independently_of_pull_request_size()
	{
		fixture large;
		large.State.SupportsSparse = true;
		large.State.SparseObjects  = true;
		establish_ready_comparison(large, application::accepted_pull_request_shape{ 101U, 1U, 1U });
		expect(std::ranges::find(large.State.Calls, "cache.prepare-sparse-revision") != large.State.Calls.end()
				   && std::ranges::find(large.State.Calls, "cache.prepare-revision") == large.State.Calls.end()
				   && large.State.Retained
				   && large.State.Retained->Residency == application::git_revision_residency::sparse_commit_trees
				   && large.Host->state().AcceptedComparison,
			   "large accepted shape imports exact sparse current anchors and resolves their comparison");

		fixture small;
		small.State.SupportsSparse = true;
		small.State.SparseObjects  = true;
		establish_ready_comparison(small, application::accepted_pull_request_shape{ 40U, 4U, 4U });
		expect(std::ranges::find(small.State.Calls, "cache.prepare-sparse-revision") != small.State.Calls.end()
				   && std::ranges::find(small.State.Calls, "cache.prepare-revision") == small.State.Calls.end()
				   && small.State.Retained
				   && small.State.Retained->Residency == application::git_revision_residency::sparse_commit_trees
				   && small.Host->state().AcceptedComparison,
			   "a small PR on a sparse-capable lease imports exact trees without unrelated repository history");

		fixture complete;
		establish_ready_comparison(complete, application::accepted_pull_request_shape{ 40U, 4U, 4U });
		expect(complete.State.Retained
				   && complete.State.Retained->Residency == application::git_revision_residency::complete_closure,
			   "a lease without sparse support retains complete acquisition");

		fixture missingMarker;
		missingMarker.State.SupportsSparse             = true;
		missingMarker.State.SparseObjects              = true;
		missingMarker.State.SparseCatalogMarkerMissing = true;
		auto handoff                                   = make_handoff(missingMarker.State, 1U, 20U, true);
		handoff.Shape                                  = application::accepted_pull_request_shape{ 101U, 1U, 1U };
		missingMarker.Host->accept(std::move(handoff));
		drive_through_preparation(missingMarker);
		run_effect_completion(missingMarker);
		drain(missingMarker);
		expect(missingMarker.Host->state().AcceptedComparison
				   && missingMarker.Host->state().AcceptedComparison->Left.Availability != core::anchor_availability::available
				   && missingMarker.Host->state().AcceptedComparison->Right.Availability != core::anchor_availability::available
				   && std::ranges::find(missingMarker.State.Calls, "catalog.retained-residency") != missingMarker.State.Calls.end()
				   && std::ranges::find(missingMarker.State.Calls, "cache.inspect-sparse-commit") == missingMarker.State.Calls.end(),
			   "physical sparse objects alone cannot replace a missing durable residency marker");
	}

	void selected_large_occurrence_imports_only_its_sparse_head()
	{
		fixture value;
		value.State.SupportsSparse       = true;
		value.State.SparseObjects        = true;
		value.State.OccurrencesSupported = true;
		value.Clock.set_next_utc(1'789'000'000'500LL);
		auto handoff                 = make_handoff(value.State, 1U, 20U, true);
		handoff.Shape                = application::accepted_pull_request_shape{ 101U, 1U, 1U };
		handoff.ForcePushes          = { force_push(101U, 21U) };
		handoff.TimelineAvailability = application::epoch_timeline_availability::available;
		value.Host->accept(std::move(handoff));
		drain(value);
		expect(value.Host->state().AcceptedComparison && value.Host->state().Catalog
				   && value.State.Occurrences.Occurrences.size() == 1U
				   && !value.State.Occurrences.Occurrences.front().LocalEpoch
				   && std::ranges::count(value.State.Calls, "cache.prepare-sparse-revision") == 1,
			   "large open establishes only the current sparse pair while the older event remains unimported");
		const auto before                 = *value.Host->state().AcceptedComparison;
		const auto catalog                = *value.Host->state().Catalog;
		const auto occurrence             = value.State.Occurrences.Occurrences.front().Id;
		value.State.AfterCommitAuthorized = [&value] {
			expect(value.State.Operation.has_value(), "selected import has one durable operation");
			value.State.RecoveryInspections.emplace_back(recovery_observation(
				*value.State.Operation, application::git_recovery_ref_topology::staging,
				application::git_recovery_match::exact, application::git_recovery_match::absent));
		};
		value.Host->dispatch(core::select_epoch_occurrence_intent{
			.Side            = core::comparison_side::left,
			.Occurrence      = occurrence,
			.ExpectedCatalog = catalog,
		});
		drain(value);
		const auto& linked = value.State.Occurrences.Occurrences.front();
		expect(linked.LocalEpoch && linked.DerivedComparisonBase == before.Left.Commit
				   && !linked.HistoricalBase
				   && value.Host->state().Catalog->Revision.value() == catalog.Revision.value() + 1U
				   && std::ranges::count(value.State.Calls, "cache.prepare-sparse-revision") == 2
				   && std::ranges::find(value.State.Calls, "cache.prepare-historical-revision") == value.State.Calls.end(),
			   "one exact selected event imports a sparse head and labels its comparison base without inventing historical provenance");
		expect(value.Host->state().AcceptedComparison
				   && value.Host->state().AcceptedComparison->Left.Identity.Kind == core::comparison_anchor_kind::epoch_head
				   && value.Host->state().AcceptedComparison->Left.Identity.Epoch == linked.LocalEpoch->Ordinal
				   && value.Host->state().AcceptedComparison->Left.Commit == linked.Evidence.Head,
			   "the selected occurrence becomes the readable exact left comparison anchor after durable import");
		const auto selected = resolve_selected_diff(value, selected_diff_request_for(value), { });
		expect(selected.Status == application::workbench_selected_diff_status::available
				   && selected.LeftBlob && selected.RightBlob,
			   "selected historical file resolves exact blobs before remote materialization");
		value.State.CachedBlobMissing = true;
		// A complete opposite endpoint cannot veto the exact missing sparse blob.
		const auto currentOrdinal = value.State.Snapshot.LatestLive->Id.Ordinal;
		const auto currentMarker  = std::ranges::find_if(value.State.RetainedResidencies,
														 [currentOrdinal](const auto& aEntry) { return aEntry.first == currentOrdinal; });
		expect(currentMarker != value.State.RetainedResidencies.end(),
			   "current comparison endpoint has a durable residency marker");
		currentMarker->second        = application::git_revision_residency::complete_closure;
		auto blob                    = blob_request_for(value.Host->state());
		blob.Context.Key.LeftAnchor  = selected.Request.Diff.Comparison.Left.Identity;
		blob.Context.Key.RightAnchor = selected.Request.Diff.Comparison.Right.Identity;
		blob.Context.Key.LeftBlob    = selected.LeftBlob;
		blob.Context.Key.RightBlob   = selected.RightBlob;
		blob.Context.Key.File        = selected.Request.File.File;
		blob.Context.Key.Options     = selected.Request.Diff.OptionsDigest;
		blob.Context.Key.Algorithm   = selected.Request.Diff.Algorithm;
		blob.Blob                    = *selected.LeftBlob;
		auto materialized            = std::optional<application::diff_blob_observation>{ };
		auto reader                  = std::jthread{ [&] { materialized = value.Host->materialize(blob, { }); } };
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		reader.join();
		expect(materialized && materialized->Status == application::diff_blob_status::available
				   && materialized->Bytes == std::vector<std::uint8_t>{ 'e', 'x', 'a', 'c', 't', ' ', 's', 'e', 'l', 'e', 'c', 't', 'e', 'd', ' ', 'b', 'l', 'o', 'b' }
				   && value.State.ExactBlobRequests.size() == 1U
				   && value.State.ExactBlobRequests.front().ExpectedBlob == blob.Blob,
			   "historical sparse endpoint reads only its selected exact blob with a complete opposite endpoint");
		// [[BPR-1560]]. The imported update's base is the current comparison
		// base taken as is, unproven for that head: "Show this update alone"
		// is unavailable, while its head stays a readable anchor.
		{
			const auto importedEpoch = *linked.LocalEpoch;
			const auto anchorOf      = [&](const core::comparison_anchor_kind aKind) {
				return core::comparison_anchor_identity{ .Authority = importedEpoch.Authority, .Incarnation = importedEpoch.Incarnation, .Kind = aKind, .Epoch = importedEpoch.Ordinal };
			};
			value.Host->dispatch(core::select_comparison_anchors_intent{
				.Left                    = anchorOf(core::comparison_anchor_kind::epoch_base),
				.Right                   = anchorOf(core::comparison_anchor_kind::epoch_head),
				.ExpectedCatalogRevision = value.Host->state().Catalog->Revision,
			});
			drain(value);
			expect(value.Host->state().AcceptedComparison
					   && value.Host->state().AcceptedComparison->Left.Identity.Kind == core::comparison_anchor_kind::epoch_base
					   && value.Host->state().AcceptedComparison->Left.Availability == core::anchor_availability::unavailable
					   && value.Host->state().AcceptedComparison->Right.Availability == core::anchor_availability::available,
				   "a selected-objects import's own base is no anchor to compare from");
		}
		const auto imports = std::ranges::count(value.State.Calls, "cache.prepare-sparse-revision");
		value.Host->dispatch(core::select_epoch_occurrence_intent{
			.Side            = core::comparison_side::left,
			.Occurrence      = occurrence,
			.ExpectedCatalog = catalog,
		});
		drain(value);
		expect(std::ranges::count(value.State.Calls, "cache.prepare-sparse-revision") == imports,
			   "a stale catalog gesture cannot acquire or rebind an older object");
	}

	void selected_large_occurrence_switch_drops_late_import()
	{
		fixture value;
		value.State.SupportsSparse       = true;
		value.State.SparseObjects        = true;
		value.State.OccurrencesSupported = true;
		value.Clock.set_next_utc(1'789'000'000'500LL);
		auto handoff                 = make_handoff(value.State, 1U, 20U, true);
		handoff.Shape                = application::accepted_pull_request_shape{ 101U, 1U, 1U };
		handoff.ForcePushes          = { force_push(101U, 21U) };
		handoff.TimelineAvailability = application::epoch_timeline_availability::available;
		value.Host->accept(std::move(handoff));
		drain(value);
		expect(value.State.Occurrences.Occurrences.size() == 1U
				   && !value.State.Occurrences.Occurrences.front().LocalEpoch,
			   "large review has one unimported event before a stale switch");
		value.Host->dispatch(core::select_epoch_occurrence_intent{
			.Side            = core::comparison_side::left,
			.Occurrence      = value.State.Occurrences.Occurrences.front().Id,
			.ExpectedCatalog = *value.Host->state().Catalog,
		});
		value.Host->invalidate(matching_invalidation());
		drain(value);
		expect(!value.State.Occurrences.Occurrences.front().LocalEpoch
				   && std::ranges::count(value.State.Calls, "cache.prepare-sparse-revision") == 1
				   && !value.Host->state().AcceptedComparison,
			   "switching away before the effect runs cancels the targeted object read and cannot publish its selection");
	}

	void selected_large_occurrence_rebases_after_superseded_commit(const bool aRepeatOccurrence = false)
	{
		fixture value;
		value.State.SupportsSparse       = true;
		value.State.SparseObjects        = true;
		value.State.OccurrencesSupported = true;
		value.Clock.set_next_utc(1'789'000'000'500LL);
		auto handoff                 = make_handoff(value.State, 1U, 20U, true);
		handoff.Shape                = application::accepted_pull_request_shape{ 101U, 1U, 1U };
		handoff.ForcePushes          = { force_push(101U, 21U), force_push(102U, 22U) };
		handoff.TimelineAvailability = application::epoch_timeline_availability::available;
		value.Host->accept(std::move(handoff));
		drain(value);
		expect(value.State.Occurrences.Occurrences.size() == 2U
				   && !value.State.Occurrences.Occurrences.front().LocalEpoch
				   && !value.State.Occurrences.Occurrences.back().LocalEpoch,
			   "large review keeps both older events unimported");
		const auto observeStagedCommit = [&value] {
			expect(value.State.Operation.has_value(), "targeted import has a durable operation");
			value.State.RecoveryInspections.emplace_back(recovery_observation(
				*value.State.Operation, application::git_recovery_ref_topology::staging,
				application::git_recovery_match::exact, application::git_recovery_match::absent));
		};
		value.State.AfterCommitAuthorized = observeStagedCommit;
		const auto first                  = value.State.Occurrences.Occurrences.front().Id;
		const auto second                 = aRepeatOccurrence ? first : value.State.Occurrences.Occurrences.back().Id;
		const auto catalog                = *value.Host->state().Catalog;
		value.Host->dispatch(core::select_epoch_occurrence_intent{
			.Side = core::comparison_side::left, .Occurrence = first, .ExpectedCatalog = catalog });
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		expect(value.State.Occurrences.Occurrences.front().LocalEpoch
				   && value.Host->state().Catalog == catalog && value.Decisions.pending() > 0U,
			   "first historical import commits durably while its decision is held");
		// The commit hook fires once; a superseding import authorizes its own commit.
		value.State.AfterCommitAuthorized = observeStagedCommit;
		value.Host->dispatch(core::select_epoch_occurrence_intent{
			.Side = core::comparison_side::left, .Occurrence = second, .ExpectedCatalog = catalog });
		const auto updatesBeforeCommitDecision = value.State.Updates.size();
		value.Decisions.run_next();
		const auto firstPublished = std::ranges::find_if(value.State.Updates.begin() + static_cast<std::ptrdiff_t>(updatesBeforeCommitDecision),
														 value.State.Updates.end(), [](const auto& aUpdate) {
															 return aUpdate.Catalog && aUpdate.Catalog->OccurrenceHistory.has_value();
														 });
		expect(value.Host->state().Catalog == value.State.Snapshot.Binding.Catalog
				   && firstPublished != value.State.Updates.end()
				   && firstPublished->Catalog->Catalog == value.Host->state().Catalog,
			   "superseded commit publishes only the catalog core has accepted");
		drain(value);
		const auto linked = std::ranges::find(value.State.Occurrences.Occurrences, second,
											  &core::epoch_occurrence::Id);
		expect(linked != value.State.Occurrences.Occurrences.end() && linked->LocalEpoch
				   && value.Host->state().Catalog == value.State.Snapshot.Binding.Catalog
				   && value.Host->state().AcceptedComparison
				   && value.Host->state().AcceptedComparison->Left.Commit == linked->Evidence.Head,
			   "superseded durable commit rebases the latest selection and keeps core/catalog synchronized");
		if (aRepeatOccurrence) {
			expect(std::ranges::count(value.State.Calls, "cache.prepare-sparse-revision") == 2,
				   "repeated selection reuses the durably imported occurrence without another object fetch");
		}
	}

	void selected_large_occurrence_reconciles_cancel_after_commit_boundary()
	{
		fixture value;
		value.State.SupportsSparse       = true;
		value.State.SparseObjects        = true;
		value.State.OccurrencesSupported = true;
		value.Clock.set_next_utc(1'789'000'000'500LL);
		auto handoff                 = make_handoff(value.State, 1U, 20U, true);
		handoff.Shape                = application::accepted_pull_request_shape{ 101U, 1U, 1U };
		handoff.ForcePushes          = { force_push(101U, 21U), force_push(102U, 22U) };
		handoff.TimelineAvailability = application::epoch_timeline_availability::available;
		value.Host->accept(std::move(handoff));
		drain(value);
		const auto first               = value.State.Occurrences.Occurrences.front().Id;
		const auto second              = value.State.Occurrences.Occurrences.back().Id;
		const auto catalog             = *value.Host->state().Catalog;
		const auto observeStagedCommit = [&value] {
			expect(value.State.Operation.has_value(), "historical append has a durable operation");
			value.State.RecoveryInspections.emplace_back(recovery_observation(
				*value.State.Operation, application::git_recovery_ref_topology::staging,
				application::git_recovery_match::exact, application::git_recovery_match::absent));
		};
		value.State.AfterCommitAuthorized = observeStagedCommit;
		std::mutex              gate;
		std::condition_variable changed;
		bool                    reachedCommitBoundary = false;
		bool                    releaseWorker         = false;
		value.State.AfterRefCommitInProgress          = [&] {
			std::unique_lock lock{ gate };
			reachedCommitBoundary = true;
			changed.notify_one();
			changed.wait(lock, [&] { return releaseWorker; });
		};
		value.Host->dispatch(core::select_epoch_occurrence_intent{
			.Side = core::comparison_side::left, .Occurrence = first, .ExpectedCatalog = catalog });
		value.Effects.wait_for_pending();
		std::thread worker{ [&value] { value.Effects.run_next(); } };
		{
			std::unique_lock lock{ gate };
			changed.wait(lock, [&] { return reachedCommitBoundary; });
		}
		value.Host->dispatch(core::select_epoch_occurrence_intent{
			.Side = core::comparison_side::left, .Occurrence = second, .ExpectedCatalog = catalog });
		{
			std::scoped_lock lock{ gate };
			releaseWorker = true;
			changed.notify_all();
		}
		worker.join();
		// The commit hook fires once; the later selection authorizes its own commit.
		value.State.AfterCommitAuthorized = observeStagedCommit;
		drain(value);
		const auto linked = std::ranges::find(value.State.Occurrences.Occurrences, second,
											  &core::epoch_occurrence::Id);
		expect(linked != value.State.Occurrences.Occurrences.end() && linked->LocalEpoch
				   && value.Host->state().Catalog == value.State.Snapshot.Binding.Catalog
				   && value.Host->state().AcceptedComparison
				   && value.Host->state().AcceptedComparison->Left.Commit == linked->Evidence.Head,
			   "post-commit cancellation still reconciles catalog and honors the later exact selection");
	}

	[[nodiscard]] auto resolve_selected_diff(
		fixture&                                            aValue,
		const application::workbench_selected_diff_request& aRequest,
		const std::stop_token                               aStop = { })
		-> application::workbench_selected_diff_observation
	{
		auto observation
			= std::optional<application::workbench_selected_diff_observation>{ };
		auto worker = std::jthread{ [&] {
			observation = aValue.Host->resolve(aRequest, aStop);
		} };
		aValue.Effects.wait_for_pending();
		aValue.Effects.run_next();
		worker.join();
		expect(observation.has_value(), "selected diff returns one terminal");
		return std::move(*observation);
	}

	void select_reverse_comparison(fixture& aValue)
	{
		const auto pair    = *aValue.Host->state().AcceptedComparison;
		const auto catalog = *aValue.Host->state().Catalog;
		aValue.Host->dispatch(core::select_comparison_anchors_intent{
			.Left                    = pair.Right.Identity,
			.Right                   = pair.Left.Identity,
			.ExpectedCatalogRevision = catalog.Revision,
		});
		run_effect_completion(aValue);
		expect(
			aValue.Host->state().AcceptedComparison.has_value()
				&& aValue.Host->state().AcceptedComparison->Left == pair.Right
				&& aValue.Host->state().AcceptedComparison->Right == pair.Left,
			"the public explicit-anchor intent accepts the reverse pair");
	}

	void selected_diff_resolution_reuses_and_revalidates_the_epoch_cache_actor()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto request     = selected_diff_request_for(value);
		const auto observation = resolve_selected_diff(value, request);
		expect(
			observation.Status
					== application::workbench_selected_diff_status::available
				&& observation.LeftBlob
					== std::optional{ oid<core::git_blob_oid>(11U) }
				&& observation.RightBlob
					== std::optional{ oid<core::git_blob_oid>(21U) }
				&& observation.LeftPath
					== std::optional{ std::filesystem::path{ "file.cpp" } }
				&& observation.RightPath
					== std::optional{ std::filesystem::path{ "file.cpp" } }
				&& observation.LeftEndpoint.has_value()
				&& application::valid(
					*observation.LeftEndpoint,
					request,
					core::diff_side::left,
					*observation.LeftBlob,
					*observation.LeftPath)
				&& observation.RightEndpoint.has_value()
				&& application::valid(
					*observation.RightEndpoint,
					request,
					core::diff_side::right,
					*observation.RightBlob,
					*observation.RightPath)
				&& value.State.TreeEntryRequests
					== std::vector{
						application::git_cache_tree_entry_request{
							.Commit = request.Diff.Comparison.Left.Commit,
							.Path   = "file.cpp",
						},
						application::git_cache_tree_entry_request{
							.Commit = request.Diff.Comparison.Right.Commit,
							.Path   = "file.cpp",
						},
					}
				&& value.State.CacheOpenRequests.size() == 1U && value.State.MaximumActiveCacheSessions == 1U && value.State.ActiveCacheSessions == 1U,
			"selected path lookup serializes through the retained exact cache session");

		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back();
		auto missing            = request;
		missing.Context.Request = core::request_id{ 802U };
		expect(
			resolve_selected_diff(value, missing).Status
				== application::workbench_selected_diff_status::both_missing,
			"two required absent sides remain a distinct typed terminal");

		auto cancelledSource = std::stop_source{ };
		static_cast<void>(cancelledSource.request_stop());
		auto cancelled            = request;
		cancelled.Context.Request = core::request_id{ 803U };
		expect(
			resolve_selected_diff(value, cancelled, cancelledSource.get_token()).Status
				== application::workbench_selected_diff_status::cancelled,
			"caller cancellation remains typed through queued actor work");

		auto mismatched                        = request;
		mismatched.Context.Request             = core::request_id{ 804U };
		mismatched.WorkbenchAuthority.Snapshot = core::provider_snapshot_id{ 2U };
		expect(
			value.Host->resolve(mismatched, { }).Status
					== application::workbench_selected_diff_status::stale
				&& value.State.TreeEntryRequests.size() == 4U,
			"snapshot mismatch is stale before cache admission");

		value.Host->invalidate(matching_invalidation());
		expect(
			value.Host->resolve(request, { }).Status
					== application::workbench_selected_diff_status::stale
				&& value.State.CacheOpenRequests.size() == 1U,
			"matching invalidation revokes selected-diff publication authority");
	}

	void modified_file_requires_both_selected_endpoints()
	{
		for (const auto change : { application::workbench_file_change_kind::modified,
								   application::workbench_file_change_kind::type_changed,
								   application::workbench_file_change_kind::unchanged }) {
			for (const auto missingLeft : { true, false }) {
				fixture value;
				establish_ready_comparison(value);
				auto request        = selected_diff_request_for(value);
				request.File.Change = change;
				const auto blob     = std::optional{ oid<core::git_blob_oid>(71U) };
				value.State.TreeEntryResults.emplace_back(missingLeft ? std::nullopt : blob);
				value.State.TreeEntryResults.emplace_back(missingLeft ? blob : std::nullopt);
				const auto observation = resolve_selected_diff(value, request);
				expect(observation.Status == (missingLeft ? application::workbench_selected_diff_status::missing_left : application::workbench_selected_diff_status::missing_right)
						   && !observation.LeftEndpoint && !observation.RightEndpoint
						   && !observation.LeftBlob && !observation.RightBlob,
					   "modified material cannot mint a successful one-sided comparison when the selected tree lacks a required path");
				expect(value.State.TreeEntryRequests.size() == 2U
						   && value.State.TreeEntryRequests[0].Commit == request.Diff.Comparison.Left.Commit
						   && value.State.TreeEntryRequests[1].Commit == request.Diff.Comparison.Right.Commit,
					   "unavailable preserves exact selected anchors and makes no fallback history lookup");
			}
		}
	}

	void zero_length_modified_endpoint_is_present_material()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto request   = selected_diff_request_for(value);
		const auto emptyBlob = core::parse_git_blob_oid("e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");
		value.State.TreeEntryResults.emplace_back(emptyBlob);
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(71U));
		const auto emptySource = resolve_selected_diff(value, request);
		expect(emptySource.Status == application::workbench_selected_diff_status::available
				   && emptySource.LeftBlob == emptyBlob && emptySource.LeftEndpoint,
			   "a present zero-byte Git blob is real material, not an absent tree entry");
	}

	// [[BPR-0654]]. The overview's changed-since-review set: one lookup per
	// path per side through the retained cache session, exactly the paths
	// whose blob differs (an absent side counts as a difference), delivered
	// on the decision lane; refused before admission for a right commit that
	// is not the accepted head, for another snapshot's authority, and once
	// the host is closing.
	void changed_paths_resolve_beside_the_selected_comparison()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto authority = workbench_authority_for(value.Host->state());
		const auto head      = value.Host->state().AcceptedRevision->Head;
		const auto reviewed  = oid<core::git_commit_oid>(0x42U);
		const auto same      = oid<core::git_blob_oid>(0x61U);
		value.State.TreeEntryResults.emplace_back(same);
		value.State.TreeEntryResults.emplace_back(same);
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x62U));
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x63U));
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x64U));
		auto       observations = std::vector<application::changed_paths_observation>{ };
		const auto record       = [&observations](const application::changed_paths_observation& aObservation) {
			observations.push_back(aObservation);
		};
		const auto request = application::changed_paths_request{
			.WorkbenchAuthority = authority,
			.Left               = reviewed,
			.Right              = head,
			.Paths              = { "a.cpp", "b.cpp", "c.cpp" },
		};
		const auto lookupsBefore = value.State.TreeEntryRequests.size();
		expect(value.Host->resolve_changed_paths(request, record) && observations.empty(),
			   "an exact request for the accepted head is admitted and answers later");
		drain(value);
		expect(observations.size() == 1U
				   && observations.front().Status == application::changed_paths_status::resolved
				   && observations.front().Request == request
				   && observations.front().Changed
					   == std::vector<std::filesystem::path>{ "b.cpp", "c.cpp" },
			   "exactly the paths whose blob differs are named, in the request's order");
		expect(value.State.TreeEntryRequests.size() == lookupsBefore + 6U
				   && value.State.TreeEntryRequests[lookupsBefore]
					   == application::git_cache_tree_entry_request{ .Commit = reviewed, .Path = "a.cpp" }
				   && value.State.TreeEntryRequests[lookupsBefore + 1U]
					   == application::git_cache_tree_entry_request{ .Commit = head, .Path = "a.cpp" }
				   && value.State.CacheOpenRequests.size() == 1U
				   && value.Host->state().AcceptedComparison.has_value()
				   && value.Host->outstanding_jobs() == 0U,
			   "one lookup per path per side through the one retained session, the "
			   "selected comparison untouched and the job retired");
		auto moved  = request;
		moved.Right = reviewed;
		expect(!value.Host->resolve_changed_paths(moved, record),
			   "a right commit other than the accepted head is refused before admission");
		auto other                        = request;
		other.WorkbenchAuthority.Snapshot = core::provider_snapshot_id{ 2U };
		expect(!value.Host->resolve_changed_paths(other, record),
			   "another snapshot's authority is refused before admission");
		value.Host->dispatch(core::refresh_epochs_intent{ });
		expect(value.Host->state().PendingIngestion.has_value()
				   && !value.Host->resolve_changed_paths(request, record),
			   "an ingestion in flight refuses, since the head's objects may not be retained yet");
		drain(value);
		value.Host->shutdown();
		expect(!value.Host->resolve_changed_paths(request, record) && observations.size() == 1U,
			   "a closing host admits nothing and answers nothing more");
	}

	// [[BPR-1212]] v002. Whether a comparison's left document is numbered as
	// GitHub numbers `LEFT` lines: the unique merge base of GitHub's base and
	// the accepted head, from the retained objects, holds the same blob at the
	// left path as the left commit does (or is the left commit). Otherwise the
	// answer is `differs`, or `unavailable` without a unique merge base. Same
	// admission as the changed-paths job.
	void base_numbering_compares_the_merge_base_with_the_left_side()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto authority    = workbench_authority_for(value.Host->state());
		const auto head         = value.Host->state().AcceptedRevision->Head;
		const auto baseTip      = oid<core::git_commit_oid>(0x51U);
		const auto left         = oid<core::git_commit_oid>(0x52U);
		const auto mergeBase    = oid<core::git_commit_oid>(0x53U);
		auto       observations = std::vector<application::github_base_numbering_observation>{ };
		const auto record       = [&observations](const application::github_base_numbering_observation& aObservation) {
			observations.push_back(aObservation);
		};
		const auto request = application::github_base_numbering_request{
			.WorkbenchAuthority = authority,
			.BaseTip            = baseTip,
			.Head               = head,
			.Left               = left,
			.LeftPath           = "src/old.cpp",
		};
		const auto unique = [](const core::git_commit_oid aBase) {
			return application::git_cache_comparison_base_observation{ .Status = application::git_comparison_base_status::unique, .Base = aBase };
		};

		// The merge base holds the same blob at the left path.
		value.State.ComparisonBaseResults.push_back(unique(mergeBase));
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x61U));
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x61U));
		const auto lookupsBefore = value.State.TreeEntryRequests.size();
		expect(value.Host->resolve_github_base_numbering(request, record) && observations.empty(), "an exact request for the accepted head is admitted and answers later");
		drain(value);
		expect(observations.size() == 1U && observations.back().Status == application::github_base_numbering_status::same
				   && observations.back().MergeBase == mergeBase && observations.back().Request == request,
			   "the same blob at the merge base is `same`");
		expect(value.State.ComparisonBaseRequests.size() == 1U && value.State.ComparisonBaseRequests.back().Head == head
				   && value.State.ComparisonBaseRequests.back().SelectedBase == baseTip
				   && value.State.TreeEntryRequests.size() == lookupsBefore + 2U
				   && value.State.TreeEntryRequests[lookupsBefore] == application::git_cache_tree_entry_request{ .Commit = mergeBase, .Path = "src/old.cpp" }
				   && value.State.TreeEntryRequests[lookupsBefore + 1U] == application::git_cache_tree_entry_request{ .Commit = left, .Path = "src/old.cpp" }
				   && value.Host->state().AcceptedComparison.has_value() && value.Host->outstanding_jobs() == 0U,
			   "the merge base of GitHub's base and the head, then the left path at it and at the left commit, the comparison untouched");

		// Another blob there.
		value.State.ComparisonBaseResults.push_back(unique(mergeBase));
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x61U));
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x62U));
		expect(value.Host->resolve_github_base_numbering(request, record), "asked again");
		drain(value);
		expect(observations.size() == 2U && observations.back().Status == application::github_base_numbering_status::differs,
			   "another blob at the merge base is `differs`");

		// No file there at all.
		value.State.ComparisonBaseResults.push_back(unique(mergeBase));
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back();
		expect(value.Host->resolve_github_base_numbering(request, record), "asked a third time");
		drain(value);
		expect(observations.size() == 3U && observations.back().Status == application::github_base_numbering_status::differs,
			   "no file at the merge base is `differs`, never `same`");

		// The merge base is the left commit itself: no lookup is needed.
		value.State.ComparisonBaseResults.push_back(unique(left));
		const auto lookups = value.State.TreeEntryRequests.size();
		expect(value.Host->resolve_github_base_numbering(request, record), "asked a fourth time");
		drain(value);
		expect(observations.size() == 4U && observations.back().Status == application::github_base_numbering_status::same
				   && value.State.TreeEntryRequests.size() == lookups,
			   "a merge base that is the left commit is `same` without a lookup");

		// No unique merge base (several, none, or objects a sparse cache lacks).
		for (const auto status : { application::git_comparison_base_status::multiple_merge_bases, application::git_comparison_base_status::no_common_ancestor,
								   application::git_comparison_base_status::objects_unavailable }) {
			value.State.ComparisonBaseResults.push_back({ .Status = status });
			expect(value.Host->resolve_github_base_numbering(request, record), "asked without a unique merge base");
			drain(value);
			expect(observations.back().Status == application::github_base_numbering_status::unavailable && !observations.back().MergeBase,
				   "without a unique merge base the answer is `unavailable`");
		}

		auto moved = request;
		moved.Head = baseTip;
		expect(!value.Host->resolve_github_base_numbering(moved, record), "a head other than the accepted head is refused before admission");
		auto other                        = request;
		other.WorkbenchAuthority.Snapshot = core::provider_snapshot_id{ 2U };
		expect(!value.Host->resolve_github_base_numbering(other, record), "another snapshot's authority is refused before admission");
		auto pathless = request;
		pathless.LeftPath.clear();
		expect(!value.Host->resolve_github_base_numbering(pathless, record), "a request without the left path is refused");
		const auto answered = observations.size();
		value.Host->shutdown();
		expect(!value.Host->resolve_github_base_numbering(request, record) && observations.size() == answered, "a closing host admits nothing and answers nothing more");
	}

	void selected_diff_resolves_each_endpoint_without_provider_direction()
	{
		fixture value;
		establish_ready_comparison(value);
		auto request         = selected_diff_request_for(value);
		request.File.Change  = application::workbench_file_change_kind::added;
		const auto leftBlob  = oid<core::git_blob_oid>(0x61U);
		const auto rightBlob = oid<core::git_blob_oid>(0x71U);
		value.State.TreeEntryResults.emplace_back(leftBlob);
		value.State.TreeEntryResults.emplace_back(rightBlob);
		const auto both = resolve_selected_diff(value, request);
		expect(
			both.Status == application::workbench_selected_diff_status::available
				&& both.LeftBlob == std::optional{ leftBlob }
				&& both.RightBlob == std::optional{ rightBlob }
				&& both.LeftPath == std::optional{ std::filesystem::path{ "file.cpp" } }
				&& both.RightPath == std::optional{ std::filesystem::path{ "file.cpp" } }
				&& both.LeftEndpoint.has_value() && both.RightEndpoint.has_value()
				&& application::valid(
					*both.LeftEndpoint,
					request,
					core::diff_side::left,
					*both.LeftBlob,
					*both.LeftPath)
				&& application::valid(
					*both.RightEndpoint,
					request,
					core::diff_side::right,
					*both.RightBlob,
					*both.RightPath),
			"a provider-added file that was modified between selected commits resolves both blobs");

		select_reverse_comparison(value);
		auto reverseAdd        = selected_diff_request_for(value, core::request_id{ 811U });
		reverseAdd.File.Change = application::workbench_file_change_kind::added;
		value.State.TreeEntryResults.emplace_back(rightBlob);
		value.State.TreeEntryResults.emplace_back();
		const auto added = resolve_selected_diff(value, reverseAdd);
		expect(
			added.Status == application::workbench_selected_diff_status::available
				&& added.LeftBlob == std::optional{ rightBlob }
				&& !added.RightBlob.has_value()
				&& added.LeftPath == std::optional{ std::filesystem::path{ "file.cpp" } }
				&& !added.RightPath.has_value() && added.LeftEndpoint.has_value()
				&& !added.RightEndpoint.has_value()
				&& application::valid(
					*added.LeftEndpoint,
					reverseAdd,
					core::diff_side::left,
					*added.LeftBlob,
					*added.LeftPath),
			"a reverse add resolves the selected left endpoint and preserves right absence");

		auto reverseDelete            = reverseAdd;
		reverseDelete.Context.Request = core::request_id{ 812U };
		reverseDelete.File.Change     = application::workbench_file_change_kind::removed;
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(leftBlob);
		const auto removed = resolve_selected_diff(value, reverseDelete);
		expect(
			removed.Status == application::workbench_selected_diff_status::available
				&& !removed.LeftBlob.has_value()
				&& removed.RightBlob == std::optional{ leftBlob }
				&& !removed.LeftPath.has_value()
				&& removed.RightPath == std::optional{ std::filesystem::path{ "file.cpp" } }
				&& !removed.LeftEndpoint.has_value()
				&& removed.RightEndpoint.has_value()
				&& application::valid(
					*removed.RightEndpoint,
					reverseDelete,
					core::diff_side::right,
					*removed.RightBlob,
					*removed.RightPath),
			"a reverse delete preserves selected left absence and resolves the right endpoint");

		auto copied                 = reverseAdd;
		copied.Context.Request      = core::request_id{ 813U };
		copied.File.Change          = application::workbench_file_change_kind::copied;
		copied.File.PreviousPath    = "source-file.cpp";
		const auto copyLookupOffset = value.State.TreeEntryRequests.size();
		value.State.TreeEntryResults.emplace_back(rightBlob);
		value.State.TreeEntryResults.emplace_back();
		const auto copiedResult = resolve_selected_diff(value, copied);
		expect(
			copiedResult.Status == application::workbench_selected_diff_status::available
				&& copiedResult.LeftBlob == std::optional{ rightBlob }
				&& !copiedResult.RightBlob.has_value()
				&& copiedResult.LeftPath
					== std::optional{ std::filesystem::path{ "file.cpp" } }
				&& !copiedResult.RightPath.has_value()
				&& copiedResult.LeftEndpoint.has_value()
				&& !copiedResult.RightEndpoint.has_value()
				&& application::valid(
					*copiedResult.LeftEndpoint,
					copied,
					core::diff_side::left,
					*copiedResult.LeftBlob,
					*copiedResult.LeftPath)
				&& value.State.TreeEntryRequests.size() == copyLookupOffset + 2U
				&& value.State.TreeEntryRequests[copyLookupOffset].Path == "file.cpp"
				&& value.State.TreeEntryRequests[copyLookupOffset + 1U].Path
					== "file.cpp",
			"a copied file resolves only its current path and preserves endpoint absence");
		expect(
			std::ranges::all_of(
				value.State.TreeEntryRequests,
				[](const application::git_cache_tree_entry_request& aLookup) {
					return aLookup.Path == "file.cpp";
				}),
			"add and delete facts never substitute a provider base-to-head path");
	}

	void selected_diff_rename_resolution_is_directional_and_ambiguous_closed()
	{
		fixture value;
		establish_ready_comparison(value);
		auto request              = selected_diff_request_for(value);
		request.File.Change       = application::workbench_file_change_kind::renamed;
		request.File.PreviousPath = "old-file.cpp";
		const auto oldBlob        = oid<core::git_blob_oid>(0x62U);
		const auto newBlob        = oid<core::git_blob_oid>(0x72U);
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(oldBlob);
		value.State.TreeEntryResults.emplace_back(newBlob);
		value.State.TreeEntryResults.emplace_back();
		const auto forward = resolve_selected_diff(value, request);
		expect(
			forward.Status == application::workbench_selected_diff_status::available
				&& forward.LeftBlob == std::optional{ oldBlob }
				&& forward.RightBlob == std::optional{ newBlob }
				&& forward.LeftPath
					== std::optional{ std::filesystem::path{ "old-file.cpp" } }
				&& forward.RightPath
					== std::optional{ std::filesystem::path{ "file.cpp" } }
				&& forward.LeftEndpoint.has_value()
				&& forward.RightEndpoint.has_value()
				&& application::valid(
					*forward.LeftEndpoint,
					request,
					core::diff_side::left,
					*forward.LeftBlob,
					*forward.LeftPath)
				&& application::valid(
					*forward.RightEndpoint,
					request,
					core::diff_side::right,
					*forward.RightBlob,
					*forward.RightPath),
			"a forward rename resolves the exact previous and current endpoint candidates");

		select_reverse_comparison(value);
		auto reverse = selected_diff_request_for(value, core::request_id{ 821U });
		reverse.File = request.File;
		value.State.TreeEntryResults.emplace_back(newBlob);
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(oldBlob);
		const auto reversed = resolve_selected_diff(value, reverse);
		expect(
			reversed.Status == application::workbench_selected_diff_status::available
				&& reversed.LeftBlob == std::optional{ newBlob }
				&& reversed.RightBlob == std::optional{ oldBlob }
				&& reversed.LeftPath
					== std::optional{ std::filesystem::path{ "file.cpp" } }
				&& reversed.RightPath
					== std::optional{ std::filesystem::path{ "old-file.cpp" } }
				&& reversed.LeftEndpoint.has_value()
				&& reversed.RightEndpoint.has_value()
				&& application::valid(
					*reversed.LeftEndpoint,
					reverse,
					core::diff_side::left,
					*reversed.LeftBlob,
					*reversed.LeftPath)
				&& application::valid(
					*reversed.RightEndpoint,
					reverse,
					core::diff_side::right,
					*reversed.RightBlob,
					*reversed.RightPath),
			"a reverse rename independently resolves each selected endpoint");

		auto ambiguous            = reverse;
		ambiguous.Context.Request = core::request_id{ 822U };
		value.State.TreeEntryResults.emplace_back(newBlob);
		value.State.TreeEntryResults.emplace_back(oldBlob);
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(oldBlob);
		const auto refused = resolve_selected_diff(value, ambiguous);
		expect(
			refused.Status
					== application::workbench_selected_diff_status::ambiguous_left
				&& !refused.LeftBlob.has_value() && !refused.RightBlob.has_value(),
			"two rename candidates at one endpoint fail closed without guessing");
	}

	void selected_diff_multi_hop_rename_uses_the_exact_selected_pair()
	{
		fixture value;
		establish_ready_comparison(value);
		auto request                  = selected_diff_request_for(value);
		request.File.PresentationPath = "c.cpp";
		request.File.CurrentPath      = "c.cpp";
		request.File.PreviousPath     = "a.cpp";
		request.File.Change           = application::workbench_file_change_kind::renamed;
		const auto intermediateBlob   = oid<core::git_blob_oid>(0x63U);
		const auto currentBlob        = oid<core::git_blob_oid>(0x73U);

		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(currentBlob);
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(intermediateBlob);
		value.State.TreeEntryResults.emplace_back(currentBlob);
		auto forwardPairs        = application::git_rename_pair_observation{ };
		forwardPairs.Records[0U] = {
			.LeftPath  = "b.cpp",
			.RightPath = "c.cpp",
		};
		forwardPairs.Count = 1U;
		value.State.RenamePairResults.emplace_back(forwardPairs);
		const auto forward = resolve_selected_diff(value, request);
		expect(
			forward.Status == application::workbench_selected_diff_status::available
				&& forward.LeftBlob == std::optional{ intermediateBlob }
				&& forward.RightBlob == std::optional{ currentBlob }
				&& forward.LeftPath
					== std::optional{ std::filesystem::path{ "b.cpp" } }
				&& forward.RightPath
					== std::optional{ std::filesystem::path{ "c.cpp" } }
				&& value.State.RenamePairRequests.size() == 1U
				&& value.State.RenamePairRequests.back()
					== application::git_cache_rename_pair_request{
						.Left         = request.Diff.Comparison.Left.Commit,
						.Right        = request.Diff.Comparison.Right.Commit,
						.CurrentPath  = "c.cpp",
						.PreviousPath = "a.cpp",
					},
			"H1 b.cpp to H2 c.cpp resolves through the retained selected-pair query");
		expect(
			value.State.TreeEntryRequests[value.State.TreeEntryRequests.size() - 2U].Path
					== "b.cpp"
				&& value.State.TreeEntryRequests.back().Path == "c.cpp",
			"the selected-pair record is re-resolved to exact blobs before publication");

		select_reverse_comparison(value);
		auto reverse            = selected_diff_request_for(value, core::request_id{ 831U });
		reverse.File            = request.File;
		const auto reverseLeft  = reverse.Diff.Comparison.Left.Commit;
		const auto reverseRight = reverse.Diff.Comparison.Right.Commit;
		value.State.TreeEntryResults.emplace_back(currentBlob);
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back();
		value.State.TreeEntryResults.emplace_back(currentBlob);
		value.State.TreeEntryResults.emplace_back(intermediateBlob);
		auto reversePairs        = application::git_rename_pair_observation{ };
		reversePairs.Records[0U] = {
			.LeftPath  = "c.cpp",
			.RightPath = "b.cpp",
		};
		reversePairs.Count = 1U;
		value.State.RenamePairResults.emplace_back(reversePairs);
		const auto reversed = resolve_selected_diff(value, reverse);
		expect(
			reversed.Status == application::workbench_selected_diff_status::available
				&& reversed.LeftBlob == std::optional{ currentBlob }
				&& reversed.RightBlob == std::optional{ intermediateBlob }
				&& reversed.LeftPath
					== std::optional{ std::filesystem::path{ "c.cpp" } }
				&& reversed.RightPath
					== std::optional{ std::filesystem::path{ "b.cpp" } }
				&& value.State.RenamePairRequests.back().Left == reverseLeft
				&& value.State.RenamePairRequests.back().Right == reverseRight,
			"H2 c.cpp to H1 b.cpp resolves with the exact reversed commit order");

		auto multiple            = reverse;
		multiple.Context.Request = core::request_id{ 832U };
		for (auto index = 0U; index < 4U; ++index) {
			value.State.TreeEntryResults.emplace_back();
		}
		auto multiplePairs        = application::git_rename_pair_observation{ };
		multiplePairs.Records[0U] = {
			.LeftPath  = "c.cpp",
			.RightPath = "b.cpp",
		};
		multiplePairs.Records[1U] = {
			.LeftPath  = "c.cpp",
			.RightPath = "other.cpp",
		};
		multiplePairs.Count = 2U;
		value.State.RenamePairResults.emplace_back(multiplePairs);
		expect(
			resolve_selected_diff(value, multiple).Status
				== application::workbench_selected_diff_status::ambiguous_both,
			"multiple causally bound rename records fail closed as typed ambiguity");

		auto unbound            = reverse;
		unbound.Context.Request = core::request_id{ 833U };
		for (auto index = 0U; index < 4U; ++index) {
			value.State.TreeEntryResults.emplace_back();
		}
		auto unboundPairs        = application::git_rename_pair_observation{ };
		unboundPairs.Records[0U] = {
			.LeftPath  = "x.cpp",
			.RightPath = "y.cpp",
		};
		unboundPairs.Count = 1U;
		value.State.RenamePairResults.emplace_back(unboundPairs);
		expect(
			resolve_selected_diff(value, unbound).Status
				== application::workbench_selected_diff_status::ambiguous_both,
			"a rename record unbound to accepted lineage cannot invent an intermediate path");

		auto malformed            = reverse;
		malformed.Context.Request = core::request_id{ 834U };
		for (auto index = 0U; index < 4U; ++index) {
			value.State.TreeEntryResults.emplace_back();
		}
		value.State.RenamePairResults.emplace_back(std::unexpected{
			application::git_cache_failure::process_failure });
		expect(
			resolve_selected_diff(value, malformed).Status
				== application::workbench_selected_diff_status::failure,
			"malformed selected-pair Git output remains a typed resolution failure");
	}

	/// \brief BPR-0532. The second selected-diff authorization, proved three
	/// ways.
	///
	/// \details A continued agent turn asks about a pair the review has moved
	/// past, so the host has to answer for two comparisons at once without
	/// letting either become a hole. Served: the context the host mints for
	/// one kept binding resolves through the same port, and the reviewer's own
	/// selection keeps resolving beside it. Scoped: what it will mint for is
	/// bounded to this review's own host, repository, and pull request.
	/// Minted: a caller that assembles the same retargeting itself is stale,
	/// because the warrant is the host's decision to serve the pair and not
	/// the shape of the request that names it.
	void realignment_context_is_minted_scoped_and_never_assumed()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto authority = workbench_authority_for(value.Host->state());
		auto       request   = selected_diff_request_for(value);
		const auto onScreen  = request.Diff;
		// The binding a kept agent session carries: this review's own host,
		// repository, and pull request, and a pair that is not the one on
		// screen.
		const auto kept = core::agent_revision_binding{
			.Host        = authority.Host,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.BaseOid     = oid<core::git_commit_oid>(0x31U),
			.HeadOid     = oid<core::git_commit_oid>(0x32U),
			.LeftEpoch   = core::epoch_ordinal{ 4U },
			.RightEpoch  = core::epoch_ordinal{ 5U },
		};
		const auto minted = value.Host->realignment_context(authority, kept);
		expect(
			minted.has_value()
				&& minted->Comparison.Left.Identity.Epoch == kept.LeftEpoch
				&& minted->Comparison.Left.Commit == kept.BaseOid
				&& minted->Comparison.Right.Identity.Epoch == kept.RightEpoch
				&& minted->Comparison.Right.Commit == kept.HeadOid
				&& minted->Authority == onScreen.Authority
				&& minted->OptionsDigest == onScreen.OptionsDigest
				&& minted->Algorithm == onScreen.Algorithm
				&& minted->Comparison != onScreen.Comparison,
			"the minted context moves both anchors to the kept pair and keeps the review's own authority");

		auto realigned            = request;
		realigned.Context.Request = core::request_id{ 811U };
		realigned.Diff            = *minted;
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x62U));
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x72U));
		expect(
			resolve_selected_diff(value, realigned).Status
				== application::workbench_selected_diff_status::available,
			"the kept pair resolves through the port the workbench resolves through");

		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x63U));
		value.State.TreeEntryResults.emplace_back(oid<core::git_blob_oid>(0x73U));
		auto current            = request;
		current.Context.Request = core::request_id{ 812U };
		expect(
			resolve_selected_diff(value, current).Status
				== application::workbench_selected_diff_status::available,
			"the re-alignment is authorized beside the reviewer's own selection rather than over it");

		auto foreign        = kept;
		foreign.PullRequest = core::pull_request_id{ 72U };
		expect(
			!value.Host->realignment_context(authority, foreign).has_value(),
			"a pair of another pull request is not this review's to answer for");

		auto forged                                = realigned;
		forged.Context.Request                     = core::request_id{ 813U };
		forged.Diff.Comparison.Left.Identity.Epoch = core::epoch_ordinal{ 6U };
		expect(
			value.Host->resolve(forged, { }).Status
				== application::workbench_selected_diff_status::stale,
			"a comparison the host never minted is stale before any cache work");
	}

	void invalidated_authority_cannot_materialize_retained_cache()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto request = blob_request_for(value.Host->state());

		value.Host->invalidate(matching_invalidation());
		const auto effectsBefore = value.Effects.pending();
		const auto jobsBefore    = value.Host->outstanding_jobs();
		const auto blobsBefore   = value.State.BlobRequests.size();
		const auto rejected      = value.Host->materialize(request, { });

		expect(
			rejected.Status == application::diff_blob_status::failure
				&& value.State.BlobRequests.size() == blobsBefore
				&& value.Effects.pending() == effectsBefore
				&& value.Host->outstanding_jobs() == jobsBefore,
			"matching invalidation synchronously revokes blob authorization even "
			"while the repository actor retains its cache session");

		value.Host->shutdown();
		drain(value);
		expect(
			value.Host->is_quiescent(),
			"the invalidated retained-cache rejection remains quiescent");
	}

	void agent_workspace_failures_preserve_the_cache_diagnostic()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto diagnostic = std::string{
			"fatal: could not materialize /tmp/bpr-agent-workspace from "
			"<redacted-url>\n"
		};

		value.State.WorkspaceMaterializationFailure = application::git_cache_error{
			application::git_cache_failure::process_failure,
			diagnostic,
		};
		auto materialized      = std::optional<std::expected<
			application::owned_agent_workspace,
			application::agent_workspace_error>>{ };
		auto materializeCaller = std::jthread{ [&] {
			materialized = value.Host->materialize_agent_workspace(
				workspace_request_for(value.Host->state()),
				{ });
		} };
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		materializeCaller.join();
		expect(
			materialized.has_value() && !materialized->has_value()
				&& materialized->error()
					== application::agent_workspace_failure::unavailable
				&& materialized->error().Detail == diagnostic,
			"workspace materialization maps the code and preserves cache detail exactly");

		value.State.WorkspaceCleanupFailure = application::git_cache_error{
			application::git_cache_failure::process_failure,
			diagnostic,
		};
		auto cleaned = std::optional<
			std::expected<void, application::agent_workspace_error>>{ };
		auto cleanupCaller = std::jthread{ [&] {
			cleaned = value.Host->cleanup_agent_workspace(
				owned_workspace_for(value.Host->state()),
				{ });
		} };
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		cleanupCaller.join();
		expect(
			cleaned.has_value() && !cleaned->has_value()
				&& cleaned->error()
					== application::agent_workspace_failure::cleanup_failed
				&& cleaned->error().Detail == diagnostic,
			"workspace cleanup maps the code and preserves cache detail exactly");
	}

	void invalidated_inflight_blob_cannot_publish_bytes()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto              request = blob_request_for(value.Host->state());
		std::mutex              gate;
		std::condition_variable changed;
		auto                    entered            = false;
		auto                    release            = false;
		value.State.BlobMaterializationIgnoresStop = true;
		value.State.DuringBlobMaterialization      = [&](const std::stop_token) {
			std::unique_lock lock{ gate };
			entered = true;
			changed.notify_one();
			changed.wait(lock, [&] {
				return release;
			});
		};

		auto observation = std::optional<application::diff_blob_observation>{ };
		auto caller      = std::jthread{ [&] {
			observation = value.Host->materialize(request, { });
		} };
		value.Effects.wait_for_pending();
		auto effect = std::jthread{ [&] {
			value.Effects.run_next();
		} };
		{
			std::unique_lock lock{ gate };
			changed.wait(lock, [&] {
				return entered;
			});
		}

		value.Host->invalidate(matching_invalidation());
		{
			const std::lock_guard lock{ gate };
			release = true;
		}
		changed.notify_one();
		effect.join();
		caller.join();

		expect(
			observation
				&& observation->Status != application::diff_blob_status::available
				&& observation->Bytes.empty()
				&& value.State.BlobRequests.size() == 1U,
			"post-read authority and stop checks discard bytes returned by a cache "
			"that ignores advisory cancellation after matching invalidation");

		value.Host->shutdown();
		drain(value);
		expect(
			value.Host->is_quiescent(),
			"the revoked in-flight blob path retires its actor reservation");
	}

	void cancellation_before_ponr_aborts_without_publication()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		value.Host->dispatch(core::cancel_epoch_refresh_intent{ });
		run_effect_completion(value);
		expect(
			std::ranges::find(value.State.Calls, "catalog.transition:ponr")
					== value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "cache.promote")
					== value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "catalog.finalize")
					== value.State.Calls.end()
				&& std::ranges::find(
					   value.State.Calls, "catalog.transition:aborted")
					!= value.State.Calls.end()
				&& value.Host->state().Catalog == catalog_identity(),
			"pre-PONR cancellation aborts staging and leaves catalog invisible");
	}

	void stop_immediately_after_ponr_cannot_replace_committed_terminal()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		std::mutex              gate;
		std::condition_variable changed;
		auto                    reachedPonr   = false;
		auto                    releaseWorker = false;
		auto                    stopInjected  = false;
		value.State.AfterRefCommitInProgress  = [&] {
			std::unique_lock lock{ gate };
			reachedPonr = true;
			changed.notify_one();
			changed.wait(lock, [&releaseWorker] {
				return releaseWorker;
			});
		};

		std::thread worker{ [&value] {
			value.Effects.run_next();
		} };
		{
			std::unique_lock lock{ gate };
			changed.wait(lock, [&reachedPonr] {
				return reachedPonr;
			});
		}
		value.Decisions.post([&] {
			stopInjected = true;
			value.Host->dispatch(core::cancel_epoch_refresh_intent{ });
		});
		value.Decisions.run_next();
		{
			const std::lock_guard lock{ gate };
			releaseWorker = true;
		}
		changed.notify_one();
		worker.join();
		expect(
			value.Decisions.pending() == 1U,
			"the released post-PONR worker queues one serialized terminal");
		value.Decisions.run_next();
		drain(value);

		const auto committedTerminals = std::ranges::count_if(
			value.State.Updates,
			[](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Catalog
					&& aUpdate.Catalog->Catalog == catalog_identity(1)
					&& std::ranges::any_of(aUpdate.Catalog->Changes,
										   [](const application::epoch_row_change_projection& aChange) {
											   return aChange.Kind
												   == application::epoch_row_change_kind::insert;
										   });
			});
		expect(
			stopInjected
				&& std::ranges::find(value.State.Calls, "cache.promote")
					!= value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "catalog.finalize")
					!= value.State.Calls.end()
				&& value.Host->state().Catalog == catalog_identity(1)
				&& committedTerminals == 1
				&& value.State.ProjectionOutsideDecision == 0U,
			"a stop requested after durable PONR still promotes, finalizes, and "
			"publishes one initial newest-row terminal");
	}

	void applied_commit_authorization_error_aborts_exactly_before_ponr()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		value.State.AfterCommitAuthorized = [&value] {
			expect(
				value.State.Operation.has_value(),
				"applied commit authorization leaves one durable operation");
			seed_recovery(value.State, *value.State.Operation);
			throw std::runtime_error{
				"injected applied commit-authorization error"
			};
		};

		run_effect_completion(value);

		expect(
			std::ranges::find(
				value.State.Calls, "catalog.transition:abort-requested")
					!= value.State.Calls.end()
				&& std::ranges::find(
					   value.State.Calls, "cache.abort-exact")
					!= value.State.Calls.end()
				&& std::ranges::find(
					   value.State.Calls, "catalog.transition:aborted")
					!= value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "cache.promote")
					== value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "catalog.finalize")
					== value.State.Calls.end()
				&& value.Host->state().Catalog == catalog_identity(),
			"an ambiguous commit-authorization result pages the exact durable "
			"operation and aborts while cancellation is still legal");
	}

	void applied_ponr_error_recovers_to_mandatory_success()
	{
		using match = application::git_recovery_match;
		using refs  = application::git_recovery_ref_topology;
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		value.State.AfterRefCommitInProgress = [&value] {
			expect(
				value.State.Operation.has_value(),
				"applied PONR leaves one durable operation");
			const auto durable = *value.State.Operation;
			seed_recovery(value.State, durable);
			value.State.RecoveryInspections.emplace_back(recovery_observation(
				durable, refs::staging, match::exact, match::absent));
			throw std::runtime_error{ "injected applied-PONR error" };
		};

		run_effect_completion(value);
		drain(value);

		const auto committedTerminals = std::ranges::count_if(
			value.State.Updates,
			[](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Catalog
					&& aUpdate.Catalog->Catalog == catalog_identity(1)
					&& std::ranges::any_of(aUpdate.Catalog->Changes,
										   [](const application::epoch_row_change_projection& aChange) {
											   return aChange.Kind
												   == application::epoch_row_change_kind::insert;
										   });
			});
		expect(
			std::ranges::find(value.State.Calls, "catalog.recovery-page")
					!= value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "cache.promote")
					!= value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "catalog.finalize")
					!= value.State.Calls.end()
				&& value.Host->state().Catalog == catalog_identity(1)
				&& committedTerminals == 1,
			"an applied-then-throw PONR transition resumes the exact durable "
			"operation and publishes one initial newest-row terminal");
	}

	void throwing_exact_abort_degrades_to_a_bounded_terminal()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		value.State.ThrowRecoveryAbort = true;

		value.Host->dispatch(core::cancel_epoch_refresh_intent{ });
		drain(value);

		const auto diagnosed = std::ranges::any_of(
			value.State.Updates,
			[](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Transition
					&& std::ranges::any_of(
						   aUpdate.Transition->Diagnostics,
						   [](const core::epoch_diagnostic& aDiagnostic) {
							   return aDiagnostic.Code
								   == core::epoch_reason_code::repair_required;
						   });
			});
		expect(
			diagnosed
				&& std::ranges::find(value.State.Calls, "cache.abort-exact")
					!= value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "cache.promote")
					== value.State.Calls.end()
				&& std::ranges::find(value.State.Calls, "catalog.finalize")
					== value.State.Calls.end()
				&& value.Host->state().Catalog == catalog_identity(),
			"a throwing abort port cannot terminate or publish a pre-PONR append");
	}

	void staging_ready_recovery_journals_abort_before_cleanup_and_acceptance()
	{
		fixture    value;
		const auto record = recovery_record(
			value.State,
			application::epoch_catalog_operation_phase::staging_ready);
		seed_recovery(value.State, record);

		value.Host->accept(make_handoff(value.State));
		value.Effects.run_next();
		expect(
			!value.Host->state().Authority
				&& value.State.ProjectionCalls == 0U,
			"restart recovery finishes before the offered revision reaches core");
		expect_in_order(
			value.State.Calls,
			{
				"catalog.recovery-page",
				"catalog.transition:abort-requested",
				"cache.abort-exact",
				"catalog.transition:aborted",
				"catalog.source-probe",
			});
		expect(
			value.State.Operation
				&& value.State.Operation->Phase
					== application::epoch_catalog_operation_phase::aborted
				&& value.State.Operation->RecoveryInputCompacted,
			"staging-ready recovery reaches the compacted aborted terminal");
		expect(
			value.State.RecoveryAbortRequests.size() == 1U
				&& application::valid(value.State.RecoveryAbortRequests.front())
				&& value.State.RecoveryAbortRequests.front().Operation
					== record.Context.Operation
				&& value.State.RecoveryAbortRequests.front().Scope
					== value.State.Binding.Authority.Scope
				&& value.State.RecoveryAbortRequests.front().Ordinal
					== record.ReservedOrdinal,
			"pre-PONR cleanup is bound to the exact durable operation and refs");

		value.Decisions.run_next();
		expect(
			value.Host->state().Authority == authority()
				&& value.Host->state().AcceptedRevision.has_value(),
			"the offer reaches core only after exact recovery and source probing");
	}

	void post_ponr_recovery_resumes_every_durable_phase()
	{
		using enum application::epoch_catalog_operation_phase;
		using match = application::git_recovery_match;
		using refs  = application::git_recovery_ref_topology;
		struct recovery_case
		{
			application::epoch_catalog_operation_phase Phase;
			application::git_recovery_ref_topology     Refs;
			application::git_recovery_match            Staging;
			application::git_recovery_match            Final;
			bool                                       Promotes       = false;
			bool                                       Finalizes      = false;
			bool                                       AlreadyVisible = false;
			std::string_view                           Label;
		};
		const auto cases = std::array{
			recovery_case{
				.Phase     = commit_authorized,
				.Refs      = refs::staging,
				.Staging   = match::exact,
				.Final     = match::absent,
				.Promotes  = true,
				.Finalizes = true,
				.Label     = "commit-authorized recovery resumes promotion" },
			recovery_case{
				.Phase     = ref_commit_in_progress,
				.Refs      = refs::staging,
				.Staging   = match::exact,
				.Final     = match::absent,
				.Promotes  = true,
				.Finalizes = true,
				.Label     = "in-progress staging recovery resumes promotion" },
			recovery_case{
				.Phase     = ref_commit_in_progress,
				.Refs      = refs::final,
				.Staging   = match::absent,
				.Final     = match::exact,
				.Finalizes = true,
				.Label     = "in-progress final refs resume catalog completion" },
			recovery_case{
				.Phase     = refs_committed,
				.Refs      = refs::final,
				.Staging   = match::absent,
				.Final     = match::exact,
				.Finalizes = true,
				.Label     = "refs-committed recovery resumes visibility" },
			recovery_case{
				.Phase          = visible_cleanup_pending,
				.Refs           = refs::final,
				.Staging        = match::exact,
				.Final          = match::exact,
				.AlreadyVisible = true,
				.Label          = "visible recovery removes exact staging residue" },
		};

		for (const auto& item : cases) {
			fixture    value;
			const auto record = recovery_record(value.State, item.Phase);
			if (item.AlreadyVisible) {
				value.State.Binding.Catalog          = catalog_identity(1);
				value.State.Snapshot.Binding.Catalog = catalog_identity(1);
				value.State.Snapshot.LatestLive      = record.Candidate->Epoch;
				value.State.Snapshot.NextOrdinal     = core::epoch_ordinal{ 2U };
			}
			seed_recovery(value.State, record);
			value.State.RecoveryInspections.emplace_back(recovery_observation(
				record, item.Refs, item.Staging, item.Final));
			if (!item.Promotes) {
				value.State.Retained = application::git_retained_ref_pair{
					.Scope   = value.State.Binding.Authority.Scope,
					.Ordinal = record.ReservedOrdinal,
					.Base    = record.Context.Revision.Base,
					.Head    = record.Context.Revision.Head,
				};
			}

			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);

			const auto promoted = std::ranges::count(
				value.State.Calls, "cache.promote");
			const auto finalized = std::ranges::count(
				value.State.Calls, "catalog.finalize");
			expect(
				promoted == static_cast<std::ptrdiff_t>(item.Promotes),
				item.Label);
			expect(
				finalized == static_cast<std::ptrdiff_t>(item.Finalizes),
				item.Label);
			expect(
				value.State.Operation
					&& value.State.Operation->Phase == complete
					&& value.State.Operation->RecoveryInputCompacted
					&& value.State.RecoveryCompletionRequests.size() == 1U
					&& application::valid(
						value.State.RecoveryCompletionRequests.front())
					&& value.Host->state().AcceptedRevision.has_value(),
				item.Label);
			expect_in_order(
				value.State.Calls,
				{
					"catalog.recovery-page",
					"cache.inspect-recovery",
					"cache.inspect-retained",
					"cache.complete-operation",
					"catalog.transition:cleanup",
					"catalog.source-probe",
				});
		}
	}

	void promotion_error_is_resolved_by_exact_postcondition_inspection()
	{
		using match = application::git_recovery_match;
		using refs  = application::git_recovery_ref_topology;
		fixture    value;
		const auto record = recovery_record(
			value.State,
			application::epoch_catalog_operation_phase::ref_commit_in_progress);
		seed_recovery(value.State, record);
		value.State.RecoveryInspections.emplace_back(recovery_observation(
			record, refs::staging, match::exact, match::absent));
		value.State.RecoveryInspections.emplace_back(recovery_observation(
			record, refs::final, match::absent, match::exact));
		value.State.PromoteAppliesThenFails = true;

		value.Host->accept(make_handoff(value.State));
		run_effect_completion(value);

		expect(
			std::ranges::count(value.State.Calls, "cache.inspect-recovery")
					== 2
				&& std::ranges::count(value.State.Calls, "cache.promote") == 1
				&& std::ranges::count(value.State.Calls, "catalog.finalize")
					== 1
				&& value.State.Operation
				&& value.State.Operation->Phase
					== application::epoch_catalog_operation_phase::complete
				&& value.Host->state().AcceptedRevision.has_value(),
			"applied-then-error promotion resumes from exact final postcondition");
	}

	[[nodiscard]] auto has_diagnostic(
		const shared_state&           aState,
		const core::epoch_reason_code aCode) -> bool
	{
		return std::ranges::any_of(
			aState.Updates,
			[aCode](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Transition
					&& std::ranges::any_of(
						   aUpdate.Transition->Diagnostics,
						   [aCode](const core::epoch_diagnostic& aDiagnostic) {
							   return aDiagnostic.Code == aCode;
						   });
			});
	}

	[[nodiscard]] auto has_repair_diagnostic(const shared_state& aState) -> bool
	{
		return std::ranges::any_of(
			aState.Updates,
			[](const application::epoch_presentation_update& aUpdate) {
				return aUpdate.Transition
					&& std::ranges::any_of(
						   aUpdate.Transition->Diagnostics,
						   [](const core::epoch_diagnostic& aDiagnostic) {
							   return aDiagnostic.Code
								   == core::epoch_reason_code::repair_required;
						   });
			});
	}

	void recovery_identity_and_pagination_mismatches_fail_closed()
	{
		using match = application::git_recovery_match;
		using refs  = application::git_recovery_ref_topology;
		{
			fixture    value;
			const auto record = recovery_record(
				value.State,
				application::epoch_catalog_operation_phase::commit_authorized);
			seed_recovery(value.State, record);
			auto wrong = recovery_observation(
				record, refs::staging, match::exact, match::absent);
			wrong.Operation = opaque<core::epoch_operation_id>(0x7FU);
			value.State.RecoveryInspections.emplace_back(wrong);

			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(
				!value.Host->state().Authority && has_repair_diagnostic(value.State)
					&& std::ranges::find(
						   value.State.Calls,
						   "catalog.transition:repair-required")
						!= value.State.Calls.end()
					&& std::ranges::find(
						   value.State.Calls, "catalog.source-probe")
						== value.State.Calls.end(),
				"mismatched physical operation identity is journaled for repair "
				"before acceptance");
		}

		{
			fixture value;
			value.State.RecoveryPages.push_back(
				application::epoch_recovery_page{
					.Incarnation = opaque<core::epoch_catalog_incarnation_id>(0x7EU),
				});
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(
				!value.Host->state().Authority && has_repair_diagnostic(value.State)
					&& std::ranges::find(
						   value.State.Calls, "catalog.source-probe")
						== value.State.Calls.end(),
				"mismatched recovery-page incarnation fails closed as repair");
		}

		{
			fixture    value;
			const auto first = recovery_record(
				value.State,
				application::epoch_catalog_operation_phase::staging_ready,
				0x10U);
			const auto continuation = opaque<core::epoch_operation_id>(0x20U);
			const auto wrongNext    = recovery_record(
				value.State,
				application::epoch_catalog_operation_phase::staging_ready,
				0x30U);
			value.State.Operation = first;
			value.State.RecoveryPages.push_back(
				application::epoch_recovery_page{
					.Incarnation  = value.State.Binding.Catalog.Incarnation,
					.Operations   = { first },
					.Continuation = continuation,
				});
			value.State.RecoveryPages.push_back(
				application::epoch_recovery_page{
					.Incarnation = value.State.Binding.Catalog.Incarnation,
					.Operations  = { wrongNext },
				});

			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(
				!value.Host->state().Authority && has_repair_diagnostic(value.State)
					&& value.State.RecoveryPageCursors
						== std::vector<std::optional<core::epoch_operation_id>>{
							std::nullopt,
							continuation,
						}
					&& std::ranges::find(value.State.Calls, "catalog.source-probe") == value.State.Calls.end(),
				"pagination must resume at the exact advertised operation identity");
		}
	}

	void core_authorized_prune_composes_exact_durable_cleanup()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto epoch = value.State.Snapshot.LatestLive->Id;
		value.State.Calls.clear();
		value.Host->dispatch(core::prune_review_epochs_intent{
			.Catalog = *value.Host->state().Catalog,
			.Epochs  = { epoch },
		});
		run_effect_completion(value);

		expect(value.State.BeginPruneRequests.size() == 1U
				   && value.State.BeginPruneRequests.front().Epochs.size() == 1U
				   && value.State.BeginPruneRequests.front().Epochs.front().Epoch
					   == epoch
				   && value.State.PruneRequests.size() == 1U
				   && value.State.PruneRequests.front().Expected
					   == value.State.BeginPruneRequests.front()
							  .Epochs.front()
							  .Retained
				   && value.State.PruneRequests.front().DurableIntentToken
					   == value.State.BeginPruneRequests.front().Operation.bytes()
				   && value.Host->state().Catalog == catalog_identity(2)
				   && !value.Host->state().PendingMaintenance,
			   "the host seals and executes one exact core-authorized prune batch");
		expect_in_order(value.State.Calls,
						{
							"catalog.exact-anchors",
							"catalog.begin-prune",
							"catalog.transition:ref-delete",
							"cache.prune",
							"catalog.finalize-prune",
							"catalog.transition:cleanup",
						});
		expect(std::ranges::any_of(value.State.Updates,
								   [epoch](const auto& aUpdate) {
									   return aUpdate.Catalog
										   && aUpdate.Catalog->AvailabilityChangedEpochs
										   == std::vector{ epoch }
									   && aUpdate.Catalog->AvailabilityChangedTo
										   == core::anchor_availability::pruned
										   && !aUpdate.Catalog->Replace;
								   }),
			   "prune publishes an incremental stable-ID availability projection");
	}

	void core_authorized_cache_delete_closes_session_and_projects_bulk_change()
	{
		fixture value;
		establish_ready_comparison(value);
		expect(value.State.ActiveCacheSessions == 1U,
			   "the ready fixture owns one live cache session");
		value.State.Calls.clear();
		value.Host->dispatch(core::delete_repository_cache_intent{
			.Catalog = *value.Host->state().Catalog,
			.Cache   = value.State.Binding.Authority.Cache,
		});
		run_effect_completion(value);

		expect(value.State.BeginCacheDeleteRequests.size() == 1U
				   && value.State.BeginCacheDeleteRequests.front().Impact.Scopes
					   == std::vector{ application::cache_delete_scope_fact{
						   .Authority        = value.State.Binding.Authority.Scope,
						   .ExpectedRevision = core::epoch_catalog_revision{ 1 },
					   } }
				   && value.State.DeleteRequests.size() == 2U
				   && value.State.DeleteRequests.front().DurableIntentToken
					   == value.State.BeginCacheDeleteRequests.front()
							  .Operation.bytes()
				   && value.State.DeleteRequests.back()
					   == value.State.DeleteRequests.front()
				   && value.State.ActiveCacheSessions == 0U
				   && value.Host->state().Catalog == catalog_identity(2),
			   "cache delete seals every affected scope, closes the handle, and uses "
			   "one replay-stable authorization twice");
		expect_in_order(value.State.Calls,
						{
							"catalog.cache-delete-impact",
							"catalog.begin-cache-delete",
							"catalog.transition:quarantine-rename",
							"cache.session-close:71",
							"cache.delete",
							"catalog.transition:quarantined",
							"catalog.finalize-cache-delete",
							"cache.delete",
							"catalog.transition:cleanup",
						});
		expect(std::ranges::any_of(value.State.Updates,
								   [](const auto& aUpdate) {
									   return aUpdate.Catalog
										   && aUpdate.Catalog->AllEpochAvailability
										   == core::anchor_availability::cache_deleted
										   && !aUpdate.Catalog->Replace;
								   }),
			   "cache deletion publishes one bulk incremental tombstone projection");
	}

	void maintenance_cancellation_obeys_the_durable_point_of_no_return()
	{
		{
			fixture value;
			establish_ready_comparison(value);
			value.State.Calls.clear();
			value.Host->dispatch(core::delete_repository_cache_intent{
				.Catalog = *value.Host->state().Catalog,
				.Cache   = value.State.Binding.Authority.Cache,
			});
			value.Host->dispatch(core::cancel_epoch_maintenance_intent{ });
			run_effect_completion(value);
			expect(value.State.BeginCacheDeleteRequests.empty()
					   && value.State.DeleteRequests.empty()
					   && value.Host->state().Catalog == catalog_identity(1)
					   && value.Host->state().Status == core::epoch_status::cancelled,
				   "cancellation before durable sealing performs no destructive work");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			const auto              epoch = value.State.Snapshot.LatestLive->Id;
			std::mutex              gate;
			std::condition_variable changed;
			auto                    reachedPoint  = false;
			auto                    releaseWorker = false;
			value.State.AfterRefCommitInProgress  = [&] {
				std::unique_lock lock{ gate };
				reachedPoint = true;
				changed.notify_one();
				changed.wait(lock, [&releaseWorker] { return releaseWorker; });
			};
			value.Host->dispatch(core::prune_review_epochs_intent{
				.Catalog = *value.Host->state().Catalog,
				.Epochs  = { epoch },
			});
			std::thread worker{ [&value] { value.Effects.run_next(); } };
			{
				std::unique_lock lock{ gate };
				changed.wait(lock, [&reachedPoint] { return reachedPoint; });
			}
			value.Decisions.post([&] { value.Host->dispatch(core::cancel_epoch_maintenance_intent{ }); });
			value.Decisions.run_next();
			{
				const std::lock_guard lock{ gate };
				releaseWorker = true;
			}
			changed.notify_one();
			worker.join();
			value.Decisions.run_next();
			expect(value.State.PruneRequests.size() == 1U
					   && value.Host->state().Catalog == catalog_identity(2)
					   && !value.Host->state().PendingMaintenance,
				   "post-point-of-no-return cancellation cannot replace durable prune "
				   "success");
		}
	}

	void maintenance_restart_recovery_aborts_or_completes_by_phase()
	{
		{
			fixture    value;
			const auto record = maintenance_recovery_record(value.State,
															application::epoch_catalog_operation_kind::prune,
															application::epoch_catalog_operation_phase::prune_intent);
			seed_maintenance_recovery(value.State, record);
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(std::ranges::find(value.State.Calls,
									 "catalog.transition:abort-requested")
						   != value.State.Calls.end()
					   && std::ranges::find(value.State.Calls,
											"catalog.transition:aborted")
						   != value.State.Calls.end()
					   && value.State.PruneRequests.empty()
					   && value.Host->state().Authority.has_value(),
				   "restart aborts a sealed pre-PONR maintenance intent before "
				   "authority acceptance");
		}

		{
			fixture    value;
			const auto record = maintenance_recovery_record(value.State,
															application::epoch_catalog_operation_kind::prune,
															application::epoch_catalog_operation_phase::ref_delete_in_progress);
			seed_maintenance_recovery(value.State, record);
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(value.State.PruneRequests.size() == 1U
					   && value.State.Binding.Catalog == catalog_identity(1)
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::complete
					   && value.Host->state().Authority.has_value(),
				   "restart completes an exact post-PONR prune before authority "
				   "acceptance");
		}

		{
			fixture    value;
			const auto record = maintenance_recovery_record(value.State,
															application::epoch_catalog_operation_kind::cache_delete,
															application::epoch_catalog_operation_phase::
																quarantine_rename_in_progress);
			seed_maintenance_recovery(value.State, record);
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(value.State.DeleteRequests.size() == 2U
					   && value.State.Binding.Catalog == catalog_identity(1)
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::complete
					   && value.Host->state().Authority.has_value(),
				   "restart completes quarantine, catalog tombstone, and exact "
				   "cleanup "
				   "before authority acceptance");
		}
	}

	void maintenance_restart_recovery_covers_every_durable_phase()
	{
		using enum application::epoch_catalog_operation_phase;

		for (const auto phase : { prune_intent, abort_requested }) {
			fixture value;
			seed_maintenance_recovery(value.State,
									  maintenance_recovery_record(value.State,
																  application::epoch_catalog_operation_kind::prune,
																  phase));
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(value.State.Maintenance->Phase == aborted
					   && value.State.PruneRequests.empty()
					   && value.Host->state().Authority.has_value(),
				   "every pre-PONR prune phase restarts by exact abort");
		}

		{
			fixture value;
			seed_maintenance_recovery(value.State,
									  maintenance_recovery_record(value.State,
																  application::epoch_catalog_operation_kind::prune,
																  visible_cleanup_pending));
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(value.State.Maintenance->Phase == complete
					   && value.State.PruneRequests.empty()
					   && std::ranges::find(value.State.Calls,
											"catalog.finalize-prune")
						   == value.State.Calls.end(),
				   "restart after visible prune commit performs cleanup only");
		}

		for (const auto phase : { cache_delete_building_exact_set,
								  cache_delete_intent_sealed,
								  abort_requested }) {
			fixture value;
			seed_maintenance_recovery(value.State,
									  maintenance_recovery_record(value.State,
																  application::epoch_catalog_operation_kind::cache_delete,
																  phase));
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(value.State.Maintenance->Phase == aborted
					   && value.State.DeleteRequests.empty()
					   && value.Host->state().Authority.has_value(),
				   "every pre-PONR cache-delete phase restarts by exact abort");
		}

		for (const auto [phase, expectedDeletes] : std::array{
				 std::pair{ quarantined, std::size_t{ 1U } },
				 std::pair{ catalog_tombstoned_cleanup_pending, std::size_t{ 1U } },
			 }) {
			fixture value;
			seed_maintenance_recovery(value.State,
									  maintenance_recovery_record(value.State,
																  application::epoch_catalog_operation_kind::cache_delete,
																  phase));
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(value.State.Maintenance->Phase == complete
					   && value.State.DeleteRequests.size() == expectedDeletes
					   && value.Host->state().Authority.has_value(),
				   "every post-quarantine cache-delete phase resumes exact cleanup");
		}

		for (const auto kind : { application::epoch_catalog_operation_kind::prune,
								 application::epoch_catalog_operation_kind::cache_delete }) {
			for (const auto phase : { complete, aborted }) {
				fixture value;
				seed_maintenance_recovery(value.State,
										  maintenance_recovery_record(value.State, kind, phase));
				value.Host->accept(make_handoff(value.State));
				run_effect_completion(value);
				expect(value.State.Maintenance->Phase == phase
						   && value.State.PruneRequests.empty()
						   && value.State.DeleteRequests.empty()
						   && value.Host->state().Authority.has_value(),
					   "terminal maintenance recovery is an inert replay");
			}

			fixture value;
			auto    repair         = maintenance_recovery_record(value.State, kind, repair_required);
			repair.RepairFromPhase = kind == application::epoch_catalog_operation_kind::prune
				? std::optional{ ref_delete_in_progress }
				: std::optional{ quarantine_rename_in_progress };
			seed_maintenance_recovery(value.State, repair);
			value.Host->accept(make_handoff(value.State));
			run_effect_completion(value);
			expect(!value.Host->state().Authority
					   && has_repair_diagnostic(value.State)
					   && value.State.PruneRequests.empty()
					   && value.State.DeleteRequests.empty(),
				   "repair-required maintenance blocks authority activation");
		}
	}

	void maintenance_faults_fail_closed_and_restart_from_durable_phase()
	{
		const auto restart = [](fixture& aValue) {
			expect(aValue.State.Maintenance.has_value(),
				   "fault leaves an exact durable maintenance record");
			seed_maintenance_recovery(aValue.State, *aValue.State.Maintenance);
			aValue.Host->accept(make_handoff(aValue.State));
			run_effect_completion(aValue);
		};

		{
			fixture value;
			establish_ready_comparison(value);
			const auto epoch = value.State.Snapshot.LatestLive->Id;
			value.State.MaintenanceCatalogFailures.emplace_back(
				"catalog.exact-anchors",
				application::epoch_catalog_failure::unavailable);
			value.Host->dispatch(core::prune_review_epochs_intent{
				.Catalog = *value.Host->state().Catalog,
				.Epochs  = { epoch },
			});
			run_effect_completion(value);
			expect(!value.State.Maintenance && value.State.PruneRequests.empty()
					   && value.Host->state().Catalog == catalog_identity(1)
					   && value.Host->state().Status == core::epoch_status::failed,
				   "pre-intent anchor lookup failure performs no destructive work");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			const auto epoch = value.State.Snapshot.LatestLive->Id;
			value.State.MaintenanceCatalogFailures.emplace_back(
				"catalog.transition:ref-delete",
				application::epoch_catalog_failure::unavailable);
			value.Host->dispatch(core::prune_review_epochs_intent{
				.Catalog = *value.Host->state().Catalog,
				.Epochs  = { epoch },
			});
			run_effect_completion(value);
			expect(value.State.Maintenance
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::prune_intent
					   && value.State.PruneRequests.empty(),
				   "point-of-no-return transition failure preserves a sealed "
				   "abortable "
				   "prune intent");
			restart(value);
			expect(value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::aborted
					   && value.State.PruneRequests.empty(),
				   "restart aborts a prune whose point-of-no-return transition did "
				   "not "
				   "commit");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			const auto epoch = value.State.Snapshot.LatestLive->Id;
			value.State.MaintenanceCacheFailures.emplace_back("cache.prune",
															  application::git_cache_failure::ref_conflict);
			value.Host->dispatch(core::prune_review_epochs_intent{
				.Catalog = *value.Host->state().Catalog,
				.Epochs  = { epoch },
			});
			run_effect_completion(value);
			expect(value.State.Maintenance
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::
							   repair_required
					   && has_repair_diagnostic(value.State),
				   "ambiguous exact-ref failure is durably marked repair-required");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			const auto epoch = value.State.Snapshot.LatestLive->Id;
			value.State.MaintenanceCatalogFailures.emplace_back(
				"catalog.finalize-prune",
				application::epoch_catalog_failure::unavailable);
			value.Host->dispatch(core::prune_review_epochs_intent{
				.Catalog = *value.Host->state().Catalog,
				.Epochs  = { epoch },
			});
			run_effect_completion(value);
			expect(value.State.Maintenance
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::
							   ref_delete_in_progress
					   && value.State.PruneRequests.size() == 1U,
				   "catalog failure after exact ref deletion retains the post-PONR "
				   "phase");
			const auto authorization = value.State.PruneRequests.front();
			restart(value);
			expect(value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::complete
					   && value.State.PruneRequests.size() == 2U
					   && value.State.PruneRequests.back() == authorization
					   && value.State.Binding.Catalog == catalog_identity(2),
				   "restart replays the same prune authorization and completes "
				   "visibility once");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			value.State.MaintenanceCacheFailures.emplace_back("cache.delete",
															  application::git_cache_failure::process_failure);
			value.Host->dispatch(core::delete_repository_cache_intent{
				.Catalog = *value.Host->state().Catalog,
				.Cache   = value.State.Binding.Authority.Cache,
			});
			run_effect_completion(value);
			expect(value.State.Maintenance
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::
							   quarantine_rename_in_progress
					   && value.State.DeleteRequests.size() == 1U,
				   "physical deletion failure retains the exact post-PONR quarantine "
				   "phase");
			const auto authorization = value.State.DeleteRequests.front();
			restart(value);
			expect(value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::complete
					   && value.State.DeleteRequests.size() == 3U
					   && std::ranges::all_of(value.State.DeleteRequests,
											  [&authorization](const auto& aRequest) { return aRequest == authorization; })
					   && value.State.Binding.Catalog == catalog_identity(2),
				   "restart replays one cache-delete authorization through "
				   "quarantine, "
				   "tombstone, and cleanup");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			value.State.CacheDeleteFinalizesWithoutChange = true;
			value.Host->dispatch(core::delete_repository_cache_intent{
				.Catalog = *value.Host->state().Catalog,
				.Cache   = value.State.Binding.Authority.Cache,
			});
			run_effect_completion(value);
			expect(value.State.Maintenance
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::
							   repair_required
					   && has_repair_diagnostic(value.State),
				   "cache-delete success without the exact catalog revision "
				   "postcondition "
				   "is marked repair-required");
		}

		{
			fixture value;
			establish_ready_comparison(value);
			value.State.MaintenanceCatalogFailures.emplace_back(
				"catalog.transition:cleanup",
				application::epoch_catalog_failure::unavailable);
			value.Host->dispatch(core::delete_repository_cache_intent{
				.Catalog = *value.Host->state().Catalog,
				.Cache   = value.State.Binding.Authority.Cache,
			});
			run_effect_completion(value);
			expect(value.State.Maintenance
					   && value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::
							   catalog_tombstoned_cleanup_pending
					   && value.State.Binding.Catalog == catalog_identity(2),
				   "terminal bookkeeping failure preserves the already-visible cache "
				   "tombstone phase");
			const auto authorization = value.State.DeleteRequests.front();
			restart(value);
			expect(value.State.Maintenance->Phase
						   == application::epoch_catalog_operation_phase::complete
					   && std::ranges::all_of(value.State.DeleteRequests,
											  [&authorization](const auto& aRequest) { return aRequest == authorization; })
					   && value.State.Binding.Catalog == catalog_identity(2),
				   "restart finishes bookkeeping without a second catalog revision");
		}
	}

	/// \brief A retained catalog naming absent commits must say so.
	///
	/// \details BPR-0200. A catalog written before the repository's history
	/// changed still names commits by OID, and nothing in the storage layer can
	/// notice they have gone: `validate_schema` checks the schema, the
	/// application id and the catalog incarnation, none of which move when only
	/// the objects do. The guard is the cache, which refuses to prepare a
	/// revision it cannot produce.
	///
	/// What matters is what a reviewer is then told. Three outcomes were
	/// plausible when this was carded — a silent rebuild, an actionable failure,
	/// or stale history presented as correct — and the last is the one that
	/// would be a wrong answer about their own review state.
	///
	/// This asserts the second: `object_mismatch` from the resolve path reaches
	/// presentation as `repair_required`, which the comparison control words as
	/// "The local review history requires repair."
	void retained_catalog_naming_absent_commits_asks_for_repair()
	{
		{
			fixture value;
			value.State.PrepareCacheFailures.emplace_back(
				"cache.prepare-revision",
				application::git_cache_failure::object_mismatch);
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			// Drained rather than driven through a fixed number of effects. A
			// preparation that fails queues fewer of them, and
			// drive_through_preparation runs exactly three.
			drain(value);

			expect(
				has_repair_diagnostic(value.State),
				"a commit the cache cannot produce is reported as requiring "
				"repair rather than as an empty history");
			expect(
				!value.Host->state().AcceptedComparison.has_value(),
				"no comparison is accepted against commits that are gone, so "
				"nothing stale is presented as current");
		}

		// And the neighbouring failure, so the diagnostic is not simply what
		// this path always says. An inventory mismatch is a different cause with
		// the same remedy; a cancellation is neither.
		{
			fixture value;
			value.State.PrepareCacheFailures.emplace_back(
				"cache.prepare-revision",
				application::git_cache_failure::cancelled);
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(
				!has_repair_diagnostic(value.State),
				"a cancelled preparation is not reported as requiring repair, "
				"so the repair diagnostic means what it says");
		}
	}

	/// BPR-0361. A Git subprocess failure used to arrive as `unavailable`,
	/// which it shared with a cancellation, an invalid path, an unavailable
	/// lock, and a spool failure. Five things to go and look at, one code.
	///
	/// The card asked whether the epoch path should instead carry the text Git
	/// wrote, as the agent path does since BPR-0348. It should not: the
	/// functional core holds compact typed state, and threading a message
	/// through it would put an owning payload where the architecture has none.
	/// A finer code says which layer failed; the text stays in the adapter that
	/// produced it.
	void a_failed_git_subprocess_names_itself_rather_than_unavailable()
	{
		{
			fixture value;
			value.State.PrepareCacheFailures.emplace_back(
				"cache.prepare-revision",
				application::git_cache_failure::process_failure);
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(
				has_diagnostic(
					value.State, core::epoch_reason_code::process_failure),
				"a failing Git subprocess reaches presentation as a process "
				"failure rather than as a bare unavailability");
			expect(
				!has_diagnostic(
					value.State, core::epoch_reason_code::unavailable),
				"and does not also arrive as the code it used to be collapsed "
				"into, which is what made the two indistinguishable");
		}

		// The neighbour it used to share a code with. If this ever reports a
		// process failure the distinction has been lost again.
		{
			fixture value;
			value.State.PrepareCacheFailures.emplace_back(
				"cache.prepare-revision",
				application::git_cache_failure::lock_unavailable);
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(
				!has_diagnostic(
					value.State, core::epoch_reason_code::process_failure),
				"an unavailable lock is not reported as a Git subprocess "
				"failure, so the process-failure code means what it says");
		}

		// BPR-0880 continues the same division. Git refusing an object its own
		// fsck rules class as an error is not a command that failed and not a
		// cache that needs rebuilding, and being told either sends a reviewer
		// somewhere there is nothing to find.
		{
			fixture value;
			value.State.PrepareCacheFailures.emplace_back(
				"cache.prepare-revision",
				application::git_cache_failure::unsupported_object);
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(
				has_diagnostic(
					value.State, core::epoch_reason_code::unsupported_object),
				"an object Git refuses reaches presentation as an unsupported "
				"object rather than as a bare process failure");
			expect(
				!has_diagnostic(
					value.State, core::epoch_reason_code::process_failure)
					&& !has_diagnostic(
						value.State, core::epoch_reason_code::repair_required),
				"and is neither a failed command nor a local repair, because "
				"neither a retry nor a rebuild can change the answer");
		}
	}

	/// BPR-0459. `start_comparison_resolution` separates a cancellation from a
	/// failure on every catalog result and then mapped its cache open through
	/// `cache_failure`, where `git_cache_failure::cancelled` becomes
	/// `epoch_failure::unavailable`. A reviewer who cancelled was told the
	/// comparison was unavailable, which names a repository problem they do not
	/// have.
	///
	/// The offered revision here is already the catalog's latest live epoch, so
	/// nothing is prepared or committed and the resolve performs the run's only
	/// cache open. The cancellation is injected rather than requested through
	/// the host, because a requested stop also moves the comparison context and
	/// the completion would be rejected as stale before it could be mapped.
	void a_cancelled_comparison_cache_open_is_reported_as_cancelled()
	{
		fixture    value;
		auto       accepted = make_handoff(value.State);
		const auto revision = core::accepted_pull_request_revision{
			.Source = {
				.Observation           = opaque<core::epoch_source_observation_id>(0x64U),
				.Snapshot              = accepted.Snapshot,
				.IdentityGeneration    = accepted.IdentityGeneration,
				.PullRequestGeneration = accepted.PullRequestGeneration,
			},
			.Origin     = accepted.Origin,
			.BaseBranch = accepted.BaseBranch,
			.Base       = accepted.Base,
			.Head       = accepted.Head,
		};
		value.State.Snapshot.LatestLive  = summary_for(revision, value.State.Binding.Catalog);
		value.State.Snapshot.NextOrdinal = core::epoch_ordinal{ 2 };
		value.State.CacheOpenFailures.push_back(
			application::git_cache_failure::cancelled);

		value.Host->accept(std::move(accepted));
		drain(value);

		expect(
			value.State.CacheOpenFailures.empty()
				&& std::ranges::count(value.State.Calls, "cache.open") == 1,
			"the resolve performs the run's only cache open, so the cancelled "
			"open is the one under test");
		expect(
			value.Host->state().Status == core::epoch_status::cancelled
				&& !value.Host->state().AcceptedComparison.has_value(),
			"a cancelled cache open leaves the comparison cancelled rather than "
			"failed");
		expect(
			has_diagnostic(value.State, core::epoch_reason_code::cancelled)
				&& !has_diagnostic(
					value.State, core::epoch_reason_code::unavailable),
			"and reaches presentation as a cancellation rather than as the "
			"unavailability the shared failure mapping used to produce");
	}

	/// BPR-0459, the same distinction on the append commit's own cache open.
	/// `start_catalog_commit` reopens the repository actor's cache session when
	/// the retained one is not the session it needs, and answered a cancelled
	/// reopen with `abortAndFail`, three lines above the `abortAndCancel` its
	/// adjacent stop check already used.
	void a_cancelled_commit_cache_open_is_reported_as_cancelled()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State, 1U, 20U, true));
		drive_through_preparation(value);
		value.State.RetainedCacheSessionIsForeign = true;
		value.State.CacheOpenFailures.push_back(
			application::git_cache_failure::cancelled);

		run_effect_completion(value);

		expect(
			value.State.CacheOpenFailures.empty()
				&& std::ranges::count(value.State.Calls, "cache.open") == 2,
			"the commit reopens the session it needs, and that open is the "
			"cancelled one");
		expect(
			value.Host->state().Status == core::epoch_status::cancelled
				&& value.Host->state().Catalog == catalog_identity()
				&& std::ranges::find(value.State.Calls, "catalog.transition:ponr")
					== value.State.Calls.end(),
			"a cancelled cache open aborts the append as a cancellation before "
			"the point of no return rather than as a failure");
		expect(
			has_diagnostic(value.State, core::epoch_reason_code::cancelled)
				&& !has_diagnostic(
					value.State, core::epoch_reason_code::unavailable),
			"and says so in the diagnostic the commit terminal publishes");
	}

	void payload_requires_durable_receiving_ownership()
	{
		for (const auto fault : { receiving_witness_fault::omitted, receiving_witness_fault::repeated, receiving_witness_fault::foreign_operation }) {
			fixture value;
			value.State.ReceivingWitnessFault = fault;
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(!value.State.Retained && !value.Host->state().PendingIngestion
					   && std::ranges::count(value.State.Calls, "transport.acquire") == 0
					   && std::ranges::count(value.State.Calls, "catalog.receiving") == (fault == receiving_witness_fault::repeated ? 1 : 0)
					   && has_diagnostic(value.State, core::epoch_reason_code::repair_required),
				   "omitted, repeated or foreign receiving witnesses cannot start payload acquisition");
		}
		for (const auto failure : { application::epoch_catalog_failure::unavailable, application::epoch_catalog_failure::cancelled }) {
			fixture value;
			value.State.ReceivingFailure = failure;
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(!value.State.Retained && !value.Host->state().PendingIngestion
					   && std::ranges::count(value.State.Calls, "transport.acquire") == 0
					   && std::ranges::count(value.State.Calls, "catalog.receiving") == 1
					   && has_diagnostic(value.State, failure == application::epoch_catalog_failure::cancelled ? core::epoch_reason_code::cancelled : core::epoch_reason_code::unavailable),
				   "failed or cancelled receiving persistence stops acquisition with its typed terminal");
		}
	}

	void installation_requires_one_exact_durable_witness()
	{
		for (const auto fault : { install_witness_fault::omitted, install_witness_fault::repeated, install_witness_fault::changed_result }) {
			fixture value;
			value.State.InstallWitnessFault = fault;
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(!value.State.Retained && !value.Host->state().PendingIngestion
					   && std::ranges::count(value.State.Calls, "catalog.imported") == (fault == install_witness_fault::omitted ? 0 : 1)
					   && std::ranges::count(value.State.Calls, "catalog.commit-authorized") == 0
					   && has_diagnostic(value.State, core::epoch_reason_code::repair_required),
				   "omitted, duplicate and changed installation witnesses cannot authorize retained refs");
		}
		for (const auto failure : { application::epoch_catalog_failure::unavailable, application::epoch_catalog_failure::cancelled }) {
			fixture value;
			value.State.InstallFailure = failure;
			value.Host->accept(make_handoff(value.State, 1U, 20U, true));
			drain(value);
			expect(!value.State.Retained && !value.Host->state().PendingIngestion
					   && std::ranges::count(value.State.Calls, "catalog.imported") == 1
					   && std::ranges::count(value.State.Calls, "catalog.commit-authorized") == 0
					   && has_diagnostic(value.State, failure == application::epoch_catalog_failure::cancelled ? core::epoch_reason_code::cancelled : core::epoch_reason_code::unavailable),
				   "failed or cancelled install persistence stops preparation with its typed terminal");
		}
	}

	void shutdown_waits_for_worker_and_decision_quiescence()
	{
		fixture value;
		value.Host->accept(make_handoff(value.State));
		value.Host->shutdown();
		expect(
			value.Host->is_closing() && !value.Host->is_quiescent(),
			"shutdown closes ingress while an accepted worker remains");
		value.Effects.run_next();
		expect(
			!value.Host->is_quiescent(),
			"worker completion is not quiescent before decision-lane retirement");
		value.Decisions.run_next();
		expect(
			value.Host->is_quiescent(),
			"shutdown becomes quiescent after terminal decision retirement");
	}
	class epoch_test_windows final : public application::epoch_window_context_port
	{
	public:
		core::profile_window_catalog Catalog;
		[[nodiscard]] auto           current(const core::bpr_window_context aContext) const noexcept -> bool override
		{
			return Catalog.accepts(aContext);
		}
		[[nodiscard]] auto launch() -> core::bpr_window_context
		{
			const auto launched = Catalog.launch(core::default_bpr_profile_id, core::profile_launch_intent::new_window);
			expect(launched.has_value(), "test window is admitted by the catalog");
			return launched->Activated;
		}
	};

	[[nodiscard]] auto profile_actor_key() -> application::profile_epoch_actor_key
	{
		const auto owner = authority();
		return { .Profile = owner.Profile, .ProfileRevision = owner.ProfileRevision, .Host = owner.Host, .HostAuthority = owner.HostAuthority, .Account = owner.Account, .Repository = owner.Repository, .ObjectFormat = core::git_hash_algorithm::sha1, .Mode = owner.Mode };
	}

	void profile_actor_registry_bounds_authority_and_draining_leases()
	{
		epoch_test_windows                        windows;
		queued_executor                           effects{ false };
		shared_state                              state;
		fake_cache_port                           caches{ state };
		application::profile_epoch_actor_registry registry{ core::default_bpr_profile_id, windows, caches, effects };
		auto                                      window   = windows.launch();
		auto                                      consumer = registry.attach(window);
		expect(consumer.has_value(), "live window attaches");
		expect(!registry.attach(window), "one exact window cannot attach twice");
		auto foreign    = window;
		foreign.Profile = core::bpr_profile_id{ 2U };
		expect(!registry.attach(foreign), "foreign profile has no consumer");
		const auto key   = profile_actor_key();
		auto       first = (*consumer)->acquire(key);
		auto       same  = (*consumer)->acquire(key);
		expect(first && same && *first == *same, "equal authority reuses one actor");
		auto variants               = std::vector<application::profile_epoch_actor_key>(6U, key);
		variants[0].Host            = core::git_host_id{ key.Host.value() + 1U };
		variants[1].ProfileRevision = core::github_profile_revision{ key.ProfileRevision.value() + 1U };
		variants[2].HostAuthority   = opaque<core::host_authority_fingerprint>(0x96U);
		variants[3].Account         = core::github_account_id{ key.Account.value() + 1U };
		variants[4].ObjectFormat    = core::git_hash_algorithm::sha256;
		variants[5].Mode            = core::github_access_mode::public_preview;
		variants[5].Account         = { };
		for (const auto& variant : variants) {
			const auto distinct = (*consumer)->acquire(variant);
			expect(distinct && *distinct != *first, "foreign cache authority and access mode never reuse actor");
		}
		auto invalid       = key;
		invalid.Repository = { };
		expect(!(*consumer)->acquire(invalid), "invalid actor authority is refused");
		for (std::size_t index = 0U; index < 40U; ++index) {
			(*consumer)->retire();
			expect(windows.Catalog.close(window).has_value(), "retired window closes");
			window   = windows.launch();
			consumer = registry.attach(window);
			expect(consumer.has_value(), "lifetime churn is not concurrent capacity");
		}
		auto actors = std::vector<std::shared_ptr<application::profile_epoch_actor>>{ *first };
		for (std::size_t index = 1U; index < 1'000U; ++index) {
			auto next       = key;
			next.Repository = core::repository_id{ key.Repository.value() + index };
			auto actor      = (*consumer)->acquire(next);
			expect(actor.has_value(), "actor boundary admits exactly one thousand live leases");
			actors.push_back(std::move(*actor));
		}
		auto excess       = key;
		excess.Repository = core::repository_id{ key.Repository.value() + 1'000U };
		expect(!(*consumer)->acquire(excess), "actor count plus one is refused");
		actors.clear();
		int calls = 0;
		(*first)->enqueue([&calls] { ++calls; });
		first->reset();
		same->reset();
		registry.shutdown();
		expect(!(*consumer)->active() && !registry.is_quiescent(), "shutdown retires admission but queued work retains actor");
		effects.run_next();
		expect(calls == 1 && registry.is_quiescent(), "final FIFO drain releases actor despite retained inert consumer handle");
	}

	/// [[BPR-1344]]. Queued preparation never runs ahead of the reviewer's work,
	/// whether that work was queued before the drain began or during it.
	void profile_actor_runs_reviewer_work_before_queued_preparation()
	{
		epoch_test_windows                        windows;
		queued_executor                           effects{ false };
		shared_state                              state;
		fake_cache_port                           caches{ state };
		application::profile_epoch_actor_registry registry{ core::default_bpr_profile_id, windows, caches, effects };
		const auto                                consumer = registry.attach(windows.launch());
		expect(consumer.has_value(), "priority test attaches one window");
		auto acquired = (*consumer)->acquire(profile_actor_key());
		expect(acquired.has_value(), "priority test acquires cache actor");
		auto       actor       = std::move(*acquired);
		auto       order       = std::vector<std::string>{ };
		const auto preparation = application::profile_epoch_actor_priority::preparation;
		actor->enqueue([&order, &actor] {
			order.emplace_back("first preparation");
			actor->enqueue([&order] { order.emplace_back("reviewer during preparation"); });
		},
					   preparation);
		actor->enqueue([&order] { order.emplace_back("second preparation"); }, preparation);
		actor->enqueue([&order] { order.emplace_back("reviewer"); });
		expect(effects.pending() == 1U, "one drain is scheduled for both queues");
		effects.run_next();
		expect(order == std::vector<std::string>{ "reviewer", "first preparation", "reviewer during preparation", "second preparation" },
			   "the reviewer's work runs before any preparation still queued");
		expect(effects.pending() == 0U, "the drain empties both queues");
		actor.reset();
		registry.shutdown();
		expect(registry.is_quiescent(), "the drained actor releases");
	}

	void profile_actor_replacement_waits_for_session_destruction()
	{
		epoch_test_windows                        windows;
		queued_executor                           effects{ false };
		shared_state                              state;
		fake_cache_port                           caches{ state };
		application::profile_epoch_actor_registry registry{ core::default_bpr_profile_id, windows, caches, effects };
		const auto                                consumer = registry.attach(windows.launch());
		expect(consumer.has_value(), "destruction test attaches one window");
		auto acquired = (*consumer)->acquire(profile_actor_key());
		expect(acquired.has_value(), "destruction test acquires cache actor");
		auto actor = std::move(*acquired);
		actor->enqueue([actor, &caches] {
			auto opened = caches.open(application::git_cache_open_request{
										  .ApplicationSession = core::application_session_id{ 91U },
										  .Authority          = authority(),
										  .ObjectFormat       = core::git_hash_algorithm::sha1 },
									  { });
			expect(opened.has_value(), "destruction test opens the retained session");
			actor->cache() = std::move(*opened);
		});
		effects.run_next();
		std::mutex              mutex;
		std::condition_variable changed;
		bool                    entered = false;
		bool                    release = false;
		state.DuringCacheDestruction    = [&] {
			std::unique_lock lock{ mutex };
			entered = true;
			changed.notify_all();
			changed.wait(lock, [&] { return release; });
		};
		std::jthread worker{ [retiring = std::move(actor)]() mutable { retiring.reset(); } };
		{
			std::unique_lock lock{ mutex };
			changed.wait(lock, [&] { return entered; });
		}
		const auto refused = (*consumer)->acquire(profile_actor_key());
		expect(!refused && refused.error() == application::profile_epoch_actor_failure::actor_draining,
			   "weak expiration does not permit overlapping native session destruction and reopen");
		{
			std::scoped_lock lock{ mutex };
			release = true;
		}
		changed.notify_all();
		worker.join();
		state.DuringCacheDestruction = { };
		auto reopened                = (*consumer)->acquire(profile_actor_key());
		expect(reopened.has_value() && state.ActiveCacheSessions == 0U,
			   "replacement admission resumes after exact session destruction completes");
		reopened->reset();
		registry.shutdown();
		expect(registry.is_quiescent(), "destruction marker participates in profile quiescence");
	}

	void profile_windows_share_one_cache_without_sharing_selection_or_lifetime()
	{
		fixture                                   first;
		fixture                                   second;
		epoch_test_windows                        windows;
		application::profile_epoch_actor_registry registry{ core::default_bpr_profile_id, windows, first.Caches, first.Effects };
		const auto                                firstWindow  = windows.launch();
		const auto                                secondWindow = windows.launch();
		first.Host.reset();
		second.Host.reset();
		second.State.Binding.Authority.PullRequest = core::pull_request_id{ 72U };
		second.State.Binding.Authority.Number      = core::pull_request_number{ 82U };
		second.State.Binding.Authority.Scope       = opaque<core::pull_request_authority_scope_id>(0x44U);
		second.State.Snapshot.Binding              = second.State.Binding;
		const auto attach                          = [&](fixture& aFixture, const core::bpr_window_context aWindow,
														 const core::application_session_id aSession) {
			aFixture.Host = std::make_unique<application::epoch_application_host>(
				core::epoch_state{ .ApplicationSession = aSession }, registry, aWindow,
				aFixture.Catalogs, aFixture.Entropy, aFixture.Clock,
				first.Effects, aFixture.Decisions, aFixture.Projections);
		};
		attach(first, firstWindow, core::application_session_id{ 91U });
		attach(second, secondWindow, core::application_session_id{ 92U });
		const auto drainBoth = [&] {
			while (first.Effects.pending() || first.Decisions.pending() || second.Decisions.pending()) {
				if (first.Effects.pending()) {
					first.Effects.run_next();
				}
				if (first.Decisions.pending()) {
					first.Decisions.run_next();
				}
				if (second.Decisions.pending()) {
					second.Decisions.run_next();
				}
			}
		};
		first.Host->accept(make_handoff(first.State, 1U, 20U, true));
		drainBoth();
		const auto firstSelection        = selected_diff_request_for(first);
		auto       secondHandoff         = make_handoff(second.State, 1U, 20U, true);
		secondHandoff.ApplicationSession = core::application_session_id{ 92U };
		secondHandoff.PullRequest        = second.State.Binding.Authority.PullRequest;
		secondHandoff.Number             = second.State.Binding.Authority.Number;
		secondHandoff.SmartGit           = std::make_unique<fake_smart_authority>(second.State, application::authority_binding(secondHandoff), 1U);
		second.Host->accept(std::move(secondHandoff));
		drainBoth();
		expect(second.Host->state().AcceptedComparison.has_value(), "second window independently ingests and selects its PR");
		auto       secondSelection = selected_diff_request_for(second);
		const auto resolve         = [&](fixture& aFixture, const application::workbench_selected_diff_request& aRequest) {
			std::optional<application::workbench_selected_diff_observation> result;
			std::jthread                                                    worker{ [&] { result = aFixture.Host->resolve(aRequest, { }); } };
			first.Effects.wait_for_pending();
			first.Effects.run_next();
			worker.join();
			expect(result && result->Status == application::workbench_selected_diff_status::available,
				   "window selection remains usable after sibling cache request switch");
			expect(first.State.CacheOpenRequests.back().ApplicationSession == aRequest.Diff.ApplicationSession
					   && first.State.CacheOpenRequests.back().Authority == aRequest.Diff.Authority,
				   "FIFO selects the calling window's exact immutable cache request");
		};
		resolve(first, firstSelection);
		const auto openCount = first.State.CacheOpenRequests.size();
		resolve(first, firstSelection);
		expect(first.State.CacheOpenRequests.size() == openCount, "equal exact request reuses session");
		resolve(second, secondSelection);
		resolve(first, firstSelection);
		expect(first.State.MaximumActiveCacheSessions == 1U, "interleaved windows never open competing cache sessions");
		const auto callsBefore = first.State.Calls.size();
		const auto foreign     = second.Host->resolve(firstSelection, { });
		expect(foreign.Status != application::workbench_selected_diff_status::available
				   && first.State.Calls.size() == callsBefore,
			   "another window's selected warrant is refused without effects");
		const auto oldPair = *second.Host->state().AcceptedComparison;
		second.Host->dispatch(core::select_comparison_anchors_intent{
			.Left = oldPair.Right.Identity, .Right = oldPair.Left.Identity, .ExpectedCatalogRevision = second.Host->state().Catalog->Revision });
		const auto secondProjections = second.State.Updates.size();
		first.Host->shutdown();
		expect(first.Host->is_quiescent() && first.Effects.pending() != 0U,
			   "A quiescence does not wait for B's queued work on the shared FIFO");
		expect(windows.Catalog.close(firstWindow).has_value(), "first catalog window closes after retirement");
		drainBoth();
		expect(first.Host->is_quiescent(), "first host quiesces independently of surviving window");
		expect(second.State.Updates.size() > secondProjections
				   && second.Host->state().AcceptedComparison->Left.Identity == oldPair.Right.Identity,
			   "B queued selection and projection complete after A closes");
		secondSelection = selected_diff_request_for(second);
		resolve(second, secondSelection);
		const auto staleWindow = secondWindow;
		second.Host->shutdown();
		expect(windows.Catalog.close(secondWindow).has_value(), "final window closes");
		drainBoth();
		expect(second.Host->is_quiescent() && first.State.ActiveCacheSessions == 0U,
			   "final window drain releases the lifetime-exclusive cache session");
		expect(!registry.attach(staleWindow), "closed exact generation cannot reattach");
		const auto reopened = windows.launch();
		attach(second, reopened, core::application_session_id{ 92U });
		auto again               = make_handoff(second.State, 2U, 20U);
		again.ApplicationSession = core::application_session_id{ 92U };
		again.PullRequest        = second.State.Binding.Authority.PullRequest;
		again.Number             = second.State.Binding.Authority.Number;
		second.Host->accept(std::move(again));
		drainBoth();
		expect(second.Host->state().AcceptedRevision.has_value(), "reopened consumer can reacquire drained actor");
		second.Host->shutdown();
		drainBoth();
		registry.shutdown();
		expect(registry.is_quiescent(), "profile registry drains separately from its window facades");
	}
} // namespace

namespace {
	// [[BPR-1327]]. `--live-single-blob-ms=N --live-batch-ms=N` replace the
	// stand-in round trips with measured ones.
	constinit std::chrono::milliseconds LiveSingleBlob{ 20 }; // NOLINT(bugprone-throwing-static-initialization): constinit forbids dynamic initialization; libc++ omits noexcept.
	constinit std::chrono::milliseconds LiveBatch{ 25 }; // NOLINT(bugprone-throwing-static-initialization): constinit forbids dynamic initialization; libc++ omits noexcept.

	void parse_live_latencies(const int aCount, char** aArguments)
	{
		for (int index = 1; index < aCount; ++index) {
			const auto argument = std::string_view{ aArguments[index] };
			const auto value    = [&argument](const std::string_view aPrefix) -> std::optional<std::chrono::milliseconds> {
				if (!argument.starts_with(aPrefix)) {
					return std::nullopt;
				}
				return std::chrono::milliseconds{ std::stoi(std::string{ argument.substr(aPrefix.size()) }) };
			};
			if (const auto single = value("--live-single-blob-ms=")) {
				LiveSingleBlob = *single;
			} else if (const auto batch = value("--live-batch-ms=")) {
				LiveBatch = *batch;
			}
		}
	}

	void corpus_sparse_read_coalesces_replacement_and_yields_to_navigation()
	{
		fixture value;
		establish_sparse_three_files(value);
		value.State.TreeEntryResults.clear();
		const auto selected = selected_diff_request_for(value);
		const auto scope    = value.Host->bind_source_search(selected.WorkbenchAuthority, selected.Diff.Comparison);
		expect(scope.has_value(), "sparse search scope minted without selected-file grant");
		auto entry   = application::git_cache_changed_entry{ };
		entry.Right  = application::git_cache_changed_side{ "unopened.cpp", sha1_blob("exact selected blob"), 0100644U };
		entry.Change = 'A';
		value.State.CatalogueEntries.push_back(std::move(entry));
		auto catalogue = application::source_corpus_observation{ };
		auto caller    = std::jthread{ [&] { catalogue = value.Host->catalogue(*scope, { }); } };
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		caller.join();
		expect(catalogue.Corpus && catalogue.Corpus->Entries.size() == 1U, "complete exact sparse catalogue available");
		value.State.ExactBlobLatency             = std::chrono::milliseconds{ 100 };
		value.State.ExactBlobLatencyObservesStop = true;
		auto oldStop                             = std::stop_source{ };
		auto first                               = application::source_corpus_material{ };
		auto replacement                         = application::source_corpus_material{ };
		caller                                   = std::jthread{ [&] { first = value.Host->read(catalogue.Corpus, 0U, core::diff_side::right, oldStop.get_token(), [] { return true; }); } };
		value.Effects.wait_for_pending();
		auto actor = std::jthread{ [&] { value.Effects.run_next(); } };
		while (value.State.ExactBlobReadsInFlight.load() == 0U) {
			std::this_thread::yield();
		}
		oldStop.request_stop();
		caller.join();
		expect(first.Status == application::diff_blob_status::cancelled, "old query returns its cancelled terminal while material remains needed");
		caller = std::jthread{ [&] { replacement = value.Host->read(catalogue.Corpus, 0U, core::diff_side::right, { }, [] { return true; }); } };
		actor.join();
		caller.join();
		expect(replacement.Status == application::diff_blob_status::available && value.State.ExactBlobRequests.size() == 1U && value.State.BlobRequests.size() == 1U, "replacement joins one immutable in-flight acquisition, independent of query generation");
		expect(value.Host->outstanding_jobs() == 0U, "coalesced callers retire one actor job");
		const auto nextScope = value.Host->bind_source_search(selected.WorkbenchAuthority, selected.Diff.Comparison, core::document_tab_id{ 2U });
		caller               = std::jthread{ [&] { catalogue = value.Host->catalogue(*nextScope, { }); } };
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		caller.join();
		// Use a new exact blob, so the next read must reach the transport.
		auto other                      = value.State.CatalogueEntries[0];
		other.Right->Object             = sha1_blob("different unserved blob");
		value.State.CatalogueEntries[0] = other;
		const auto priorityScope        = value.Host->bind_source_search(selected.WorkbenchAuthority, selected.Diff.Comparison, core::document_tab_id{ 3U });
		caller                          = std::jthread{ [&] { catalogue = value.Host->catalogue(*priorityScope, { }); } };
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		caller.join();
		caller = std::jthread{ [&] { first = value.Host->read(catalogue.Corpus, 0U, core::diff_side::right, { }, [] { return true; }); } };
		value.Effects.wait_for_pending();
		actor = std::jthread{ [&] { value.Effects.run_next(); } };
		while (value.State.ExactBlobReadsInFlight.load() == 0U) {
			std::this_thread::yield();
		}
		const auto scanStop = value.Host->scan_stop_token(*priorityScope);
		value.Host->yield_to_source_navigation();
		actor.join();
		caller.join();
		expect(first.Status == application::diff_blob_status::cancelled && first.Yielded && scanStop.stop_requested() && value.State.ExactBlobStopsObserved.load() == 1U, "pane Find/navigation priority stops a background network read and matching slice without invalidating exact scope");
		value.State.CatalogueFailure = application::git_cache_failure::object_mismatch;
		const auto unavailableScope  = value.Host->bind_source_search(selected.WorkbenchAuthority, selected.Diff.Comparison, core::document_tab_id{ 4U });
		caller                       = std::jthread{ [&] { catalogue = value.Host->catalogue(*unavailableScope, { }); } };
		value.Effects.wait_for_pending();
		value.Effects.run_next();
		caller.join();
		expect(catalogue.Status == core::source_search_status::unavailable && !catalogue.Corpus, "missing retained trees are unavailable rather than empty or today's intersection");
		value.Host->shutdown();
		drain(value);
		expect(value.Host->is_quiescent(), "all source calls and background actor jobs quiesce");
	}

	void exact_historical_corpus_uses_its_pair_and_host_path()
	{
		fixture value;
		establish_ready_comparison(value);
		const auto historical = *value.Host->state().AcceptedComparison;
		value.Host->accept(make_handoff(value.State, 2U, 21U, true));
		drain(value);
		const auto latest = *value.Host->state().AcceptedComparison;
		expect(historical.Right.Commit != value.Host->state().AcceptedRevision->Head, "historical fixture distinguishes latest head");
		const auto scope = value.Host->bind_source_search(workbench_authority_for(value.Host->state()), historical, core::document_tab_id{ 71U });
		expect(scope.has_value(), "epoch host mints exact historical search scope");
		auto entry  = application::git_cache_changed_entry{ };
		entry.Left  = application::git_cache_changed_side{ "historical-only.cpp", oid<core::git_blob_oid>(31U), 0100644U };
		entry.Right = application::git_cache_changed_side{ "historical-only.cpp", oid<core::git_blob_oid>(32U), 0100644U };
		value.State.CatalogueEntries.push_back(std::move(entry));
		const auto content             = std::string{ "unchanged context\nneedle outside provider hunk\n" };
		value.State.CachedBlobOverride = std::vector<std::uint8_t>{ content.begin(), content.end() };
		struct projection final : application::source_search_projection_port
		{
			application::source_search_projection Last;
			void                                  publish(const application::source_search_projection& aProjection) override { Last = aProjection; }
		} published;
		queued_executor                         searchEffects{ false };
		application::source_pr_search_execution execution{ *value.Host, bpr::tests::search_engine() };
		application::source_search_host         search{ value.Decisions, searchEffects, execution, published };
		expect(search.set_scope(scope) && search.set_query("needle"), "historical query admitted through public host");
		auto worker = std::jthread{ [&] { searchEffects.run_next(); } };
		for (auto i = 0U; i < 3U; ++i) {
			value.Effects.wait_for_pending();
			value.Effects.run_next();
		}
		worker.join();
		while (value.Decisions.pending() != 0U) {
			value.Decisions.run_next();
		}
		expect(search.state().Status == core::source_search_status::complete && published.Last.Matches && published.Last.Matches->size() == 2U, "full historical both-side records accepted on decision lane");
		expect(value.State.CatalogueRequests.size() == 1U && value.State.CatalogueRequests[0].Left == historical.Left.Commit && value.State.CatalogueRequests[0].Right == historical.Right.Commit, "catalogue is exact historical pair, never today's membership");
		expect(published.Last.Corpus && published.Last.Corpus->Entries[0].path() == "historical-only.cpp" && value.Host->state().AcceptedComparison == latest, "search preserves latest comparison while scanning historical-only path");
		const auto reads = value.State.BlobRequests.size();
		expect(search.set_query("context"), "replacement query admitted");
		worker = std::jthread{ [&] { searchEffects.run_next(); } };
		for (auto i = 0U; i < 2U; ++i) {
			value.Effects.wait_for_pending();
			value.Effects.run_next();
		}
		worker.join();
		while (value.Decisions.pending() != 0U) {
			value.Decisions.run_next();
		}
		expect(value.State.CatalogueRequests.size() == 1U && value.State.BlobRequests.size() == reads, "replacement reuses immutable catalogue and exact reads without query keys");
		for (auto i = 0U; i < 1000U; ++i) {
			static_cast<void>(search.navigate(core::change_navigation_direction::next));
		}
		expect(value.State.BlobRequests.size() == reads && searchEffects.pending() == 0U, "PR cursor traversal performs no source acquisition or scan");
		search.shutdown();
		expect(search.is_quiescent(), "historical public search quiesces");
		value.Host->invalidate(matching_invalidation());
		drain(value);
		expect(value.Host->catalogue(*scope, { }).Status == core::source_search_status::unavailable, "revoked search corpus cannot be reused");
	}
} // namespace

int main(int argc, char** argv)
{
	parse_live_latencies(argc, argv);
	exact_historical_corpus_uses_its_pair_and_host_path();
	corpus_sparse_read_coalesces_replacement_and_yields_to_navigation();
	historical_source_does_not_accept_live_authority();
	local_clock_skew_cannot_associate_old_provider_event_with_new_transition();
	intervening_provider_update_excludes_old_event_despite_observation_interval();
	base_rewrite_replaces_projected_comparison_despite_sidecar_failure(true, true);
	base_rewrite_replaces_projected_comparison_despite_sidecar_failure(false, true);
	base_rewrite_replaces_projected_comparison_despite_sidecar_failure(true, false);
	base_rewrite_replaces_projected_comparison_despite_sidecar_failure(true, true, true);
	base_rewrite_replaces_projected_comparison_despite_sidecar_failure(true, false, true);
	independent_catalogues_agree_on_evidenced_occurrences();
	live_returns_use_distinct_events_without_rebinding_old_occurrences();
	ordinary_transition_identity_survives_access_and_catalogue_changes();
	missing_events_do_not_invent_a_second_provisional_head();
	degraded_timeline_and_sidecar_failures_preserve_live_append();
	refused_timeline_reconciliation_preserves_base_rewrite();
	delayed_event_keeps_older_observation_when_current_head_changed();
	newly_discovered_old_events_cannot_name_a_later_ordinary_transition();
	uncertain_event_chronology_keeps_the_live_transition_unresolved();
	unavailable_commit_dates_remain_undated_and_retry_on_refresh();
	a_reopened_review_dates_an_undated_update_at_an_unchanged_head();
	delayed_event_keeps_observed_head_separate_without_causal_equivalence();
	existing_durable_event_binding_is_preserved_without_new_supersession();
	old_unbound_event_cannot_identify_a_new_return_to_its_head();
	profile_actor_registry_bounds_authority_and_draining_leases();
	profile_actor_runs_reviewer_work_before_queued_preparation();
	profile_actor_replacement_waits_for_session_destruction();
	profile_windows_share_one_cache_without_sharing_selection_or_lifetime();
	epoch_preparation_diagnostics_preserve_results();
	git_tool_refusals_preserve_actionable_reasons_and_retry();
	gh_tool_refusals_preserve_actionable_reasons_and_retry();
	invalid_handoff_has_no_effects();
	source_collision_retries_before_core_acceptance();
	successful_append_preserves_exact_order();
	installation_requires_one_exact_durable_witness();
	payload_requires_durable_receiving_ownership();
	projection_failure_is_contained_and_decision_progress_drains();
	reentrant_projection_ingress_is_deferred_without_recursion();
	epoch_host_limits_reject_invalid_capacity();
	every_sibling_actor_wait_releases_at_its_bound();
	every_sibling_actor_wait_releases_on_caller_cancellation();
	admission_allocation_failure_rolls_back_every_boundary();
	executor_refusal_rolls_back_offer_admission();
	actor_quota_rejection_preserves_the_admitted_offer();
	refresh_flood_bounds_jobs_actor_fifo_and_completions();
	request_quota_uses_exact_typed_terminal();
	concurrent_executor_refusal_preserves_later_actor_work();
	shutdown_cancels_an_inflight_blob_call();
	shutdown_stop_callback_can_reenter_blob_ingress();
	terminal_slot_reservation_protects_mandatory_cleanup();
	replacement_reserves_required_context_cleanup();
	cleanup_executor_refusal_prevents_replacement_offer();
	replacement_offer_refusal_preserves_cross_repository_authority();
	same_authority_offer_keeps_terminal_capacity_for_core_ingestion();
	replacement_cancellation_does_not_stop_mandatory_cleanup();
	reentrant_terminal_requests_close_blob_authority_before_return();
	reentrant_ingress_flood_preserves_terminal_priority();
	reentrant_ordinary_ingress_retains_fifo_order();
	reentrant_invalidation_precedes_ordinary_ingress();
	failed_same_authority_offer_preserves_active_comparison();
	successful_same_authority_offer_swaps_on_exact_terminal();
	historical_origin_reaches_authority_projection();
	superseded_offer_failure_emits_no_diagnostic();
	matching_invalidation_cancels_pending_offer_and_active_context();
	bounded_history_pages_project_incrementally();
	empty_one_and_exact_bound_history_pages_reach_public_projection();
	history_page_failures_and_catalog_revision_changes_are_typed();
	reordered_history_completion_cannot_cross_source_identity();
	unchanged_and_association_only_skip_git();
	applied_association_error_reports_mandatory_success();
	a_cached_head_publishes_the_newest_history_page_before_ready();
	stale_load_terminal_is_inert_under_reordering();
	stale_prepare_and_comparison_terminals_are_inert();
	same_repository_actor_switches_exact_pr_cache_sessions_in_fifo_order();
	diff_blob_materialization_reuses_the_epoch_cache_actor();
	selected_blob_reads_only_the_exact_missing_object();
	tour_source_materializes_sparse_tree_without_selected_file();
	pinned_source_sparse_read_does_not_grant_ordinary_blob_reads();
	prepared_files_read_their_missing_blobs_in_one_batch();
	preparation_jobs_are_counted_apart_from_the_reviewers();
	a_prepared_blob_read_is_preparations_job();
	a_batch_answer_that_is_not_its_blob_is_not_retained();
	preparation_without_a_batching_transport_reads_nothing_ahead();
	a_reviewer_selection_stops_preparations_exact_read();
	a_failed_batch_backs_off_preparations_single_reads();
	a_cold_open_reads_both_sides_at_once(LiveSingleBlob);
	preparation_cannot_stop_or_delay_either_concurrent_reviewer_read();
	a_superseded_cold_open_stops_both_reads();
	preparation_is_bounded_cancellable_and_complete_reviews_stay_local();
	live_shaped_open_latency(LiveSingleBlob, LiveBatch);
	sparse_capability_applies_independently_of_pull_request_size();
	selected_large_occurrence_imports_only_its_sparse_head();
	selected_large_occurrence_switch_drops_late_import();
	selected_large_occurrence_rebases_after_superseded_commit();
	selected_large_occurrence_rebases_after_superseded_commit(true);
	selected_large_occurrence_reconciles_cancel_after_commit_boundary();
	selected_diff_resolution_reuses_and_revalidates_the_epoch_cache_actor();
	modified_file_requires_both_selected_endpoints();
	zero_length_modified_endpoint_is_present_material();
	selected_diff_resolves_each_endpoint_without_provider_direction();
	selected_diff_rename_resolution_is_directional_and_ambiguous_closed();
	selected_diff_multi_hop_rename_uses_the_exact_selected_pair();
	realignment_context_is_minted_scoped_and_never_assumed();
	invalidated_authority_cannot_materialize_retained_cache();
	agent_workspace_failures_preserve_the_cache_diagnostic();
	invalidated_inflight_blob_cannot_publish_bytes();
	cancellation_before_ponr_aborts_without_publication();
	stop_immediately_after_ponr_cannot_replace_committed_terminal();
	applied_commit_authorization_error_aborts_exactly_before_ponr();
	applied_ponr_error_recovers_to_mandatory_success();
	throwing_exact_abort_degrades_to_a_bounded_terminal();
	staging_ready_recovery_journals_abort_before_cleanup_and_acceptance();
	post_ponr_recovery_resumes_every_durable_phase();
	promotion_error_is_resolved_by_exact_postcondition_inspection();
	recovery_identity_and_pagination_mismatches_fail_closed();
	core_authorized_prune_composes_exact_durable_cleanup();
	core_authorized_cache_delete_closes_session_and_projects_bulk_change();
	maintenance_cancellation_obeys_the_durable_point_of_no_return();
	maintenance_restart_recovery_aborts_or_completes_by_phase();
	maintenance_restart_recovery_covers_every_durable_phase();
	maintenance_faults_fail_closed_and_restart_from_durable_phase();
	retained_catalog_naming_absent_commits_asks_for_repair();
	a_failed_git_subprocess_names_itself_rather_than_unavailable();
	a_cancelled_comparison_cache_open_is_reported_as_cancelled();
	a_cancelled_commit_cache_open_is_reported_as_cancelled();
	shutdown_waits_for_worker_and_decision_quiescence();
	changed_paths_resolve_beside_the_selected_comparison();
	base_numbering_compares_the_merge_base_with_the_left_side();
	return EXIT_SUCCESS;
}
