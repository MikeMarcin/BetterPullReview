#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/testing.hpp"
#include "bpr/core/comparison_preparation.hpp"
#include "bpr/core/git_objects.hpp"

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
	namespace {
		std::atomic<int> ArmedAdmissionStage{ -1 };

		// Consumes a matching arming exactly once so a single armed failure
		// cannot leak into an unrelated later admission.
		void fail_admission_at(
			const detail::testing::epoch_admission_allocation_stage aStage)
		{
			auto expected = static_cast<int>(aStage);
			if (ArmedAdmissionStage.compare_exchange_strong(
					expected, -1, std::memory_order_acq_rel)) {
				throw std::bad_alloc{ };
			}
		}
	} // namespace
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
	namespace {
		inline constexpr std::size_t maximum_identity_attempts    = 32U;
		inline constexpr std::size_t maximum_pending_preparations = 100U;
		inline constexpr std::size_t maximum_recovery_operations  = 1'000U;
		inline constexpr std::size_t maximum_cache_actors         = 1'000U;

		[[nodiscard]] auto validated_limits(const epoch_host_limits aLimits)
			-> epoch_host_limits
		{
			if (!valid(aLimits)) {
				throw std::invalid_argument{ "Invalid epoch host limits" };
			}
			return aLimits;
		}

		using cache_actor_key = profile_epoch_actor_key;

		[[nodiscard]] auto actor_key(
			const accepted_pull_request_handoff& aHandoff) noexcept -> cache_actor_key
		{
			return {
				.Profile         = aHandoff.Profile,
				.ProfileRevision = aHandoff.ProfileRevision,
				.Host            = aHandoff.Host,
				.HostAuthority   = aHandoff.HostAuthority,
				.Account         = aHandoff.Account,
				.Repository      = aHandoff.Repository,
				.ObjectFormat    = aHandoff.ObjectFormat,
				.Mode            = aHandoff.Mode,
			};
		}

		[[nodiscard]] auto actor_key(
			const diff_blob_request& aRequest) noexcept
			-> std::optional<cache_actor_key>
		{
			const auto algorithm = aRequest.Blob.algorithm();
			if (
				!core::valid(aRequest.Context) || !aRequest.Blob.valid()
				|| aRequest.MaximumBytes == 0U
				|| (algorithm != core::git_hash_algorithm::sha1
					&& algorithm != core::git_hash_algorithm::sha256)) {
				return std::nullopt;
			}
			const auto& authority = aRequest.Context.Key.Authority;
			return cache_actor_key{
				.Profile         = authority.Profile,
				.ProfileRevision = authority.ProfileRevision,
				.Host            = authority.Host,
				.HostAuthority   = authority.HostAuthority,
				.Account         = authority.Account,
				.Repository      = authority.Repository,
				.ObjectFormat    = algorithm,
				.Mode            = authority.Mode,
			};
		}

		[[nodiscard]] auto actor_key(
			const agent_workspace_materialization_request& aRequest) noexcept
			-> std::optional<cache_actor_key>
		{
			if (!application::valid(aRequest)) {
				return std::nullopt;
			}
			return cache_actor_key{
				.Profile         = aRequest.Authority.Profile,
				.ProfileRevision = aRequest.Authority.ProfileRevision,
				.Host            = aRequest.Authority.Host,
				.HostAuthority   = aRequest.Authority.HostAuthority,
				.Account         = aRequest.Authority.Account,
				.Repository      = aRequest.Authority.Repository,
				.ObjectFormat    = aRequest.Session.Binding.HeadOid.algorithm(),
				.Mode            = aRequest.Authority.Mode,
			};
		}

		[[nodiscard]] auto actor_key(
			const owned_agent_workspace& aWorkspace) noexcept
			-> std::optional<cache_actor_key>
		{
			if (!application::valid(aWorkspace)) {
				return std::nullopt;
			}
			return cache_actor_key{
				.Profile         = aWorkspace.Authority.Profile,
				.ProfileRevision = aWorkspace.Authority.ProfileRevision,
				.Host            = aWorkspace.Authority.Host,
				.HostAuthority   = aWorkspace.Authority.HostAuthority,
				.Account         = aWorkspace.Authority.Account,
				.Repository      = aWorkspace.Authority.Repository,
				.ObjectFormat    = aWorkspace.Workspace.PinnedHead->algorithm(),
				.Mode            = aWorkspace.Authority.Mode,
			};
		}

		[[nodiscard]] auto actor_key(
			const workbench_selected_diff_request& aRequest) noexcept
			-> std::optional<cache_actor_key>
		{
			const auto algorithm = aRequest.Diff.Comparison.Left.Commit.algorithm();
			if (!application::valid(aRequest.WorkbenchAuthority)
				|| !application::valid(aRequest.File)
				|| aRequest.Context.Request.value() == 0U
				|| aRequest.Context.Session.value() == 0U
				|| aRequest.Context.Generation.value() == 0U
				|| !core::valid(aRequest.Diff.Authority)
				|| !core::valid(aRequest.Diff.Comparison.Left)
				|| !core::valid(aRequest.Diff.Comparison.Right)
				|| !core::valid(aRequest.Diff.ComparisonRevision)
				|| aRequest.Diff.ApplicationSession != aRequest.Context.Session
				|| aRequest.WorkbenchAuthority.ApplicationSession
					!= aRequest.Diff.ApplicationSession
				|| aRequest.WorkbenchAuthority.Profile
					!= aRequest.Diff.Authority.Profile
				|| aRequest.WorkbenchAuthority.ProfileRevision
					!= aRequest.Diff.Authority.ProfileRevision
				|| aRequest.WorkbenchAuthority.Host != aRequest.Diff.Authority.Host
				|| aRequest.WorkbenchAuthority.HostAuthority
					!= aRequest.Diff.Authority.HostAuthority
				|| aRequest.WorkbenchAuthority.Account
					!= aRequest.Diff.Authority.Account
				|| aRequest.WorkbenchAuthority.Viewer
					!= aRequest.Diff.Authority.Viewer
				|| aRequest.WorkbenchAuthority.Repository
					!= aRequest.Diff.Authority.Repository
				|| aRequest.WorkbenchAuthority.PullRequest
					!= aRequest.Diff.Authority.PullRequest
				|| aRequest.WorkbenchAuthority.Number
					!= aRequest.Diff.Authority.Number
				|| aRequest.Diff.Comparison.Right.Commit.algorithm() != algorithm
				|| core::normalize_diff_options(aRequest.Diff.Options)
					!= std::optional{ aRequest.Diff.Options }
				|| core::options_digest(aRequest.Diff.Options)
					!= std::optional{ aRequest.Diff.OptionsDigest }
				|| aRequest.Diff.Algorithm
					!= core::supported_diff_algorithm_revision
				|| (algorithm != core::git_hash_algorithm::sha1
					&& algorithm != core::git_hash_algorithm::sha256)) {
				return std::nullopt;
			}
			const auto& authority = aRequest.Diff.Authority;
			return cache_actor_key{
				.Profile         = authority.Profile,
				.ProfileRevision = authority.ProfileRevision,
				.Host            = authority.Host,
				.HostAuthority   = authority.HostAuthority,
				.Account         = authority.Account,
				.Repository      = authority.Repository,
				.ObjectFormat    = algorithm,
				.Mode            = authority.Mode,
			};
		}

		template <typename Identity>
		[[nodiscard]] auto mint_identity(epoch_entropy_port& aEntropy)
			-> std::optional<Identity>
		{
			const auto bytes = aEntropy.generate();
			if (!bytes || !application::valid(*bytes)) {
				return std::nullopt;
			}
			return Identity{ *bytes };
		}

		// Captured by one bounded worker job; no host or presentation lifetime is retained.
		struct epoch_diagnostics
		{
			std::shared_ptr<diagnostic_log_port> Log;
			std::uint64_t                        Request       = 0U;
			std::uint64_t                        WindowSession = 0U;
			std::uint64_t                        Generation    = Log ? Log->context_generation() : 0U;

			void record(diagnostic_epoch_stage aStage, diagnostic_outcome aOutcome,
						diagnostic_epoch_failure               aFailure = { },
						std::optional<git_pack_stream_failure> aPack    = { },
						git_smart_transport_step               aStep    = git_smart_transport_step::none) const noexcept
			{
				if (Log) {
					static_cast<void>(Log->record({ .Operation         = diagnostic_operation::epoch,
													.Outcome           = aOutcome,
													.Request           = Request,
													.WindowSession     = WindowSession,
													.EpochStage        = aStage,
													.EpochFailure      = std::move(aFailure),
													.EpochPackFailure  = aPack,
													.GitStep           = aStep,
													.ContextGeneration = Generation }));
				}
			}
			template <typename Failure>
			void failed(diagnostic_epoch_stage aStage, const Failure aFailure) const noexcept
			{
				record(aStage, diagnostic_outcome::failed, aFailure);
			}
			void failed(diagnostic_epoch_stage aStage, const git_cache_error& aFailure) const noexcept
			{
				failed(aStage, aFailure.Code);
			}
			void failed(diagnostic_epoch_stage aStage, const git_smart_transport_error& aFailure) const noexcept
			{
				record(aStage, diagnostic_outcome::failed, aFailure.Failure, { }, aFailure.Step);
			}
			void failed(diagnostic_epoch_stage aStage, const git_protocol_v2_error& aFailure) const noexcept
			{
				record(aStage, diagnostic_outcome::failed, aFailure.Failure, aFailure.PackFailure);
			}
			template <typename... Failure>
			void failed(diagnostic_epoch_stage aStage, const std::variant<Failure...>& aFailure) const noexcept
			{
				std::visit([&](const auto& aValue) { failed(aStage, aValue); }, aFailure);
			}
			template <typename Effect>
			auto perform(diagnostic_epoch_stage aStage, Effect&& aEffect) const -> decltype(aEffect())
			{
				record(aStage, diagnostic_outcome::started);
				try {
					auto result = aEffect();
					if (result) {
						record(aStage, diagnostic_outcome::succeeded);
					} else {
						failed(aStage, result.error());
					}
					return result;
				} catch (...) {
					failed(aStage, core::epoch_failure::unavailable);
					throw;
				}
			}
		};

		[[nodiscard]] auto catalog_failure(
			const epoch_catalog_failure aFailure) noexcept -> core::epoch_failure
		{
			switch (aFailure) {
			case epoch_catalog_failure::cancelled:
				return core::epoch_failure::unavailable;
			case epoch_catalog_failure::quota_exceeded:
				return core::epoch_failure::quota_exceeded;
			case epoch_catalog_failure::ordinal_exhausted:
				return core::epoch_failure::ordinal_exhausted;
			case epoch_catalog_failure::revision_conflict:
			case epoch_catalog_failure::catalog_mismatch:
			case epoch_catalog_failure::authority_mismatch:
			case epoch_catalog_failure::operation_identity_conflict:
			case epoch_catalog_failure::source_observation_conflict:
			case epoch_catalog_failure::operation_conflict:
			case epoch_catalog_failure::phase_conflict:
			case epoch_catalog_failure::busy:
				return core::epoch_failure::catalog_conflict;
			case epoch_catalog_failure::schema_mismatch:
			case epoch_catalog_failure::integrity_failure:
			case epoch_catalog_failure::repair_required:
				return core::epoch_failure::repair_required;
			case epoch_catalog_failure::invalid_request:
				return core::epoch_failure::malformed_observation;
			case epoch_catalog_failure::invalid_path:
			case epoch_catalog_failure::invalid_permissions:
			case epoch_catalog_failure::unavailable:
			case epoch_catalog_failure::platform_unsupported:
				return core::epoch_failure::unavailable;
			}
			return core::epoch_failure::unavailable;
		}

		[[nodiscard]] auto cache_failure(const git_cache_error& aFailure) noexcept
			-> core::epoch_failure
		{
			switch (aFailure.Code) {
			case git_cache_failure::cancelled:
				return core::epoch_failure::unavailable;
			case git_cache_failure::quota_exceeded:
				return core::epoch_failure::quota_exceeded;
			case git_cache_failure::repair_required:
			case git_cache_failure::invalid_manifest:
			case git_cache_failure::invalid_repository:
			case git_cache_failure::inventory_mismatch:
			case git_cache_failure::authorization_mismatch:
			case git_cache_failure::object_mismatch:
			case git_cache_failure::ref_conflict:
				return core::epoch_failure::repair_required;
			case git_cache_failure::invalid_request:
			case git_cache_failure::invalid_authority:
				return core::epoch_failure::malformed_observation;
			case git_cache_failure::process_failure:
				// The Git subprocess failed. BPR-0348 retains what it said in
				// `git_cache_error::Diagnostic`; this is the part of that
				// answer the core can carry, and it is enough to say which
				// layer to look at.
				return core::epoch_failure::process_failure;
			case git_cache_failure::unsupported_object:
				// [[BPR-0880]] Git refused an object its own rules call an
				// error. Repair cannot help and neither can a retry: the code
				// says so, and the class Git named stays in the adapter.
				return core::epoch_failure::unsupported_object;
			case git_cache_failure::invalid_path:
			case git_cache_failure::lock_unavailable:
			case git_cache_failure::spool_failure:
				return core::epoch_failure::unavailable;
			}
			return core::epoch_failure::unavailable;
		}

		[[nodiscard]] auto smart_transport_failure(const git_smart_transport_error& aFailure) noexcept
			-> core::epoch_failure
		{
			switch (aFailure.Step) {
			case git_smart_transport_step::git_version:
			case git_smart_transport_step::git_capability_probe:
				return core::epoch_failure::git_tool_probe_failed;
			case git_smart_transport_step::git_qualification:
				return core::epoch_failure::git_tool_unsupported;
			case git_smart_transport_step::cli_version:
			case git_smart_transport_step::cli_capability_probe:
				return core::epoch_failure::gh_tool_probe_failed;
			case git_smart_transport_step::cli_qualification:
				return core::epoch_failure::gh_tool_unsupported;
			default:
				return core::epoch_failure::unavailable;
			}
		}

		[[nodiscard]] auto catalog_key(
			const accepted_pull_request_handoff& aHandoff)
			-> epoch_catalog_authority_key
		{
			return {
				.Profile         = aHandoff.Profile,
				.ProfileRevision = aHandoff.ProfileRevision,
				.Host            = aHandoff.Host,
				.HostAuthority   = aHandoff.HostAuthority,
				.Account         = aHandoff.Account,
				.Viewer          = aHandoff.Viewer,
				.Repository      = aHandoff.Repository,
				.PullRequest     = aHandoff.PullRequest,
				.Number          = aHandoff.Number,
				.ObjectFormat    = aHandoff.ObjectFormat,
				// The mode is part of the catalog key, so a signed-out read
				// opens its own scope rather than reusing an account's.
				.Mode = aHandoff.Mode,
			};
		}

		[[nodiscard]] auto agrees(
			const core::pull_request_authority&  aAuthority,
			const accepted_pull_request_handoff& aHandoff) noexcept -> bool
		{
			return aAuthority.Profile == aHandoff.Profile
				&& aAuthority.ProfileRevision == aHandoff.ProfileRevision
				&& aAuthority.Host == aHandoff.Host
				&& aAuthority.HostAuthority == aHandoff.HostAuthority
				&& aAuthority.Account == aHandoff.Account
				&& aAuthority.Viewer == aHandoff.Viewer
				&& aAuthority.Repository == aHandoff.Repository
				&& aAuthority.PullRequest == aHandoff.PullRequest
				&& aAuthority.Number == aHandoff.Number
				&& aAuthority.Mode == aHandoff.Mode;
		}

		[[nodiscard]] auto accepted_revision(
			const accepted_pull_request_handoff&    aHandoff,
			const core::epoch_source_observation_id aObservation)
			-> core::accepted_pull_request_revision
		{
			return {
				.Source = {
					.Observation           = aObservation,
					.Snapshot              = aHandoff.Snapshot,
					.IdentityGeneration    = aHandoff.IdentityGeneration,
					.PullRequestGeneration = aHandoff.PullRequestGeneration,
				},
				.Origin              = aHandoff.Origin,
				.BaseBranch          = aHandoff.BaseBranch,
				.Base                = aHandoff.Base,
				.Head                = aHandoff.Head,
				.CurrentViewerReview = aHandoff.CurrentViewerReview,
			};
		}

		[[nodiscard]] static auto reconcile_timeline(
			const accepted_pull_request_handoff&  aHandoff,
			core::epoch_occurrence_history        aHistory,
			const core::observed_utc_milliseconds aFirstSeen)
			-> core::epoch_occurrence_history
		{
			// A REST timeline does not contain ordinary push boundaries. Retain
			// locally observed occurrences and report the missing boundary even
			// when this bounded read exhausted all of its pages.
			aHistory.Incomplete = true;
			if (aHandoff.TimelineAvailability != epoch_timeline_availability::available) {
				return aHistory;
			}
			for (const auto& event : aHandoff.ForcePushes) {
				const auto evidence = core::epoch_occurrence_evidence{
					.Kind    = core::epoch_occurrence_kind::force_push_event,
					.EventId = event.EventId,
					.Head    = event.Head,
				};
				const auto id = core::make_epoch_occurrence_id(
					aHandoff.Hostname.value(), aHandoff.PullRequest, evidence);
				if (!id) {
					aHistory.Ambiguous = true;
					continue;
				}
				const auto found = std::ranges::find_if(
					aHistory.Occurrences, [id](const auto& aOccurrence) {
						return aOccurrence.Id == *id;
					});
				if (found != aHistory.Occurrences.end()) {
					if (found->Evidence != evidence
						|| found->DateSource != core::epoch_occurrence_date_source::github_event
						|| found->DateUtcMilliseconds != event.EventUtcMilliseconds) {
						aHistory.Ambiguous = true;
					}
					continue;
				}
				if (aHistory.Occurrences.size() >= core::maximum_epoch_occurrence_count) {
					aHistory.Ambiguous = true;
					break;
				}
				aHistory.Occurrences.push_back(core::epoch_occurrence{
					.Id                  = *id,
					.Evidence            = evidence,
					.DateSource          = core::epoch_occurrence_date_source::github_event,
					.DateUtcMilliseconds = event.EventUtcMilliseconds,
					.FirstSeen           = aFirstSeen,
				});
			}
			std::ranges::sort(aHistory.Occurrences, core::epoch_occurrence_less);
			return aHistory;
		}

		[[nodiscard]] auto evidenced_live_update(const core::epoch_cause& aCause) noexcept -> bool
		{
			return aCause.Origin == core::epoch_origin::initial_observation
				|| (aCause.Origin == core::epoch_origin::live_observation
					&& (aCause.Head == core::epoch_head_transition::descendant
						|| aCause.Head == core::epoch_head_transition::non_fast_forward
						|| aCause.Head == core::epoch_head_transition::lineage_unknown));
		}

		// One exact bounded scan detects previously observed heads. Failed or
		// capped history cannot prove that an observed-head key is unused.
		[[nodiscard]] auto read_occurrence_epochs(epoch_catalog_session& aCatalog,
												  const std::stop_token  aStop) -> std::optional<std::vector<core::review_epoch_summary>>
		{
			const auto binding   = aCatalog.binding();
			auto       request   = epoch_history_page_request{ .Catalog = binding.Catalog };
			auto       epochs    = std::vector<core::review_epoch_summary>{ };
			auto       remaining = core::maximum_epoch_occurrence_count;
			while (remaining != 0U && !aStop.stop_requested()) {
				const auto page = aCatalog.history(request, aStop);
				if (!page || page->Authority != binding.Authority.Scope
					|| page->Catalog != binding.Catalog || page->Epochs.size() > request.Limit
					|| page->Epochs.size() > remaining) {
					return std::nullopt;
				}
				auto before = request.Cursor ? std::optional{ request.Cursor->Before } : std::nullopt;
				for (const auto& epoch : page->Epochs) {
					if (!core::valid(epoch.Id) || epoch.Id.Authority != binding.Authority.Scope
						|| epoch.Id.Incarnation != binding.Catalog.Incarnation
						|| !core::valid(epoch.ObservedAt) || !epoch.Head.valid()
						|| (before && epoch.Id.Ordinal >= *before)) {
						return std::nullopt;
					}
					before = epoch.Id.Ordinal;
					epochs.push_back(epoch);
				}
				if (!page->Continuation) {
					return epochs;
				}
				remaining -= page->Epochs.size();
				if (page->Epochs.empty() || page->Continuation->Before != before) {
					return std::nullopt;
				}
				request.Cursor = page->Continuation;
				if (!application::valid(binding, request)) {
					return std::nullopt;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto record_live_occurrence(
			const accepted_pull_request_handoff&              aHandoff,
			core::epoch_occurrence_history                    aHistory,
			const core::review_epoch_summary&                 aEpoch,
			const std::span<const core::review_epoch_summary> aEpochs)
			-> core::epoch_occurrence_history
		{
			aHistory.Incomplete = true;
			if (!evidenced_live_update(aEpoch.Cause)) {
				return aHistory;
			}
			const auto existingLocal = std::ranges::find_if(aHistory.Occurrences, [&](const auto& aOccurrence) {
				return aOccurrence.LocalEpoch == aEpoch.Id;
			});
			// REST supplies an event's after-head, not a shared occurrence
			// discriminator for a local observation. Head equality, event order,
			// discovery order and either clock cannot authorize a new binding
			// or supersession. Retain both kinds of evidence with explicit ambiguity.
			if (std::ranges::any_of(aHistory.Occurrences, [&](const auto& aOccurrence) {
					return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event
						&& aOccurrence.Evidence.Head == aEpoch.Head
						&& (existingLocal == aHistory.Occurrences.end() || aOccurrence.Id != existingLocal->Id);
				})) {
				aHistory.Ambiguous = true;
			}
			if (existingLocal != aHistory.Occurrences.end()) {
				// Existing durable bindings are preserved; richer evidence is
				// required before the catalogue's exact supersession CAS is used.
				return aHistory;
			}
			if (aEpoch.Cause.Origin == core::epoch_origin::initial_observation) {
				return aHistory;
			}
			if (std::ranges::any_of(aEpochs, [&](const auto& aOther) {
					return aOther.Id != aEpoch.Id && aOther.Head == aEpoch.Head && evidenced_live_update(aOther.Cause);
				})) {
				// A repeated observed-head key cannot distinguish another return.
				// Its local epoch remains intact and provisional in the projection.
				aHistory.Ambiguous = true;
				return aHistory;
			}
			const auto evidence = core::epoch_occurrence_evidence{
				.Kind = core::epoch_occurrence_kind::observed_head,
				.Head = aEpoch.Head,
			};
			const auto id = core::make_epoch_occurrence_id(aHandoff.Hostname.value(), aHandoff.PullRequest, evidence);
			if (!id || std::ranges::any_of(aHistory.Occurrences, [&](const auto& aOccurrence) {
					return aOccurrence.Id == *id;
				})
				|| aHistory.Occurrences.size() >= core::maximum_epoch_occurrence_count) {
				aHistory.Ambiguous = true;
				return aHistory;
			}
			aHistory.Occurrences.push_back(core::epoch_occurrence{
				.Id             = *id,
				.Evidence       = evidence,
				.LocalEpoch     = aEpoch.Id,
				.HistoricalBase = aEpoch.Base,
				.FirstSeen      = aEpoch.ObservedAt,
			});
			return aHistory;
		}

		[[nodiscard]] auto reconcile_local_occurrences(const accepted_pull_request_handoff& aHandoff,
													   core::epoch_occurrence_history aHistory, epoch_catalog_session& aCatalog,
													   const std::optional<core::review_epoch_summary>& aCurrent, const std::stop_token aStop)
			-> core::epoch_occurrence_history
		{
			auto targets = std::vector<core::review_epoch_id>{ };
			for (const auto& occurrence : aHistory.Occurrences) {
				if (occurrence.Evidence.Kind == core::epoch_occurrence_kind::observed_head && occurrence.LocalEpoch) {
					targets.push_back(*occurrence.LocalEpoch);
				}
			}
			if (aCurrent && aCurrent->Head == aHandoff.Head
				&& std::ranges::find(targets, aCurrent->Id) == targets.end()
				&& std::ranges::none_of(aHistory.Occurrences, [&](const auto& aOccurrence) {
					   return aOccurrence.LocalEpoch == aCurrent->Id
						   && aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event;
				   })) {
				targets.push_back(aCurrent->Id);
			}
			if (targets.empty()) {
				return aHistory;
			}
			const auto epochs = read_occurrence_epochs(aCatalog, aStop);
			if (!epochs) {
				aHistory.Ambiguous = true;
				return aHistory;
			}
			// Revisit older observations too: delayed event evidence can make
			// their occurrence equivalence ambiguous after the live head moves.
			for (const auto& target : targets) {
				const auto epoch = std::ranges::find(*epochs, target, &core::review_epoch_summary::Id);
				if (epoch == epochs->end()) {
					aHistory.Ambiguous = true;
					continue;
				}
				aHistory = record_live_occurrence(aHandoff, std::move(aHistory), *epoch, *epochs);
			}
			return aHistory;
		}

		void enrich_commit_dates(core::epoch_occurrence_history& aHistory,
								 git_cache_session* const aCache, const std::stop_token aStop)
		{
			if (aCache == nullptr) {
				return;
			}
			for (auto& occurrence : aHistory.Occurrences) {
				if (aStop.stop_requested()) {
					return;
				}
				if (occurrence.DateSource != core::epoch_occurrence_date_source::unavailable) {
					continue;
				}
				const auto seconds = aCache->read_committer_time_seconds(occurrence.Evidence.Head, aStop);
				if (seconds && *seconds && **seconds >= -62'135'596'800LL
					&& **seconds <= core::maximum_observed_utc_milliseconds / 1'000) {
					occurrence.DateSource          = core::epoch_occurrence_date_source::commit_date;
					occurrence.DateUtcMilliseconds = **seconds * 1'000;
				}
			}
		}

		[[nodiscard]] auto invalidation_identity(
			const accepted_pull_request_handoff& aHandoff)
			-> pull_request_authority_invalidation
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
				.PullRequest        = aHandoff.PullRequest,
				.IdentityGeneration = aHandoff.IdentityGeneration,
				.Mode               = aHandoff.Mode,
			};
		}

		[[nodiscard]] auto projected_revision(
			const core::accepted_pull_request_revision& aRevision)
			-> epoch_revision_projection
		{
			return {
				.Snapshot              = aRevision.Source.Snapshot,
				.IdentityGeneration    = aRevision.Source.IdentityGeneration,
				.PullRequestGeneration = aRevision.Source.PullRequestGeneration,
				.Origin                = aRevision.Origin,
				.BaseBranch            = aRevision.BaseBranch,
				.Base                  = aRevision.Base,
				.Head                  = aRevision.Head,
				.CurrentViewerReview   = aRevision.CurrentViewerReview,
			};
		}

		[[nodiscard]] auto projected_row(const core::review_epoch_summary& aEpoch)
			-> epoch_row_projection
		{
			return {
				.Id                    = aEpoch.Id,
				.Snapshot              = aEpoch.Source.Snapshot,
				.IdentityGeneration    = aEpoch.Source.IdentityGeneration,
				.PullRequestGeneration = aEpoch.Source.PullRequestGeneration,
				.BaseBranch            = aEpoch.BaseBranch,
				.Base                  = aEpoch.Base,
				.Head                  = aEpoch.Head,
				.Material              = aEpoch.Material,
				.Cause                 = aEpoch.Cause,
				.ObservedAt            = aEpoch.ObservedAt,
				.BaseAvailability      = aEpoch.BaseAvailability,
				.HeadAvailability      = aEpoch.HeadAvailability,
			};
		}

		[[nodiscard]] auto review_anchor(
			const core::pull_request_authority&     aAuthority,
			const core::epoch_catalog_identity      aCatalog,
			const core::catalog_review_association& aReview)
			-> core::comparison_anchor
		{
			return {
				.Identity = {
					.Authority   = aAuthority.Scope,
					.Incarnation = aCatalog.Incarnation,
					.Kind        = core::comparison_anchor_kind::review_commit,
					.Review      = aReview.Association,
				},
				.Commit       = aReview.Commit,
				.Availability = aReview.Availability,
			};
		}

		[[nodiscard]] auto recovery_request(const append_operation_record& aRecord)
			-> std::optional<git_operation_recovery_request>
		{
			if (!application::valid(aRecord)) {
				return std::nullopt;
			}
			auto request = git_operation_recovery_request{
				.Operation = aRecord.Context.Operation,
				.Scope     = aRecord.StagingBase.Authority,
				.Ordinal   = aRecord.ReservedOrdinal,
				.Base      = aRecord.Context.Revision.Base,
				.Head      = aRecord.Context.Revision.Head,
				.Receiving = aRecord.Receiving,
			};
			if (aRecord.Spool && aRecord.Preimport) {
				request.Spool = git_recovery_spool_expectation{
					.ExpectedPack             = aRecord.Spool->ExpectedPack,
					.CompletedSpool           = aRecord.Spool->Spool,
					.PreimportPacks           = aRecord.Preimport->Packs,
					.PreimportRepositoryBytes = aRecord.Preimport->RepositoryBytes,
					.ExpectedPackPreexisting  = aRecord.Preimport->ExpectedPackPreexisting,
					.Residency                = aRecord.Spool->Residency,
				};
			}
			if (aRecord.Install) {
				request.Install = git_recovery_install_expectation{
					.Pack           = aRecord.Install->Pack,
					.InstalledPack  = aRecord.Install->InstalledPack,
					.InstalledIndex = aRecord.Install->InstalledIndex,
					.BaseTree       = aRecord.Install->BaseTree,
					.HeadTree       = aRecord.Install->HeadTree,
					.Residency      = aRecord.Install->Residency,
				};
			}
			return application::valid(request) ? std::optional{ std::move(request) }
											   : std::nullopt;
		}

		[[nodiscard]] auto recovered_prepared_revision(
			const append_operation_record& aRecord)
			-> std::optional<git_prepared_revision>
		{
			if (!aRecord.Install) {
				return std::nullopt;
			}
			auto prepared = git_prepared_revision{
				.Operation      = aRecord.Context.Operation,
				.Pack           = aRecord.Install->Pack,
				.InstalledPack  = aRecord.Install->InstalledPack,
				.InstalledIndex = aRecord.Install->InstalledIndex,
				.Base           = aRecord.Install->Base,
				.BaseTree       = aRecord.Install->BaseTree,
				.Head           = aRecord.Install->Head,
				.HeadTree       = aRecord.Install->HeadTree,
				.HeadLineage    = aRecord.Install->HeadLineage,
				.ImportedBytes  = aRecord.Install->InstalledPack.Bytes,
				.Residency      = aRecord.Install->Residency,
			};
			return application::valid(prepared) ? std::optional{ std::move(prepared) }
												: std::nullopt;
		}

		[[nodiscard]] auto expected_retained_pair(
			const append_operation_record& aRecord) -> git_retained_ref_pair
		{
			return {
				.Scope     = aRecord.FinalBase.Authority,
				.Ordinal   = aRecord.ReservedOrdinal,
				.Base      = aRecord.Context.Revision.Base,
				.Head      = aRecord.Context.Revision.Head,
				.Residency = aRecord.Install
					? aRecord.Install->Residency
					: git_revision_residency::complete_closure,
			};
		}

		[[nodiscard]] auto same_append_identity(
			const append_operation_record& aPrior,
			const append_operation_record& aNext) noexcept -> bool
		{
			return application::valid(aNext) && aNext.Context == aPrior.Context
				&& aNext.ReservedOrdinal == aPrior.ReservedOrdinal
				&& aNext.StagingBase == aPrior.StagingBase
				&& aNext.StagingHead == aPrior.StagingHead
				&& aNext.FinalBase == aPrior.FinalBase
				&& aNext.FinalHead == aPrior.FinalHead;
		}

		[[nodiscard]] auto same_maintenance_identity(
			const maintenance_operation_record& aPrior,
			const maintenance_operation_record& aNext) noexcept -> bool
		{
			return application::valid(aNext) && aNext.Incarnation == aPrior.Incarnation
				&& aNext.Operation == aPrior.Operation && aNext.Kind == aPrior.Kind
				&& aNext.Cache == aPrior.Cache && aNext.Epochs == aPrior.Epochs
				&& aNext.Scopes == aPrior.Scopes;
		}

		[[nodiscard]] auto exact_maintenance_transition(
			const maintenance_operation_record& aPrior,
			const maintenance_operation_record& aNext,
			const epoch_catalog_operation_phase aPhase) noexcept -> bool
		{
			return same_maintenance_identity(aPrior, aNext)
				&& application::can_advance(aPrior.OperationRevision)
				&& aNext.OperationRevision.value() == aPrior.OperationRevision.value() + 1
				&& aNext.Phase == aPhase
				&& (aPhase == epoch_catalog_operation_phase::repair_required
						? aNext.RepairFromPhase == std::optional{ aPrior.Phase }
						: !aNext.RepairFromPhase);
		}

		[[nodiscard]] auto exact_prune_intent(
			const maintenance_operation_record&   aRecord,
			const begin_prune_request&            aRequest,
			const core::repository_cache_scope_id aCache) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Incarnation == aRequest.Catalog.Incarnation
				&& aRecord.Operation == aRequest.Operation
				&& aRecord.Kind == epoch_catalog_operation_kind::prune
				&& aRecord.Phase == epoch_catalog_operation_phase::prune_intent
				&& aRecord.OperationRevision == epoch_catalog_operation_revision{ 1 }
			&& aRecord.Cache == aCache && aRecord.Epochs == aRequest.Epochs
				&& aRecord.Scopes.empty() && !aRecord.RepairFromPhase;
		}

		[[nodiscard]] auto exact_cache_delete_intent(
			const maintenance_operation_record& aRecord,
			const begin_cache_delete_request&   aRequest) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Incarnation == aRequest.Impact.Incarnation
				&& aRecord.Operation == aRequest.Operation
				&& aRecord.Kind == epoch_catalog_operation_kind::cache_delete
				&& aRecord.Phase
				== epoch_catalog_operation_phase::cache_delete_intent_sealed
				&& aRecord.OperationRevision == epoch_catalog_operation_revision{ 1 }
			&& aRecord.Cache == aRequest.Impact.Cache && aRecord.Epochs.empty()
				&& aRecord.Scopes == aRequest.Impact.Scopes && !aRecord.RepairFromPhase;
		}

		[[nodiscard]] auto recovery_operation_identity(
			const epoch_recovery_operation& aOperation) noexcept
			-> core::epoch_operation_id
		{
			return std::visit(
				[](const auto& aValue) {
					using value_type = std::remove_cvref_t<decltype(aValue)>;
					if constexpr (
						std::is_same_v<value_type, append_operation_record>) {
						return aValue.Context.Operation;
					} else {
						return aValue.Operation;
					}
				},
				aOperation);
		}

		[[nodiscard]] auto exact_append_reservation(
			const append_operation_record&              aRecord,
			const append_reservation_request&           aRequest,
			const core::pull_request_authority_scope_id aAuthority) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Context == aRequest.Context
				&& aRecord.ReservedOrdinal == aRequest.ProposedOrdinal
				&& aRecord.OperationRevision
				== epoch_catalog_operation_revision{ 1 }
			&& aRecord.Phase == epoch_catalog_operation_phase::preparing
				&& !aRecord.RepairFromPhase && !aRecord.RecoveryInputCompacted
				&& !aRecord.Receiving && !aRecord.Spool && !aRecord.Preimport && !aRecord.Install
				&& !aRecord.Candidate
				&& aRecord.StagingBase
				== epoch_catalog_ref_identity{
					   .Authority = aAuthority,
					   .Ordinal   = aRequest.ProposedOrdinal,
					   .Operation = aRequest.Context.Operation,
					   .Staging   = true,
				   }
			&& aRecord.StagingHead == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
				   .Operation = aRequest.Context.Operation,
				   .Staging   = true,
				   .Head      = true,
			   }
			&& aRecord.FinalBase == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
			   }
			&& aRecord.FinalHead == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
				   .Head      = true,
			   };
		}

		[[nodiscard]] auto exact_append_transition(
			const append_operation_record&      aPrior,
			const append_operation_record&      aNext,
			const epoch_catalog_operation_phase aPhase) noexcept -> bool
		{
			if (
				!same_append_identity(aPrior, aNext)
				|| !application::can_advance(aPrior.OperationRevision)
				|| aNext.OperationRevision.value()
					!= aPrior.OperationRevision.value() + 1
				|| aNext.Phase != aPhase) {
				return false;
			}
			const auto terminal = aPhase == epoch_catalog_operation_phase::complete
				|| aPhase == epoch_catalog_operation_phase::aborted;
			const auto exactRepairOrigin = aPhase == epoch_catalog_operation_phase::repair_required
				? aNext.RepairFromPhase == std::optional{ aPrior.Phase }
				: !aNext.RepairFromPhase;
			return exactRepairOrigin && aNext.Install == aPrior.Install
				&& aNext.Candidate == aPrior.Candidate
				&& (terminal
						? aNext.RecoveryInputCompacted && !aNext.Receiving && !aNext.Spool
							&& !aNext.Preimport
						: !aNext.RecoveryInputCompacted
							&& aNext.Receiving == aPrior.Receiving
							&& aNext.Spool == aPrior.Spool
							&& aNext.Preimport == aPrior.Preimport);
		}

		[[nodiscard]] auto exact_spool_record(
			const append_operation_record& aPrior,
			const append_operation_record& aNext,
			const record_spool_request&    aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase == epoch_catalog_operation_phase::spool_recorded
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Receiving == aPrior.Receiving
				&& aNext.Spool == std::optional{ aRequest.Spool }
			&& aNext.Preimport == std::optional{ aRequest.Preimport }
			&& aNext.Install == aPrior.Install
				&& aNext.Candidate == aPrior.Candidate;
		}

		[[nodiscard]] auto exact_install_record(
			const append_operation_record& aPrior,
			const append_operation_record& aNext,
			const record_install_request&  aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase == epoch_catalog_operation_phase::imported
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Receiving == aPrior.Receiving
				&& aNext.Spool == aPrior.Spool && aNext.Preimport == aPrior.Preimport
				&& aNext.Install == std::optional{ aRequest.Install }
			&& aNext.Candidate == aPrior.Candidate;
		}

		[[nodiscard]] auto exact_prepared_revision(
			const git_prepared_revision&        aPrepared,
			const git_prepare_revision_request& aRequest,
			const git_revision_residency        aResidency) noexcept -> bool
		{
			return application::valid(aPrepared)
				&& aPrepared.Residency == aResidency
				&& aPrepared.Operation == aRequest.Authorization.Operation
				&& aPrepared.Pack == aRequest.Authorization.ExpectedPack
				&& aPrepared.Base == aRequest.Base && aPrepared.Head == aRequest.Head
				&& aPrepared.ImportedBytes == aRequest.Receipt.Spool.Bytes
				&& aPrepared.InstalledPack.Bytes == aRequest.Receipt.Spool.Bytes;
		}

		[[nodiscard]] auto exact_candidate_record(
			const append_operation_record&  aPrior,
			const append_operation_record&  aNext,
			const record_candidate_request& aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase
				== epoch_catalog_operation_phase::commit_authorized
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Receiving == aPrior.Receiving
				&& aNext.Spool == aPrior.Spool && aNext.Preimport == aPrior.Preimport
				&& aNext.Install == aPrior.Install
				&& aNext.Candidate == std::optional{ aRequest.Candidate };
		}

		[[nodiscard]] auto exact_catalog_commit(
			const catalog_commit_result&               aResult,
			const core::epoch_catalog_identity         aPrior,
			const std::optional<core::review_epoch_id> aInserted) noexcept -> bool
		{
			return core::valid(aResult.Catalog) && aResult.Changed
				&& aPrior.Revision.value() < core::maximum_epoch_catalog_revision
				&& aResult.Catalog.Incarnation == aPrior.Incarnation
				&& aResult.Catalog.Revision.value() == aPrior.Revision.value() + 1
				&& aResult.InsertedEpoch == aInserted;
		}
	} // namespace

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

		[[nodiscard]] static auto still_needed(const source_corpus_read_needed& aNeeded) noexcept -> bool
		{
			try {
				return aNeeded && aNeeded();
			} catch (...) {
				return false;
			}
		}

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
			explicit authority_effect_actor(std::shared_ptr<profile_epoch_actor> aShared)
				: Shared{ std::move(aShared) }
				, Cache{ Shared->cache() }
			{ }
			std::shared_ptr<profile_epoch_actor>     Shared;
			std::unique_ptr<git_cache_session>&      Cache;
			std::mutex                               BlobAuthorityMutex;
			std::optional<core::source_search_scope> SearchScope;
			std::shared_ptr<const source_corpus>     SearchCorpus;
			std::shared_ptr<corpus_read_waiter>      SearchRead;
			std::uint64_t                            SearchGeneration = 0U;
			std::stop_source                         SearchScanStop;
			std::optional<blob_authority>            BlobAuthority;
			std::weak_ptr<authority_context>         BlobContext;
			std::optional<selected_diff_authority>   SelectedDiffAuthority;
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
		};

		/// Drops every prepared grant. Called under BlobAuthorityMutex.
		static void forget_prepared_grants(authority_effect_actor& aActor) noexcept
		{
			aActor.PreparedGrants.clear();
			++aActor.PreparedGrantSequence;
			aActor.PreparedReadsSuspended = false;
		}

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
			explicit reviewer_work(std::shared_ptr<authority_effect_actor> aActor)
				: Actor{ std::move(aActor) }
			{
				std::scoped_lock lock{ Actor->BlobAuthorityMutex };
				++Actor->ReviewerWork;
				Actor->SearchScanStop.request_stop();
				if (Actor->SearchRead) {
					Actor->SearchRead->Stop.request_stop();
				}
				if (Actor->PreparedRead) {
					static_cast<void>(Actor->PreparedRead->request_stop());
				}
			}

			~reviewer_work()
			{
				std::scoped_lock lock{ Actor->BlobAuthorityMutex };
				--Actor->ReviewerWork;
				if (Actor->ReviewerWork == 0U) {
					Actor->SearchScanStop = std::stop_source{ };
				}
			}

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
			prepared_read_registration(authority_effect_actor& aActor, const std::stop_source& aStop, const bool aHonorSuspension = true)
				: Actor{ aActor }
			{
				std::scoped_lock lock{ Actor.BlobAuthorityMutex };
				Suspended = aHonorSuspension && Actor.PreparedReadsSuspended;
				Admitted  = !Suspended && Actor.ReviewerWork == 0U;
				if (Admitted) {
					Actor.PreparedRead = aStop;
				}
			}

			~prepared_read_registration()
			{
				if (Admitted) {
					std::scoped_lock lock{ Actor.BlobAuthorityMutex };
					Actor.PreparedRead.reset();
				}
			}

			prepared_read_registration(const prepared_read_registration&)                    = delete;
			auto operator=(const prepared_read_registration&) -> prepared_read_registration& = delete;
			prepared_read_registration(prepared_read_registration&&)                         = delete;
			auto operator=(prepared_read_registration&&) -> prepared_read_registration&      = delete;

			[[nodiscard]] auto admitted() const noexcept -> bool
			{
				return Admitted;
			}

			[[nodiscard]] auto suspended() const noexcept -> bool
			{
				return Suspended;
			}

		private:
			authority_effect_actor& Actor;
			bool                    Admitted  = false;
			bool                    Suspended = false;
		};

		/// Drops every retained blob. Called under BlobAuthorityMutex.
		static void forget_retained_blobs(authority_effect_actor& aActor) noexcept
		{
			aActor.RetainedBlobs.clear();
			aActor.RetainedBlobBytes = 0U;
		}

		/// Retains verified bytes within the actor's bound, oldest first out.
		/// Called under BlobAuthorityMutex.
		static void retain_blob(authority_effect_actor& aActor, const core::git_blob_oid& aBlob,
								const std::vector<std::uint8_t>& aBytes, const bool aBackground = false)
		{
			if (aBytes.size() > maximum_retained_sparse_blob_bytes) {
				return;
			}
			const auto retained = std::ranges::find(aActor.RetainedBlobs, aBlob, &retained_sparse_blob::Blob);
			if (retained != aActor.RetainedBlobs.end()) {
				if (!aBackground) {
					retained->Background = false;
				}
				return;
			}
			while (!aActor.RetainedBlobs.empty()
				   && (aActor.RetainedBlobs.size() >= maximum_retained_sparse_blobs
					   || aActor.RetainedBlobBytes > maximum_retained_sparse_blob_bytes - aBytes.size())) {
				const auto victim = std::ranges::find(aActor.RetainedBlobs, true, &retained_sparse_blob::Background);
				if (victim != aActor.RetainedBlobs.end()) {
					aActor.RetainedBlobBytes -= victim->Bytes.size();
					aActor.RetainedBlobs.erase(victim);
				} else if (aBackground) {
					return;
				} else {
					aActor.RetainedBlobBytes -= aActor.RetainedBlobs.front().Bytes.size();
					aActor.RetainedBlobs.pop_front();
				}
			}
			aActor.RetainedBlobs.push_back(retained_sparse_blob{ .Blob = aBlob, .Bytes = aBytes, .Background = aBackground });
			aActor.RetainedBlobBytes += aBytes.size();
		}

		[[nodiscard]] static auto retained_blob(const authority_effect_actor& aActor, const core::git_blob_oid& aBlob)
			-> std::optional<std::vector<std::uint8_t>>
		{
			const auto found = std::ranges::find(aActor.RetainedBlobs, aBlob, &retained_sparse_blob::Blob);
			return found == aActor.RetainedBlobs.end() ? std::nullopt : std::optional{ found->Bytes };
		}

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
			explicit actor_blob_read(const diff_blob_request& aRequest)
				: Request{ aRequest }
			{
				Observation.Context = aRequest.Context;
				Observation.Side    = aRequest.Side;
				Observation.Blob    = aRequest.Blob;
				Observation.Status  = diff_blob_status::failure;
			}

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
				std::optional<core::review_epoch_summary>     aCurrentEpoch)
				: Handoff{ std::move(aAccepted) }
				, Authority{ std::move(aAuthority) }
				, Revision{ std::move(aRevision) }
				, Catalog{ std::move(aCatalog) }
				, Actor{ std::move(aActor) }
				, OccurrenceHistory{ std::move(aOccurrenceHistory) }
				, CurrentEpoch{ std::move(aCurrentEpoch) }
			{
				Preparations.reserve(maximum_pending_preparations);
			}

			~authority_context()
			{
				if (Handoff.SmartGit && !Handoff.SmartGit->revoked()) {
					Handoff.SmartGit->revoke();
				}
			}

			[[nodiscard]] auto store(prepared_operation aOperation) -> bool
			{
				if (
					Preparations.size() >= maximum_pending_preparations
					|| std::ranges::any_of(
						Preparations,
						[&](const auto& aExisting) {
							return aExisting.Operation == aOperation.Operation;
						})) {
					return false;
				}
				Preparations.push_back(std::move(aOperation));
				return true;
			}

			[[nodiscard]] auto take(const core::epoch_operation_id aOperation)
				-> std::optional<prepared_operation>
			{
				const auto found = std::ranges::find(
					Preparations,
					aOperation,
					&prepared_operation::Operation);
				if (found == Preparations.end()) {
					return std::nullopt;
				}
				auto result = std::move(*found);
				Preparations.erase(found);
				return result;
			}

			[[nodiscard]] auto requires_cache_cleanup(
				const std::optional<core::epoch_operation_id> aExpected = std::nullopt)
				const noexcept -> bool
			{
				return std::ranges::any_of(
					Preparations,
					[&](const auto& aOperation) {
						return aOperation.Kind == prepared_kind::append
							&& (!aExpected || aOperation.Operation == *aExpected);
					});
			}

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
			std::shared_ptr<profile_epoch_actor_consumer> aConsumer, const bool aLegacy = false)
			: State{ std::move(aInitialState) }
			, Caches{ aCaches }
			, Catalogs{ aCatalogs }
			, Entropy{ aEntropy }
			, Clock{ aClock }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projections{ aProjections }
			, Limits{ validated_limits(aLimits) }
			, DiagnosticLog{ std::move(aDiagnosticLog) }
			, CompletionGate{ std::make_shared<completion_gate>() }
			, Consumer{ std::move(aConsumer) }
			, WindowApplicationSession{ State.ApplicationSession }
			, Legacy{ aLegacy }
		{
			Jobs.reserve(Limits.OutstandingJobs);
			CompletionGate->Host.store(this, std::memory_order_release);
		}

		~implementation()
		{
			const std::lock_guard barrier{ CompletionGate->Barrier };
			CompletionGate->Host.store(nullptr, std::memory_order_release);
		}

		[[nodiscard]] static auto blob_authority_for(
			const authority_context& aContext) noexcept -> blob_authority
		{
			return {
				.ApplicationSession = aContext.Handoff.ApplicationSession,
				.Authority          = aContext.Authority,
			};
		}

		[[nodiscard]] static auto blob_authority_for(
			const workbench_selected_diff_request& aRequest) noexcept
			-> blob_authority
		{
			return {
				.ApplicationSession = aRequest.Diff.ApplicationSession,
				.Authority          = aRequest.Diff.Authority,
			};
		}

		[[nodiscard]] static auto blob_authority_for(
			const diff_blob_request& aRequest) noexcept -> blob_authority
		{
			return {
				.ApplicationSession = aRequest.Context.ApplicationSession,
				.Authority          = aRequest.Context.Key.Authority,
			};
		}

		[[nodiscard]] static auto blob_authority_for(
			const agent_workspace_materialization_request& aRequest) noexcept
			-> blob_authority
		{
			return {
				.ApplicationSession = aRequest.ApplicationSession,
				.Authority          = aRequest.Authority,
			};
		}

		[[nodiscard]] static auto exact_selected_blob_request(
			const workbench_selected_diff_observation& aGrant,
			const diff_blob_request&                   aRequest) noexcept -> bool
		{
			if (aGrant.Status != workbench_selected_diff_status::available) {
				return false;
			}
			const auto& selected = aGrant.Request;
			const auto& key      = aRequest.Context.Key;
			return aRequest.Context.ApplicationSession == selected.Diff.ApplicationSession
				&& key.Authority == selected.Diff.Authority
				&& key.Repository == selected.Diff.Authority.Repository
				&& key.PullRequest == selected.Diff.Authority.PullRequest
				&& key.LeftAnchor == selected.Diff.Comparison.Left.Identity
				&& key.RightAnchor == selected.Diff.Comparison.Right.Identity
				&& key.LeftBlob == aGrant.LeftBlob
				&& key.RightBlob == aGrant.RightBlob
				&& key.File == selected.File.File
				&& key.Options == selected.Diff.OptionsDigest
				&& key.Algorithm == selected.Diff.Algorithm
				&& (aRequest.Side == core::diff_side::left
						? aGrant.LeftBlob == std::optional{ aRequest.Blob }
						: aGrant.RightBlob == std::optional{ aRequest.Blob });
		}

		static void authorize_blobs(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const blob_authority&                          aAuthority,
			const std::shared_ptr<authority_context>&      aContext) noexcept
		{
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			++aActor->BlobAuthorityGeneration;
			aActor->SearchScope.reset();
			aActor->SearchCorpus.reset();
			if (aActor->SearchRead) {
				aActor->SearchRead->Stop.request_stop();
			}
			aActor->BlobAuthority = aAuthority;
			aActor->BlobContext   = aContext;
			aActor->SelectedDiffAuthority.reset();
			aActor->RealignmentAuthority.reset();
			aActor->SelectedBlobGrant.reset();
			++aActor->SelectedBlobGrantSequence;
			forget_prepared_grants(*aActor);
			forget_retained_blobs(*aActor);
		}

		static void authorize_selected_diff(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const selected_diff_authority&                 aAuthority) noexcept
		{
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			if (aActor->SelectedDiffAuthority == std::optional{ aAuthority }) {
				return;
			}
			++aActor->BlobAuthorityGeneration;
			aActor->SelectedDiffAuthority = aAuthority;
			aActor->SelectedBlobGrant.reset();
			++aActor->SelectedBlobGrantSequence;
			forget_prepared_grants(*aActor);
		}

		/// \brief Grants the one re-alignment authorization, without a bump.
		///
		/// \details Deliberately does not touch `BlobAuthorityGeneration`.
		/// The generation is what tells an in-flight resolve that the review
		/// moved under it, and a re-alignment is not the review moving; a bump
		/// here would refuse the reviewer's own selected diff for a turn they
		/// asked about a different revision. Replacement of the review's
		/// context still bumps, and still drops this.
		static void authorize_realignment(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const selected_diff_authority&                 aAuthority) noexcept
		{
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			aActor->RealignmentAuthority = aAuthority;
		}

		static void revoke_blob_authority(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const blob_authority&                          aAuthority) noexcept
		{
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			if (aActor->BlobAuthority == std::optional{ aAuthority }) {
				aActor->SearchScope.reset();
				aActor->SearchCorpus.reset();
				if (aActor->SearchRead) {
					aActor->SearchRead->Stop.request_stop();
				}
				aActor->BlobAuthority.reset();
				aActor->BlobContext.reset();
				aActor->SelectedBlobGrant.reset();
				++aActor->SelectedBlobGrantSequence;
				forget_prepared_grants(*aActor);
				forget_retained_blobs(*aActor);
				aActor->SelectedDiffAuthority.reset();
				aActor->RealignmentAuthority.reset();
				++aActor->BlobAuthorityGeneration;
			}
		}

		[[nodiscard]] static auto selected_diff_authorization_generation(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const workbench_selected_diff_request&         aRequest) noexcept
			-> std::optional<std::uint64_t>
		{
			const auto expected = selected_diff_authority{
				.Workbench = aRequest.WorkbenchAuthority,
				.Diff      = aRequest.Diff,
			};
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			// Either slot answers, and both answer with the same generation:
			// a re-alignment is admitted under the review's own generation, so
			// the review's context being replaced under it refuses it exactly
			// as it refuses the workbench's.
			if (aActor->SelectedDiffAuthority != std::optional{ expected }
				&& aActor->RealignmentAuthority != std::optional{ expected }) {
				return std::nullopt;
			}
			return aActor->BlobAuthorityGeneration;
		}

		[[nodiscard]] static auto blob_authorization_generation(
			const std::shared_ptr<authority_effect_actor>& aActor,
			const blob_authority&                          aAuthority) noexcept
			-> std::optional<std::uint64_t>
		{
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			if (aActor->BlobAuthority != std::optional{ aAuthority }) {
				return std::nullopt;
			}
			return aActor->BlobAuthorityGeneration;
		}

		static void revoke_deferred(deferred_ingress& aIngress) noexcept
		{
			if (auto* const accepted = std::get_if<deferred_accept>(&aIngress);
				accepted != nullptr && accepted->Handoff
				&& accepted->Handoff->SmartGit) {
				accepted->Handoff->SmartGit->revoke();
			}
		}

		void clear_deferred_ingress() noexcept
		{
			for (auto& ingress : DeferredIngress) {
				revoke_deferred(ingress);
			}
			DeferredIngress.clear();
		}

		void defer_diagnostic(const core::epoch_reason_code aReason) noexcept
		{
			if (!DeferredDiagnostic) {
				DeferredDiagnostic = aReason;
			}
		}

		[[nodiscard]] auto defer_ordinary(deferred_ingress aIngress) noexcept
			-> bool
		{
			if (DeferredShutdown || Closing) {
				revoke_deferred(aIngress);
				return false;
			}
			if (DeferredIngress.size() >= Limits.DeferredIngress) {
				revoke_deferred(aIngress);
				defer_diagnostic(core::epoch_reason_code::quota_exceeded);
				schedule_deferred_ingress();
				return false;
			}
			try {
				DeferredIngress.push_back(std::move(aIngress));
			} catch (...) {
				revoke_deferred(aIngress);
				defer_diagnostic(core::epoch_reason_code::unavailable);
				schedule_deferred_ingress();
				return false;
			}
			schedule_deferred_ingress();
			return true;
		}

		[[nodiscard]] auto defer_invalidation(
			const pull_request_authority_invalidation& aInvalidation) noexcept -> bool
		{
			if (DeferredShutdown || Closing) {
				return true;
			}
			for (auto ingress = DeferredIngress.begin();
				 ingress != DeferredIngress.end();) {
				auto* const accepted = std::get_if<deferred_accept>(&*ingress);
				if (accepted == nullptr || !accepted->Handoff
					|| invalidation_identity(*accepted->Handoff) != aInvalidation) {
					++ingress;
					continue;
				}
				revoke_deferred(*ingress);
				ingress = DeferredIngress.erase(ingress);
			}

			const auto activeMatches = Active
				&& invalidation_identity(Active->Handoff) == aInvalidation;
			const auto pendingMatches = PendingAuthority == aInvalidation;
			if (!activeMatches && !pendingMatches) {
				return true;
			}
			if (std::ranges::any_of(
					DeferredInvalidations,
					[&aInvalidation](const auto& aPending) {
						return aPending == aInvalidation;
					})) {
				return true;
			}
			const auto empty = std::ranges::find(
				DeferredInvalidations,
				std::optional<pull_request_authority_invalidation>{ });
			if (empty == DeferredInvalidations.end()) {
				ProjectionDeliveryFailed = true;
				return false;
			}
			*empty = aInvalidation;
			schedule_deferred_ingress();
			return true;
		}

		[[nodiscard]] auto deferred_invalidations_empty() const noexcept -> bool
		{
			return std::ranges::none_of(
				DeferredInvalidations,
				[](const auto& aPending) { return aPending.has_value(); });
		}

		[[nodiscard]] auto deferred_work_pending() const noexcept -> bool
		{
			return DeferredIngressScheduled || !DeferredIngress.empty()
				|| !deferred_invalidations_empty() || DeferredDiagnostic
				|| DeferredShutdown;
		}

		void clear_deferred_invalidations() noexcept
		{
			for (auto& invalidation : DeferredInvalidations) {
				invalidation.reset();
			}
		}

		void defer_shutdown() noexcept
		{
			DeferredShutdown = true;
			schedule_deferred_ingress();
		}

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

		void schedule_deferred_ingress() noexcept
		{
			if (
				(DeferredIngress.empty() && !DeferredShutdown
				 && !DeferredDiagnostic && deferred_invalidations_empty())
				|| DeferredIngressScheduled) {
				return;
			}
			DeferredIngressScheduled = true;
			const auto gate          = std::weak_ptr<completion_gate>{ CompletionGate };
			try {
				DecisionLane.post(
					[gate] {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard barrier{ locked->Barrier };
						auto* const           host = locked->Host.load(std::memory_order_acquire);
						if (host == nullptr) {
							return;
						}
						host->DeferredIngressScheduled = false;
						host->run_turn([] { });
					});
			} catch (...) {
				DeferredIngressScheduled = false;
				ProjectionDeliveryFailed = true;
			}
		}

		static void enqueue_completion(
			const std::weak_ptr<completion_gate>& aGate,
			decision_executor* const              aDecisionLane,
			std::shared_ptr<job_control>          aControl,
			worker_completion                     aCompletion) noexcept
		{
			aControl->Completion = std::move(aCompletion);
			auto task            = std::move(aControl->DecisionTask);
			if (!task) {
				if (const auto locked = aGate.lock()) {
					locked->DeliveryFailed.store(true, std::memory_order_release);
				}
				return;
			}
			try {
				aDecisionLane->post(std::move(task));
			} catch (...) {
				const auto locked = aGate.lock();
				if (!locked) {
					return;
				}
				// A conforming decision lane never rejects. Preserve the live
				// job (and therefore non-quiescence) instead of retiring it or
				// mutating core-owned state from this worker thread.
				locked->DeliveryFailed.store(true, std::memory_order_release);
			}
		}

		void enqueue_actor_work(
			const std::shared_ptr<authority_effect_actor>& aActor,
			std::function<void()>                          aWork,
			const profile_epoch_actor_priority             aPriority = profile_epoch_actor_priority::reviewer)
		{
			const auto workState = ActorWork;
			++workState->Pending;
			++workState->Queued;
			try {
				aActor->Shared->enqueue([aActor, workState, work = std::move(aWork)] {
					--workState->Queued;
					struct work_guard final
					{
						std::atomic_size_t& Pending;
						~work_guard() { --Pending; }
					} guard{ workState->Pending };
					work();
				},
										aPriority);
			} catch (...) {
				--workState->Queued;
				--workState->Pending;
				throw;
			}
		}

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
			std::shared_ptr<job_control>            aAdmittedControl      = { }) -> bool
		{
			auto control = std::move(aAdmittedControl);
			if (!control) {
				auto admitted = admit_job(
					aRequest, aEpochGeneration, aComparisonGeneration, aOfferToken);
				if (!admitted) {
					reject_admission(aAdmissionFailure, admitted.error());
					return false;
				}
				control = std::move(*admitted);
			}
			const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const decisionLane = &DecisionLane;
			try {
				auto work = [gate,
							 decisionLane,
							 control,
							 consumer = Consumer,
							 aExceptionCompletion,
							 task = std::move(aTask)]() mutable {
					auto completion = worker_completion{ };
					if (!consumer->active() && !control->MandatoryCleanup) {
						static_cast<void>(control->Stop.request_stop());
					}
					try {
						completion = task(control->Stop.get_token());
					} catch (...) {
						completion = std::move(aExceptionCompletion);
						if (!completion) {
							completion = [](implementation& aHost) {
								aHost.publish_diagnostic(
									core::epoch_reason_code::unavailable);
							};
						}
					}
					enqueue_completion(
						gate,
						decisionLane,
						std::move(control),
						std::move(completion));
				};
				if (aActor) {
					enqueue_actor_work(std::move(aActor), std::move(work));
				} else {
					Effects.post(std::move(work));
				}
				return true;
			} catch (...) {
				finish_job(control);
				reject_admission(
					aAdmissionFailure, core::epoch_failure::unavailable);
				return false;
			}
		}

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
			-> std::expected<std::shared_ptr<job_control>, core::epoch_failure>
		{
			try {
				std::scoped_lock lock{ JobsMutex };
				if (!aUseTerminalReservation && !aMandatoryCleanup
					&& BlobIngressClosing.load(std::memory_order_acquire)) {
					return std::unexpected{ core::epoch_failure::unavailable };
				}
				const auto usesReservation = aUseTerminalReservation && ReservedTerminalAdmissions != 0U;
				if (!usesReservation
					&& Jobs.size() + ReservedTerminalAdmissions + RetiringActorJobs
						>= Limits.OutstandingJobs) {
					return std::unexpected{ core::epoch_failure::quota_exceeded };
				}
#if defined(BPR_APPLICATION_TESTING)
				fail_admission_at(detail::testing::
									  epoch_admission_allocation_stage::job_control);
#endif
				auto control                  = std::make_shared<job_control>();
				control->Request              = aRequest;
				control->EpochGeneration      = aEpochGeneration;
				control->ComparisonGeneration = aComparisonGeneration;
				control->OfferToken           = aOfferToken;
				control->MandatoryCleanup     = aMandatoryCleanup;
#if defined(BPR_APPLICATION_TESTING)
				fail_admission_at(detail::testing::
									  epoch_admission_allocation_stage::decision_task);
#endif
				control->DecisionTask = [gate        = std::weak_ptr<completion_gate>{ CompletionGate },
										 weakControl = std::weak_ptr<job_control>{ control }]() mutable {
					const auto locked        = gate.lock();
					auto       lockedControl = weakControl.lock();
					if (!locked || !lockedControl) {
						return;
					}
					const std::lock_guard barrier{ locked->Barrier };
					auto* const           host = locked->Host.load(std::memory_order_acquire);
					if (host == nullptr) {
						return;
					}
					auto completion = std::move(lockedControl->Completion);
					if (!host->Consumer->current() && !host->Closing) {
						host->shutdown();
					}
					host->run_turn(
						[host,
						 terminalControl    = std::move(lockedControl),
						 terminalCompletion = std::move(completion)]() mutable {
							const auto reserved = host->retire_job_for_terminal(terminalControl);
							struct reservation_guard final
							{
								implementation& Host;
								bool            Reserved;

								~reservation_guard()
								{
									Host.release_terminal_reservation(Reserved);
								}
							} reservation{ *host, reserved };
							terminalCompletion(*host);
						});
				};
#if defined(BPR_APPLICATION_TESTING)
				fail_admission_at(
					detail::testing::epoch_admission_allocation_stage::job_ledger);
#endif
				control->Preparation = aPreparation;
				Jobs.push_back(control);
				if (usesReservation) {
					--ReservedTerminalAdmissions;
				}
				return control;
			} catch (...) {
				return std::unexpected{ core::epoch_failure::unavailable };
			}
		}

		void reject_admission(
			const admission_failure&  aFailure,
			const core::epoch_failure aReason) noexcept
		{
			try {
				if (aFailure) {
					aFailure(*this, aReason);
				} else {
					publish_diagnostic(core::reason_for(aReason));
				}
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void finish_job(const std::shared_ptr<job_control>& aControl) noexcept
		{
			std::scoped_lock lock{ JobsMutex };
			const auto       match = std::ranges::find(Jobs, aControl);
			if (match != Jobs.end()) {
				Jobs.erase(match);
			}
		}

		void release_pending_cleanup(
			const std::shared_ptr<pending_context_cleanup>& aCleanup) noexcept
		{
			if (!aCleanup || !aCleanup->Control) {
				return;
			}
			finish_job(aCleanup->Control);
			aCleanup->Control.reset();
			aCleanup->Context.reset();
		}

		[[nodiscard]] auto retire_job_for_terminal(
			const std::shared_ptr<job_control>& aControl) noexcept -> bool
		{
			std::scoped_lock lock{ JobsMutex };
			const auto       match = std::ranges::find(Jobs, aControl);
			if (match == Jobs.end()) {
				return false;
			}
			Jobs.erase(match);
			++ReservedTerminalAdmissions;
			return true;
		}

		void release_terminal_reservation(const bool aReserved) noexcept
		{
			if (!aReserved) {
				return;
			}
			std::scoped_lock lock{ JobsMutex };
			if (ReservedTerminalAdmissions != 0U) {
				--ReservedTerminalAdmissions;
			}
		}

		[[nodiscard]] auto retire_actor_job(
			const std::shared_ptr<job_control>& aControl) noexcept -> bool
		{
			std::scoped_lock lock{ JobsMutex };
			const auto       match = std::ranges::find(Jobs, aControl);
			if (match == Jobs.end()) {
				return false;
			}
			Jobs.erase(match);
			++RetiringActorJobs;
			if (aControl->Preparation) {
				++RetiringPreparationJobs;
			}
			return true;
		}

		void release_actor_job_reservation(
			const bool aReserved,
			const bool aPreparation = false) noexcept
		{
			if (!aReserved) {
				return;
			}
			std::scoped_lock lock{ JobsMutex };
			if (RetiringActorJobs != 0U) {
				--RetiringActorJobs;
			}
			if (aPreparation && RetiringPreparationJobs != 0U) {
				--RetiringPreparationJobs;
			}
		}

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

		void cancel_all_jobs() noexcept
		{
			cancel_jobs([](const job_control& aJob) {
				return !aJob.MandatoryCleanup;
			});
		}

		void close_blob_ingress_and_cancel_jobs() noexcept
		{
			BlobIngressClosing.store(true, std::memory_order_release);
			cancel_all_jobs();
		}

		[[nodiscard]] auto actors_idle() const noexcept -> bool
		{
			return ActorWork->Pending.load(std::memory_order_acquire) == 0U;
		}

		void cancel_authority_jobs() noexcept
		{
			cancel_jobs([](const job_control& aJob) {
				return !aJob.OfferToken && !aJob.MandatoryCleanup;
			});
		}

		void cancel_offer_job(const std::uint64_t aToken) noexcept
		{
			cancel_jobs([aToken](const job_control& aJob) {
				return aJob.OfferToken == aToken;
			});
		}

		void cancel_work(const core::cancel_epoch_work_request& aRequest) noexcept
		{
			cancel_jobs([&](const job_control& aJob) {
				if (aJob.Request != aRequest.Request) {
					return false;
				}
				const bool generationMatches = std::visit(
					[&aJob](const auto aGeneration) {
						using generation_type = std::remove_cvref_t<decltype(aGeneration)>;
						if constexpr (std::is_same_v<
										  generation_type,
										  core::epoch_generation>) {
							return aJob.EpochGeneration == aGeneration;
						} else {
							return aJob.ComparisonGeneration == aGeneration;
						}
					},
					aRequest.Generation);
				return generationMatches;
			});
		}

		// [[BPR-1302]]. No job, blob call, turn, deferred ingress, invalidation,
		// diagnostic, shutdown, offer or actor work is outstanding. Shutdown
		// quiescence and running idleness are this one fact under opposite
		// lifecycles.
		[[nodiscard]] auto outstanding_work_settled() const noexcept -> bool
		{
			return job_count() == 0U && blob_calls_idle() && !TurnActive && DeferredIngress.empty()
				&& deferred_invalidations_empty() && !DeferredIngressScheduled && !DeferredDiagnostic
				&& !DeferredShutdown && !PendingOfferToken && actors_idle();
		}

		[[nodiscard]] auto job_count() const noexcept -> std::size_t
		{
			std::scoped_lock lock{ JobsMutex };
			return Jobs.size() + ReservedTerminalAdmissions + RetiringActorJobs;
		}

		/// [[BPR-1344]]. The part of `job_count` that preparation admitted.
		[[nodiscard]] auto preparation_job_count() const noexcept -> std::size_t
		{
			std::scoped_lock lock{ JobsMutex };
			return preparation_job_count_locked();
		}

		/// [[BPR-1344]]. The rest of `job_count`, read under the same lock so
		/// that it never counts a job twice or misses one moving between them.
		[[nodiscard]] auto reviewer_job_count() const noexcept -> std::size_t
		{
			std::scoped_lock lock{ JobsMutex };
			return Jobs.size() + ReservedTerminalAdmissions + RetiringActorJobs
				- preparation_job_count_locked();
		}

		/// [[BPR-1344]]. The reviewer's jobs still to run or to answer: neither
		/// preparation's nor finished actor work holding only its retirement
		/// reservation, which the effect thread releases after the job's
		/// completion has already been posted.
		[[nodiscard]] auto active_reviewer_job_count() const noexcept -> std::size_t
		{
			std::scoped_lock lock{ JobsMutex };
			return active_reviewer_job_count_locked();
		}

		[[nodiscard]] auto active_reviewer_job_count_locked() const noexcept -> std::size_t
		{
			const auto preparing = static_cast<std::size_t>(std::ranges::count_if(
				Jobs, [](const auto& aJob) { return aJob->Preparation; }));
			return Jobs.size() - preparing + ReservedTerminalAdmissions;
		}

		[[nodiscard]] auto preparation_job_count_locked() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(
					   Jobs, [](const auto& aJob) { return aJob->Preparation; }))
				+ RetiringPreparationJobs;
		}

		[[nodiscard]] auto begin_blob_call() noexcept -> bool
		{
			std::scoped_lock lock{ JobsMutex };
			if (!Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
				return false;
			}
			++InFlightBlobCalls;
			return true;
		}

		[[nodiscard]] auto begin_cleanup_call() noexcept -> bool
		{
			std::scoped_lock lock{ JobsMutex };
			++InFlightBlobCalls;
			return true;
		}

		void finish_blob_call() noexcept
		{
			std::scoped_lock lock{ JobsMutex };
			if (InFlightBlobCalls != 0U) {
				--InFlightBlobCalls;
			}
		}

		[[nodiscard]] auto blob_calls_idle() const noexcept -> bool
		{
			std::scoped_lock lock{ JobsMutex };
			return InFlightBlobCalls == 0U;
		}

		[[nodiscard]] auto queued_actor_work() const noexcept -> std::size_t
		{
			return ActorWork->Queued.load(std::memory_order_acquire);
		}

		[[nodiscard]] static auto advance_append(
			epoch_catalog_session&              aCatalogSession,
			const append_operation_record&      aRecord,
			const core::epoch_catalog_identity  aCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<append_operation_record>
		{
			const auto evidence = required_transition_evidence(
				epoch_catalog_operation_kind::append,
				aRecord.Phase,
				aNext);
			if (!evidence) {
				return std::unexpected{ epoch_catalog_failure::phase_conflict };
			}
			auto advanced = aCatalogSession.transition_operation(
				operation_transition_request{
					.Catalog                   = aCatalog,
					.Operation                 = aRecord.Context.Operation,
					.Kind                      = epoch_catalog_operation_kind::append,
					.Expected                  = aRecord.Phase,
					.Next                      = aNext,
					.Evidence                  = *evidence,
					.ExpectedOperationRevision = aRecord.OperationRevision,
				},
				aStop);
			if (!advanced) {
				return std::unexpected{ advanced.error() };
			}
			const auto* append = std::get_if<append_operation_record>(&*advanced);
			if (
				append == nullptr
				|| !exact_append_transition(aRecord, *append, aNext)) {
				return std::unexpected{ epoch_catalog_failure::operation_conflict };
			}
			return *append;
		}

		[[nodiscard]] static auto advance_append(
			authority_context&                  aContext,
			const append_operation_record&      aRecord,
			const core::epoch_catalog_identity  aCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<append_operation_record>
		{
			return advance_append(
				*aContext.Catalog, aRecord, aCatalog, aNext, aStop);
		}

		[[nodiscard]] static auto advance_maintenance(
			epoch_catalog_session&              aCatalog,
			const maintenance_operation_record& aRecord,
			const core::epoch_catalog_identity  aExpectedCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<maintenance_operation_record>
		{
			const auto evidence = required_transition_evidence(aRecord.Kind, aRecord.Phase, aNext);
			if (!evidence) {
				return std::unexpected{ epoch_catalog_failure::phase_conflict };
			}
			auto advanced = aCatalog.transition_operation(
				operation_transition_request{
					.Catalog                   = aExpectedCatalog,
					.Operation                 = aRecord.Operation,
					.Kind                      = aRecord.Kind,
					.Expected                  = aRecord.Phase,
					.Next                      = aNext,
					.Evidence                  = *evidence,
					.ExpectedOperationRevision = aRecord.OperationRevision,
				},
				aStop);
			if (!advanced) {
				return std::unexpected{ advanced.error() };
			}
			const auto* maintenance = std::get_if<maintenance_operation_record>(&*advanced);
			if (maintenance == nullptr
				|| !exact_maintenance_transition(aRecord, *maintenance, aNext)) {
				return std::unexpected{ epoch_catalog_failure::operation_conflict };
			}
			return *maintenance;
		}

		[[nodiscard]] static auto open_actor_cache(authority_effect_actor&       aActor,
												   git_cache_port&               aCaches,
												   const git_cache_open_request& aExpected,
												   const std::stop_token         aStop)
			-> git_cache_result<git_cache_session*>
		{
			if (aActor.Cache && aActor.Cache->request() != aExpected) {
				aActor.Cache.reset();
			}
			if (!aActor.Cache) {
				auto opened = aCaches.open(aExpected, aStop);
				if (!opened) {
					return std::unexpected{ opened.error() };
				}
				if (
					!*opened
					|| !application::valid((*opened)->request())
					|| (*opened)->request() != aExpected) {
					return std::unexpected{
						git_cache_failure::repair_required
					};
				}
				aActor.Cache = std::move(*opened);
			}
			return aActor.Cache.get();
		}

		static void mark_append_repair(
			epoch_catalog_session&         aCatalog,
			const append_operation_record& aRecord)
		{
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return;
			}
			static_cast<void>(advance_append(
				aCatalog,
				aRecord,
				aCatalog.binding().Catalog,
				epoch_catalog_operation_phase::repair_required,
				{ }));
		}

		static void mark_maintenance_repair(epoch_catalog_session&              aCatalog,
											const maintenance_operation_record& aRecord)
		{
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return;
			}
			static_cast<void>(advance_maintenance(aCatalog,
												  aRecord,
												  aCatalog.binding().Catalog,
												  epoch_catalog_operation_phase::repair_required,
												  { }));
		}

		[[nodiscard]] static auto recover_append(
			epoch_catalog_session&  aCatalog,
			git_cache_session&      aCache,
			epoch_entropy_port&     aEntropy,
			append_operation_record aRecord,
			const std::stop_token   aStop)
			-> std::expected<void, core::epoch_failure>
		{
			const auto failedCache = [&](const git_cache_error& aFailure)
				-> std::expected<void, core::epoch_failure> {
				const auto mapped = cache_failure(aFailure);
				if (mapped == core::epoch_failure::repair_required) {
					mark_append_repair(aCatalog, aRecord);
				}
				return std::unexpected{ mapped };
			};
			const auto request = recovery_request(aRecord);
			if (!request) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			const auto beforePointOfNoReturn = aRecord.Phase == epoch_catalog_operation_phase::preparing
				|| aRecord.Phase
					== epoch_catalog_operation_phase::spool_recorded
				|| aRecord.Phase == epoch_catalog_operation_phase::imported
				|| aRecord.Phase
					== epoch_catalog_operation_phase::staging_ready
				|| aRecord.Phase
					== epoch_catalog_operation_phase::abort_requested;
			if (beforePointOfNoReturn) {
				if (
					aRecord.Phase
					!= epoch_catalog_operation_phase::abort_requested) {
					const auto requested = advance_append(
						aCatalog,
						aRecord,
						aCatalog.binding().Catalog,
						epoch_catalog_operation_phase::abort_requested,
						aStop);
					if (!requested) {
						return std::unexpected{
							catalog_failure(requested.error())
						};
					}
					aRecord = *requested;
				}
				auto cleanup = aCache.abort_recovered_operation(*request, aStop);
				if (!cleanup) {
					cleanup = aCache.abort_recovered_operation(*request, aStop);
				}
				if (!cleanup) {
					if (cleanup.error() == git_cache_failure::repair_required) {
						mark_append_repair(aCatalog, aRecord);
					}
					return failedCache(cleanup.error());
				}
				const auto completed = advance_append(
					aCatalog,
					aRecord,
					aCatalog.binding().Catalog,
					epoch_catalog_operation_phase::aborted,
					aStop);
				if (!completed) {
					return std::unexpected{ catalog_failure(completed.error()) };
				}
				return { };
			}

			if (
				aRecord.Phase
					!= epoch_catalog_operation_phase::commit_authorized
				&& aRecord.Phase
					!= epoch_catalog_operation_phase::ref_commit_in_progress
				&& aRecord.Phase
					!= epoch_catalog_operation_phase::refs_committed
				&& aRecord.Phase
					!= epoch_catalog_operation_phase::visible_cleanup_pending) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (!aRecord.Install || !aRecord.Candidate) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			const auto physicalStop = aRecord.Phase
					== epoch_catalog_operation_phase::commit_authorized
				? aStop
				: std::stop_token{ };
			auto       physical     = aCache.inspect_operation_recovery(*request, physicalStop);
			if (!physical) {
				return failedCache(physical.error());
			}
			if (physical->Operation != request->Operation) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto durablePhysicalMatch = physical->Installed == git_recovery_match::exact
				&& physical->Objects == git_recovery_object_match::exact
				&& (physical->Spool == git_recovery_match::absent
					|| physical->Spool == git_recovery_match::exact);
			if (!durablePhysicalMatch) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			if (
				aRecord.Phase
				== epoch_catalog_operation_phase::commit_authorized) {
				if (physical->Refs != git_recovery_ref_topology::staging) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto started = advance_append(
					aCatalog,
					aRecord,
					aCatalog.binding().Catalog,
					epoch_catalog_operation_phase::ref_commit_in_progress,
					aStop);
				if (!started) {
					return std::unexpected{ catalog_failure(started.error()) };
				}
				aRecord = *started;
			}

			const auto postCommitStop = std::stop_token{ };

			if (
				aRecord.Phase
				== epoch_catalog_operation_phase::ref_commit_in_progress) {
				if (physical->Refs == git_recovery_ref_topology::staging) {
					const auto prepared = recovered_prepared_revision(aRecord);
					const auto token    = aEntropy.generate();
					if (!prepared || !token || !application::valid(*token)) {
						return std::unexpected{ core::epoch_failure::unavailable };
					}
					const auto promoted = aCache.promote_retained(
						*prepared,
						git_ref_promotion_authorization{
							.CatalogIncarnation      = aRecord.Context.Catalog.Incarnation,
							.ExpectedCatalogRevision = aRecord.Context.Catalog.Revision,
							.Operation               = aRecord.Context.Operation,
							.Scope                   = aRecord.FinalBase.Authority,
							.Ordinal                 = aRecord.ReservedOrdinal,
							.Base                    = aRecord.Context.Revision.Base,
							.Head                    = aRecord.Context.Revision.Head,
							.DurableCommitToken      = *token,
						},
						postCommitStop);
					if (!promoted) {
						auto afterPromotion = aCache.inspect_operation_recovery(
							*request, postCommitStop);
						if (!afterPromotion) {
							return failedCache(afterPromotion.error());
						}
						if (afterPromotion->Operation != request->Operation) {
							mark_append_repair(aCatalog, aRecord);
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						const auto exactAfter = afterPromotion->Installed
								== git_recovery_match::exact
							&& afterPromotion->Objects
								== git_recovery_object_match::exact
							&& (afterPromotion->Spool
									== git_recovery_match::absent
								|| afterPromotion->Spool
									== git_recovery_match::exact);
						if (
							exactAfter
							&& afterPromotion->Refs
								== git_recovery_ref_topology::final) {
							physical = std::move(afterPromotion);
						} else if (
							exactAfter
							&& afterPromotion->Refs
								== git_recovery_ref_topology::staging) {
							return std::unexpected{
								core::epoch_failure::unavailable
							};
						} else {
							mark_append_repair(aCatalog, aRecord);
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
					} else if (*promoted != expected_retained_pair(aRecord)) {
						mark_append_repair(aCatalog, aRecord);
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
				} else if (physical->Refs != git_recovery_ref_topology::final) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto committed = advance_append(
					aCatalog,
					aRecord,
					aCatalog.binding().Catalog,
					epoch_catalog_operation_phase::refs_committed,
					postCommitStop);
				if (!committed) {
					return std::unexpected{ catalog_failure(committed.error()) };
				}
				aRecord = *committed;
			} else if (
				aRecord.Phase
						== epoch_catalog_operation_phase::visible_cleanup_pending
					? (
						  physical->Final != git_recovery_match::exact
						  || (physical->Staging != git_recovery_match::absent
							  && physical->Staging
								  != git_recovery_match::exact))
					: physical->Refs != git_recovery_ref_topology::final) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			const auto retained = aRecord.Install
					&& aRecord.Install->Residency == git_revision_residency::sparse_commit_trees
				? aCache.inspect_sparse_retained(aRecord.ReservedOrdinal, postCommitStop)
				: aCache.inspect_retained(aRecord.ReservedOrdinal, postCommitStop);
			if (!retained) {
				return failedCache(retained.error());
			}
			if (*retained != expected_retained_pair(aRecord)) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			auto cleanupCatalog = aCatalog.binding().Catalog;
			if (
				aRecord.Phase == epoch_catalog_operation_phase::refs_committed) {
				if (aCatalog.binding().Catalog != aRecord.Context.Catalog) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto finalized = aCatalog.finalize_append(
					finalize_append_request{
						.Catalog                   = aRecord.Context.Catalog,
						.Operation                 = aRecord.Context.Operation,
						.ExpectedOperationRevision = aRecord.OperationRevision,
						.Mutation                  = *aRecord.Candidate,
						.ObservedRefs              = *retained,
						.HistoricalOccurrence      = aRecord.HistoricalOccurrence,
					},
					postCommitStop);
				if (!finalized) {
					return std::unexpected{ catalog_failure(finalized.error()) };
				}
				if (
					!exact_catalog_commit(
						*finalized,
						aRecord.Context.Catalog,
						aRecord.Candidate->Epoch.Id)
					|| aCatalog.binding().Catalog != finalized->Catalog) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{
						core::epoch_failure::repair_required
					};
				}
				cleanupCatalog            = finalized->Catalog;
				aRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
				aRecord.OperationRevision = epoch_catalog_operation_revision{
					aRecord.OperationRevision.value() + 1
				};
			}

			auto cleaned = aCache.complete_recovered_operation(
				*request, postCommitStop);
			if (!cleaned) {
				const auto afterCleanup = aCache.inspect_operation_recovery(
					*request, postCommitStop);
				if (!afterCleanup) {
					return failedCache(afterCleanup.error());
				}
				const auto exactCleanupState = afterCleanup->Operation == request->Operation
					&& afterCleanup->Installed == git_recovery_match::exact
					&& afterCleanup->Objects
						== git_recovery_object_match::exact
					&& afterCleanup->Final == git_recovery_match::exact
					&& (afterCleanup->Staging == git_recovery_match::absent
						|| afterCleanup->Staging == git_recovery_match::exact)
					&& (afterCleanup->Spool == git_recovery_match::absent
						|| afterCleanup->Spool == git_recovery_match::exact);
				if (!exactCleanupState) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{
						core::epoch_failure::repair_required
					};
				}
				cleaned = aCache.complete_recovered_operation(
					*request, postCommitStop);
				if (!cleaned) {
					return std::unexpected{ core::epoch_failure::unavailable };
				}
			}
			const auto completedResult = advance_append(
				aCatalog,
				aRecord,
				cleanupCatalog,
				epoch_catalog_operation_phase::complete,
				postCommitStop);
			if (!completedResult) {
				return std::unexpected{
					catalog_failure(completedResult.error())
				};
			}
			return { };
		}

		[[nodiscard]] static auto recover_maintenance(
			epoch_catalog_session&               aCatalog,
			git_cache_port&                      aCaches,
			const accepted_pull_request_handoff& aHandoff,
			std::unique_ptr<git_cache_session>&  aCache,
			maintenance_operation_record         aRecord,
			const std::stop_token                aStop)
			-> std::expected<core::epoch_catalog_identity, core::epoch_failure>
		{
			const auto failCatalog = [&](const epoch_catalog_failure aFailure)
				-> std::expected<core::epoch_catalog_identity, core::epoch_failure> {
				const auto mapped = catalog_failure(aFailure);
				if (mapped == core::epoch_failure::repair_required) {
					mark_maintenance_repair(aCatalog, aRecord);
				}
				return std::unexpected{ mapped };
			};
			const auto failCache = [&](const git_cache_error& aFailure)
				-> std::expected<core::epoch_catalog_identity, core::epoch_failure> {
				const auto mapped = cache_failure(aFailure);
				if (mapped == core::epoch_failure::repair_required) {
					mark_maintenance_repair(aCatalog, aRecord);
				}
				return std::unexpected{ mapped };
			};
			if (!application::valid(aRecord)
				|| aRecord.Incarnation != aCatalog.binding().Catalog.Incarnation
				|| aRecord.Cache != aCatalog.binding().Authority.Cache) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::complete
				|| aRecord.Phase == epoch_catalog_operation_phase::aborted) {
				return aCatalog.binding().Catalog;
			}

			const auto beforePointOfNoReturn = !point_of_no_cancellation(aRecord.Phase);
			if (beforePointOfNoReturn) {
				if (aRecord.Phase != epoch_catalog_operation_phase::abort_requested) {
					const auto requested = advance_maintenance(aCatalog,
															   aRecord,
															   aCatalog.binding().Catalog,
															   epoch_catalog_operation_phase::abort_requested,
															   aStop);
					if (!requested) {
						return failCatalog(requested.error());
					}
					aRecord = *requested;
				}
				const auto aborted = advance_maintenance(aCatalog,
														 aRecord,
														 aCatalog.binding().Catalog,
														 epoch_catalog_operation_phase::aborted,
														 aStop);
				if (!aborted) {
					return failCatalog(aborted.error());
				}
				return aCatalog.binding().Catalog;
			}

			const auto expectedCache = git_cache_open_request{
				.ApplicationSession = aHandoff.ApplicationSession,
				.Authority          = aCatalog.binding().Authority,
				.ObjectFormat       = aHandoff.ObjectFormat,
			};
			if (aCache && aCache->request() != expectedCache) {
				aCache.reset();
			}
			const auto durableToken  = aRecord.Operation.bytes();
			const auto postPointStop = std::stop_token{ };

			if (aRecord.Kind == epoch_catalog_operation_kind::prune) {
				if (aRecord.Phase
					== epoch_catalog_operation_phase::ref_delete_in_progress) {
					if (!aCache) {
						auto opened = aCaches.open(expectedCache, postPointStop);
						if (!opened) {
							return failCache(opened.error());
						}
						if (!*opened || !application::valid((*opened)->request())
							|| (*opened)->request() != expectedCache) {
							mark_maintenance_repair(aCatalog, aRecord);
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						aCache = std::move(*opened);
					}
					std::vector<git_retained_ref_pair> absent;
					absent.reserve(aRecord.Epochs.size());
					for (const auto& fact : aRecord.Epochs) {
						const auto pruned = aCache->prune_retained(
							git_ref_prune_authorization{
								.CatalogIncarnation      = aRecord.Incarnation,
								.ExpectedCatalogRevision = aCatalog.binding().Catalog.Revision,
								.Expected                = fact.Retained,
								.DurableIntentToken      = durableToken,
							},
							postPointStop);
						if (!pruned) {
							return failCache(pruned.error());
						}
						absent.push_back(fact.Retained);
					}
					const auto before    = aCatalog.binding().Catalog;
					const auto finalized = aCatalog.finalize_prune(
						finalize_prune_request{
							.Transition = operation_transition_request{
								.Catalog   = before,
								.Operation = aRecord.Operation,
								.Kind      = aRecord.Kind,
								.Expected  = aRecord.Phase,
								.Next      = epoch_catalog_operation_phase::
									visible_cleanup_pending,
								.Evidence = epoch_catalog_transition_evidence::
									exact_refs_absent,
								.ExpectedOperationRevision = aRecord.OperationRevision,
							},
							.ObservedAbsent = std::move(absent),
						},
						postPointStop);
					if (!finalized) {
						return failCatalog(finalized.error());
					}
					if (!exact_catalog_commit(*finalized, before, std::nullopt)) {
						mark_maintenance_repair(aCatalog, aRecord);
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
					aRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
					aRecord.OperationRevision = epoch_catalog_operation_revision{
						aRecord.OperationRevision.value() + 1
					};
				}
				if (aRecord.Phase
					!= epoch_catalog_operation_phase::visible_cleanup_pending) {
					mark_maintenance_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto completed = advance_maintenance(aCatalog,
														   aRecord,
														   aCatalog.binding().Catalog,
														   epoch_catalog_operation_phase::complete,
														   postPointStop);
				if (!completed) {
					return failCatalog(completed.error());
				}
				return aCatalog.binding().Catalog;
			}

			if (aRecord.Kind != epoch_catalog_operation_kind::cache_delete) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto deletePhysicalCache =
				[&]() -> std::expected<void, core::epoch_failure> {
				aCache.reset();
				const auto deleted = aCaches.delete_cache(expectedCache,
														  git_cache_delete_authorization{
															  .ApplicationSession = aHandoff.ApplicationSession,
															  .Cache              = aRecord.Cache,
															  .DurableIntentToken = durableToken,
														  },
														  postPointStop);
				if (!deleted) {
					const auto mapped = cache_failure(deleted.error());
					if (mapped == core::epoch_failure::repair_required) {
						mark_maintenance_repair(aCatalog, aRecord);
					}
					return std::unexpected{ mapped };
				}
				return { };
			};
			if (aRecord.Phase
				== epoch_catalog_operation_phase::quarantine_rename_in_progress) {
				if (const auto deleted = deletePhysicalCache(); !deleted) {
					return std::unexpected{ deleted.error() };
				}
				const auto quarantined = advance_maintenance(aCatalog,
															 aRecord,
															 aCatalog.binding().Catalog,
															 epoch_catalog_operation_phase::quarantined,
															 postPointStop);
				if (!quarantined) {
					return failCatalog(quarantined.error());
				}
				aRecord = *quarantined;
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::quarantined) {
				const auto before    = aCatalog.binding();
				const auto finalized = aCatalog.finalize_cache_delete(
					finalize_cache_delete_request{
						.Transition = operation_transition_request{
							.Catalog   = before.Catalog,
							.Operation = aRecord.Operation,
							.Kind      = aRecord.Kind,
							.Expected  = aRecord.Phase,
							.Next      = epoch_catalog_operation_phase::
								catalog_tombstoned_cleanup_pending,
							.Evidence = epoch_catalog_transition_evidence::
								catalog_transaction,
							.ExpectedOperationRevision = aRecord.OperationRevision,
						},
					},
					postPointStop);
				if (!finalized) {
					return failCatalog(finalized.error());
				}
				const auto& after = aCatalog.binding();
				if (!application::valid(after) || after.Authority != before.Authority
					|| after.Catalog.Incarnation != before.Catalog.Incarnation
					|| before.Catalog.Revision.value()
						>= core::maximum_epoch_catalog_revision
					|| after.Catalog.Revision.value()
						!= before.Catalog.Revision.value() + 1) {
					mark_maintenance_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				aRecord.Phase = epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending;
				aRecord.OperationRevision = epoch_catalog_operation_revision{
					aRecord.OperationRevision.value() + 1
				};
			}
			if (aRecord.Phase
				!= epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (const auto deleted = deletePhysicalCache(); !deleted) {
				return std::unexpected{ deleted.error() };
			}
			const auto completed = advance_maintenance(aCatalog,
													   aRecord,
													   aCatalog.binding().Catalog,
													   epoch_catalog_operation_phase::complete,
													   postPointStop);
			if (!completed) {
				return failCatalog(completed.error());
			}
			return aCatalog.binding().Catalog;
		}

		[[nodiscard]] static auto recover_catalog(epoch_catalog_session&               aCatalog,
												  git_cache_port&                      aCaches,
												  epoch_entropy_port&                  aEntropy,
												  const accepted_pull_request_handoff& aHandoff,
												  std::unique_ptr<git_cache_session>&  aCache,
												  const std::stop_token                aStop)
			-> std::expected<void, core::epoch_failure>
		{
			std::optional<core::epoch_operation_id> after;
			std::size_t                             recoveredCount = 0U;
			while (true) {
				const auto page = aCatalog.recovery_page(
					after, maximum_epoch_recovery_page_size, aStop);
				if (!page) {
					return std::unexpected{ catalog_failure(page.error()) };
				}
				if (
					!application::valid(*page)
					|| page->Incarnation
						!= aCatalog.binding().Catalog.Incarnation
					|| (page->Continuation
						&& page->Operations.empty())
					|| (after && page->Operations.empty())
					|| (after && page->Continuation
						&& !(*after < *page->Continuation))
					|| page->Operations.size()
						> maximum_recovery_operations - recoveredCount) {
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				if (after && !page->Operations.empty()) {
					const auto first = recovery_operation_identity(
						page->Operations.front());
					// SQLite's continuation is the first omitted identity, so
					// the next page includes an identity equal to `after`.
					if (first != *after) {
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
				}
				recoveredCount += page->Operations.size();
				for (const auto& operation : page->Operations) {
					const auto* append = std::get_if<append_operation_record>(&operation);
					if (append == nullptr) {
						const auto* maintenance = std::get_if<maintenance_operation_record>(&operation);
						if (maintenance == nullptr) {
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						const auto recovered = recover_maintenance(aCatalog,
																   aCaches,
																   aHandoff,
																   aCache,
																   *maintenance,
																   aStop);
						if (!recovered) {
							return std::unexpected{ recovered.error() };
						}
						continue;
					}
					if (!aCache) {
						const auto expected = git_cache_open_request{
							.ApplicationSession = aHandoff.ApplicationSession,
							.Authority          = aCatalog.binding().Authority,
							.ObjectFormat       = aHandoff.ObjectFormat,
						};
						auto opened = aCaches.open(expected, aStop);
						if (!opened) {
							return std::unexpected{ cache_failure(opened.error()) };
						}
						if (!*opened) {
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						if (
							!application::valid((*opened)->request())
							|| (*opened)->request() != expected) {
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						aCache = std::move(*opened);
					}
					const auto recovered = recover_append(
						aCatalog, *aCache, aEntropy, *append, aStop);
					if (!recovered) {
						return recovered;
					}
				}
				if (!page->Continuation) {
					return { };
				}
				after = page->Continuation;
			}
		}

		[[nodiscard]] static auto pending_append(
			epoch_catalog_session&         aCatalog,
			const core::epoch_operation_id aOperation)
			-> std::expected<
				std::optional<append_operation_record>,
				core::epoch_failure>
		{
			auto page = aCatalog.recovery_page(aOperation, 1U, { });
			if (!page) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (
				!application::valid(*page)
				|| page->Incarnation
					!= aCatalog.binding().Catalog.Incarnation
				|| page->Operations.size() > 1U
				|| (page->Continuation && page->Operations.empty())) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (page->Operations.empty()) {
				return std::optional<append_operation_record>{ };
			}
			const auto* append = std::get_if<append_operation_record>(&page->Operations.front());
			const auto  found  = recovery_operation_identity(
				page->Operations.front());
			if (found < aOperation) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (found != aOperation) {
				return std::optional<append_operation_record>{ };
			}
			if (append == nullptr) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			return std::optional{ *append };
		}

		[[nodiscard]] static auto abort_preparation(
			authority_context&  aContext,
			prepared_operation& aOperation, git_cache_port& aCaches) -> bool
		{
			if (aOperation.Kind == prepared_kind::observation_only) {
				return true;
			}
			if (!aOperation.Record) {
				return false;
			}
			auto record = *aOperation.Record;
			if (record.Phase == epoch_catalog_operation_phase::aborted
				|| record.Phase == epoch_catalog_operation_phase::complete) {
				return true;
			}
			if (record.Phase == epoch_catalog_operation_phase::repair_required
				|| point_of_no_cancellation(record.Phase)) {
				return false;
			}
			const auto markRepair = [&aContext, &record, &aOperation]() {
				const auto repaired = advance_append(
					aContext,
					record,
					record.Context.Catalog,
					epoch_catalog_operation_phase::repair_required,
					{ });
				if (repaired) {
					record            = *repaired;
					aOperation.Record = record;
				}
			};
			if (record.Phase != epoch_catalog_operation_phase::abort_requested) {
				const auto requested = advance_append(
					aContext,
					record,
					record.Context.Catalog,
					epoch_catalog_operation_phase::abort_requested,
					{ });
				if (!requested) {
					markRepair();
					return false;
				}
				record = *requested;
			}
			if (aContext.Actor->Cache) {
				const auto cache = open_actor_cache(*aContext.Actor, aCaches,
													git_cache_open_request{ .ApplicationSession = aContext.Handoff.ApplicationSession,
																			.Authority          = aContext.Authority,
																			.ObjectFormat       = aContext.Handoff.ObjectFormat },
													{ });
				if (!cache) {
					markRepair();
					return false;
				}
				const auto request = recovery_request(record);
				if (!request) {
					markRepair();
					return false;
				}
				auto aborted = aContext.Actor->Cache->abort_recovered_operation(
					*request, { });
				if (!aborted) {
					aborted = aContext.Actor->Cache->abort_recovered_operation(
						*request, { });
				}
				if (!aborted) {
					markRepair();
					return false;
				}
			}
			const auto completed = advance_append(
				aContext,
				record,
				record.Context.Catalog,
				epoch_catalog_operation_phase::aborted,
				{ });
			if (!completed) {
				markRepair();
				return false;
			}
			aOperation.Record = *completed;
			return true;
		}

		[[nodiscard]] auto discard_context_preparation(
			std::shared_ptr<authority_context>            aContext,
			const std::optional<core::epoch_operation_id> aExpected        = std::nullopt,
			std::shared_ptr<job_control>                  aAdmittedControl = { })
			-> bool
		{
			if (!aContext->requires_cache_cleanup(aExpected)) {
				if (aExpected) {
					static_cast<void>(aContext->take(*aExpected));
				} else {
					aContext->Preparations.clear();
				}
				if (aAdmittedControl) {
					finish_job(aAdmittedControl);
				}
				return true;
			}
			auto        actor  = aContext->Actor;
			auto* const caches = &Caches;
			return submit_effect(
				[context = std::move(aContext), aExpected, caches](
					const std::stop_token aStop)
					-> worker_completion {
					const auto requiresCache = std::ranges::any_of(
						context->Preparations,
						[&](const auto& aOperation) {
							return aOperation.Kind == prepared_kind::append
								&& (!aExpected
									|| aOperation.Operation == *aExpected);
						});
					if (requiresCache) {
						const auto cache = open_actor_cache(
							*context->Actor,
							*caches,
							git_cache_open_request{
								.ApplicationSession = context->Handoff.ApplicationSession,
								.Authority          = context->Authority,
								.ObjectFormat       = context->Handoff.ObjectFormat,
							},
							aStop);
						if (!cache) {
							return [](implementation& aHost) {
								aHost.publish_diagnostic(
									core::epoch_reason_code::unavailable);
							};
						}
					}
					if (aExpected) {
						auto operation = context->take(*aExpected);
						if (operation) {
							static_cast<void>(
								abort_preparation(*context, *operation, *caches));
						}
					} else {
						while (!context->Preparations.empty()) {
							auto operation = context->take(
								context->Preparations.back().Operation);
							if (operation) {
								static_cast<void>(
									abort_preparation(*context, *operation, *caches));
							}
						}
					}
					return [](implementation&) { };
				},
				[](implementation& aHost) {
					aHost.publish_diagnostic(
						core::epoch_reason_code::unavailable);
				},
				[](implementation& aHost, const core::epoch_failure aFailure) {
					aHost.publish_diagnostic(core::reason_for(aFailure));
				},
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::move(actor),
				std::move(aAdmittedControl));
		}

		void publish_diagnostic(const core::epoch_reason_code aReason) noexcept
		{
			try {
				auto update       = epoch_presentation_update{ };
				update.Transition = epoch_transition_projection{
					.Changed     = core::epoch_changed_domain::diagnostics,
					.Diagnostics = { core::epoch_diagnostic{ .Code = aReason } },
				};
				publish(update);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void publish(const epoch_presentation_update& aUpdate) noexcept
		{
			if (!Legacy && !Consumer->current()) {
				return;
			}
			try {
				Projections.publish(aUpdate);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void publish_transition(const core::epoch_transition_output& aOutput)
		{
			if (aOutput.Changed == core::epoch_changed_domain::none
				&& aOutput.Diagnostics.empty()) {
				return;
			}
			epoch_presentation_update   update;
			epoch_transition_projection transition{
				.Changed     = aOutput.Changed,
				.Selection   = aOutput.Selection,
				.Status      = aOutput.Status,
				.Commands    = aOutput.Commands,
				.Diagnostics = aOutput.Diagnostics,
			};
			if (aOutput.Authority) {
				transition.Authority = epoch_authority_projection{
					.Authority  = aOutput.Authority->Authority,
					.Generation = aOutput.Authority->Generation,
				};
				if (aOutput.Authority->Revision) {
					transition.Authority->Revision = projected_revision(*aOutput.Authority->Revision);
				}
			}
			if (aOutput.Comparison) {
				auto reason = core::epoch_reason_code::none;
				if (!aOutput.Comparison->Pair && !aOutput.Diagnostics.empty()) {
					reason = aOutput.Diagnostics.back().Code;
				}
				transition.Comparison = epoch_comparison_projection{
					.Selection        = aOutput.Comparison->Selection,
					.Pair             = aOutput.Comparison->Pair,
					.Generation       = State.ComparisonGeneration,
					.Revision         = aOutput.Comparison->Revision,
					.AvailableForDiff = aOutput.Comparison->AvailableForDiff,
					.Reason           = reason,
				};
			}
			update.Transition = std::move(transition);

			if (aOutput.Catalog || aOutput.AcceptedEpoch || aOutput.History) {
				update.Catalog = epoch_catalog_projection{ };
				if (aOutput.Catalog) {
					update.Catalog->Catalog                   = aOutput.Catalog->Catalog;
					update.Catalog->Replace                   = !aOutput.Catalog->Catalog.has_value();
					update.Catalog->AvailabilityChangedEpochs = aOutput.Catalog->AvailabilityChangedEpochs;
					update.Catalog->AvailabilityChangedTo     = aOutput.Catalog->AvailabilityChangedTo;
					update.Catalog->AllEpochAvailability      = aOutput.Catalog->AllEpochAvailability;
				}
				if (aOutput.AcceptedEpoch) {
					update.Catalog->Catalog = core::epoch_catalog_identity{
						.Incarnation = aOutput.AcceptedEpoch->Epoch.Id.Incarnation,
						.Revision    = aOutput.AcceptedEpoch->CatalogRevision,
					};
					const bool historyWindowInvalidated = aOutput.History
						&& aOutput.History->Phase
							== core::epoch_history_page_phase::invalidated;
					if (!historyWindowInvalidated) {
						update.Catalog->Changes.push_back(
							epoch_row_change_projection{
								.Kind = epoch_row_change_kind::insert,
								.Id   = aOutput.AcceptedEpoch->Epoch.Id,
								.Row  = projected_row(aOutput.AcceptedEpoch->Epoch),
							});
					}
				}
				if (aOutput.History) {
					if (!aOutput.Catalog) {
						update.Catalog->Catalog = aOutput.History->Catalog;
					}
					auto page = epoch_history_page_projection{
						.Phase           = aOutput.History->Phase,
						.Catalog         = aOutput.History->Catalog,
						.Navigation      = aOutput.History->Navigation,
						.RequestedCursor = aOutput.History->RequestedCursor,
						.Limit           = aOutput.History->Limit,
						.Continuation    = aOutput.History->Continuation,
						.Reason          = aOutput.History->Reason,
					};
					page.Rows.reserve(aOutput.History->Epochs.size());
					for (const auto& epoch : aOutput.History->Epochs) {
						page.Rows.push_back(projected_row(epoch));
					}
					update.Catalog->HistoryPage = std::move(page);
				}
			}
			publish(update);
		}

		void publish_occurrences(
			const core::epoch_catalog_identity               aCatalog,
			const core::epoch_occurrence_history&            aHistory,
			const std::optional<core::review_epoch_summary>& aCurrent)
		{
			auto update    = epoch_presentation_update{ };
			update.Catalog = epoch_catalog_projection{
				.Catalog               = aCatalog,
				.OccurrenceHistory     = aHistory,
				.AllowOccurrenceImport = Active
					&& object_loading_for(Active->Handoff.Shape) == pull_request_object_loading::selected_objects
					&& Active->Handoff.SmartGit
					&& Active->Handoff.SmartGit->supports_sparse_commit_trees(),
			};
			// The accepted selection precedes its durable live append. Never expose
			// comparisons derived under the prior base while that append is pending.
			if (Active && core::historical_comparison_selection_changed(Active->Revision, aCurrent)) {
				core::invalidate_derived_comparison_bindings(*update.Catalog->OccurrenceHistory);
			}
			if (aCurrent) {
				update.Catalog->CurrentEpoch = projected_row(*aCurrent);
			}
			publish(update);
		}

		auto dispatch_core(
			const core::epoch_event& aEvent,
			const std::function<void(const core::epoch_transition_output&)>&
				aBeforeRequests = { })
			-> core::epoch_transition_output
		{
			auto output = core::transition(State, aEvent);
			publish_transition(output);
			if (aBeforeRequests) {
				aBeforeRequests(output);
			}
			for (const auto& request : output.Requests) {
				handle_request(request);
			}
			return output;
		}

		void handle_request(const core::epoch_host_request& aRequest)
		{
			std::visit(
				[this](const auto& aValue) {
					using request_type = std::decay_t<decltype(aValue)>;
					if constexpr (std::is_same_v<
									  request_type,
									  core::invalidate_epoch_authority_request>) {
						cancel_authority_jobs();
					} else if constexpr (std::is_same_v<request_type,
														core::import_epoch_occurrence_request>) {
						auto context = exact_context(aValue.Context.Authority, aValue.Context.Revision);
						if (!context || context->Handoff.ApplicationSession != aValue.Context.ApplicationSession
							|| State.Catalog != aValue.Context.ExpectedCatalog
							|| State.EpochGeneration != aValue.Context.Generation) {
							dispatch_core(core::epoch_occurrence_import_failed_event{
								.Context = aValue.Context,
								.Failure = core::epoch_failure::catalog_conflict,
							});
						} else {
							start_historical_import(context, aValue.Context.Request,
													aValue.Context.Generation, aValue.Context);
						}
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::cancel_epoch_work_request>) {
						cancel_work(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::load_epoch_catalog_request>) {
						start_catalog_load(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::prepare_epoch_revision_request>) {
						start_revision_preparation(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::commit_epoch_catalog_request>) {
						start_catalog_commit(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::discard_epoch_preparation_request>) {
						discard_preparation(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::load_epoch_history_page_request>) {
						start_history_page(aValue);
					} else if constexpr (std::is_same_v<request_type,
														core::perform_epoch_maintenance_request>) {
						start_epoch_maintenance(aValue);
					} else {
						start_comparison_resolution(aValue);
					}
				},
				aRequest);
		}

		[[nodiscard]] auto exact_context(
			const core::pull_request_authority&         aAuthority,
			const core::accepted_pull_request_revision& aRevision) const
			-> std::shared_ptr<authority_context>
		{
			if (Active && Active->Authority == aAuthority
				&& Active->Revision == aRevision) {
				return Active;
			}
			return { };
		}

		[[nodiscard]] auto actor_for(
			const cache_actor_key& aKey)
			-> std::shared_ptr<authority_effect_actor>
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       found = std::ranges::find(
				CacheActors, aKey, &decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				return found->second;
			}
			if (CacheActors.size() >= maximum_cache_actors) {
				return { };
			}
			auto shared = Consumer->acquire(aKey);
			if (!shared) {
				return { };
			}
			auto actor = std::make_shared<authority_effect_actor>(std::move(*shared));
			CacheActors.emplace_back(aKey, actor);
			return actor;
		}

		/// \brief Mints and authorizes one selected-diff context.
		///
		/// \details One body, two callers, because the accepted-state checks
		/// they need are the same checks. With no binding this is the
		/// workbench's own context: the accepted comparison, authorized into
		/// the slot every resolve is measured against. With a binding it is
		/// BPR-0532's re-alignment context: the same context with its two
		/// anchors moved to the pair the binding names, authorized into a
		/// second slot so the workbench keeps the one it is using.
		///
		/// The host mints it rather than validating one a caller supplies, so
		/// no caller can name a comparison the host did not decide to serve;
		/// and what it decides to serve is bounded to this review — the pair
		/// must name the same host, repository, and pull request, and the
		/// capability is scoped to the cache actor that only ever holds this
		/// pull request's objects. A pair that review's own cache cannot
		/// answer for refuses as a missing endpoint, which is the answer a
		/// re-alignment of a revision whose objects are gone should get.
		[[nodiscard]] auto authorized_selected_diff_context(
			const workbench_pull_request_authority&            aAuthority,
			const std::optional<core::agent_revision_binding>& aBinding, const bool aGrant = true) const
			-> std::optional<workbench_selected_diff_context>
		{
			if (!Consumer->current() || !application::valid(aAuthority)
				|| !State.Authority.has_value()
				|| !State.AcceptedRevision.has_value()
				|| !State.AcceptedComparison.has_value()
				|| !core::valid(State.AcceptedComparisonRevision)
				// [[BPR-1560]]. Whole PR without a merge base GitHub named has
				// a base the core marked unavailable; its recorded base is in
				// the cache, but a comparison from it is not GitHub's.
				|| (State.AcceptedBasis == core::comparison_base_basis::recorded_base
					&& !core::available_for_diff(*State.AcceptedComparison))) {
				return std::nullopt;
			}
			const auto& authority = *State.Authority;
			const auto& revision  = *State.AcceptedRevision;
			if (aAuthority.ApplicationSession != State.ApplicationSession
				|| aAuthority.Profile != authority.Profile
				|| aAuthority.ProfileRevision != authority.ProfileRevision
				|| aAuthority.Host != authority.Host
				|| aAuthority.HostAuthority != authority.HostAuthority
				|| aAuthority.Account != authority.Account
				|| aAuthority.Viewer != authority.Viewer
				|| aAuthority.Repository != authority.Repository
				|| aAuthority.PullRequest != authority.PullRequest
				|| aAuthority.Number != authority.Number
				|| aAuthority.Snapshot != revision.Source.Snapshot
				|| aAuthority.IdentityGeneration
					!= revision.Source.IdentityGeneration
				|| aAuthority.PullRequestGeneration
					!= revision.Source.PullRequestGeneration) {
				return std::nullopt;
			}
			const auto options = core::normalize_diff_options(core::diff_options{ });
			if (!options.has_value()) {
				return std::nullopt;
			}
			const auto digest = core::options_digest(*options);
			if (!digest.has_value()) {
				return std::nullopt;
			}
			auto context = workbench_selected_diff_context{
				.ApplicationSession = State.ApplicationSession,
				.Authority          = authority,
				.Comparison         = *State.AcceptedComparison,
				.ComparisonRevision = State.AcceptedComparisonRevision,
				.Options            = *options,
				.OptionsDigest      = *digest,
				.Algorithm          = core::supported_diff_algorithm_revision,
			};
			if (aBinding.has_value()) {
				if (aBinding->Host != authority.Host
					|| aBinding->Repository != authority.Repository
					|| aBinding->PullRequest != authority.PullRequest) {
					return std::nullopt;
				}
				// The whole retargeting, and deliberately nothing else: the
				// two anchors name the pair the binding was started against,
				// and the authority, options, and algorithm stay what they
				// are, so the key this produces is the key that revision's
				// alignment already had.
				context.Comparison.Left.Identity.Epoch  = aBinding->LeftEpoch;
				context.Comparison.Left.Commit          = aBinding->BaseOid;
				context.Comparison.Right.Identity.Epoch = aBinding->RightEpoch;
				context.Comparison.Right.Commit         = aBinding->HeadOid;
			}
			if (!Active || Active->Authority != authority
				|| Active->Revision != revision) {
				return std::nullopt;
			}
			const auto granted = selected_diff_authority{
				.Workbench = aAuthority,
				.Diff      = context,
			};
			if (aGrant) {
				if (aBinding.has_value()) {
					authorize_realignment(Active->Actor, granted);
				} else {
					authorize_selected_diff(Active->Actor, granted);
				}
			}
			return context;
		}

		[[nodiscard]] auto selected_diff_context(
			const workbench_pull_request_authority& aAuthority) const
			-> std::optional<workbench_selected_diff_context>
		{
			return authorized_selected_diff_context(aAuthority, std::nullopt);
		}

		static auto retained_source(authority_effect_actor& aActor, git_cache_port& aCaches, epoch_catalog_port& aCatalogs,
									const cached_review_source_request& aRequest, std::stop_token aStop) -> cached_review_source_projection
		{
			auto result = cached_review_source_projection{ .Path    = aRequest.File.Path,
														   .BaseOid = std::string{ core::canonical_hex(aRequest.Base).view() },
														   .HeadOid = std::string{ core::canonical_hex(aRequest.Head).view() } };
			if (aStop.stop_requested()) {
				return result;
			}
			const auto opened = aCatalogs.open(aRequest.Catalog, aStop);
			if (!opened || !*opened) {
				return result;
			}
			const auto snapshot = (*opened)->snapshot(aStop);
			if (!snapshot || !valid(snapshot->Binding) || snapshot->Binding != (*opened)->binding() || !snapshot->LatestLive
				|| snapshot->LatestLive->Id.Authority != snapshot->Binding.Authority.Scope
				|| snapshot->LatestLive->Id.Incarnation != snapshot->Binding.Catalog.Incarnation
				|| snapshot->LatestLive->Base != aRequest.Base || snapshot->LatestLive->Head != aRequest.Head
				|| snapshot->LatestLive->BaseAvailability != core::anchor_availability::available
				|| snapshot->LatestLive->HeadAvailability != core::anchor_availability::available
				|| snapshot->RetentionRehydration != core::retention_rehydration_requirement::not_required) {
				return result;
			}
			const auto& authority = snapshot->Binding.Authority;
			const auto& expected  = aRequest.Catalog.Authority;
			if (authority.Profile != expected.Profile || authority.ProfileRevision != expected.ProfileRevision
				|| authority.Host != expected.Host || authority.HostAuthority != expected.HostAuthority
				|| authority.Account != expected.Account || authority.Viewer != expected.Viewer
				|| authority.Repository != expected.Repository || authority.PullRequest != expected.PullRequest
				|| authority.Number != expected.Number || authority.Mode != expected.Mode || snapshot->Binding.ObjectFormat != expected.ObjectFormat) {
				return result;
			}
			const auto cacheRequest = git_cache_open_request{ .ApplicationSession = aRequest.Catalog.ApplicationSession, .Authority = authority, .ObjectFormat = expected.ObjectFormat };
			// The profile actor owns one physical repository session. Never replace a
			// sibling's active session merely to read this historical pair.
			if (aActor.Cache) {
				const auto& current = aActor.Cache->request();
				if (current.ObjectFormat != expected.ObjectFormat
					|| current.Authority.Profile != authority.Profile || current.Authority.ProfileRevision != authority.ProfileRevision
					|| current.Authority.Host != authority.Host || current.Authority.HostAuthority != authority.HostAuthority
					|| current.Authority.Account != authority.Account || current.Authority.Viewer != authority.Viewer || current.Authority.Repository != authority.Repository
					|| current.Authority.Cache != authority.Cache || current.Authority.Mode != authority.Mode) {
					return result;
				}
			} else if (!open_actor_cache(aActor, aCaches, cacheRequest, aStop)) {
				return result;
			}
			result.Status   = cached_source_status::available;
			const auto read = [&](core::git_commit_oid aCommit, const std::string& aPath, std::optional<std::string>& aText) {
				const auto blob = aActor.Cache->find_blob_at_path({ .Commit = aCommit, .Path = aPath }, aStop);
				if (!blob) {
					result.Status = cached_source_status::unavailable;
					return;
				}
				if (!*blob) {
					return;
				}
				const auto bytes = aActor.Cache->materialize_blob({ .Blob = **blob, .MaximumBytes = maximum_cached_source_bytes }, aStop);
				if (!bytes || bytes->size() > maximum_cached_source_bytes) {
					result.Status = !bytes && bytes.error().Code != git_cache_failure::quota_exceeded ? cached_source_status::unavailable : cached_source_status::too_large;
					return;
				}
				if (std::ranges::find(*bytes, std::uint8_t{ 0 }) != bytes->end()) {
					result.Status = cached_source_status::binary;
					return;
				}
				aText.emplace(bytes->begin(), bytes->end());
			};
			read(aRequest.Base, aRequest.File.PreviousPath.value_or(aRequest.File.Path), result.BaseText);
			if (result.Status == cached_source_status::available) {
				read(aRequest.Head, aRequest.File.Path, result.HeadText);
			}
			if (aStop.stop_requested()) {
				result.Status = cached_source_status::unavailable;
			}
			if (result.Status != cached_source_status::available) {
				result.BaseText.reset();
				result.HeadText.reset();
			}
			return result;
		}
		[[nodiscard]] auto read_cached_source(cached_review_source_request aRequest, std::stop_token aStop, cached_review_source_completion aCompletion) -> bool
		{
			if (Closing || !Consumer->current() || !aCompletion || aStop.stop_requested()
				|| !valid(aRequest.Catalog) || !aRequest.Catalog.ExistingOnly
				|| aRequest.Catalog.ApplicationSession != State.ApplicationSession
				|| aRequest.Catalog.Authority.Mode != core::github_access_mode::authenticated
				|| !aRequest.Base.valid() || !aRequest.Head.valid() || aRequest.Base.algorithm() != aRequest.Head.algorithm()
				|| aRequest.Base.algorithm() != aRequest.Catalog.Authority.ObjectFormat
				|| !core::valid_cached_catalogue({ aRequest.File })) {
				return false;
			}
			const auto& key   = aRequest.Catalog.Authority;
			auto        actor = actor_for(cache_actor_key{ .Profile = key.Profile, .ProfileRevision = key.ProfileRevision, .Host = key.Host, .HostAuthority = key.HostAuthority, .Account = key.Account, .Repository = key.Repository, .ObjectFormat = key.ObjectFormat, .Mode = key.Mode });
			if (!actor) {
				return false;
			}
			const auto unavailable = cached_review_source_projection{ .Path = aRequest.File.Path, .BaseOid = std::string{ core::canonical_hex(aRequest.Base).view() }, .HeadOid = std::string{ core::canonical_hex(aRequest.Head).view() } };
			return submit_effect([this, actor, request = std::move(aRequest), aStop, completion = aCompletion](std::stop_token aJobStop) -> worker_completion {
				std::stop_source combined;
				std::stop_callback external{aStop, [&] { combined.request_stop(); }};
				std::stop_callback job{aJobStop, [&] { combined.request_stop(); }};
				auto result = retained_source(*actor, Caches, Catalogs, request, combined.get_token());
				return [answer=std::move(result), completion](implementation&) mutable { completion(std::move(answer)); }; }, [completion = aCompletion, unavailable](implementation&) { completion(unavailable); },
								 [](implementation&, core::epoch_failure) { }, std::nullopt, std::nullopt, std::nullopt, std::nullopt, actor);
		}

		// [[BPR-0654]]. See `epoch_application_host::resolve_changed_paths`.
		[[nodiscard]] auto resolve_changed_paths(
			changed_paths_request    aRequest,
			changed_paths_completion aCompletion) -> bool
		{
			if (!aCompletion || !aRequest.Left.valid() || aRequest.Left == aRequest.Right
				|| aRequest.Left.algorithm() != aRequest.Right.algorithm()) {
				return false;
			}
			auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Right);
			if (!reviewActor) {
				return false;
			}
			auto       actor    = std::move(reviewActor->first);
			const auto expected = std::move(reviewActor->second);
			auto       admitted = admit_job(
				std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
			if (!admitted.has_value()) {
				return false;
			}
			auto        control      = std::move(*admitted);
			const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const decisionLane = &DecisionLane;
			try {
				enqueue_actor_work(
					actor,
					[this,
					 actor,
					 expected,
					 gate,
					 decisionLane,
					 control,
					 request    = std::move(aRequest),
					 completion = std::move(aCompletion)]() mutable {
						auto observation = changed_paths_observation{
							.Request = request,
							.Status  = changed_paths_status::failure,
						};
						const auto workStop = control->Stop.get_token();
						const auto statusOf = [](const git_cache_error& aError) {
							return aError.Code == git_cache_failure::cancelled
								? changed_paths_status::cancelled
								: changed_paths_status::failure;
						};
						if (workStop.stop_requested()
							|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
							observation.Status = changed_paths_status::cancelled;
						} else if (!open_actor_cache(*actor, Caches, expected, workStop)) {
							observation.Status = changed_paths_status::stale;
						} else if (const auto inspected
								   = actor->Cache->inspect_commit_object(request.Left, workStop);
								   !inspected.has_value()) {
							observation.Status = statusOf(inspected.error());
						} else if (!application::valid(*inspected)
								   || inspected->Commit != request.Left) {
							observation.Status = changed_paths_status::failure;
						} else {
							observation.Status = changed_paths_status::resolved;
							for (const auto& path : request.Paths) {
								const auto left = actor->Cache->find_blob_at_path(
									git_cache_tree_entry_request{ .Commit = request.Left, .Path = path },
									workStop);
								if (!left.has_value()) {
									observation.Status = statusOf(left.error());
									observation.Changed.clear();
									break;
								}
								const auto right = actor->Cache->find_blob_at_path(
									git_cache_tree_entry_request{ .Commit = request.Right, .Path = path },
									workStop);
								if (!right.has_value()) {
									observation.Status = statusOf(right.error());
									observation.Changed.clear();
									break;
								}
								if (*left != *right) {
									observation.Changed.push_back(path);
								}
							}
						}
						enqueue_completion(
							gate,
							decisionLane,
							std::move(control),
							[answer  = std::move(observation),
							 deliver = std::move(completion)](implementation&) {
								deliver(answer);
							});
					});
			} catch (...) {
				finish_job(control);
				return false;
			}
			return true;
		}

		/// \brief [[BPR-1212]]. The review's cache actor and the session it
		/// expects, for a job asked about the accepted head; none when the
		/// request would be refused before admission.
		[[nodiscard]] auto review_cache_actor(
			const workbench_pull_request_authority& aAuthority,
			const core::git_commit_oid&             aHead)
			-> std::optional<std::pair<std::shared_ptr<authority_effect_actor>, git_cache_open_request>>
		{
			if (!aHead.valid() || !authorized_selected_diff_context(aAuthority, std::nullopt).has_value()
				|| State.AcceptedRevision->Head != aHead || State.PendingIngestion.has_value()) {
				return std::nullopt;
			}
			const auto& authority = *State.Authority;
			const auto  key       = cache_actor_key{
				.Profile         = authority.Profile,
				.ProfileRevision = authority.ProfileRevision,
				.Host            = authority.Host,
				.HostAuthority   = authority.HostAuthority,
				.Account         = authority.Account,
				.Repository      = authority.Repository,
				.ObjectFormat    = aHead.algorithm(),
				.Mode            = authority.Mode,
			};
			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(
					CacheActors, key, &decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return std::nullopt;
			}
			return std::pair{ std::move(actor), git_cache_open_request{
													.ApplicationSession = State.ApplicationSession,
													.Authority          = authority,
													.ObjectFormat       = aHead.algorithm(),
												} };
		}

		[[nodiscard]] auto resolve_github_base_numbering(
			github_base_numbering_request    aRequest,
			github_base_numbering_completion aCompletion) -> bool
		{
			if (!aCompletion || !aRequest.BaseTip.valid() || !aRequest.Left.valid() || aRequest.LeftPath.empty()
				|| aRequest.BaseTip.algorithm() != aRequest.Head.algorithm() || aRequest.Left.algorithm() != aRequest.Head.algorithm()) {
				return false;
			}
			auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
			if (!reviewActor) {
				return false;
			}
			auto admitted = admit_job(
				std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
			if (!admitted.has_value()) {
				return false;
			}
			auto        control      = std::move(*admitted);
			const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const decisionLane = &DecisionLane;
			auto        actor        = std::move(reviewActor->first);
			try {
				enqueue_actor_work(
					actor,
					[this,
					 actor,
					 expected = std::move(reviewActor->second),
					 gate,
					 decisionLane,
					 control,
					 request    = std::move(aRequest),
					 completion = std::move(aCompletion)]() mutable {
						auto observation = github_base_numbering_observation{
							.Request = request,
							.Status  = github_base_numbering_status::failure,
						};
						const auto workStop = control->Stop.get_token();
						const auto statusOf = [](const git_cache_error& aError) {
							return aError.Code == git_cache_failure::cancelled
								? github_base_numbering_status::cancelled
								: github_base_numbering_status::failure;
						};
						if (workStop.stop_requested()
							|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
							observation.Status = github_base_numbering_status::cancelled;
						} else if (!open_actor_cache(*actor, Caches, expected, workStop)) {
							observation.Status = github_base_numbering_status::stale;
						} else if (const auto derived = actor->Cache->derive_comparison_base(
									   git_cache_comparison_base_request{ .Head = request.Head, .SelectedBase = request.BaseTip }, workStop);
								   !derived.has_value()) {
							observation.Status = statusOf(derived.error());
						} else if (derived->Status != git_comparison_base_status::unique || !derived->Base || !derived->Base->valid()) {
							// No common ancestor, several, or objects the cache
							// does not hold (a sparse review): not knowable here.
							observation.Status = github_base_numbering_status::unavailable;
						} else {
							observation.MergeBase = derived->Base;
							if (*derived->Base == request.Left) {
								observation.Status = github_base_numbering_status::same;
							} else if (const auto atBase = actor->Cache->find_blob_at_path(
										   git_cache_tree_entry_request{ .Commit = *derived->Base, .Path = request.LeftPath }, workStop);
									   !atBase.has_value()) {
								observation.Status = statusOf(atBase.error());
							} else if (const auto atLeft = actor->Cache->find_blob_at_path(
										   git_cache_tree_entry_request{ .Commit = request.Left, .Path = request.LeftPath }, workStop);
									   !atLeft.has_value()) {
								observation.Status = statusOf(atLeft.error());
							} else {
								observation.Status = atBase->has_value() && *atBase == *atLeft
									? github_base_numbering_status::same
									: github_base_numbering_status::differs;
							}
						}
						enqueue_completion(
							gate,
							decisionLane,
							std::move(control),
							[answer  = std::move(observation),
							 deliver = std::move(completion)](implementation&) {
								deliver(answer);
							});
					});
			} catch (...) {
				finish_job(control);
				return false;
			}
			return true;
		}

		/// \brief Proves a comparison endpoint is durably sparse: the only
		/// state in which a missing blob may be read from the network.
		///
		/// \details Runs on the actor, which owns the cache session. Factored
		/// out of `materialize` for [[BPR-1327]], whose batched read for
		/// prepared files requires exactly the same proof.
		[[nodiscard]] static auto durably_sparse_anchor(
			authority_effect_actor&        aActor,
			authority_context&             aContext,
			const core::comparison_anchor& aAnchorToProve,
			const std::stop_token          aStop) -> bool
		{
			const auto snapshot = aContext.Catalog->snapshot(aStop);
			if (!snapshot || !snapshot->LatestLive
				|| snapshot->LatestLive->Base != aContext.Revision.Base
				|| snapshot->LatestLive->Head != aContext.Revision.Head) {
				return false;
			}
			const auto exactSparseAnchor = [&](const core::comparison_anchor& aAnchor) {
				if (!core::valid(aAnchor)
					|| aAnchor.Availability != core::anchor_availability::available
					|| (aAnchor.Identity.Kind != core::comparison_anchor_kind::epoch_base
						&& aAnchor.Identity.Kind != core::comparison_anchor_kind::epoch_head)
					|| aAnchor.Identity.Authority != aContext.Authority.Scope
					|| aAnchor.Identity.Incarnation != snapshot->Binding.Catalog.Incarnation) {
					return false;
				}
				const auto exact = aContext.Catalog->exact_anchors(
					exact_anchor_lookup_request{
						.Catalog = snapshot->Binding.Catalog,
						.Anchors = { aAnchor.Identity },
					},
					aStop);
				if (!exact || exact->Catalog != snapshot->Binding.Catalog
					|| exact->Anchors.size() != 1U || exact->Anchors.front() != aAnchor) {
					return false;
				}
				const auto recorded = aContext.Catalog->retained_residency(
					aAnchor.Identity.Epoch, aStop);
				if (!recorded || *recorded != git_revision_residency::sparse_commit_trees) {
					return false;
				}
				const auto retained = aActor.Cache->inspect_sparse_retained(
					aAnchor.Identity.Epoch, aStop);
				if (!retained || !application::valid(*retained)
					|| retained->Residency != git_revision_residency::sparse_commit_trees
					|| retained->Scope != aContext.Authority.Scope
					|| retained->Ordinal != aAnchor.Identity.Epoch
					|| (aAnchor.Identity.Kind == core::comparison_anchor_kind::epoch_base
							? retained->Base
							: retained->Head)
						!= aAnchor.Commit) {
					return false;
				}
				const auto inspected = aActor.Cache->inspect_sparse_commit_object(
					aAnchor.Commit, aStop);
				return inspected && application::valid(*inspected)
					&& inspected->Commit == aAnchor.Commit
					&& inspected->Residency == git_revision_residency::sparse_commit_trees;
			};
			return exactSparseAnchor(aAnchorToProve);
		}

		/// \details [[BPR-1327]]. `aPrepared` resolves a file the reviewer has
		/// not opened yet: the same lookups under the same held authority, kept
		/// as a prepared grant instead of replacing the selected one. A later
		/// selection of that exact file is answered from its prepared grant
		/// without running Git again.
		[[nodiscard]] auto resolve_selected_diff(
			const workbench_selected_diff_request& aRequest,
			const std::stop_token                  aStop,
			const bool                             aPrepared = false)
			-> workbench_selected_diff_observation
		{
			auto result = workbench_selected_diff_observation{
				.Request = aRequest,
				.Status  = workbench_selected_diff_status::failure,
			};
			if (aRequest.File.ContentKind == workbench_file_content_kind::binary) {
				result.Status = workbench_selected_diff_status::binary;
				return result;
			}
			if (!begin_blob_call()) {
				return result;
			}
			struct selected_call_guard final
			{
				implementation& Host;

				~selected_call_guard()
				{
					Host.finish_blob_call();
				}
			} call{ *this };
			const auto key = actor_key(aRequest);
			if (!key.has_value()) {
				return result;
			}
			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(
					CacheActors, *key, &decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return result;
			}
			const auto authorizationGeneration
				= selected_diff_authorization_generation(actor, aRequest);
			if (!authorizationGeneration.has_value()) {
				result.Status = workbench_selected_diff_status::stale;
				return result;
			}
			std::uint64_t grantSequence    = 0U;
			std::uint64_t preparedSequence = 0U;
			bool          selectedReview   = false;
			{
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				selectedReview   = actor->SelectedDiffAuthority == std::optional{ selected_diff_authority{
									   .Workbench = aRequest.WorkbenchAuthority,
									   .Diff      = aRequest.Diff,
								   } };
				preparedSequence = actor->PreparedGrantSequence;
				if (aPrepared) {
					if (!selectedReview || actor->BlobAuthorityGeneration != *authorizationGeneration) {
						result.Status = workbench_selected_diff_status::stale;
						return result;
					}
				} else if (selectedReview) {
					const auto sameSelection = actor->SelectedBlobGrant
						&& actor->SelectedBlobGrant->Request.WorkbenchAuthority == aRequest.WorkbenchAuthority
						&& actor->SelectedBlobGrant->Request.Diff == aRequest.Diff
						&& actor->SelectedBlobGrant->Request.File == aRequest.File;
					if (!sameSelection) {
						actor->SelectedBlobGrant.reset();
						++actor->SelectedBlobGrantSequence;
					}
					grantSequence = actor->SelectedBlobGrantSequence;
					if (actor->BlobAuthorityGeneration == *authorizationGeneration) {
						const auto prepared = std::ranges::find_if(
							actor->PreparedGrants, [&aRequest](const workbench_selected_diff_observation& aGrant) {
								return aGrant.Request.WorkbenchAuthority == aRequest.WorkbenchAuthority
									&& aGrant.Request.Diff == aRequest.Diff && aGrant.Request.File == aRequest.File;
							});
						if (prepared != actor->PreparedGrants.end()) {
							// [[BPR-1327]]. Prepared under this exact authority: the
							// same blobs and paths, bound to this request.
							auto reused = workbench_selected_diff_observation{
								.Request   = aRequest,
								.Status    = workbench_selected_diff_status::available,
								.LeftBlob  = prepared->LeftBlob,
								.RightBlob = prepared->RightBlob,
								.LeftPath  = prepared->LeftPath,
								.RightPath = prepared->RightPath,
							};
							if (reused.LeftBlob && reused.LeftPath) {
								reused.LeftEndpoint = epoch_application_host::mint_selected_diff_endpoint(
									aRequest, core::diff_side::left, *reused.LeftBlob, *reused.LeftPath);
							}
							if (reused.RightBlob && reused.RightPath) {
								reused.RightEndpoint = epoch_application_host::mint_selected_diff_endpoint(
									aRequest, core::diff_side::right, *reused.RightBlob, *reused.RightPath);
							}
							actor->SelectedBlobGrant = reused;
							return reused;
						}
					}
				}
			}
			auto waiter = std::shared_ptr<selected_diff_waiter>{ };
			try {
				waiter = std::make_shared<selected_diff_waiter>();
			} catch (...) {
				return result;
			}
			auto admitted = admit_job(
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				false,
				false,
				aPrepared);
			if (!admitted.has_value()) {
				return result;
			}
			auto       control  = std::move(*admitted);
			const auto expected = git_cache_open_request{
				.ApplicationSession = aRequest.Diff.ApplicationSession,
				.Authority          = aRequest.Diff.Authority,
				.ObjectFormat       = aRequest.Diff.Comparison.Left.Commit.algorithm(),
			};
			try {
				// [[BPR-1359]]. The reviewer's own resolve does not wait out a
				// prepared exact read; a prepared resolve is preparation.
				auto turn = aPrepared ? std::shared_ptr<reviewer_work>{ } : std::make_shared<reviewer_work>(actor);
				enqueue_actor_work(
					actor,
					[this,
					 actor,
					 admittedGeneration = *authorizationGeneration,
					 grantSequence,
					 preparedSequence,
					 selectedReview,
					 aPrepared,
					 expected,
					 aRequest,
					 aStop,
					 waiter,
					 control,
					 reviewer = std::move(turn)] {
						auto observation = workbench_selected_diff_observation{
							.Request = aRequest,
							.Status  = workbench_selected_diff_status::failure,
						};
						auto publicationAuthorityLock = std::unique_lock<std::mutex>{ };
						try {
							auto forwardCallerStop = std::stop_callback{
								aStop,
								[control] {
									static_cast<void>(control->Stop.request_stop());
								}
							};
							const auto workStop = control->Stop.get_token();
							if (workStop.stop_requested()
								|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
								observation.Status = workbench_selected_diff_status::cancelled;
							} else if (selected_diff_authorization_generation(actor, aRequest)
									   != admittedGeneration) {
								observation.Status = workbench_selected_diff_status::stale;
							} else if (open_actor_cache(*actor, Caches, expected, workStop).has_value()) {
								const auto lookup = [&](const std::filesystem::path& aPath,
														const core::git_commit_oid   aCommit)
									-> git_cache_result<std::optional<core::git_blob_oid>> {
									return actor->Cache->find_blob_at_path(
										git_cache_tree_entry_request{
											.Commit = aCommit,
											.Path   = aPath,
										},
										workStop);
								};
								struct endpoint_resolution final
								{
									std::optional<core::git_blob_oid>    Blob;
									std::optional<std::filesystem::path> Path;
									bool                                 Ambiguous = false;
								};
								const auto resolveEndpoint = [&](const core::git_commit_oid aCommit)
									-> git_cache_result<endpoint_resolution> {
									const auto current = lookup(aRequest.File.CurrentPath, aCommit);
									if (!current.has_value()) {
										return std::unexpected{ current.error() };
									}
									if (aRequest.File.Change
										!= workbench_file_change_kind::renamed) {
										return endpoint_resolution{
											.Blob = *current,
											.Path = current->has_value()
												? std::optional{ aRequest.File.CurrentPath }
												: std::nullopt,
										};
									}
									const auto previous = lookup(*aRequest.File.PreviousPath, aCommit);
									if (!previous.has_value()) {
										return std::unexpected{ previous.error() };
									}
									if (current->has_value() && previous->has_value()) {
										return endpoint_resolution{ .Ambiguous = true };
									}
									return endpoint_resolution{
										.Blob = current->has_value() ? *current : *previous,
										.Path = current->has_value()
											? std::optional{ aRequest.File.CurrentPath }
											: previous->has_value()
											? aRequest.File.PreviousPath
											: std::nullopt,
									};
								};
								auto left = resolveEndpoint(
									aRequest.Diff.Comparison.Left.Commit);
								auto right          = left.has_value()
									? resolveEndpoint(
										  aRequest.Diff.Comparison.Right.Commit)
									: git_cache_result<endpoint_resolution>{
										  std::unexpected{ left.error() }
									  };
								auto endpointsReady = left.has_value() && right.has_value();
								if (!endpointsReady) {
									const auto failure = !left.has_value()
										? left.error()
										: right.error();
									observation.Status = failure == git_cache_failure::cancelled
										? workbench_selected_diff_status::cancelled
										: workbench_selected_diff_status::failure;
								}
								if (endpointsReady
									&& aRequest.File.Change
										== workbench_file_change_kind::renamed) {
									const auto leftNeedsPair
										= left->Ambiguous || !left->Blob.has_value();
									const auto rightNeedsPair
										= right->Ambiguous || !right->Blob.has_value();
									if (leftNeedsPair || rightNeedsPair) {
										const auto pairs = actor->Cache->find_rename_pairs(
											git_cache_rename_pair_request{
												.Left         = aRequest.Diff.Comparison.Left.Commit,
												.Right        = aRequest.Diff.Comparison.Right.Commit,
												.CurrentPath  = aRequest.File.CurrentPath,
												.PreviousPath = aRequest.File.PreviousPath,
											},
											workStop);
										if (!pairs.has_value()) {
											observation.Status
												= pairs.error() == git_cache_failure::cancelled
												? workbench_selected_diff_status::cancelled
												: workbench_selected_diff_status::failure;
											endpointsReady = false;
										} else {
											auto boundPair = std::optional<git_rename_pair>{ };
											for (auto index = std::size_t{ 0U };
												 index < pairs->Count;
												 ++index) {
												const auto& pair     = pairs->Records[index];
												const auto  accepted = [&](const std::filesystem::path& aPath) {
													return aPath == aRequest.File.CurrentPath
														|| (aRequest.File.PreviousPath.has_value()
															&& aPath == *aRequest.File.PreviousPath);
												};
												if ((!left->Path.has_value()
													 || pair.LeftPath == *left->Path)
													&& (!right->Path.has_value()
														|| pair.RightPath == *right->Path)
													&& (accepted(pair.LeftPath)
														|| accepted(pair.RightPath))) {
													if (boundPair.has_value()) {
														boundPair.reset();
														break;
													}
													boundPair = pair;
												}
											}
											if (!boundPair.has_value()) {
												observation.Status = leftNeedsPair && rightNeedsPair
													? workbench_selected_diff_status::ambiguous_both
													: leftNeedsPair
													? workbench_selected_diff_status::ambiguous_left
													: workbench_selected_diff_status::ambiguous_right;
												endpointsReady     = false;
											} else {
												const auto exactLeft = lookup(
													boundPair->LeftPath,
													aRequest.Diff.Comparison.Left.Commit);
												const auto exactRight = exactLeft.has_value()
													? lookup(
														  boundPair->RightPath,
														  aRequest.Diff.Comparison.Right.Commit)
													: git_cache_result<std::optional<core::git_blob_oid>>{
														  std::unexpected{ exactLeft.error() }
													  };
												if (!exactLeft.has_value() || !exactRight.has_value()) {
													const auto failure = !exactLeft.has_value()
														? exactLeft.error()
														: exactRight.error();
													observation.Status
														= failure == git_cache_failure::cancelled
														? workbench_selected_diff_status::cancelled
														: workbench_selected_diff_status::failure;
													endpointsReady = false;
												} else if (!exactLeft->has_value()
														   || !exactRight->has_value()
														   || (left->Blob.has_value()
															   && left->Blob != *exactLeft)
														   || (right->Blob.has_value()
															   && right->Blob != *exactRight)) {
													observation.Status
														= workbench_selected_diff_status::failure;
													endpointsReady = false;
												} else {
													left = endpoint_resolution{
														.Blob = *exactLeft,
														.Path = boundPair->LeftPath,
													};
													right = endpoint_resolution{
														.Blob = *exactRight,
														.Path = boundPair->RightPath,
													};
												}
											}
										}
									}
								}
								if (endpointsReady && !left->Blob.has_value()
									&& !right->Blob.has_value()) {
									observation.Status = workbench_selected_diff_status::both_missing;
									endpointsReady     = false;
								}
								// Provider M/type/unchanged facts require material on both
								// selected sides. A different comparison basis may genuinely
								// lack the path; report that instead of publishing a normal M
								// with invented empty source or substituting another commit.
								const auto requiresBoth = aRequest.File.Change == workbench_file_change_kind::modified
									|| aRequest.File.Change == workbench_file_change_kind::type_changed
									|| aRequest.File.Change == workbench_file_change_kind::unchanged;
								if (endpointsReady && requiresBoth && (!left->Blob || !right->Blob)) {
									observation.Status = !left->Blob ? workbench_selected_diff_status::missing_left
																	 : workbench_selected_diff_status::missing_right;
									endpointsReady     = false;
								}
								if (endpointsReady) {
									publicationAuthorityLock = std::unique_lock{
										actor->BlobAuthorityMutex
									};
									if (workStop.stop_requested()
										|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
										observation.Status = workbench_selected_diff_status::cancelled;
									} else if (const auto held = std::optional{ selected_diff_authority{
												   .Workbench = aRequest.WorkbenchAuthority,
												   .Diff      = aRequest.Diff,
											   } };
											   (actor->SelectedDiffAuthority != held
												&& actor->RealignmentAuthority != held)
											   || actor->BlobAuthorityGeneration
												   != admittedGeneration
											   || (aPrepared
												   && (actor->SelectedDiffAuthority != held
													   || actor->PreparedGrantSequence != preparedSequence))
											   || (!aPrepared && selectedReview && actor->SelectedBlobGrantSequence != grantSequence)) {
										observation.Status = workbench_selected_diff_status::stale;
									} else {
										observation.Status = workbench_selected_diff_status::available;
										if (left->Blob.has_value() && left->Path.has_value()) {
											observation.LeftEndpoint
												= epoch_application_host::mint_selected_diff_endpoint(
													aRequest,
													core::diff_side::left,
													*left->Blob,
													*left->Path);
										}
										if (right->Blob.has_value() && right->Path.has_value()) {
											observation.RightEndpoint
												= epoch_application_host::mint_selected_diff_endpoint(
													aRequest,
													core::diff_side::right,
													*right->Blob,
													*right->Path);
										}
										observation.LeftBlob  = std::move(left->Blob);
										observation.RightBlob = std::move(right->Blob);
										observation.LeftPath  = std::move(left->Path);
										observation.RightPath = std::move(right->Path);
										if (aPrepared) {
											std::erase_if(actor->PreparedGrants, [&aRequest](const workbench_selected_diff_observation& aGrant) {
												return aGrant.Request.File == aRequest.File;
											});
											if (actor->PreparedGrants.size() >= core::maximum_prepared_comparison_window) {
												actor->PreparedGrants.erase(actor->PreparedGrants.begin());
											}
											actor->PreparedGrants.push_back(observation);
										} else if (selectedReview) {
											actor->SelectedBlobGrant = observation;
										}
									}
								}
							}
						} catch (...) {
							observation.Status = workbench_selected_diff_status::failure;
						}
						auto waiterLock     = std::unique_lock{ waiter->Mutex };
						waiter->Result      = std::move(observation);
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
					// [[BPR-1344]]. A file prepared for the reviewer waits behind their own work.
					aPrepared ? profile_epoch_actor_priority::preparation : profile_epoch_actor_priority::reviewer);
			} catch (...) {
				finish_job(control);
				return result;
			}

			// Registered before the lock is taken, for the reason `materialize`
			// records: an already-stopped token invokes its callback during
			// construction, and doing that under the waiter's mutex would
			// deadlock. BPR-0397.
			auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
														{
															const auto guard  = std::lock_guard{ waiter->Mutex };
															waiter->Abandoned = true;
														}
														waiter->Completed.notify_all();
													} };
			std::unique_lock lock{ waiter->Mutex };
			static_cast<void>(waiter->Completed.wait_for(
				lock, Limits.BlobWait, [&waiter] {
					return waiter->Result.has_value() || waiter->Abandoned;
				}));
			if (waiter->Result.has_value()) {
				return std::move(*waiter->Result);
			}
			// `result` already carries the request. Bounded by `BlobWait` rather
			// than `WorkspaceWait` because this is the same class of work as
			// `materialize`: a read of objects the cache already holds, queued
			// behind whatever the repository actor is running.
			result.Status = waiter->Abandoned
				? workbench_selected_diff_status::cancelled
				: workbench_selected_diff_status::failure;
			return result;
		}

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

		[[nodiscard]] auto search_actor(const core::source_search_scope& aScope) const -> std::shared_ptr<authority_effect_actor>
		{
			std::scoped_lock actorsLock{ CacheActorsMutex };
			for (const auto& [key, actor] : CacheActors) {
				static_cast<void>(key);
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->SearchScope == aScope && actor->BlobAuthority == blob_authority{ aScope.Session, aScope.Authority }) {
					return actor;
				}
			}
			return { };
		}

		[[nodiscard]] auto bind_source_search(const workbench_pull_request_authority& aAuthority, const core::comparison_pair& aComparison, std::optional<core::document_tab_id> aTab) -> std::optional<core::source_search_scope>
		{
			if (!authorized_selected_diff_context(aAuthority, { }, false) || !core::valid_exact_pair(aComparison) || !Active
				|| aComparison.Left.Identity.Authority != Active->Authority.Scope || aComparison.Right.Identity.Authority != Active->Authority.Scope
				|| aComparison.Left.Identity.Incarnation != State.AcceptedComparison->Left.Identity.Incarnation
				|| aComparison.Right.Identity.Incarnation != State.AcceptedComparison->Right.Identity.Incarnation) {
				return { };
			}
			auto scope             = core::source_search_scope{ };
			scope.Session          = State.ApplicationSession;
			scope.Authority        = Active->Authority;
			scope.Comparison       = aComparison;
			scope.Tab              = aTab;
			scope.Target           = core::source_search_target::files;
			auto             actor = Active->Actor;
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->SearchScope && actor->SearchScope->Comparison == aComparison && actor->SearchScope->Tab == aTab) {
				return actor->SearchScope;
			}
			if (actor->SearchGeneration == std::numeric_limits<std::uint64_t>::max()) {
				return { };
			}
			scope.CatalogueGeneration = ++actor->SearchGeneration;
			if (!core::valid(scope)) {
				return { };
			}
			actor->SearchScope = scope;
			actor->SearchCorpus.reset();
			if (actor->SearchRead) {
				actor->SearchRead->Stop.request_stop();
			}
			return scope;
		}

		[[nodiscard]] static auto exact_search_anchors(authority_context& aContext, const core::comparison_pair& aPair, std::stop_token aStop) -> bool
		{
			const auto snapshot = aContext.Catalog->snapshot(aStop);
			if (!snapshot) {
				return false;
			}
			auto identities = std::vector{ aPair.Left.Identity };
			if (aPair.Right.Identity != aPair.Left.Identity) {
				identities.push_back(aPair.Right.Identity);
			}
			const auto anchors = aContext.Catalog->exact_anchors({ snapshot->Binding.Catalog, identities }, aStop);
			return anchors && anchors->Catalog == snapshot->Binding.Catalog && anchors->Anchors.size() == identities.size()
				&& anchors->Anchors.front() == aPair.Left && anchors->Anchors.back() == aPair.Right;
		}

		[[nodiscard]] static auto build_source_corpus(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const core::source_search_scope& aScope, std::stop_token aWorkStop) -> source_corpus_observation
		{
			if (!exact_search_anchors(*aContext, aScope.Comparison, aWorkStop)) {
				return { };
			}
			auto changes = aActor->Cache->changed_entries({ aScope.Comparison.Left.Commit, aScope.Comparison.Right.Commit }, aWorkStop);
			if (!changes) {
				return { changes.error() == git_cache_failure::quota_exceeded ? core::source_search_status::resource_limit : aWorkStop.stop_requested() ? core::source_search_status::cancelled
																																						: core::source_search_status::unavailable,
						 { } };
			}
			if (changes->size() > maximum_git_changed_entries) {
				return { core::source_search_status::resource_limit, { } };
			}
			auto corpus   = std::make_shared<source_corpus>();
			corpus->Scope = aScope;
			corpus->Entries.reserve(changes->size());
			corpus->OwnedBytes = sizeof(source_corpus) + changes->size() * sizeof(source_corpus_entry);
			for (auto& change : *changes) {
				if (aWorkStop.stop_requested()) {
					return { core::source_search_status::cancelled, { } };
				}
				if (!change.Left && !change.Right) {
					return { };
				}
				for (const auto& side : { change.Left, change.Right }) {
					if (!side) {
						continue;
					}
					if (side->Path.empty() || side->Path.size() > maximum_git_tree_entry_path_bytes || !side->Object.valid()
						|| side->Object.algorithm() != aScope.Comparison.Left.Commit.algorithm()
						|| core::inspect_source_search_query(side->Path) == core::source_search_input::invalid_utf8) {
						return { };
					}
					corpus->OwnedBytes += side->Path.capacity() + 1U;
				}
				if (corpus->OwnedBytes > maximum_git_changed_catalogue_bytes) {
					return { core::source_search_status::resource_limit, { } };
				}
				auto entry   = source_corpus_entry{ };
				entry.Left   = std::move(change.Left);
				entry.Right  = std::move(change.Right);
				entry.Change = change.Change;
				entry.File   = core::file_identity_of(entry.path());
				corpus->Entries.push_back(std::move(entry));
			}
			std::ranges::sort(corpus->Entries, { }, &source_corpus_entry::path);
			for (auto i = std::size_t{ 1U }; i < corpus->Entries.size(); ++i) {
				if (corpus->Entries[i - 1U].path() == corpus->Entries[i].path()) {
					return { };
				}
			}
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			if (aActor->SearchScope != aScope || !aActor->BlobAuthority || aWorkStop.stop_requested()) {
				return { };
			}
			aActor->SearchCorpus = corpus;
			return { core::source_search_status::complete, std::move(corpus) };
		}

		[[nodiscard]] static auto search_catalogue_turn(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation
		{
			auto stop     = std::stop_source{ };
			auto forward  = std::stop_callback{ aStop, [&] { stop.request_stop(); } };
			auto priority = prepared_read_registration{ *aActor, stop, false };
			if (!priority.admitted()) {
				return { core::source_search_status::searching, { } };
			}
			auto result = build_source_corpus(aActor, aContext, aScope, stop.get_token());
			if (stop.stop_requested() && !aStop.stop_requested()) {
				return { core::source_search_status::searching, { } };
			}
			return result;
		}
		[[nodiscard]] auto catalogue(const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation
		{
			if (aStop.stop_requested()) {
				return { core::source_search_status::cancelled, { } };
			}
			if (!begin_blob_call()) {
				return { };
			}
			struct call_guard
			{
				implementation& Host;
				~call_guard() { Host.finish_blob_call(); }
			} call{ *this };
			const auto actor = search_actor(aScope);
			if (!actor) {
				return { };
			}
			auto context = std::shared_ptr<authority_context>{ };
			{
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->SearchCorpus) {
					return { core::source_search_status::complete, actor->SearchCorpus };
				}
				context = actor->BlobContext.lock();
			}
			if (!context) {
				return { };
			}
			while (!aStop.stop_requested()) {
				if (search_actor(aScope) != actor) {
					return { };
				}
				const auto result = run_on_actor<source_corpus_observation>(actor, { .ApplicationSession = aScope.Session, .Authority = aScope.Authority, .ObjectFormat = aScope.Comparison.Left.Commit.algorithm() }, [actor, context, aScope](std::stop_token aWorkStop) { return search_catalogue_turn(actor, context, aScope, aWorkStop); }, aStop);
				if (!result) {
					return { aStop.stop_requested() ? core::source_search_status::cancelled : core::source_search_status::unavailable, { } };
				}
				if (result->Status != core::source_search_status::searching) {
					return *result;
				}
				std::this_thread::yield();
			}
			return { core::source_search_status::cancelled, { } };
		}

		[[nodiscard]] auto acquire_source_corpus_material(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const std::shared_ptr<const source_corpus>& aCorpus, core::diff_side aSide, const std::shared_ptr<corpus_read_waiter>& aWaiter, const std::shared_ptr<job_control>& aControl) -> source_corpus_material
		{
			auto material = source_corpus_material{ };
			try {
				auto forwardJobStop = std::stop_callback{ aControl->Stop.get_token(), [aWaiter] { aWaiter->Stop.request_stop(); } };
				auto priority       = prepared_read_registration{ *aActor, aWaiter->Stop, false };
				if (!priority.admitted()) {
					return { diff_blob_status::cancelled, true, { } };
				}
				const auto workStop   = aWaiter->Stop.get_token();
				const auto scope      = aCorpus->Scope;
				auto       validGrant = false;
				{
					std::scoped_lock lock{ aActor->BlobAuthorityMutex };
					validGrant = aActor->SearchCorpus == aCorpus && aActor->SearchScope == scope;
				}
				if (validGrant && !workStop.stop_requested() && Consumer->active() && !BlobIngressClosing.load(std::memory_order_acquire)
					&& open_actor_cache(*aActor, Caches, { .ApplicationSession = scope.Session, .Authority = scope.Authority, .ObjectFormat = aWaiter->Blob.algorithm() }, workStop)) {
					auto bytes = std::optional<std::vector<std::uint8_t>>{ };
					{
						std::scoped_lock lock{ aActor->BlobAuthorityMutex };
						bytes = retained_blob(*aActor, aWaiter->Blob);
					}
					material.Cached = bytes.has_value();
					auto cached     = bytes ? git_cache_result<std::vector<std::uint8_t>>{ std::move(*bytes) } : aActor->Cache->materialize_blob({ aWaiter->Blob, maximum_git_cache_blob_materialization_bytes }, workStop);
					if (cached) {
						bytes = std::move(*cached);
					} else {
						bytes.reset();
						material.Status    = cached.error() == git_cache_failure::quota_exceeded ? diff_blob_status::materialization_limit : diff_blob_status::missing;
						const auto& anchor = aSide == core::diff_side::left ? scope.Comparison.Left : scope.Comparison.Right;
						if (cached.error() == git_cache_failure::object_mismatch && aContext->Handoff.SmartGit && aContext->Handoff.SmartGit->supports_sparse_commit_trees()
							&& durably_sparse_anchor(*aActor, *aContext, anchor, workStop)) {
							if (priority.admitted()) {
								auto smart               = git_smart_request_context{ };
								smart.ApplicationSession = aContext->Handoff.ApplicationSession;
								smart.Authority          = aContext->Authority;
								smart.Revision           = aContext->Revision;
								smart.Repository         = aContext->Handoff.Locator;
								smart.BaseRef            = aContext->Handoff.BaseRef;
								const auto transport     = aContext->Handoff.SmartGit->acquire(smart);
								if (transport && *transport) {
									const auto fetched = (*transport)->read_exact_blob({ smart, aWaiter->Blob, maximum_git_cache_blob_materialization_bytes }, workStop);
									if (fetched) {
										bytes.emplace(fetched->size());
										std::ranges::transform(*fetched, bytes->begin(), [](std::byte aByte) { return std::to_integer<std::uint8_t>(aByte); });
										// A mismatch or a collision detection both end here, at failure.
										if (blob_matches(aWaiter->Blob, *bytes) != blob_check::matches) {
											bytes.reset();
											material.Status = diff_blob_status::failure;
										}
									} else {
										material.Status = fetched.error().Failure == git_smart_transport_failure::response_limit ? diff_blob_status::materialization_limit : diff_blob_status::failure;
									}
								}
							}
						}
					}
					std::scoped_lock lock{ aActor->BlobAuthorityMutex };
					if (bytes && aActor->SearchCorpus == aCorpus && aActor->SearchScope == scope && !workStop.stop_requested()) {
						if (bytes->size() > maximum_git_cache_blob_materialization_bytes) {
							material.Status = diff_blob_status::materialization_limit;
						} else {
							retain_blob(*aActor, aWaiter->Blob, *bytes, true);
							material.Bytes  = std::make_shared<const std::vector<std::uint8_t>>(std::move(*bytes));
							material.Status = diff_blob_status::available;
						}
					}
				}
				if (workStop.stop_requested()) {
					material.Status = diff_blob_status::cancelled;
					std::scoped_lock lock{ aActor->BlobAuthorityMutex };
					material.Yielded = !aControl->Stop.stop_requested() && aActor->SearchCorpus == aCorpus && still_needed(aWaiter->Needed);
				}
			} catch (...) {
				material = { };
			}
			return material;
		}

		[[nodiscard]] auto read_source_corpus(const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop, source_corpus_read_needed aNeeded) -> source_corpus_material
		{
			if (aStop.stop_requested()) {
				return { diff_blob_status::cancelled, false, { } };
			}
			if (aSide != core::diff_side::left && aSide != core::diff_side::right) {
				return { };
			}
			if (!aCorpus || aEntry >= aCorpus->Entries.size() || !begin_blob_call()) {
				return { };
			}
			struct call_guard
			{
				implementation& Host;
				~call_guard() { Host.finish_blob_call(); }
			} call{ *this };
			const auto actor = search_actor(aCorpus->Scope);
			if (!actor) {
				return { };
			}
			const auto& side = aSide == core::diff_side::left ? aCorpus->Entries[aEntry].Left : aCorpus->Entries[aEntry].Right;
			if (!side) {
				return { diff_blob_status::missing, false, { } };
			}
			if (side->Mode != 0100644U && side->Mode != 0100755U) {
				return { diff_blob_status::generated_file_unsupported, false, { } };
			}
			auto context   = std::shared_ptr<authority_context>{ };
			auto waiter    = std::shared_ptr<corpus_read_waiter>{ };
			auto coalesced = false;
			{
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->SearchCorpus != aCorpus) {
					return { };
				}
				context = actor->BlobContext.lock();
				if (actor->SearchRead) {
					if (actor->SearchRead->Blob != side->Object || actor->SearchRead->Corpus.lock() != aCorpus) {
						return { diff_blob_status::cancelled, true, { } };
					}
					waiter         = actor->SearchRead;
					waiter->Needed = std::move(aNeeded);
					coalesced      = true;
				} else {
					waiter            = std::make_shared<corpus_read_waiter>();
					waiter->Blob      = side->Object;
					waiter->Corpus    = aCorpus;
					waiter->Needed    = std::move(aNeeded);
					actor->SearchRead = waiter;
				}
			}
			if (!context) {
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->SearchRead == waiter && !coalesced) {
					actor->SearchRead.reset();
				}
				return { };
			}
			if (!coalesced) {
				auto admitted = admit_job({ }, { }, { }, { }, false, false, true);
				if (!admitted) {
					std::scoped_lock lock{ actor->BlobAuthorityMutex };
					if (actor->SearchRead == waiter) {
						actor->SearchRead.reset();
					}
					return { };
				}
				auto control = std::move(*admitted);
				try {
					enqueue_actor_work(actor, [this, actor, context, corpus = aCorpus, aSide, waiter, control] {
                        auto material = acquire_source_corpus_material(actor,context,corpus,aSide,waiter,control);

                        {
                            std::scoped_lock lock{actor->BlobAuthorityMutex};
                            if (actor->SearchRead == waiter) { actor->SearchRead.reset(); }
                        }
                        {
                            std::scoped_lock lock{waiter->Mutex};
                            waiter->Material = std::move(material);
                            waiter->Done = true;
                        }
                        const auto reserved = retire_actor_job(control);
                        waiter->Changed.notify_all();
                        release_actor_job_reservation(reserved,control->Preparation); }, profile_epoch_actor_priority::preparation);
				} catch (...) {
					finish_job(control);
					std::scoped_lock lock{ actor->BlobAuthorityMutex };
					if (actor->SearchRead == waiter) {
						actor->SearchRead.reset();
					}
					return { };
				}
			}
			auto wake = std::stop_callback{ aStop, [waiter, actor] {
											   {
												   std::scoped_lock lock{ actor->BlobAuthorityMutex };
												   if (!still_needed(waiter->Needed)) {
													   waiter->Stop.request_stop();
												   }
											   }
											   waiter->Changed.notify_all();
										   } };
			std::unique_lock lock{ waiter->Mutex };
			const auto       done = waiter->Changed.wait_for(lock, Limits.BlobWait, [&] { return waiter->Done || aStop.stop_requested(); });
			if (aStop.stop_requested()) {
				return { diff_blob_status::cancelled, false, { } };
			}
			if (!done) {
				waiter->Stop.request_stop();
				return { };
			}
			return waiter->Material;
		}

		/// \brief Prepares the files the reviewer is about to open. [[BPR-1327]]
		[[nodiscard]] auto prepare_selected_diffs(
			const std::vector<workbench_selected_diff_request>& aRequests,
			const std::stop_token                               aStop) -> epoch_prepared_comparisons
		{
			auto result = epoch_prepared_comparisons{ };
			if (aRequests.empty() || aRequests.size() > core::maximum_prepared_comparison_window) {
				return result;
			}
			const auto& first = aRequests.front();
			if (!std::ranges::all_of(aRequests, [&first](const workbench_selected_diff_request& aRequest) {
					return aRequest.WorkbenchAuthority == first.WorkbenchAuthority && aRequest.Diff == first.Diff;
				})) {
				return result;
			}
			for (const auto& request : aRequests) {
				if (aStop.stop_requested()) {
					return result;
				}
				auto observation = resolve_selected_diff(request, aStop, true);
				if (observation.Status == workbench_selected_diff_status::available) {
					result.Observations.push_back(std::move(observation));
				} else if (observation.Status == workbench_selected_diff_status::stale
						   || observation.Status == workbench_selected_diff_status::cancelled) {
					return result;
				}
			}
			fetch_prepared_blobs(first, result, aStop);
			return result;
		}

		/// \brief Reads the prepared files' missing sparse blobs in batches.
		///
		/// \details Only blobs a prepared grant names, only for an endpoint the
		/// actor proves durably sparse (the proof a single exact read needs),
		/// and only through a transport that reads batches. The network round
		/// trip runs on the caller's thread, never on the actor, so the
		/// reviewer's own work on the repository is not queued behind it.
		/// Verified bytes are retained for the authority that read them. A
		/// batch that fails, other than by cancellation, suspends preparation's
		/// single reads under the authority until one succeeds. [[BPR-1359]]
		void fetch_prepared_blobs(const workbench_selected_diff_request& aTemplate,
								  epoch_prepared_comparisons&            aResult,
								  const std::stop_token                  aStop)
		{
			if (aResult.Observations.empty() || aStop.stop_requested() || !begin_blob_call()) {
				return;
			}
			struct blob_call_guard final
			{
				implementation& Host;

				~blob_call_guard()
				{
					Host.finish_blob_call();
				}
			} call{ *this };
			const auto key = actor_key(aTemplate);
			if (!key.has_value()) {
				return;
			}
			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(
					CacheActors, *key, &decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return;
			}
			const auto authorization = blob_authority{
				.ApplicationSession = aTemplate.Diff.ApplicationSession,
				.Authority          = aTemplate.Diff.Authority,
			};
			auto context    = std::shared_ptr<authority_context>{ };
			auto generation = std::uint64_t{ 0U };
			auto left       = std::vector<core::git_blob_oid>{ };
			auto right      = std::vector<core::git_blob_oid>{ };
			{
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->BlobAuthority != std::optional{ authorization }) {
					return;
				}
				generation      = actor->BlobAuthorityGeneration;
				context         = actor->BlobContext.lock();
				const auto want = [&actor, &left, &right](const std::optional<core::git_blob_oid>& aBlob,
														  std::vector<core::git_blob_oid>&         aSide) {
					if (aBlob && std::ranges::find(left, *aBlob) == left.end()
						&& std::ranges::find(right, *aBlob) == right.end()
						&& !retained_blob(*actor, *aBlob)) {
						aSide.push_back(*aBlob);
					}
				};
				for (const auto& observation : aResult.Observations) {
					want(observation.LeftBlob, left);
					want(observation.RightBlob, right);
				}
			}
			if (!context || (left.empty() && right.empty())
				|| object_loading_for(context->Handoff.Shape) != pull_request_object_loading::selected_objects
				|| !context->Handoff.SmartGit || !context->Handoff.SmartGit->supports_sparse_commit_trees()) {
				return;
			}
			const auto comparison = aTemplate.Diff.Comparison;
			const auto durable    = run_on_actor<std::pair<bool, bool>>(
				actor,
				git_cache_open_request{
					.ApplicationSession = aTemplate.Diff.ApplicationSession,
					.Authority          = aTemplate.Diff.Authority,
					.ObjectFormat       = comparison.Left.Commit.algorithm(),
				},
				[actor, context, comparison](const std::stop_token aWorkStop) {
					return std::pair{
						durably_sparse_anchor(*actor, *context, comparison.Left, aWorkStop),
						durably_sparse_anchor(*actor, *context, comparison.Right, aWorkStop),
					};
				},
				aStop);
			if (!durable) {
				return;
			}
			auto wanted = durable->first ? std::move(left) : std::vector<core::git_blob_oid>{ };
			if (durable->second) {
				wanted.insert(wanted.end(), right.begin(), right.end());
			}
			if (wanted.empty()) {
				return;
			}
			const auto smartContext = git_smart_request_context{
				.ApplicationSession = context->Handoff.ApplicationSession,
				.Authority          = context->Authority,
				.Revision           = context->Revision,
				.Repository         = context->Handoff.Locator,
				.BaseRef            = context->Handoff.BaseRef,
			};
			const auto transport = context->Handoff.SmartGit->acquire(smartContext);
			if (!transport || !*transport) {
				return;
			}
			for (std::size_t offset = 0U; offset < wanted.size() && !aStop.stop_requested();
				 offset += maximum_git_exact_blob_batch) {
				const auto count   = std::min(maximum_git_exact_blob_batch, wanted.size() - offset);
				auto       request = git_smart_exact_blobs_request{
					.Context       = smartContext,
					.ExpectedBlobs = { wanted.begin() + static_cast<std::ptrdiff_t>(offset),
									   wanted.begin() + static_cast<std::ptrdiff_t>(offset + count) },
					.MaximumBytes  = maximum_git_exact_blob_batch_bytes,
				};
				const auto read = (*transport)->read_exact_blobs(request, aStop);
				++aResult.RemoteBatches;
				const auto cancelled = aStop.stop_requested()
					|| (!read && read.error().Failure == git_smart_transport_failure::cancelled);
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->BlobAuthority != std::optional{ authorization }
					|| actor->BlobAuthorityGeneration != generation) {
					return;
				}
				if (!read || read->size() != count) {
					// A transport that cannot batch reads nothing ahead; the
					// reviewer's open still reads its own file exactly.
					// [[BPR-1359]]. Nor are the prepared files' blobs then read
					// one at a time: until a batch succeeds, preparation backs
					// off rather than spend a process per blob.
					actor->PreparedReadsSuspended = actor->PreparedReadsSuspended || !cancelled;
					return;
				}
				actor->PreparedReadsSuspended = false;
				for (std::size_t index = 0U; index < count; ++index) {
					const auto& answer = (*read)[index];
					if (!answer || answer->size() > maximum_git_exact_blob_batch_bytes) {
						continue;
					}
					auto bytes = std::vector<std::uint8_t>{ };
					bytes.reserve(answer->size());
					for (const auto byte : *answer) {
						bytes.push_back(std::to_integer<std::uint8_t>(byte));
					}
					if (blob_matches(request.ExpectedBlobs[index], bytes) != blob_check::matches) {
						continue;
					}
					retain_blob(*actor, request.ExpectedBlobs[index], bytes);
					++aResult.RemoteBlobs;
				}
			}
		}

		/// What the host's own blob check found. [[BPR-1450]]
		enum class blob_check : std::uint8_t {
			matches,
			/// Not a SHA-1 blob, or the bytes are another object.
			mismatch,
			/// SHA-1 collision detection refused the bytes; no identity exists.
			collision_detected,
		};

		/// The host's own check that bytes are the blob they claim to be,
		/// beside the transport's, through the core's collision-detecting
		/// SHA-1. SHA-1 object IDs only; a SHA-256 blob is not retained from a
		/// batch. Every caller ends anything but a match at a failure.
		[[nodiscard]] static auto blob_matches(const core::git_blob_oid&        aBlob,
											   const std::vector<std::uint8_t>& aBytes) noexcept -> blob_check
		{
			if (aBlob.algorithm() != core::git_hash_algorithm::sha1) {
				return blob_check::mismatch;
			}
			auto hash = core::git_object_hasher{ core::git_hash_algorithm::sha1, core::git_object_type::blob, aBytes.size() };
			hash.update(aBytes);
			const auto digest = hash.finish();
			if (!digest) {
				return digest.error() == core::git_object_refusal::collision_detected ? blob_check::collision_detected : blob_check::mismatch;
			}
			return std::ranges::equal(std::span{ *digest }.first(20U), std::span{ aBlob.bytes() }.first(20U)) ? blob_check::matches : blob_check::mismatch;
		}

		/// \brief The actor's part of a blob read, up to the network.
		/// [[BPR-1360]]
		///
		/// \details Runs on the actor. Leaves `Remote` set when the blob must
		/// be read exactly from the network.
		void begin_actor_blob_read(actor_blob_read&                               aRead,
								   const std::shared_ptr<authority_effect_actor>& aActor,
								   const std::shared_ptr<authority_context>&      aSelected,
								   const blob_authority&                          aAuthorization,
								   const std::uint64_t                            aAdmittedGeneration,
								   const git_cache_open_request&                  aExpected,
								   const std::stop_source&                        aJobStop)
		{
			const auto& request  = aRead.Request;
			const auto  workStop = aJobStop.get_token();
			if (workStop.stop_requested() || !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
				aRead.Observation.Status = diff_blob_status::cancelled;
				return;
			}
			if (blob_authorization_generation(aActor, aAuthorization) != aAdmittedGeneration) {
				aRead.Observation.Status = diff_blob_status::failure;
				return;
			}
			if (!open_actor_cache(*aActor, Caches, aExpected, workStop).has_value()) {
				return;
			}
			aRead.Opened = true;
			aRead.Cached = aActor->Cache->materialize_blob(
				git_cache_blob_request{
					.Blob         = request.Blob,
					.MaximumBytes = request.MaximumBytes,
				},
				workStop);
			if (*aRead.Cached) {
				aRead.Bytes = std::move(**aRead.Cached);
				return;
			}
			if (aRead.Cached->error() != git_cache_failure::object_mismatch
				|| object_loading_for(aSelected->Handoff.Shape) != pull_request_object_loading::selected_objects
				|| blob_authority_for(*aSelected) != aAuthorization || !aSelected->Handoff.SmartGit
				|| !aSelected->Handoff.SmartGit->supports_sparse_commit_trees()
				|| blob_authorization_generation(aActor, aAuthorization) != aAdmittedGeneration) {
				return;
			}
			auto selectedPair = std::optional<core::comparison_pair>{ };
			{
				std::scoped_lock lock{ aActor->BlobAuthorityMutex };
				if (aActor->SelectedBlobGrant && exact_selected_blob_request(*aActor->SelectedBlobGrant, request)) {
					aRead.SelectedGrantSequence = aActor->SelectedBlobGrantSequence;
					selectedPair                = aActor->SelectedBlobGrant->Request.Diff.Comparison;
				} else if (const auto prepared = std::ranges::find_if(
							   aActor->PreparedGrants,
							   [&request](const workbench_selected_diff_observation& aGrant) {
								   return exact_selected_blob_request(aGrant, request);
							   });
						   prepared != aActor->PreparedGrants.end()) {
					// [[BPR-1327]]. A prepared file's own blob.
					aRead.PreparedGrantSequence = aActor->PreparedGrantSequence;
					selectedPair                = prepared->Request.Diff.Comparison;
				}
				if (selectedPair) {
					// Bytes already read and verified for this authority need
					// no second network read.
					aRead.Bytes = retained_blob(*aActor, request.Blob);
					if (aRead.Bytes && aRead.Bytes->size() > request.MaximumBytes) {
						aRead.Bytes.reset();
						aRead.RetainedOverLimit = true;
					}
				}
			}
			if (aRead.Bytes || aRead.RetainedOverLimit || (!aRead.SelectedGrantSequence && !aRead.PreparedGrantSequence)
				|| !selectedPair
				|| !durably_sparse_anchor(*aActor, *aSelected,
										  request.Side == core::diff_side::left ? selectedPair->Left : selectedPair->Right,
										  workStop)) {
				return;
			}
			// [[BPR-1359]]. A prepared file's blob read for preparation alone:
			// it yields to reviewer work on this actor, and is not spent at all
			// while this authority's batched read is failing.
			if (aRead.PreparedGrantSequence && !aRead.SelectedGrantSequence) {
				aRead.PreparedRead.emplace(*aActor, aJobStop);
				if (!aRead.PreparedRead->admitted()) {
					aRead.Observation.Status = aRead.PreparedRead->suspended() ? diff_blob_status::failure
																			   : diff_blob_status::cancelled;
					aRead.PreparedRead.reset();
					return;
				}
			}
			const auto smartContext = git_smart_request_context{
				.ApplicationSession = aSelected->Handoff.ApplicationSession,
				.Authority          = aSelected->Authority,
				.Revision           = aSelected->Revision,
				.Repository         = aSelected->Handoff.Locator,
				.BaseRef            = aSelected->Handoff.BaseRef,
			};
			auto transport = aSelected->Handoff.SmartGit->acquire(smartContext);
			if (transport && *transport) {
				// Assigned member by member: GCC 15 at -O3 misreads a moved
				// designated-initializer temporary's strings (tests/AGENTS.md).
				aRead.Transport = std::move(*transport);
				aRead.Remote.emplace();
				aRead.Remote->Context      = smartContext;
				aRead.Remote->ExpectedBlob = request.Blob;
				aRead.Remote->MaximumBytes = static_cast<std::size_t>(request.MaximumBytes);
				return;
			}
			if (!transport && transport.error().Failure == git_smart_transport_failure::cancelled) {
				aRead.Observation.Status = diff_blob_status::cancelled;
			}
			aRead.PreparedRead.reset();
		}

		/// \brief The one exact network read a blob read still owes, if any.
		/// [[BPR-1360]]
		///
		/// \details Touches no cache session and no actor state, so it may run
		/// off the actor while the actor waits for it.
		static void read_remote_blob(actor_blob_read& aRead, const std::stop_token aStop) noexcept
		{
			if (!aRead.Remote) {
				return;
			}
			try {
				const auto fetched = aRead.Transport->read_exact_blob(*aRead.Remote, aStop);
				if (fetched) {
					aRead.Bytes.emplace();
					aRead.Bytes->reserve(fetched->size());
					for (const auto byte : *fetched) {
						aRead.Bytes->push_back(std::to_integer<std::uint8_t>(byte));
					}
					aRead.FetchedRemotely = true;
				} else if (fetched.error().Failure == git_smart_transport_failure::cancelled) {
					aRead.Observation.Status = diff_blob_status::cancelled;
				} else if (fetched.error().Failure == git_smart_transport_failure::response_limit) {
					aRead.Observation.Status = diff_blob_status::materialization_limit;
				}
			} catch (...) {
				aRead.Bytes.reset();
				aRead.FetchedRemotely    = false;
				aRead.Observation.Status = diff_blob_status::failure;
			}
			aRead.Remote.reset();
			aRead.PreparedRead.reset();
		}

		/// \brief Publishes a blob read under the actor's authority lock.
		/// [[BPR-1360]]
		///
		/// \details Takes the lock if the caller does not already hold it and
		/// leaves it held, so the caller hands the result over before an
		/// invalidation can intervene.
		void finish_actor_blob_read(actor_blob_read&                               aRead,
									const std::shared_ptr<authority_effect_actor>& aActor,
									const std::shared_ptr<authority_context>&      aSelected,
									const blob_authority&                          aAuthorization,
									const std::uint64_t                            aAdmittedGeneration,
									const std::stop_token                          aStop,
									std::unique_lock<std::mutex>&                  aPublicationLock)
		{
			if (!aRead.Opened) {
				return;
			}
			const auto& request = aRead.Request;
			const auto& cached  = *aRead.Cached;
			if (aRead.Bytes.has_value()) {
				if (!aPublicationLock.owns_lock()) {
					aPublicationLock = std::unique_lock{ aActor->BlobAuthorityMutex };
				}
				if (aStop.stop_requested() || !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
					aRead.Observation.Status = diff_blob_status::cancelled;
				} else if (aActor->BlobAuthority != std::optional{ aAuthorization }
						   || aActor->BlobAuthorityGeneration != aAdmittedGeneration
						   || (aRead.SelectedGrantSequence
							   && (aActor->SelectedBlobGrantSequence != *aRead.SelectedGrantSequence
								   || !aActor->SelectedBlobGrant
								   || !exact_selected_blob_request(*aActor->SelectedBlobGrant, request)))
						   || (aRead.PreparedGrantSequence
							   && (aActor->PreparedGrantSequence != *aRead.PreparedGrantSequence
								   || std::ranges::none_of(aActor->PreparedGrants,
														   [&request](const workbench_selected_diff_observation& aGrant) {
															   return exact_selected_blob_request(aGrant, request);
														   })))) {
					aRead.Observation.Status = diff_blob_status::failure;
				} else {
					if (aRead.FetchedRemotely) {
						try {
							retain_blob(*aActor, request.Blob, *aRead.Bytes);
						} catch (...) {
							// Retention is an optimization; the read stands.
						}
					}
					if (aRead.SelectedGrantSequence) {
						retain_blob(*aActor, request.Blob, *aRead.Bytes);
					}
					aRead.Observation.Status = diff_blob_status::available;
					aRead.Observation.Bytes  = std::move(*aRead.Bytes);
				}
			} else if (!aRead.RetainedOverLimit && cached.error() == git_cache_failure::cancelled) {
				aRead.Observation.Status = diff_blob_status::cancelled;
			} else if (aRead.RetainedOverLimit || cached.error() == git_cache_failure::quota_exceeded) {
				aRead.Observation.Status = diff_blob_status::materialization_limit;
			} else if (cached.error() == git_cache_failure::object_mismatch
					   && (object_loading_for(aSelected->Handoff.Shape) != pull_request_object_loading::selected_objects
						   || !aSelected->Handoff.SmartGit || !aSelected->Handoff.SmartGit->supports_sparse_commit_trees())) {
				aRead.Observation.Status = diff_blob_status::missing;
			}
		}

		[[nodiscard]] auto materialize(
			const diff_blob_request& aRequest,
			const std::stop_token    aStop) -> diff_blob_observation
		{
			auto result = diff_blob_observation{
				.Context = aRequest.Context,
				.Side    = aRequest.Side,
				.Blob    = aRequest.Blob,
				.Status  = diff_blob_status::failure,
			};
			if (!begin_blob_call()) {
				return result;
			}
			struct blob_call_guard final
			{
				implementation& Host;

				~blob_call_guard()
				{
					Host.finish_blob_call();
				}
			} call{ *this };
			const auto key = actor_key(aRequest);
			if (
				!key.has_value()
				|| aRequest.MaximumBytes
					> maximum_git_cache_blob_materialization_bytes) {
				return result;
			}

			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(
					CacheActors, *key, &decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return result;
			}
			const auto authorization           = blob_authority_for(aRequest);
			const auto authorizationGeneration = blob_authorization_generation(
				actor, authorization);
			if (!authorizationGeneration) {
				return result;
			}
			auto selectedContext = std::shared_ptr<authority_context>{ };
			auto reviewerBlob    = false;
			{
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->BlobAuthority == std::optional{ authorization }
					&& actor->BlobAuthorityGeneration == *authorizationGeneration) {
					selectedContext = actor->BlobContext.lock();
					reviewerBlob    = actor->SelectedBlobGrant
						&& exact_selected_blob_request(*actor->SelectedBlobGrant, aRequest);
				}
			}
			if (!selectedContext) {
				return result;
			}
			// [[BPR-1344]]. A prepared comparison's read is preparation's job;
			// the reviewer's own selected file never is.
			const auto preparation = aRequest.Prepared && !reviewerBlob;
			auto       waiter      = std::shared_ptr<blob_waiter>{ };
			try {
				waiter = std::make_shared<blob_waiter>();
			} catch (...) {
				return result;
			}
			auto admitted = admit_job(
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				false,
				false,
				preparation);
			if (!admitted) {
				return result;
			}
			auto control = std::move(*admitted);

			const auto expected = git_cache_open_request{
				.ApplicationSession = aRequest.Context.ApplicationSession,
				.Authority          = aRequest.Context.Key.Authority,
				.ObjectFormat       = aRequest.Blob.algorithm(),
			};
			try {
				// [[BPR-1359]]. A blob of the reviewer's selected file is the
				// reviewer's read, and does not wait out a prepared one.
				auto turn = reviewerBlob ? std::make_shared<reviewer_work>(actor) : std::shared_ptr<reviewer_work>{ };
				enqueue_actor_work(
					actor,
					[this,
					 actor,
					 selectedContext,
					 authorization,
					 admittedGeneration = *authorizationGeneration,
					 expected,
					 aRequest,
					 aStop,
					 waiter,
					 control,
					 reviewer = std::move(turn)] {
						auto read                     = actor_blob_read{ aRequest };
						auto publicationAuthorityLock = std::unique_lock<std::mutex>{ };
						try {
							auto forwardCallerStop = std::stop_callback{
								aStop,
								[control] {
									static_cast<void>(control->Stop.request_stop());
								}
							};
							const auto workStop = control->Stop.get_token();
							begin_actor_blob_read(read, actor, selectedContext, authorization, admittedGeneration, expected,
												  control->Stop);
							read_remote_blob(read, workStop);
							finish_actor_blob_read(read, actor, selectedContext, authorization, admittedGeneration, workStop,
												   publicationAuthorityLock);
						} catch (...) {
							read.Observation.Status = diff_blob_status::failure;
						}
						auto observation    = std::move(read.Observation);
						auto waiterLock     = std::unique_lock{ waiter->Mutex };
						waiter->Result      = std::move(observation);
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
					preparation ? profile_epoch_actor_priority::preparation : profile_epoch_actor_priority::reviewer);
			} catch (...) {
				finish_job(control);
				return result;
			}

			// Registered before the lock is taken: a token that has already been
			// stopped invokes the callback during construction, and doing that
			// while holding the mutex would deadlock. Setting `Abandoned` under
			// the mutex before notifying means no wakeup can be lost, because the
			// predicate is evaluated on entry.
			auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
														{
															const auto guard  = std::lock_guard{ waiter->Mutex };
															waiter->Abandoned = true;
														}
														waiter->Completed.notify_all();
													} };
			std::unique_lock lock{ waiter->Mutex };
			// Bounded, where it used to wait forever. `waiter` is a shared owner,
			// so abandoning the wait is safe: the closure still completes and
			// writes its result into a record nothing reads.
			static_cast<void>(waiter->Completed.wait_for(
				lock, Limits.BlobWait, [&waiter] {
					return waiter->Result.has_value() || waiter->Abandoned;
				}));
			if (waiter->Result.has_value()) {
				return std::move(*waiter->Result);
			}
			// `result` already carries the request's identity. Say which of the
			// two releases happened rather than collapsing both to `failure`: a
			// cancelled read is the reviewer moving on, and an elapsed bound is an
			// actor that never got to it.
			result.Status = waiter->Abandoned
				? diff_blob_status::cancelled
				: diff_blob_status::failure;
			return result;
		}

		/// \brief Both sides of the reviewer's cold open, read at once.
		/// [[BPR-1360]]
		///
		/// \details One job on the actor does both sides' cache and grant
		/// work, as two `materialize` calls would. When both sides must be
		/// read from the network, the actor lends the left side's read to this
		/// caller, which is only waiting, and reads the right side itself, so
		/// the open waits about one round trip rather than two. Both are the
		/// reviewer's reads ([[BPR-1359]]): the job stops a prepared read in
		/// flight and none is admitted while it is queued or running, and
		/// only this caller's stop or the host's closing stops either read.
		/// Absent unless both blobs belong to the reviewer's selected file:
		/// the diff host then reads the sides one at a time, as before.
		[[nodiscard]] auto materialize_both(const diff_blob_request& aLeft,
											const diff_blob_request& aRight,
											const std::stop_token    aStop)
			-> std::optional<std::pair<diff_blob_observation, diff_blob_observation>>
		{
			if (aLeft.Side != core::diff_side::left || aRight.Side != core::diff_side::right
				|| aLeft.Context != aRight.Context) {
				return std::nullopt;
			}
			if (!begin_blob_call()) {
				return std::nullopt;
			}
			struct blob_call_guard final
			{
				implementation& Host;

				~blob_call_guard()
				{
					Host.finish_blob_call();
				}
			} call{ *this };
			const auto key = actor_key(aLeft);
			if (!key.has_value() || actor_key(aRight) != key
				|| std::max(aLeft.MaximumBytes, aRight.MaximumBytes) > maximum_git_cache_blob_materialization_bytes) {
				return std::nullopt;
			}
			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(CacheActors, *key, &decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return std::nullopt;
			}
			const auto authorization           = blob_authority_for(aLeft);
			const auto authorizationGeneration = blob_authorization_generation(actor, authorization);
			if (!authorizationGeneration) {
				return std::nullopt;
			}
			auto selectedContext = std::shared_ptr<authority_context>{ };
			{
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->BlobAuthority == std::optional{ authorization }
					&& actor->BlobAuthorityGeneration == *authorizationGeneration && actor->SelectedBlobGrant
					&& exact_selected_blob_request(*actor->SelectedBlobGrant, aLeft)
					&& exact_selected_blob_request(*actor->SelectedBlobGrant, aRight)) {
					selectedContext = actor->BlobContext.lock();
				}
			}
			if (!selectedContext) {
				return std::nullopt;
			}

			const auto unread = [](const diff_blob_request& aRequest) {
				auto observation    = diff_blob_observation{ };
				observation.Context = aRequest.Context;
				observation.Side    = aRequest.Side;
				observation.Blob    = aRequest.Blob;
				observation.Status  = diff_blob_status::failure;
				return observation;
			};
			auto result = std::pair{ unread(aLeft), unread(aRight) };
			auto waiter = std::shared_ptr<blob_pair_waiter>{ };
			try {
				waiter = std::make_shared<blob_pair_waiter>();
			} catch (...) {
				return result;
			}
			auto admitted = admit_job(std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
			if (!admitted) {
				return result;
			}
			auto       control  = std::move(*admitted);
			const auto expected = git_cache_open_request{
				.ApplicationSession = aLeft.Context.ApplicationSession,
				.Authority          = aLeft.Context.Key.Authority,
				.ObjectFormat       = aLeft.Blob.algorithm(),
			};
			try {
				auto turn = std::make_shared<reviewer_work>(actor);
				enqueue_actor_work(
					actor,
					[this,
					 actor,
					 selectedContext,
					 authorization,
					 admittedGeneration = *authorizationGeneration,
					 expected,
					 aLeft,
					 aRight,
					 aStop,
					 waiter,
					 control,
					 reviewer = std::move(turn)] {
						auto left                     = actor_blob_read{ aLeft };
						auto right                    = actor_blob_read{ aRight };
						auto publicationAuthorityLock = std::unique_lock<std::mutex>{ };
						try {
							auto forwardCallerStop = std::stop_callback{
								aStop, [control] { static_cast<void>(control->Stop.request_stop()); }
							};
							const auto workStop = control->Stop.get_token();
							begin_actor_blob_read(left, actor, selectedContext, authorization, admittedGeneration, expected,
												  control->Stop);
							if (left.Bytes || left.Remote) {
								begin_actor_blob_read(right, actor, selectedContext, authorization, admittedGeneration,
													  expected, control->Stop);
							} else {
								// The left side has already failed, and the
								// diff host reports the left side first: the
								// right side's read would be wasted.
								right.Observation.Status = diff_blob_status::cancelled;
							}
							read_both_remote_blobs(*waiter, left, right, workStop);
							finish_actor_blob_read(left, actor, selectedContext, authorization, admittedGeneration, workStop,
												   publicationAuthorityLock);
							finish_actor_blob_read(right, actor, selectedContext, authorization, admittedGeneration,
												   workStop, publicationAuthorityLock);
						} catch (...) {
							left.Observation.Status  = diff_blob_status::failure;
							right.Observation.Status = diff_blob_status::failure;
							left.Observation.Bytes.clear();
							right.Observation.Bytes.clear();
						}
						auto waiterLock     = std::unique_lock{ waiter->Mutex };
						waiter->Result      = std::pair{ std::move(left.Observation), std::move(right.Observation) };
						const auto reserved = retire_actor_job(control);
						struct actor_exit_reservation final
						{
							implementation& Host;
							bool            Reserved;

							~actor_exit_reservation()
							{
								Host.release_actor_job_reservation(Reserved);
							}
						} actorExit{ *this, reserved };
						waiterLock.unlock();
						waiter->Changed.notify_all();
					});
			} catch (...) {
				finish_job(control);
				return result;
			}

			// Registered before the lock is taken, per `materialize`.
			auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
														{
															const auto guard  = std::lock_guard{ waiter->Mutex };
															waiter->Abandoned = true;
														}
														waiter->Changed.notify_all();
													} };
			std::unique_lock lock{ waiter->Mutex };
			while (true) {
				const auto woken = waiter->Changed.wait_for(lock, Limits.BlobWait, [&waiter] {
					return waiter->Result.has_value() || waiter->Abandoned
						|| waiter->Lend == blob_pair_waiter::lent_read::offered;
				});
				if (waiter->Result.has_value()) {
					return std::move(waiter->Result);
				}
				if (!woken || waiter->Abandoned) {
					break;
				}
				// The actor lent this caller one side's network read.
				waiter->Lend  = blob_pair_waiter::lent_read::taken;
				auto lentRead = std::move(waiter->Lent);
				waiter->Lent  = { };
				lock.unlock();
				lentRead();
				lock.lock();
				waiter->Lend = blob_pair_waiter::lent_read::returned;
				waiter->Changed.notify_all();
			}
			waiter->Gone = true;
			waiter->Changed.notify_all();
			const auto status    = waiter->Abandoned ? diff_blob_status::cancelled : diff_blob_status::failure;
			result.first.Status  = status;
			result.second.Status = status;
			return result;
		}

		/// \brief Makes both sides' owed network reads, at once when both owe
		/// one. [[BPR-1360]]
		///
		/// \details Runs on the actor. The left side's read is lent to the
		/// caller waiting in `materialize_both`, and the right side's is made
		/// here meanwhile; the actor then waits for the lent read to come
		/// back. A lent read the caller never took, because it stopped
		/// waiting, is made here instead (a stopped job's read ends at once).
		static void read_both_remote_blobs(blob_pair_waiter&     aWaiter,
										   actor_blob_read&      aLeft,
										   actor_blob_read&      aRight,
										   const std::stop_token aStop) noexcept
		{
			auto lent = false;
			if (aLeft.Remote && aRight.Remote) {
				try {
					std::scoped_lock lock{ aWaiter.Mutex };
					if (!aWaiter.Gone && !aWaiter.Abandoned) {
						aWaiter.Lent = [&aLeft, aStop] { read_remote_blob(aLeft, aStop); };
						aWaiter.Lend = blob_pair_waiter::lent_read::offered;
						lent         = true;
					}
				} catch (...) {
					lent = false;
				}
			}
			if (lent) {
				aWaiter.Changed.notify_all();
			}
			read_remote_blob(aRight, aStop);
			if (lent) {
				std::unique_lock lock{ aWaiter.Mutex };
				aWaiter.Changed.wait(lock, [&aWaiter] {
					return aWaiter.Lend == blob_pair_waiter::lent_read::returned
						|| (aWaiter.Lend == blob_pair_waiter::lent_read::offered && (aWaiter.Gone || aWaiter.Abandoned));
				});
				lent         = aWaiter.Lend == blob_pair_waiter::lent_read::returned;
				aWaiter.Lend = blob_pair_waiter::lent_read::none;
				aWaiter.Lent = { };
			}
			if (!lent) {
				read_remote_blob(aLeft, aStop);
			}
		}

		[[nodiscard]] auto materialize_agent_workspace(
			const agent_workspace_materialization_request& aRequest,
			const std::stop_token                          aStop)
			-> std::expected<owned_agent_workspace, agent_workspace_error>
		{
			if (!begin_blob_call()) {
				return std::unexpected{ agent_workspace_failure::cancelled };
			}
			struct workspace_call_guard final
			{
				implementation& Host;

				~workspace_call_guard()
				{
					Host.finish_blob_call();
				}
			} call{ *this };
			const auto key = actor_key(aRequest);
			if (!key.has_value()) {
				return std::unexpected{ agent_workspace_failure::invalid_request };
			}
			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(
					CacheActors,
					*key,
					&decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return std::unexpected{ agent_workspace_failure::unavailable };
			}
			const auto authorization           = blob_authority_for(aRequest);
			const auto authorizationGeneration = blob_authorization_generation(
				actor,
				authorization);
			if (!authorizationGeneration.has_value()) {
				return std::unexpected{ agent_workspace_failure::stale };
			}
			auto waiter = std::shared_ptr<workspace_waiter>{ };
			try {
				waiter = std::make_shared<workspace_waiter>();
			} catch (...) {
				return std::unexpected{ agent_workspace_failure::unavailable };
			}
			auto admitted = admit_job(
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				false);
			if (!admitted.has_value()) {
				return std::unexpected{ agent_workspace_failure::unavailable };
			}
			auto       control  = std::move(*admitted);
			const auto expected = git_cache_open_request{
				.ApplicationSession = aRequest.ApplicationSession,
				.Authority          = aRequest.Authority,
				.ObjectFormat       = aRequest.Session.Binding.HeadOid.algorithm(),
			};
			try {
				enqueue_actor_work(
					actor,
					[this,
					 actor,
					 authorization,
					 admittedGeneration = *authorizationGeneration,
					 expected,
					 aRequest,
					 aStop,
					 waiter,
					 control] {
						auto result = std::expected<
							owned_agent_workspace,
							agent_workspace_error>{
							std::unexpected{ agent_workspace_failure::unavailable }
						};
						try {
							auto forwardCallerStop = std::stop_callback{
								aStop,
								[control] {
									static_cast<void>(control->Stop.request_stop());
								}
							};
							const auto workStop = control->Stop.get_token();
							if (
								workStop.stop_requested()
								|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
								result = std::unexpected{
									agent_workspace_failure::cancelled
								};
							} else if (
								blob_authorization_generation(actor, authorization)
								!= admittedGeneration) {
								result = std::unexpected{ agent_workspace_failure::stale };
							} else if (
								open_actor_cache(*actor, Caches, expected, workStop).has_value()) {
								auto materialized = actor->Cache->materialize_agent_workspace(
									aRequest,
									workStop);
								if (materialized.has_value()) {
									std::scoped_lock authorityLock{
										actor->BlobAuthorityMutex
									};
									if (
										workStop.stop_requested()
										|| !Consumer->active()
										|| actor->BlobAuthority
											!= std::optional{ authorization }
										|| actor->BlobAuthorityGeneration
											!= admittedGeneration) {
										static_cast<void>(
											actor->Cache->cleanup_agent_workspace(
												*materialized,
												{ }));
										result = std::unexpected{
											workStop.stop_requested()
												? agent_workspace_failure::cancelled
												: agent_workspace_failure::stale
										};
									} else {
										result = std::move(*materialized);
									}
								} else {
									auto failure = agent_workspace_failure::unavailable;
									if (
										materialized.error()
										== git_cache_failure::cancelled) {
										failure = agent_workspace_failure::cancelled;
									} else if (
										materialized.error()
										== git_cache_failure::invalid_authority) {
										failure = agent_workspace_failure::stale;
									}
									result = std::unexpected{ agent_workspace_error{
										failure,
										std::move(materialized.error().Detail) } };
								}
							}
						} catch (...) {
							result = std::unexpected{ agent_workspace_failure::unavailable };
						}
						auto waiterLock = std::unique_lock{ waiter->Mutex };
						// The caller may have given up while this was queued or
						// running. A materialized workspace it will never read is
						// a directory nobody owns, so remove it here rather than
						// leak it. Decided under the waiter's mutex so the caller
						// cannot abandon between the check and the store; the
						// removal path does not take this mutex, and nothing
						// contends for it once the caller is gone. BPR-0397.
						if (
							waiter->Abandoned && result.has_value()
							&& actor->Cache != nullptr) {
							static_cast<void>(actor->Cache->cleanup_agent_workspace(
								*result,
								{ }));
							result = std::unexpected{
								agent_workspace_failure::cancelled
							};
						}
						waiter->Result      = std::move(result);
						const auto reserved = retire_actor_job(control);
						struct actor_exit_reservation final
						{
							implementation& Host;
							bool            Reserved;

							~actor_exit_reservation()
							{
								Host.release_actor_job_reservation(Reserved);
							}
						} actorExit{ *this, reserved };
						waiterLock.unlock();
						waiter->Completed.notify_all();
					});
			} catch (...) {
				finish_job(control);
				return std::unexpected{ agent_workspace_failure::unavailable };
			}

			// Registered before the lock, per `materialize`'s deadlock note.
			auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
														{
															const auto guard  = std::lock_guard{ waiter->Mutex };
															waiter->Abandoned = true;
														}
														waiter->Completed.notify_all();
													} };
			std::unique_lock lock{ waiter->Mutex };
			static_cast<void>(waiter->Completed.wait_for(
				lock, Limits.WorkspaceWait, [&waiter] {
					return waiter->Result.has_value() || waiter->Abandoned;
				}));
			if (waiter->Result.has_value()) {
				return std::move(*waiter->Result);
			}
			// Bounded by `WorkspaceWait`, not `BlobWait`: this writes a whole
			// tree to disk behind the actor's FIFO. Say which release happened —
			// a caller that moved on is `cancelled`, an actor that never got to
			// the work is `unavailable`.
			return std::unexpected{ waiter->Abandoned
										? agent_workspace_failure::cancelled
										: agent_workspace_failure::unavailable };
		}

		[[nodiscard]] auto cleanup_agent_workspace(
			const owned_agent_workspace& aWorkspace,
			const std::stop_token        aStop)
			-> std::expected<void, agent_workspace_error>
		{
			if (!Consumer->active() || aWorkspace.ApplicationSession != WindowApplicationSession) {
				return std::unexpected{ agent_workspace_failure::cleanup_failed };
			}
			static_cast<void>(begin_cleanup_call());
			struct cleanup_call_guard final
			{
				implementation& Host;

				~cleanup_call_guard()
				{
					Host.finish_blob_call();
				}
			} call{ *this };
			const auto key = actor_key(aWorkspace);
			if (!key.has_value()) {
				return std::unexpected{ agent_workspace_failure::invalid_request };
			}
			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(
					CacheActors,
					*key,
					&decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return std::unexpected{ agent_workspace_failure::cleanup_failed };
			}
			auto waiter = std::shared_ptr<workspace_cleanup_waiter>{ };
			try {
				waiter = std::make_shared<workspace_cleanup_waiter>();
			} catch (...) {
				return std::unexpected{ agent_workspace_failure::cleanup_failed };
			}
			auto admitted = admit_job(
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				false,
				true);
			if (!admitted.has_value()) {
				return std::unexpected{ agent_workspace_failure::cleanup_failed };
			}
			auto       control  = std::move(*admitted);
			const auto expected = git_cache_open_request{
				.ApplicationSession = aWorkspace.ApplicationSession,
				.Authority          = aWorkspace.Authority,
				.ObjectFormat       = aWorkspace.Workspace.PinnedHead->algorithm(),
			};
			try {
				enqueue_actor_work(
					actor,
					[this, actor, expected, aWorkspace, aStop, waiter, control] {
						auto result = std::expected<void, agent_workspace_error>{ };
						try {
							auto forwardCallerStop = std::stop_callback{
								aStop,
								[control] {
									static_cast<void>(control->Stop.request_stop());
								}
							};
							if (
								!open_actor_cache(*actor, Caches, expected, control->Stop.get_token())) {
								result = std::unexpected{
									agent_workspace_failure::cleanup_failed
								};
							} else {
								const auto cleaned = actor->Cache->cleanup_agent_workspace(
									aWorkspace,
									control->Stop.get_token());
								if (!cleaned.has_value()) {
									result = std::unexpected{ agent_workspace_error{
										cleaned.error() == git_cache_failure::cancelled
											? agent_workspace_failure::cancelled
											: agent_workspace_failure::cleanup_failed,
										std::move(cleaned.error().Detail) } };
								}
							}
						} catch (...) {
							result = std::unexpected{
								agent_workspace_failure::cleanup_failed
							};
						}
						auto waiterLock     = std::unique_lock{ waiter->Mutex };
						waiter->Result      = std::move(result);
						const auto reserved = retire_actor_job(control);
						struct actor_exit_reservation final
						{
							implementation& Host;
							bool            Reserved;

							~actor_exit_reservation()
							{
								Host.release_actor_job_reservation(Reserved);
							}
						} actorExit{ *this, reserved };
						waiterLock.unlock();
						waiter->Completed.notify_all();
					});
			} catch (...) {
				finish_job(control);
				return std::unexpected{ agent_workspace_failure::cleanup_failed };
			}

			// Registered before the lock, per `materialize`'s deadlock note.
			auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
														{
															const auto guard  = std::lock_guard{ waiter->Mutex };
															waiter->Abandoned = true;
														}
														waiter->Completed.notify_all();
													} };
			std::unique_lock lock{ waiter->Mutex };
			static_cast<void>(waiter->Completed.wait_for(
				lock, Limits.WorkspaceWait, [&waiter] {
					return waiter->Result.has_value() || waiter->Abandoned;
				}));
			if (waiter->Result.has_value()) {
				return std::move(*waiter->Result);
			}
			// Abandoning this one leaves nothing behind: the removal still runs
			// on the actor, and only its outcome goes unread. So the caller is
			// told what released it and nothing else needs undoing. BPR-0397.
			return std::unexpected{ waiter->Abandoned
										? agent_workspace_failure::cancelled
										: agent_workspace_failure::cleanup_failed };
		}

		void accept(accepted_pull_request_handoff aHandoff)
		{
			if (!Consumer->current() || !application::valid(aHandoff)
				|| aHandoff.ApplicationSession != State.ApplicationSession) {
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				return;
			}
			if (Closing || DeferredShutdown) {
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				return;
			}
			auto accepted = std::shared_ptr<accepted_pull_request_handoff>{ };
			try {
				accepted = std::make_shared<accepted_pull_request_handoff>(
					std::move(aHandoff));
			} catch (...) {
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				if (TurnActive) {
					defer_diagnostic(core::epoch_reason_code::unavailable);
				} else {
					publish_diagnostic(core::epoch_reason_code::unavailable);
				}
				return;
			}
			if (TurnActive || deferred_work_pending()) {
				static_cast<void>(defer_ordinary(
					deferred_accept{ std::move(accepted) }));
				return;
			}
			run_turn([this, &accepted] {
				accept_now(std::move(*accepted));
			});
		}

		void accept_now(accepted_pull_request_handoff aHandoff)
		{
			if (!Consumer->current() || Closing) {
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				return;
			}
			auto actor = std::shared_ptr<authority_effect_actor>{ };
			try {
				actor = actor_for(actor_key(aHandoff));
			} catch (...) {
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				publish_diagnostic(core::epoch_reason_code::unavailable);
				return;
			}
			if (!actor) {
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				publish_diagnostic(core::epoch_reason_code::quota_exceeded);
				return;
			}
			const auto token   = OfferToken + 1U;
			auto       offered = std::shared_ptr<accepted_pull_request_handoff>{ };
			try {
				offered = std::make_shared<accepted_pull_request_handoff>(
					std::move(aHandoff));
			} catch (...) {
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				publish_diagnostic(core::epoch_reason_code::unavailable);
				return;
			}
			auto admitted = admit_job(
				std::nullopt, std::nullopt, std::nullopt, token);
			if (!admitted) {
				if (offered->SmartGit) {
					offered->SmartGit->revoke();
				}
				publish_diagnostic(core::reason_for(admitted.error()));
				return;
			}
			auto control = std::move(*admitted);
			auto cleanup = std::shared_ptr<pending_context_cleanup>{ };
			if (Active && Active->requires_cache_cleanup()) {
				try {
					cleanup = std::make_shared<pending_context_cleanup>(
						pending_context_cleanup{ .Context = Active });
					auto cleanupAdmission = admit_job(
						std::nullopt,
						std::nullopt,
						std::nullopt,
						std::nullopt,
						true,
						true);
					if (!cleanupAdmission) {
						finish_job(control);
						if (offered->SmartGit) {
							offered->SmartGit->revoke();
						}
						publish_diagnostic(core::reason_for(cleanupAdmission.error()));
						return;
					}
					cleanup->Control = std::move(*cleanupAdmission);
				} catch (...) {
					finish_job(control);
					release_pending_cleanup(cleanup);
					if (offered->SmartGit) {
						offered->SmartGit->revoke();
					}
					publish_diagnostic(core::epoch_reason_code::unavailable);
					return;
				}
			}
			struct offer_admission_rollback final
			{
				implementation&                                Host;
				std::shared_ptr<job_control>                   Control;
				std::shared_ptr<accepted_pull_request_handoff> Offered;
				std::shared_ptr<pending_context_cleanup>       Cleanup;
				std::uint64_t                                  Token;
				bool                                           Armed    = true;
				bool                                           Diagnose = true;

				~offer_admission_rollback()
				{
					if (!Armed) {
						return;
					}
					Host.finish_job(Control);
					Host.release_pending_cleanup(Cleanup);
					if (Token == Host.OfferToken) {
						Host.PendingAuthority.reset();
						Host.PendingOfferToken.reset();
					}
					if (Offered->SmartGit) {
						Offered->SmartGit->revoke();
					}
					if (Diagnose) {
						Host.publish_diagnostic(core::epoch_reason_code::unavailable);
					}
				}

				void dismiss() noexcept
				{
					Armed = false;
				}
			} rollback{ *this, control, offered, cleanup, token };
			++OfferToken;
			if (PendingOfferToken) {
				cancel_offer_job(*PendingOfferToken);
			}
			const bool preservesActiveAuthority = Active && agrees(Active->Authority, *offered);
			if (Active && !preservesActiveAuthority) {
				if (cleanup
					&& !discard_context_preparation(
						cleanup->Context,
						std::nullopt,
						cleanup->Control)) {
					rollback.Diagnose = false;
					return;
				}
				if (cleanup) {
					cleanup->Context.reset();
					cleanup->Control.reset();
				}
			}

			PendingAuthority     = invalidation_identity(*offered);
			PendingOfferToken    = token;
			auto* const catalogs = &Catalogs;
			auto* const caches   = &Caches;
			auto* const entropy  = &Entropy;
			auto* const clock    = &Clock;
			// Keep worker and scheduling ownership distinct before entering the
			// call. Capturing and moving one local in separate arguments would
			// make the worker's actor depend on argument evaluation order.
			auto       workerActor = actor;
			const auto submitted   = submit_effect(
				[offered,
				 effectActor = std::move(workerActor),
				 catalogs,
				 caches,
				 entropy,
				 clock,
				 token,
				 cleanup, diagnostics = epoch_diagnostics{ DiagnosticLog, 0U, offered->ApplicationSession.value() }](
					const std::stop_token aStop) mutable -> worker_completion {
					struct lease_guard final
					{
						git_smart_authority Authority;

						~lease_guard()
						{
							if (Authority && !Authority->revoked()) {
								Authority->revoke();
							}
						}

						[[nodiscard]] auto release() noexcept -> git_smart_authority
						{
							return std::move(Authority);
						}
					} lease{ std::move(offered->SmartGit) };
					const auto diagnostic = [token, cleanup](
												const core::epoch_reason_code aReason)
						-> worker_completion {
						return [token, aReason, cleanup](implementation& aHost) {
							aHost.release_pending_cleanup(cleanup);
							if (token != aHost.OfferToken) {
								return;
							}
							aHost.PendingAuthority.reset();
							aHost.PendingOfferToken.reset();
							aHost.publish_diagnostic(aReason);
						};
					};
					if (aStop.stop_requested()) {
						return [cleanup](implementation& aHost) {
							aHost.release_pending_cleanup(cleanup);
						};
					}
					auto opened = diagnostics.perform(
						diagnostic_epoch_stage::catalog_open,
						[&] { return catalogs->open(
								  epoch_catalog_open_request{
									  .ApplicationSession = offered->ApplicationSession,
									  .Authority          = catalog_key(*offered),
								  },
								  aStop); });
					if (!opened) {
						const auto reason = core::reason_for(
							catalog_failure(opened.error()));
						return diagnostic(reason);
					}
					if (!*opened) {
						return diagnostic(
							core::epoch_reason_code::repair_required);
					}

					const auto initialBinding = (*opened)->binding();
					if (!application::valid(initialBinding)
						|| initialBinding.ObjectFormat != offered->ObjectFormat
						|| !agrees(initialBinding.Authority, *offered)) {
						return diagnostic(core::epoch_reason_code::invalid_identity);
					}

					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = offered->ApplicationSession,
						.Authority          = initialBinding.Authority,
						.ObjectFormat       = offered->ObjectFormat,
					};
					if (effectActor->Cache && effectActor->Cache->request() != expectedCache) {
						effectActor->Cache.reset();
					}
					const auto recovered = diagnostics.perform(
						diagnostic_epoch_stage::catalog_recovery,
						[&] { return recover_catalog(
								  **opened,
								  *caches,
								  *entropy,
								  *offered,
								  effectActor->Cache,
								  aStop); });
					if (!recovered) {
						return diagnostic(core::reason_for(recovered.error()));
					}

					// Occurrence evidence is supplementary to the live revision.
					// A failed timeline or sidecar read never refuses the accepted PR.
					auto occurrenceHistory = std::optional<core::epoch_occurrence_history>{ };
					auto currentEpoch      = std::optional<core::review_epoch_summary>{ };
					try {
						if (auto stored = (*opened)->occurrences(aStop)) {
							// Publish exact stored evidence even when optional reconciliation refuses.
							occurrenceHistory = *stored;
							if (const auto snapshot = (*opened)->snapshot(aStop);
								snapshot && snapshot->Binding == (*opened)->binding()) {
								currentEpoch = snapshot->LatestLive;
							}
							if (const auto observed = clock->observe_utc(); observed && core::valid(*observed)) {
								auto reconciled = reconcile_timeline(*offered, std::move(*stored), *observed);
								reconciled      = reconcile_local_occurrences(*offered, std::move(reconciled), **opened, currentEpoch, aStop);
								// [[BPR-1340]]. A new process has no cache open yet, and only a
								// new head's commit opened one, so an update recorded undated
								// stayed "Date unavailable" while the head did not move. Open the
								// actor's cache here when there is a date to read; a refusal
								// leaves the date unavailable and the offer unchanged.
								if (std::ranges::any_of(reconciled.Occurrences, [](const auto& aOccurrence) {
										return aOccurrence.DateSource == core::epoch_occurrence_date_source::unavailable;
									})) {
									static_cast<void>(open_actor_cache(*effectActor, *caches, expectedCache, aStop));
								}
								enrich_commit_dates(reconciled, effectActor->Cache.get(), aStop);
								std::ranges::sort(reconciled.Occurrences, core::epoch_occurrence_less);
								if ((*opened)->record_occurrences(reconciled, aStop)) {
									occurrenceHistory = std::move(reconciled);
								}
							}
						}
					} catch (...) {
						// Keep stored evidence, or the explicit unavailable fallback.
						// Publication must not depend on supplementary persistence.
					}

					std::optional<core::epoch_source_observation_id> source;
					for (std::size_t attempt = 0U;
						 attempt < maximum_identity_attempts;
						 ++attempt) {
						source = mint_identity<core::epoch_source_observation_id>(
							*entropy);
						if (!source) {
							break;
						}
						const auto exists = diagnostics.perform(
							diagnostic_epoch_stage::catalog_prepare,
							[&] { return (*opened)->source_observation_exists(*source, aStop); });
						if (!exists) {
							const auto reason = core::reason_for(
								catalog_failure(exists.error()));
							return diagnostic(reason);
						}
						if (!*exists) {
							break;
						}
						source.reset();
					}
					if (!source || aStop.stop_requested()) {
						return diagnostic(core::epoch_reason_code::unavailable);
					}

					const auto binding = (*opened)->binding();
					if (!application::valid(binding)
						|| binding.ObjectFormat != offered->ObjectFormat
						|| !agrees(binding.Authority, *offered)) {
						return diagnostic(core::epoch_reason_code::invalid_identity);
					}
					auto revision = accepted_revision(*offered, *source);
					auto context  = std::make_shared<authority_context>(
						std::move(*offered),
						binding.Authority,
						revision,
						std::move(*opened),
						std::move(effectActor),
						std::move(occurrenceHistory),
						std::move(currentEpoch));
					context->Handoff.SmartGit = lease.release();
					return [authorityContext = std::move(context), token, cleanup](
							   implementation& aHost) {
						if (aHost.Closing || token != aHost.OfferToken) {
							aHost.release_pending_cleanup(cleanup);
							if (authorityContext->Handoff.SmartGit) {
								authorityContext->Handoff.SmartGit->revoke();
							}
							return;
						}
						aHost.PendingAuthority.reset();
						aHost.PendingOfferToken.reset();
						auto previous = aHost.Active;
						if (cleanup && cleanup->Control) {
							if (!aHost.discard_context_preparation(
									cleanup->Context,
									std::nullopt,
									cleanup->Control)) {
								cleanup->Control.reset();
								cleanup->Context.reset();
								if (authorityContext->Handoff.SmartGit) {
									authorityContext->Handoff.SmartGit->revoke();
								}
								return;
							}
							cleanup->Control.reset();
							cleanup->Context.reset();
						}
						aHost.Active  = authorityContext;
						bool accepted = false;
						static_cast<void>(aHost.dispatch_core(
							core::pull_request_revision_accepted_event{
								.ApplicationSession = authorityContext->Handoff.ApplicationSession,
								.Authority          = authorityContext->Authority,
								.Revision           = authorityContext->Revision,
								.Basis              = authorityContext->Handoff.BaseBasis,
							},
							[&](const core::epoch_transition_output&) {
								accepted = aHost.State.Authority
									&& aHost.State.AcceptedRevision
									&& *aHost.State.Authority
										== authorityContext->Authority
									&& *aHost.State.AcceptedRevision
										== authorityContext->Revision;
								if (accepted && previous) {
									implementation::revoke_blob_authority(
										previous->Actor,
										implementation::blob_authority_for(*previous));
									if (previous->Handoff.SmartGit) {
										previous->Handoff.SmartGit->revoke();
									}
								}
							}));
						if (accepted) {
							if (!authorityContext->OccurrenceHistory && previous
								&& previous->Authority == authorityContext->Authority && previous->OccurrenceHistory) {
								// A failed read must clear previously projected evidence for this authority.
								authorityContext->OccurrenceHistory = core::epoch_occurrence_history{ };
							}
							if (authorityContext->OccurrenceHistory) {
								aHost.publish_occurrences(
									authorityContext->Catalog->binding().Catalog,
									*authorityContext->OccurrenceHistory,
									authorityContext->CurrentEpoch);
							}
							const auto invalidation = invalidation_identity(
								authorityContext->Handoff);
							const auto invalidationPending = std::ranges::any_of(
								aHost.DeferredInvalidations,
								[&](const auto& aPending) {
									return aPending == std::optional{ invalidation };
								});
							if (
								!aHost.Closing && !aHost.DeferredShutdown
								&& !invalidationPending) {
								implementation::authorize_blobs(
									authorityContext->Actor,
									implementation::blob_authority_for(*authorityContext),
									authorityContext);
							}
							return;
						}
						const auto previousStillOwned = previous
							&& aHost.State.Authority
							&& aHost.State.AcceptedRevision
							&& *aHost.State.Authority == previous->Authority
							&& *aHost.State.AcceptedRevision
								== previous->Revision;
						if (previousStillOwned) {
							aHost.Active = std::move(previous);
							if (authorityContext->Handoff.SmartGit) {
								authorityContext->Handoff.SmartGit->revoke();
							}
							return;
						}
						aHost.Active.reset();
						if (authorityContext->Handoff.SmartGit) {
							authorityContext->Handoff.SmartGit->revoke();
						}
						if (previous) {
							implementation::revoke_blob_authority(
								previous->Actor,
								implementation::blob_authority_for(*previous));
							if (previous->Handoff.SmartGit) {
								previous->Handoff.SmartGit->revoke();
							}
						}
					};
				},
				[token, cleanup](implementation& aHost) {
					aHost.release_pending_cleanup(cleanup);
					if (token == aHost.OfferToken) {
						aHost.PendingAuthority.reset();
						aHost.PendingOfferToken.reset();
						aHost.publish_diagnostic(
							core::epoch_reason_code::unavailable);
					}
				},
				[token, cleanup](
					implementation& aHost, const core::epoch_failure aFailure) {
					aHost.release_pending_cleanup(cleanup);
					if (token == aHost.OfferToken) {
						aHost.PendingAuthority.reset();
						aHost.PendingOfferToken.reset();
						aHost.publish_diagnostic(core::reason_for(aFailure));
					}
				},
				std::nullopt,
				std::nullopt,
				std::nullopt,
				token,
				std::move(actor),
				std::move(control));
			rollback.dismiss();
			if (!submitted) {
				if (token == OfferToken) {
					PendingAuthority.reset();
					PendingOfferToken.reset();
					if (offered->SmartGit) {
						offered->SmartGit->revoke();
					}
				}
				return;
			}
			if (Active) {
				cancel_authority_jobs();
			}
			if (Active && !preservesActiveAuthority) {
				auto retired = std::move(Active);
				revoke_blob_authority(
					retired->Actor, blob_authority_for(*retired));
				if (retired->Handoff.SmartGit) {
					retired->Handoff.SmartGit->revoke();
				}
				dispatch_core(core::pull_request_revision_invalidated_event{
					.ApplicationSession = State.ApplicationSession,
					.Authority          = retired->Authority,
					.Revision           = retired->Revision,
					.Generation         = State.EpochGeneration,
				});
			}
		}

		[[nodiscard]] auto active_matches(
			const pull_request_authority_invalidation& aInvalidation) const noexcept
			-> bool
		{
			return Active
				&& aInvalidation.ApplicationSession
				== Active->Handoff.ApplicationSession
				&& aInvalidation.Profile == Active->Authority.Profile
				&& aInvalidation.ProfileRevision
				== Active->Authority.ProfileRevision
				&& aInvalidation.Host == Active->Authority.Host
				&& aInvalidation.HostAuthority == Active->Authority.HostAuthority
				&& aInvalidation.Account == Active->Authority.Account
				&& aInvalidation.Viewer == Active->Authority.Viewer
				&& aInvalidation.Repository == Active->Authority.Repository
				&& aInvalidation.PullRequest == Active->Authority.PullRequest
				&& aInvalidation.IdentityGeneration
				== Active->Revision.Source.IdentityGeneration;
		}

		void invalidate(const pull_request_authority_invalidation aInvalidation)
		{
			if (Closing || !application::valid(aInvalidation)) {
				return;
			}
			if (active_matches(aInvalidation)) {
				cancel_authority_jobs();
				revoke_blob_authority(
					Active->Actor, blob_authority_for(*Active));
			}
			if (TurnActive || deferred_work_pending()) {
				static_cast<void>(defer_invalidation(aInvalidation));
				return;
			}
			run_turn([this, &aInvalidation] {
				invalidate_now(aInvalidation);
			});
		}

		void invalidate_now(
			const pull_request_authority_invalidation& aInvalidation)
		{
			if (PendingAuthority == std::optional{ aInvalidation }) {
				++OfferToken;
				if (PendingOfferToken) {
					cancel_offer_job(*PendingOfferToken);
				}
				PendingAuthority.reset();
				PendingOfferToken.reset();
			}
			if (!active_matches(aInvalidation)) {
				return;
			}
			cancel_authority_jobs();
			revoke_blob_authority(
				Active->Actor, blob_authority_for(*Active));
			if (Active->Handoff.SmartGit) {
				Active->Handoff.SmartGit->revoke();
			}
			dispatch_core(core::pull_request_revision_invalidated_event{
				.ApplicationSession = aInvalidation.ApplicationSession,
				.Authority          = Active->Authority,
				.Revision           = Active->Revision,
				.Generation         = State.EpochGeneration,
			});
			Active.reset();
		}

		void dispatch(const epoch_intent aIntent)
		{
			if (!Consumer->current() || Closing) {
				return;
			}
			if (TurnActive || deferred_work_pending()) {
				static_cast<void>(defer_ordinary(deferred_dispatch{ aIntent }));
				return;
			}
			run_turn([this, &aIntent] {
				dispatch_now(aIntent);
			});
		}

		void dispatch_now(const epoch_intent& aIntent)
		{
			if (!Consumer->current() || Closing) {
				return;
			}
			std::visit(
				[this](const auto& aValue) {
					dispatch_core(core::epoch_event{ aValue });
				},
				aIntent);
		}

		void shutdown()
		{
			Consumer->retire();
			close_blob_ingress_and_cancel_jobs();
			if (TurnActive || deferred_work_pending()) {
				defer_shutdown();
				return;
			}
			run_turn([this] {
				shutdown_now();
			});
		}

		void shutdown_now()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			++OfferToken;
			close_blob_ingress_and_cancel_jobs();
			PendingAuthority.reset();
			PendingOfferToken.reset();
			if (Active) {
				revoke_blob_authority(
					Active->Actor, blob_authority_for(*Active));
				if (Active->Handoff.SmartGit) {
					Active->Handoff.SmartGit->revoke();
				}
				dispatch_core(core::pull_request_revision_invalidated_event{
					.ApplicationSession = State.ApplicationSession,
					.Authority          = Active->Authority,
					.Revision           = Active->Revision,
					.Generation         = State.EpochGeneration,
				});
				Active.reset();
			}
			std::scoped_lock actorsLock{ CacheActorsMutex };
			CacheActors.clear();
		}

		void start_catalog_load(const core::load_epoch_catalog_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			auto actor = context->Actor;
			submit_effect(
				[context, aRequest, diagnostics = epoch_diagnostics{ DiagnosticLog, aRequest.Context.Request.value(), aRequest.Context.ApplicationSession.value() }](const std::stop_token aStop)
					-> worker_completion {
					const auto snapshot = diagnostics.perform(
						diagnostic_epoch_stage::catalog_prepare,
						[&] { return context->Catalog->snapshot(aStop); });
					if (!snapshot) {
						if (snapshot.error() == epoch_catalog_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::epoch_catalog_load_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = catalog_failure(snapshot.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(core::epoch_catalog_load_failed_event{
								.Context = aRequest.Context,
								.Failure = failure,
							});
						};
					}
					if (
						!application::valid(snapshot->Binding)
						|| snapshot->Binding != context->Catalog->binding()) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(core::epoch_catalog_load_failed_event{
								.Context = aRequest.Context,
								.Failure = core::epoch_failure::repair_required,
							});
						};
					}
					const auto catalog = snapshot->Binding.Catalog;
					return [aRequest, catalog](implementation& aHost) {
						aHost.dispatch_core(core::epoch_catalog_loaded_event{
							.Context = aRequest.Context,
							.Catalog = catalog,
						});
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::epoch_catalog_load_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
				},
				[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
					aHost.dispatch_core(core::epoch_catalog_load_failed_event{
						.Context = aRequest.Context,
						.Failure = aFailure,
					});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void start_revision_preparation(
			const core::prepare_epoch_revision_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			auto* const caches  = &Caches;
			auto* const entropy = &Entropy;
			auto* const clock   = &Clock;
			auto        actor   = context->Actor;
			submit_effect(
				[context, aRequest, caches, entropy, clock, diagnostics = epoch_diagnostics{ DiagnosticLog, aRequest.Context.Request.value(), aRequest.Context.ApplicationSession.value() }](
					const std::stop_token aStop) -> worker_completion {
					const auto cancelled = [aRequest]() -> worker_completion {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_revision_prepare_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					};
					const auto failed = [aRequest, diagnostics](const core::epoch_failure aFailure)
						-> worker_completion {
						diagnostics.failed(diagnostic_epoch_stage::revision_prepare, aFailure);
						return [aRequest, aFailure](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_revision_prepare_failed_event{
									.Context = aRequest.Context,
									.Failure = aFailure,
								});
						};
					};
					if (aStop.stop_requested()) {
						return cancelled();
					}
					const auto snapshot = diagnostics.perform(
						diagnostic_epoch_stage::catalog_prepare,
						[&] { return context->Catalog->snapshot(aStop); });
					if (!snapshot) {
						return snapshot.error() == epoch_catalog_failure::cancelled
							? cancelled()
							: failed(catalog_failure(snapshot.error()));
					}
					if (
						!application::valid(snapshot->Binding)
						|| snapshot->Binding != context->Catalog->binding()) {
						return failed(core::epoch_failure::repair_required);
					}
					const auto expectedCatalog = core::epoch_catalog_identity{
						.Incarnation = aRequest.Context.CatalogIncarnation,
						.Revision    = aRequest.Context.ExpectedCatalogRevision,
					};
					if (snapshot->Binding.Catalog != expectedCatalog) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					const auto effect = core::plan_epoch_preparation(
						context->Authority,
						aRequest.Context.Revision,
						expectedCatalog,
						core::epoch_preparation_catalog_observation{
							.LatestLive           = snapshot->LatestLive,
							.RetentionRehydration = snapshot->RetentionRehydration,
						});
					if (effect.Rejection != core::epoch_reason_code::none) {
						return failed(core::epoch_failure::malformed_observation);
					}

					const auto canonicalReview =
						[&](const std::optional<projected_append_head> aProjected)
						-> std::expected<
							std::optional<core::catalog_review_association>,
							core::epoch_failure> {
						if (!aRequest.Context.Revision.CurrentViewerReview) {
							return std::nullopt;
						}
						for (std::size_t attempt = 0U;
							 attempt < maximum_identity_attempts;
							 ++attempt) {
							const auto association = mint_identity<core::review_association_id>(
								*entropy);
							if (!association) {
								return std::unexpected{
									core::epoch_failure::unavailable
								};
							}
							const auto review = diagnostics.perform(
								diagnostic_epoch_stage::catalog_prepare,
								[&] { return context->Catalog->canonical_review(
										  canonical_review_request{
											  .Catalog             = expectedCatalog,
											  .Candidate           = *aRequest.Context.Revision.CurrentViewerReview,
											  .ProposedAssociation = *association,
											  .ProjectedHead       = aProjected,
										  },
										  aStop); });
							if (review) {
								return std::optional{ *review };
							}
							if (review.error()
								!= epoch_catalog_failure::
									operation_identity_conflict) {
								return std::unexpected{
									catalog_failure(review.error())
								};
							}
						}
						return std::unexpected{
							core::epoch_failure::catalog_conflict
						};
					};

					const auto observedTime = [&]()
						-> std::optional<core::observed_utc_milliseconds> {
						const auto observed = clock->observe_utc();
						return observed && core::valid(*observed)
							? observed
							: std::nullopt;
					};

					if (effect.Effect == core::epoch_preparation_effect::none) {
						const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
						const auto review    = canonicalReview(std::nullopt);
						const auto observed  = observedTime();
						if (!operation || !review || !observed) {
							return failed(
								review ? core::epoch_failure::unavailable
									   : review.error());
						}
						const auto prepared = prepared_operation{
							.Kind      = prepared_kind::observation_only,
							.Operation = *operation,
						};
						if (!context->store(prepared)) {
							return failed(core::epoch_failure::unavailable);
						}
						const auto event = core::epoch_revision_prepared_event{
							.Context     = aRequest.Context,
							.Operation   = *operation,
							.Observation = {
								.LatestLive                  = snapshot->LatestLive,
								.CatalogCurrentViewerReview  = snapshot->CurrentViewerReview,
								.ProposedCurrentViewerReview = *review,
								.ObservedAt                  = *observed,
								.HeadLineage                 = core::head_lineage::not_applicable,
								.RetentionRehydration        = snapshot->RetentionRehydration,
							},
						};
						return [context, event](implementation& aHost) {
							const auto output        = aHost.dispatch_core(event);
							const auto liveUnchanged = std::ranges::any_of(
								output.Requests,
								[](const auto& aNext) {
									return std::holds_alternative<core::resolve_comparison_request>(aNext);
								});
							const auto accepted = std::ranges::any_of(
								output.Requests,
								[](const auto& aNext) {
									return std::holds_alternative<
											   core::commit_epoch_catalog_request>(
											   aNext)
										|| std::holds_alternative<
											   core::discard_epoch_preparation_request>(
											   aNext);
								});
							if (!accepted) {
								static_cast<void>(
									aHost.discard_context_preparation(
										context, event.Operation));
							} else if (liveUnchanged && aHost.Active == context
									   && aHost.State.Catalog == context->Catalog->binding().Catalog) {
								aHost.start_historical_import(context, event.Context.Request,
															  event.Context.Generation);
							}
						};
					}

					if (!snapshot->NextOrdinal) {
						return failed(core::epoch_failure::ordinal_exhausted);
					}

					auto prepared = prepared_operation{
						.Kind = prepared_kind::append,
					};
					for (std::size_t attempt = 0U;
						 attempt < maximum_identity_attempts;
						 ++attempt) {
						const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
						if (!operation) {
							return failed(core::epoch_failure::unavailable);
						}
						const auto reservation = append_reservation_request{
							.Context = {
								.ApplicationSession = aRequest.Context.ApplicationSession,
								.Request            = aRequest.Context.Request,
								.Generation         = aRequest.Context.Generation,
								.Operation          = *operation,
								.Revision           = aRequest.Context.Revision,
								.Catalog            = expectedCatalog,
							},
							.ProposedOrdinal = *snapshot->NextOrdinal,
						};
						const auto reserved = diagnostics.perform(
							diagnostic_epoch_stage::catalog_prepare,
							[&] { return context->Catalog->reserve_append(
									  reservation, aStop); });
						if (reserved) {
							if (!exact_append_reservation(
									*reserved,
									reservation,
									context->Authority.Scope)) {
								return failed(
									core::epoch_failure::repair_required);
							}
							prepared.Operation = *operation;
							prepared.Record    = *reserved;
							break;
						}
						if (reserved.error()
							== epoch_catalog_failure::operation_identity_conflict) {
							continue;
						}
						if (reserved.error()
							== epoch_catalog_failure::source_observation_conflict) {
							std::optional<core::epoch_source_observation_id>
								replacement;
							for (std::size_t sourceAttempt = 0U;
								 sourceAttempt < maximum_identity_attempts;
								 ++sourceAttempt) {
								replacement = mint_identity<
									core::epoch_source_observation_id>(*entropy);
								if (!replacement) {
									break;
								}
								const auto exists = diagnostics.perform(
									diagnostic_epoch_stage::catalog_prepare,
									[&] { return context->Catalog->source_observation_exists(*replacement, aStop); });
								if (!exists) {
									return failed(catalog_failure(exists.error()));
								}
								if (!*exists) {
									break;
								}
								replacement.reset();
							}
							if (!replacement) {
								return failed(core::epoch_failure::unavailable);
							}
							auto nextRevision               = context->Revision;
							nextRevision.Source.Observation = *replacement;
							return [context,
									prior = aRequest.Context.Revision,
									nextRevision](implementation& aHost) {
								if (aHost.Active != context
									|| aHost.State.AcceptedRevision
										!= std::optional{ prior }) {
									return;
								}
								context->Revision = nextRevision;
								aHost.dispatch_core(
									core::pull_request_revision_accepted_event{
										.ApplicationSession = context->Handoff.ApplicationSession,
										.Authority          = context->Authority,
										.Revision           = nextRevision,
										.Basis              = context->Handoff.BaseBasis,
									});
								if (aHost.State.AcceptedRevision
									!= std::optional{ nextRevision }) {
									context->Revision = prior;
								}
							};
						}
						return reserved.error()
								== epoch_catalog_failure::cancelled
							? cancelled()
							: failed(catalog_failure(reserved.error()));
					}
					if (!prepared.Record) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					const auto failPrepared =
						[&](const core::epoch_failure aFailure)
						-> worker_completion {
						const auto safe = abort_preparation(*context, prepared, *caches);
						return failed(
							safe ? aFailure : core::epoch_failure::repair_required);
					};
					const auto cancelPrepared = [&]() -> worker_completion {
						const auto safe = abort_preparation(*context, prepared, *caches);
						return safe
							? cancelled()
							: failed(core::epoch_failure::repair_required);
					};

					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.ObjectFormat       = context->Handoff.ObjectFormat,
					};
					const auto cache = diagnostics.perform(
						diagnostic_epoch_stage::cache_open,
						[&] { return open_actor_cache(
								  *context->Actor, *caches, expectedCache, aStop); });
					if (!cache) {
						return cache.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(cache.error()));
					}
					if (!context->Handoff.SmartGit
						|| context->Handoff.SmartGit->revoked()) {
						return failPrepared(core::epoch_failure::unavailable);
					}
					bool                                 receivingWitnessed      = false;
					bool                                 invalidReceivingWitness = false;
					std::optional<epoch_catalog_failure> receivingFailure;
					const auto                           maximumPackBytes = context->Actor->Cache->request().Limits.OperationPackBytes;
					const auto                           persistReceiving = [&](const git_spool_ownership& aOwnership) {
						if (receivingWitnessed || receivingFailure || invalidReceivingWitness || !valid(aOwnership)
							|| aOwnership.Operation != prepared.Operation || aOwnership.MaximumBytes != maximumPackBytes) {
							invalidReceivingWitness = true;
							return false;
						}
						const auto request = record_receiving_request{
							.Catalog                   = expectedCatalog,
							.Operation                 = prepared.Operation,
							.ExpectedOperationRevision = prepared.Record->OperationRevision,
							.Receiving                 = aOwnership,
						};
						const auto record = diagnostics.perform(diagnostic_epoch_stage::catalog_prepare,
																[&] { return context->Catalog->record_receiving(request, aStop); });
						if (!record) {
							receivingFailure = record.error();
							return false;
						}
						auto expected              = *prepared.Record;
						expected.Receiving         = aOwnership;
						expected.OperationRevision = epoch_catalog_operation_revision{ request.ExpectedOperationRevision.value() + 1 };
						if (!valid(*record) || *record != expected) {
							invalidReceivingWitness = true;
							return false;
						}
						prepared.Record    = *record;
						receivingWitnessed = true;
						return true;
					};
					auto sink = diagnostics.perform(
						diagnostic_epoch_stage::pack_sink,
						[&] { return context->Actor->Cache->create_pack_sink(
								  prepared.Operation,
								  maximumPackBytes,
								  aStop, persistReceiving); });
					if (invalidReceivingWitness) {
						return failPrepared(core::epoch_failure::repair_required);
					}
					if (receivingFailure) {
						return *receivingFailure == epoch_catalog_failure::cancelled
							? cancelPrepared()
							: failPrepared(catalog_failure(*receivingFailure));
					}
					if (!sink) {
						return sink.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(sink.error()));
					}
					if (!*sink || !receivingWitnessed) {
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					const auto smartContext = git_smart_request_context{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.Revision           = aRequest.Context.Revision,
						.Repository         = context->Handoff.Locator,
						.BaseRef            = context->Handoff.BaseRef,
						.AcquisitionMode    = object_loading_for(context->Handoff.Shape)
									== pull_request_object_loading::selected_objects
								&& context->Handoff.SmartGit->supports_sparse_commit_trees()
							? git_smart_acquisition_mode::sparse_commit_trees
							: git_smart_acquisition_mode::complete_closure,
					};
					auto transport = diagnostics.perform(
						diagnostic_epoch_stage::smart_acquire,
						[&] { return context->Handoff.SmartGit->acquire(smartContext); });
					if (!transport) {
						(*sink)->abort();
						return aStop.stop_requested()
								|| transport.error().Failure
									== git_smart_transport_failure::cancelled
							? cancelPrepared()
							: failPrepared(smart_transport_failure(transport.error()));
					}
					if (!*transport) {
						(*sink)->abort();
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					std::vector<git_protocol_v2_have> haves;
					if (snapshot->LatestLive) {
						const auto appendHave = [&](
													const core::git_commit_oid aOid,
													const core::anchor_availability
														aAvailability) {
							if (aAvailability != core::anchor_availability::available
								|| std::ranges::any_of(
									haves,
									[&](const auto& aHave) {
										return aHave.Commit == aOid;
									})) {
								return;
							}
							const auto inspected = context->Actor->Cache->inspect_commit_object(
								aOid, aStop);
							if (
								!inspected || !application::valid(*inspected)
								|| inspected->Commit != aOid) {
								return;
							}
							haves.push_back(git_protocol_v2_have{
								.Cache          = context->Authority.Cache,
								.Commit         = aOid,
								.CatalogVisible = true,
								.ObjectVerified = true,
							});
						};
						appendHave(
							snapshot->LatestLive->Base,
							snapshot->LatestLive->BaseAvailability);
						appendHave(
							snapshot->LatestLive->Head,
							snapshot->LatestLive->HeadAvailability);
					}
					const auto acquisition = diagnostics.perform(
						diagnostic_epoch_stage::protocol_acquire,
						[&] { return acquire_git_protocol_v2_pack(
								  **transport,
								  smartContext,
								  haves,
								  **sink,
								  git_protocol_v2_fetch_limits{
									  .MaximumWireBytes     = maximum_git_smart_fetch_response_bytes,
									  .MaximumPackBytes     = context->Actor->Cache->request().Limits.OperationPackBytes,
									  .MaximumProgressBytes = maximum_git_protocol_v2_progress_bytes,
									  .MaximumPackets       = maximum_git_protocol_v2_fetch_packets,
								  },
								  aStop); });
					if (!acquisition) {
						(*transport)->revoke();
						const auto wasCancelled = aStop.stop_requested()
							|| (std::holds_alternative<git_smart_transport_error>(
									acquisition.error())
								&& std::get<git_smart_transport_error>(
									   acquisition.error())
										.Failure
									== git_smart_transport_failure::cancelled);
						const auto* transportFailure = std::get_if<git_smart_transport_error>(&acquisition.error());
						return wasCancelled
							? cancelPrepared()
							: failPrepared(transportFailure != nullptr
											   ? smart_transport_failure(*transportFailure)
											   : core::epoch_failure::unavailable);
					}
					const auto inventory = diagnostics.perform(
						diagnostic_epoch_stage::cache_inventory,
						[&] { return context->Actor->Cache->observe_preimport(
								  acquisition->Fetch.Pack, aStop); });
					if (!inventory) {
						return inventory.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(inventory.error()));
					}
					auto spoolFact          = durable_fact(acquisition->Fetch.Pack);
					spoolFact.Residency     = smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
						? git_revision_residency::sparse_commit_trees
						: git_revision_residency::complete_closure;
					const auto spoolRequest = record_spool_request{
						.Catalog                   = expectedCatalog,
						.Operation                 = prepared.Operation,
						.ExpectedOperationRevision = prepared.Record->OperationRevision,
						.Spool                     = spoolFact,
						.Preimport                 = durable_fact(*inventory),
					};
					auto record = diagnostics.perform(
						diagnostic_epoch_stage::catalog_prepare,
						[&] { return context->Catalog->record_spool(
								  spoolRequest, aStop); });
					if (!record) {
						return record.error() == epoch_catalog_failure::cancelled
							? cancelPrepared()
							: failPrepared(catalog_failure(record.error()));
					}
					if (!exact_spool_record(
							*prepared.Record, *record, spoolRequest)) {
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					prepared.Record = *record;

					const auto durableRecordToken = entropy->generate();
					if (!durableRecordToken
						|| !application::valid(*durableRecordToken)) {
						return failPrepared(core::epoch_failure::unavailable);
					}
					std::optional<core::git_commit_oid> priorHead;
					if (snapshot->LatestLive) {
						priorHead = snapshot->LatestLive->Head;
					}
					const auto gitRequest = git_prepare_revision_request{
						.Receipt       = acquisition->Fetch.Pack,
						.Inventory     = *inventory,
						.Authorization = {
							.CatalogIncarnation      = expectedCatalog.Incarnation,
							.ExpectedCatalogRevision = expectedCatalog.Revision,
							.Operation               = prepared.Operation,
							.ExpectedPack            = acquisition->Fetch.Pack.ExpectedPack,
							.ReceiptToken            = acquisition->Fetch.Pack.OpaqueToken,
							.InventoryToken          = inventory->OpaqueToken,
							.DurableRecordToken      = *durableRecordToken,
						},
						.Base      = aRequest.Context.Revision.Base,
						.Head      = aRequest.Context.Revision.Head,
						.PriorHead = priorHead,
					};
					const auto normalizeLineage = [&](git_prepared_revision aPrepared) {
						if (snapshot->LatestLive
							&& snapshot->LatestLive->Head != aRequest.Context.Revision.Head
							&& snapshot->LatestLive->HeadAvailability != core::anchor_availability::available) {
							aPrepared.HeadLineage = core::head_lineage::unknown;
						}
						return aPrepared;
					};
					bool                               witnessed        = false;
					bool                               witnessCancelled = false;
					std::optional<core::epoch_failure> witnessFailure;
					const auto                         persistInstallation = [&](const git_prepared_revision& aInstallation) {
						if (witnessed || witnessFailure || witnessCancelled
							|| !exact_prepared_revision(
								aInstallation, gitRequest,
								smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
									? git_revision_residency::sparse_commit_trees
									: git_revision_residency::complete_closure)) {
							witnessFailure = core::epoch_failure::repair_required;
							return false;
						}
						const auto installRequest = record_install_request{
							.Catalog                   = expectedCatalog,
							.Operation                 = prepared.Operation,
							.ExpectedOperationRevision = prepared.Record->OperationRevision,
							.Install                   = durable_fact(normalizeLineage(aInstallation)),
						};
						const auto installed = diagnostics.perform(diagnostic_epoch_stage::catalog_prepare,
																   [&] { return context->Catalog->record_install(installRequest, aStop); });
						if (!installed) {
							witnessCancelled = installed.error() == epoch_catalog_failure::cancelled;
							witnessFailure   = catalog_failure(installed.error());
							return false;
						}
						if (!exact_install_record(*prepared.Record, *installed, installRequest)) {
							witnessFailure = core::epoch_failure::repair_required;
							return false;
						}
						prepared.Record = *installed;
						witnessed       = true;
						return true;
					};
					auto git = diagnostics.perform(
						diagnostic_epoch_stage::revision_prepare,
						[&] {
							return smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
								? context->Actor->Cache->prepare_sparse_revision(gitRequest, aStop, persistInstallation)
								: context->Actor->Cache->prepare_revision(gitRequest, aStop, persistInstallation);
						});
					if (witnessFailure) {
						return witnessCancelled ? cancelPrepared() : failPrepared(*witnessFailure);
					}
					if (!git) {
						return git.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(git.error()));
					}
					if (!witnessed || !exact_prepared_revision(*git, gitRequest, smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees ? git_revision_residency::sparse_commit_trees : git_revision_residency::complete_closure)) {
						return failPrepared(core::epoch_failure::repair_required);
					}
					*git = normalizeLineage(*git);
					if (prepared.Record->Install != durable_fact(*git)) {
						return failPrepared(core::epoch_failure::repair_required);
					}
					prepared.Git       = *git;
					const auto staging = diagnostics.perform(
						diagnostic_epoch_stage::catalog_prepare,
						[&] { return advance_append(
								  *context,
								  *prepared.Record,
								  expectedCatalog,
								  epoch_catalog_operation_phase::staging_ready,
								  aStop); });
					if (!staging) {
						return staging.error()
								== epoch_catalog_failure::cancelled
							? cancelPrepared()
							: failPrepared(catalog_failure(staging.error()));
					}
					prepared.Record = *staging;

					const auto review   = canonicalReview(projected_append_head{
						.Ordinal = prepared.Record->ReservedOrdinal,
						.Head    = aRequest.Context.Revision.Head,
					});
					const auto observed = observedTime();
					if (!review || !observed) {
						return failPrepared(
							review ? core::epoch_failure::unavailable
								   : review.error());
					}
					if (!context->store(prepared)) {
						const auto safe = abort_preparation(*context, prepared, *caches);
						return failed(
							safe ? core::epoch_failure::unavailable
								 : core::epoch_failure::repair_required);
					}
					const auto event = core::epoch_revision_prepared_event{
						.Context     = aRequest.Context,
						.Operation   = prepared.Operation,
						.Observation = {
							.LatestLive                  = snapshot->LatestLive,
							.CatalogCurrentViewerReview  = snapshot->CurrentViewerReview,
							.ProposedCurrentViewerReview = *review,
							.ProposedOrdinal             = prepared.Record->ReservedOrdinal,
							.ObservedAt                  = *observed,
							.HeadLineage                 = git->HeadLineage,
							.Material                    = core::epoch_material_identity{
								.BaseTree       = git->BaseTree,
								.HeadTree       = git->HeadTree,
								.PolicyRevision = core::comparison_policy_revision{ 1U },
							},
							.RetentionRehydration = snapshot->RetentionRehydration,
						},
					};
					return [context, event](implementation& aHost) {
						const auto output   = aHost.dispatch_core(event);
						const auto accepted = std::ranges::any_of(
							output.Requests,
							[](const auto& aNext) {
								return std::holds_alternative<
										   core::commit_epoch_catalog_request>(aNext)
									|| std::holds_alternative<
										   core::discard_epoch_preparation_request>(
										   aNext);
							});
						if (!accepted) {
							static_cast<void>(
								aHost.discard_context_preparation(
									context, event.Operation));
						}
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(
						core::epoch_revision_prepare_failed_event{
							.Context = aRequest.Context,
							.Failure = core::epoch_failure::unavailable,
						});
				},
				[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
					aHost.dispatch_core(
						core::epoch_revision_prepare_failed_event{
							.Context = aRequest.Context,
							.Failure = aFailure,
						});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void resume_pending_occurrence_import(const std::shared_ptr<authority_context>& aContext)
		{
			if (aContext->HistoricalImportPending || Active != aContext
				|| State.Authority != aContext->Authority
				|| State.AcceptedRevision != aContext->Revision
				|| !State.Catalog || !State.PendingOccurrence) {
				return;
			}
			const auto pending = *State.PendingOccurrence;
			start_historical_import(aContext, pending.Request, State.EpochGeneration,
									core::epoch_occurrence_import_context{
										State.ApplicationSession, pending.Request, *State.Authority,
										*State.AcceptedRevision, State.EpochGeneration,
										State.ComparisonGeneration, *State.Catalog,
										pending.Occurrence, pending.Side });
		}

		void start_historical_import(const std::shared_ptr<authority_context>&                  aContext,
									 const core::epoch_request_id                               aRequest,
									 const core::epoch_generation                               aGeneration,
									 const std::optional<core::epoch_occurrence_import_context> aSelection = std::nullopt)
		{
			// A historical import is supplementary. One actor serializes it with live
			// retention, and each successful completion may admit another event.
			// Large CLI reviews keep older comparison objects out of the initial
			// load. Unproven historical anchors remain unavailable; a separate
			// selected-epoch acquisition must prove them before comparison.
			const bool sparse = object_loading_for(aContext->Handoff.Shape)
					== pull_request_object_loading::selected_objects
				&& aContext->Handoff.SmartGit
				&& aContext->Handoff.SmartGit->supports_sparse_commit_trees();
			if (sparse && !aSelection) {
				return;
			}
			const auto failSelection = [aSelection](implementation& aHost) {
				if (aSelection) {
					aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
						.Context = *aSelection,
						.Failure = core::epoch_failure::unavailable,
					});
				}
			};
			if (aSelection && !sparse) {
				failSelection(*this);
				return;
			}
			if (aContext->HistoricalImportPending) {
				// The latest typed selection remains pending in core and is resumed
				// after this actor's current historical effect completes.
				return;
			}
			if (aContext->HistoricalImports + aContext->HistoricalSkipped.size() >= 8U
				|| !aContext->OccurrenceHistory || !aContext->Handoff.SmartGit
				|| aContext->Handoff.SmartGit->revoked()) {
				failSelection(*this);
				return;
			}
			aContext->HistoricalImportPending = true;
			auto* const entropy               = &Entropy;
			auto* const caches                = &Caches;
			auto* const clock                 = &Clock;
			const auto  submitted             = submit_effect(
				[context = aContext, aRequest, aGeneration, aSelection, sparse, entropy, caches, clock](
					const std::stop_token aStop) -> worker_completion {
					const auto noImport = [context, aSelection](implementation& aHost) {
						context->HistoricalImportPending = false;
						if (aSelection) {
							aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
								.Context = *aSelection,
								.Failure = core::epoch_failure::unavailable,
							});
						}
						aHost.resume_pending_occurrence_import(context);
					};
					if (aStop.stop_requested()) {
						return noImport;
					}
					const auto snapshot = context->Catalog->snapshot(aStop);
					const auto history  = context->Catalog->occurrences(aStop);
					if (!snapshot || !history || !snapshot->LatestLive
						|| snapshot->Binding != context->Catalog->binding()
						|| (aSelection && snapshot->Binding.Catalog != aSelection->ExpectedCatalog)) {
						return noImport;
					}
					const auto event = std::ranges::find_if(history->Occurrences,
															[&](const core::epoch_occurrence& aOccurrence) {
																return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event
																	&& !aOccurrence.LocalEpoch
																	&& (aSelection ? aOccurrence.Id == aSelection->Occurrence
																				   : std::ranges::find(context->HistoricalSkipped, aOccurrence.Id) == context->HistoricalSkipped.end());
															});
					if (event == history->Occurrences.end() || !snapshot->NextOrdinal
						|| !context->Handoff.SmartGit || context->Handoff.SmartGit->revoked()) {
						return noImport;
					}
					const auto skip = [context, aRequest, aGeneration, aSelection, id = event->Id](implementation& aHost) {
						context->HistoricalImportPending = false;
						if (aSelection) {
							aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
								.Context = *aSelection,
								.Failure = core::epoch_failure::unavailable,
							});
							aHost.resume_pending_occurrence_import(context);
							return;
						}
						context->HistoricalSkipped.push_back(id);
						if (aHost.Active == context && aHost.State.Authority == context->Authority) {
							aHost.start_historical_import(context, aRequest, aGeneration);
						}
						aHost.resume_pending_occurrence_import(context);
					};
					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = context->Handoff.ApplicationSession,
						.Authority          = context->Authority,
						.ObjectFormat       = context->Handoff.ObjectFormat,
					};
					if (!open_actor_cache(*context->Actor, *caches, expectedCache, aStop)) {
						return noImport;
					}
					std::optional<core::epoch_source_observation_id> source;
					for (std::size_t attempt = 0U; attempt < maximum_identity_attempts; ++attempt) {
						source = mint_identity<core::epoch_source_observation_id>(*entropy);
						if (!source) {
							return noImport;
						}
						const auto exists = context->Catalog->source_observation_exists(*source, aStop);
						if (!exists) {
							return noImport;
						}
						if (!*exists) {
							break;
						}
						source.reset();
					}
					const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
					if (!source || !operation) {
						return noImport;
					}
					auto historical               = context->Revision;
					historical.Source.Observation = *source;
					historical.Origin             = core::pull_request_revision_origin::explicit_historical_import;
					historical.Head               = event->Evidence.Head;
					historical.CurrentViewerReview.reset();
					const auto reservation = append_reservation_request{
						.Context = {
							.ApplicationSession = context->Handoff.ApplicationSession,
							.Request            = aRequest,
							.Generation         = aGeneration,
							.Operation          = *operation,
							.Revision           = historical,
							.Catalog            = snapshot->Binding.Catalog,
						},
						.ProposedOrdinal = *snapshot->NextOrdinal,
					};
					auto record = context->Catalog->reserve_append(reservation, aStop);
					if (!record || !exact_append_reservation(*record, reservation, context->Authority.Scope)) {
						return noImport;
					}
					auto prepared = prepared_operation{
						.Kind      = prepared_kind::append,
						.Operation = *operation,
						.Record    = *record,
					};
					const auto fail = [&]() -> worker_completion {
						const auto aborted = abort_preparation(*context, prepared, *caches);
						if (aborted && !aStop.stop_requested()) {
							return skip;
						}
						return noImport;
					};
					const auto maximumPackBytes   = context->Actor->Cache->request().Limits.OperationPackBytes;
					bool       receivingWitnessed = false;
					auto       sink               = context->Actor->Cache->create_pack_sink(
						*operation, maximumPackBytes, aStop,
						[&](const git_spool_ownership& aOwnership) {
							if (receivingWitnessed || !valid(aOwnership) || aOwnership.Operation != *operation
								|| aOwnership.MaximumBytes != maximumPackBytes) {
								return false;
							}
							const auto next = context->Catalog->record_receiving({
																					 .Catalog                   = snapshot->Binding.Catalog,
																					 .Operation                 = *operation,
																					 .ExpectedOperationRevision = prepared.Record->OperationRevision,
																					 .Receiving                 = aOwnership,
																				 },
																				 aStop);
							if (!next) {
								return false;
							}
							prepared.Record    = *next;
							receivingWitnessed = true;
							return true;
						});
					if (!sink || !*sink || !receivingWitnessed) {
						return fail();
					}
					const auto smartContext = git_smart_request_context{
						.ApplicationSession = context->Handoff.ApplicationSession,
						.Authority          = context->Authority,
						.Revision           = context->Revision,
						.Repository         = context->Handoff.Locator,
						.BaseRef            = context->Handoff.BaseRef,
						.AcquisitionMode    = sparse ? git_smart_acquisition_mode::sparse_commit_trees
													 : git_smart_acquisition_mode::complete_closure,
					};
					auto transport = context->Handoff.SmartGit->acquire(smartContext);
					if (!transport || !*transport) {
						(*sink)->abort();
						return fail();
					}
					const auto acquisition = acquire_git_protocol_v2_historical_pack(
						**transport, smartContext, event->Evidence.Head, { }, **sink,
						git_protocol_v2_fetch_limits{
							.MaximumWireBytes     = maximum_git_smart_fetch_response_bytes,
							.MaximumPackBytes     = maximumPackBytes,
							.MaximumProgressBytes = maximum_git_protocol_v2_progress_bytes,
							.MaximumPackets       = maximum_git_protocol_v2_fetch_packets,
						},
						aStop);
					if (!acquisition) {
						(*transport)->revoke();
						return fail();
					}
					const auto inventory = context->Actor->Cache->observe_preimport(acquisition->Fetch.Pack, aStop);
					if (!inventory) {
						return fail();
					}
					auto spoolFact      = durable_fact(acquisition->Fetch.Pack);
					spoolFact.Residency = sparse ? git_revision_residency::sparse_commit_trees
												 : git_revision_residency::complete_closure;
					const auto spooled  = context->Catalog->record_spool({
																			 .Catalog                   = snapshot->Binding.Catalog,
																			 .Operation                 = *operation,
																			 .ExpectedOperationRevision = prepared.Record->OperationRevision,
																			 .Spool                     = spoolFact,
																			 .Preimport                 = durable_fact(*inventory),
																		 },
																		 aStop);
					if (!spooled) {
						return fail();
					}
					prepared.Record  = *spooled;
					const auto token = entropy->generate();
					if (!token || !application::valid(*token)) {
						return fail();
					}
					const auto authorization = git_import_authorization{
						.CatalogIncarnation      = snapshot->Binding.Catalog.Incarnation,
						.ExpectedCatalogRevision = snapshot->Binding.Catalog.Revision,
						.Operation               = *operation,
						.ExpectedPack            = acquisition->Fetch.Pack.ExpectedPack,
						.ReceiptToken            = acquisition->Fetch.Pack.OpaqueToken,
						.InventoryToken          = inventory->OpaqueToken,
						.DurableRecordToken      = *token,
					};
					const auto sparseRequest = git_prepare_revision_request{
						.Receipt       = acquisition->Fetch.Pack,
						.Inventory     = *inventory,
						.Authorization = authorization,
						.Base          = context->Revision.Base,
						.Head          = event->Evidence.Head,
					};
					bool       installationWitnessed = false;
					const auto persistInstallation   = [&](const git_prepared_revision& aInstallation) {
						if (installationWitnessed || !application::valid(aInstallation)
							|| aInstallation.Head != event->Evidence.Head
							|| (sparse && !exact_prepared_revision(aInstallation, sparseRequest, git_revision_residency::sparse_commit_trees))
							|| aInstallation.Residency != (sparse ? git_revision_residency::sparse_commit_trees : git_revision_residency::complete_closure)) {
							return false;
						}
						const auto next = context->Catalog->record_install({
																			   .Catalog                   = snapshot->Binding.Catalog,
																			   .Operation                 = *operation,
																			   .ExpectedOperationRevision = prepared.Record->OperationRevision,
																			   .Install                   = durable_fact(aInstallation),
																			   .DerivedComparisonBase     = aInstallation.Base,
																		   },
																		   aStop);
						if (!next) {
							return false;
						}
						prepared.Record       = *next;
						installationWitnessed = true;
						return true;
					};
					auto installed = sparse
						? context->Actor->Cache->prepare_sparse_revision(sparseRequest, aStop, persistInstallation)
						: context->Actor->Cache->prepare_historical_revision({
																				 .Receipt       = acquisition->Fetch.Pack,
																				 .Inventory     = *inventory,
																				 .Authorization = authorization,
																				 .SelectedBase  = context->Revision.Base,
																				 .Head          = event->Evidence.Head,
																			 },
																			 aStop, persistInstallation);
					if (!installed || !installationWitnessed
						|| (sparse && !exact_prepared_revision(*installed, sparseRequest, git_revision_residency::sparse_commit_trees))
						|| prepared.Record->Context.Revision.Base != installed->Base) {
						return fail();
					}
					prepared.Git       = *installed;
					const auto staging = advance_append(*context, *prepared.Record, snapshot->Binding.Catalog,
														epoch_catalog_operation_phase::staging_ready, aStop);
					if (!staging) {
						return fail();
					}
					prepared.Record     = *staging;
					const auto observed = clock->observe_utc();
					if (!observed || !core::valid(*observed)) {
						return fail();
					}
					const auto plan = core::plan_historical_epoch_mutation(
						context->Authority, context->Revision, historical.Source,
						snapshot->Binding.Catalog, *event, installed->Base,
						core::epoch_preparation_observation{
							.LatestLive      = snapshot->LatestLive,
							.ProposedOrdinal = prepared.Record->ReservedOrdinal,
							.ObservedAt      = *observed,
							.HeadLineage     = core::head_lineage::not_applicable,
							.Material        = core::epoch_material_identity{
								.BaseTree       = installed->BaseTree,
								.HeadTree       = installed->HeadTree,
								.PolicyRevision = core::comparison_policy_revision{ 1U },
							},
							.RetentionRehydration = snapshot->RetentionRehydration,
						});
					const auto* append = plan.Mutation
						? std::get_if<core::append_epoch_mutation>(&*plan.Mutation)
						: nullptr;
					if (!append) {
						return fail();
					}
					const auto candidate = context->Catalog->record_candidate({
																				  .Catalog                   = snapshot->Binding.Catalog,
																				  .Operation                 = *operation,
																				  .ExpectedOperationRevision = prepared.Record->OperationRevision,
																				  .Candidate                 = *append,
																				  .HistoricalOccurrence      = event->Id,
																			  },
																			  aStop);
					if (!candidate || candidate->HistoricalOccurrence != event->Id
						|| candidate->Candidate != std::optional{ *append }) {
						return fail();
					}
					prepared.Record      = *candidate;
					const auto committed = recover_append(*context->Catalog, *context->Actor->Cache,
														  *entropy, *prepared.Record, aStop);
					if (!committed) {
						return noImport;
					}
					// The append may have crossed its durable point of no return even
					// when the caller has since cancelled. Read its committed identity
					// before delivering any selection or allowing a newer selection
					// to reuse this actor.
					const auto postCommitStop = std::stop_token{ };
					const auto current        = context->Catalog->snapshot(postCommitStop);
					const auto updated        = context->Catalog->occurrences(postCommitStop);
					if (!current || !updated || !current->LatestLive
						|| current->LatestLive != snapshot->LatestLive) {
						return noImport;
					}
					if (aSelection) {
						const auto linked = std::ranges::find(updated->Occurrences,
															  aSelection->Occurrence, &core::epoch_occurrence::Id);
						if (linked == updated->Occurrences.end()
							|| linked->LocalEpoch != append->Epoch.Id
							|| linked->Evidence.Head != append->Epoch.Head
							|| linked->DerivedComparisonBase != append->Epoch.Base
							|| current->Binding.Catalog == snapshot->Binding.Catalog) {
							return noImport;
						}
					}
					return [context, aRequest, aGeneration, expectedCatalog = snapshot->Binding.Catalog, catalog = current->Binding.Catalog,
							currentEpoch = current->LatestLive, completedHistory = *updated,
							aSelection, imported                                 = append->Epoch](implementation& aHost) {
						context->HistoricalImportPending = false;
						++context->HistoricalImports;
						if (aHost.Active == context && aHost.State.Authority == context->Authority) {
							const auto importedHead = core::comparison_anchor{
								.Identity = core::comparison_anchor_identity{
									.Authority   = imported.Id.Authority,
									.Incarnation = imported.Id.Incarnation,
									.Kind        = core::comparison_anchor_kind::epoch_head,
									.Epoch       = imported.Id.Ordinal,
								},
								.Commit       = imported.Head,
								.Availability = imported.HeadAvailability,
							};
							if (aSelection) {
								aHost.dispatch_core(core::epoch_occurrence_imported_event{
									.Context    = *aSelection,
									.NewCatalog = catalog,
									.Head       = importedHead,
								});
								// A repeated gesture can supersede the request after its
								// append is durable. The same verified link satisfies that
								// latest exact occurrence without fetching it again.
								if (aHost.State.Catalog == expectedCatalog
									&& aHost.State.PendingOccurrence
									&& aHost.State.PendingOccurrence->Occurrence == aSelection->Occurrence) {
									const auto pending = *aHost.State.PendingOccurrence;
									aHost.dispatch_core(core::epoch_occurrence_imported_event{
										.Context = core::epoch_occurrence_import_context{
											aHost.State.ApplicationSession, pending.Request,
											*aHost.State.Authority, *aHost.State.AcceptedRevision,
											aHost.State.EpochGeneration, aHost.State.ComparisonGeneration,
											expectedCatalog, pending.Occurrence, pending.Side },
										.NewCatalog = catalog,
										.Head       = importedHead,
									});
								}
							}
							if (aHost.State.Catalog == expectedCatalog) {
								aHost.dispatch_core(core::epoch_historical_catalog_advanced_event{
									.ApplicationSession = context->Handoff.ApplicationSession,
									.Authority          = context->Authority,
									.Revision           = context->Revision,
									.ExpectedCatalog    = expectedCatalog,
									.NewCatalog         = catalog,
								});
							}
							if (aHost.State.Catalog == catalog) {
								context->OccurrenceHistory = completedHistory;
								aHost.publish_occurrences(catalog, completedHistory, currentEpoch);
							}
							if (!aSelection) {
								aHost.start_historical_import(context, aRequest, aGeneration);
							}
							aHost.resume_pending_occurrence_import(context);
						}
					};
				},
				[context = aContext, aSelection](implementation& aHost) {
					context->HistoricalImportPending = false;
					if (aSelection) {
						aHost.dispatch_core(core::epoch_occurrence_import_cancelled_event{
							.Context = *aSelection,
						});
					}
					aHost.resume_pending_occurrence_import(context);
				},
				[context = aContext, aSelection](implementation& aHost, core::epoch_failure aFailure) {
					context->HistoricalImportPending = false;
					if (aSelection) {
						aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
							.Context = *aSelection,
							.Failure = aFailure,
						});
					}
					aHost.resume_pending_occurrence_import(context);
				},
				aRequest, aGeneration, std::nullopt, std::nullopt, aContext->Actor);
			if (!submitted) {
				aContext->HistoricalImportPending = false;
				failSelection(*this);
			}
		}

		void start_catalog_commit(const core::commit_epoch_catalog_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			auto* const entropy = &Entropy;
			auto* const caches  = &Caches;
			auto        actor   = context->Actor;
			submit_effect(
				[context, aRequest, entropy, caches](const std::stop_token aStop)
					-> worker_completion {
					const auto cancelled = [aRequest](const core::epoch_commit_progress
														  aProgress)
						-> worker_completion {
						return [aRequest, aProgress](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_catalog_commit_cancelled_event{
									.Context  = aRequest.Context,
									.Mutation = aRequest.Mutation,
									.Progress = aProgress,
								});
						};
					};
					const auto failed = [aRequest](const core::epoch_failure aFailure)
						-> worker_completion {
						return [aRequest, aFailure](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_catalog_commit_failed_event{
									.Context  = aRequest.Context,
									.Mutation = aRequest.Mutation,
									.Failure  = aFailure,
								});
						};
					};
					const auto succeeded = [aRequest, context, aStop](
											   const core::epoch_catalog_revision aRevision) -> worker_completion {
						auto history = std::optional<core::epoch_occurrence_history>{ };
						auto current = std::optional<core::review_epoch_summary>{ };
						try {
							if (auto stored = context->Catalog->occurrences(aStop)) {
								history = *stored;
								if (const auto snapshot = context->Catalog->snapshot(aStop);
									snapshot && snapshot->Binding == context->Catalog->binding()) {
									current = snapshot->LatestLive;
								}
								auto reconciled = std::move(*stored);
								reconciled      = reconcile_local_occurrences(context->Handoff, std::move(reconciled), *context->Catalog, current, aStop);
								enrich_commit_dates(reconciled, context->Actor->Cache.get(), aStop);
								std::ranges::sort(reconciled.Occurrences, core::epoch_occurrence_less);
								if (context->Catalog->record_occurrences(reconciled, aStop)) {
									history = std::move(reconciled);
								}
							}
						} catch (...) {
							// The durable live commit succeeded. Supplementary evidence
							// must never replace its terminal with a failure or suppress
							// publication of the loaded, durably unlinked history.
						}
						const auto catalog = context->Catalog->binding().Catalog;
						return [aRequest, aRevision, context, catalog,
								completedHistory = std::move(history), completedCurrent = std::move(current)](
								   implementation& aHost) {
							aHost.dispatch_core(core::epoch_catalog_committed_event{
								.Context     = aRequest.Context,
								.Mutation    = aRequest.Mutation,
								.NewRevision = aRevision,
							});
							if (aHost.Active == context
								&& aHost.State.Authority == context->Authority
								&& aHost.State.Catalog == catalog && (completedHistory || context->OccurrenceHistory)) {
								context->OccurrenceHistory = completedHistory.value_or(core::epoch_occurrence_history{ });
								aHost.publish_occurrences(catalog, *context->OccurrenceHistory, completedCurrent);
								aHost.start_historical_import(context, aRequest.Context.Request,
															  aRequest.Context.Generation);
							}
						};
					};

					auto claimed = context->take(aRequest.Context.Operation);
					if (!claimed) {
						return failed(core::epoch_failure::catalog_conflict);
					}
					auto       operation       = std::move(*claimed);
					const auto expectedCatalog = core::epoch_catalog_identity{
						.Incarnation = aRequest.Context.CatalogIncarnation,
						.Revision    = aRequest.Context.ExpectedCatalogRevision,
					};
					const auto abortAndFail =
						[&](const core::epoch_failure aFailure)
						-> worker_completion {
						const auto safe = abort_preparation(*context, operation, *caches);
						return failed(
							safe ? aFailure : core::epoch_failure::repair_required);
					};
					const auto abortAndCancel = [&]() -> worker_completion {
						const auto safe = abort_preparation(*context, operation, *caches);
						return safe
							? cancelled(
								  core::epoch_commit_progress::before_ref_commit)
							: failed(core::epoch_failure::repair_required);
					};

					if (const auto* association = std::get_if<core::associate_review_mutation>(
							&aRequest.Mutation)) {
						if (operation.Kind != prepared_kind::observation_only) {
							return abortAndFail(
								core::epoch_failure::malformed_observation);
						}
						if (aStop.stop_requested()) {
							return cancelled(
								core::epoch_commit_progress::before_ref_commit);
						}
						const auto visibleAssociation = [&]()
							-> std::optional<core::epoch_catalog_revision> {
							try {
								const auto snapshot = context->Catalog->snapshot({ });
								if (
									!snapshot
									|| !application::valid(snapshot->Binding)
									|| snapshot->Binding
										!= context->Catalog->binding()
									|| snapshot->Binding.Catalog.Incarnation
										!= expectedCatalog.Incarnation
									|| expectedCatalog.Revision.value()
										>= core::maximum_epoch_catalog_revision
									|| snapshot->Binding.Catalog.Revision.value()
										!= expectedCatalog.Revision.value() + 1
									|| snapshot->CurrentViewerReview
										!= std::optional{ association->Review }) {
									return std::nullopt;
								}
								return snapshot->Binding.Catalog.Revision;
							} catch (...) {
								return std::nullopt;
							}
						};
						epoch_catalog_result<catalog_commit_result> committed = std::unexpected{ epoch_catalog_failure::unavailable };
						try {
							committed = context->Catalog->associate_review(
								associate_review_request{
									.Catalog          = expectedCatalog,
									.Review           = association->Review,
									.AcceptedSnapshot = aRequest.Context.Revision.Source.Snapshot,
								},
								aStop);
						} catch (...) {
							const auto visible = visibleAssociation();
							return visible ? succeeded(*visible)
										   : failed(
												 core::epoch_failure::
													 repair_required);
						}
						if (!committed) {
							if (const auto visible = visibleAssociation()) {
								return succeeded(*visible);
							}
							return committed.error()
									== epoch_catalog_failure::cancelled
								? cancelled(core::epoch_commit_progress::
												before_ref_commit)
								: failed(catalog_failure(committed.error()));
						}
						if (
							!exact_catalog_commit(
								*committed, expectedCatalog, std::nullopt)
							|| context->Catalog->binding().Catalog
								!= committed->Catalog) {
							const auto visible = visibleAssociation();
							return visible ? succeeded(*visible)
										   : failed(
												 core::epoch_failure::
													 catalog_conflict);
						}
						return succeeded(committed->Catalog.Revision);
					}

					const auto* append = std::get_if<core::append_epoch_mutation>(&aRequest.Mutation);
					if (append == nullptr || operation.Kind != prepared_kind::append
						|| !operation.Record || !operation.Git
						|| operation.Record->Context.Catalog != expectedCatalog
						|| operation.Record->ReservedOrdinal
							!= append->Epoch.Id.Ordinal
						|| operation.Git->Base != append->Epoch.Base
						|| operation.Git->Head != append->Epoch.Head) {
						return abortAndFail(
							core::epoch_failure::malformed_observation);
					}
					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.ObjectFormat       = context->Handoff.ObjectFormat,
					};
					const auto cache = open_actor_cache(
						*context->Actor, *caches, expectedCache, aStop);
					if (!cache) {
						return cache.error() == git_cache_failure::cancelled
							? abortAndCancel()
							: abortAndFail(cache_failure(cache.error()));
					}
					if (aStop.stop_requested()) {
						return abortAndCancel();
					}
					const auto durableCommitToken = entropy->generate();
					if (!durableCommitToken
						|| !application::valid(*durableCommitToken)) {
						return abortAndFail(core::epoch_failure::unavailable);
					}

					const auto visibleAppend = [&]()
						-> std::optional<core::epoch_catalog_revision> {
						try {
							const auto snapshot = context->Catalog->snapshot({ });
							if (
								!snapshot
								|| !application::valid(snapshot->Binding)
								|| snapshot->Binding
									!= context->Catalog->binding()
								|| snapshot->Binding.Catalog.Incarnation
									!= expectedCatalog.Incarnation
								|| expectedCatalog.Revision.value()
									>= core::maximum_epoch_catalog_revision
								|| snapshot->Binding.Catalog.Revision.value()
									!= expectedCatalog.Revision.value() + 1
								|| snapshot->LatestLive
									!= std::optional{ append->Epoch }) {
								return std::nullopt;
							}
							return snapshot->Binding.Catalog.Revision;
						} catch (...) {
							return std::nullopt;
						}
					};
					const auto resolveAmbiguity = [&, context](
													  const core::epoch_failure aFallback,
													  const bool                aCancelledBeforePonr) -> worker_completion {
						if (const auto visible = visibleAppend()) {
							return succeeded(*visible);
						}
						try {
							const auto pending = pending_append(
								*context->Catalog, operation.Operation);
							if (!pending) {
								return failed(
									core::epoch_failure::repair_required);
							}
							if (!*pending) {
								if (const auto visible = visibleAppend()) {
									return succeeded(*visible);
								}
								return aCancelledBeforePonr
									? cancelled(core::epoch_commit_progress::
													before_ref_commit)
									: failed(aFallback);
							}
							const auto& durable = **pending;
							if (
								!same_append_identity(
									*operation.Record, durable)
								|| durable.Spool != operation.Record->Spool
								|| durable.Preimport
									!= operation.Record->Preimport
								|| durable.Install != operation.Record->Install
								|| (durable.Candidate
									&& durable.Candidate
										!= std::optional{ *append })) {
								return failed(
									core::epoch_failure::repair_required);
							}
							operation.Record = durable;
							if (!point_of_no_cancellation(durable.Phase)) {
								const auto safe = abort_preparation(*context, operation, *caches);
								if (!safe) {
									return failed(
										core::epoch_failure::repair_required);
								}
								return aCancelledBeforePonr
									? cancelled(core::epoch_commit_progress::
													before_ref_commit)
									: failed(aFallback);
							}

							const auto recovered = recover_append(
								*context->Catalog,
								**cache,
								*entropy,
								durable,
								{ });
							if (const auto visible = visibleAppend()) {
								return succeeded(*visible);
							}
							return recovered
								? failed(core::epoch_failure::repair_required)
								: failed(recovered.error());
						} catch (...) {
							if (const auto visible = visibleAppend()) {
								return succeeded(*visible);
							}
							return failed(
								core::epoch_failure::repair_required);
						}
					};

					const auto candidateRequest = record_candidate_request{
						.Catalog                   = expectedCatalog,
						.Operation                 = operation.Operation,
						.ExpectedOperationRevision = operation.Record->OperationRevision,
						.Candidate                 = *append,
					};
					epoch_catalog_result<append_operation_record> record = std::unexpected{ epoch_catalog_failure::unavailable };
					try {
						record = context->Catalog->record_candidate(
							candidateRequest, aStop);
					} catch (...) {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					}
					if (!record) {
						const auto wasCancelled = record.error() == epoch_catalog_failure::cancelled;
						return resolveAmbiguity(
							wasCancelled
								? core::epoch_failure::unavailable
								: catalog_failure(record.error()),
							wasCancelled);
					}
					if (!exact_candidate_record(
							*operation.Record, *record, candidateRequest)) {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					}
					operation.Record = *record;
					if (aStop.stop_requested()) {
						return abortAndCancel();
					}
					epoch_catalog_result<append_operation_record>
						pointOfNoReturn = std::unexpected{
							epoch_catalog_failure::unavailable
						};
					try {
						pointOfNoReturn = advance_append(
							*context,
							*operation.Record,
							expectedCatalog,
							epoch_catalog_operation_phase::
								ref_commit_in_progress,
							aStop);
					} catch (...) {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					}
					if (!pointOfNoReturn) {
						const auto wasCancelled = pointOfNoReturn.error()
							== epoch_catalog_failure::cancelled;
						return resolveAmbiguity(
							wasCancelled
								? core::epoch_failure::unavailable
								: catalog_failure(
									  pointOfNoReturn.error()),
							wasCancelled);
					}
					operation.Record = *pointOfNoReturn;

					const auto resolveRecovery = [&]() -> worker_completion {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					};

					try {
						auto retained = context->Actor->Cache->promote_retained(
							*operation.Git,
							git_ref_promotion_authorization{
								.CatalogIncarnation      = expectedCatalog.Incarnation,
								.ExpectedCatalogRevision = expectedCatalog.Revision,
								.Operation               = operation.Operation,
								.Scope                   = context->Authority.Scope,
								.Ordinal                 = append->Epoch.Id.Ordinal,
								.Base                    = append->Epoch.Base,
								.Head                    = append->Epoch.Head,
								.DurableCommitToken      = *durableCommitToken,
							},
							{ });
						if (!retained) {
							return resolveRecovery();
						}
						if (*retained != expected_retained_pair(*operation.Record)) {
							return resolveRecovery();
						}
						record = advance_append(
							*context,
							*operation.Record,
							expectedCatalog,
							epoch_catalog_operation_phase::refs_committed,
							{ });
						if (!record) {
							return resolveRecovery();
						}
						operation.Record     = *record;
						const auto inspected = operation.Git->Residency
								== git_revision_residency::sparse_commit_trees
							? context->Actor->Cache->inspect_sparse_retained(
								  append->Epoch.Id.Ordinal, { })
							: context->Actor->Cache->inspect_retained(
								  append->Epoch.Id.Ordinal, { });
						if (!inspected) {
							return resolveRecovery();
						}
						if (
							*inspected
							!= expected_retained_pair(*operation.Record)) {
							return resolveRecovery();
						}
						const auto finalized = context->Catalog->finalize_append(
							finalize_append_request{
								.Catalog                   = expectedCatalog,
								.Operation                 = operation.Operation,
								.ExpectedOperationRevision = operation.Record->OperationRevision,
								.Mutation                  = *append,
								.ObservedRefs              = *inspected,
							},
							{ });
						if (!finalized) {
							return resolveRecovery();
						}
						if (
							!exact_catalog_commit(
								*finalized,
								expectedCatalog,
								append->Epoch.Id)
							|| context->Catalog->binding().Catalog
								!= finalized->Catalog) {
							return resolveRecovery();
						}

						// Catalog visibility is the durable-success linearization
						// point. Cleanup failure remains recoverable and must not
						// replace this committed terminal with failure/cancellation.
						auto cleanupRecord              = *operation.Record;
						cleanupRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
						cleanupRecord.OperationRevision = epoch_catalog_operation_revision{
							cleanupRecord.OperationRevision.value() + 1
						};
						const auto cleanupRequest = recovery_request(cleanupRecord);
						if (!cleanupRequest) {
							mark_append_repair(*context->Catalog, cleanupRecord);
						} else {
							const auto cleaned = context->Actor->Cache->complete_recovered_operation(
								*cleanupRequest, { });
							if (cleaned) {
								static_cast<void>(advance_append(
									*context,
									cleanupRecord,
									finalized->Catalog,
									epoch_catalog_operation_phase::complete,
									{ }));
							} else if (
								cache_failure(cleaned.error())
								== core::epoch_failure::repair_required) {
								mark_append_repair(
									*context->Catalog, cleanupRecord);
							}
						}
						return succeeded(finalized->Catalog.Revision);
					} catch (...) {
						return resolveRecovery();
					}
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::epoch_catalog_commit_failed_event{
						.Context  = aRequest.Context,
						.Mutation = aRequest.Mutation,
						.Failure  = core::epoch_failure::repair_required,
					});
				},
				[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
					aHost.dispatch_core(core::epoch_catalog_commit_failed_event{
						.Context  = aRequest.Context,
						.Mutation = aRequest.Mutation,
						.Failure  = aFailure,
					});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void discard_preparation(
			const core::discard_epoch_preparation_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			static_cast<void>(discard_context_preparation(
				std::move(context), aRequest.Context.Operation));
		}

		void start_epoch_maintenance(
			const core::perform_epoch_maintenance_request& aRequest)
		{
			auto context = Active;
			if (!context || context->Authority != aRequest.Context.Authority
				|| context->Revision.Source != aRequest.Context.Source) {
				return;
			}
			auto* const caches  = &Caches;
			auto* const entropy = &Entropy;
			auto        actor   = context->Actor;
			submit_effect(
				[context, aRequest, caches, entropy](
					const std::stop_token aStop) -> worker_completion {
					const auto completed =
						[aRequest](const core::epoch_catalog_identity aCatalog)
						-> worker_completion {
						return [aRequest, aCatalog](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_maintenance_completed_event{
									.Context    = aRequest.Context,
									.NewCatalog = aCatalog,
								});
						};
					};
					const auto failed =
						[aRequest](
							const core::epoch_failure aFailure) -> worker_completion {
						return [aRequest, aFailure](implementation& aHost) {
							aHost.dispatch_core(core::epoch_maintenance_failed_event{
								.Context = aRequest.Context,
								.Failure = aFailure,
							});
						};
					};
					const auto cancelled = [aRequest]() -> worker_completion {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_maintenance_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					};
					const auto& binding = context->Catalog->binding();
					if (!application::valid(binding)
						|| binding.Authority != aRequest.Context.Authority
						|| binding.Catalog != aRequest.Context.Catalog
						|| aRequest.Context.Cache != binding.Authority.Cache
						|| aRequest.Context.ApplicationSession
							!= context->Handoff.ApplicationSession
						|| aRequest.Context.Generation.value() == 0U
						|| aRequest.Context.Request.value() == 0U
						|| !aRequest.Context.TargetDigest.valid()) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					const auto snapshot = context->Catalog->snapshot(aStop);
					if (!snapshot) {
						return snapshot.error() == epoch_catalog_failure::cancelled
							? cancelled()
							: failed(catalog_failure(snapshot.error()));
					}
					if (!application::valid(snapshot->Binding)
						|| snapshot->Binding != binding) {
						return failed(core::epoch_failure::repair_required);
					}

					std::optional<maintenance_operation_record> operation;
					for (std::size_t attempt = 0U;
						 attempt < maximum_identity_attempts;
						 ++attempt) {
						const auto identity = mint_identity<core::epoch_operation_id>(*entropy);
						if (!identity) {
							return failed(core::epoch_failure::unavailable);
						}

						if (aRequest.Context.Kind
							== core::epoch_maintenance_kind::prune_epochs) {
							if (aRequest.Context.Epochs.empty()
								|| aRequest.Context.Epochs.size()
									> maximum_prune_epoch_count) {
								return failed(
									core::epoch_failure::malformed_observation);
							}
							std::vector<core::comparison_anchor_identity>
								identities;
							identities.reserve(aRequest.Context.Epochs.size() * 2U);
							for (std::size_t index = 0U;
								 index < aRequest.Context.Epochs.size();
								 ++index) {
								const auto& epoch = aRequest.Context.Epochs[index];
								if (!core::valid(epoch)
									|| epoch.Authority != binding.Authority.Scope
									|| epoch.Incarnation
										!= binding.Catalog.Incarnation
									|| (index != 0U
										&& !(aRequest.Context.Epochs[index - 1U]
											 < epoch))) {
									return failed(
										core::epoch_failure::malformed_observation);
								}
								identities.push_back(
									core::comparison_anchor_identity{
										.Authority   = epoch.Authority,
										.Incarnation = epoch.Incarnation,
										.Kind        = core::comparison_anchor_kind::
											epoch_base,
										.Epoch = epoch.Ordinal,
									});
								identities.push_back(
									core::comparison_anchor_identity{
										.Authority   = epoch.Authority,
										.Incarnation = epoch.Incarnation,
										.Kind        = core::comparison_anchor_kind::
											epoch_head,
										.Epoch = epoch.Ordinal,
									});
							}
							std::ranges::sort(identities);
							const auto anchors = context->Catalog->exact_anchors(
								exact_anchor_lookup_request{
									.Catalog = aRequest.Context.Catalog,
									.Anchors = identities,
								},
								aStop);
							if (!anchors) {
								return anchors.error()
										== epoch_catalog_failure::cancelled
									? cancelled()
									: failed(catalog_failure(anchors.error()));
							}
							if (anchors->Catalog != aRequest.Context.Catalog
								|| anchors->Anchors.size() != identities.size()) {
								return failed(core::epoch_failure::repair_required);
							}
							std::vector<prune_epoch_fact> facts;
							facts.reserve(aRequest.Context.Epochs.size());
							for (const auto& epoch : aRequest.Context.Epochs) {
								const auto find = [&](const auto aKind)
									-> const core::comparison_anchor* {
									const auto anchorIdentity = core::comparison_anchor_identity{
										.Authority   = epoch.Authority,
										.Incarnation = epoch.Incarnation,
										.Kind        = aKind,
										.Epoch       = epoch.Ordinal,
									};
									const auto found = std::ranges::find(anchors->Anchors,
																		 anchorIdentity,
																		 &core::comparison_anchor::Identity);
									return found == anchors->Anchors.end()
										? nullptr
										: &*found;
								};
								const auto* base = find(core::comparison_anchor_kind::epoch_base);
								const auto* head = find(core::comparison_anchor_kind::epoch_head);
								if (base == nullptr || head == nullptr
									|| !core::valid(*base) || !core::valid(*head)
									|| !valid_prune_anchor_availability(
										base->Availability)
									|| !valid_prune_anchor_availability(
										head->Availability)
									|| base->Commit.algorithm()
										!= context->Handoff.ObjectFormat
									|| head->Commit.algorithm()
										!= context->Handoff.ObjectFormat) {
									return failed(
										core::epoch_failure::repair_required);
								}
								facts.push_back(prune_epoch_fact{
									.Epoch    = epoch,
									.Retained = git_retained_ref_pair{
										.Scope   = epoch.Authority,
										.Ordinal = epoch.Ordinal,
										.Base    = base->Commit,
										.Head    = head->Commit,
									},
									.BaseRef = epoch_catalog_ref_identity{
										.Authority = epoch.Authority,
										.Ordinal   = epoch.Ordinal,
									},
									.HeadRef = epoch_catalog_ref_identity{
										.Authority = epoch.Authority,
										.Ordinal   = epoch.Ordinal,
										.Head      = true,
									},
									.ExpectedBaseAvailability = base->Availability,
									.ExpectedHeadAvailability = head->Availability,
								});
							}
							const auto begin = begin_prune_request{
								.Catalog   = aRequest.Context.Catalog,
								.Operation = *identity,
								.Epochs    = std::move(facts),
							};
							auto begun = context->Catalog->begin_prune(begin, aStop);
							if (begun) {
								if (!exact_prune_intent(*begun,
														begin,
														binding.Authority.Cache)) {
									return failed(
										core::epoch_failure::repair_required);
								}
								operation = std::move(*begun);
								break;
							}
							if (begun.error() == epoch_catalog_failure::cancelled) {
								return cancelled();
							}
							if (begun.error()
									!= epoch_catalog_failure::operation_conflict
								&& begun.error()
									!= epoch_catalog_failure::
										operation_identity_conflict) {
								return failed(catalog_failure(begun.error()));
							}
						} else if (aRequest.Context.Kind
								   == core::epoch_maintenance_kind::
									   delete_repository_cache) {
							if (!aRequest.Context.Epochs.empty()) {
								return failed(
									core::epoch_failure::malformed_observation);
							}
							const auto impact = context->Catalog->cache_delete_impact(
								aRequest.Context.Cache,
								aStop);
							if (!impact) {
								return impact.error()
										== epoch_catalog_failure::cancelled
									? cancelled()
									: failed(catalog_failure(impact.error()));
							}
							const auto currentScope = std::ranges::find(impact->Scopes,
																		binding.Authority.Scope,
																		&cache_delete_scope_fact::Authority);
							if (!application::valid(*impact)
								|| impact->Incarnation
									!= binding.Catalog.Incarnation
								|| impact->Cache != binding.Authority.Cache
								|| currentScope == impact->Scopes.end()
								|| currentScope->ExpectedRevision
									!= binding.Catalog.Revision) {
								return failed(core::epoch_failure::repair_required);
							}
							const auto begin = begin_cache_delete_request{
								.Operation = *identity,
								.Impact    = *impact,
							};
							auto begun = context->Catalog->begin_cache_delete(begin, aStop);
							if (begun) {
								if (!exact_cache_delete_intent(*begun, begin)) {
									return failed(
										core::epoch_failure::repair_required);
								}
								operation = std::move(*begun);
								break;
							}
							if (begun.error() == epoch_catalog_failure::cancelled) {
								return cancelled();
							}
							if (begun.error()
									!= epoch_catalog_failure::operation_conflict
								&& begun.error()
									!= epoch_catalog_failure::
										operation_identity_conflict) {
								return failed(catalog_failure(begun.error()));
							}
						} else {
							return failed(
								core::epoch_failure::malformed_observation);
						}
					}
					if (!operation) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					if (aStop.stop_requested()) {
						const auto aborted = recover_maintenance(*context->Catalog,
																 *caches,
																 context->Handoff,
																 context->Actor->Cache,
																 *operation,
																 { });
						return aborted ? cancelled() : failed(aborted.error());
					}
					const auto next       = operation->Kind == epoch_catalog_operation_kind::prune
						? epoch_catalog_operation_phase::ref_delete_in_progress
						: epoch_catalog_operation_phase::
							  quarantine_rename_in_progress;
					auto       authorized = advance_maintenance(*context->Catalog,
																*operation,
																aRequest.Context.Catalog,
																next,
																aStop);
					if (!authorized) {
						if (authorized.error() == epoch_catalog_failure::cancelled) {
							const auto aborted = recover_maintenance(*context->Catalog,
																	 *caches,
																	 context->Handoff,
																	 context->Actor->Cache,
																	 *operation,
																	 { });
							return aborted ? cancelled() : failed(aborted.error());
						}
						return failed(catalog_failure(authorized.error()));
					}
					const auto recovered = recover_maintenance(*context->Catalog,
															   *caches,
															   context->Handoff,
															   context->Actor->Cache,
															   *authorized,
															   { });
					return recovered ? completed(*recovered)
									 : failed(recovered.error());
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::epoch_maintenance_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
				},
				[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
					aHost.dispatch_core(core::epoch_maintenance_failed_event{
						.Context = aRequest.Context,
						.Failure = aFailure,
					});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void start_history_page(
			const core::load_epoch_history_page_request& aRequest)
		{
			auto context = Active;
			if (!context || context->Authority != aRequest.Context.Authority
				|| context->Revision.Source != aRequest.Context.Source) {
				return;
			}
			auto actor = context->Actor;
			submit_effect(
				[context, aRequest](const std::stop_token aStop)
					-> worker_completion {
					if (
						!application::valid(context->Catalog->binding())
						|| context->Catalog->binding().Authority
							!= aRequest.Context.Authority
						|| context->Catalog->binding().Catalog
							!= aRequest.Context.Catalog) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_history_page_load_failed_event{
									.Context = aRequest.Context,
									.Failure = core::epoch_failure::catalog_conflict,
								});
						};
					}
					auto page = context->Catalog->history(
						epoch_history_page_request{
							.Catalog = aRequest.Context.Catalog,
							.Cursor  = aRequest.Context.Cursor,
							.Limit   = aRequest.Context.Limit,
						},
						aStop);
					if (!page) {
						if (page.error() == epoch_catalog_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::epoch_history_page_load_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = catalog_failure(page.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_history_page_load_failed_event{
									.Context = aRequest.Context,
									.Failure = failure,
								});
						};
					}
					auto observation = core::epoch_history_page_observation{
						.Authority    = page->Authority,
						.Catalog      = page->Catalog,
						.Epochs       = std::move(page->Epochs),
						.Continuation = page->Continuation,
					};
					return [aRequest, pageObservation = std::move(observation)](
							   implementation& aHost) {
						aHost.dispatch_core(core::epoch_history_page_loaded_event{
							.Context = aRequest.Context,
							.Page    = pageObservation,
						});
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(
						core::epoch_history_page_load_failed_event{
							.Context = aRequest.Context,
							.Failure = core::epoch_failure::unavailable,
						});
				},
				[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
					aHost.dispatch_core(
						core::epoch_history_page_load_failed_event{
							.Context = aRequest.Context,
							.Failure = aFailure,
						});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void start_comparison_resolution(
			const core::resolve_comparison_request& aRequest)
		{
			auto context = Active;
			if (!context || context->Authority != aRequest.Context.Authority
				|| context->Revision.Source != aRequest.Context.Source) {
				return;
			}
			auto* const caches = &Caches;
			auto        actor  = context->Actor;
			submit_effect(
				[context, aRequest, caches](const std::stop_token aStop)
					-> worker_completion {
					const auto snapshot = context->Catalog->snapshot(aStop);
					if (!snapshot) {
						if (snapshot.error() == epoch_catalog_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = catalog_failure(snapshot.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(core::comparison_resolve_failed_event{
								.Context = aRequest.Context,
								.Failure = failure,
							});
						};
					}
					if (
						!application::valid(snapshot->Binding)
						|| snapshot->Binding != context->Catalog->binding()) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_failed_event{
									.Context = aRequest.Context,
									.Failure = core::epoch_failure::repair_required,
								});
						};
					}
					const auto expectedCatalog = core::epoch_catalog_identity{
						.Incarnation = aRequest.Context.CatalogIncarnation,
						.Revision    = aRequest.Context.CatalogRevision,
					};
					if (snapshot->Binding.Catalog != expectedCatalog) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(core::comparison_resolve_failed_event{
								.Context = aRequest.Context,
								.Failure = core::epoch_failure::catalog_conflict,
							});
						};
					}

					core::comparison_candidates candidates{
						.Catalog              = snapshot->Binding.Catalog,
						.LatestLive           = snapshot->LatestLive,
						.PreviousDistinctHead = snapshot->PreviousDistinctHead,
					};
					if (snapshot->CurrentViewerReview) {
						candidates.CurrentViewerReview = review_anchor(
							context->Authority,
							snapshot->Binding.Catalog,
							*snapshot->CurrentViewerReview);
					}
					if (const auto* explicitPair = std::get_if<core::explicit_comparison_selection>(
							&aRequest.Context.Selection)) {
						std::vector<core::comparison_anchor_identity> identities{
							explicitPair->Left,
							explicitPair->Right,
						};
						std::ranges::sort(identities);
						identities.erase(
							std::unique(identities.begin(), identities.end()),
							identities.end());
						const auto exact = context->Catalog->exact_anchors(
							exact_anchor_lookup_request{
								.Catalog = snapshot->Binding.Catalog,
								.Anchors = identities,
							},
							aStop);
						if (!exact) {
							if (exact.error() == epoch_catalog_failure::cancelled) {
								return [aRequest](implementation& aHost) {
									aHost.dispatch_core(
										core::comparison_resolve_cancelled_event{
											.Context = aRequest.Context,
										});
								};
							}
							const auto failure = catalog_failure(exact.error());
							return [aRequest, failure](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_failed_event{
										.Context = aRequest.Context,
										.Failure = failure,
									});
							};
						}
						std::vector<core::comparison_anchor_identity>
							returnedIdentities;
						returnedIdentities.reserve(exact->Anchors.size());
						for (const auto& anchor : exact->Anchors) {
							if (
								!core::valid(anchor)
								|| !std::ranges::binary_search(
									identities, anchor.Identity)) {
								return [aRequest](implementation& aHost) {
									aHost.dispatch_core(
										core::comparison_resolve_failed_event{
											.Context = aRequest.Context,
											.Failure = core::epoch_failure::
												repair_required,
										});
								};
							}
							returnedIdentities.push_back(anchor.Identity);
						}
						std::ranges::sort(returnedIdentities);
						if (
							exact->Catalog != snapshot->Binding.Catalog
							|| exact->Anchors.size() > identities.size()
							|| std::ranges::adjacent_find(returnedIdentities)
								!= returnedIdentities.end()) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_failed_event{
										.Context = aRequest.Context,
										.Failure = core::epoch_failure::
											repair_required,
									});
							};
						}
						const auto findAnchor = [&exact](const auto& aIdentity)
							-> std::optional<core::comparison_anchor> {
							const auto found = std::ranges::find(
								exact->Anchors,
								aIdentity,
								&core::comparison_anchor::Identity);
							return found == exact->Anchors.end()
								? std::nullopt
								: std::optional{ *found };
						};
						candidates.ExplicitLeft  = findAnchor(explicitPair->Left);
						candidates.ExplicitRight = findAnchor(explicitPair->Right);
					}

					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.ObjectFormat       = context->Handoff.ObjectFormat,
					};
					const auto openedCache = open_actor_cache(
						*context->Actor, *caches, expectedCache, aStop);
					if (!openedCache) {
						if (openedCache.error() == git_cache_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = cache_failure(openedCache.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_failed_event{
									.Context = aRequest.Context,
									.Failure = failure,
								});
						};
					}
					std::vector<std::pair<core::git_commit_oid, bool>> verified;
					const auto                                         sparseCandidate = candidates.LatestLive
						&& candidates.LatestLive->Material.has_value()
						&& candidates.LatestLive->Base == context->Revision.Base
						&& candidates.LatestLive->Head == context->Revision.Head
						&& object_loading_for(context->Handoff.Shape)
							== pull_request_object_loading::selected_objects
						&& context->Handoff.SmartGit
						&& context->Handoff.SmartGit->supports_sparse_commit_trees();
					const auto sparseCurrentPair = [&] {
						if (!sparseCandidate) {
							return false;
						}
						const auto recorded = context->Catalog->retained_residency(
							candidates.LatestLive->Id.Ordinal, aStop);
						if (!recorded || *recorded != git_revision_residency::sparse_commit_trees) {
							return false;
						}
						const auto retained = context->Actor->Cache->inspect_sparse_retained(
							candidates.LatestLive->Id.Ordinal, aStop);
						return retained && application::valid(*retained)
							&& retained->Residency == git_revision_residency::sparse_commit_trees
							&& retained->Scope == context->Authority.Scope
							&& retained->Ordinal == candidates.LatestLive->Id.Ordinal
							&& retained->Base == context->Revision.Base
							&& retained->Head == context->Revision.Head;
					}();
					const auto isVerified = [&](const core::git_commit_oid aCommit) {
						const auto prior = std::ranges::find_if(
							verified,
							[&](const auto& aEntry) {
								return aEntry.first == aCommit;
							});
						if (prior != verified.end()) {
							return prior->second;
						}
						const auto inspected = context->Actor->Cache->inspect_commit_object(
							aCommit, aStop);
						auto exact = inspected
							&& application::valid(*inspected)
							&& inspected->Commit == aCommit
							&& inspected->Residency == git_revision_residency::complete_closure;
						if (!exact && sparseCurrentPair
							&& (aCommit == context->Revision.Base
								|| aCommit == context->Revision.Head)) {
							const auto sparse = context->Actor->Cache->inspect_sparse_commit_object(
								aCommit, aStop);
							exact = sparse && application::valid(*sparse)
								&& sparse->Commit == aCommit
								&& sparse->Residency == git_revision_residency::sparse_commit_trees
								&& sparse->Tree == (aCommit == context->Revision.Base ? candidates.LatestLive->Material->BaseTree : candidates.LatestLive->Material->HeadTree);
						}
						if (!exact && sparseCandidate) {
							const auto matchesSparseAnchor = [&](const std::optional<core::comparison_anchor>& aAnchor) {
								if (!aAnchor || aAnchor->Commit != aCommit
									|| (aAnchor->Identity.Kind != core::comparison_anchor_kind::epoch_base
										&& aAnchor->Identity.Kind != core::comparison_anchor_kind::epoch_head)
									|| aAnchor->Identity.Authority != context->Authority.Scope
									|| aAnchor->Identity.Incarnation != snapshot->Binding.Catalog.Incarnation) {
									return false;
								}
								const auto recorded = context->Catalog->retained_residency(aAnchor->Identity.Epoch, aStop);
								if (!recorded || *recorded != git_revision_residency::sparse_commit_trees) {
									return false;
								}
								const auto retained = context->Actor->Cache->inspect_sparse_retained(aAnchor->Identity.Epoch, aStop);
								if (!retained || !application::valid(*retained)
									|| retained->Residency != git_revision_residency::sparse_commit_trees
									|| retained->Scope != context->Authority.Scope
									|| retained->Ordinal != aAnchor->Identity.Epoch
									|| (aAnchor->Identity.Kind == core::comparison_anchor_kind::epoch_base
											? retained->Base
											: retained->Head)
										!= aCommit) {
									return false;
								}
								const auto sparse = context->Actor->Cache->inspect_sparse_commit_object(aCommit, aStop);
								return sparse && application::valid(*sparse) && sparse->Commit == aCommit
									&& sparse->Residency == git_revision_residency::sparse_commit_trees;
							};
							// [[BPR-1560]]. Previous upload's left side is the
							// preceding update's retained head, proved exactly as
							// an explicitly chosen one is.
							const auto previousHead = candidates.PreviousDistinctHead
								? std::optional{ core::comparison_anchor{
									  .Identity     = { .Authority   = candidates.PreviousDistinctHead->Id.Authority,
														.Incarnation = candidates.PreviousDistinctHead->Id.Incarnation,
														.Kind        = core::comparison_anchor_kind::epoch_head,
														.Epoch       = candidates.PreviousDistinctHead->Id.Ordinal },
									  .Commit       = candidates.PreviousDistinctHead->Head,
									  .Availability = candidates.PreviousDistinctHead->HeadAvailability } }
								: std::nullopt;
							exact                   = matchesSparseAnchor(candidates.ExplicitLeft)
								|| matchesSparseAnchor(candidates.ExplicitRight) || matchesSparseAnchor(previousHead);
						}
						verified.emplace_back(aCommit, exact);
						return exact;
					};
					const auto verifyEpoch = [&](auto& aEpoch) {
						if (
							aEpoch.BaseAvailability
								== core::anchor_availability::available
							&& !isVerified(aEpoch.Base)) {
							aEpoch.BaseAvailability = core::anchor_availability::unavailable;
						}
						if (
							aEpoch.HeadAvailability
								== core::anchor_availability::available
							&& !isVerified(aEpoch.Head)) {
							aEpoch.HeadAvailability = core::anchor_availability::unavailable;
						}
					};
					const auto verifyAnchor = [&](auto& aAnchor) {
						if (
							aAnchor
							&& aAnchor->Availability
								== core::anchor_availability::available
							&& !isVerified(aAnchor->Commit)) {
							aAnchor->Availability = core::anchor_availability::unavailable;
						}
					};
					if (candidates.LatestLive) {
						verifyEpoch(*candidates.LatestLive);
					}
					if (candidates.PreviousDistinctHead) {
						verifyEpoch(*candidates.PreviousDistinctHead);
					}
					verifyAnchor(candidates.CurrentViewerReview);
					verifyAnchor(candidates.ExplicitLeft);
					verifyAnchor(candidates.ExplicitRight);
					return [aRequest, resolvedCandidates = std::move(candidates)](
							   implementation& aHost) {
						aHost.dispatch_core(core::comparison_resolved_event{
							.Context    = aRequest.Context,
							.Candidates = resolvedCandidates,
						});
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::comparison_resolve_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
				},
				[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
					aHost.dispatch_core(core::comparison_resolve_failed_event{
						.Context = aRequest.Context,
						.Failure = aFailure,
					});
				},
				aRequest.Context.Request,
				std::nullopt,
				aRequest.Context.Generation,
				std::nullopt,
				std::move(actor));
		}

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
		mutable std::mutex                                 CacheActorsMutex;
		mutable std::mutex                                 JobsMutex;
		std::shared_ptr<authority_context>                 Active;
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
		std::uint64_t                          OfferToken               = 0U;
		bool                                   TurnActive               = false;
		bool                                   DeferredIngressScheduled = false;
		bool                                   DeferredShutdown         = false;
		bool                                   Closing                  = false;
		bool                                   ProjectionDeliveryFailed = false;
		std::atomic_bool                       BlobIngressClosing       = false;
	};

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
		return Implementation->materialize_agent_workspace(aRequest, aStop);
	}

	auto epoch_application_host::cleanup_agent_workspace(
		const owned_agent_workspace& aWorkspace,
		const std::stop_token        aStop)
		-> std::expected<void, agent_workspace_error>
	{
		return Implementation->cleanup_agent_workspace(aWorkspace, aStop);
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

	auto epoch_application_host::resolve_github_base_numbering(
		github_base_numbering_request    aRequest,
		github_base_numbering_completion aCompletion) -> bool
	{
		return Implementation->resolve_github_base_numbering(std::move(aRequest), std::move(aCompletion));
	}

	auto epoch_application_host::bind_source_search(const workbench_pull_request_authority& aAuthority, const core::comparison_pair& aComparison, std::optional<core::document_tab_id> aTab) -> std::optional<core::source_search_scope>
	{
		return Implementation->bind_source_search(aAuthority, aComparison, aTab);
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
