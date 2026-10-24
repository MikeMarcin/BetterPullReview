#include "bpr/application/github_capability_cache.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto authority(
		const std::uint64_t                       aProfile         = 1U,
		const std::uint64_t                       aProfileRevision = 1U,
		const std::uint64_t                       aHost            = 2U,
		const std::uint64_t                       aAccount         = 3U,
		const application::github_server_contract aServerContract  = application::github_server_contract::
			github_com_2022_11_28,
		const std::uint64_t aProbeRevision = 1U)
		-> application::github_capability_authority_key
	{
		return {
			.Profile               = core::host_profile_id{ aProfile },
			.ProfileRevision       = application::github_profile_revision{ aProfileRevision },
			.Host                  = core::git_host_id{ aHost },
			.Account               = core::github_account_id{ aAccount },
			.ServerContract        = aServerContract,
			.ProbeContractRevision = application::github_probe_contract_revision{
				aProbeRevision },
		};
	}

	auto conclusive_facts()
		-> std::array<
			application::github_capability_fact,
			application::github_capability_fact_count>
	{
		using capability = core::github_capability;
		using source     = application::github_capability_evidence_source;
		using state      = core::github_capability_state;
		return {
			application::github_capability_fact{
				.Capability = capability::review_threads,
				.State      = state::supported,
				.Source     = source::recorded_contract,
			},
			application::github_capability_fact{
				.Capability = capability::viewed_file_mutations,
				.State      = state::unsupported,
				.Source     = source::observed_contract,
			},
			application::github_capability_fact{
				.Capability = capability::resolve_thread,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			application::github_capability_fact{
				.Capability = capability::unresolve_thread,
				.State      = state::unsupported,
				.Source     = source::recorded_contract,
			},
		};
	}

	auto conclusive_schema_facts()
		-> std::array<
			application::github_schema_capability_fact,
			application::github_schema_capability_fact_count>
	{
		using capability = application::github_schema_capability;
		using fact       = application::github_schema_capability_fact;
		using source     = application::github_capability_evidence_source;
		using state      = core::github_capability_state;
		return {
			fact{
				.Capability = capability::review_threads_read,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::viewed_file_state_read,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::mark_file_as_viewed,
				.State      = state::unsupported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::unmark_file_as_viewed,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::resolve_permission,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::resolve_review_thread,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::unresolve_permission,
				.State      = state::supported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::unresolve_review_thread,
				.State      = state::unsupported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::reply_permission,
				.State      = state::unsupported,
				.Source     = source::observed_contract,
			},
			fact{
				.Capability = capability::conversation_lock,
				.State      = state::unsupported,
				.Source     = source::observed_contract,
			},
		};
	}

	auto observation(
		application::github_capability_authority_key aKey,
		const std::uint64_t                          aEpoch)
		-> application::github_capability_probe_observation
	{
		return {
			.Authority   = aKey,
			.ObservedAt  = application::github_freshness_epoch{ aEpoch },
			.Facts       = conclusive_facts(),
			.SchemaFacts = conclusive_schema_facts(),
		};
	}

	auto unavailable(
		application::github_capability_authority_key aKey,
		const std::uint64_t                          aEpoch)
		-> application::github_capability_probe_unavailable
	{
		return {
			.Authority    = aKey,
			.ObservedAt   = application::github_freshness_epoch{ aEpoch },
			.Capabilities = { },
			.Reason       = application::github_capability_unavailable_reason::network,
		};
	}

	auto make_cache(
		const application::github_capability_cache_policy aPolicy)
		-> application::github_capability_cache
	{
		auto result = application::github_capability_cache::create(aPolicy);
		expect(result.has_value(), "cache policy creates a bounded cache");
		return std::move(*result);
	}

	auto store(
		application::github_capability_cache&            aCache,
		application::github_capability_probe_observation aValue)
		-> application::github_capability_cache_store_result
	{
		return aCache.store( // NOLINT(clang-analyzer-cplusplus.Move): the moved-from cache contract is exercised by a caller.
			application::github_capability_probe_result{ std::move(aValue) });
	}

	auto store(
		application::github_capability_cache&            aCache,
		application::github_capability_probe_unavailable aValue)
		-> application::github_capability_cache_store_result
	{
		return aCache.store(
			application::github_capability_probe_result{ std::move(aValue) });
	}

	auto test_policy_and_evidence_are_bounded() -> void
	{
		using application::github_capability_cache;
		using application::github_capability_cache_error;
		using application::github_capability_cache_policy;

		expect(
			!github_capability_cache::create(
				 github_capability_cache_policy{ .Capacity = 0U })
				 .has_value(),
			"zero cache capacity is rejected");
		expect(
			!github_capability_cache::create(
				 github_capability_cache_policy{ .Capacity = 65U })
				 .has_value(),
			"capacity above the security ceiling is rejected");
		expect(
			!github_capability_cache::create(
				 github_capability_cache_policy{
					 .FreshnessWindowEpochs = 0U })
				 .has_value(),
			"zero freshness window is rejected");
		expect(
			!github_capability_cache::create(
				 github_capability_cache_policy{
					 .FreshnessWindowEpochs = 901U })
				 .has_value(),
			"freshness window above 900 epochs is rejected");
		expect(
			!github_capability_cache::create(
				 github_capability_cache_policy{
					 .FirstRevision = core::capability_revision{ } })
				 .has_value(),
			"zero first capability revision is rejected");

		auto validObservation = observation(authority(), 1U);
		expect(
			application::valid(validObservation),
			"four canonical conclusive facts are valid");
		const auto projected = application::projected_capabilities(validObservation);
		expect(
			projected.ReviewThreads
					== core::github_capability_state::supported
				&& projected.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& projected.ResolveThread
					== core::github_capability_state::supported
				&& projected.UnresolveThread
					== core::github_capability_state::unsupported,
			"canonical facts project without losing tri-state meaning");
		expect(
			application::projected_read_shape(validObservation)
				== application::github_read_shape{
					.ReviewThreads       = true,
					.ViewedFileState     = true,
					.ResolvePermission   = true,
					.UnresolvePermission = true,
				},
			"detailed schema evidence projects the exact production read shape");

		auto reordered = validObservation;
		std::swap(reordered.Facts[0U], reordered.Facts[1U]);
		expect(
			!application::valid(reordered),
			"facts outside canonical core-enum order are rejected");

		auto reorderedSchema = validObservation;
		std::swap(
			reorderedSchema.SchemaFacts[0U],
			reorderedSchema.SchemaFacts[1U]);
		expect(
			!application::valid(reorderedSchema),
			"schema facts outside canonical order are rejected");

		auto inconsistent                  = validObservation;
		inconsistent.SchemaFacts[5U].State = core::github_capability_state::unsupported;
		expect(
			!application::valid(inconsistent),
			"aggregate support cannot contradict detailed schema evidence");

		auto inconclusive            = validObservation;
		inconclusive.Facts[2U].State = core::github_capability_state::unknown;
		expect(
			!application::valid(inconclusive),
			"a cacheable observation cannot contain Unknown");

		auto assumed             = validObservation;
		assumed.Facts[2U].Source = application::github_capability_evidence_source::uninitialized;
		expect(
			!application::valid(assumed),
			"support without recorded or observed evidence is rejected");

		auto invalidState            = validObservation;
		invalidState.Facts[2U].State = static_cast<core::github_capability_state>(255U);
		expect(
			!application::valid(invalidState),
			"an unknown capability-state enumerator is rejected");

		auto invalidSource             = validObservation;
		invalidSource.Facts[2U].Source = static_cast<
			application::github_capability_evidence_source>(255U);
		expect(
			!application::valid(invalidSource),
			"an unknown evidence-source enumerator is rejected");

		auto wholeUnavailable = unavailable(authority(), 2U);
		expect(
			application::valid(wholeUnavailable)
				&& application::all_unknown(
					wholeUnavailable.Capabilities),
			"whole-probe unavailability is explicit all-Unknown");

		auto falseUnsupported                       = wholeUnavailable;
		falseUnsupported.Capabilities.ReviewThreads = core::github_capability_state::unsupported;
		expect(
			!application::valid(falseUnsupported),
			"unavailability cannot be mislabeled Unsupported");

		auto missingReason   = wholeUnavailable;
		missingReason.Reason = application::github_capability_unavailable_reason::none;
		expect(
			!application::valid(missingReason),
			"whole-probe unavailability requires a bounded reason");

		auto       cache            = make_cache({ });
		const auto unavailableStore = store(cache, wholeUnavailable);
		expect(
			!unavailableStore.stored()
				&& unavailableStore.Error
					== github_capability_cache_error::probe_unavailable
				&& cache.size() == 0U
				&& cache.last_epoch().value() == 0U,
			"a valid unavailable probe is never cached");

		const auto malformedStore = store(cache, falseUnsupported);
		expect(
			malformedStore.Error
				== github_capability_cache_error::invalid_probe,
			"a malformed unavailable probe is rejected distinctly");
	}

	auto test_every_authority_dimension_is_exact() -> void
	{
		using lookup_status = application::github_capability_cache_lookup_status;

		auto       cache  = make_cache({
			.Capacity              = 1U,
			.FreshnessWindowEpochs = 20U,
		});
		const auto base   = authority();
		const auto stored = store(cache, observation(base, 5U));
		expect(stored.stored(), "base authority is cached");

		const auto initialHit = cache.lookup(base, application::github_freshness_epoch{ 5U });
		expect(
			initialHit.hit() && initialHit.Entry.has_value()
				&& initialHit.Entry->Revision
					== core::capability_revision{ 1U },
			"exact authority hits with its generated revision");

		const std::array changedKeys{
			authority(9U, 1U, 2U, 3U),
			authority(1U, 9U, 2U, 3U),
			authority(1U, 1U, 9U, 3U),
			authority(1U, 1U, 2U, 9U),
			authority(
				1U,
				1U,
				2U,
				3U,
				application::github_server_contract::
					ghes_3_18_2022_11_28),
			authority(1U, 1U, 2U, 3U,
					  application::github_server_contract::
						  github_com_2022_11_28,
					  9U),
		};
		for (const auto& changed : changedKeys) {
			const auto result = cache.lookup(
				changed,
				application::github_freshness_epoch{ 5U });
			expect(
				result.Status == lookup_status::miss
					&& !result.Entry.has_value(),
				"changing one authority-key dimension produces a miss");
		}

		auto invalid             = base;
		invalid.Account          = { };
		const auto invalidLookup = cache.lookup(invalid, application::github_freshness_epoch{ 6U });
		expect(
			invalidLookup.Status == lookup_status::rejected
				&& invalidLookup.Error
					== application::github_capability_cache_error::
						invalid_authority
				&& cache.last_epoch().value() == 5U,
			"an invalid authority is rejected without advancing cache time");

		auto invalidContract           = base;
		invalidContract.ServerContract = static_cast<application::github_server_contract>(255U);
		expect(
			cache
					.lookup(
						invalidContract,
						application::github_freshness_epoch{ 6U })
					.Error
				== application::github_capability_cache_error::
					invalid_authority,
			"an unknown server-contract enumerator is rejected");

		const auto repeatedHit = cache.lookup(base, application::github_freshness_epoch{ 5U });
		expect(
			repeatedHit.hit()
				&& repeatedHit.Entry->Revision
					== initialHit.Entry->Revision,
			"cache hits reuse the stored capability revision");
	}

	auto test_freshness_and_epoch_failures_are_atomic() -> void
	{
		using error  = application::github_capability_cache_error;
		using status = application::github_capability_cache_lookup_status;

		auto       cache  = make_cache({
			.Capacity              = 2U,
			.FreshnessWindowEpochs = 3U,
		});
		const auto first  = authority();
		const auto stored = store(cache, observation(first, 10U));
		expect(
			stored.Entry.has_value()
				&& stored.Entry->ObservedAt.value() == 10U
				&& stored.Entry->ExpiresBefore.value() == 13U,
			"store records a half-open logical freshness interval");

		expect(
			cache.lookup(
					 first,
					 application::github_freshness_epoch{ 12U })
				.hit(),
			"entry is fresh immediately before ExpiresBefore");
		expect(
			cache.lookup(
					 first,
					 application::github_freshness_epoch{ 13U })
					.Status
				== status::expired,
			"entry expires exactly at ExpiresBefore");

		const auto regression = cache.lookup(
			first,
			application::github_freshness_epoch{ 12U });
		expect(
			regression.Status == status::rejected
				&& regression.Error == error::epoch_regression
				&& cache.last_epoch().value() == 13U,
			"epoch regression is rejected without changing cache time");

		const auto zeroEpoch = cache.lookup(first, application::github_freshness_epoch{ });
		expect(
			zeroEpoch.Error == error::invalid_epoch
				&& cache.last_epoch().value() == 13U,
			"zero epoch is rejected atomically");

		const auto maximum  = std::numeric_limits<std::uint64_t>::max();
		const auto overflow = store(
			cache,
			observation(
				authority(4U),
				maximum - 2U));
		expect(
			overflow.Error == error::epoch_overflow
				&& cache.size() == 1U
				&& cache.last_epoch().value() == 13U,
			"freshness addition overflow leaves all cache state unchanged");

		const auto second = store(cache, observation(authority(4U), 14U));
		expect(
			second.stored()
				&& second.Entry->Revision
					== core::capability_revision{ 2U },
			"an overflow rejection does not consume a revision");
	}

	auto test_capacity_never_evicts_fresh_authority() -> void
	{
		using disposition = application::github_capability_cache_store_disposition;
		using error       = application::github_capability_cache_error;

		auto       cache  = make_cache({
			.Capacity              = 2U,
			.FreshnessWindowEpochs = 5U,
		});
		const auto first  = authority(1U);
		const auto second = authority(2U);
		const auto third  = authority(3U);

		const auto firstStore  = store(cache, observation(first, 1U));
		const auto secondStore = store(cache, observation(second, 2U));
		expect(
			firstStore.Entry->Revision
					== core::capability_revision{ 1U }
				&& secondStore.Entry->Revision
					== core::capability_revision{ 2U }
				&& cache.size() == 2U,
			"the bounded cache accepts exactly N distinct authorities");

		const auto full = store(cache, observation(third, 3U));
		expect(
			full.Error == error::capacity_exhausted
				&& cache.size() == 2U
				&& cache.last_epoch().value() == 2U,
			"N plus one fresh authority fails without mutation");

		const auto reprobe = store(cache, observation(first, 3U));
		expect(
			reprobe.Disposition == disposition::replaced
				&& reprobe.Entry->Revision
					== core::capability_revision{ 3U }
				&& reprobe.Entry->Capabilities
					== firstStore.Entry->Capabilities,
			"same-state re-probe replaces in place and advances revision");
		expect(
			cache.lookup(
					 first,
					 application::github_freshness_epoch{ 3U })
					.Entry->Revision
				== core::capability_revision{ 3U },
			"a hit reuses the re-probe revision");

		const auto stillFull = store(cache, observation(third, 6U));
		expect(
			stillFull.Error == error::capacity_exhausted
				&& cache.last_epoch().value() == 3U,
			"a failed later store cannot evict either fresh entry");

		const auto reused = store(cache, observation(third, 7U));
		expect(
			reused.Disposition == disposition::reused_expired
				&& reused.Entry->Revision
					== core::capability_revision{ 4U }
				&& cache.size() == 2U,
			"a proven-expired slot is reusable at its boundary");
		expect(
			cache.lookup(
					 first,
					 application::github_freshness_epoch{ 7U })
					.Entry->Revision
				== core::capability_revision{ 3U },
			"expired-slot reuse preserves the unrelated fresh authority");
		expect(
			cache.lookup(
					 second,
					 application::github_freshness_epoch{ 7U })
					.Status
				== application::github_capability_cache_lookup_status::miss,
			"only the expired authority was replaced");
	}

	auto test_stage_is_non_mutating_and_commit_is_exact() -> void
	{
		using error = application::github_capability_cache_error;

		auto       cache     = make_cache({
			.Capacity              = 2U,
			.FreshnessWindowEpochs = 10U,
		});
		const auto first     = authority();
		const auto candidate = application::github_capability_probe_result{
			observation(first, 1U)
		};
		const auto staged = cache.stage(candidate);
		expect(
			staged.staged()
				&& staged.Stage->entry().Revision
					== core::capability_revision{ 1U }
				&& staged.Stage->entry().ReadShape.ReviewThreads
				&& cache.size() == 0U
				&& cache.last_epoch() == application::github_freshness_epoch{ },
			"staging computes an exact candidate without mutating cache state");

		const auto committed = cache.commit(*staged.Stage);
		expect(
			committed.stored() && cache.size() == 1U
				&& cache.last_epoch()
					== application::github_freshness_epoch{ 1U },
			"one exact stage commit mutates cache state");
		expect(
			cache.commit(*staged.Stage).Error == error::stale_stage
				&& cache.size() == 1U,
			"a committed stage cannot be replayed");

		const auto replacement = cache.stage(
			application::github_capability_probe_result{
				observation(first, 2U) });
		expect(replacement.staged(), "replacement can be staged");
		expect(
			cache.invalidate_exact(first).Removed == 1U,
			"intervening invalidation mutates the staged authority");
		expect(
			cache.commit(*replacement.Stage).Error == error::stale_stage
				&& cache.size() == 0U,
			"an intervening mutation invalidates the staged commit token");

		const auto expectZeroRemovalInvalidatesStage =
			[&](const auto& aInvalidate, const std::string_view aMessage) {
				const auto candidateStage = cache.stage(
					application::github_capability_probe_result{
						observation(first, 3U) });
				expect(candidateStage.staged(), "zero-removal stage creates");
				aInvalidate();
				expect(
					cache.commit(*candidateStage.Stage).Error
						== error::stale_stage,
					aMessage);
			};
		expectZeroRemovalInvalidatesStage(
			[&] {
				expect(
					cache.invalidate_exact(authority(9U)).Removed == 0U,
					"exact zero-removal invalidation is accepted");
			},
			"exact zero-removal invalidation invalidates an older stage");
		expectZeroRemovalInvalidatesStage(
			[&] {
				expect(
					cache.invalidate_profile(core::host_profile_id{ 9U }).Removed
						== 0U,
					"profile zero-removal invalidation is accepted");
			},
			"profile zero-removal invalidation invalidates an older stage");
		expectZeroRemovalInvalidatesStage(
			[&] {
				expect(
					cache.invalidate_identity(
							 core::host_profile_id{ 9U },
							 core::git_host_id{ 9U },
							 core::github_account_id{ 9U })
							.Removed
						== 0U,
					"identity zero-removal invalidation is accepted");
			},
			"identity zero-removal invalidation invalidates an older stage");
		expectZeroRemovalInvalidatesStage(
			[&] {
				expect(
					cache.clear() == 0U,
					"empty clear reports zero removals");
			},
			"empty clear invalidates an older stage");
	}

	auto test_invalid_probe_and_revision_overflow_are_atomic() -> void
	{
		using error = application::github_capability_cache_error;

		auto       cache = make_cache({
			.Capacity              = 2U,
			.FreshnessWindowEpochs = 10U,
		});
		const auto first = authority();
		expect(
			store(cache, observation(first, 1U))
					.Entry->Revision
				== core::capability_revision{ 1U },
			"first conclusive store generates revision one");

		auto malformed                 = observation(first, 4U);
		malformed.Facts[0U].Capability = core::github_capability::unresolve_thread;
		const auto rejected            = store(cache, malformed);
		expect(
			rejected.Error == error::invalid_probe
				&& cache.last_epoch().value() == 1U
				&& cache.size() == 1U,
			"malformed fact order cannot replace a cached entry");

		const auto afterRejection = store(cache, observation(authority(2U), 4U));
		expect(
			afterRejection.Entry->Revision
				== core::capability_revision{ 2U },
			"invalid evidence does not consume a capability revision");

		const auto maximum      = std::numeric_limits<std::uint64_t>::max();
		auto       exhausted    = make_cache({
			.Capacity              = 2U,
			.FreshnessWindowEpochs = 10U,
			.FirstRevision         = core::capability_revision{ maximum },
		});
		const auto maximumStore = store(exhausted, observation(first, 1U));
		expect(
			maximumStore.stored()
				&& maximumStore.Entry->Revision.value() == maximum,
			"the maximum nonzero capability revision is representable");

		const auto overflow = store(exhausted, observation(authority(2U), 2U));
		expect(
			overflow.Error == error::revision_overflow
				&& exhausted.size() == 1U
				&& exhausted.last_epoch().value() == 1U,
			"revision overflow cannot wrap or partially store");
		expect(
			exhausted
					.lookup(
						first,
						application::github_freshness_epoch{ 1U })
					.Entry->Revision.value()
				== maximum,
			"revision overflow preserves the last valid cached value");
	}

	auto test_invalidation_is_exact_and_isolated() -> void
	{
		auto       cache                     = make_cache({
			.Capacity              = 6U,
			.FreshnessWindowEpochs = 100U,
		});
		const auto exact                     = authority(1U, 1U, 2U, 3U);
		const auto sameIdentityOtherRevision = authority(1U, 2U, 2U, 3U);
		const auto otherAccount              = authority(1U, 1U, 2U, 8U);
		const auto otherProfile              = authority(9U, 1U, 2U, 3U);

		static_cast<void>(store(cache, observation(exact, 1U)));
		static_cast<void>(
			store(cache, observation(sameIdentityOtherRevision, 2U)));
		static_cast<void>(store(cache, observation(otherAccount, 3U)));
		static_cast<void>(store(cache, observation(otherProfile, 4U)));
		expect(cache.size() == 4U, "four isolated authorities are present");

		const auto exactResult = cache.invalidate_exact(exact);
		expect(
			exactResult.accepted() && exactResult.Removed == 1U
				&& cache.size() == 3U,
			"exact invalidation removes only the full key");

		auto invalid             = exact;
		invalid.Profile          = { };
		const auto invalidResult = cache.invalidate_exact(invalid);
		expect(
			!invalidResult.accepted() && invalidResult.Removed == 0U
				&& cache.size() == 3U,
			"invalid exact-invalidation input changes nothing");

		const auto identityResult = cache.invalidate_identity(
			core::host_profile_id{ 1U },
			core::git_host_id{ 2U },
			core::github_account_id{ 3U });
		expect(
			identityResult.accepted() && identityResult.Removed == 1U
				&& cache.size() == 2U,
			"identity invalidation crosses profile revisions only");
		expect(
			cache.lookup(
					 otherAccount,
					 application::github_freshness_epoch{ 4U })
				.hit(),
			"identity invalidation preserves another account");
		expect(
			cache.lookup(
					 otherProfile,
					 application::github_freshness_epoch{ 4U })
				.hit(),
			"identity invalidation preserves another profile");

		const auto profileResult = cache.invalidate_profile(core::host_profile_id{ 1U });
		expect(
			profileResult.accepted() && profileResult.Removed == 1U
				&& cache.size() == 1U,
			"profile invalidation removes all remaining profile identities");
		expect(
			cache.lookup(
					 otherProfile,
					 application::github_freshness_epoch{ 4U })
				.hit(),
			"profile invalidation preserves unrelated profiles");

		const auto invalidIdentity = cache.invalidate_identity(
			{ },
			core::git_host_id{ 2U },
			core::github_account_id{ 3U });
		expect(
			!invalidIdentity.accepted() && cache.size() == 1U,
			"invalid identity-invalidation input changes nothing");
		expect(
			cache.clear() == 1U && cache.size() == 0U,
			"clear reports the exact number removed");
	}

	auto test_move_and_result_helpers_fail_closed() -> void
	{
		using application::github_capability_cache_error;
		using application::github_capability_cache_lookup_result;
		using application::github_capability_cache_lookup_status;
		using application::github_capability_cache_store_disposition;
		using application::github_capability_cache_store_result;

		auto       source = make_cache({
			.Capacity              = 2U,
			.FreshnessWindowEpochs = 10U,
		});
		const auto first  = store(source, observation(authority(), 1U));
		expect(
			first.Entry->Revision == core::capability_revision{ 1U },
			"source cache allocates its first revision");

		auto       destination = std::move(source);
		const auto second      = store(destination, observation(authority(2U), 2U));
		expect(
			second.stored()
				&& second.Entry->Revision
					== core::capability_revision{ 2U },
			"move transfers the one revision sequence");

		const auto movedFromStore = store(source, observation(authority(3U), 2U));
		expect(
			!movedFromStore.stored()
				&& movedFromStore.Error
					== github_capability_cache_error::invalid_cache
				&& source.size() == 0U && source.capacity() == 0U
				&& source.last_epoch().value() == 0U,
			"moved-from cache cannot allocate duplicate revisions");
		expect(
			source
					.lookup(
						authority(),
						application::github_freshness_epoch{ 2U })
					.Error
				== github_capability_cache_error::invalid_cache,
			"moved-from cache lookup fails explicitly");

		github_capability_cache_store_result malformedStore{
			.Disposition = static_cast<
				github_capability_cache_store_disposition>(255U),
			.Entry = second.Entry,
		};
		expect(
			!malformedStore.stored(),
			"malformed store disposition cannot report success");
		malformedStore.Disposition = github_capability_cache_store_disposition::inserted;
		malformedStore.Error       = github_capability_cache_error::invalid_probe;
		expect(
			!malformedStore.stored(),
			"store error cannot report success");
		malformedStore.Error = github_capability_cache_error::none;
		malformedStore.Entry.reset();
		expect(
			!malformedStore.stored(),
			"store success requires the stored entry");

		github_capability_cache_lookup_result malformedLookup{
			.Status = github_capability_cache_lookup_status::hit,
			.Error  = github_capability_cache_error::none,
			.Entry  = std::nullopt,
		};
		expect(
			!malformedLookup.hit(),
			"cache hit requires a returned entry");
		malformedLookup.Entry = second.Entry;
		malformedLookup.Error = github_capability_cache_error::invalid_probe;
		expect(
			!malformedLookup.hit(),
			"cache hit with an error fails closed");
	}
}

int main()
{
	test_policy_and_evidence_are_bounded();
	test_every_authority_dimension_is_exact();
	test_freshness_and_epoch_failures_are_atomic();
	test_capacity_never_evicts_fresh_authority();
	test_stage_is_non_mutating_and_commit_is_exact();
	test_invalid_probe_and_revision_overflow_are_atomic();
	test_invalidation_is_exact_and_isolated();
	test_move_and_result_helpers_fail_closed();
	std::cout << "application github capability cache tests passed\n";
	return EXIT_SUCCESS;
}
