#include "epoch_host_support.hpp"

#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/diff_transition.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace bpr::application {
	auto validated_limits(const epoch_host_limits aLimits)
		-> epoch_host_limits
	{
		if (!valid(aLimits)) {
			throw std::invalid_argument{ "Invalid epoch host limits" };
		}
		return aLimits;
	}

	auto actor_key(
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

	auto actor_key(
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

	auto actor_key(
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

	auto actor_key(
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

	auto actor_key(
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
			|| (aRequest.Filter.has_value()
				&& (!core::valid(*aRequest.Filter) || aRequest.Filter->Left.algorithm() != algorithm))
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

	void epoch_diagnostics::record(diagnostic_epoch_stage aStage, diagnostic_outcome aOutcome,
								   diagnostic_epoch_failure               aFailure,
								   std::optional<git_pack_stream_failure> aPack,
								   git_smart_transport_step               aStep) const noexcept
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

	void epoch_diagnostics::failed(diagnostic_epoch_stage aStage, const git_cache_error& aFailure) const noexcept
	{
		failed(aStage, aFailure.Code);
	}

	void epoch_diagnostics::failed(diagnostic_epoch_stage aStage, const git_smart_transport_error& aFailure) const noexcept
	{
		record(aStage, diagnostic_outcome::failed, aFailure.Failure, { }, aFailure.Step);
	}

	void epoch_diagnostics::failed(diagnostic_epoch_stage aStage, const git_protocol_v2_error& aFailure) const noexcept
	{
		record(aStage, diagnostic_outcome::failed, aFailure.Failure, aFailure.PackFailure);
	}

	auto catalog_failure(
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

	auto cache_failure(const git_cache_error& aFailure) noexcept
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

	auto evidenced_live_update(const core::epoch_cause& aCause) noexcept -> bool
	{
		return aCause.Origin == core::epoch_origin::initial_observation
			|| (aCause.Origin == core::epoch_origin::live_observation
				&& (aCause.Head == core::epoch_head_transition::descendant
					|| aCause.Head == core::epoch_head_transition::non_fast_forward
					|| aCause.Head == core::epoch_head_transition::lineage_unknown));
	}

	auto read_occurrence_epochs(epoch_catalog_session& aCatalog,
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

	auto record_live_occurrence(
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

	auto reconcile_local_occurrences(const accepted_pull_request_handoff& aHandoff,
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

	auto invalidation_identity(
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

	auto recovery_request(const append_operation_record& aRecord)
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

	auto expected_retained_pair(
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

	auto same_append_identity(
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

	auto exact_catalog_commit(
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
}
