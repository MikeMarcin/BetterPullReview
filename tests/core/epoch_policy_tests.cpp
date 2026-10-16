#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto test_git_tool_failure_reasons() -> void
	{
		expect(reason_for(epoch_failure::git_tool_unsupported) == epoch_reason_code::git_tool_unsupported,
			   "unsupported Git operations retain their actionable reason");
		expect(reason_for(epoch_failure::git_tool_probe_failed) == epoch_reason_code::git_tool_probe_failed,
			   "a failed Git check is distinct from unsupported operations");
		expect(reason_for(epoch_failure::gh_tool_unsupported) == epoch_reason_code::gh_tool_unsupported,
			   "unsupported GitHub CLI operations retain their actionable reason");
		expect(reason_for(epoch_failure::gh_tool_probe_failed) == epoch_reason_code::gh_tool_probe_failed,
			   "a failed GitHub CLI check is distinct from unsupported operations and from Git");
	}

	auto test_sha256_known_vectors() -> void
	{
		static constexpr std::array<std::uint8_t, 32> emptyDigest{
			0xe3U,
			0xb0U,
			0xc4U,
			0x42U,
			0x98U,
			0xfcU,
			0x1cU,
			0x14U,
			0x9aU,
			0xfbU,
			0xf4U,
			0xc8U,
			0x99U,
			0x6fU,
			0xb9U,
			0x24U,
			0x27U,
			0xaeU,
			0x41U,
			0xe4U,
			0x64U,
			0x9bU,
			0x93U,
			0x4cU,
			0xa4U,
			0x95U,
			0x99U,
			0x1bU,
			0x78U,
			0x52U,
			0xb8U,
			0x55U,
		};
		static constexpr std::array<std::uint8_t, 3>  abc{ 'a', 'b', 'c' };
		static constexpr std::array<std::uint8_t, 32> abcDigest{
			0xbaU,
			0x78U,
			0x16U,
			0xbfU,
			0x8fU,
			0x01U,
			0xcfU,
			0xeaU,
			0x41U,
			0x41U,
			0x40U,
			0xdeU,
			0x5dU,
			0xaeU,
			0x22U,
			0x23U,
			0xb0U,
			0x03U,
			0x61U,
			0xa3U,
			0x96U,
			0x17U,
			0x7aU,
			0x9cU,
			0xb4U,
			0x10U,
			0xffU,
			0x61U,
			0xf2U,
			0x00U,
			0x15U,
			0xadU,
		};
		expect(sha256(std::span<const std::uint8_t>{ }) == emptyDigest,
			   "SHA-256 hashes the empty message canonically");
		expect(sha256(abc) == abcDigest,
			   "SHA-256 hashes a non-empty message canonically");
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	template <typename Oid>
	[[nodiscard]] constexpr auto oid(
		const std::uint8_t       aSeed,
		const git_hash_algorithm aAlgorithm = git_hash_algorithm::sha1) -> Oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		const std::size_t            count = aAlgorithm == git_hash_algorithm::sha1 ? 20U : 32U;
		for (std::size_t index = 0; index < count; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return Oid{ aAlgorithm, bytes };
	}

	[[nodiscard]] auto authority(const std::uint8_t aSeed = 1U)
		-> pull_request_authority
	{
		return pull_request_authority{
			.Profile         = host_profile_id{ static_cast<std::uint64_t>(aSeed) + 10U },
			.ProfileRevision = github_profile_revision{ static_cast<std::uint64_t>(aSeed) + 20U },
			.Host            = git_host_id{ static_cast<std::uint64_t>(aSeed) + 30U },
			.HostAuthority   = opaque<host_authority_fingerprint>(
				static_cast<std::uint8_t>(aSeed + 1U)),
			.Account    = github_account_id{ static_cast<std::uint64_t>(aSeed) + 40U },
			.Viewer     = github_viewer_actor_id{ static_cast<std::uint64_t>(aSeed) + 50U },
			.Repository = repository_id{ static_cast<std::uint64_t>(aSeed) + 60U },
			.Cache      = opaque<repository_cache_scope_id>(
				static_cast<std::uint8_t>(aSeed + 2U)),
			.PullRequest = pull_request_id{ static_cast<std::uint64_t>(aSeed) + 70U },
			.Number      = pull_request_number{ static_cast<std::uint32_t>(aSeed) + 80U },
			.Scope       = opaque<pull_request_authority_scope_id>(
				static_cast<std::uint8_t>(aSeed + 3U)),
		};
	}

	[[nodiscard]] auto provider_review(
		const std::uint8_t           aSeed,
		const git_commit_oid         aCommit,
		const github_viewer_actor_id aActor = github_viewer_actor_id{ 51U })
		-> current_viewer_review_candidate
	{
		return current_viewer_review_candidate{
			.Review = github_review_id{ static_cast<std::uint64_t>(aSeed) + 20U },
			.Actor  = aActor,
			.Order  = github_review_order{ static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(aSeed) + 1U) },
			.State  = github_review_state::commented,
			.Commit = aCommit,
		};
	}

	[[nodiscard]] auto associated_review(
		const std::uint8_t           aSeed,
		const git_commit_oid         aCommit,
		const anchor_availability    aAvailability = anchor_availability::available,
		const github_viewer_actor_id aActor        = github_viewer_actor_id{ 51U })
		-> catalog_review_association
	{
		const auto provider = provider_review(aSeed, aCommit, aActor);
		return catalog_review_association{
			.Association = opaque<review_association_id>(
				static_cast<std::uint8_t>(aSeed + 10U)),
			.Review       = provider.Review,
			.Actor        = provider.Actor,
			.Order        = provider.Order,
			.State        = provider.State,
			.Commit       = provider.Commit,
			.Availability = aAvailability,
		};
	}

	[[nodiscard]] auto revision(
		const std::uint64_t                            aSnapshot      = 1U,
		const std::uint8_t                             aBase          = 10U,
		const std::uint8_t                             aHead          = 20U,
		const std::uint8_t                             aBranch        = 30U,
		const pull_request_revision_origin             aOrigin        = pull_request_revision_origin::live,
		std::optional<current_viewer_review_candidate> aCurrentReview = std::nullopt) -> accepted_pull_request_revision
	{
		return accepted_pull_request_revision{
			.Source = pull_request_source_revision{
				.Observation = opaque<epoch_source_observation_id>(
					static_cast<std::uint8_t>(aSnapshot)),
				.Snapshot              = provider_snapshot_id{ aSnapshot },
				.IdentityGeneration    = github_identity_generation{ 3U },
				.PullRequestGeneration = request_generation{ 4U },
			},
			.Origin              = aOrigin,
			.BaseBranch          = opaque<base_branch_id>(aBranch),
			.Base                = oid<git_commit_oid>(aBase),
			.Head                = oid<git_commit_oid>(aHead),
			.CurrentViewerReview = std::move(aCurrentReview),
		};
	}

	[[nodiscard]] auto catalog(const std::int64_t aValue = 0)
		-> epoch_catalog_identity
	{
		return epoch_catalog_identity{
			.Incarnation = opaque<epoch_catalog_incarnation_id>(41U),
			.Revision    = epoch_catalog_revision{ aValue },
		};
	}

	[[nodiscard]] auto material(
		const std::uint8_t  aBase   = 60U,
		const std::uint8_t  aHead   = 70U,
		const std::uint64_t aPolicy = 1U) -> epoch_material_identity
	{
		return epoch_material_identity{
			.BaseTree       = oid<git_tree_oid>(aBase),
			.HeadTree       = oid<git_tree_oid>(aHead),
			.PolicyRevision = comparison_policy_revision{ aPolicy },
		};
	}

	[[nodiscard]] auto summary(
		const pull_request_authority&          aOwner,
		const epoch_catalog_identity&          aOwnerCatalog,
		const std::int64_t                     aOrdinal,
		const std::uint8_t                     aBase,
		const std::uint8_t                     aHead,
		const std::uint8_t                     aBranch           = 30U,
		const epoch_origin                     aOrigin           = epoch_origin::live_observation,
		const anchor_availability              aBaseAvailability = anchor_availability::available,
		const anchor_availability              aHeadAvailability = anchor_availability::available,
		std::optional<epoch_material_identity> aComparedMaterial = material())
		-> review_epoch_summary
	{
		return review_epoch_summary{
			.Id = review_epoch_id{
				.Authority   = aOwner.Scope,
				.Incarnation = aOwnerCatalog.Incarnation,
				.Ordinal     = epoch_ordinal{ aOrdinal },
			},
			.Source = pull_request_source_revision{
				.Observation           = opaque<epoch_source_observation_id>(static_cast<std::uint8_t>(aOrdinal + 90)),
				.Snapshot              = provider_snapshot_id{ static_cast<std::uint64_t>(aOrdinal) + 100U },
				.IdentityGeneration    = github_identity_generation{ 3U },
				.PullRequestGeneration = request_generation{ 4U },
			},
			.BaseBranch = opaque<base_branch_id>(aBranch),
			.Base       = oid<git_commit_oid>(aBase),
			.Head       = oid<git_commit_oid>(aHead),
			.Material   = std::move(aComparedMaterial),
			.Cause      = epoch_cause{
				.Origin   = aOrigin,
				.Base     = aOrigin == epoch_origin::explicit_historical_import ? epoch_base_transition::not_applicable : epoch_base_transition::unchanged,
				.Head     = aOrigin == epoch_origin::explicit_historical_import ? epoch_head_transition::not_applicable : epoch_head_transition::descendant,
				.Material = epoch_material_relation::different,
				.Primary  = aOrigin == epoch_origin::explicit_historical_import ? epoch_primary_cause::explicit_historical_import : epoch_primary_cause::author_upload,
			},
			.ObservedAt       = observed_utc_milliseconds{ aOrdinal * 100 },
			.BaseAvailability = aBaseAvailability,
			.HeadAvailability = aHeadAvailability,
		};
	}

	[[nodiscard]] auto anchor(
		const review_epoch_summary& aEpoch, const comparison_anchor_kind aKind)
		-> comparison_anchor
	{
		return comparison_anchor{
			.Identity = comparison_anchor_identity{
				.Authority   = aEpoch.Id.Authority,
				.Incarnation = aEpoch.Id.Incarnation,
				.Kind        = aKind,
				.Epoch       = aEpoch.Id.Ordinal,
			},
			.Commit       = aKind == comparison_anchor_kind::epoch_base ? aEpoch.Base : aEpoch.Head,
			.Availability = aKind == comparison_anchor_kind::epoch_base ? aEpoch.BaseAvailability : aEpoch.HeadAvailability,
		};
	}

	[[nodiscard]] auto review_anchor(
		const pull_request_authority&     aOwner,
		const epoch_catalog_identity&     aOwnerCatalog,
		const catalog_review_association& aCurrent) -> comparison_anchor
	{
		return comparison_anchor{
			.Identity = comparison_anchor_identity{
				.Authority   = aOwner.Scope,
				.Incarnation = aOwnerCatalog.Incarnation,
				.Kind        = comparison_anchor_kind::review_commit,
				.Review      = aCurrent.Association,
			},
			.Commit       = aCurrent.Commit,
			.Availability = aCurrent.Availability,
		};
	}

	template <typename Request>
	[[nodiscard]] auto request_from(const epoch_transition_output& aOutput)
		-> const Request*
	{
		for (const auto& request : aOutput.Requests) {
			if (const auto* typed = std::get_if<Request>(&request)) {
				return typed;
			}
		}
		return nullptr;
	}

	/// The position of the first request of this type, or the request count
	/// when there is none, so an order check fails when either is missing.
	template <typename Request>
	[[nodiscard]] auto request_index(const epoch_transition_output& aOutput)
		-> std::size_t
	{
		for (std::size_t index = 0U; index < aOutput.Requests.size(); ++index) {
			if (std::holds_alternative<Request>(aOutput.Requests[index])) {
				return index;
			}
		}
		return aOutput.Requests.size();
	}

	[[nodiscard]] auto initial_state() -> epoch_state
	{
		return epoch_state{
			.ApplicationSession = application_session_id{ 91U },
		};
	}

	struct ready_fixture
	{
		epoch_state                    State;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;
		epoch_catalog_identity         Catalog;
		review_epoch_summary           Epoch;

		auto operator<=>(const ready_fixture&) const = default;
	};

	[[nodiscard]] auto make_ready_fixture(epoch_state aState = initial_state())
		-> ready_fixture
	{
		const auto owner          = authority();
		const auto accepted       = revision();
		const auto initialCatalog = catalog();

		const auto acceptedOutput = transition(
			aState,
			pull_request_revision_accepted_event{
				.ApplicationSession = aState.ApplicationSession,
				.Authority          = owner,
				.Revision           = accepted,
			});
		const auto* load = request_from<load_epoch_catalog_request>(acceptedOutput);
		expect(load != nullptr, "new authority requests a catalog load");

		const auto loadedOutput = transition(
			aState,
			epoch_catalog_loaded_event{
				.Context = load->Context,
				.Catalog = initialCatalog,
			});
		const auto* prepare = request_from<prepare_epoch_revision_request>(loadedOutput);
		expect(prepare != nullptr, "catalog load requests preparation");

		const auto preparedOutput = transition(
			aState,
			epoch_revision_prepared_event{
				.Context     = prepare->Context,
				.Operation   = opaque<epoch_operation_id>(51U),
				.Observation = epoch_preparation_observation{
					.ProposedOrdinal = epoch_ordinal{ 1 },
					.ObservedAt      = observed_utc_milliseconds{ 100 },
					.Material        = material(),
				},
			});
		const auto* commit = request_from<commit_epoch_catalog_request>(preparedOutput);
		expect(commit != nullptr, "preparation requests an exact catalog commit");
		const auto* append = std::get_if<append_epoch_mutation>(&commit->Mutation);
		expect(append != nullptr, "initial commit carries an append mutation");

		const auto committedOutput = transition(
			aState,
			epoch_catalog_committed_event{
				.Context     = commit->Context,
				.Mutation    = commit->Mutation,
				.NewRevision = epoch_catalog_revision{ 1 },
			});
		const auto* resolve = request_from<resolve_comparison_request>(committedOutput);
		expect(resolve != nullptr, "commit requests semantic comparison");
		const auto committedEpoch = append->Epoch;
		const auto updatedCatalog = catalog(1);

		// BPR-0725. A committed append settles the catalogue, so ingestion asks
		// for the newest history page ahead of the comparison; the host's
		// actor FIFO then publishes the rows before the ready status.
		const auto* newest = request_from<load_epoch_history_page_request>(committedOutput);
		if (aState.RequestCounterExhausted) {
			expect(newest == nullptr && !committedOutput.History && !aState.PendingHistory,
				   "an exhausted request counter skips the newest page without "
				   "failing the ingestion that succeeded");
		} else {
			expect(
				newest != nullptr
					&& request_index<load_epoch_history_page_request>(committedOutput)
						< request_index<resolve_comparison_request>(committedOutput)
					&& newest->Context.Catalog == updatedCatalog
					&& newest->Context.Navigation == epoch_history_navigation::newest
					&& !newest->Context.Cursor
					&& newest->Context.Limit == maximum_epoch_history_page_size
					&& committedOutput.AcceptedEpoch.has_value()
					&& committedOutput.History
					&& committedOutput.History->Phase == epoch_history_page_phase::loading,
				"a committed append requests the newest history page before the "
				"comparison");
			// The fixture's ready state keeps no history window, as after a
			// cancelled newest page; tests that need a window load it themselves.
			const auto cancelledPage = transition(
				aState,
				epoch_history_page_load_cancelled_event{ .Context = newest->Context });
			expect(
				cancelledPage.History
					&& cancelledPage.History->Phase == epoch_history_page_phase::cancelled
					&& !aState.PendingHistory && !aState.HistoryCatalog,
				"the fixture's newest page ends without a history window");
		}

		const auto resolvedOutput = transition(
			aState,
			comparison_resolved_event{
				.Context    = resolve->Context,
				.Candidates = comparison_candidates{
					.Catalog    = updatedCatalog,
					.LatestLive = committedEpoch,
				},
			});
		expect(
			resolvedOutput.Comparison.has_value()
				&& resolvedOutput.Comparison->AvailableForDiff,
			"fixture resolves an available Whole PR pair");
		return ready_fixture{
			.State     = aState,
			.Authority = owner,
			.Revision  = accepted,
			.Catalog   = updatedCatalog,
			.Epoch     = committedEpoch,
		};
	}

	[[nodiscard]] auto diagnostic_is(
		const epoch_transition_output& aOutput, const epoch_reason_code aReason)
		-> bool
	{
		return aOutput.Diagnostics == std::vector{ epoch_diagnostic{ .Code = aReason } };
	}

	[[nodiscard]] auto history_cursor(
		const ready_fixture& aFixture, const std::int64_t aBefore)
		-> epoch_history_cursor
	{
		return {
			.Authority   = aFixture.Authority.Scope,
			.Incarnation = aFixture.Catalog.Incarnation,
			.Revision    = aFixture.Catalog.Revision,
			.Before      = epoch_ordinal{ aBefore },
		};
	}

	[[nodiscard]] auto history_page(
		const ready_fixture&                aFixture,
		std::vector<review_epoch_summary>   aEpochs,
		std::optional<epoch_history_cursor> aContinuation = std::nullopt)
		-> epoch_history_page_observation
	{
		return {
			.Authority    = aFixture.Authority.Scope,
			.Catalog      = aFixture.Catalog,
			.Epochs       = std::move(aEpochs),
			.Continuation = aContinuation,
		};
	}

	[[nodiscard]] auto append_from(const planned_epoch_mutation& aPlan)
		-> const append_epoch_mutation*
	{
		if (!aPlan.Mutation.has_value()) {
			return nullptr;
		}
		return std::get_if<append_epoch_mutation>(&*aPlan.Mutation);
	}

	auto test_preparation_effect_policy() -> void
	{
		const auto owner        = authority();
		const auto ownerCatalog = catalog(4);
		const auto prior        = summary(owner, ownerCatalog, 4, 10U, 20U);

		expect(
			plan_epoch_preparation(
				owner,
				revision(),
				ownerCatalog,
				{ })
				== planned_epoch_preparation{
					.Effect = epoch_preparation_effect::retain_revision,
				},
			"initial observation requires retained-revision preparation");

		auto unchanged = revision(2U, 10U, 20U);
		expect(
			plan_epoch_preparation(
				owner,
				unchanged,
				ownerCatalog,
				epoch_preparation_catalog_observation{ .LatestLive = prior })
				== planned_epoch_preparation{ },
			"unchanged observation requires no Git preparation");

		unchanged.CurrentViewerReview = provider_review(7U, unchanged.Head);
		expect(
			plan_epoch_preparation(
				owner,
				unchanged,
				ownerCatalog,
				epoch_preparation_catalog_observation{ .LatestLive = prior })
				== planned_epoch_preparation{ },
			"association-only observation requires no Git preparation");

		auto changed = revision(2U, 10U, 21U);
		expect(
			plan_epoch_preparation(
				owner,
				changed,
				ownerCatalog,
				epoch_preparation_catalog_observation{ .LatestLive = prior })
				== planned_epoch_preparation{
					.Effect = epoch_preparation_effect::retain_revision,
				},
			"changed head requires retained-revision preparation");

		changed.Source.Observation = prior.Source.Observation;
		expect(
			plan_epoch_preparation(
				owner,
				changed,
				ownerCatalog,
				epoch_preparation_catalog_observation{ .LatestLive = prior })
					.Rejection
				== epoch_reason_code::invalid_observation,
			"changed revision rejects reuse of the latest source observation");

		auto deleted             = prior;
		deleted.BaseAvailability = anchor_availability::cache_deleted;
		deleted.HeadAvailability = anchor_availability::cache_deleted;
		auto rehydrated          = revision(3U, 10U, 20U);
		expect(
			plan_epoch_preparation(
				owner,
				rehydrated,
				ownerCatalog,
				epoch_preparation_catalog_observation{
					.LatestLive           = deleted,
					.RetentionRehydration = retention_rehydration_requirement::required,
				})
				== planned_epoch_preparation{
					.Effect = epoch_preparation_effect::retain_revision,
				},
			"durable cache-delete requirement requests exact rehydration work");
	}

	auto test_binary_oid_types_and_compact_state() -> void
	{
		static_assert(!std::is_same_v<git_commit_oid, git_tree_oid>);
		static_assert(!std::is_same_v<epoch_generation, comparison_generation>);
		static_assert(!std::is_same_v<
					  pull_request_authority_scope_id,
					  repository_cache_scope_id>);
		static_assert(std::is_trivially_copyable_v<epoch_state>);
		static_assert(std::is_trivially_destructible_v<epoch_state>);
		static_assert(sizeof(epoch_state) <= 2'112U);
		static_assert(std::variant_size_v<epoch_event> == 35U);
		static_assert(std::variant_size_v<epoch_host_request> == 10U);

		const auto sha1 = parse_git_commit_oid("0123456789ABCDEF0123456789ABCDEF01234567");
		expect(sha1.has_value(), "40 hexadecimal digits parse as SHA-1");
		expect(
			sha1->algorithm() == git_hash_algorithm::sha1
				&& sha1->byte_count() == 20U,
			"SHA-1 remains algorithm tagged and binary");
		expect(
			canonical_hex(*sha1).view()
				== "0123456789abcdef0123456789abcdef01234567",
			"SHA-1 renders once as complete canonical lowercase text");

		const auto sha256 = parse_git_tree_oid(
			"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef");
		expect(sha256.has_value(), "64 hexadecimal digits parse as SHA-256");
		expect(
			sha256->algorithm() == git_hash_algorithm::sha256
				&& canonical_hex(*sha256).view().size() == 64U,
			"SHA-256 remains algorithm tagged and complete");
		expect(
			!parse_git_commit_oid(std::string(40U, '0')).has_value(),
			"all-zero OIDs are invalid");
		expect(
			!parse_git_commit_oid("abcd").has_value()
				&& !parse_git_commit_oid(std::string(40U, 'z')).has_value(),
			"wrong length and non-hexadecimal OIDs fail closed");

		auto malformedBytes = oid<git_commit_oid>(7U).bytes();
		malformedBytes[31]  = 1U;
		expect(
			!git_commit_oid{ git_hash_algorithm::sha1, malformedBytes }.valid(),
			"SHA-1 rejects nonzero bytes outside its exact 20-byte digest");
		expect(valid(authority()), "complete authority fixture is valid");
		auto incomplete  = authority();
		incomplete.Cache = { };
		expect(!valid(incomplete), "missing cache scope invalidates authority");
	}

	auto test_cause_precedence_and_material_policy() -> void
	{
		const auto owner        = authority();
		const auto ownerCatalog = catalog();
		const auto prior        = summary(owner, ownerCatalog, 4, 10U, 20U, 30U);

		struct cause_case
		{
			const char*                    Name;
			accepted_pull_request_revision Revision;
			head_lineage                   Lineage;
			epoch_base_transition          Base;
			epoch_head_transition          Head;
			epoch_primary_cause            Primary;
		};

		const std::array cases{
			cause_case{
				.Name     = "base update",
				.Revision = revision(2U, 11U, 20U, 30U),
				.Lineage  = head_lineage::not_applicable,
				.Base     = epoch_base_transition::base_oid_changed,
				.Head     = epoch_head_transition::unchanged,
				.Primary  = epoch_primary_cause::base_update,
			},
			cause_case{
				.Name     = "author upload",
				.Revision = revision(2U, 10U, 21U, 30U),
				.Lineage  = head_lineage::descendant,
				.Base     = epoch_base_transition::unchanged,
				.Head     = epoch_head_transition::descendant,
				.Primary  = epoch_primary_cause::author_upload,
			},
			cause_case{
				.Name     = "force push",
				.Revision = revision(2U, 10U, 22U, 30U),
				.Lineage  = head_lineage::non_fast_forward,
				.Base     = epoch_base_transition::unchanged,
				.Head     = epoch_head_transition::non_fast_forward,
				.Primary  = epoch_primary_cause::force_push_rebase,
			},
			cause_case{
				.Name     = "unknown lineage",
				.Revision = revision(2U, 10U, 23U, 30U),
				.Lineage  = head_lineage::unknown,
				.Base     = epoch_base_transition::unchanged,
				.Head     = epoch_head_transition::lineage_unknown,
				.Primary  = epoch_primary_cause::head_change_lineage_unavailable,
			},
			cause_case{
				.Name     = "simultaneous base and head",
				.Revision = revision(2U, 12U, 24U, 30U),
				.Lineage  = head_lineage::descendant,
				.Base     = epoch_base_transition::base_oid_changed,
				.Head     = epoch_head_transition::descendant,
				.Primary  = epoch_primary_cause::author_upload,
			},
			cause_case{
				.Name     = "branch outranks head",
				.Revision = revision(2U, 12U, 25U, 31U),
				.Lineage  = head_lineage::non_fast_forward,
				.Base     = epoch_base_transition::base_branch_changed,
				.Head     = epoch_head_transition::non_fast_forward,
				.Primary  = epoch_primary_cause::base_branch_change,
			},
		};

		for (const auto& item : cases) {
			const auto plan = plan_epoch_mutation(
				owner,
				item.Revision,
				ownerCatalog,
				epoch_preparation_observation{
					.LatestLive      = prior,
					.ProposedOrdinal = epoch_ordinal{ 5 },
					.ObservedAt      = observed_utc_milliseconds{ 500 },
					.HeadLineage     = item.Lineage,
					.Material        = material(61U, 71U),
				});
			const auto* append = append_from(plan);
			expect(
				plan.Rejection == epoch_reason_code::none && append != nullptr,
				item.Name);
			const auto& cause = append->Epoch.Cause;
			expect(
				cause.Origin == epoch_origin::live_observation
					&& cause.Base == item.Base && cause.Head == item.Head
					&& cause.Primary == item.Primary,
				item.Name);
		}

		const auto initial = plan_epoch_mutation(
			owner,
			revision(),
			ownerCatalog,
			epoch_preparation_observation{
				.ProposedOrdinal = epoch_ordinal{ 1 },
				.ObservedAt      = observed_utc_milliseconds{ 0 },
				.Material        = material(),
			});
		const auto* initialAppend = append_from(initial);
		expect(initialAppend != nullptr && initialAppend->Epoch.Cause == epoch_cause{
											   .Origin   = epoch_origin::initial_observation,
											   .Base     = epoch_base_transition::not_applicable,
											   .Head     = epoch_head_transition::not_applicable,
											   .Material = epoch_material_relation::unknown,
											   .Primary  = epoch_primary_cause::initial_observation,
										   },
			   "first live observation has exact initial cause axes");

		const auto historical = plan_epoch_mutation(
			owner,
			revision(
				2U,
				7U,
				8U,
				9U,
				pull_request_revision_origin::explicit_historical_import),
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive      = prior,
				.ProposedOrdinal = epoch_ordinal{ 5 },
				.ObservedAt      = observed_utc_milliseconds{ 500 },
				.Material        = material(),
			});
		const auto* historicalAppend = append_from(historical);
		expect(
			historicalAppend != nullptr
				&& historicalAppend->Epoch.Cause.Origin
					== epoch_origin::explicit_historical_import
				&& historicalAppend->Epoch.Cause.Base
					== epoch_base_transition::not_applicable
				&& historicalAppend->Epoch.Cause.Head
					== epoch_head_transition::not_applicable,
			"explicit historical import never invents live transitions");

		const auto materiallySame = plan_epoch_mutation(
			owner,
			revision(2U, 10U, 21U, 30U),
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive      = prior,
				.ProposedOrdinal = epoch_ordinal{ 5 },
				.ObservedAt      = observed_utc_milliseconds{ 500 },
				.HeadLineage     = head_lineage::descendant,
				.Material        = prior.Material,
			});
		const auto* materiallySameAppend = append_from(materiallySame);
		expect(
			materiallySameAppend != nullptr
				&& materiallySameAppend->Epoch.Cause.Material
					== epoch_material_relation::same,
			"material equality annotates but never suppresses a changed epoch");

		const auto badLineage = plan_epoch_mutation(
			owner,
			revision(2U, 10U, 21U, 30U),
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive      = prior,
				.ProposedOrdinal = epoch_ordinal{ 5 },
				.ObservedAt      = observed_utc_milliseconds{ 500 },
				.HeadLineage     = head_lineage::not_applicable,
			});
		expect(
			badLineage.Rejection == epoch_reason_code::invalid_observation,
			"changed head requires an explicit bounded lineage fact");

		const auto maxCatalog = catalog(maximum_epoch_catalog_revision);
		const auto maxPrior   = summary(owner, maxCatalog, 4, 10U, 20U, 30U);
		const auto exhausted  = plan_epoch_mutation(
			owner,
			revision(2U, 10U, 21U, 30U),
			maxCatalog,
			epoch_preparation_observation{
				.LatestLive      = maxPrior,
				.ProposedOrdinal = epoch_ordinal{ 5 },
				.ObservedAt      = observed_utc_milliseconds{ 500 },
				.HeadLineage     = head_lineage::descendant,
			});
		expect(
			exhausted.Rejection == epoch_reason_code::catalog_revision_exhausted,
			"catalog revision exhaustion fails before a mutation");

		const auto idempotentAtMax = plan_epoch_mutation(
			owner,
			revision(2U, 10U, 20U, 30U),
			maxCatalog,
			epoch_preparation_observation{
				.LatestLive = maxPrior,
				.ObservedAt = observed_utc_milliseconds{ 600 },
			});
		expect(
			!idempotentAtMax.Mutation.has_value()
				&& idempotentAtMax.Rejection == epoch_reason_code::none,
			"catalog revision exhaustion still permits a true idempotent no-op");
	}

	auto test_retention_rehydration_policy() -> void
	{
		const auto owner        = authority();
		const auto ownerCatalog = catalog(8);
		auto       prior        = summary(
			owner,
			ownerCatalog,
			4,
			10U,
			20U,
			30U,
			epoch_origin::live_observation,
			anchor_availability::cache_deleted,
			anchor_availability::cache_deleted,
			material());
		auto unchanged               = revision(prior.Source.Snapshot.value());
		unchanged.Source             = prior.Source;
		unchanged.Source.Observation = opaque<epoch_source_observation_id>(91U);

		const auto rehydration = plan_epoch_mutation(
			owner,
			unchanged,
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive           = prior,
				.ProposedOrdinal      = epoch_ordinal{ 5 },
				.ObservedAt           = observed_utc_milliseconds{ 500 },
				.Material             = material(60U, 70U, 2U),
				.RetentionRehydration = retention_rehydration_requirement::required,
			});
		const auto* append = append_from(rehydration);
		expect(
			rehydration.Rejection == epoch_reason_code::none && append
				&& append->Epoch.Source.Observation
					!= prior.Source.Observation
				&& append->Epoch.Source.Snapshot == prior.Source.Snapshot
				&& append->Epoch.Source.IdentityGeneration
					== prior.Source.IdentityGeneration
				&& append->Epoch.Source.PullRequestGeneration
					== prior.Source.PullRequestGeneration
				&& append->Epoch.Id.Ordinal == epoch_ordinal{ 5 }
				&& append->Epoch.Cause
					== epoch_cause{
						.Origin   = epoch_origin::retention_rehydration,
						.Base     = epoch_base_transition::unchanged,
						.Head     = epoch_head_transition::unchanged,
						.Material = epoch_material_relation::different,
						.Primary  = epoch_primary_cause::retention_rehydration,
					},
			"durable retention loss deliberately creates one fresh-observation "
			"replacement epoch with an explicit cause even after counters reset");
		expect(
			prior.BaseAvailability == anchor_availability::cache_deleted
				&& prior.HeadAvailability == anchor_availability::cache_deleted,
			"planning a replacement leaves the prior cache-delete tombstone "
			"unchanged");

		auto notRequired = epoch_preparation_observation{
			.LatestLive = prior,
			.ObservedAt = observed_utc_milliseconds{ 500 },
			.Material   = material(),
		};
		const auto ordinaryNoop = plan_epoch_mutation(
			owner, unchanged, ownerCatalog, notRequired);
		expect(
			ordinaryNoop.Rejection == epoch_reason_code::none
				&& !ordinaryNoop.Mutation,
			"an unchanged source remains a no-op without the durable requirement");

		auto malformed = epoch_preparation_observation{
			.LatestLive           = prior,
			.ProposedOrdinal      = epoch_ordinal{ 5 },
			.ObservedAt           = observed_utc_milliseconds{ 500 },
			.Material             = material(),
			.RetentionRehydration = retention_rehydration_requirement::required,
		};
		malformed.LatestLive->HeadAvailability = anchor_availability::available;
		expect(
			plan_epoch_mutation(owner, unchanged, ownerCatalog, malformed)
					.Rejection
				== epoch_reason_code::invalid_observation,
			"required rehydration must name an exact cache-deleted comparison "
			"point");

		malformed.LatestLive            = prior;
		auto reusedSource               = unchanged;
		reusedSource.Source.Observation = prior.Source.Observation;
		reusedSource.Source.Snapshot    = provider_snapshot_id{ 999U };
		expect(
			plan_epoch_mutation(
				owner, reusedSource, ownerCatalog, malformed)
					.Rejection
				== epoch_reason_code::invalid_observation,
			"retention rehydration rejects reuse of the prior durable source-"
			"observation identity regardless of process-local counters");

		auto newer         = unchanged;
		newer.Base         = oid<git_commit_oid>(11U);
		const auto changed = plan_epoch_mutation(
			owner,
			newer,
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive           = prior,
				.ProposedOrdinal      = epoch_ordinal{ 5 },
				.ObservedAt           = observed_utc_milliseconds{ 500 },
				.Material             = material(61U, 70U),
				.RetentionRehydration = retention_rehydration_requirement::required,
			});
		const auto* changedAppend = append_from(changed);
		expect(
			changed.Rejection == epoch_reason_code::none && changedAppend
				&& changedAppend->Epoch.Cause.Origin
					== epoch_origin::live_observation
				&& changedAppend->Epoch.Cause.Primary
					== epoch_primary_cause::base_update,
			"a genuine newer live revision replaces missing retention using its "
			"ordinary semantic cause");

		auto duplicateObservation               = newer;
		duplicateObservation.Source.Observation = prior.Source.Observation;
		expect(
			plan_epoch_mutation(
				owner,
				duplicateObservation,
				ownerCatalog,
				epoch_preparation_observation{
					.LatestLive           = prior,
					.ProposedOrdinal      = epoch_ordinal{ 5 },
					.ObservedAt           = observed_utc_milliseconds{ 500 },
					.HeadLineage          = head_lineage::not_applicable,
					.Material             = material(61U, 70U),
					.RetentionRehydration = retention_rehydration_requirement::required,
				})
					.Rejection
				== epoch_reason_code::invalid_observation,
			"one source-observation identity cannot publish a changed semantic "
			"revision");

		malformed.LatestLive.reset();
		expect(
			plan_epoch_mutation(owner, unchanged, ownerCatalog, malformed)
					.Rejection
				== epoch_reason_code::invalid_observation,
			"a never-retained pull request cannot claim rehydration");
	}

	auto test_association_only_policy() -> void
	{
		const auto owner             = authority();
		const auto ownerCatalog      = catalog();
		const auto prior             = summary(owner, ownerCatalog, 4, 10U, 20U, 30U);
		const auto oldReview         = associated_review(1U, prior.Head);
		const auto newReview         = associated_review(2U, prior.Head);
		auto       accepted          = revision(2U, 10U, 20U, 30U);
		accepted.CurrentViewerReview = provider_review(2U, prior.Head);

		const auto changed = plan_epoch_mutation(
			owner,
			accepted,
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive                  = prior,
				.CatalogCurrentViewerReview  = oldReview,
				.ProposedCurrentViewerReview = newReview,
				.ObservedAt                  = observed_utc_milliseconds{ 500 },
			});
		const auto* association = changed.Mutation.has_value()
			? std::get_if<associate_review_mutation>(&*changed.Mutation)
			: nullptr;
		expect(
			association != nullptr && association->Review == newReview,
			"changed review on unchanged revision is association-only");

		const auto identical = plan_epoch_mutation(
			owner,
			accepted,
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive                  = prior,
				.CatalogCurrentViewerReview  = newReview,
				.ProposedCurrentViewerReview = newReview,
				.ObservedAt                  = observed_utc_milliseconds{ 500 },
			});
		expect(
			!identical.Mutation.has_value()
				&& identical.Rejection == epoch_reason_code::none,
			"identical review association is idempotent");

		const auto absentProviderFact = plan_epoch_mutation(
			owner,
			revision(3U, 10U, 20U, 30U),
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive                 = prior,
				.CatalogCurrentViewerReview = newReview,
				.ObservedAt                 = observed_utc_milliseconds{ 600 },
			});
		expect(
			!absentProviderFact.Mutation.has_value()
				&& absentProviderFact.Rejection == epoch_reason_code::none,
			"absent provider candidate does not erase a retained association");

		auto mismatched               = newReview;
		mismatched.Review             = github_review_id{ 999U };
		const auto mismatchedProposal = plan_epoch_mutation(
			owner,
			accepted,
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive                  = prior,
				.CatalogCurrentViewerReview  = oldReview,
				.ProposedCurrentViewerReview = mismatched,
				.ObservedAt                  = observed_utc_milliseconds{ 700 },
			});
		expect(
			mismatchedProposal.Rejection == epoch_reason_code::invalid_observation,
			"catalog association proposal must match exact provider review facts");
	}

	auto test_alias_resolution_and_exact_pairs() -> void
	{
		const auto owner        = authority();
		const auto ownerCatalog = catalog(8);
		const auto previous     = summary(owner, ownerCatalog, 7, 10U, 20U);
		const auto latest       = summary(owner, ownerCatalog, 8, 11U, 30U);
		const auto newestReview = associated_review(8U, latest.Head);
		const auto reviewCommit = review_anchor(owner, ownerCatalog, newestReview);

		const auto candidates = comparison_candidates{
			.Catalog              = ownerCatalog,
			.LatestLive           = latest,
			.PreviousDistinctHead = previous,
			.CurrentViewerReview  = reviewCommit,
		};

		const auto whole = resolve_comparison(
			owner, named_selection(named_comparison::whole_pr), candidates);
		expect(
			whole.Pair.has_value()
				&& whole.Pair->Left.Identity.Kind
					== comparison_anchor_kind::epoch_base
				&& whole.Pair->Left.Commit == latest.Base
				&& whole.Pair->Right.Commit == latest.Head,
			"Whole PR resolves latest live base to latest live head");

		const auto previousUpload = resolve_comparison(
			owner,
			named_selection(named_comparison::previous_upload_to_latest),
			candidates);
		expect(
			previousUpload.Pair.has_value()
				&& previousUpload.Pair->Left.Commit == previous.Head
				&& previousUpload.Pair->Right.Commit == latest.Head,
			"Previous upload uses the supplied preceding distinct live head");

		const auto myReview = resolve_comparison(
			owner,
			named_selection(named_comparison::my_last_reviewed_to_latest),
			candidates);
		expect(
			myReview.Pair.has_value()
				&& myReview.Pair->Left.Identity.Kind
					== comparison_anchor_kind::review_commit
				&& myReview.Pair->Right.Identity.Kind
					== comparison_anchor_kind::epoch_head
				&& myReview.Pair->Left.Commit == myReview.Pair->Right.Commit
				&& myReview.Pair->Left.Identity != myReview.Pair->Right.Identity,
			"review identity stays distinct from an equal epoch-head OID");

		auto wrongReviewKind                = candidates;
		wrongReviewKind.CurrentViewerReview = anchor(latest, comparison_anchor_kind::epoch_head);
		expect(
			resolve_comparison(
				owner,
				named_selection(named_comparison::my_last_reviewed_to_latest),
				wrongReviewKind)
					.Reason
				== epoch_reason_code::invalid_observation,
			"My last reviewed accepts only an exact review-commit identity");

		auto unavailableCandidates                                   = candidates;
		unavailableCandidates.PreviousDistinctHead->HeadAvailability = anchor_availability::pruned;
		const auto unavailable                                       = resolve_comparison(
			owner,
			named_selection(named_comparison::previous_upload_to_latest),
			unavailableCandidates);
		expect(
			unavailable.Pair.has_value()
				&& unavailable.Pair->Left.Identity.Epoch == epoch_ordinal{ 7 }
				&& unavailable.Reason == epoch_reason_code::unavailable,
			"alias chooses semantic candidate before availability with no "
			"fallback");

		auto historicalLatest                     = candidates;
		historicalLatest.LatestLive->Cause.Origin = epoch_origin::explicit_historical_import;
		expect(
			!resolve_comparison(
				 owner,
				 named_selection(named_comparison::whole_pr),
				 historicalLatest)
				 .Pair.has_value(),
			"historical import never becomes Latest");
		auto historicalPrevious                               = candidates;
		historicalPrevious.PreviousDistinctHead->Cause.Origin = epoch_origin::explicit_historical_import;
		expect(
			!resolve_comparison(
				 owner,
				 named_selection(named_comparison::previous_upload_to_latest),
				 historicalPrevious)
				 .Pair.has_value(),
			"historical import never becomes Previous upload");

		const auto previousHead = anchor(previous, comparison_anchor_kind::epoch_head);
		const auto latestHead   = anchor(latest, comparison_anchor_kind::epoch_head);
		const auto reverse      = resolve_comparison(
			owner,
			explicit_selection(latestHead.Identity, previousHead.Identity),
			comparison_candidates{
				.Catalog       = ownerCatalog,
				.ExplicitLeft  = latestHead,
				.ExplicitRight = previousHead,
			});
		expect(
			reverse.Pair.has_value() && reverse.Pair->Left.Commit == latest.Head
				&& reverse.Pair->Right.Commit == previous.Head,
			"explicit reverse pair remains exact and legal");

		const auto identical = resolve_comparison(
			owner,
			explicit_selection(latestHead.Identity, latestHead.Identity),
			comparison_candidates{
				.Catalog       = ownerCatalog,
				.ExplicitLeft  = latestHead,
				.ExplicitRight = latestHead,
			});
		expect(
			identical.Pair.has_value()
				&& identical.Pair->Left == identical.Pair->Right
				&& available_for_diff(*identical.Pair),
			"explicit identical pair remains representable as an empty comparison");

		auto foreign               = latestHead;
		foreign.Identity.Authority = authority(9U).Scope;
		expect(
			resolve_comparison(
				owner,
				explicit_selection(foreign.Identity, latestHead.Identity),
				comparison_candidates{
					.Catalog       = ownerCatalog,
					.ExplicitLeft  = foreign,
					.ExplicitRight = latestHead,
				})
					.Reason
				== epoch_reason_code::invalid_selection,
			"cross-authority explicit pair fails closed");
	}

	auto test_prepare_commit_visibility_and_linearization() -> void
	{
		auto       state          = initial_state();
		const auto owner          = authority();
		const auto accepted       = revision();
		const auto acceptedOutput = transition(
			state,
			pull_request_revision_accepted_event{
				.ApplicationSession = state.ApplicationSession,
				.Authority          = owner,
				.Revision           = accepted,
			});
		expect(
			state.RequestedSelection == named_selection(named_comparison::whole_pr)
				&& request_from<invalidate_epoch_authority_request>(acceptedOutput)
					!= nullptr
				&& acceptedOutput.Catalog.has_value()
				&& !acceptedOutput.Catalog->Catalog.has_value()
				&& acceptedOutput.Comparison.has_value()
				&& !acceptedOutput.Comparison->Pair.has_value(),
			"new authority resets to semantic Whole PR and invalidates prior "
			"scope");
		const auto* load = request_from<load_epoch_catalog_request>(acceptedOutput);
		expect(load != nullptr, "new authority emits load request");

		auto wrongLoad              = load->Context;
		wrongLoad.Authority.Account = github_account_id{ 999U };
		const auto beforeMismatch   = state;
		const auto mismatch         = transition(
			state,
			epoch_catalog_loaded_event{
				.Context = wrongLoad,
				.Catalog = catalog(),
			});
		expect(
			diagnostic_is(mismatch, epoch_reason_code::completion_identity_mismatch)
				&& state == beforeMismatch,
			"same request/generation with wrong authority is mismatch and inert");

		auto staleLoad    = load->Context;
		staleLoad.Request = epoch_request_id{ 999U };
		const auto stale  = transition(
			state,
			epoch_catalog_loaded_event{
				.Context = staleLoad,
				.Catalog = catalog(),
			});
		expect(
			diagnostic_is(stale, epoch_reason_code::stale_completion)
				&& state == beforeMismatch,
			"different request is stale and cannot clear current work");

		const auto loaded = transition(
			state,
			epoch_catalog_loaded_event{
				.Context = load->Context,
				.Catalog = catalog(),
			});
		const auto* prepare = request_from<prepare_epoch_revision_request>(loaded);
		expect(prepare != nullptr, "matching load enters preparation");
		expect(
			diagnostic_is(
				transition(
					state,
					epoch_catalog_loaded_event{
						.Context = load->Context,
						.Catalog = catalog(),
					}),
				epoch_reason_code::stale_completion),
			"duplicate load terminal is stale");

		const auto prepared = transition(
			state,
			epoch_revision_prepared_event{
				.Context     = prepare->Context,
				.Operation   = opaque<epoch_operation_id>(71U),
				.Observation = epoch_preparation_observation{
					.ProposedOrdinal = epoch_ordinal{ 1 },
					.ObservedAt      = observed_utc_milliseconds{ 100 },
					.Material        = material(),
				},
			});
		const auto* commit = request_from<commit_epoch_catalog_request>(prepared);
		expect(
			commit != nullptr && !prepared.AcceptedEpoch.has_value()
				&& state.Catalog->Revision == epoch_catalog_revision{ 0 }
				&& !state.AcceptedComparison.has_value(),
			"prepared candidate is invisible until exact durable commit terminal");

		const auto cancellation = transition(state, cancel_epoch_refresh_intent{ });
		expect(
			request_from<cancel_epoch_work_request>(cancellation) != nullptr
				&& state.PendingIngestion->CancellationRequested
				&& cancellation.Commands.has_value()
				&& !cancellation.Commands->Commands[2].Enabled
				&& has_changed(
					cancellation.Changed, epoch_changed_domain::commands),
			"advisory cancellation disables its command while commit is pending");

		const auto committed = transition(
			state,
			epoch_catalog_committed_event{
				.Context     = commit->Context,
				.Mutation    = commit->Mutation,
				.NewRevision = epoch_catalog_revision{ 1 },
			});
		expect(
			committed.AcceptedEpoch.has_value()
				&& state.Catalog->Revision == epoch_catalog_revision{ 1 }
				&& request_from<resolve_comparison_request>(committed) != nullptr,
			"matching success wins a cancellation race at catalog linearization");

		const auto afterCommit = state;
		const auto duplicate   = transition(
			state,
			epoch_catalog_committed_event{
				.Context     = commit->Context,
				.Mutation    = commit->Mutation,
				.NewRevision = epoch_catalog_revision{ 1 },
			});
		expect(
			diagnostic_is(duplicate, epoch_reason_code::stale_completion)
				&& state == afterCommit,
			"duplicate committed terminal is stale and inert");
	}

	auto test_commit_payload_binding_and_cancellation_point() -> void
	{
		auto beginCommit = [] {
			auto       state          = initial_state();
			const auto owner          = authority();
			const auto accepted       = revision();
			const auto acceptedOutput = transition(
				state,
				pull_request_revision_accepted_event{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = owner,
					.Revision           = accepted,
				});
			const auto load   = *request_from<load_epoch_catalog_request>(acceptedOutput);
			const auto loaded = transition(
				state,
				epoch_catalog_loaded_event{
					.Context = load.Context,
					.Catalog = catalog(),
				});
			const auto prepare  = *request_from<prepare_epoch_revision_request>(loaded);
			const auto prepared = transition(
				state,
				epoch_revision_prepared_event{
					.Context     = prepare.Context,
					.Operation   = opaque<epoch_operation_id>(81U),
					.Observation = epoch_preparation_observation{
						.ProposedOrdinal = epoch_ordinal{ 1 },
						.ObservedAt      = observed_utc_milliseconds{ 100 },
					},
				});
			return std::pair{
				state,
				*request_from<commit_epoch_catalog_request>(prepared),
			};
		};

		{
			auto [state, commit]   = beginCommit();
			auto wrongContext      = commit.Context;
			wrongContext.Operation = opaque<epoch_operation_id>(82U);
			const auto before      = state;
			const auto result      = transition(
				state,
				epoch_catalog_committed_event{
					.Context     = wrongContext,
					.Mutation    = commit.Mutation,
					.NewRevision = epoch_catalog_revision{ 1 },
				});
			expect(
				diagnostic_is(
					result, epoch_reason_code::completion_identity_mismatch)
					&& state == before,
				"wrong durable operation ID cannot commit another preparation");
		}
		{
			auto [state, commit]                       = beginCommit();
			const epoch_catalog_mutation wrongMutation = associate_review_mutation{
				.Review = associated_review(9U, oid<git_commit_oid>(20U)),
			};
			const auto before = state;
			const auto result = transition(
				state,
				epoch_catalog_committed_event{
					.Context     = commit.Context,
					.Mutation    = wrongMutation,
					.NewRevision = epoch_catalog_revision{ 1 },
				});
			expect(
				diagnostic_is(
					result, epoch_reason_code::completion_identity_mismatch)
					&& state == before,
				"same request and operation with wrong mutation is mismatch");
		}
		{
			auto [state, commit] = beginCommit();
			const auto before    = state;
			const auto result    = transition(
				state,
				epoch_catalog_commit_cancelled_event{
					.Context  = commit.Context,
					.Mutation = commit.Mutation,
					.Progress = epoch_commit_progress::ref_commit_in_progress,
				});
			expect(
				diagnostic_is(
					result,
					epoch_reason_code::cancellation_after_point_of_no_return)
					&& state == before,
				"cancellation after ref-commit phase is rejected pending recovery");
		}
		{
			auto [state, commit] = beginCommit();
			const auto before    = state;
			const auto result    = transition(
				state,
				epoch_catalog_commit_cancelled_event{
					.Context  = commit.Context,
					.Mutation = commit.Mutation,
					.Progress = static_cast<epoch_commit_progress>(255U),
				});
			expect(
				diagnostic_is(result, epoch_reason_code::invalid_observation)
					&& state == before,
				"malformed cancellation progress retains pending recovery work");
		}
		{
			auto [state, commit] = beginCommit();
			const auto result    = transition(
				state,
				epoch_catalog_commit_cancelled_event{
					.Context  = commit.Context,
					.Mutation = commit.Mutation,
					.Progress = epoch_commit_progress::before_ref_commit,
				});
			expect(
				diagnostic_is(result, epoch_reason_code::cancelled)
					&& !state.PendingIngestion.has_value()
					&& state.Catalog->Revision == epoch_catalog_revision{ 0 }
					&& !result.AcceptedEpoch.has_value(),
				"pre-ref-commit cancellation publishes no epoch");
		}
		{
			auto [state, commit] = beginCommit();
			const auto result    = transition(
				state,
				epoch_catalog_committed_event{
					.Context     = commit.Context,
					.Mutation    = commit.Mutation,
					.NewRevision = epoch_catalog_revision{ 2 },
				});
			expect(
				diagnostic_is(
					result, epoch_reason_code::completion_identity_mismatch)
					&& !state.PendingIngestion.has_value()
					&& state.Catalog->Revision == epoch_catalog_revision{ 0 }
					&& state.Status == epoch_status::failed,
				"current success with a wrong next revision terminalizes as "
				"failure");
		}
		{
			auto [state, commit]     = beginCommit();
			const auto oldGeneration = state.EpochGeneration;
			const auto result        = transition(
				state,
				epoch_catalog_commit_failed_event{
					.Context  = commit.Context,
					.Mutation = commit.Mutation,
					.Failure  = epoch_failure::catalog_conflict,
				});
			const auto* reload = request_from<load_epoch_catalog_request>(result);
			expect(
				diagnostic_is(result, epoch_reason_code::catalog_conflict)
					&& reload != nullptr
					&& reload->Context.Generation != oldGeneration
					&& !state.Catalog.has_value()
					&& !state.AcceptedComparison.has_value()
					&& state.AcceptedComparisonRevision
						== comparison_revision{ }
					&& state.RequestedSelection
						== named_selection(named_comparison::whole_pr)
					&& result.Catalog.has_value()
					&& !result.Catalog->Catalog.has_value()
					&& result.Selection.has_value()
					&& result.Comparison.has_value()
					&& !result.Comparison->Pair.has_value()
					&& has_changed(
						result.Changed, epoch_changed_domain::catalog)
					&& has_changed(
						result.Changed, epoch_changed_domain::selection)
					&& has_changed(
						result.Changed, epoch_changed_domain::comparison)
					&& state.PendingIngestion.has_value()
					&& state.PendingIngestion->Phase
						== epoch_ingestion_phase::loading_catalog
					&& state.Status == epoch_status::loading_catalog,
				"catalog conflict clears every catalog-bound projection and "
				"loads a fresh bounded catalog");

			auto newCatalog        = catalog(7);
			newCatalog.Incarnation = opaque<epoch_catalog_incarnation_id>(42U);
			const auto loaded      = transition(
				state,
				epoch_catalog_loaded_event{
					.Context = reload->Context,
					.Catalog = newCatalog,
				});
			expect(
				request_from<prepare_epoch_revision_request>(loaded) != nullptr
					&& state.Catalog == newCatalog
					&& !state.AcceptedComparison.has_value(),
				"a replacement catalog incarnation cannot inherit an old pair");

			const auto replacementEpoch = summary(authority(), newCatalog, 1, 10U, 20U);
			const auto replacementHead  = anchor(replacementEpoch, comparison_anchor_kind::epoch_head);
			const auto beforeSelection  = state;
			const auto oneSided         = transition(
				state,
				select_comparison_anchor_intent{
					.Side                    = comparison_side::right,
					.Anchor                  = replacementHead.Identity,
					.ExpectedCatalogRevision = newCatalog.Revision,
				});
			expect(
				diagnostic_is(
					oneSided, epoch_reason_code::no_resolved_comparison)
					&& state == beforeSelection,
				"one-sided selection cannot combine a new catalog anchor with "
				"an old-incarnation pair");
		}
	}

	auto test_catalog_conflict_reloads_catalog() -> void
	{
		{
			auto       fixture       = make_ready_fixture();
			const auto oldGeneration = fixture.State.EpochGeneration;
			const auto refresh       = transition(fixture.State, refresh_epochs_intent{ });
			const auto prepare       = *request_from<prepare_epoch_revision_request>(refresh);
			static_cast<void>(
				transition(fixture.State, cancel_epoch_refresh_intent{ }));

			auto wrongContext               = prepare.Context;
			wrongContext.CatalogIncarnation = opaque<epoch_catalog_incarnation_id>(99U);
			const auto beforeMismatch       = fixture.State;
			const auto mismatch             = transition(
				fixture.State,
				epoch_revision_prepare_failed_event{
					.Context = wrongContext,
					.Failure = epoch_failure::catalog_conflict,
				});
			expect(
				diagnostic_is(
					mismatch, epoch_reason_code::completion_identity_mismatch)
					&& fixture.State == beforeMismatch,
				"mismatched preparation conflict cannot clear catalog state");

			const auto conflict = transition(
				fixture.State,
				epoch_revision_prepare_failed_event{
					.Context = prepare.Context,
					.Failure = epoch_failure::catalog_conflict,
				});
			const auto* reload = request_from<load_epoch_catalog_request>(conflict);
			expect(
				diagnostic_is(conflict, epoch_reason_code::catalog_conflict)
					&& reload != nullptr
					&& reload->Context.Generation != oldGeneration
					&& reload->Context.Authority == fixture.Authority
					&& reload->Context.Revision == fixture.Revision
					&& !fixture.State.Catalog.has_value()
					&& !fixture.State.AcceptedComparison.has_value()
					&& fixture.State.AcceptedComparisonRevision
						== comparison_revision{ }
					&& fixture.State.RequestedSelection
						== named_selection(named_comparison::whole_pr)
					&& fixture.State.PendingIngestion.has_value()
					&& fixture.State.PendingIngestion->Phase
						== epoch_ingestion_phase::loading_catalog
					&& fixture.State.Status == epoch_status::loading_catalog
					&& conflict.Catalog.has_value()
					&& !conflict.Catalog->Catalog.has_value()
					&& conflict.Selection.has_value()
					&& conflict.Comparison.has_value()
					&& !conflict.Comparison->Pair.has_value(),
				"preparation catalog conflict supersedes advisory cancellation "
				"and reloads from fail-closed catalog state");

			const auto beforeStale   = fixture.State;
			const auto stalePrepared = transition(
				fixture.State,
				epoch_revision_prepared_event{
					.Context     = prepare.Context,
					.Operation   = opaque<epoch_operation_id>(72U),
					.Observation = epoch_preparation_observation{
						.ProposedOrdinal = epoch_ordinal{ 1 },
						.ObservedAt      = observed_utc_milliseconds{ 101 },
						.Material        = material(),
					},
				});
			const auto staleFailed = transition(
				fixture.State,
				epoch_revision_prepare_failed_event{
					.Context = prepare.Context,
					.Failure = epoch_failure::unavailable,
				});
			const auto staleCancelled = transition(
				fixture.State,
				epoch_revision_prepare_cancelled_event{
					.Context = prepare.Context,
				});
			expect(
				diagnostic_is(
					stalePrepared, epoch_reason_code::stale_completion)
					&& diagnostic_is(
						staleFailed, epoch_reason_code::stale_completion)
					&& diagnostic_is(
						staleCancelled, epoch_reason_code::stale_completion)
					&& fixture.State == beforeStale,
				"pre-conflict preparation terminals cannot disturb the fresh "
				"catalog load");
		}

		{
			auto       fixture       = make_ready_fixture();
			const auto oldGeneration = fixture.State.EpochGeneration;
			const auto selected      = transition(
				fixture.State,
				select_named_comparison_intent{
					.Alias                   = named_comparison::previous_upload_to_latest,
					.ExpectedCatalogRevision = fixture.Catalog.Revision,
				});
			const auto resolve = *request_from<resolve_comparison_request>(selected);

			const auto conflict = transition(
				fixture.State,
				comparison_resolve_failed_event{
					.Context = resolve.Context,
					.Failure = epoch_failure::catalog_conflict,
				});
			const auto* reload = request_from<load_epoch_catalog_request>(conflict);
			expect(
				diagnostic_is(conflict, epoch_reason_code::catalog_conflict)
					&& conflict.Requests.size() == 1U && reload != nullptr
					&& reload->Context.Generation != oldGeneration
					&& !fixture.State.Catalog.has_value()
					&& !fixture.State.AcceptedComparison.has_value()
					&& fixture.State.AcceptedComparisonRevision
						== comparison_revision{ }
					&& fixture.State.RequestedSelection
						== named_selection(named_comparison::whole_pr)
					&& !fixture.State.PendingComparison.has_value()
					&& fixture.State.PendingIngestion.has_value()
					&& fixture.State.PendingIngestion->Phase
						== epoch_ingestion_phase::loading_catalog
					&& fixture.State.Status == epoch_status::loading_catalog,
				"comparison catalog conflict retires its terminal lane and "
				"starts the same bounded catalog reload path");

			const auto beforeStale = fixture.State;
			const auto stale       = transition(
				fixture.State,
				comparison_resolved_event{
					.Context    = resolve.Context,
					.Candidates = comparison_candidates{
						.Catalog    = fixture.Catalog,
						.LatestLive = fixture.Epoch,
					},
				});
			expect(
				diagnostic_is(stale, epoch_reason_code::stale_completion)
					&& fixture.State == beforeStale,
				"pre-conflict comparison results cannot overwrite the fresh "
				"catalog load");
		}

		{
			auto fixture              = make_ready_fixture();
			fixture.State.NextRequest = epoch_request_id{
				std::numeric_limits<std::uint64_t>::max()
			};
			const auto selected = transition(
				fixture.State,
				select_named_comparison_intent{
					.Alias                   = named_comparison::previous_upload_to_latest,
					.ExpectedCatalogRevision = fixture.Catalog.Revision,
				});
			const auto resolve = *request_from<resolve_comparison_request>(selected);
			expect(
				fixture.State.RequestCounterExhausted,
				"comparison can consume the final bounded request identity");

			const auto conflict = transition(
				fixture.State,
				comparison_resolve_failed_event{
					.Context = resolve.Context,
					.Failure = epoch_failure::catalog_conflict,
				});
			expect(
				conflict.Diagnostics
						== std::vector{
							epoch_diagnostic{
								.Code = epoch_reason_code::catalog_conflict,
							},
							epoch_diagnostic{
								.Code = epoch_reason_code::request_counter_exhausted,
							},
						}
					&& conflict.Requests.empty() && !fixture.State.Catalog.has_value() && !fixture.State.PendingComparison.has_value() && !fixture.State.PendingIngestion.has_value() && fixture.State.Status == epoch_status::failed,
				"catalog conflict fails closed when the bounded reload request "
				"space is exhausted");
		}
	}

	auto test_current_malformed_terminals_close_work() -> void
	{
		{
			auto       state    = initial_state();
			const auto accepted = transition(
				state,
				pull_request_revision_accepted_event{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = authority(),
					.Revision           = revision(),
				});
			const auto load      = *request_from<load_epoch_catalog_request>(accepted);
			const auto malformed = transition(
				state,
				epoch_catalog_loaded_event{
					.Context = load.Context,
					.Catalog = { },
				});
			expect(
				diagnostic_is(malformed, epoch_reason_code::invalid_catalog)
					&& !state.PendingIngestion.has_value()
					&& state.Status == epoch_status::failed,
				"current malformed catalog load terminal closes its pending work");
		}

		{
			auto       state    = initial_state();
			const auto accepted = transition(
				state,
				pull_request_revision_accepted_event{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = authority(),
					.Revision           = revision(),
				});
			const auto load   = *request_from<load_epoch_catalog_request>(accepted);
			const auto loaded = transition(
				state,
				epoch_catalog_loaded_event{
					.Context = load.Context,
					.Catalog = catalog(),
				});
			const auto prepare   = *request_from<prepare_epoch_revision_request>(loaded);
			const auto malformed = transition(
				state,
				epoch_revision_prepared_event{
					.Context     = prepare.Context,
					.Operation   = { },
					.Observation = epoch_preparation_observation{
						.ProposedOrdinal = epoch_ordinal{ 1 },
						.ObservedAt      = observed_utc_milliseconds{ 100 },
					},
				});
			expect(
				diagnostic_is(malformed, epoch_reason_code::invalid_identity)
					&& !state.PendingIngestion.has_value()
					&& state.Status == epoch_status::failed,
				"current prepared terminal with no operation identity closes its "
				"pending work");
		}

		{
			auto       state    = initial_state();
			const auto accepted = transition(
				state,
				pull_request_revision_accepted_event{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = authority(),
					.Revision           = revision(),
				});
			const auto load   = *request_from<load_epoch_catalog_request>(accepted);
			const auto loaded = transition(
				state,
				epoch_catalog_loaded_event{
					.Context = load.Context,
					.Catalog = catalog(),
				});
			const auto prepare   = *request_from<prepare_epoch_revision_request>(loaded);
			const auto malformed = transition(
				state,
				epoch_revision_prepared_event{
					.Context     = prepare.Context,
					.Operation   = opaque<epoch_operation_id>(83U),
					.Observation = epoch_preparation_observation{
						.ObservedAt = observed_utc_milliseconds{ 100 },
					},
				});
			expect(
				diagnostic_is(malformed, epoch_reason_code::invalid_observation)
					&& request_from<discard_epoch_preparation_request>(malformed)
						!= nullptr
					&& !state.PendingIngestion.has_value(),
				"current rejected preparation is exact-discarded before failure");
		}

		{
			auto       fixture  = make_ready_fixture();
			const auto selected = transition(
				fixture.State,
				select_named_comparison_intent{
					.Alias                   = named_comparison::previous_upload_to_latest,
					.ExpectedCatalogRevision = fixture.Catalog.Revision,
				});
			const auto resolve            = *request_from<resolve_comparison_request>(selected);
			auto       mismatchedCatalog  = fixture.Catalog;
			mismatchedCatalog.Incarnation = opaque<epoch_catalog_incarnation_id>(42U);
			const auto malformed          = transition(
				fixture.State,
				comparison_resolved_event{
					.Context    = resolve.Context,
					.Candidates = comparison_candidates{
						.Catalog = mismatchedCatalog,
					},
				});
			expect(
				diagnostic_is(
					malformed, epoch_reason_code::completion_identity_mismatch)
					&& !fixture.State.PendingComparison.has_value()
					&& fixture.State.Status == epoch_status::failed,
				"current comparison terminal with a mismatched catalog closes its "
				"pending work");
		}
	}

	auto test_noop_discard_and_exact_invalidation() -> void
	{
		{
			auto       switched    = make_ready_fixture();
			const auto replacement = authority(9U);
			const auto result      = transition(
				switched.State,
				pull_request_revision_accepted_event{
					.ApplicationSession = switched.State.ApplicationSession,
					.Authority          = replacement,
					.Revision           = revision(2U, 11U, 21U, 31U),
				});
			expect(
				switched.State.Authority == replacement
					&& !switched.State.Catalog.has_value()
					&& !switched.State.AcceptedComparison.has_value()
					&& result.Catalog.has_value()
					&& !result.Catalog->Catalog.has_value()
					&& result.Selection.has_value()
					&& result.Selection->Selection
						== named_selection(named_comparison::whole_pr)
					&& result.Comparison.has_value()
					&& !result.Comparison->Pair.has_value()
					&& has_changed(
						result.Changed, epoch_changed_domain::catalog)
					&& has_changed(
						result.Changed, epoch_changed_domain::selection)
					&& has_changed(
						result.Changed, epoch_changed_domain::comparison),
				"authority switch publishes every catalog-bound projection "
				"clear");
		}

		auto       fixture           = make_ready_fixture();
		const auto oldRevision       = fixture.State.AcceptedRevision;
		const auto oldGeneration     = fixture.State.EpochGeneration;
		const auto refreshedRevision = revision(2U, 10U, 20U, 30U);
		const auto refresh           = transition(
			fixture.State,
			pull_request_revision_accepted_event{
				.ApplicationSession = fixture.State.ApplicationSession,
				.Authority          = fixture.Authority,
				.Revision           = refreshedRevision,
			});
		const auto* prepare = request_from<prepare_epoch_revision_request>(refresh);
		expect(prepare != nullptr, "same-authority refresh prepares directly");

		const auto beforeDelayed = fixture.State;
		const auto delayed       = transition(
			fixture.State,
			pull_request_revision_invalidated_event{
				.ApplicationSession = fixture.State.ApplicationSession,
				.Authority          = fixture.Authority,
				.Revision           = *oldRevision,
				.Generation         = oldGeneration,
			});
		expect(
			diagnostic_is(delayed, epoch_reason_code::stale_completion)
				&& fixture.State == beforeDelayed,
			"delayed same-authority invalidation cannot clear newer revision");

		const auto noChange = transition(
			fixture.State,
			epoch_revision_prepared_event{
				.Context     = prepare->Context,
				.Operation   = opaque<epoch_operation_id>(91U),
				.Observation = epoch_preparation_observation{
					.LatestLive = fixture.Epoch,
					.ObservedAt = observed_utc_milliseconds{ 200 },
					.Material   = fixture.Epoch.Material,
				},
			});
		expect(
			request_from<discard_epoch_preparation_request>(noChange) != nullptr
				&& request_from<commit_epoch_catalog_request>(noChange) == nullptr
				&& request_from<resolve_comparison_request>(noChange) != nullptr
				&& fixture.State.Catalog->Revision == fixture.Catalog.Revision,
			"unchanged prepared operation is exact-discarded without commit");

		const auto exactRevision   = *fixture.State.AcceptedRevision;
		const auto exactGeneration = fixture.State.EpochGeneration;
		const auto invalidated     = transition(
			fixture.State,
			pull_request_revision_invalidated_event{
				.ApplicationSession = fixture.State.ApplicationSession,
				.Authority          = fixture.Authority,
				.Revision           = exactRevision,
				.Generation         = exactGeneration,
			});
		expect(
			invalidated.Authority.has_value()
				&& !invalidated.Authority->Authority.has_value()
				&& invalidated.Catalog.has_value()
				&& !invalidated.Catalog->Catalog.has_value()
				&& invalidated.Selection.has_value()
				&& invalidated.Selection->Selection
					== named_selection(named_comparison::whole_pr)
				&& invalidated.Comparison.has_value()
				&& !invalidated.Comparison->Pair.has_value()
				&& has_changed(
					invalidated.Changed, epoch_changed_domain::catalog)
				&& has_changed(
					invalidated.Changed, epoch_changed_domain::selection)
				&& has_changed(
					invalidated.Changed, epoch_changed_domain::comparison)
				&& !fixture.State.Authority.has_value()
				&& !fixture.State.AcceptedComparison.has_value(),
			"exact invalidation publishes every authority-sensitive clear");
	}

	auto test_association_only_commit_transition() -> void
	{
		auto       fixture            = make_ready_fixture();
		const auto provider           = provider_review(3U, fixture.Epoch.Head);
		const auto association        = associated_review(3U, fixture.Epoch.Head);
		auto       refreshed          = revision(2U, 10U, 20U, 30U);
		refreshed.CurrentViewerReview = provider;

		const auto refresh = transition(
			fixture.State,
			pull_request_revision_accepted_event{
				.ApplicationSession = fixture.State.ApplicationSession,
				.Authority          = fixture.Authority,
				.Revision           = refreshed,
			});
		const auto* prepare  = request_from<prepare_epoch_revision_request>(refresh);
		const auto  prepared = transition(
			fixture.State,
			epoch_revision_prepared_event{
				.Context     = prepare->Context,
				.Operation   = opaque<epoch_operation_id>(92U),
				.Observation = epoch_preparation_observation{
					.LatestLive                  = fixture.Epoch,
					.ProposedCurrentViewerReview = association,
					.ObservedAt                  = observed_utc_milliseconds{ 200 },
					.Material                    = fixture.Epoch.Material,
				},
			});
		const auto* commit = request_from<commit_epoch_catalog_request>(prepared);
		expect(
			commit != nullptr
				&& std::holds_alternative<associate_review_mutation>(
					commit->Mutation)
				&& !prepared.AcceptedEpoch.has_value(),
			"unchanged revision emits only association mutation");

		const auto committed = transition(
			fixture.State,
			epoch_catalog_committed_event{
				.Context     = commit->Context,
				.Mutation    = commit->Mutation,
				.NewRevision = epoch_catalog_revision{ 2 },
			});
		expect(
			!committed.AcceptedEpoch.has_value() && committed.Catalog.has_value()
				&& committed.Catalog->ChangedReviewAssociation
					== association.Association
				&& !committed.Catalog->InsertedEpoch.has_value()
				&& fixture.State.Catalog->Revision == epoch_catalog_revision{ 2 },
			"association terminal advances catalog once and creates no epoch");
	}

	auto test_fail_closed_comparison_exhaustion() -> void
	{
		{
			auto       fixture   = make_ready_fixture();
			const auto refreshed = revision(2U, 10U, 21U, 30U);
			const auto refresh   = transition(
				fixture.State,
				pull_request_revision_accepted_event{
					.ApplicationSession = fixture.State.ApplicationSession,
					.Authority          = fixture.Authority,
					.Revision           = refreshed,
				});
			const auto* prepare  = request_from<prepare_epoch_revision_request>(refresh);
			const auto  prepared = transition(
				fixture.State,
				epoch_revision_prepared_event{
					.Context     = prepare->Context,
					.Operation   = opaque<epoch_operation_id>(93U),
					.Observation = epoch_preparation_observation{
						.LatestLive      = fixture.Epoch,
						.ProposedOrdinal = epoch_ordinal{ 2 },
						.ObservedAt      = observed_utc_milliseconds{ 200 },
						.HeadLineage     = head_lineage::descendant,
						.Material        = material(62U, 72U),
					},
				});
			const auto* commit   = request_from<commit_epoch_catalog_request>(prepared);
			const auto  selected = transition(
				fixture.State,
				select_named_comparison_intent{
					.Alias                   = named_comparison::previous_upload_to_latest,
					.ExpectedCatalogRevision = fixture.Catalog.Revision,
				});
			const auto pendingComparison       = *request_from<resolve_comparison_request>(selected);
			fixture.State.ComparisonGeneration = comparison_generation{ std::numeric_limits<std::uint64_t>::max() };
			const auto committed               = transition(
				fixture.State,
				epoch_catalog_committed_event{
					.Context     = commit->Context,
					.Mutation    = commit->Mutation,
					.NewRevision = epoch_catalog_revision{ 2 },
				});
			const auto* cancelledComparison = request_from<cancel_epoch_work_request>(committed);
			expect(
				committed.AcceptedEpoch.has_value()
					&& diagnostic_is(
						committed,
						epoch_reason_code::comparison_generation_exhausted)
					&& fixture.State.Catalog->Revision == epoch_catalog_revision{ 2 }
					&& !fixture.State.AcceptedComparison.has_value()
					&& !fixture.State.PendingComparison.has_value()
					&& cancelledComparison != nullptr
					&& cancelledComparison->Request
						== pendingComparison.Context.Request,
				"generation exhaustion retires superseded comparison work after a "
				"durable commit");
		}

		{
			auto fixture                             = make_ready_fixture();
			fixture.State.AcceptedComparisonRevision = comparison_revision{ maximum_comparison_revision };
			const auto alternate                     = summary(fixture.Authority, fixture.Catalog, 2, 12U, 32U);
			const auto alternateHead                 = anchor(alternate, comparison_anchor_kind::epoch_head);
			const auto selected                      = transition(
				fixture.State,
				select_comparison_anchors_intent{
					.Left                    = alternateHead.Identity,
					.Right                   = alternateHead.Identity,
					.ExpectedCatalogRevision = fixture.Catalog.Revision,
				});
			const auto* resolve   = request_from<resolve_comparison_request>(selected);
			const auto  exhausted = transition(
				fixture.State,
				comparison_resolved_event{
					.Context    = resolve->Context,
					.Candidates = comparison_candidates{
						.Catalog       = fixture.Catalog,
						.ExplicitLeft  = alternateHead,
						.ExplicitRight = alternateHead,
					},
				});
			expect(
				diagnostic_is(
					exhausted, epoch_reason_code::comparison_revision_exhausted)
					&& !fixture.State.AcceptedComparison.has_value()
					&& !fixture.State.PendingComparison.has_value()
					&& fixture.State.Status == epoch_status::unavailable,
				"comparison revision exhaustion clears pending and old "
				"authorization");
		}
	}

	auto test_invalid_cross_facts_and_command_policy() -> void
	{
		const auto owner             = authority();
		const auto ownerCatalog      = catalog();
		const auto prior             = summary(owner, ownerCatalog, 1, 10U, 20U);
		const auto initialBadLineage = plan_epoch_mutation(
			owner,
			revision(),
			ownerCatalog,
			epoch_preparation_observation{
				.ProposedOrdinal = epoch_ordinal{ 1 },
				.ObservedAt      = observed_utc_milliseconds{ 100 },
				.HeadLineage     = head_lineage::descendant,
			});
		expect(
			initialBadLineage.Rejection == epoch_reason_code::invalid_observation,
			"initial observation rejects an invented lineage transition");

		const auto unchangedBadLineage = plan_epoch_mutation(
			owner,
			revision(2U, 10U, 20U),
			ownerCatalog,
			epoch_preparation_observation{
				.LatestLive  = prior,
				.ObservedAt  = observed_utc_milliseconds{ 200 },
				.HeadLineage = head_lineage::descendant,
			});
		expect(
			unchangedBadLineage.Rejection
				== epoch_reason_code::invalid_observation,
			"unchanged head rejects a non-applicable ancestry claim");

		auto malformed          = prior;
		malformed.Cause.Primary = static_cast<epoch_primary_cause>(255U);
		expect(
			resolve_comparison(
				owner,
				named_selection(named_comparison::whole_pr),
				comparison_candidates{
					.Catalog    = ownerCatalog,
					.LatestLive = malformed,
				})
					.Reason
				== epoch_reason_code::invalid_observation,
			"malformed persisted cause enum fails closed");
		expect(
			resolve_comparison(
				owner,
				named_selection(static_cast<named_comparison>(255U)),
				comparison_candidates{
					.Catalog    = ownerCatalog,
					.LatestLive = prior,
				})
					.Reason
				== epoch_reason_code::invalid_selection,
			"malformed named alias fails closed");

		auto invalidReviewRevision                = revision();
		invalidReviewRevision.CurrentViewerReview = provider_review(
			1U, invalidReviewRevision.Head, github_viewer_actor_id{ 999U });
		auto       state  = initial_state();
		const auto before = state;
		expect(
			diagnostic_is(
				transition(
					state,
					pull_request_revision_accepted_event{
						.ApplicationSession = state.ApplicationSession,
						.Authority          = owner,
						.Revision           = invalidReviewRevision,
					}),
				epoch_reason_code::invalid_revision)
				&& state == before,
			"review actor must equal exact accepted viewer authority");

		{
			epoch_state invalidSession;
			const auto  sessionBefore = invalidSession;
			const auto  result        = transition(
				invalidSession,
				pull_request_revision_accepted_event{
					.ApplicationSession = application_session_id{ },
					.Authority          = owner,
					.Revision           = revision(),
				});
			expect(
				diagnostic_is(result, epoch_reason_code::invalid_revision)
					&& invalidSession == sessionBefore && result.Requests.empty(),
				"zero application session cannot enter a host request context");
		}

		auto       fixture       = make_ready_fixture();
		const auto readyCommands = epoch_commands(fixture.State);
		expect(
			readyCommands[0].Enabled && readyCommands[1].Enabled
				&& readyCommands[2].Enabled && readyCommands[3].Enabled
				&& !readyCommands[4].Enabled && !readyCommands[5].Enabled,
			"ready core enables refresh, selection, and maintenance but not "
			"cancellation");

		{
			auto withoutPair = fixture.State;
			withoutPair.AcceptedComparison.reset();
			const auto commands = epoch_commands(withoutPair);
			expect(!commands[2].Enabled
					   && commands[2].DisabledReason
						   == epoch_reason_code::no_resolved_comparison
					   && commands[3].Enabled,
				   "prune requires an accepted comparison while repository-cache "
				   "deletion remains generally available");
		}

		{
			auto       reviewOnly         = fixture.State;
			const auto first              = review_anchor(fixture.Authority,
														  fixture.Catalog,
														  associated_review(1U, fixture.Epoch.Head));
			const auto second             = review_anchor(fixture.Authority,
														  fixture.Catalog,
														  associated_review(2U, fixture.Epoch.Head));
			reviewOnly.AcceptedComparison = comparison_pair{
				.Left  = first,
				.Right = second,
			};
			const auto commands = epoch_commands(reviewOnly);
			expect(!commands[2].Enabled
					   && commands[2].DisabledReason
						   == epoch_reason_code::invalid_maintenance_target
					   && commands[3].Enabled,
				   "a comparison containing only review anchors cannot project a "
				   "prune command");
		}

		{
			const auto epochPair = [&](const anchor_availability aBase,
									   const anchor_availability aHead) {
				auto epoch                   = fixture.Epoch;
				epoch.BaseAvailability       = aBase;
				epoch.HeadAvailability       = aHead;
				auto pairState               = fixture.State;
				pairState.AcceptedComparison = comparison_pair{
					.Left  = anchor(epoch, comparison_anchor_kind::epoch_base),
					.Right = anchor(epoch, comparison_anchor_kind::epoch_head),
				};
				return pairState;
			};
			const auto pruneCommand = [](const epoch_state& aState) {
				return epoch_commands(aState)[2];
			};

			expect(pruneCommand(epochPair(anchor_availability::available,
										  anchor_availability::available))
						   .Enabled
					   && pruneCommand(epochPair(anchor_availability::unavailable,
												 anchor_availability::unavailable))
							  .Enabled
					   && pruneCommand(epochPair(anchor_availability::corrupt,
												 anchor_availability::corrupt))
							  .Enabled,
				   "available, unavailable, and corrupt epoch anchors admit the "
				   "prune command");

			for (const auto ineligible : { anchor_availability::pruned,
										   anchor_availability::cache_deleted,
										   anchor_availability::never_observed }) {
				const auto ineligibleState = epochPair(ineligible, ineligible);
				const auto commands        = epoch_commands(ineligibleState);
				expect(!commands[2].Enabled
						   && commands[2].DisabledReason
							   == epoch_reason_code::invalid_maintenance_target
						   && commands[3].Enabled,
					   "an epoch pair whose anchors cannot enter a prune "
					   "operation disables prune while repository-cache deletion "
					   "remains available");
			}

			expect(pruneCommand(epochPair(anchor_availability::pruned,
										  anchor_availability::available))
					   .Enabled,
				   "one prunable anchor of the same epoch admits prune without "
				   "duplicating its target");

			{
				auto epoch               = fixture.Epoch;
				epoch.HeadAvailability   = anchor_availability::pruned;
				auto mixed               = fixture.State;
				mixed.AcceptedComparison = comparison_pair{
					.Left  = review_anchor(fixture.Authority,
										   fixture.Catalog,
										   associated_review(1U, fixture.Epoch.Head)),
					.Right = anchor(epoch, comparison_anchor_kind::epoch_head),
				};
				expect(!pruneCommand(mixed).Enabled
						   && pruneCommand(mixed).DisabledReason
							   == epoch_reason_code::invalid_maintenance_target,
					   "a review anchor cannot supply prune eligibility for an "
					   "already pruned epoch anchor");

				epoch.HeadAvailability   = anchor_availability::corrupt;
				mixed.AcceptedComparison = comparison_pair{
					.Left  = review_anchor(fixture.Authority,
										   fixture.Catalog,
										   associated_review(1U, fixture.Epoch.Head)),
					.Right = anchor(epoch, comparison_anchor_kind::epoch_head),
				};
				expect(pruneCommand(mixed).Enabled,
					   "a corrupt epoch anchor beside a review anchor still "
					   "admits prune");
			}

			{
				auto       ineligible = epochPair(anchor_availability::pruned,
												  anchor_availability::cache_deleted);
				const auto dispatched = transition(ineligible,
												   prune_review_epochs_intent{
													   .Catalog = fixture.Catalog,
													   .Epochs  = { fixture.Epoch.Id },
												   });
				expect(ineligible.PendingMaintenance.has_value()
						   && !dispatched.Requests.empty(),
					   "the disabled projection is a presentation pre-check, not "
					   "a new authorization boundary: a directly injected intent "
					   "still reaches the durable authority that fails it closed");
			}
		}

		{
			auto lastRequestState        = initial_state();
			lastRequestState.NextRequest = epoch_request_id{ std::numeric_limits<std::uint64_t>::max() };
			const auto exhausted         = make_ready_fixture(lastRequestState);
			const auto commands          = epoch_commands(exhausted.State);
			expect(
				exhausted.State.RequestCounterExhausted && !commands[0].Enabled
					&& commands[0].DisabledReason
						== epoch_reason_code::request_counter_exhausted
					&& !commands[1].Enabled
					&& commands[1].DisabledReason
						== epoch_reason_code::request_counter_exhausted,
				"last request completes normally and then disables new "
				"request-backed commands");
		}

		{
			auto exhausted                  = make_ready_fixture();
			exhausted.State.EpochGeneration = epoch_generation{ std::numeric_limits<std::uint64_t>::max() };
			const auto commands             = epoch_commands(exhausted.State);
			expect(
				!commands[0].Enabled
					&& commands[0].DisabledReason
						== epoch_reason_code::epoch_generation_exhausted
					&& commands[1].Enabled,
				"epoch generation exhaustion disables only refresh");
		}

		{
			auto exhausted                       = make_ready_fixture();
			exhausted.State.ComparisonGeneration = comparison_generation{ std::numeric_limits<std::uint64_t>::max() };
			const auto commands                  = epoch_commands(exhausted.State);
			expect(
				commands[0].Enabled && !commands[1].Enabled
					&& commands[1].DisabledReason
						== epoch_reason_code::comparison_generation_exhausted,
				"comparison generation exhaustion disables only selection");
		}

		{
			auto        retryFixture = make_ready_fixture();
			const auto  acceptedPair = retryFixture.State.AcceptedComparison;
			const auto  firstRefresh = transition(retryFixture.State, refresh_epochs_intent{ });
			const auto* firstPrepare = request_from<prepare_epoch_revision_request>(firstRefresh);
			expect(
				firstPrepare != nullptr
					&& retryFixture.State.AcceptedComparison == acceptedPair
					&& retryFixture.State.Status == epoch_status::preparing,
				"explicit refresh retries the accepted revision while retaining "
				"its "
				"pair");

			const auto failed = transition(
				retryFixture.State,
				epoch_revision_prepare_failed_event{
					.Context = firstPrepare->Context,
					.Failure = epoch_failure::unavailable,
				});
			expect(
				diagnostic_is(failed, epoch_reason_code::unavailable)
					&& !retryFixture.State.PendingIngestion.has_value(),
				"failed refresh terminal clears only the pending ingestion");

			const auto  retried       = transition(retryFixture.State, refresh_epochs_intent{ });
			const auto* retryPrepare  = request_from<prepare_epoch_revision_request>(retried);
			const auto  retryCommands = epoch_commands(retryFixture.State);
			expect(
				retryPrepare != nullptr
					&& retryPrepare->Context.Request
						!= firstPrepare->Context.Request
					&& retryPrepare->Context.Generation
						!= firstPrepare->Context.Generation
					&& retryFixture.State.AcceptedComparison == acceptedPair
					&& !retryCommands[0].Enabled && retryCommands[4].Enabled
					&& !retryCommands[2].Enabled
					&& !retryCommands[3].Enabled,
				"refresh intent provides a new exact retry after failure");
		}

		{
			auto       interleaved = make_ready_fixture();
			const auto selection   = transition(
				interleaved.State,
				select_named_comparison_intent{
					.Alias                   = named_comparison::previous_upload_to_latest,
					.ExpectedCatalogRevision = interleaved.Catalog.Revision,
				});
			const auto  resolve   = *request_from<resolve_comparison_request>(selection);
			const auto  refresh   = transition(interleaved.State, refresh_epochs_intent{ });
			const auto* cancelled = request_from<cancel_epoch_work_request>(refresh);
			expect(
				cancelled != nullptr
					&& cancelled->Request == resolve.Context.Request
					&& !interleaved.State.PendingComparison.has_value()
					&& interleaved.State.PendingIngestion.has_value(),
				"refresh cancels and retires an older comparison lane");

			const auto delayed = transition(
				interleaved.State,
				comparison_resolved_event{
					.Context    = resolve.Context,
					.Candidates = comparison_candidates{
						.Catalog    = interleaved.Catalog,
						.LatestLive = interleaved.Epoch,
					},
				});
			expect(
				diagnostic_is(delayed, epoch_reason_code::stale_completion)
					&& interleaved.State.Status == epoch_status::preparing,
				"retired comparison cannot overwrite a newer refresh status");

			const auto  priorRequest   = interleaved.State.PendingIngestion->Request;
			const auto  restarted      = transition(interleaved.State, refresh_epochs_intent{ });
			const auto* restartCancel  = request_from<cancel_epoch_work_request>(restarted);
			const auto* restartPrepare = request_from<prepare_epoch_revision_request>(restarted);
			expect(
				restartCancel != nullptr
					&& restartCancel->Request == priorRequest
					&& restartPrepare != nullptr
					&& restartPrepare->Context.Request != priorRequest,
				"refresh intent explicitly restarts an in-flight ingestion");
		}

		const auto beforeSide  = fixture.State;
		const auto invalidSide = transition(
			fixture.State,
			select_comparison_anchor_intent{
				.Side                    = static_cast<comparison_side>(255U),
				.Anchor                  = fixture.State.AcceptedComparison->Left.Identity,
				.ExpectedCatalogRevision = fixture.Catalog.Revision,
			});
		expect(
			diagnostic_is(invalidSide, epoch_reason_code::invalid_selection)
				&& fixture.State == beforeSide,
			"malformed comparison side cannot become an explicit selection");
	}

	auto test_selection_transition_policy() -> void
	{
		auto       fixture       = make_ready_fixture();
		const auto wholePair     = *fixture.State.AcceptedComparison;
		const auto alternate     = summary(fixture.Authority, fixture.Catalog, 2, 12U, 32U);
		const auto alternateHead = anchor(alternate, comparison_anchor_kind::epoch_head);

		const auto changed = transition(
			fixture.State,
			select_comparison_anchor_intent{
				.Side                    = comparison_side::left,
				.Anchor                  = alternateHead.Identity,
				.ExpectedCatalogRevision = fixture.Catalog.Revision,
			});
		const auto* explicitPair = changed.Selection.has_value()
			? std::get_if<explicit_comparison_selection>(
				  &changed.Selection->Selection)
			: nullptr;
		expect(
			explicitPair != nullptr
				&& explicitPair->Left == alternateHead.Identity
				&& explicitPair->Right == wholePair.Right.Identity
				&& !fixture.State.AcceptedComparison.has_value(),
			"editing one side converts named selection to an exact explicit pair");

		const auto* resolve = request_from<resolve_comparison_request>(changed);
		expect(resolve != nullptr, "one-side edit requests exact resolution");
		const auto resolved = transition(
			fixture.State,
			comparison_resolved_event{
				.Context    = resolve->Context,
				.Candidates = comparison_candidates{
					.Catalog       = fixture.Catalog,
					.ExplicitLeft  = alternateHead,
					.ExplicitRight = wholePair.Right,
				},
			});
		expect(
			resolved.Comparison.has_value()
				&& resolved.Comparison->Pair->Left == alternateHead
				&& resolved.Comparison->Pair->Right == wholePair.Right,
			"explicit one-side resolution preserves both exact identities");

		const auto beforeMismatch = fixture.State;
		const auto badRevision    = transition(
			fixture.State,
			select_named_comparison_intent{
				.Alias                   = named_comparison::whole_pr,
				.ExpectedCatalogRevision = epoch_catalog_revision{ 999 },
			});
		expect(
			diagnostic_is(
				badRevision, epoch_reason_code::catalog_revision_mismatch)
				&& fixture.State == beforeMismatch,
			"stale selector catalog revision cannot mutate selection");

		const auto unavailableHead = comparison_anchor{
			.Identity     = wholePair.Right.Identity,
			.Commit       = wholePair.Right.Commit,
			.Availability = anchor_availability::pruned,
		};
		const auto selectUnavailable = transition(
			fixture.State,
			select_comparison_anchors_intent{
				.Left                    = unavailableHead.Identity,
				.Right                   = unavailableHead.Identity,
				.ExpectedCatalogRevision = fixture.Catalog.Revision,
			});
		const auto* unavailableRequest = request_from<resolve_comparison_request>(selectUnavailable);
		const auto  unavailable        = transition(
			fixture.State,
			comparison_resolved_event{
				.Context    = unavailableRequest->Context,
				.Candidates = comparison_candidates{
					.Catalog       = fixture.Catalog,
					.ExplicitLeft  = unavailableHead,
					.ExplicitRight = unavailableHead,
				},
			});
		expect(
			unavailable.Comparison.has_value()
				&& unavailable.Comparison->Pair.has_value()
				&& !unavailable.Comparison->AvailableForDiff
				&& fixture.State.Status == epoch_status::unavailable,
			"exact explicit pair remains selected when its object is unavailable");
	}

	/// [[BPR-1560]]. Whole PR compares from the merge base GitHub named. When
	/// GitHub named none, the base the revision carries is only the recorded
	/// base: Whole PR names its pair with that base unavailable and says why,
	/// while comparisons between updates are untouched.
	auto test_whole_pr_compares_only_from_a_named_merge_base() -> void
	{
		const auto owner        = authority();
		const auto ownerCatalog = catalog(8);
		const auto previous     = summary(owner, ownerCatalog, 7, 10U, 20U);
		const auto latest       = summary(owner, ownerCatalog, 8, 11U, 30U);
		const auto candidates   = comparison_candidates{
			.Catalog              = ownerCatalog,
			.LatestLive           = latest,
			.PreviousDistinctHead = previous,
		};

		const auto named = resolve_comparison(owner, named_selection(named_comparison::whole_pr), candidates, comparison_base_basis::merge_base);
		expect(named.Reason == epoch_reason_code::none && named.Pair && available_for_diff(*named.Pair) && named.Pair->Left.Commit == latest.Base,
			   "with a named merge base Whole PR compares from the newest base");

		const auto recorded = resolve_comparison(owner, named_selection(named_comparison::whole_pr), candidates, comparison_base_basis::recorded_base);
		expect(recorded.Reason == epoch_reason_code::merge_base_unavailable && recorded.Pair && !available_for_diff(*recorded.Pair)
				   && recorded.Pair->Left.Identity.Kind == comparison_anchor_kind::epoch_base && recorded.Pair->Left.Commit == latest.Base
				   && recorded.Pair->Left.Availability == anchor_availability::unavailable && recorded.Pair->Right.Commit == latest.Head
				   && recorded.Pair->Right.Availability == anchor_availability::available,
			   "without a named merge base Whole PR never compares from the recorded base");

		const auto upload = resolve_comparison(owner, named_selection(named_comparison::previous_upload_to_latest), candidates, comparison_base_basis::recorded_base);
		expect(upload.Reason == epoch_reason_code::none && upload.Pair && available_for_diff(*upload.Pair) && upload.Pair->Left.Commit == previous.Head,
			   "an interdiff between updates does not depend on the merge base");

		const auto previousHead = anchor(previous, comparison_anchor_kind::epoch_head);
		const auto latestBase   = anchor(latest, comparison_anchor_kind::epoch_base);
		const auto exact        = resolve_comparison(owner, explicit_selection(latestBase.Identity, previousHead.Identity),
													 comparison_candidates{ .Catalog = ownerCatalog, .ExplicitLeft = latestBase, .ExplicitRight = previousHead },
													 comparison_base_basis::recorded_base);
		expect(exact.Reason == epoch_reason_code::none && exact.Pair && available_for_diff(*exact.Pair),
			   "an exact pair the reviewer chose stays exactly what was chosen");

		// The transition: a ready review whose revision loses its named merge
		// base is not deduplicated, and Whole PR then says why it shows nothing.
		auto       fixture = make_ready_fixture();
		const auto same    = transition(fixture.State, pull_request_revision_accepted_event{ .ApplicationSession = fixture.State.ApplicationSession, .Authority = fixture.Authority, .Revision = fixture.Revision });
		expect(same == epoch_transition_output{ }, "the same revision and basis change nothing");
		const auto changed = transition(fixture.State, pull_request_revision_accepted_event{ .ApplicationSession = fixture.State.ApplicationSession, .Authority = fixture.Authority, .Revision = fixture.Revision, .Basis = comparison_base_basis::recorded_base });
		expect(changed != epoch_transition_output{ } && fixture.State.AcceptedBasis == comparison_base_basis::recorded_base,
			   "the same revision without a named merge base is accepted again");

		auto ready                = make_ready_fixture();
		ready.State.AcceptedBasis = comparison_base_basis::recorded_base;
		const auto  select        = transition(ready.State, select_named_comparison_intent{ .Alias = named_comparison::previous_upload_to_latest, .ExpectedCatalogRevision = ready.Catalog.Revision });
		const auto* toUpload      = request_from<resolve_comparison_request>(select);
		expect(toUpload != nullptr, "an interdiff selection asks for resolution");
		static_cast<void>(transition(ready.State, comparison_resolved_event{ .Context = toUpload->Context, .Candidates = { .Catalog = ready.Catalog, .LatestLive = ready.Epoch } }));
		const auto  whole   = transition(ready.State, select_named_comparison_intent{ .Alias = named_comparison::whole_pr, .ExpectedCatalogRevision = ready.Catalog.Revision });
		const auto* toWhole = request_from<resolve_comparison_request>(whole);
		expect(toWhole != nullptr, "Whole PR asks for resolution");
		const auto resolved = transition(ready.State, comparison_resolved_event{ .Context = toWhole->Context, .Candidates = { .Catalog = ready.Catalog, .LatestLive = ready.Epoch } });
		expect(resolved.Comparison && resolved.Comparison->Pair && !resolved.Comparison->AvailableForDiff && ready.State.Status == epoch_status::unavailable
				   && std::ranges::any_of(resolved.Diagnostics, [](const epoch_diagnostic& aDiagnostic) { return aDiagnostic.Code == epoch_reason_code::merge_base_unavailable; }),
			   "Whole PR without a named merge base is unavailable and says why");
	}

	auto test_pending_one_sided_selection_supersession() -> void
	{
		auto        fixture      = make_ready_fixture();
		const auto  whole        = *fixture.State.AcceptedComparison;
		const auto  alternate    = anchor(summary(fixture.Authority, fixture.Catalog, 2, 12U, 32U), comparison_anchor_kind::epoch_head);
		const auto  first        = transition(fixture.State, select_comparison_anchor_intent{ .Side = comparison_side::left, .Anchor = alternate.Identity, .ExpectedCatalogRevision = fixture.Catalog.Revision });
		const auto* firstRequest = request_from<resolve_comparison_request>(first);
		expect(firstRequest && !fixture.State.AcceptedComparison, "first selector starts unresolved exact comparison");
		const auto  second        = transition(fixture.State, select_comparison_anchor_intent{ .Side = comparison_side::right, .Anchor = whole.Left.Identity, .ExpectedCatalogRevision = fixture.Catalog.Revision });
		const auto* secondRequest = request_from<resolve_comparison_request>(second);
		const auto* cancelled     = request_from<cancel_epoch_work_request>(second);
		expect(secondRequest && cancelled && cancelled->Request == firstRequest->Context.Request
				   && fixture.State.RequestedSelection == explicit_selection(alternate.Identity, whole.Left.Identity),
			   "enabled second selector supersedes pending resolution and preserves the requested opposite side");
		const auto beforeLate = fixture.State;
		const auto late       = transition(fixture.State, comparison_resolved_event{ .Context = firstRequest->Context, .Candidates = comparison_candidates{ .Catalog = fixture.Catalog, .ExplicitLeft = alternate, .ExplicitRight = whole.Right } });
		expect(diagnostic_is(late, epoch_reason_code::stale_completion) && fixture.State == beforeLate,
			   "superseded one-side result cannot replace the pending pair");
		const auto beforeInvalid = fixture.State;
		const auto invalid       = transition(fixture.State, select_comparison_anchor_intent{ .Side = comparison_side::right, .Anchor = whole.Right.Identity, .ExpectedCatalogRevision = epoch_catalog_revision{ 999 } });
		expect(diagnostic_is(invalid, epoch_reason_code::catalog_revision_mismatch) && fixture.State == beforeInvalid,
			   "pending selection still rejects a stale catalog revision");
		const auto  third        = transition(fixture.State, select_comparison_anchor_intent{ .Side = comparison_side::left, .Anchor = whole.Right.Identity, .ExpectedCatalogRevision = fixture.Catalog.Revision });
		const auto* thirdRequest = request_from<resolve_comparison_request>(third);
		expect(thirdRequest && fixture.State.RequestedSelection == explicit_selection(whole.Right.Identity, whole.Left.Identity),
			   "another pending edit preserves both latest requested sides");
		const auto resolved = transition(fixture.State, comparison_resolved_event{ .Context = thirdRequest->Context, .Candidates = comparison_candidates{ .Catalog = fixture.Catalog, .ExplicitLeft = whole.Right, .ExplicitRight = whole.Left } });
		expect(resolved.Comparison && resolved.Comparison->Pair && resolved.Comparison->Pair->Left == whole.Right
				   && resolved.Comparison->Pair->Right == whole.Left && !fixture.State.PendingComparison,
			   "latest pending selector pair alone becomes the accepted comparison");
	}

	auto test_overflow_is_checked_and_invalidation_fails_closed() -> void
	{
		{
			auto state        = initial_state();
			state.NextRequest = epoch_request_id{ std::numeric_limits<std::uint64_t>::max() };
			const auto first  = transition(
				state,
				pull_request_revision_accepted_event{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = authority(),
					.Revision           = revision(),
				});
			const auto* load = request_from<load_epoch_catalog_request>(first);
			expect(
				load != nullptr
					&& load->Context.Request
						== epoch_request_id{ std::numeric_limits<
							std::uint64_t>::max() }
					&& state.RequestCounterExhausted,
				"maximum request ID is issued once and then marked exhausted");

			const auto second = transition(
				state,
				pull_request_revision_accepted_event{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = authority(),
					.Revision           = revision(2U, 10U, 21U),
				});
			expect(
				diagnostic_is(second, epoch_reason_code::request_counter_exhausted)
					&& !state.Authority.has_value()
					&& !state.AcceptedComparison.has_value()
					&& second.Catalog.has_value()
					&& !second.Catalog->Catalog.has_value()
					&& second.Selection.has_value()
					&& second.Comparison.has_value()
					&& !second.Comparison->Pair.has_value()
					&& has_changed(
						second.Changed, epoch_changed_domain::catalog)
					&& has_changed(
						second.Changed, epoch_changed_domain::selection)
					&& has_changed(
						second.Changed, epoch_changed_domain::comparison),
				"exhausted refresh request publishes fail-closed projection "
				"clears");
		}
		{
			auto state            = initial_state();
			state.EpochGeneration = epoch_generation{ std::numeric_limits<std::uint64_t>::max() };
			const auto before     = state;
			const auto result     = transition(
				state,
				pull_request_revision_accepted_event{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = authority(),
					.Revision           = revision(),
				});
			expect(
				diagnostic_is(result, epoch_reason_code::epoch_generation_exhausted)
					&& state == before,
				"generation overflow rejects activation without partial state");
		}
		{
			auto fixture                       = make_ready_fixture();
			fixture.State.ComparisonGeneration = comparison_generation{ std::numeric_limits<std::uint64_t>::max() };
			const auto before                  = fixture.State;
			const auto result                  = transition(
				fixture.State,
				select_named_comparison_intent{
					.Alias                   = named_comparison::previous_upload_to_latest,
					.ExpectedCatalogRevision = fixture.Catalog.Revision,
				});
			expect(
				diagnostic_is(
					result, epoch_reason_code::comparison_generation_exhausted)
					&& fixture.State == before,
				"comparison generation overflow leaves selection unchanged");
		}
		{
			auto fixture                       = make_ready_fixture();
			fixture.State.EpochGeneration      = epoch_generation{ std::numeric_limits<std::uint64_t>::max() };
			fixture.State.ComparisonGeneration = comparison_generation{ std::numeric_limits<std::uint64_t>::max() };
			const auto result                  = transition(
				fixture.State,
				pull_request_revision_invalidated_event{
					.ApplicationSession = fixture.State.ApplicationSession,
					.Authority          = fixture.Authority,
					.Revision           = fixture.Revision,
					.Generation         = fixture.State.EpochGeneration,
				});
			expect(
				!fixture.State.Authority.has_value()
					&& !fixture.State.AcceptedRevision.has_value()
					&& !fixture.State.Catalog.has_value()
					&& result.Diagnostics.size() == 2U,
				"externally revoked authority clears fail-closed even at max "
				"generations");
		}
	}

	/// BPR-0725. A cache hit prepares no mutation and appends nothing, so the
	/// newest history page is the only thing that can give the line its rows.
	auto test_settled_ingestion_requests_the_newest_history_page() -> void
	{
		const auto refreshToPreparing = [](ready_fixture& aFixture)
			-> prepare_epoch_revision_request {
			const auto  refreshed   = transition(aFixture.State, refresh_epochs_intent{ });
			const auto* preparation = request_from<prepare_epoch_revision_request>(refreshed);
			expect(preparation != nullptr
					   && request_from<load_epoch_catalog_request>(refreshed) == nullptr
					   && (!refreshed.History
						   || refreshed.History->Phase
							   != epoch_history_page_phase::invalidated),
				   "a refresh over a retained catalogue prepares directly and "
				   "keeps its history window");
			return *preparation;
		};
		const auto unchanged = [](const ready_fixture&                  aFixture,
								  const prepare_epoch_revision_request& aPrepare) {
			return epoch_revision_prepared_event{
				.Context     = aPrepare.Context,
				.Operation   = opaque<epoch_operation_id>(52U),
				.Observation = epoch_preparation_observation{
					.LatestLive = aFixture.Epoch,
					.ObservedAt = observed_utc_milliseconds{ 200 },
					.Material   = material(),
				},
			};
		};

		{
			auto        fixture  = make_ready_fixture();
			const auto  prepare  = refreshToPreparing(fixture);
			const auto  prepared = transition(fixture.State, unchanged(fixture, prepare));
			const auto* page     = request_from<load_epoch_history_page_request>(prepared);
			expect(
				request_from<commit_epoch_catalog_request>(prepared) == nullptr
					&& request_from<discard_epoch_preparation_request>(prepared) != nullptr
					&& page != nullptr
					&& request_index<load_epoch_history_page_request>(prepared)
						< request_index<resolve_comparison_request>(prepared)
					&& page->Context.Catalog == fixture.Catalog
					&& page->Context.Navigation == epoch_history_navigation::newest
					&& !page->Context.Cursor
					&& page->Context.Limit == maximum_epoch_history_page_size
					&& prepared.History
					&& prepared.History->Phase == epoch_history_page_phase::loading
					&& has_changed(prepared.Changed, epoch_changed_domain::history)
					&& fixture.State.PendingHistory
					&& fixture.State.PendingHistory->Request == page->Context.Request,
				"a no-mutation prepare requests the newest history page before the "
				"comparison");

			const auto loaded = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = page->Context,
					.Page    = history_page(fixture, { fixture.Epoch }),
				});
			expect(
				loaded.History
					&& loaded.History->Phase == epoch_history_page_phase::loaded
					&& loaded.History->Epochs == std::vector{ fixture.Epoch }
					&& fixture.State.HistoryCatalog == fixture.Catalog,
				"the requested page loads the epoch the catalogue holds");

			const auto again    = refreshToPreparing(fixture);
			const auto repeated = transition(fixture.State, unchanged(fixture, again));
			expect(
				request_from<load_epoch_history_page_request>(repeated) == nullptr
					&& !repeated.History
					&& request_from<resolve_comparison_request>(repeated) != nullptr
					&& fixture.State.HistoryCatalog == fixture.Catalog
					&& fixture.State.HistoryNavigation
						== epoch_history_navigation::newest
					&& !fixture.State.PendingHistory,
				"a window already bound to the catalogue revision is left alone");
		}

		{
			auto       fixture      = make_ready_fixture();
			const auto prepare      = refreshToPreparing(fixture);
			const auto explicitPage = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto* inFlight = request_from<load_epoch_history_page_request>(explicitPage);
			expect(inFlight != nullptr && fixture.State.PendingHistory,
				   "a page can be requested while the revision prepares");
			const auto pending  = *fixture.State.PendingHistory;
			const auto prepared = transition(fixture.State, unchanged(fixture, prepare));
			expect(
				request_from<load_epoch_history_page_request>(prepared) == nullptr
					&& !prepared.History
					&& request_from<resolve_comparison_request>(prepared) != nullptr
					&& fixture.State.PendingHistory == std::optional{ pending },
				"an in-flight page is not duplicated by a settled ingestion");
		}
	}

	auto test_bounded_epoch_history_paging() -> void
	{
		{
			auto       state  = initial_state();
			const auto result = transition(
				state,
				request_epoch_history_page_intent{
					.Catalog = catalog(),
					.Limit   = 10U,
				});
			expect(
				diagnostic_is(result, epoch_reason_code::no_active_authority)
					&& !state.PendingHistory,
				"history paging requires an active pull-request authority");
		}

		{
			auto             fixture = make_ready_fixture();
			const auto       before  = fixture.State;
			const std::array invalidRequests{
				request_epoch_history_page_intent{
					.Catalog = { },
					.Limit   = 10U,
				},
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 0U,
				},
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = static_cast<std::uint16_t>(
						maximum_epoch_history_page_size + 1U),
				},
			};
			const std::array expected{
				epoch_reason_code::invalid_catalog,
				epoch_reason_code::invalid_observation,
				epoch_reason_code::invalid_observation,
			};
			for (std::size_t index = 0U; index < invalidRequests.size(); ++index) {
				const auto result = transition(fixture.State, invalidRequests[index]);
				expect(
					diagnostic_is(result, expected[index])
						&& fixture.State == before,
					"invalid history catalog and page bounds are inert");
			}

			auto wrongCatalog        = fixture.Catalog;
			wrongCatalog.Revision    = epoch_catalog_revision{ 99 };
			const auto wrongRevision = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = wrongCatalog,
					.Limit   = 10U,
				});
			expect(
				diagnostic_is(
					wrongRevision,
					epoch_reason_code::catalog_revision_mismatch)
					&& fixture.State == before,
				"history paging rejects a stale catalog revision");

			const auto wrongCursor = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog    = fixture.Catalog,
					.Navigation = epoch_history_navigation::older,
					.Cursor     = history_cursor(fixture, 4),
					.Limit      = 2U,
				});
			expect(
				diagnostic_is(
					wrongCursor, epoch_reason_code::invalid_history_cursor)
					&& fixture.State == before,
				"a continuation cursor is valid only after the exact prior page");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto status  = fixture.State.Status;
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto* request = request_from<load_epoch_history_page_request>(loading);
			expect(
				request != nullptr && loading.Requests.size() == 1U
					&& loading.History
					&& loading.History->Phase == epoch_history_page_phase::loading
					&& loading.History->Epochs.empty()
					&& has_changed(
						loading.Changed, epoch_changed_domain::history)
					&& fixture.State.PendingHistory
					&& fixture.State.Status == status,
				"one initial intent emits one bounded request without disturbing "
				"comparison status");

			const auto pending   = fixture.State;
			const auto duplicate = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			expect(
				diagnostic_is(
					duplicate, epoch_reason_code::duplicate_history_page)
					&& fixture.State == pending,
				"an in-flight history page cannot be scheduled twice");

			const auto continuation = history_cursor(fixture, 2);
			const auto loaded       = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = request->Context,
					.Page    = history_page(
						fixture,
						{
							summary(
								fixture.Authority,
								fixture.Catalog,
								3,
								10U,
								23U),
							summary(
								fixture.Authority,
								fixture.Catalog,
								2,
								10U,
								22U),
						},
						continuation),
				});
			expect(
				loaded.History
					&& loaded.History->Phase == epoch_history_page_phase::loaded
					&& loaded.History->Epochs.size() == 2U
					&& loaded.History->Continuation == continuation
					&& fixture.State.HistoryCatalog
						== std::optional{ fixture.Catalog }
					&& fixture.State.HistoryNavigation
						== epoch_history_navigation::newest
					&& fixture.State.HistoryContinuation == continuation
					&& !fixture.State.PendingHistory
					&& fixture.State.Status == status,
				"a full initial page publishes rows transiently and retains only "
				"its exact continuation");

			const auto wrongContinuation = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog    = fixture.Catalog,
					.Navigation = epoch_history_navigation::older,
					.Cursor     = history_cursor(fixture, 1),
					.Limit      = 2U,
				});
			expect(
				diagnostic_is(
					wrongContinuation,
					epoch_reason_code::invalid_history_cursor)
					&& !fixture.State.PendingHistory,
				"a caller cannot skip or rewrite the retained continuation");

			const auto continuing = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog    = fixture.Catalog,
					.Navigation = epoch_history_navigation::older,
					.Cursor     = continuation,
					.Limit      = 2U,
				});
			const auto* continuationRequest = request_from<load_epoch_history_page_request>(continuing);
			expect(
				continuationRequest
					&& continuationRequest->Context.Navigation
						== epoch_history_navigation::older
					&& continuationRequest->Context.Cursor == continuation,
				"the next page request carries the exact revision-bound cursor");

			auto mismatchedAuthority           = continuationRequest->Context;
			mismatchedAuthority.Authority      = authority(2U);
			const auto beforeAuthorityMismatch = fixture.State;
			const auto authorityMismatch       = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = mismatchedAuthority,
					.Page    = history_page(fixture, { }),
				});
			expect(
				diagnostic_is(
					authorityMismatch,
					epoch_reason_code::completion_identity_mismatch)
					&& fixture.State == beforeAuthorityMismatch,
				"a history terminal with mismatched authority is inert");

			auto mismatchedContext            = continuationRequest->Context;
			mismatchedContext.Source.Snapshot = provider_snapshot_id{ 999U };
			const auto beforeMismatch         = fixture.State;
			const auto mismatch               = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = mismatchedContext,
					.Page    = history_page(fixture, { }),
				});
			expect(
				diagnostic_is(
					mismatch,
					epoch_reason_code::completion_identity_mismatch)
					&& fixture.State == beforeMismatch,
				"a history terminal with mismatched source identity is inert");

			const auto finalPage = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = continuationRequest->Context,
					.Page    = history_page(
						fixture,
						{
							summary(
								fixture.Authority,
								fixture.Catalog,
								1,
								10U,
								21U),
						}),
				});
			expect(
				finalPage.History && finalPage.History->Epochs.size() == 1U
					&& !finalPage.History->Continuation
					&& !fixture.State.HistoryContinuation
					&& fixture.State.HistoryNavigation
						== epoch_history_navigation::older
					&& !fixture.State.PendingHistory,
				"a short continuation page terminates the bounded sequence");

			const auto newest = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto* newestRequest = request_from<load_epoch_history_page_request>(newest);
			expect(
				newestRequest
					&& newestRequest->Context.Navigation
						== epoch_history_navigation::newest
					&& !newestRequest->Context.Cursor,
				"core explicitly authorizes returning from an older window to "
				"the newest page without a fabricated cursor");
			const auto newestLoaded = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = newestRequest->Context,
					.Page    = history_page(
						fixture,
						{
							summary(
								fixture.Authority,
								fixture.Catalog,
								3,
								10U,
								23U),
							summary(
								fixture.Authority,
								fixture.Catalog,
								2,
								10U,
								22U),
						},
						continuation),
				});
			expect(newestLoaded.History
					   && fixture.State.HistoryNavigation
						   == epoch_history_navigation::newest,
				   "a successful newest restart replaces the retained navigation "
				   "position");

			const auto duplicateNewest = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			expect(diagnostic_is(duplicateNewest,
								 epoch_reason_code::duplicate_history_page),
				   "an already-visible newest page cannot be loaded twice");
		}

		{
			auto fixture = make_ready_fixture();
			auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 1U,
				});
			auto context = request_from<load_epoch_history_page_request>(loading)->Context;

			constexpr std::int64_t newestOrdinal = 1'002;
			for (std::int64_t ordinal = newestOrdinal; ordinal >= 1; --ordinal) {
				const auto continuation = ordinal == 1
					? std::optional<epoch_history_cursor>{ }
					: std::optional{ history_cursor(fixture, ordinal) };
				const auto loaded       = transition(
					fixture.State,
					epoch_history_page_loaded_event{
						.Context = context,
						.Page    = history_page(
							fixture,
							{
								summary(fixture.Authority,
										fixture.Catalog,
										ordinal,
										10U,
										20U),
							},
							continuation),
					});
				expect(loaded.History && loaded.History->Epochs.size() == 1U
						   && loaded.History->Epochs.front().Id.Ordinal
							   == epoch_ordinal{ ordinal }
						   && fixture.State.HistoryCatalog
							   == std::optional{ fixture.Catalog }
						   && fixture.State.HistoryContinuation == continuation
						   && !fixture.State.PendingHistory,
					   "each catalog terminal publishes only its bounded page and "
					   "one continuation");
				if (ordinal == 1) {
					expect(loaded.History->Epochs.front().Id.Ordinal
								   == epoch_ordinal{ 1 }
							   && fixture.State.HistoryNavigation
								   == epoch_history_navigation::older,
						   "more than one thousand exact continuations reach "
						   "ordinal one without retaining prior rows in core");
					break;
				}

				loading = transition(
					fixture.State,
					request_epoch_history_page_intent{
						.Catalog    = fixture.Catalog,
						.Navigation = epoch_history_navigation::older,
						.Cursor     = continuation,
						.Limit      = 1U,
					});
				const auto* request = request_from<load_epoch_history_page_request>(loading);
				expect(request && request->Context.Cursor == continuation
						   && request->Context.Navigation
							   == epoch_history_navigation::older,
					   "each older request is authorized only from the exact "
					   "retained continuation");
				context = request->Context;
			}

			const auto noContinuation = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog    = fixture.Catalog,
					.Navigation = epoch_history_navigation::older,
					.Limit      = 1U,
				});
			expect(diagnostic_is(noContinuation,
								 epoch_reason_code::invalid_history_cursor),
				   "older navigation is disabled once ordinal one has no exact "
				   "continuation");

			const auto newest = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 1U,
				});
			const auto* newestRequest = request_from<load_epoch_history_page_request>(newest);
			expect(newestRequest && !newestRequest->Context.Cursor
					   && newestRequest->Context.Navigation
						   == epoch_history_navigation::newest,
				   "the core can restart the newest page exactly after traversing "
				   "more than one thousand older pages");
		}

		{
			const auto rejectMalformedPage = [](
												 const auto& aMutate, const std::string_view aClaim) {
				auto       fixture = make_ready_fixture();
				const auto loading = transition(
					fixture.State,
					request_epoch_history_page_intent{
						.Catalog = fixture.Catalog,
						.Limit   = 2U,
					});
				const auto request = *request_from<load_epoch_history_page_request>(loading);
				auto       page    = history_page(fixture, { });
				aMutate(page, fixture);
				const auto rejected = transition(
					fixture.State,
					epoch_history_page_loaded_event{
						.Context = request.Context,
						.Page    = std::move(page),
					});
				expect(
					diagnostic_is(
						rejected, epoch_reason_code::invalid_observation)
						&& rejected.History
						&& rejected.History->Phase
							== epoch_history_page_phase::failed
						&& !fixture.State.PendingHistory
						&& !fixture.State.HistoryCatalog,
					aClaim);
			};

			rejectMalformedPage(
				[](auto& aPage, const auto&) {
					aPage.Authority = authority(2U).Scope;
				},
				"a page cannot cross pull-request authority");
			rejectMalformedPage(
				[](auto& aPage, const auto&) {
					aPage.Catalog.Revision = epoch_catalog_revision{ 99 };
				},
				"a page cannot cross catalog revision");
			rejectMalformedPage(
				[](auto& aPage, const auto& aFixture) {
					aPage.Epochs = {
						summary(
							aFixture.Authority,
							aFixture.Catalog,
							3,
							10U,
							23U),
						summary(
							aFixture.Authority,
							aFixture.Catalog,
							2,
							10U,
							22U),
						summary(
							aFixture.Authority,
							aFixture.Catalog,
							1,
							10U,
							21U),
					};
				},
				"a page cannot exceed its requested bound");
			rejectMalformedPage(
				[](auto& aPage, const auto& aFixture) {
					aPage.Epochs = {
						summary(
							aFixture.Authority,
							aFixture.Catalog,
							2,
							10U,
							22U),
						summary(
							aFixture.Authority,
							aFixture.Catalog,
							1,
							10U,
							21U),
					};
					aPage.Continuation = history_cursor(aFixture, 2);
				},
				"a continuation must equal the last returned stable ordinal");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto request    = *request_from<load_epoch_history_page_request>(loading);
			const auto exactBound = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = request.Context,
					.Page    = history_page(
						fixture,
						{
							summary(
								fixture.Authority,
								fixture.Catalog,
								2,
								10U,
								22U),
							summary(
								fixture.Authority,
								fixture.Catalog,
								1,
								10U,
								21U),
						}),
				});
			expect(
				exactBound.History && exactBound.History->Epochs.size() == 2U
					&& !exactBound.History->Continuation
					&& !fixture.State.HistoryContinuation,
				"an exact-bound terminal page needs no invented continuation");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto request = *request_from<load_epoch_history_page_request>(loading);
			const auto empty   = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = request.Context,
					.Page    = history_page(fixture, { }),
				});
			expect(
				empty.History && empty.History->Epochs.empty()
					&& !empty.History->Continuation
					&& fixture.State.HistoryCatalog
						== std::optional{ fixture.Catalog },
				"an empty first page is a valid completed history sequence");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto request   = *request_from<load_epoch_history_page_request>(loading);
			const auto malformed = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = request.Context,
					.Page    = history_page(
						fixture,
						{
							summary(
								fixture.Authority,
								fixture.Catalog,
								1,
								10U,
								21U),
							summary(
								fixture.Authority,
								fixture.Catalog,
								2,
								10U,
								22U),
						}),
				});
			expect(
				diagnostic_is(
					malformed, epoch_reason_code::invalid_observation)
					&& malformed.History
					&& malformed.History->Phase
						== epoch_history_page_phase::failed
					&& !fixture.State.PendingHistory
					&& !fixture.State.HistoryCatalog,
				"duplicate or non-descending stable identities fail closed");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto request = *request_from<load_epoch_history_page_request>(loading);
			const auto failed  = transition(
				fixture.State,
				epoch_history_page_load_failed_event{
					.Context = request.Context,
					.Failure = epoch_failure::unavailable,
				});
			expect(
				diagnostic_is(failed, epoch_reason_code::unavailable)
					&& failed.History
					&& failed.History->Phase == epoch_history_page_phase::failed
					&& !fixture.State.PendingHistory,
				"a failed history page produces one terminal and remains retryable");

			const auto retry = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto retryRequest = *request_from<load_epoch_history_page_request>(retry);
			const auto cancelled    = transition(
				fixture.State,
				epoch_history_page_load_cancelled_event{
					.Context = retryRequest.Context,
				});
			expect(
				diagnostic_is(cancelled, epoch_reason_code::cancelled)
					&& cancelled.History
					&& cancelled.History->Phase
						== epoch_history_page_phase::cancelled
					&& !fixture.State.PendingHistory,
				"a cancelled history page produces one terminal and remains retryable");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto request = *request_from<load_epoch_history_page_request>(loading);
			const auto refresh = transition(fixture.State, refresh_epochs_intent{ });
			expect(
				refresh.History
					&& refresh.History->Phase
						== epoch_history_page_phase::cancelled
					&& has_changed(
						refresh.Changed, epoch_changed_domain::history)
					&& has_changed(
						refresh.Changed, epoch_changed_domain::authority)
					&& !fixture.State.PendingHistory,
				"a new epoch generation explicitly cancels the pending page");
			const auto beforeStale = fixture.State;
			const auto stale       = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = request.Context,
					.Page    = history_page(fixture, { }),
				});
			expect(
				diagnostic_is(stale, epoch_reason_code::stale_completion)
					&& fixture.State == beforeStale,
				"a page from the prior generation cannot publish after refresh");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto request     = *request_from<load_epoch_history_page_request>(loading);
			const auto invalidated = transition(
				fixture.State,
				pull_request_revision_invalidated_event{
					.ApplicationSession = fixture.State.ApplicationSession,
					.Authority          = fixture.Authority,
					.Revision           = fixture.Revision,
					.Generation         = fixture.State.EpochGeneration,
				});
			expect(
				invalidated.History
					&& invalidated.History->Phase
						== epoch_history_page_phase::cancelled
					&& !fixture.State.Authority && !fixture.State.Catalog
					&& !fixture.State.HistoryCatalog
					&& !fixture.State.HistoryContinuation,
				"an authority switch clears every history-page binding");
			const auto stale = transition(
				fixture.State,
				epoch_history_page_loaded_event{
					.Context = request.Context,
					.Page    = history_page(fixture, { }),
				});
			expect(
				diagnostic_is(stale, epoch_reason_code::stale_completion),
				"an invalidated authority rejects a late page terminal");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto loading = transition(
				fixture.State,
				request_epoch_history_page_intent{
					.Catalog = fixture.Catalog,
					.Limit   = 2U,
				});
			const auto request  = *request_from<load_epoch_history_page_request>(loading);
			const auto conflict = transition(
				fixture.State,
				epoch_history_page_load_failed_event{
					.Context = request.Context,
					.Failure = epoch_failure::catalog_conflict,
				});
			expect(
				diagnostic_is(conflict, epoch_reason_code::catalog_conflict)
					&& conflict.History
					&& conflict.History->Phase
						== epoch_history_page_phase::cancelled
					&& request_from<load_epoch_catalog_request>(conflict)
						!= nullptr
					&& !fixture.State.Catalog
					&& fixture.State.PendingIngestion
					&& fixture.State.PendingIngestion->Phase
						== epoch_ingestion_phase::loading_catalog,
				"a changed catalog revision retires paging and reloads exact "
				"authority state");
		}
	}

	auto test_core_authorized_epoch_maintenance() -> void
	{
		{
			auto       state  = initial_state();
			const auto result = transition(state,
										   prune_review_epochs_intent{
											   .Catalog = catalog(),
											   .Epochs  = { },
										   });
			expect(diagnostic_is(result, epoch_reason_code::no_active_authority),
				   "maintenance requires an active exact authority");
		}

		{
			auto       fixture       = make_ready_fixture();
			const auto before        = fixture.State;
			auto       wrongCatalog  = fixture.Catalog;
			wrongCatalog.Revision    = epoch_catalog_revision{ 99 };
			const auto wrongRevision = transition(fixture.State,
												  prune_review_epochs_intent{
													  .Catalog = wrongCatalog,
													  .Epochs  = { fixture.Epoch.Id },
												  });
			expect(diagnostic_is(wrongRevision,
								 epoch_reason_code::catalog_revision_mismatch)
					   && fixture.State == before,
				   "maintenance rejects a stale catalog revision");

			const auto empty = transition(fixture.State,
										  prune_review_epochs_intent{
											  .Catalog = fixture.Catalog,
										  });
			expect(
				diagnostic_is(empty, epoch_reason_code::invalid_maintenance_target)
					&& fixture.State == before,
				"prune requires at least one exact epoch");

			auto foreign       = fixture.Epoch.Id;
			foreign.Authority  = authority(2U).Scope;
			const auto crossed = transition(fixture.State,
											prune_review_epochs_intent{
												.Catalog = fixture.Catalog,
												.Epochs  = { foreign },
											});
			expect(diagnostic_is(crossed,
								 epoch_reason_code::invalid_maintenance_target)
					   && fixture.State == before,
				   "prune cannot cross pull-request authority");

			const auto duplicate = transition(fixture.State,
											  prune_review_epochs_intent{
												  .Catalog = fixture.Catalog,
												  .Epochs  = { fixture.Epoch.Id, fixture.Epoch.Id },
											  });
			expect(diagnostic_is(duplicate,
								 epoch_reason_code::invalid_maintenance_target)
					   && fixture.State == before,
				   "prune targets are strictly ordered and unique");

			std::vector<review_epoch_id> oversized;
			oversized.reserve(maximum_epoch_maintenance_target_count + 1U);
			for (std::uint16_t index = 0U;
				 index <= maximum_epoch_maintenance_target_count;
				 ++index) {
				auto id    = fixture.Epoch.Id;
				id.Ordinal = epoch_ordinal{ static_cast<std::int64_t>(index) + 1 };
				oversized.push_back(id);
			}
			const auto tooMany = transition(fixture.State,
											prune_review_epochs_intent{
												.Catalog = fixture.Catalog,
												.Epochs  = std::move(oversized),
											});
			expect(diagnostic_is(tooMany,
								 epoch_reason_code::invalid_maintenance_target)
					   && fixture.State == before,
				   "prune batches are bounded at one hundred exact epochs");
		}

		{
			auto fixture                                          = make_ready_fixture();
			auto second                                           = fixture.Epoch.Id;
			second.Ordinal                                        = epoch_ordinal{ 2 };
			const auto                                    started = transition(fixture.State,
																			   prune_review_epochs_intent{
																				   .Catalog = fixture.Catalog,
																				   .Epochs  = { fixture.Epoch.Id, second },
																			   });
			const auto*                                   request = request_from<perform_epoch_maintenance_request>(started);
			static constexpr std::array<std::uint8_t, 32> expectedDigest{
				0x95U,
				0xcdU,
				0xd8U,
				0x19U,
				0xd2U,
				0x58U,
				0x99U,
				0xfcU,
				0x28U,
				0x39U,
				0xa3U,
				0xfcU,
				0xf2U,
				0xacU,
				0x24U,
				0x1aU,
				0xdaU,
				0xcbU,
				0x4eU,
				0xbeU,
				0xd4U,
				0x45U,
				0x3eU,
				0xd3U,
				0x9eU,
				0x55U,
				0x22U,
				0xdaU,
				0xc4U,
				0xa0U,
				0x28U,
				0xa5U,
			};
			expect(request
					   && request->Context.Kind == epoch_maintenance_kind::prune_epochs
					   && request->Context.Cache == fixture.Authority.Cache
					   && request->Context.Epochs
						   == std::vector{ fixture.Epoch.Id, second }
					   && request->Context.TargetDigest.valid()
					   && request->Context.TargetDigest.bytes() == expectedDigest
					   && fixture.State.PendingMaintenance
					   && fixture.State.PendingMaintenance->EpochCount == 2U
					   && fixture.State.Status == epoch_status::maintaining
					   && !fixture.State.PendingComparison
					   && started.Commands
					   && !started.Commands->Commands[2].Enabled
					   && !started.Commands->Commands[3].Enabled
					   && started.Commands->Commands[5].Enabled
					   && has_changed(started.Changed,
									  epoch_changed_domain::maintenance),
				   "one prune intent binds a bounded exact batch and retires stale "
				   "comparison work while projecting exact maintenance command "
				   "legality");

			const auto pending   = fixture.State;
			const auto duplicate = transition(fixture.State,
											  prune_review_epochs_intent{
												  .Catalog = fixture.Catalog,
												  .Epochs  = { fixture.Epoch.Id },
											  });
			expect(
				diagnostic_is(duplicate, epoch_reason_code::duplicate_maintenance)
					&& fixture.State == pending,
				"only one maintenance action may be pending");

			auto tampered                  = request->Context;
			tampered.Epochs.back().Ordinal = epoch_ordinal{ 3 };
			const auto mismatch            = transition(fixture.State,
														epoch_maintenance_completed_event{
															.Context    = tampered,
															.NewCatalog = catalog(2),
														});
			expect(diagnostic_is(mismatch,
								 epoch_reason_code::completion_identity_mismatch)
					   && fixture.State == pending,
				   "a terminal cannot alter a digest-bound prune target");

			const auto cancelling = transition(fixture.State, cancel_epoch_maintenance_intent{ });
			expect(request_from<cancel_epoch_work_request>(cancelling) != nullptr
					   && fixture.State.PendingMaintenance->CancellationRequested
					   && cancelling.Commands
					   && !cancelling.Commands->Commands[5].Enabled,
				   "maintenance cancellation is an explicit request");
			const auto afterPoint = transition(fixture.State,
											   epoch_maintenance_cancelled_event{
												   .Context  = request->Context,
												   .Progress = epoch_maintenance_progress::after_point_of_no_return,
											   });
			expect(diagnostic_is(afterPoint,
								 epoch_reason_code::cancellation_after_point_of_no_return)
					   && fixture.State.PendingMaintenance,
				   "post-point-of-no-return cancellation cannot terminate "
				   "maintenance");

			const auto completed = transition(fixture.State,
											  epoch_maintenance_completed_event{
												  .Context    = request->Context,
												  .NewCatalog = catalog(2),
											  });
			expect(!fixture.State.PendingMaintenance
					   && fixture.State.Catalog == std::optional{ catalog(2) }
					   && completed.Catalog
					   && completed.Catalog->AvailabilityChangedEpochs
						   == std::vector{ fixture.Epoch.Id, second }
					   && completed.Catalog->AvailabilityChangedTo
						   == anchor_availability::pruned
					   && !completed.Catalog->AllEpochAvailability
					   && request_from<resolve_comparison_request>(completed)
						   != nullptr,
				   "durable prune success wins after the point of no return and "
				   "incrementally marks exact stable IDs");

			const auto stale = transition(fixture.State,
										  epoch_maintenance_completed_event{
											  .Context    = request->Context,
											  .NewCatalog = catalog(2),
										  });
			expect(diagnostic_is(stale, epoch_reason_code::stale_completion),
				   "a duplicate maintenance terminal is inert");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto started = transition(fixture.State,
											delete_repository_cache_intent{
												.Catalog = fixture.Catalog,
												.Cache   = fixture.Authority.Cache,
											});
			const auto request = *request_from<perform_epoch_maintenance_request>(started);
			expect(request.Context.Kind
						   == epoch_maintenance_kind::delete_repository_cache
					   && request.Context.Epochs.empty()
					   && request.Context.TargetDigest.valid(),
				   "cache deletion binds the exact active repository cache");

			const auto completed = transition(fixture.State,
											  epoch_maintenance_completed_event{
												  .Context    = request.Context,
												  .NewCatalog = catalog(2),
											  });
			expect(completed.Catalog
					   && completed.Catalog->AllEpochAvailability
						   == anchor_availability::cache_deleted
					   && completed.Catalog->AvailabilityChangedEpochs.empty(),
				   "whole-cache success publishes one bulk cache-deleted availability "
				   "delta");
		}

		{
			auto       fixture = make_ready_fixture();
			const auto started = transition(fixture.State,
											delete_repository_cache_intent{
												.Catalog = fixture.Catalog,
												.Cache   = fixture.Authority.Cache,
											});
			const auto request = *request_from<perform_epoch_maintenance_request>(started);
			static_cast<void>(
				transition(fixture.State, cancel_epoch_maintenance_intent{ }));
			const auto cancelled = transition(fixture.State,
											  epoch_maintenance_cancelled_event{
												  .Context = request.Context,
											  });
			expect(diagnostic_is(cancelled, epoch_reason_code::cancelled)
					   && !fixture.State.PendingMaintenance
					   && fixture.State.Catalog == std::optional{ fixture.Catalog }
					   && fixture.State.Status == epoch_status::cancelled,
				   "pre-point-of-no-return cancellation preserves the catalog");
		}

		{
			auto       fixture   = make_ready_fixture();
			const auto started   = transition(fixture.State,
											  prune_review_epochs_intent{
												  .Catalog = fixture.Catalog,
												  .Epochs  = { fixture.Epoch.Id },
											  });
			const auto request   = *request_from<perform_epoch_maintenance_request>(started);
			const auto refreshed = transition(fixture.State, refresh_epochs_intent{ });
			expect(request_from<cancel_epoch_work_request>(refreshed) != nullptr
					   && !fixture.State.PendingMaintenance,
				   "refresh retires maintenance from the superseded generation");
			const auto stale = transition(fixture.State,
										  epoch_maintenance_completed_event{
											  .Context    = request.Context,
											  .NewCatalog = catalog(2),
										  });
			expect(diagnostic_is(stale, epoch_reason_code::stale_completion),
				   "maintenance completion from a retired generation cannot publish");
		}
	}

	auto test_authority_access_mode_discrimination() -> void
	{
		// The authority as every existing caller writes it: eleven fields and
		// no mode. It must still mean the authenticated authority it meant
		// before the discriminant existed.
		const auto authenticated = authority();
		expect(authenticated.Mode == github_access_mode::authenticated,
			   "an authority initializer that names no mode is authenticated");
		expect(valid(authenticated),
			   "the authenticated rule is unchanged: an account and a viewer "
			   "make a valid authority");

		auto noAccount    = authenticated;
		noAccount.Account = github_account_id{ };
		expect(!valid(noAccount),
			   "an authenticated authority without an account is invalid");
		auto noViewer   = authenticated;
		noViewer.Viewer = github_viewer_actor_id{ };
		expect(!valid(noViewer),
			   "an authenticated authority without a viewer is invalid");

		// Public preview has no reader to name, so absence is the rule rather
		// than a tolerated shape. Every other field is validated exactly as it
		// is under the authenticated mode.
		auto preview    = authenticated;
		preview.Mode    = github_access_mode::public_preview;
		preview.Account = github_account_id{ };
		preview.Viewer  = github_viewer_actor_id{ };
		expect(valid(preview),
			   "a public-preview authority carrying neither an account nor a "
			   "viewer is valid");
		auto previewWithoutScope  = preview;
		previewWithoutScope.Scope = pull_request_authority_scope_id{ };
		expect(!valid(previewWithoutScope),
			   "public preview relaxes the identity fields and nothing else");

		auto aliasingAccount    = preview;
		aliasingAccount.Account = authenticated.Account;
		expect(!valid(aliasingAccount),
			   "a public-preview authority carrying an account is refused: an "
			   "anonymous read cannot wear an account's identity");
		auto aliasingViewer   = preview;
		aliasingViewer.Viewer = authenticated.Viewer;
		expect(!valid(aliasingViewer),
			   "a public-preview authority carrying a viewer is refused");
		auto aliasingBoth    = preview;
		aliasingBoth.Account = authenticated.Account;
		aliasingBoth.Viewer  = authenticated.Viewer;
		expect(!valid(aliasingBoth),
			   "a public-preview authority carrying a whole account identity "
			   "is refused rather than normalized to an anonymous one");

		auto corruptMode = authenticated;
		corruptMode.Mode = static_cast<github_access_mode>(0x7FU);
		expect(!valid(corruptMode),
			   "a mode outside the enumeration is a corrupt discriminant, not "
			   "a third mode to interpret");

		// The pair the type exists to keep apart: every field agrees,
		// including the account and the viewer, and only the discriminant
		// differs. Neither is a valid authority on its own, which is the
		// point: nothing may collapse them on the way to a key.
		auto crossedModes = authenticated;
		crossedModes.Mode = github_access_mode::public_preview;
		expect(authenticated != crossedModes,
			   "two authorities that differ only in mode are never equal");
		expect((authenticated <=> crossedModes) < 0
				   && (crossedModes <=> authenticated) > 0,
			   "the mode orders two otherwise identical authorities "
			   "deterministically and antisymmetrically");
		expect((crossedModes <=> crossedModes) == 0,
			   "an authority orders equal to itself");

		// One epoch transition that keys on the whole authority. A mode change
		// is an authority change, so identical material does not carry across
		// it and the relation is unknown rather than same.
		expect(material_relation(authenticated, material(), authenticated, material())
				   == epoch_material_relation::same,
			   "one authority observing the same material twice relates same");
		expect(material_relation(preview, material(), preview, material())
				   == epoch_material_relation::same,
			   "a public-preview authority relates its own material the same "
			   "way an authenticated one does");
		expect(material_relation(preview, material(), authenticated, material())
				   == epoch_material_relation::unknown,
			   "a mode change is an authority change, so material observed "
			   "under one mode does not relate to material under the other");
	}

	void test_portable_occurrence_identity()
	{
		const auto                      head = oid<git_commit_oid>(7U);
		const epoch_occurrence_evidence first{ epoch_occurrence_kind::force_push_event, 101U, head };
		auto                            second = first;
		second.EventId                         = 102U;
		const auto firstId                     = make_epoch_occurrence_id("github.com", pull_request_id{ 42U }, first);
		const auto secondId                    = make_epoch_occurrence_id("github.com", pull_request_id{ 42U }, second);
		const auto observed                    = make_epoch_occurrence_id("github.com", pull_request_id{ 42U }, { epoch_occurrence_kind::observed_head, 0U, head });
		expect(firstId && secondId && observed && firstId != secondId && firstId != observed,
			   "A to B to A retains distinct event occurrences and a separate observed-head namespace");
		expect(firstId == make_epoch_occurrence_id("github.com", pull_request_id{ 42U }, first), "portable digest is repeatable");
		expect(firstId != make_epoch_occurrence_id("ghe.example.com", pull_request_id{ 42U }, first)
				   && firstId != make_epoch_occurrence_id("github.com", pull_request_id{ 43U }, first),
			   "host and provider PR identity partition occurrences");
		auto inconsistent = first;
		inconsistent.Head = oid<git_commit_oid>(8U);
		expect(firstId == make_epoch_occurrence_id("github.com", pull_request_id{ 42U }, inconsistent), "a conflicting after-head cannot mint a second identity for one event");
		for (const auto hostname : { "GitHub.com", "github.com.", "a..b", "-a.com", "a-.com", "https://github.com" }) {
			expect(!make_epoch_occurrence_id(hostname, pull_request_id{ 42U }, first), "noncanonical hostname is refused");
		}
		expect(!make_epoch_occurrence_id("github.com", pull_request_id{ 42U }, { epoch_occurrence_kind::force_push_event, 0U, head })
				   && !make_epoch_occurrence_id("github.com", pull_request_id{ 42U }, { epoch_occurrence_kind::observed_head, 1U, head }),
			   "typed evidence refuses fabricated discriminators");
		epoch_occurrence left{ .Id = *firstId, .Evidence = first, .DerivedComparisonBase = oid<git_commit_oid>(9U), .DateSource = epoch_occurrence_date_source::github_event, .DateUtcMilliseconds = 500 };
		auto             right = left;
		right.Id               = *secondId;
		right.Evidence         = second;
		expect(valid(left) && !left.HistoricalBase && left.DerivedComparisonBase, "head-only history never promotes a derived base to historical truth");
		expect(epoch_occurrence_less(left, right) == (left.Id < right.Id), "equal dates use stable portable identity");
		right.DateSource          = epoch_occurrence_date_source::commit_date;
		right.DateUtcMilliseconds = 400;
		expect(epoch_occurrence_less(right, left), "mixed dates have deterministic display order without inferring push order");
		right.DateSource          = epoch_occurrence_date_source::unavailable;
		right.DateUtcMilliseconds = 0;
		expect(epoch_occurrence_less(left, right), "undated history follows dated history");
		right.FirstSeen = observed_utc_milliseconds{ 900 };
		expect(epoch_occurrence_less(left, right), "local first-seen cannot affect portable ordering");
	}

	void test_historical_append_planner()
	{
		const auto       live     = revision();
		const auto       original = live;
		epoch_occurrence occurrence{
			.Evidence = { epoch_occurrence_kind::force_push_event, 501U, oid<git_commit_oid>(80U) },
		};
		occurrence.Id = *make_epoch_occurrence_id("github.com", authority().PullRequest, occurrence.Evidence);
		const epoch_preparation_observation observation{
			.ProposedOrdinal = epoch_ordinal{ 4 },
			.ObservedAt      = observed_utc_milliseconds{ 100 },
			.Material        = material(),
		};
		const auto planned = plan_historical_epoch_mutation(authority(), live, revision(2).Source, catalog(), occurrence, oid<git_commit_oid>(90U), observation);
		expect(planned.Mutation && live == original && !occurrence.HistoricalBase, "historical planning leaves live revision and unknown historical base intact");
		const auto& append = std::get<append_epoch_mutation>(*planned.Mutation);
		expect(append.Epoch.Head == occurrence.Evidence.Head
				   && append.Epoch.Base == oid<git_commit_oid>(90U) && append.Epoch.Cause.Origin == epoch_origin::explicit_historical_import,
			   "historical mutation names exact occurrence and derived comparison anchor");
		expect(!plan_historical_epoch_mutation(authority(), live, revision(2).Source, catalog(), occurrence, { }, observation).Mutation,
			   "missing derived base leaves history without a fabricated comparison");
		occurrence.LocalEpoch = append.Epoch.Id;
		expect(!plan_historical_epoch_mutation(authority(), live, revision(2).Source, catalog(), occurrence, oid<git_commit_oid>(90U), observation).Mutation,
			   "already retained occurrence does not allocate another epoch");
	}

	void test_derived_comparison_invalidation()
	{
		auto live        = revision();
		auto prior       = summary(authority(), catalog(), 1, 10U, 20U);
		prior.Base       = live.Base;
		prior.BaseBranch = live.BaseBranch;
		expect(!historical_comparison_selection_changed(live, prior), "same live selection retains derived comparisons");
		expect(historical_comparison_selection_changed(live, std::nullopt), "unobserved selected-base provenance cannot expose an old comparison");
		live.Head = oid<git_commit_oid>(77U);
		expect(!historical_comparison_selection_changed(live, prior), "head-only updates retain derived comparisons");
		live.Base = oid<git_commit_oid>(78U);
		expect(historical_comparison_selection_changed(live, prior), "base OID replacement invalidates comparisons");
		live.Base       = prior.Base;
		live.BaseBranch = opaque<base_branch_id>(79U);
		expect(historical_comparison_selection_changed(live, prior), "base branch change invalidates comparisons");
		live.Origin = pull_request_revision_origin::explicit_historical_import;
		expect(!historical_comparison_selection_changed(live, prior), "historical append cannot invalidate live-selected comparisons");
		epoch_occurrence imported{ .LocalEpoch = prior.Id, .DerivedComparisonBase = prior.Base };
		imported.Evidence.Kind = epoch_occurrence_kind::force_push_event;
		auto ambiguous         = imported;
		ambiguous.DerivedComparisonBase.reset();
		epoch_occurrence observed{ .LocalEpoch = prior.Id, .HistoricalBase = prior.Base };
		auto             observedUnknown = observed;
		observedUnknown.HistoricalBase.reset();
		epoch_occurrence_history history{ .Occurrences = { imported, ambiguous, observed, observedUnknown } };
		invalidate_derived_comparison_bindings(history);
		imported.LocalEpoch.reset();
		imported.DerivedComparisonBase.reset();
		ambiguous.LocalEpoch.reset();
		expect(history.Occurrences == std::vector{ imported, ambiguous, observed, observedUnknown },
			   "invalidation masks unknown-base event links even without a derived base and preserves every observed-head link");
	}

	void test_historical_catalog_advance()
	{
		auto fixture     = make_ready_fixture();
		auto next        = fixture.Catalog;
		next.Revision    = epoch_catalog_revision{ next.Revision.value() + 1U };
		const auto event = epoch_historical_catalog_advanced_event{
			.ApplicationSession = fixture.State.ApplicationSession,
			.Authority          = fixture.Authority,
			.Revision           = fixture.Revision,
			.ExpectedCatalog    = fixture.Catalog,
			.NewCatalog         = next,
		};
		const auto before              = fixture.State;
		auto       stale               = event;
		stale.ExpectedCatalog.Revision = next.Revision;
		const auto rejected            = transition(fixture.State, stale);
		expect(diagnostic_is(rejected, epoch_reason_code::stale_completion)
				   && fixture.State == before,
			   "historical completion cannot advance an unrelated catalog revision");
		const auto accepted = transition(fixture.State, event);
		expect(fixture.State.Catalog == next && accepted.Catalog
				   && accepted.Catalog->Catalog == next
				   && request_from<resolve_comparison_request>(accepted) != nullptr,
			   "durable historical append advances core revision and re-resolves the selection");
		const auto selected = transition(fixture.State,
										 select_comparison_anchor_intent{
											 .Side   = comparison_side::left,
											 .Anchor = {
												 .Authority   = fixture.Authority.Scope,
												 .Incarnation = next.Incarnation,
												 .Kind        = comparison_anchor_kind::epoch_head,
												 .Epoch       = fixture.Epoch.Id.Ordinal,
											 },
											 .ExpectedCatalogRevision = next.Revision,
										 });
		expect(!diagnostic_is(selected, epoch_reason_code::catalog_revision_mismatch),
			   "the newly published historical line can dispatch against the adopted revision");
	}

	auto test_pending_occurrence_catalog_rebase() -> void
	{
		auto       fixture       = make_ready_fixture();
		const auto originalPair  = fixture.State.AcceptedComparison;
		const auto first         = transition(fixture.State, select_epoch_occurrence_intent{ comparison_side::left, opaque<epoch_occurrence_id>(90), fixture.Catalog });
		const auto firstContext  = request_from<import_epoch_occurrence_request>(first)->Context;
		const auto second        = transition(fixture.State, select_epoch_occurrence_intent{ comparison_side::right, opaque<epoch_occurrence_id>(91), fixture.Catalog });
		const auto secondContext = request_from<import_epoch_occurrence_request>(second)->Context;
		const auto selection     = fixture.State.RequestedSelection;
		auto       next          = fixture.Catalog;
		next.Revision            = epoch_catalog_revision{ next.Revision.value() + 1U };
		const auto stale         = transition(fixture.State, epoch_occurrence_imported_event{ firstContext, next, originalPair->Left });
		expect(diagnostic_is(stale, epoch_reason_code::stale_completion) && fixture.State.PendingOccurrence->Occurrence == secondContext.Occurrence,
			   "late A success does not replace pending B");
		const auto  advanced  = transition(fixture.State, epoch_historical_catalog_advanced_event{ fixture.State.ApplicationSession, fixture.Authority, fixture.Revision, fixture.Catalog, next });
		const auto* rebased   = request_from<import_epoch_occurrence_request>(advanced);
		const auto* cancelled = request_from<cancel_epoch_work_request>(advanced);
		expect(rebased && cancelled && cancelled->Request == secondContext.Request
				   && rebased->Context.Request != secondContext.Request && rebased->Context.ComparisonGeneration > secondContext.ComparisonGeneration
				   && rebased->Context.Occurrence == secondContext.Occurrence && rebased->Context.Side == secondContext.Side
				   && rebased->Context.ExpectedCatalog == next && rebased->Context.Authority == secondContext.Authority
				   && rebased->Context.Revision == secondContext.Revision && rebased->Context.ApplicationSession == secondContext.ApplicationSession
				   && rebased->Context.Generation == secondContext.Generation,
			   "durable A catalog advance retires old B and reissues exact B with fresh request and comparison generation");
		expect(fixture.State.Catalog == next && fixture.State.RequestedSelection == selection
				   && fixture.State.AcceptedComparison == originalPair && !advanced.Selection && !advanced.Comparison
				   && !request_from<resolve_comparison_request>(advanced),
			   "catalog rebase preserves selection without inventing a historical anchor");
		const auto pending = fixture.State;
		expect(diagnostic_is(transition(fixture.State, epoch_occurrence_import_failed_event{ secondContext }), epoch_reason_code::stale_completion)
				   && fixture.State == pending,
			   "old B terminal cannot consume rebased B");
	}

	auto test_targeted_occurrence_selection() -> void
	{
		for (const auto side : { comparison_side::left, comparison_side::right }) {
			auto        fixture      = make_ready_fixture();
			const auto  originalPair = *fixture.State.AcceptedComparison;
			const auto  intent       = select_epoch_occurrence_intent{ side, opaque<epoch_occurrence_id>(90), *fixture.State.Catalog };
			const auto  output       = transition(fixture.State, intent);
			const auto* request      = request_from<import_epoch_occurrence_request>(output);
			expect(request && request->Context.Occurrence == intent.Occurrence && request->Context.Side == side,
				   "unimported occurrence emits one exact typed import request");
			auto head             = originalPair.Right;
			head.Identity.Epoch   = epoch_ordinal{ 42 };
			auto next             = *fixture.State.Catalog;
			next.Revision         = epoch_catalog_revision{ next.Revision.value() + 1 };
			const auto completion = epoch_occurrence_imported_event{ request->Context, next, head };
			for (int mismatch = 0; mismatch < 8; ++mismatch) {
				auto bad = completion;
				if (mismatch == 0) {
					bad.Context.Occurrence = opaque<epoch_occurrence_id>(91);
				}
				if (mismatch == 1) {
					bad.Context.Request = epoch_request_id{ 999 };
				}
				if (mismatch == 2) {
					bad.Context.Authority = authority(2);
				}
				if (mismatch == 3) {
					bad.Context.ComparisonGeneration = comparison_generation{ 999 };
				}
				if (mismatch == 4) {
					bad.Context.Generation = epoch_generation{ 999 };
				}
				if (mismatch == 5) {
					bad.Context.ExpectedCatalog.Revision = next.Revision;
				}
				if (mismatch == 6) {
					bad.Context.Side = side == comparison_side::left ? comparison_side::right : comparison_side::left;
				}
				if (mismatch == 7) {
					bad.Context.ApplicationSession = application_session_id{ 999 };
				}
				auto       state    = fixture.State;
				const auto rejected = transition(state, bad);
				expect(state == fixture.State && diagnostic_is(rejected, epoch_reason_code::stale_completion),
					   "mismatched occurrence completion cannot mutate pending selection");
			}
			for (int mismatch = 0; mismatch < 4; ++mismatch) {
				auto bad = completion;
				if (mismatch == 0) {
					bad.NewCatalog = intent.ExpectedCatalog;
				}
				if (mismatch == 1) {
					bad.Head.Identity.Kind = comparison_anchor_kind::epoch_base;
				}
				if (mismatch == 2) {
					bad.Head.Availability = anchor_availability::unavailable;
				}
				if (mismatch == 3) {
					bad.Head.Identity.Authority = authority(2).Scope;
				}
				auto       state    = fixture.State;
				const auto rejected = transition(state, bad);
				expect(!state.PendingOccurrence && state.AcceptedComparison == originalPair
						   && diagnostic_is(rejected, epoch_reason_code::completion_identity_mismatch),
					   "invalid imported anchor or non-advanced catalog terminates without selecting");
			}
			auto failedState = fixture.State;
			static_cast<void>(transition(failedState, epoch_occurrence_import_failed_event{ request->Context, epoch_failure::unavailable }));
			expect(!failedState.PendingOccurrence && failedState.AcceptedComparison == originalPair,
				   "import failure preserves accepted comparison");
			auto       superseded  = fixture.State;
			const auto replacement = transition(superseded, select_named_comparison_intent{ named_comparison::whole_pr, intent.ExpectedCatalog.Revision });
			expect(request_from<cancel_epoch_work_request>(replacement) && !superseded.PendingOccurrence,
				   "ordinary comparison choice cancels the pending occurrence import");
			const auto late = transition(superseded, completion);
			expect(diagnostic_is(late, epoch_reason_code::stale_completion), "superseded success cannot select");
			const auto  accepted = transition(fixture.State, completion);
			const auto* resolve  = request_from<resolve_comparison_request>(accepted);
			expect(resolve && fixture.State.Catalog == next && !fixture.State.PendingOccurrence,
				   "successful durable import immediately resolves the requested pair");
			const auto pair = std::get<explicit_comparison_selection>(resolve->Context.Selection);
			expect((side == comparison_side::left ? pair.Left : pair.Right) == head.Identity
					   && (side == comparison_side::left ? pair.Right : pair.Left) == (side == comparison_side::left ? originalPair.Right.Identity : originalPair.Left.Identity),
				   "completion selects imported head on requested side and preserves opposite anchor");
			const auto resolved = transition(fixture.State, comparison_resolved_event{ .Context = resolve->Context, .Candidates = { .Catalog = next, .ExplicitLeft = side == comparison_side::left ? head : originalPair.Left, .ExplicitRight = side == comparison_side::right ? head : originalPair.Right } });
			expect(resolved.Comparison && resolved.Comparison->Pair && fixture.State.AcceptedComparison
					   && (side == comparison_side::left ? fixture.State.AcceptedComparison->Left : fixture.State.AcceptedComparison->Right) == head,
				   "the imported head becomes the accepted comparison without a second gesture");
			expect(diagnostic_is(transition(fixture.State, completion), epoch_reason_code::stale_completion),
				   "duplicate import completion cannot apply twice");
		}
		auto fixture = make_ready_fixture();
		auto intent  = select_epoch_occurrence_intent{ comparison_side::left, { }, *fixture.State.Catalog };
		expect(diagnostic_is(transition(fixture.State, intent), epoch_reason_code::invalid_selection), "empty occurrence is refused");
		intent.Occurrence               = opaque<epoch_occurrence_id>(90);
		intent.ExpectedCatalog.Revision = epoch_catalog_revision{ 999 };
		expect(diagnostic_is(transition(fixture.State, intent), epoch_reason_code::catalog_revision_mismatch), "stale occurrence gesture is refused");
	}

	auto test_deterministic_replay() -> void
	{
		const auto first  = make_ready_fixture();
		const auto second = make_ready_fixture();
		expect(
			first == second,
			"same bounded observations replay to byte-equivalent semantic state");
	}
} // namespace

