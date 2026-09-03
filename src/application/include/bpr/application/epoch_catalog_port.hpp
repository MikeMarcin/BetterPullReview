#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_CATALOG_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_CATALOG_PORT_HPP

#include "bpr/application/git_cache_port.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <variant>
#include <vector>

namespace bpr::application {
	inline constexpr std::uint16_t maximum_epoch_history_page_size   = core::maximum_epoch_history_page_size;
	inline constexpr std::uint16_t maximum_epoch_recovery_page_size  = 100U;
	inline constexpr std::size_t   maximum_exact_anchor_lookup_count = 100U;
	inline constexpr std::size_t   maximum_prune_epoch_count         = 100U;
	inline constexpr std::size_t   maximum_cache_delete_scope_count  = 10'000U;
	inline constexpr std::uint32_t
		maximum_retained_epoch_count_per_pull_request = maximum_git_cache_private_ref_count / 2U;
	static_assert(maximum_git_cache_private_ref_count % 2U == 0U);

	struct epoch_catalog_operation_revision_tag;
	using epoch_catalog_operation_revision = core::integral_identity<
		epoch_catalog_operation_revision_tag,
		std::int64_t>;
	inline constexpr auto maximum_epoch_catalog_operation_revision = std::numeric_limits<std::int64_t>::max();

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const epoch_catalog_operation_revision aRevision) noexcept -> bool
	{
		return aRevision.value() > 0
			&& aRevision.value()
			<= maximum_epoch_catalog_operation_revision;
	}

	/// \brief Reports whether the can advance condition holds.
	[[nodiscard]] constexpr auto can_advance(
		const epoch_catalog_operation_revision aRevision) noexcept -> bool
	{
		return application::valid(aRevision)
			&& aRevision.value()
			< maximum_epoch_catalog_operation_revision;
	}

	/// \brief Reports whether the can advance nonterminal condition holds.
	[[nodiscard]] constexpr auto can_advance_nonterminal(
		const epoch_catalog_operation_revision aRevision) noexcept -> bool
	{
		return application::valid(aRevision)
			&& aRevision.value()
			< maximum_epoch_catalog_operation_revision - 1;
	}

	/// \brief Describes the epoch catalog authority key record.
	struct epoch_catalog_authority_key
	{
		core::host_profile_id            Profile;
		core::github_profile_revision    ProfileRevision;
		core::git_host_id                Host;
		core::host_authority_fingerprint HostAuthority;
		core::github_account_id          Account;
		core::github_viewer_actor_id     Viewer;
		core::repository_id              Repository;
		core::pull_request_id            PullRequest;
		core::pull_request_number        Number;
		core::git_hash_algorithm         ObjectFormat = core::git_hash_algorithm::invalid;
		// Last and defaulted so every key written before the mode existed
		// still names the authenticated authority it named, and part of the
		// defaulted comparison so no derived key can drop the discriminant.
		core::github_access_mode Mode{ };

		auto operator<=>(const epoch_catalog_authority_key&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const epoch_catalog_authority_key& aKey) noexcept -> bool
	{
		// Two rules, one per mode. Public preview has no reader, so its key
		// must carry none; the authenticated mode must carry an exact one.
		// A key can therefore never be valid under both readings.
		const auto readerAsExpected = aKey.Mode
				== core::github_access_mode::public_preview
			? (aKey.Account.value() == 0U && aKey.Viewer.value() == 0U)
			: (aKey.Account.value() != 0U && aKey.Viewer.value() != 0U);
		return aKey.Profile.value() != 0U
			&& aKey.ProfileRevision.value() != 0U
			&& aKey.Host.value() != 0U && aKey.HostAuthority.valid()
			&& readerAsExpected
			&& aKey.Repository.value() != 0U
			&& aKey.PullRequest.value() != 0U && aKey.Number.valid()
			&& (aKey.ObjectFormat == core::git_hash_algorithm::sha1
				|| aKey.ObjectFormat == core::git_hash_algorithm::sha256);
	}

	/// \brief Describes the epoch catalog open request record.
	struct epoch_catalog_open_request
	{
		core::application_session_id ApplicationSession;
		epoch_catalog_authority_key  Authority;

		auto operator<=>(const epoch_catalog_open_request&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const epoch_catalog_open_request& aRequest) noexcept -> bool
	{
		return aRequest.ApplicationSession.value() != 0U
			&& valid(aRequest.Authority);
	}

	/// \brief Describes the epoch catalog binding record.
	struct epoch_catalog_binding
	{
		core::pull_request_authority Authority;
		core::epoch_catalog_identity Catalog;
		core::git_hash_algorithm     ObjectFormat = core::git_hash_algorithm::invalid;

		auto operator<=>(const epoch_catalog_binding&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const epoch_catalog_binding& aBinding) noexcept -> bool
	{
		return core::valid(aBinding.Authority)
			&& core::valid(aBinding.Catalog)
			&& (aBinding.ObjectFormat == core::git_hash_algorithm::sha1
				|| aBinding.ObjectFormat == core::git_hash_algorithm::sha256);
	}

	/// \brief Describes the epoch catalog snapshot record.
	struct epoch_catalog_snapshot
	{
		epoch_catalog_binding Binding;
		// The exact next never-used ordinal. An empty value means the durable
		// ordinal space is exhausted; callers must not infer this from live
		// history because aborted reservations intentionally leave gaps.
		std::optional<core::epoch_ordinal>              NextOrdinal;
		std::optional<core::review_epoch_summary>       LatestLive;
		std::optional<core::review_epoch_summary>       PreviousDistinctHead;
		std::optional<core::catalog_review_association> CurrentViewerReview;
		core::retention_rehydration_requirement         RetentionRehydration = core::retention_rehydration_requirement::not_required;

		auto operator<=>(const epoch_catalog_snapshot&) const = default;
	};

	/// \brief Describes the projected append head record.
	struct projected_append_head
	{
		core::epoch_ordinal  Ordinal;
		core::git_commit_oid Head;

		auto operator<=>(const projected_append_head&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const projected_append_head& aProjected) noexcept -> bool
	{
		return core::valid(aProjected.Ordinal) && aProjected.Head.valid();
	}

	/// \brief Describes the canonical review request record.
	struct canonical_review_request
	{
		core::epoch_catalog_identity          Catalog;
		core::current_viewer_review_candidate Candidate;
		core::review_association_id           ProposedAssociation;
		std::optional<projected_append_head>  ProjectedHead;

		auto operator<=>(const canonical_review_request&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const canonical_review_request& aRequest) noexcept -> bool
	{
		const auto& candidate = aRequest.Candidate;
		return core::valid(aRequest.Catalog)
			&& aRequest.ProposedAssociation.valid()
			&& candidate.Review.value() != 0U
			&& candidate.Actor.value() != 0U && core::valid(candidate.Order)
			&& candidate.State != core::github_review_state::pending
			&& static_cast<std::uint8_t>(candidate.State)
			<= static_cast<std::uint8_t>(
				   core::github_review_state::dismissed)
			&& candidate.Commit.valid()
			&& (!aRequest.ProjectedHead
				|| (application::valid(*aRequest.ProjectedHead)
					&& aRequest.ProjectedHead->Head.algorithm()
						== candidate.Commit.algorithm()));
	}

	/// \brief Describes the exact anchor lookup request record.
	struct exact_anchor_lookup_request
	{
		core::epoch_catalog_identity                  Catalog;
		std::vector<core::comparison_anchor_identity> Anchors;

		auto operator<=>(const exact_anchor_lookup_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const exact_anchor_lookup_request& aRequest) noexcept -> bool
	{
		if (
			!core::valid(aRequest.Catalog) || aRequest.Anchors.empty()
			|| aRequest.Anchors.size() > maximum_exact_anchor_lookup_count) {
			return false;
		}
		for (std::size_t index = 0U; index < aRequest.Anchors.size(); ++index) {
			const auto& anchor = aRequest.Anchors[index];
			if (
				!core::valid(anchor)
				|| anchor.Incarnation != aRequest.Catalog.Incarnation
				|| (index != 0U
					&& !(aRequest.Anchors[index - 1U] < anchor))) {
				return false;
			}
		}
		return true;
	}

	/// \brief Describes the exact anchor lookup result record.
	struct exact_anchor_lookup_result
	{
		core::epoch_catalog_identity         Catalog;
		std::vector<core::comparison_anchor> Anchors;

		auto operator<=>(const exact_anchor_lookup_result&) const = default;
	};

	/// \brief Describes the anchor availability change record.
	struct anchor_availability_change
	{
		core::comparison_anchor_identity Anchor;
		core::git_commit_oid             ExpectedCommit;
		core::anchor_availability        Expected = core::anchor_availability::unavailable;
		core::anchor_availability        Next     = core::anchor_availability::corrupt;

		auto operator<=>(const anchor_availability_change&) const = default;
	};

	/// \brief Describes the mark anchor availability request record.
	struct mark_anchor_availability_request
	{
		core::epoch_catalog_identity            Catalog;
		std::vector<anchor_availability_change> Changes;

		auto operator<=>(const mark_anchor_availability_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const mark_anchor_availability_request& aRequest) noexcept -> bool
	{
		if (
			!core::valid(aRequest.Catalog) || aRequest.Changes.empty()
			|| aRequest.Changes.size() > maximum_exact_anchor_lookup_count) {
			return false;
		}
		for (std::size_t index = 0U; index < aRequest.Changes.size(); ++index) {
			const auto& change = aRequest.Changes[index];
			if (
				!core::valid(change.Anchor)
				|| change.Anchor.Incarnation
					!= aRequest.Catalog.Incarnation
				|| !change.ExpectedCommit.valid()
				|| static_cast<std::uint8_t>(change.Expected)
					> static_cast<std::uint8_t>(
						core::anchor_availability::never_observed)
				|| (!(
						change.Expected
							== core::anchor_availability::available
						&& (change.Next
								== core::anchor_availability::unavailable
							|| change.Next
								== core::anchor_availability::corrupt))
					&& !(
						change.Expected
							== core::anchor_availability::unavailable
						&& change.Next
							== core::anchor_availability::corrupt))
				|| (index != 0U
					&& !(aRequest.Changes[index - 1U].Anchor
						 < change.Anchor))) {
				return false;
			}
		}
		return true;
	}

	using epoch_history_cursor = core::epoch_history_cursor;

	/// \brief Describes the epoch history page request record.
	struct epoch_history_page_request
	{
		core::epoch_catalog_identity        Catalog;
		std::optional<epoch_history_cursor> Cursor;
		std::uint16_t                       Limit = maximum_epoch_history_page_size;

		auto operator<=>(const epoch_history_page_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const epoch_catalog_binding&      aBinding,
		const epoch_history_page_request& aRequest) noexcept -> bool
	{
		if (
			!valid(aBinding) || !core::valid(aRequest.Catalog)
			|| aRequest.Catalog != aBinding.Catalog || aRequest.Limit == 0U
			|| aRequest.Limit > maximum_epoch_history_page_size) {
			return false;
		}
		return !aRequest.Cursor
			|| (core::valid(*aRequest.Cursor)
				&& aRequest.Cursor->Authority == aBinding.Authority.Scope
				&& aRequest.Cursor->Incarnation
					== aBinding.Catalog.Incarnation
				&& aRequest.Cursor->Revision == aBinding.Catalog.Revision);
	}

	/// \brief Describes the epoch history page record.
	struct epoch_history_page
	{
		core::pull_request_authority_scope_id   Authority;
		core::epoch_catalog_identity            Catalog;
		std::vector<core::review_epoch_summary> Epochs;
		std::optional<epoch_history_cursor>     Continuation;

		auto operator<=>(const epoch_history_page&) const = default;
	};

	enum class epoch_catalog_operation_kind : std::uint8_t {
		append       = 1U,
		prune        = 2U,
		cache_delete = 3U,
	};

	enum class epoch_catalog_operation_phase : std::uint8_t {
		preparing                          = 1U,
		spool_recorded                     = 2U,
		imported                           = 3U,
		staging_ready                      = 4U,
		commit_authorized                  = 5U,
		ref_commit_in_progress             = 6U,
		refs_committed                     = 7U,
		visible_cleanup_pending            = 8U,
		complete                           = 9U,
		abort_requested                    = 10U,
		aborted                            = 11U,
		repair_required                    = 12U,
		prune_intent                       = 13U,
		ref_delete_in_progress             = 14U,
		cache_delete_building_exact_set    = 15U,
		cache_delete_intent_sealed         = 16U,
		quarantine_rename_in_progress      = 17U,
		quarantined                        = 18U,
		catalog_tombstoned_cleanup_pending = 19U,
	};

	/// \brief Reports whether the terminal condition holds.
	[[nodiscard]] constexpr auto terminal(
		const epoch_catalog_operation_phase aPhase) noexcept -> bool
	{
		return aPhase == epoch_catalog_operation_phase::complete
			|| aPhase == epoch_catalog_operation_phase::aborted
			|| aPhase == epoch_catalog_operation_phase::repair_required;
	}

	enum class epoch_catalog_transition_evidence : std::uint8_t {
		catalog_transaction       = 1U,
		exact_staging_pair        = 2U,
		core_commit_authorization = 3U,
		exact_final_pair          = 4U,
		exact_cleanup_complete    = 5U,
		exact_refs_absent         = 6U,
		exact_quarantine          = 7U,
		repair_ambiguity          = 8U,
	};

	/// \brief Computes the point of no cancellation result.
	[[nodiscard]] constexpr auto point_of_no_cancellation(
		const epoch_catalog_operation_phase aPhase) noexcept -> bool
	{
		return aPhase == epoch_catalog_operation_phase::ref_commit_in_progress
			|| aPhase == epoch_catalog_operation_phase::refs_committed
			|| aPhase
			== epoch_catalog_operation_phase::visible_cleanup_pending
			|| aPhase == epoch_catalog_operation_phase::complete
			|| aPhase
			== epoch_catalog_operation_phase::ref_delete_in_progress
			|| aPhase
			== epoch_catalog_operation_phase::
				quarantine_rename_in_progress
			|| aPhase == epoch_catalog_operation_phase::quarantined
			|| aPhase
			== epoch_catalog_operation_phase::
				catalog_tombstoned_cleanup_pending;
	}

	/// \brief Computes the valid phase result.
	[[nodiscard]] constexpr auto valid_phase(
		const epoch_catalog_operation_kind  aKind,
		const epoch_catalog_operation_phase aPhase) noexcept -> bool
	{
		using enum epoch_catalog_operation_phase;

		if (aPhase == repair_required || aPhase == complete) {
			return true;
		}
		switch (aKind) {
		case epoch_catalog_operation_kind::append:
			return aPhase == preparing || aPhase == spool_recorded
				|| aPhase == imported || aPhase == staging_ready
				|| aPhase == commit_authorized
				|| aPhase == ref_commit_in_progress
				|| aPhase == refs_committed
				|| aPhase == visible_cleanup_pending
				|| aPhase == abort_requested || aPhase == aborted;
		case epoch_catalog_operation_kind::prune:
			return aPhase == prune_intent
				|| aPhase == ref_delete_in_progress
				|| aPhase == visible_cleanup_pending
				|| aPhase == abort_requested || aPhase == aborted;
		case epoch_catalog_operation_kind::cache_delete:
			return aPhase == cache_delete_building_exact_set
				|| aPhase == cache_delete_intent_sealed
				|| aPhase == quarantine_rename_in_progress
				|| aPhase == quarantined
				|| aPhase == catalog_tombstoned_cleanup_pending
				|| aPhase == abort_requested || aPhase == aborted;
		}
		return false;
	}

	/// \brief Computes the valid repair origin result.
	[[nodiscard]] constexpr auto valid_repair_origin(
		const epoch_catalog_operation_kind                 aKind,
		const epoch_catalog_operation_phase                aPhase,
		const std::optional<epoch_catalog_operation_phase> aOrigin) noexcept
		-> bool
	{
		if (
			(aPhase == epoch_catalog_operation_phase::repair_required)
			!= aOrigin.has_value()) {
			return false;
		}
		return !aOrigin
			|| (valid_phase(aKind, *aOrigin)
				&& *aOrigin
					!= epoch_catalog_operation_phase::repair_required
				&& *aOrigin != epoch_catalog_operation_phase::complete
				&& *aOrigin != epoch_catalog_operation_phase::aborted);
	}

	/// \brief Computes the valid phase transition result.
	[[nodiscard]] constexpr auto valid_phase_transition(
		const epoch_catalog_operation_kind  aKind,
		const epoch_catalog_operation_phase aFrom,
		const epoch_catalog_operation_phase aTo) noexcept -> bool
	{
		if (!valid_phase(aKind, aFrom) || !valid_phase(aKind, aTo)) {
			return false;
		}
		if (aFrom == aTo) {
			return true;
		}
		if (
			aTo == epoch_catalog_operation_phase::repair_required
			&& aFrom != epoch_catalog_operation_phase::complete
			&& aFrom != epoch_catalog_operation_phase::aborted) {
			return true;
		}

		switch (aKind) {
		case epoch_catalog_operation_kind::append:
			switch (aFrom) {
			case epoch_catalog_operation_phase::preparing:
				return aTo == epoch_catalog_operation_phase::spool_recorded
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::spool_recorded:
				return aTo == epoch_catalog_operation_phase::imported
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::imported:
				return aTo == epoch_catalog_operation_phase::staging_ready
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::staging_ready:
				return aTo
					== epoch_catalog_operation_phase::commit_authorized
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::commit_authorized:
				return aTo
					== epoch_catalog_operation_phase::
						ref_commit_in_progress
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::ref_commit_in_progress:
				return aTo == epoch_catalog_operation_phase::refs_committed;
			case epoch_catalog_operation_phase::refs_committed:
				return aTo
					== epoch_catalog_operation_phase::
						visible_cleanup_pending;
			case epoch_catalog_operation_phase::visible_cleanup_pending:
				return aTo == epoch_catalog_operation_phase::complete;
			case epoch_catalog_operation_phase::abort_requested:
				return aTo == epoch_catalog_operation_phase::aborted;
			default:
				return false;
			}
		case epoch_catalog_operation_kind::prune:
			switch (aFrom) {
			case epoch_catalog_operation_phase::prune_intent:
				return aTo
					== epoch_catalog_operation_phase::
						ref_delete_in_progress
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::ref_delete_in_progress:
				return aTo
					== epoch_catalog_operation_phase::
						visible_cleanup_pending;
			case epoch_catalog_operation_phase::visible_cleanup_pending:
				return aTo == epoch_catalog_operation_phase::complete;
			case epoch_catalog_operation_phase::abort_requested:
				return aTo == epoch_catalog_operation_phase::aborted;
			default:
				return false;
			}
		case epoch_catalog_operation_kind::cache_delete:
			switch (aFrom) {
			case epoch_catalog_operation_phase::
				cache_delete_building_exact_set:
				return aTo
					== epoch_catalog_operation_phase::
						cache_delete_intent_sealed
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::cache_delete_intent_sealed:
				return aTo
					== epoch_catalog_operation_phase::
						quarantine_rename_in_progress
					|| aTo
					== epoch_catalog_operation_phase::abort_requested;
			case epoch_catalog_operation_phase::
				quarantine_rename_in_progress:
				return aTo == epoch_catalog_operation_phase::quarantined;
			case epoch_catalog_operation_phase::quarantined:
				return aTo
					== epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending;
			case epoch_catalog_operation_phase::
				catalog_tombstoned_cleanup_pending:
				return aTo == epoch_catalog_operation_phase::complete;
			case epoch_catalog_operation_phase::abort_requested:
				return aTo == epoch_catalog_operation_phase::aborted;
			default:
				return false;
			}
		}
		return false;
	}

	/// \brief Computes the generic transition allowed result.
	[[nodiscard]] constexpr auto generic_transition_allowed(
		const epoch_catalog_operation_kind  aKind,
		const epoch_catalog_operation_phase aFrom,
		const epoch_catalog_operation_phase aTo) noexcept -> bool
	{
		using enum epoch_catalog_operation_phase;

		if (!valid_phase_transition(aKind, aFrom, aTo) || aFrom == aTo) {
			return false;
		}
		if (aTo == repair_required || aTo == abort_requested || aTo == aborted) {
			return true;
		}
		switch (aKind) {
		case epoch_catalog_operation_kind::append:
			return (aFrom == imported && aTo == staging_ready)
				|| (aFrom == commit_authorized
					&& aTo == ref_commit_in_progress)
				|| (aFrom == ref_commit_in_progress
					&& aTo == refs_committed)
				|| (aFrom == visible_cleanup_pending && aTo == complete);
		case epoch_catalog_operation_kind::prune:
			return (aFrom == prune_intent && aTo == ref_delete_in_progress)
				|| (aFrom == visible_cleanup_pending && aTo == complete);
		case epoch_catalog_operation_kind::cache_delete:
			return (
					   aFrom == cache_delete_intent_sealed
					   && aTo == quarantine_rename_in_progress)
				|| (aFrom == quarantine_rename_in_progress
					&& aTo == quarantined)
				|| (aFrom == catalog_tombstoned_cleanup_pending
					&& aTo == complete);
		}
		return false;
	}

	/// \brief Computes the required transition evidence result.
	[[nodiscard]] constexpr auto required_transition_evidence(
		const epoch_catalog_operation_kind  aKind,
		const epoch_catalog_operation_phase aFrom,
		const epoch_catalog_operation_phase aTo) noexcept
		-> std::optional<epoch_catalog_transition_evidence>
	{
		using enum epoch_catalog_operation_phase;
		using enum epoch_catalog_transition_evidence;

		if (!valid_phase_transition(aKind, aFrom, aTo)) {
			return std::nullopt;
		}
		if (aFrom == aTo) {
			return catalog_transaction;
		}
		if (aTo == repair_required) {
			return repair_ambiguity;
		}
		if (aTo == staging_ready) {
			return exact_staging_pair;
		}
		if (aTo == commit_authorized) {
			return core_commit_authorization;
		}
		if (aTo == refs_committed) {
			return exact_final_pair;
		}
		if (
			(aFrom == abort_requested && aTo == aborted)
			|| (aFrom == visible_cleanup_pending && aTo == complete)
			|| (aFrom == catalog_tombstoned_cleanup_pending
				&& aTo == complete)) {
			return exact_cleanup_complete;
		}
		if (
			aKind == epoch_catalog_operation_kind::prune
			&& aFrom == ref_delete_in_progress
			&& aTo == visible_cleanup_pending) {
			return exact_refs_absent;
		}
		if (
			aKind == epoch_catalog_operation_kind::cache_delete
			&& aFrom == quarantine_rename_in_progress
			&& aTo == quarantined) {
			return exact_quarantine;
		}
		return catalog_transaction;
	}

	/// \brief Describes the epoch catalog ref identity record.
	struct epoch_catalog_ref_identity
	{
		core::pull_request_authority_scope_id Authority;
		core::epoch_ordinal                   Ordinal;
		core::epoch_operation_id              Operation;
		bool                                  Staging = false;
		bool                                  Head    = false;

		auto operator<=>(const epoch_catalog_ref_identity&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const epoch_catalog_ref_identity& aIdentity) noexcept -> bool
	{
		if (!aIdentity.Authority.valid() || !core::valid(aIdentity.Ordinal)) {
			return false;
		}
		return aIdentity.Staging ? aIdentity.Operation.valid()
								 : !aIdentity.Operation.valid();
	}

	/// \brief Describes the durable git spool fact record.
	struct durable_git_spool_fact
	{
		core::epoch_operation_id Operation;
		git_pack_oid             ExpectedPack;
		git_pack_oid             ObservedPack;
		git_artifact_fact        Spool{
			.Role = git_artifact_role::completed_spool
		};

		auto operator<=>(const durable_git_spool_fact&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const durable_git_spool_fact& aFact) noexcept -> bool
	{
		return aFact.Operation.valid() && application::valid(aFact.ExpectedPack)
			&& aFact.ExpectedPack == aFact.ObservedPack
			&& application::valid(
				   aFact.Spool,
				   git_artifact_role::completed_spool)
			&& aFact.Spool.Bytes <= maximum_git_cache_operation_pack_bytes;
	}

	/// \brief Computes the durable fact result.
	[[nodiscard]] constexpr auto durable_fact(
		const completed_git_pack_receipt& aReceipt) noexcept
		-> durable_git_spool_fact
	{
		return {
			.Operation    = aReceipt.Operation,
			.ExpectedPack = aReceipt.ExpectedPack,
			.ObservedPack = aReceipt.ObservedPack,
			.Spool        = aReceipt.Spool,
		};
	}

	/// \brief Describes the durable git preimport fact record.
	struct durable_git_preimport_fact
	{
		core::epoch_operation_id              Operation;
		git_pack_oid                          ExpectedPack;
		std::vector<git_pack_inventory_entry> Packs;
		std::uint64_t                         RepositoryBytes         = 0U;
		bool                                  ExpectedPackPreexisting = false;

		auto operator<=>(const durable_git_preimport_fact&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const durable_git_preimport_fact& aFact) noexcept -> bool
	{
		if (
			!aFact.Operation.valid() || !application::valid(aFact.ExpectedPack)
			|| aFact.Packs.size() > maximum_git_cache_pack_count
			|| aFact.RepositoryBytes > maximum_git_repository_cache_bytes) {
			return false;
		}

		auto          expectedPresent = false;
		std::uint64_t inventoryBytes  = 0U;
		for (std::size_t index = 0U; index < aFact.Packs.size(); ++index) {
			const auto& pack = aFact.Packs[index];
			if (
				!application::valid(pack)
				|| pack.PackBytes
					> aFact.RepositoryBytes - inventoryBytes
				|| pack.IndexBytes
					> aFact.RepositoryBytes
						- inventoryBytes - pack.PackBytes
				|| pack.Pack.Algorithm != aFact.ExpectedPack.Algorithm
				|| (index != 0U
					&& !(aFact.Packs[index - 1U].Pack < pack.Pack))) {
				return false;
			}
			inventoryBytes += pack.PackBytes + pack.IndexBytes;
			expectedPresent = expectedPresent || pack.Pack == aFact.ExpectedPack;
		}
		return expectedPresent == aFact.ExpectedPackPreexisting;
	}

	[[nodiscard]] inline auto durable_fact(
		const git_preimport_observation& aObservation)
		-> durable_git_preimport_fact
	{
		return {
			.Operation               = aObservation.Operation,
			.ExpectedPack            = aObservation.ExpectedPack,
			.Packs                   = aObservation.Packs,
			.RepositoryBytes         = aObservation.RepositoryBytes,
			.ExpectedPackPreexisting = aObservation.ExpectedPackPreexisting,
		};
	}

	/// \brief Describes the durable git install fact record.
	struct durable_git_install_fact
	{
		core::epoch_operation_id Operation;
		git_pack_oid             Pack;
		git_artifact_fact        InstalledPack{
			.Role = git_artifact_role::installed_pack
		};
		git_artifact_fact InstalledIndex{
			.Role = git_artifact_role::installed_index
		};
		core::git_commit_oid Base;
		core::git_tree_oid   BaseTree;
		core::git_commit_oid Head;
		core::git_tree_oid   HeadTree;
		core::head_lineage   HeadLineage = core::head_lineage::not_applicable;

		auto operator<=>(const durable_git_install_fact&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const durable_git_install_fact& aFact) noexcept -> bool
	{
		return aFact.Operation.valid() && application::valid(aFact.Pack)
			&& application::valid(
				   aFact.InstalledPack,
				   git_artifact_role::installed_pack)
			&& application::valid(
				   aFact.InstalledIndex,
				   git_artifact_role::installed_index)
			&& aFact.InstalledPack.Bytes
			<= maximum_git_cache_operation_pack_bytes
			&& aFact.InstalledIndex.Bytes
			<= maximum_git_repository_cache_bytes
			&& aFact.Base.valid() && aFact.BaseTree.valid()
			&& aFact.Head.valid() && aFact.HeadTree.valid()
			&& aFact.Base.algorithm() == aFact.Pack.Algorithm
			&& aFact.Base.algorithm() == aFact.BaseTree.algorithm()
			&& aFact.Base.algorithm() == aFact.Head.algorithm()
			&& aFact.Base.algorithm() == aFact.HeadTree.algorithm()
			&& (aFact.HeadLineage == core::head_lineage::not_applicable
				|| aFact.HeadLineage == core::head_lineage::descendant
				|| aFact.HeadLineage
					== core::head_lineage::non_fast_forward
				|| aFact.HeadLineage == core::head_lineage::unknown);
	}

	/// \brief Computes the durable fact result.
	[[nodiscard]] constexpr auto durable_fact(
		const git_prepared_revision& aRevision) noexcept
		-> durable_git_install_fact
	{
		return {
			.Operation      = aRevision.Operation,
			.Pack           = aRevision.Pack,
			.InstalledPack  = aRevision.InstalledPack,
			.InstalledIndex = aRevision.InstalledIndex,
			.Base           = aRevision.Base,
			.BaseTree       = aRevision.BaseTree,
			.Head           = aRevision.Head,
			.HeadTree       = aRevision.HeadTree,
			.HeadLineage    = aRevision.HeadLineage,
		};
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const core::catalog_review_association& aReview) noexcept -> bool
	{
		return aReview.Association.valid() && aReview.Review.value() != 0U
			&& aReview.Actor.value() != 0U && core::valid(aReview.Order)
			&& aReview.State != core::github_review_state::pending
			&& static_cast<std::uint8_t>(aReview.State)
			<= static_cast<std::uint8_t>(
				   core::github_review_state::dismissed)
			&& aReview.Commit.valid()
			&& static_cast<std::uint8_t>(aReview.Availability)
			<= static_cast<std::uint8_t>(
				   core::anchor_availability::never_observed);
	}

	/// \brief Computes the valid new review association result.
	[[nodiscard]] constexpr auto valid_new_review_association(
		const core::catalog_review_association& aReview) noexcept -> bool
	{
		return application::valid(aReview)
			&& (aReview.Availability == core::anchor_availability::available
				|| aReview.Availability
					== core::anchor_availability::unavailable
				|| aReview.Availability
					== core::anchor_availability::never_observed);
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const core::review_epoch_summary& aEpoch) noexcept -> bool
	{
		if (
			!core::valid(aEpoch.Id) || !core::valid(aEpoch.Source)
			|| !aEpoch.BaseBranch.valid() || !aEpoch.Base.valid()
			|| !aEpoch.Head.valid()
			|| aEpoch.Base.algorithm() != aEpoch.Head.algorithm()
			|| !core::valid(aEpoch.ObservedAt)
			|| !core::valid(aEpoch.Cause)
			|| static_cast<std::uint8_t>(aEpoch.BaseAvailability)
				> static_cast<std::uint8_t>(
					core::anchor_availability::never_observed)
			|| static_cast<std::uint8_t>(aEpoch.HeadAvailability)
				> static_cast<std::uint8_t>(
					core::anchor_availability::never_observed)) {
			return false;
		}
		return !aEpoch.Material
			|| (aEpoch.Material->BaseTree.valid()
				&& aEpoch.Material->HeadTree.valid()
				&& aEpoch.Material->PolicyRevision.value() != 0U
				&& aEpoch.Material->BaseTree.algorithm()
					== aEpoch.Base.algorithm()
				&& aEpoch.Material->HeadTree.algorithm()
					== aEpoch.Base.algorithm());
	}

	/// \brief Describes the append operation context record.
	struct append_operation_context
	{
		core::application_session_id         ApplicationSession;
		core::epoch_request_id               Request;
		core::epoch_generation               Generation;
		core::epoch_operation_id             Operation;
		core::accepted_pull_request_revision Revision;
		core::epoch_catalog_identity         Catalog;

		auto operator<=>(const append_operation_context&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const append_operation_context& aContext) noexcept -> bool
	{
		return aContext.ApplicationSession.value() != 0U
			&& aContext.Request.value() != 0U
			&& aContext.Generation.value() != 0U
			&& aContext.Operation.valid() && core::valid(aContext.Revision)
			&& core::valid(aContext.Catalog);
	}

	/// \brief Describes the append reservation request record.
	struct append_reservation_request
	{
		append_operation_context Context;
		core::epoch_ordinal      ProposedOrdinal;

		auto operator<=>(const append_reservation_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const append_reservation_request& aRequest) noexcept -> bool
	{
		return valid(aRequest.Context) && core::valid(aRequest.ProposedOrdinal);
	}

	/// \brief Describes the append operation record record.
	struct append_operation_record
	{
		append_operation_context                     Context;
		core::epoch_ordinal                          ReservedOrdinal;
		epoch_catalog_operation_revision             OperationRevision;
		epoch_catalog_operation_phase                Phase = epoch_catalog_operation_phase::preparing;
		std::optional<epoch_catalog_operation_phase> RepairFromPhase;
		bool                                         RecoveryInputCompacted = false;
		epoch_catalog_ref_identity                   StagingBase;
		epoch_catalog_ref_identity                   StagingHead;
		epoch_catalog_ref_identity                   FinalBase;
		epoch_catalog_ref_identity                   FinalHead;
		std::optional<durable_git_spool_fact>        Spool;
		std::optional<durable_git_preimport_fact>    Preimport;
		std::optional<durable_git_install_fact>      Install;
		std::optional<core::append_epoch_mutation>   Candidate;

		auto operator<=>(const append_operation_record&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const append_operation_record& aRecord) noexcept -> bool
	{
		if (
			!valid(aRecord.Context) || !core::valid(aRecord.ReservedOrdinal)
			|| !application::valid(aRecord.OperationRevision)
			|| !valid_phase(
				epoch_catalog_operation_kind::append,
				aRecord.Phase)
			|| !valid_repair_origin(
				epoch_catalog_operation_kind::append,
				aRecord.Phase,
				aRecord.RepairFromPhase)
			|| aRecord.RecoveryInputCompacted
				!= (aRecord.Phase
						== epoch_catalog_operation_phase::complete
					|| aRecord.Phase
						== epoch_catalog_operation_phase::aborted)
			|| !application::valid(aRecord.StagingBase)
			|| !application::valid(aRecord.StagingHead)
			|| !application::valid(aRecord.FinalBase)
			|| !application::valid(aRecord.FinalHead)) {
			return false;
		}
		const auto& operation    = aRecord.Context.Operation;
		const auto  sameRefOwner = aRecord.StagingBase.Authority
				== aRecord.StagingHead.Authority
			&& aRecord.StagingBase.Authority
				== aRecord.FinalBase.Authority
			&& aRecord.StagingBase.Authority
				== aRecord.FinalHead.Authority
			&& aRecord.StagingBase.Ordinal == aRecord.ReservedOrdinal
			&& aRecord.StagingHead.Ordinal == aRecord.ReservedOrdinal
			&& aRecord.FinalBase.Ordinal == aRecord.ReservedOrdinal
			&& aRecord.FinalHead.Ordinal == aRecord.ReservedOrdinal
			&& aRecord.StagingBase.Operation == operation
			&& aRecord.StagingHead.Operation == operation
			&& aRecord.StagingBase.Staging && aRecord.StagingHead.Staging
			&& !aRecord.FinalBase.Staging && !aRecord.FinalHead.Staging
			&& !aRecord.StagingBase.Head && aRecord.StagingHead.Head
			&& !aRecord.FinalBase.Head && aRecord.FinalHead.Head;
		if (!sameRefOwner) {
			return false;
		}
		if (
			aRecord.Spool
			&& (!application::valid(*aRecord.Spool)
				|| aRecord.Spool->Operation != operation)) {
			return false;
		}
		if (
			aRecord.Preimport
			&& (!application::valid(*aRecord.Preimport)
				|| aRecord.Preimport->Operation != operation)) {
			return false;
		}
		if (
			aRecord.Install
			&& (!application::valid(*aRecord.Install)
				|| aRecord.Install->Operation != operation)) {
			return false;
		}
		if (aRecord.Candidate) {
			if (
				!application::valid(aRecord.Candidate->Epoch)
				|| aRecord.Candidate->Epoch.Id.Authority
					!= aRecord.StagingBase.Authority
				|| aRecord.Candidate->Epoch.Id.Incarnation
					!= aRecord.Context.Catalog.Incarnation
				|| aRecord.Candidate->Epoch.Id.Ordinal
					!= aRecord.ReservedOrdinal
				|| (aRecord.Candidate->Review
					&& !application::valid(
						*aRecord.Candidate->Review))) {
				return false;
			}
		}
		const auto noRecoveryInput    = !aRecord.Spool && !aRecord.Preimport;
		const auto exactRecoveryInput = aRecord.Spool && aRecord.Preimport
			&& aRecord.Spool->ExpectedPack
				== aRecord.Preimport->ExpectedPack;
		if (
			(!noRecoveryInput && !exactRecoveryInput)
			|| (aRecord.Candidate && !aRecord.Install)
			|| (!aRecord.RecoveryInputCompacted
				&& aRecord.Install && !exactRecoveryInput)) {
			return false;
		}

		using enum epoch_catalog_operation_phase;
		const auto phaseBeforeRepair = aRecord.Phase == repair_required ? *aRecord.RepairFromPhase
																		: aRecord.Phase;
		switch (phaseBeforeRepair) {
		case preparing:
			return noRecoveryInput && !aRecord.Install
				&& !aRecord.Candidate;
		case spool_recorded:
			return exactRecoveryInput && !aRecord.Install
				&& !aRecord.Candidate;
		case imported:
		case staging_ready:
			return exactRecoveryInput && aRecord.Install
				&& !aRecord.Candidate;
		case commit_authorized:
		case ref_commit_in_progress:
		case refs_committed:
		case visible_cleanup_pending:
			return exactRecoveryInput && aRecord.Install
				&& aRecord.Candidate;
		case complete:
			return aRecord.RecoveryInputCompacted
				&& noRecoveryInput && aRecord.Install
				&& aRecord.Candidate;
		case abort_requested:
			return noRecoveryInput
				? !aRecord.Install && !aRecord.Candidate
				: exactRecoveryInput;
		case aborted:
			return aRecord.RecoveryInputCompacted
				&& noRecoveryInput;
		default:
			return false;
		}
	}

	/// \brief Describes the record spool request record.
	struct record_spool_request
	{
		core::epoch_catalog_identity     Catalog;
		core::epoch_operation_id         Operation;
		epoch_catalog_operation_revision ExpectedOperationRevision;
		durable_git_spool_fact           Spool;
		durable_git_preimport_fact       Preimport;

		auto operator<=>(const record_spool_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const record_spool_request& aRequest) noexcept -> bool
	{
		if (
			!core::valid(aRequest.Catalog) || !aRequest.Operation.valid()
			|| !application::can_advance_nonterminal(
				aRequest.ExpectedOperationRevision)
			|| !application::valid(aRequest.Spool)
			|| !application::valid(aRequest.Preimport)
			|| aRequest.Spool.Operation != aRequest.Operation
			|| aRequest.Preimport.Operation != aRequest.Operation
			|| aRequest.Spool.ExpectedPack
				!= aRequest.Preimport.ExpectedPack) {
			return false;
		}
		if (!aRequest.Preimport.ExpectedPackPreexisting) {
			return true;
		}
		const auto expected = std::ranges::find_if(
			aRequest.Preimport.Packs,
			[&aRequest](const auto& aEntry) {
				return aEntry.Pack == aRequest.Spool.ExpectedPack;
			});
		return expected != aRequest.Preimport.Packs.end()
			&& expected->PackBytes == aRequest.Spool.Spool.Bytes;
	}

	/// \brief Describes the record install request record.
	struct record_install_request
	{
		core::epoch_catalog_identity     Catalog;
		core::epoch_operation_id         Operation;
		epoch_catalog_operation_revision ExpectedOperationRevision;
		durable_git_install_fact         Install;

		auto operator<=>(const record_install_request&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const record_install_request& aRequest) noexcept -> bool
	{
		return core::valid(aRequest.Catalog) && aRequest.Operation.valid()
			&& application::can_advance_nonterminal(
				   aRequest.ExpectedOperationRevision)
			&& application::valid(aRequest.Install)
			&& aRequest.Install.Operation == aRequest.Operation;
	}

	/// \brief Describes the record candidate request record.
	struct record_candidate_request
	{
		core::epoch_catalog_identity     Catalog;
		core::epoch_operation_id         Operation;
		epoch_catalog_operation_revision ExpectedOperationRevision;
		core::append_epoch_mutation      Candidate;

		auto operator<=>(const record_candidate_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const record_candidate_request& aRequest) noexcept -> bool
	{
		return core::valid(aRequest.Catalog) && aRequest.Operation.valid()
			&& application::can_advance_nonterminal(
				   aRequest.ExpectedOperationRevision)
			&& application::valid(aRequest.Candidate.Epoch)
			&& aRequest.Candidate.Epoch.Material.has_value()
			&& aRequest.Candidate.Epoch.BaseAvailability
			== core::anchor_availability::available
			&& aRequest.Candidate.Epoch.HeadAvailability
			== core::anchor_availability::available
			&& (!aRequest.Candidate.Review
				|| application::valid_new_review_association(
					*aRequest.Candidate.Review))
			&& aRequest.Candidate.Epoch.Id.Incarnation
			== aRequest.Catalog.Incarnation;
	}

	/// \brief Describes the operation transition request record.
	struct operation_transition_request
	{
		core::epoch_catalog_identity      Catalog;
		core::epoch_operation_id          Operation;
		epoch_catalog_operation_kind      Kind     = epoch_catalog_operation_kind::append;
		epoch_catalog_operation_phase     Expected = epoch_catalog_operation_phase::preparing;
		epoch_catalog_operation_phase     Next     = epoch_catalog_operation_phase::preparing;
		epoch_catalog_transition_evidence Evidence = epoch_catalog_transition_evidence::catalog_transaction;
		epoch_catalog_operation_revision  ExpectedOperationRevision;

		auto operator<=>(const operation_transition_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const operation_transition_request& aRequest) noexcept -> bool
	{
		const auto required = required_transition_evidence(
			aRequest.Kind,
			aRequest.Expected,
			aRequest.Next);
		return core::valid(aRequest.Catalog) && aRequest.Operation.valid()
			&& (application::terminal(aRequest.Next)
					? application::can_advance(
						  aRequest.ExpectedOperationRevision)
					: application::can_advance_nonterminal(
						  aRequest.ExpectedOperationRevision))
			&& generic_transition_allowed(
				   aRequest.Kind,
				   aRequest.Expected,
				   aRequest.Next)
			&& required && aRequest.Evidence == *required;
	}

	/// \brief Describes the finalize append request record.
	struct finalize_append_request
	{
		core::epoch_catalog_identity     Catalog;
		core::epoch_operation_id         Operation;
		epoch_catalog_operation_revision ExpectedOperationRevision;
		core::append_epoch_mutation      Mutation;
		git_retained_ref_pair            ObservedRefs;

		auto operator<=>(const finalize_append_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const finalize_append_request& aRequest) noexcept -> bool
	{
		const auto& epoch = aRequest.Mutation.Epoch;
		return core::valid(aRequest.Catalog) && aRequest.Operation.valid()
			&& application::can_advance_nonterminal(
				   aRequest.ExpectedOperationRevision)
			&& application::valid(epoch)
			&& epoch.Material.has_value()
			&& epoch.BaseAvailability
			== core::anchor_availability::available
			&& epoch.HeadAvailability
			== core::anchor_availability::available
			&& epoch.Id.Incarnation == aRequest.Catalog.Incarnation
			&& (!aRequest.Mutation.Review
				|| application::valid_new_review_association(
					*aRequest.Mutation.Review))
			&& application::valid(aRequest.ObservedRefs)
			&& aRequest.ObservedRefs.Scope == epoch.Id.Authority
			&& aRequest.ObservedRefs.Ordinal == epoch.Id.Ordinal
			&& aRequest.ObservedRefs.Base == epoch.Base
			&& aRequest.ObservedRefs.Head == epoch.Head;
	}

	/// \brief Describes the associate review request record.
	struct associate_review_request
	{
		core::epoch_catalog_identity     Catalog;
		core::catalog_review_association Review;
		core::provider_snapshot_id       AcceptedSnapshot;

		auto operator<=>(const associate_review_request&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const associate_review_request& aRequest) noexcept -> bool
	{
		return core::valid(aRequest.Catalog)
			&& application::valid_new_review_association(aRequest.Review)
			&& aRequest.AcceptedSnapshot.value() != 0U;
	}

	/// \brief Describes the catalog commit result record.
	struct catalog_commit_result
	{
		core::epoch_catalog_identity         Catalog;
		bool                                 Changed = false;
		std::optional<core::review_epoch_id> InsertedEpoch;

		auto operator<=>(const catalog_commit_result&) const = default;
	};

	/// \brief Describes the prune epoch fact record.
	struct prune_epoch_fact
	{
		core::review_epoch_id      Epoch;
		git_retained_ref_pair      Retained;
		epoch_catalog_ref_identity BaseRef;
		epoch_catalog_ref_identity HeadRef;
		core::anchor_availability  ExpectedBaseAvailability = core::anchor_availability::available;
		core::anchor_availability  ExpectedHeadAvailability = core::anchor_availability::available;

		auto operator<=>(const prune_epoch_fact&) const = default;
	};

	/// \brief Computes the valid prune anchor availability result.
	///
	/// Defers to `core::prunable` so the durable maintenance authority and the
	/// core command projection share one definition of prune eligibility.
	[[nodiscard]] constexpr auto valid_prune_anchor_availability(
		const core::anchor_availability aAvailability) noexcept -> bool
	{
		return core::prunable(aAvailability);
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const prune_epoch_fact& aFact) noexcept -> bool
	{
		return core::valid(aFact.Epoch)
			&& application::valid(aFact.Retained)
			&& application::valid(aFact.BaseRef)
			&& application::valid(aFact.HeadRef)
			&& aFact.Retained.Scope == aFact.Epoch.Authority
			&& aFact.Retained.Ordinal == aFact.Epoch.Ordinal
			&& aFact.BaseRef.Authority == aFact.Epoch.Authority
			&& aFact.HeadRef.Authority == aFact.Epoch.Authority
			&& aFact.BaseRef.Ordinal == aFact.Epoch.Ordinal
			&& aFact.HeadRef.Ordinal == aFact.Epoch.Ordinal
			&& !aFact.BaseRef.Staging && !aFact.HeadRef.Staging
			&& !aFact.BaseRef.Head && aFact.HeadRef.Head
			&& application::valid_prune_anchor_availability(
				   aFact.ExpectedBaseAvailability)
			&& application::valid_prune_anchor_availability(
				   aFact.ExpectedHeadAvailability);
	}

	/// \brief Describes the begin prune request record.
	struct begin_prune_request
	{
		core::epoch_catalog_identity  Catalog;
		core::epoch_operation_id      Operation;
		std::vector<prune_epoch_fact> Epochs;

		auto operator<=>(const begin_prune_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const begin_prune_request& aRequest) noexcept -> bool
	{
		if (
			!core::valid(aRequest.Catalog) || !aRequest.Operation.valid()
			|| aRequest.Epochs.empty()
			|| aRequest.Epochs.size() > maximum_prune_epoch_count) {
			return false;
		}
		for (std::size_t index = 0U; index < aRequest.Epochs.size(); ++index) {
			const auto& epoch = aRequest.Epochs[index];
			if (
				!application::valid(epoch)
				|| epoch.Epoch.Incarnation
					!= aRequest.Catalog.Incarnation
				|| (index != 0U
					&& !(aRequest.Epochs[index - 1U].Epoch
						 < epoch.Epoch))) {
				return false;
			}
		}
		return true;
	}

	/// \brief Describes the cache delete scope fact record.
	struct cache_delete_scope_fact
	{
		core::pull_request_authority_scope_id Authority;
		core::epoch_catalog_revision          ExpectedRevision;

		auto operator<=>(const cache_delete_scope_fact&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const cache_delete_scope_fact& aFact) noexcept -> bool
	{
		return aFact.Authority.valid() && core::valid(aFact.ExpectedRevision);
	}

	/// \brief Describes the cache delete impact record.
	struct cache_delete_impact
	{
		core::epoch_catalog_incarnation_id   Incarnation;
		core::repository_cache_scope_id      Cache;
		std::vector<cache_delete_scope_fact> Scopes;
		std::uint64_t                        RetainedEpochCount = 0U;

		auto operator<=>(const cache_delete_impact&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const cache_delete_impact& aImpact) noexcept -> bool
	{
		if (
			!aImpact.Incarnation.valid() || !aImpact.Cache.valid()
			|| aImpact.Scopes.empty()
			|| aImpact.Scopes.size() > maximum_cache_delete_scope_count
			|| aImpact.RetainedEpochCount
				> static_cast<std::uint64_t>(aImpact.Scopes.size())
					* maximum_retained_epoch_count_per_pull_request) {
			return false;
		}
		for (std::size_t index = 0U; index < aImpact.Scopes.size(); ++index) {
			if (
				!application::valid(aImpact.Scopes[index])
				|| (index != 0U
					&& !(aImpact.Scopes[index - 1U].Authority
						 < aImpact.Scopes[index].Authority))) {
				return false;
			}
		}
		return true;
	}

	/// \brief Describes the begin cache delete request record.
	struct begin_cache_delete_request
	{
		core::epoch_operation_id Operation;
		cache_delete_impact      Impact;

		auto operator<=>(const begin_cache_delete_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const begin_cache_delete_request& aRequest) noexcept -> bool
	{
		return aRequest.Operation.valid() && application::valid(aRequest.Impact);
	}

	/// \brief Describes the maintenance operation record record.
	struct maintenance_operation_record
	{
		core::epoch_catalog_incarnation_id           Incarnation;
		core::epoch_operation_id                     Operation;
		epoch_catalog_operation_kind                 Kind  = epoch_catalog_operation_kind::prune;
		epoch_catalog_operation_phase                Phase = epoch_catalog_operation_phase::prune_intent;
		std::optional<epoch_catalog_operation_phase> RepairFromPhase;
		epoch_catalog_operation_revision             OperationRevision;
		core::repository_cache_scope_id              Cache;
		std::vector<prune_epoch_fact>                Epochs;
		std::vector<cache_delete_scope_fact>         Scopes;

		auto operator<=>(const maintenance_operation_record&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const maintenance_operation_record& aRecord) noexcept -> bool
	{
		if (
			!aRecord.Incarnation.valid() || !aRecord.Operation.valid()
			|| !application::valid(aRecord.OperationRevision)
			|| !valid_phase(aRecord.Kind, aRecord.Phase)
			|| !valid_repair_origin(
				aRecord.Kind, aRecord.Phase, aRecord.RepairFromPhase)) {
			return false;
		}
		if (aRecord.Kind == epoch_catalog_operation_kind::prune) {
			if (!aRecord.Cache.valid() || aRecord.Epochs.empty()
				|| !aRecord.Scopes.empty()
				|| aRecord.Epochs.size() > maximum_prune_epoch_count) {
				return false;
			}
			for (std::size_t index = 0U; index < aRecord.Epochs.size(); ++index) {
				if (
					!application::valid(aRecord.Epochs[index])
					|| (index != 0U
						&& !(aRecord.Epochs[index - 1U].Epoch
							 < aRecord.Epochs[index].Epoch))) {
					return false;
				}
			}
			return true;
		}
		if (aRecord.Kind == epoch_catalog_operation_kind::cache_delete) {
			if (!aRecord.Cache.valid() || !aRecord.Epochs.empty()
				|| aRecord.Scopes.empty()
				|| aRecord.Scopes.size() > maximum_cache_delete_scope_count) {
				return false;
			}
			for (std::size_t index = 0U; index < aRecord.Scopes.size(); ++index) {
				if (
					!application::valid(aRecord.Scopes[index])
					|| (index != 0U
						&& !(aRecord.Scopes[index - 1U].Authority
							 < aRecord.Scopes[index].Authority))) {
					return false;
				}
			}
			return true;
		}
		return false;
	}

	/// \brief Describes the finalize prune request record.
	struct finalize_prune_request
	{
		operation_transition_request       Transition;
		std::vector<git_retained_ref_pair> ObservedAbsent;

		auto operator<=>(const finalize_prune_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const finalize_prune_request& aRequest) noexcept -> bool
	{
		if (
			!core::valid(aRequest.Transition.Catalog)
			|| !aRequest.Transition.Operation.valid()
			|| !application::can_advance_nonterminal(
				aRequest.Transition.ExpectedOperationRevision)
			|| aRequest.Transition.Kind
				!= epoch_catalog_operation_kind::prune
			|| aRequest.Transition.Expected
				!= epoch_catalog_operation_phase::
					ref_delete_in_progress
			|| aRequest.Transition.Next
				!= epoch_catalog_operation_phase::
					visible_cleanup_pending
			|| aRequest.Transition.Evidence
				!= epoch_catalog_transition_evidence::exact_refs_absent
			|| aRequest.ObservedAbsent.empty()
			|| aRequest.ObservedAbsent.size() > maximum_prune_epoch_count) {
			return false;
		}
		for (std::size_t index = 0U; index < aRequest.ObservedAbsent.size(); ++index) {
			if (
				!application::valid(aRequest.ObservedAbsent[index])
				|| (index != 0U
					&& !(
						aRequest.ObservedAbsent[index - 1U].Ordinal
						< aRequest.ObservedAbsent[index].Ordinal))) {
				return false;
			}
		}
		return true;
	}

	/// \brief Describes the finalize cache delete request record.
	struct finalize_cache_delete_request
	{
		operation_transition_request Transition;

		auto operator<=>(const finalize_cache_delete_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const finalize_cache_delete_request& aRequest) noexcept -> bool
	{
		return core::valid(aRequest.Transition.Catalog)
			&& aRequest.Transition.Operation.valid()
			&& application::can_advance_nonterminal(
				   aRequest.Transition.ExpectedOperationRevision)
			&& aRequest.Transition.Kind
			== epoch_catalog_operation_kind::cache_delete
			&& aRequest.Transition.Expected
			== epoch_catalog_operation_phase::quarantined
			&& aRequest.Transition.Next
			== epoch_catalog_operation_phase::
				catalog_tombstoned_cleanup_pending
			&& aRequest.Transition.Evidence
			== epoch_catalog_transition_evidence::
				catalog_transaction;
	}

	using epoch_recovery_operation = std::variant<append_operation_record, maintenance_operation_record>;

	/// \brief Describes the epoch recovery page record.
	struct epoch_recovery_page
	{
		core::epoch_catalog_incarnation_id      Incarnation;
		std::vector<epoch_recovery_operation>   Operations;
		std::optional<core::epoch_operation_id> Continuation;

		auto operator<=>(const epoch_recovery_page&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const epoch_recovery_page& aPage) noexcept -> bool
	{
		if (
			!aPage.Incarnation.valid()
			|| aPage.Operations.size() > maximum_epoch_recovery_page_size
			|| (aPage.Continuation
				&& !aPage.Continuation->valid())) {
			return false;
		}

		std::optional<core::epoch_operation_id> prior;
		for (const auto& operation : aPage.Operations) {
			const auto current = std::visit(
				[](const auto& aValue) {
					if constexpr (
						std::is_same_v<
							std::remove_cvref_t<decltype(aValue)>,
							append_operation_record>) {
						return aValue.Context.Operation;
					} else {
						return aValue.Operation;
					}
				},
				operation);
			const auto recordValid = std::visit(
				[](const auto& aValue) {
					return application::valid(aValue);
				},
				operation);
			const auto incarnation = std::visit(
				[](const auto& aValue) {
					if constexpr (
						std::is_same_v<
							std::remove_cvref_t<decltype(aValue)>,
							append_operation_record>) {
						return aValue.Context.Catalog.Incarnation;
					} else {
						return aValue.Incarnation;
					}
				},
				operation);
			if (
				!recordValid || incarnation != aPage.Incarnation
				|| (prior && !(*prior < current))) {
				return false;
			}
			prior = current;
		}
		return !aPage.Continuation
			|| !prior || *prior < *aPage.Continuation;
	}

	enum class epoch_catalog_failure : std::uint8_t {
		invalid_request,
		invalid_path,
		invalid_permissions,
		unavailable,
		schema_mismatch,
		integrity_failure,
		authority_mismatch,
		catalog_mismatch,
		revision_conflict,
		operation_identity_conflict,
		source_observation_conflict,
		operation_conflict,
		phase_conflict,
		quota_exceeded,
		ordinal_exhausted,
		busy,
		repair_required,
		cancelled,
		platform_unsupported,
	};

	template <typename Value>
	using epoch_catalog_result = std::expected<Value, epoch_catalog_failure>;

	/// \brief Provides the epoch catalog session contract.
	class epoch_catalog_session
	{
	public:
		epoch_catalog_session()                             = default;
		epoch_catalog_session(const epoch_catalog_session&) = delete;
		auto operator=(const epoch_catalog_session&)
			-> epoch_catalog_session&                  = delete;
		epoch_catalog_session(epoch_catalog_session&&) = delete;
		auto operator=(epoch_catalog_session&&)
			-> epoch_catalog_session&    = delete;
		virtual ~epoch_catalog_session() = default;

		[[nodiscard]] virtual auto binding() const noexcept
			-> const epoch_catalog_binding& = 0;

		[[nodiscard]] virtual auto snapshot(std::stop_token aStop)
			-> epoch_catalog_result<epoch_catalog_snapshot> = 0;

		// Incarnation-wide collision probe for every accepted observation,
		// including paths that do not reserve an append. reserve_append repeats
		// this check inside its writer transaction.
		[[nodiscard]] virtual auto source_observation_exists(
			core::epoch_source_observation_id aObservation,
			std::stop_token                   aStop) -> epoch_catalog_result<bool> = 0;

		// Resolves stable local identity and anchor availability without writing
		// the catalog or scanning history. A matching provider review reuses its
		// existing association even when ProposedAssociation differs.
		[[nodiscard]] virtual auto canonical_review(
			const canonical_review_request& aRequest,
			std::stop_token                 aStop)
			-> epoch_catalog_result<core::catalog_review_association> = 0;

		[[nodiscard]] virtual auto exact_anchors(
			const exact_anchor_lookup_request& aRequest,
			std::stop_token                    aStop)
			-> epoch_catalog_result<exact_anchor_lookup_result> = 0;

		[[nodiscard]] virtual auto history(
			const epoch_history_page_request& aRequest,
			std::stop_token                   aStop)
			-> epoch_catalog_result<epoch_history_page> = 0;

		[[nodiscard]] virtual auto mark_anchor_availability(
			const mark_anchor_availability_request& aRequest,
			std::stop_token                         aStop)
			-> epoch_catalog_result<catalog_commit_result> = 0;

		[[nodiscard]] virtual auto reserve_append(
			const append_reservation_request& aRequest,
			std::stop_token                   aStop)
			-> epoch_catalog_result<append_operation_record> = 0;

		[[nodiscard]] virtual auto record_spool(
			const record_spool_request& aRequest,
			std::stop_token             aStop)
			-> epoch_catalog_result<append_operation_record> = 0;

		[[nodiscard]] virtual auto record_install(
			const record_install_request& aRequest,
			std::stop_token               aStop)
			-> epoch_catalog_result<append_operation_record> = 0;

		[[nodiscard]] virtual auto record_candidate(
			const record_candidate_request& aRequest,
			std::stop_token                 aStop)
			-> epoch_catalog_result<append_operation_record> = 0;

		[[nodiscard]] virtual auto transition_operation(
			const operation_transition_request& aRequest,
			std::stop_token                     aStop)
			-> epoch_catalog_result<epoch_recovery_operation> = 0;

		[[nodiscard]] virtual auto finalize_append(
			const finalize_append_request& aRequest,
			std::stop_token                aStop)
			-> epoch_catalog_result<catalog_commit_result> = 0;

		[[nodiscard]] virtual auto associate_review(
			const associate_review_request& aRequest,
			std::stop_token                 aStop)
			-> epoch_catalog_result<catalog_commit_result> = 0;

		[[nodiscard]] virtual auto begin_prune(
			const begin_prune_request& aRequest,
			std::stop_token            aStop)
			-> epoch_catalog_result<maintenance_operation_record> = 0;

		[[nodiscard]] virtual auto finalize_prune(
			const finalize_prune_request& aRequest,
			std::stop_token               aStop)
			-> epoch_catalog_result<catalog_commit_result> = 0;

		[[nodiscard]] virtual auto begin_cache_delete(
			const begin_cache_delete_request& aRequest,
			std::stop_token                   aStop)
			-> epoch_catalog_result<maintenance_operation_record> = 0;

		[[nodiscard]] virtual auto cache_delete_impact(
			core::repository_cache_scope_id aCache,
			std::stop_token                 aStop)
			-> epoch_catalog_result<application::cache_delete_impact> = 0;

		[[nodiscard]] virtual auto finalize_cache_delete(
			const finalize_cache_delete_request& aRequest,
			std::stop_token                      aStop)
			-> epoch_catalog_result<void> = 0;

		[[nodiscard]] virtual auto recovery_page(
			std::optional<core::epoch_operation_id> aAfter,
			std::uint16_t                           aLimit,
			std::stop_token                         aStop)
			-> epoch_catalog_result<epoch_recovery_page> = 0;
	};

	/// \brief Provides the epoch catalog port contract.
	class epoch_catalog_port
	{
	public:
		epoch_catalog_port()                          = default;
		epoch_catalog_port(const epoch_catalog_port&) = delete;
		auto operator=(const epoch_catalog_port&)
			-> epoch_catalog_port&               = delete;
		epoch_catalog_port(epoch_catalog_port&&) = delete;
		auto operator=(epoch_catalog_port&&)
			-> epoch_catalog_port&    = delete;
		virtual ~epoch_catalog_port() = default;

		[[nodiscard]] virtual auto open(
			const epoch_catalog_open_request& aRequest,
			std::stop_token                   aStop)
			-> epoch_catalog_result<std::unique_ptr<epoch_catalog_session>> = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_EPOCH_CATALOG_PORT_HPP
