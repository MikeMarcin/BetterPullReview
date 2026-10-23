#include "bpr/application/github_capability_cache.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <new>
#include <optional>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] constexpr auto conclusive(
			const core::github_capability_state aState) noexcept -> bool
		{
			return aState == core::github_capability_state::supported
				|| aState == core::github_capability_state::unsupported;
		}

		[[nodiscard]] auto valid_facts(
			const std::array<
				github_capability_fact,
				github_capability_fact_count>& aFacts) noexcept -> bool
		{
			for (std::size_t index = 0U; index < aFacts.size(); ++index) {
				if (
					aFacts[index].Capability != canonical_github_capabilities[index]
					|| !conclusive(aFacts[index].State)
					|| !valid(aFacts[index].Source)) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto valid_schema_facts(
			const std::array<
				github_schema_capability_fact,
				github_schema_capability_fact_count>& aFacts) noexcept -> bool
		{
			for (std::size_t index = 0U; index < aFacts.size(); ++index) {
				if (
					aFacts[index].Capability
						!= canonical_github_schema_capabilities[index]
					|| !conclusive(aFacts[index].State)
					|| !valid(aFacts[index].Source)) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] constexpr auto combined_state(
			const std::initializer_list<core::github_capability_state> aStates) noexcept -> core::github_capability_state
		{
			for (const auto state : aStates) {
				if (state != core::github_capability_state::supported) {
					return core::github_capability_state::unsupported;
				}
			}
			return core::github_capability_state::supported;
		}

		[[nodiscard]] auto schema_capabilities(
			const std::array<
				github_schema_capability_fact,
				github_schema_capability_fact_count>& aFacts) noexcept
			-> core::github_capabilities
		{
			return {
				.ReviewThreads       = aFacts[0U].State,
				.ViewedFileMutations = combined_state(
					{ aFacts[1U].State, aFacts[2U].State, aFacts[3U].State }),
				.ResolveThread = combined_state(
					{ aFacts[0U].State, aFacts[4U].State, aFacts[5U].State }),
				.UnresolveThread = combined_state(
					{ aFacts[0U].State, aFacts[6U].State, aFacts[7U].State }),
			};
		}

		[[nodiscard]] auto lookup_rejected(
			const github_capability_cache_error aError) noexcept
			-> github_capability_cache_lookup_result
		{
			return {
				.Status = github_capability_cache_lookup_status::rejected,
				.Error  = aError,
				.Entry  = std::nullopt,
			};
		}

		[[nodiscard]] auto store_rejected(
			const github_capability_cache_error aError) noexcept
			-> github_capability_cache_store_result
		{
			return {
				.Disposition = github_capability_cache_store_disposition::rejected,
				.Error       = aError,
				.Entry       = std::nullopt,
			};
		}

		[[nodiscard]] auto stage_rejected(
			const github_capability_cache_error aError) noexcept
			-> github_capability_cache_stage_result
		{
			return {
				.Error = aError,
				.Stage = std::nullopt,
			};
		}

		void advance_mutation_generation(std::uint64_t& aGeneration) noexcept
		{
			if (aGeneration != std::numeric_limits<std::uint64_t>::max()) {
				++aGeneration;
			}
		}

		struct cache_storage_slot
		{
			std::vector<github_capability_cache_entry>::iterator Reusable;
			github_capability_cache_store_disposition            Disposition = github_capability_cache_store_disposition::inserted;
		};

		[[nodiscard]] auto select_storage_slot(
			std::vector<github_capability_cache_entry>& aEntries,
			const github_capability_authority_key&      aAuthority,
			const github_freshness_epoch                aObservedAt,
			const std::size_t                           aCapacity) noexcept
			-> std::expected<
				cache_storage_slot,
				github_capability_cache_error>
		{
			const auto sameAuthority = std::ranges::find(
				aEntries,
				aAuthority,
				&github_capability_cache_entry::Authority);
			if (sameAuthority != aEntries.end()) {
				return cache_storage_slot{
					.Reusable    = sameAuthority,
					.Disposition = github_capability_cache_store_disposition::replaced,
				};
			}
			if (aEntries.size() != aCapacity) {
				return cache_storage_slot{
					.Reusable    = aEntries.end(),
					.Disposition = github_capability_cache_store_disposition::inserted,
				};
			}

			const auto expired = std::ranges::find_if(
				aEntries,
				[aObservedAt](const auto& aEntry) {
					return aObservedAt >= aEntry.ExpiresBefore;
				});
			if (expired == aEntries.end()) {
				return std::unexpected{
					github_capability_cache_error::capacity_exhausted
				};
			}
			return cache_storage_slot{
				.Reusable    = expired,
				.Disposition = github_capability_cache_store_disposition::reused_expired,
			};
		}
	}

	auto all_unknown(const core::github_capabilities& aCapabilities) noexcept
		-> bool
	{
		return aCapabilities.ReviewThreads
			== core::github_capability_state::unknown
			&& aCapabilities.ViewedFileMutations
			== core::github_capability_state::unknown
			&& aCapabilities.ResolveThread
			== core::github_capability_state::unknown
			&& aCapabilities.UnresolveThread
			== core::github_capability_state::unknown;
	}

	auto valid(
		const github_capability_probe_observation& aObservation) noexcept -> bool
	{
		return valid(aObservation.Authority)
			&& aObservation.ObservedAt.value() != 0U
			&& valid_facts(aObservation.Facts)
			&& valid_schema_facts(aObservation.SchemaFacts)
			&& projected_capabilities(aObservation)
			== core::github_capabilities{
				   .ReviewThreads       = aObservation.Facts[0U].State,
				   .ViewedFileMutations = aObservation.Facts[1U].State,
				   .ResolveThread       = aObservation.Facts[2U].State,
				   .UnresolveThread     = aObservation.Facts[3U].State,
			   };
	}

	auto valid(
		const github_capability_probe_unavailable& aUnavailable) noexcept -> bool
	{
		return valid(aUnavailable.Authority)
			&& aUnavailable.ObservedAt.value() != 0U
			&& all_unknown(aUnavailable.Capabilities)
			&& valid(aUnavailable.Reason);
	}

	auto projected_capabilities(
		const github_capability_probe_observation& aObservation) noexcept
		-> core::github_capabilities
	{
		if (!valid_schema_facts(aObservation.SchemaFacts)) {
			return { };
		}

		return schema_capabilities(aObservation.SchemaFacts);
	}

	auto projected_read_shape(
		const github_capability_probe_observation& aObservation) noexcept
		-> github_read_shape
	{
		if (!valid_schema_facts(aObservation.SchemaFacts)) {
			return { };
		}

		const auto reviewThreads = aObservation.SchemaFacts[0U].State
			== core::github_capability_state::supported;
		return {
			.ReviewThreads   = reviewThreads,
			.ViewedFileState = aObservation.SchemaFacts[1U].State
				== core::github_capability_state::supported,
			.ResolvePermission = aObservation.SchemaFacts[4U].State
				== core::github_capability_state::supported,
			.UnresolvePermission = aObservation.SchemaFacts[6U].State
				== core::github_capability_state::supported,
			// [[BPR-PENDING-AX]]. Read with the threads, so only when they are.
			.ReplyPermission = reviewThreads
				&& aObservation.SchemaFacts[8U].State
					== core::github_capability_state::supported,
			.ConversationLock = reviewThreads
				&& aObservation.SchemaFacts[9U].State
					== core::github_capability_state::supported,
		};
	}

	github_capability_cache::github_capability_cache(
		github_capability_cache_policy aPolicy) noexcept
		: Policy{ aPolicy }
		, NextRevision{ aPolicy.FirstRevision.value() }
	{
	}

	github_capability_cache::github_capability_cache(
		github_capability_cache&& aOther) noexcept
		: Policy{ aOther.Policy }
		, Entries{ std::move(aOther.Entries) }
		, LastEpoch{ aOther.LastEpoch }
		, NextRevision{ aOther.NextRevision }
		, MutationGeneration{ aOther.MutationGeneration }
		, RevisionExhausted{ aOther.RevisionExhausted }
		, Usable{ aOther.Usable }
	{
		aOther.LastEpoch          = { };
		aOther.NextRevision       = 0U;
		aOther.MutationGeneration = 0U;
		aOther.RevisionExhausted  = true;
		aOther.Usable             = false;
	}

	auto github_capability_cache::create(
		github_capability_cache_policy aPolicy) noexcept
		-> std::expected<
			github_capability_cache,
			github_capability_cache_error>
	{
		if (!valid(aPolicy)) {
			return std::unexpected{
				github_capability_cache_error::invalid_policy
			};
		}

		github_capability_cache cache{ aPolicy };
		try {
			cache.Entries.reserve(aPolicy.Capacity);
		} catch (const std::bad_alloc&) {
			return std::unexpected{
				github_capability_cache_error::allocation_failure
			};
		}
		return cache;
	}

	auto github_capability_cache::lookup(
		const github_capability_authority_key& aAuthority,
		const github_freshness_epoch           aNow) noexcept
		-> github_capability_cache_lookup_result
	{
		if (!Usable) {
			return lookup_rejected(
				github_capability_cache_error::invalid_cache);
		}
		if (!valid(aAuthority)) {
			return lookup_rejected(
				github_capability_cache_error::invalid_authority);
		}
		if (aNow.value() == 0U) {
			return lookup_rejected(
				github_capability_cache_error::invalid_epoch);
		}
		if (aNow < LastEpoch) {
			return lookup_rejected(
				github_capability_cache_error::epoch_regression);
		}

		const auto found = std::ranges::find(
			Entries,
			aAuthority,
			&github_capability_cache_entry::Authority);
		LastEpoch = aNow;
		if (found == Entries.end()) {
			return {
				.Status = github_capability_cache_lookup_status::miss,
				.Error  = github_capability_cache_error::none,
				.Entry  = std::nullopt,
			};
		}
		if (aNow >= found->ExpiresBefore) {
			return {
				.Status = github_capability_cache_lookup_status::expired,
				.Error  = github_capability_cache_error::none,
				.Entry  = std::nullopt,
			};
		}
		return {
			.Status = github_capability_cache_lookup_status::hit,
			.Entry  = *found,
		};
	}

	auto github_capability_cache::store(
		const github_capability_probe_result& aResult) noexcept
		-> github_capability_cache_store_result
	{
		const auto staged = stage(aResult);
		if (!staged.staged()) {
			return store_rejected(staged.Error);
		}
		return commit(*staged.Stage);
	}

	auto github_capability_cache::stage(
		const github_capability_probe_result& aResult) noexcept
		-> github_capability_cache_stage_result
	{
		if (!Usable) {
			return stage_rejected(
				github_capability_cache_error::invalid_cache);
		}
		if (aResult.valueless_by_exception()) {
			return stage_rejected(
				github_capability_cache_error::invalid_probe);
		}
		if (const auto* unavailable = std::get_if<github_capability_probe_unavailable>(&aResult);
			unavailable != nullptr) {
			if (!valid(unavailable->Authority)) {
				return stage_rejected(
					github_capability_cache_error::invalid_authority);
			}
			if (unavailable->ObservedAt.value() == 0U) {
				return stage_rejected(
					github_capability_cache_error::invalid_epoch);
			}
			if (unavailable->ObservedAt < LastEpoch) {
				return stage_rejected(
					github_capability_cache_error::epoch_regression);
			}
			if (!valid(*unavailable)) {
				return stage_rejected(
					github_capability_cache_error::invalid_probe);
			}
			return stage_rejected(
				github_capability_cache_error::probe_unavailable);
		}

		const auto& observation = std::get<github_capability_probe_observation>(aResult);
		if (!valid(observation.Authority)) {
			return stage_rejected(
				github_capability_cache_error::invalid_authority);
		}
		if (observation.ObservedAt.value() == 0U) {
			return stage_rejected(
				github_capability_cache_error::invalid_epoch);
		}
		if (observation.ObservedAt < LastEpoch) {
			return stage_rejected(
				github_capability_cache_error::epoch_regression);
		}
		if (!valid(observation)) {
			return stage_rejected(
				github_capability_cache_error::invalid_probe);
		}
		if (
			observation.ObservedAt.value()
			> std::numeric_limits<std::uint64_t>::max()
				- Policy.FreshnessWindowEpochs) {
			return stage_rejected(
				github_capability_cache_error::epoch_overflow);
		}
		if (RevisionExhausted) {
			return stage_rejected(
				github_capability_cache_error::revision_overflow);
		}

		const auto slot = select_storage_slot(
			Entries,
			observation.Authority,
			observation.ObservedAt,
			Policy.Capacity);
		if (!slot.has_value()) {
			return stage_rejected(slot.error());
		}

		const auto expiresBefore = github_freshness_epoch{
			observation.ObservedAt.value()
			+ Policy.FreshnessWindowEpochs
		};
		const auto                          revision = core::capability_revision{ NextRevision };
		const github_capability_cache_entry entry{
			.Authority     = observation.Authority,
			.Facts         = observation.Facts,
			.SchemaFacts   = observation.SchemaFacts,
			.Capabilities  = projected_capabilities(observation),
			.ReadShape     = projected_read_shape(observation),
			.Revision      = revision,
			.ObservedAt    = observation.ObservedAt,
			.ExpiresBefore = expiresBefore,
		};

		return {
			.Error = github_capability_cache_error::none,
			.Stage = github_capability_cache_stage{
				entry,
				slot->Disposition,
				MutationGeneration },
		};
	}

	auto github_capability_cache::commit(
		const github_capability_cache_stage& aStage) noexcept
		-> github_capability_cache_store_result
	{
		if (!Usable) {
			return store_rejected(
				github_capability_cache_error::invalid_cache);
		}
		if (
			aStage.MutationGeneration != MutationGeneration
			|| aStage.Entry.Revision.value() != NextRevision) {
			return store_rejected(
				github_capability_cache_error::stale_stage);
		}
		if (aStage.Entry.ObservedAt < LastEpoch) {
			return store_rejected(
				github_capability_cache_error::epoch_regression);
		}
		if (
			MutationGeneration
				== std::numeric_limits<std::uint64_t>::max()
			|| RevisionExhausted) {
			return store_rejected(
				github_capability_cache_error::revision_overflow);
		}

		const github_capability_probe_observation observation{
			.Authority   = aStage.Entry.Authority,
			.ObservedAt  = aStage.Entry.ObservedAt,
			.Facts       = aStage.Entry.Facts,
			.SchemaFacts = aStage.Entry.SchemaFacts,
		};
		if (
			!valid(observation)
			|| aStage.Entry.Capabilities
				!= projected_capabilities(observation)
			|| aStage.Entry.ReadShape != projected_read_shape(observation)
			|| aStage.Entry.ObservedAt.value()
				> std::numeric_limits<std::uint64_t>::max()
					- Policy.FreshnessWindowEpochs
			|| aStage.Entry.ExpiresBefore
				!= github_freshness_epoch{
					aStage.Entry.ObservedAt.value()
					+ Policy.FreshnessWindowEpochs }) {
			return store_rejected(
				github_capability_cache_error::invalid_probe);
		}

		const auto slot = select_storage_slot(
			Entries,
			aStage.Entry.Authority,
			aStage.Entry.ObservedAt,
			Policy.Capacity);
		if (!slot.has_value()) {
			return store_rejected(slot.error());
		}
		if (slot->Disposition != aStage.Disposition) {
			return store_rejected(
				github_capability_cache_error::stale_stage);
		}

		if (slot->Reusable == Entries.end()) {
			try {
				Entries.push_back(aStage.Entry);
			} catch (const std::bad_alloc&) {
				return store_rejected(
					github_capability_cache_error::allocation_failure);
			}
		} else {
			*slot->Reusable = aStage.Entry;
		}

		if (NextRevision == std::numeric_limits<std::uint64_t>::max()) {
			RevisionExhausted = true;
		} else {
			++NextRevision;
		}
		LastEpoch = aStage.Entry.ObservedAt;
		advance_mutation_generation(MutationGeneration);

		return {
			.Disposition = slot->Disposition,
			.Entry       = aStage.Entry,
		};
	}

	auto github_capability_cache::invalidate_exact(
		const github_capability_authority_key& aAuthority) noexcept
		-> github_capability_cache_invalidation_result
	{
		if (!Usable) {
			return {
				.Error = github_capability_cache_error::invalid_cache,
			};
		}
		if (!valid(aAuthority)) {
			return {
				.Error = github_capability_cache_error::invalid_authority,
			};
		}

		const auto oldSize = Entries.size();
		std::erase_if(
			Entries,
			[&aAuthority](const auto& aEntry) {
				return aEntry.Authority == aAuthority;
			});
		const auto removed = oldSize - Entries.size();
		advance_mutation_generation(MutationGeneration);
		return { .Removed = removed };
	}

	auto github_capability_cache::invalidate_profile(
		const core::host_profile_id aProfile) noexcept
		-> github_capability_cache_invalidation_result
	{
		if (!Usable) {
			return {
				.Error = github_capability_cache_error::invalid_cache,
			};
		}
		if (aProfile.value() == 0U) {
			return {
				.Error = github_capability_cache_error::invalid_authority,
			};
		}

		const auto oldSize = Entries.size();
		std::erase_if(
			Entries,
			[aProfile](const auto& aEntry) {
				return aEntry.Authority.Profile == aProfile;
			});
		const auto removed = oldSize - Entries.size();
		advance_mutation_generation(MutationGeneration);
		return { .Removed = removed };
	}

	auto github_capability_cache::invalidate_identity(
		const core::host_profile_id   aProfile,
		const core::git_host_id       aHost,
		const core::github_account_id aAccount) noexcept
		-> github_capability_cache_invalidation_result
	{
		if (!Usable) {
			return {
				.Error = github_capability_cache_error::invalid_cache,
			};
		}
		if (
			aProfile.value() == 0U || aHost.value() == 0U
			|| aAccount.value() == 0U) {
			return {
				.Error = github_capability_cache_error::invalid_authority,
			};
		}

		const auto oldSize = Entries.size();
		std::erase_if(
			Entries,
			[aProfile, aHost, aAccount](const auto& aEntry) {
				return aEntry.Authority.Profile == aProfile
					&& aEntry.Authority.Host == aHost
					&& aEntry.Authority.Account == aAccount;
			});
		const auto removed = oldSize - Entries.size();
		advance_mutation_generation(MutationGeneration);
		return { .Removed = removed };
	}

	auto github_capability_cache::clear() noexcept -> std::size_t
	{
		if (!Usable) {
			return 0U;
		}
		const auto oldSize = Entries.size();
		Entries.clear();
		advance_mutation_generation(MutationGeneration);
		return oldSize;
	}

	auto github_capability_cache::size() const noexcept -> std::size_t
	{
		return Usable ? Entries.size() : 0U;
	}

	auto github_capability_cache::capacity() const noexcept -> std::size_t
	{
		return Usable ? Policy.Capacity : 0U;
	}

	auto github_capability_cache::last_epoch() const noexcept
		-> github_freshness_epoch
	{
		return Usable ? LastEpoch : github_freshness_epoch{ };
	}
}