auto main() -> int
{
	test_git_tool_failure_reasons();
	test_sha256_known_vectors();
	test_portable_occurrence_identity();
	test_binary_oid_types_and_compact_state();
	test_preparation_effect_policy();
	test_cause_precedence_and_material_policy();
	test_retention_rehydration_policy();
	test_association_only_policy();
	test_alias_resolution_and_exact_pairs();
	test_prepare_commit_visibility_and_linearization();
	test_commit_payload_binding_and_cancellation_point();
	test_catalog_conflict_reloads_catalog();
	test_current_malformed_terminals_close_work();
	test_noop_discard_and_exact_invalidation();
	test_association_only_commit_transition();
	test_fail_closed_comparison_exhaustion();
	test_invalid_cross_facts_and_command_policy();
	test_selection_transition_policy();
	test_whole_pr_compares_only_from_a_named_merge_base();
	test_pending_one_sided_selection_supersession();
	test_overflow_is_checked_and_invalidation_fails_closed();
	test_bounded_epoch_history_paging();
	test_settled_ingestion_requests_the_newest_history_page();
	test_core_authorized_epoch_maintenance();
	test_authority_access_mode_discrimination();
	test_historical_append_planner();
	test_derived_comparison_invalidation();
	test_historical_catalog_advance();
	test_pending_occurrence_catalog_rebase();
	test_targeted_occurrence_selection();
	test_deterministic_replay();
	return EXIT_SUCCESS;
}
