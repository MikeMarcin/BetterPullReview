#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_EPOCH_MODEL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_EPOCH_MODEL_HPP

#include "bpr/core/github_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace bpr::core {
	enum class git_hash_algorithm : std::uint8_t {
		invalid,
		sha1,
		sha256,
	};
	/// \brief Provides the git object oid contract.

	template <typename Kind>
	class git_object_oid
	{
	public:
		constexpr git_object_oid() = default;

		explicit constexpr git_object_oid(
			const git_hash_algorithm            aAlgorithm,
			const std::array<std::uint8_t, 32>& aBytes) noexcept
			: Algorithm{ aAlgorithm }
			, Bytes{ aBytes }
		{
		}

		[[nodiscard]] constexpr auto algorithm() const noexcept
			-> git_hash_algorithm
		{
			return Algorithm;
		}

		[[nodiscard]] constexpr auto bytes() const noexcept
			-> const std::array<std::uint8_t, 32>&
		{
			return Bytes;
		}

		[[nodiscard]] constexpr auto byte_count() const noexcept -> std::size_t
		{
			switch (Algorithm) {
			case git_hash_algorithm::sha1:
				return 20U;
			case git_hash_algorithm::sha256:
				return 32U;
			case git_hash_algorithm::invalid:
				return 0U;
			}
			return 0U;
		}

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			const auto size = byte_count();
			if (size == 0U) {
				return false;
			}

			bool nonzero = false;
			for (std::size_t index = 0; index < size; ++index) {
				nonzero = nonzero || Bytes[index] != 0U;
			}
			for (std::size_t index = size; index < Bytes.size(); ++index) {
				if (Bytes[index] != 0U) {
					return false;
				}
			}
			return nonzero;
		}

		auto operator<=>(const git_object_oid&) const = default;

	private:
		git_hash_algorithm           Algorithm = git_hash_algorithm::invalid;
		std::array<std::uint8_t, 32> Bytes{ };
	};

	struct git_commit_oid_kind;
	struct git_tree_oid_kind;

	using git_commit_oid = git_object_oid<git_commit_oid_kind>;
	using git_tree_oid   = git_object_oid<git_tree_oid_kind>;
	struct git_blob_oid_kind;
	using git_blob_oid = git_object_oid<git_blob_oid_kind>;

	/// \brief Provides the canonical git oid contract.
	class canonical_git_oid
	{
	public:
		constexpr canonical_git_oid() = default;

		explicit constexpr canonical_git_oid(
			const std::array<char, 64>& aCharacters,
			const std::uint8_t          aLength) noexcept
			: Characters{ aCharacters }
			, Length{ static_cast<std::uint8_t>(
				  aLength == 40U || aLength == 64U ? aLength : 0U) }
		{
		}

		[[nodiscard]] constexpr auto view() const noexcept -> std::string_view
		{
			return { Characters.data(), Length };
		}

		auto operator<=>(const canonical_git_oid&) const = default;

	private:
		std::array<char, 64> Characters{ };
		std::uint8_t         Length = 0;
	};

	/// \brief Parses the supplied git commit oid.
	[[nodiscard]] auto parse_git_commit_oid(std::string_view aText) noexcept
		-> std::optional<git_commit_oid>;
	/// \brief Parses the supplied git tree oid.
	[[nodiscard]] auto parse_git_tree_oid(std::string_view aText) noexcept
		-> std::optional<git_tree_oid>;
	/// \brief Parses the supplied git blob oid.
	[[nodiscard]] auto parse_git_blob_oid(std::string_view aText) noexcept
		-> std::optional<git_blob_oid>;
	/// \brief Reports whether the canonical hex condition holds.
	[[nodiscard]] auto canonical_hex(const git_commit_oid& aOid) noexcept
		-> canonical_git_oid;
	/// \brief Reports whether the canonical hex condition holds.
	[[nodiscard]] auto canonical_hex(const git_tree_oid& aOid) noexcept
		-> canonical_git_oid;
	/// \brief Reports whether the canonical hex condition holds.
	[[nodiscard]] auto canonical_hex(const git_blob_oid& aOid) noexcept
		-> canonical_git_oid;
	/// \brief Provides the fixed identity 256 contract.

	template <typename Tag>
	class fixed_identity_256
	{
	public:
		constexpr fixed_identity_256() = default;

		explicit constexpr fixed_identity_256(
			const std::array<std::uint8_t, 32>& aBytes) noexcept
			: Bytes{ aBytes }
		{
		}

		[[nodiscard]] constexpr auto bytes() const noexcept
			-> const std::array<std::uint8_t, 32>&
		{
			return Bytes;
		}

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			for (const auto byte : Bytes) {
				if (byte != 0U) {
					return true;
				}
			}
			return false;
		}

		auto operator<=>(const fixed_identity_256&) const = default;

	private:
		std::array<std::uint8_t, 32> Bytes{ };
	};
	/// \brief Provides the integral identity contract.

	template <typename Tag, typename Representation>
	class integral_identity
	{
	public:
		constexpr integral_identity() = default;

		explicit constexpr integral_identity(const Representation aValue) noexcept
			: Value{ aValue }
		{
		}

		[[nodiscard]] constexpr auto value() const noexcept -> Representation
		{
			return Value;
		}

		auto operator<=>(const integral_identity&) const = default;

	private:
		Representation Value = 0;
	};

	struct github_profile_revision_tag;
	struct github_viewer_actor_id_tag;
	struct github_review_id_tag;
	struct comparison_policy_revision_tag;
	struct epoch_request_id_tag;
	struct epoch_ordinal_tag;
	struct epoch_catalog_revision_tag;
	struct epoch_generation_tag;
	struct comparison_generation_tag;
	struct comparison_revision_tag;
	struct observed_utc_milliseconds_tag;
	struct github_review_order_tag;
	struct pull_request_authority_scope_id_tag;
	struct epoch_catalog_incarnation_id_tag;
	struct base_branch_id_tag;
	struct review_association_id_tag;
	struct repository_cache_scope_id_tag;
	struct host_authority_fingerprint_tag;
	struct epoch_operation_id_tag;
	struct epoch_source_observation_id_tag;
	struct epoch_maintenance_target_digest_tag;

	using github_profile_revision         = strong_id<github_profile_revision_tag>;
	using github_viewer_actor_id          = strong_id<github_viewer_actor_id_tag>;
	using github_review_id                = strong_id<github_review_id_tag>;
	using comparison_policy_revision      = strong_id<comparison_policy_revision_tag>;
	using epoch_request_id                = strong_id<epoch_request_id_tag>;
	using epoch_ordinal                   = integral_identity<epoch_ordinal_tag, std::int64_t>;
	using epoch_catalog_revision          = integral_identity<epoch_catalog_revision_tag, std::int64_t>;
	using epoch_generation                = strong_id<epoch_generation_tag>;
	using comparison_generation           = strong_id<comparison_generation_tag>;
	using comparison_revision             = integral_identity<comparison_revision_tag, std::int64_t>;
	using observed_utc_milliseconds       = integral_identity<observed_utc_milliseconds_tag, std::int64_t>;
	using github_review_order             = integral_identity<github_review_order_tag, std::uint16_t>;
	using pull_request_authority_scope_id = fixed_identity_256<pull_request_authority_scope_id_tag>;
	using epoch_catalog_incarnation_id    = fixed_identity_256<epoch_catalog_incarnation_id_tag>;
	using base_branch_id                  = fixed_identity_256<base_branch_id_tag>;
	using review_association_id           = fixed_identity_256<review_association_id_tag>;
	using repository_cache_scope_id       = fixed_identity_256<repository_cache_scope_id_tag>;
	using host_authority_fingerprint      = fixed_identity_256<host_authority_fingerprint_tag>;
	using epoch_operation_id              = fixed_identity_256<epoch_operation_id_tag>;
	using epoch_source_observation_id     = fixed_identity_256<epoch_source_observation_id_tag>;
	using epoch_maintenance_target_digest = fixed_identity_256<epoch_maintenance_target_digest_tag>;

	inline constexpr auto          maximum_epoch_ordinal                  = std::numeric_limits<std::int64_t>::max();
	inline constexpr auto          maximum_epoch_catalog_revision         = std::numeric_limits<std::int64_t>::max();
	inline constexpr auto          maximum_comparison_revision            = std::numeric_limits<std::int64_t>::max();
	inline constexpr std::uint16_t maximum_epoch_history_page_size        = 100U;
	inline constexpr std::uint16_t maximum_epoch_maintenance_target_count = 100U;
	inline constexpr std::int64_t  maximum_observed_utc_milliseconds      = 253'402'300'799'999LL;
	inline constexpr std::uint16_t maximum_github_review_order            = 10'000U;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const epoch_ordinal aValue) noexcept -> bool
	{
		return aValue.value() > 0 && aValue.value() <= maximum_epoch_ordinal;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const epoch_catalog_revision aValue) noexcept
		-> bool
	{
		return aValue.value() >= 0
			&& aValue.value() <= maximum_epoch_catalog_revision;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const comparison_revision aValue) noexcept
		-> bool
	{
		return aValue.value() > 0 && aValue.value() <= maximum_comparison_revision;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const observed_utc_milliseconds aValue) noexcept -> bool
	{
		return aValue.value() >= 0
			&& aValue.value() <= maximum_observed_utc_milliseconds;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const github_review_order aValue) noexcept
		-> bool
	{
		return aValue.value() > 0 && aValue.value() <= maximum_github_review_order;
	}

	enum class pull_request_revision_origin : std::uint8_t {
		live,
		explicit_historical_import,
	};

	// Declared, not included. `bpr/core/anonymous_read_policy.hpp` owns the
	// definition and includes this header's siblings; naming the type here
	// with its fixed underlying type keeps the epoch model free of the policy
	// header while still letting the authority carry the discriminant. The
	// enumerators are deliberately not visible here, so `Mode` is
	// value-initialized; `epoch_transition.cpp` static_asserts that the
	// value-initialized mode is the authenticated one.
	enum class github_access_mode : std::uint8_t;

	/// \brief Describes the pull request authority record.
	///
	/// The record is discriminated by its access mode. An authenticated
	/// authority names an account and a viewer; a public-preview authority
	/// names neither, because a signed-out reader has no identity to carry.
	/// The two are different authorities even when every other field agrees,
	/// so nothing keyed on this record may reuse an authenticated namespace
	/// for an anonymous read or the other way round.
	struct pull_request_authority
	{
		host_profile_id                 Profile;
		github_profile_revision         ProfileRevision;
		git_host_id                     Host;
		host_authority_fingerprint      HostAuthority;
		github_account_id               Account;
		github_viewer_actor_id          Viewer;
		repository_id                   Repository;
		repository_cache_scope_id       Cache;
		pull_request_id                 PullRequest;
		pull_request_number             Number;
		pull_request_authority_scope_id Scope;
		// Last and defaulted so every authority written before the mode
		// existed still means the authenticated authority it meant, and part
		// of the defaulted comparison so no ordering, equality, or derived key
		// can drop the discriminant.
		github_access_mode Mode{ };

		auto operator<=>(const pull_request_authority&) const = default;
	};

	/// \brief Describes the pull request source revision record.
	struct pull_request_source_revision
	{
		epoch_source_observation_id Observation;
		provider_snapshot_id        Snapshot;
		github_identity_generation  IdentityGeneration;
		request_generation          PullRequestGeneration;

		auto operator<=>(const pull_request_source_revision&) const = default;
	};

	enum class anchor_availability : std::uint8_t {
		available,
		unavailable,
		pruned,
		cache_deleted,
		corrupt,
		never_observed,
	};

	/// \brief Checks whether the supplied availability can enter a prune
	/// operation.
	///
	/// This is the single definition of prune eligibility. The command
	/// projection uses it to avoid offering an operation that the durable
	/// maintenance path would reject, and the catalog port defers to it so the
	/// projection and the final authority cannot drift apart.
	[[nodiscard]] constexpr auto prunable(
		const anchor_availability aAvailability) noexcept -> bool
	{
		return aAvailability == anchor_availability::available
			|| aAvailability == anchor_availability::unavailable
			|| aAvailability == anchor_availability::corrupt;
	}

	enum class github_review_state : std::uint8_t {
		pending,
		approved,
		changes_requested,
		commented,
		dismissed,
	};

	/// \brief Describes the current viewer review candidate record.
	struct current_viewer_review_candidate
	{
		github_review_id       Review;
		github_viewer_actor_id Actor;
		github_review_order    Order;
		github_review_state    State = github_review_state::pending;
		git_commit_oid         Commit;

		auto operator<=>(const current_viewer_review_candidate&) const = default;
	};

	/// \brief Describes the catalog review association record.
	struct catalog_review_association
	{
		review_association_id  Association;
		github_review_id       Review;
		github_viewer_actor_id Actor;
		github_review_order    Order;
		github_review_state    State = github_review_state::pending;
		git_commit_oid         Commit;
		anchor_availability    Availability = anchor_availability::unavailable;

		auto operator<=>(const catalog_review_association&) const = default;
	};

	/// \brief Describes the accepted pull request revision record.
	struct accepted_pull_request_revision
	{
		pull_request_source_revision                   Source;
		pull_request_revision_origin                   Origin = pull_request_revision_origin::live;
		base_branch_id                                 BaseBranch;
		git_commit_oid                                 Base;
		git_commit_oid                                 Head;
		std::optional<current_viewer_review_candidate> CurrentViewerReview;

		auto operator<=>(const accepted_pull_request_revision&) const = default;
	};

	/// \brief Describes the epoch catalog identity record.
	struct epoch_catalog_identity
	{
		epoch_catalog_incarnation_id Incarnation;
		epoch_catalog_revision       Revision;

		auto operator<=>(const epoch_catalog_identity&) const = default;
	};

	/// \brief Describes the epoch history cursor record.
	struct epoch_history_cursor
	{
		pull_request_authority_scope_id Authority;
		epoch_catalog_incarnation_id    Incarnation;
		epoch_catalog_revision          Revision;
		epoch_ordinal                   Before;

		auto operator<=>(const epoch_history_cursor&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const epoch_history_cursor& aCursor) noexcept -> bool
	{
		return aCursor.Authority.valid() && aCursor.Incarnation.valid()
			&& core::valid(aCursor.Revision) && core::valid(aCursor.Before);
	}

	/// \brief Describes the review epoch id record.
	struct review_epoch_id
	{
		pull_request_authority_scope_id Authority;
		epoch_catalog_incarnation_id    Incarnation;
		epoch_ordinal                   Ordinal;

		auto operator<=>(const review_epoch_id&) const = default;
	};

	struct epoch_occurrence_id_tag;
	using epoch_occurrence_id = fixed_identity_256<epoch_occurrence_id_tag>;

	enum class epoch_occurrence_kind : std::uint8_t { force_push_event,
													  observed_head };
	enum class epoch_occurrence_date_source : std::uint8_t { unavailable,
															 github_event,
															 commit_date };

	/// \brief Names provider evidence without any catalogue-local identity.
	struct epoch_occurrence_evidence
	{
		epoch_occurrence_kind Kind    = epoch_occurrence_kind::observed_head;
		std::uint64_t         EventId = 0U;
		git_commit_oid        Head;
		auto                  operator<=>(const epoch_occurrence_evidence&) const = default;
	};

	/// \brief Derives the version-one portable identity from canonical provider facts.
	[[nodiscard]] auto make_epoch_occurrence_id(std::string_view aCanonicalHostname,
												pull_request_id aPullRequest, const epoch_occurrence_evidence& aEvidence) noexcept
		-> std::optional<epoch_occurrence_id>;

	/// \brief Keeps historical truth separate from optional comparison material.
	struct epoch_occurrence
	{
		epoch_occurrence_id            Id;
		epoch_occurrence_evidence      Evidence;
		std::optional<review_epoch_id> LocalEpoch;
		std::optional<git_commit_oid>  HistoricalBase;
		std::optional<git_commit_oid>  DerivedComparisonBase;
		epoch_occurrence_date_source   DateSource          = epoch_occurrence_date_source::unavailable;
		std::int64_t                   DateUtcMilliseconds = 0;
		observed_utc_milliseconds      FirstSeen;
		auto                           operator<=>(const epoch_occurrence&) const = default;
	};

	inline constexpr std::size_t maximum_epoch_occurrence_count = 1'000U;

	/// \brief Describes bounded known occurrences without claiming complete push history.
	struct epoch_occurrence_history
	{
		std::vector<epoch_occurrence> Occurrences;
		bool                          Incomplete                                         = true;
		bool                          Ambiguous                                          = false;
		bool                          UnresolvedLegacy                                   = false;
		auto                          operator<=>(const epoch_occurrence_history&) const = default;
	};

	/// \brief Validates occurrence evidence, optional anchors and labeled dates.
	[[nodiscard]] auto valid(const epoch_occurrence& aOccurrence) noexcept -> bool;
	/// \brief Orders dated entries first, then by UTC date and portable key; this is not push order.
	[[nodiscard]] auto epoch_occurrence_less(const epoch_occurrence& aLeft,
											 const epoch_occurrence& aRight) noexcept -> bool;

	/// \brief Describes the epoch material identity record.
	struct epoch_material_identity
	{
		git_tree_oid               BaseTree;
		git_tree_oid               HeadTree;
		comparison_policy_revision PolicyRevision;

		auto operator<=>(const epoch_material_identity&) const = default;
	};

	enum class epoch_origin : std::uint8_t {
		initial_observation,
		live_observation,
		explicit_historical_import,
		retention_rehydration,
	};

	enum class epoch_base_transition : std::uint8_t {
		not_applicable,
		unchanged,
		base_oid_changed,
		base_branch_changed,
	};

	enum class epoch_head_transition : std::uint8_t {
		not_applicable,
		unchanged,
		descendant,
		non_fast_forward,
		lineage_unknown,
	};

	enum class epoch_material_relation : std::uint8_t {
		same,
		different,
		unknown,
	};

	enum class epoch_primary_cause : std::uint8_t {
		initial_observation,
		explicit_historical_import,
		base_branch_change,
		force_push_rebase,
		author_upload,
		head_change_lineage_unavailable,
		base_update,
		retention_rehydration,
	};

	/// \brief Describes the epoch cause record.
	struct epoch_cause
	{
		epoch_origin            Origin   = epoch_origin::initial_observation;
		epoch_base_transition   Base     = epoch_base_transition::not_applicable;
		epoch_head_transition   Head     = epoch_head_transition::not_applicable;
		epoch_material_relation Material = epoch_material_relation::unknown;
		epoch_primary_cause     Primary  = epoch_primary_cause::initial_observation;

		auto operator<=>(const epoch_cause&) const = default;
	};

	enum class head_lineage : std::uint8_t {
		not_applicable,
		descendant,
		non_fast_forward,
		unknown,
	};

	/// \brief Describes the review epoch summary record.
	struct review_epoch_summary
	{
		review_epoch_id                        Id;
		pull_request_source_revision           Source;
		base_branch_id                         BaseBranch;
		git_commit_oid                         Base;
		git_commit_oid                         Head;
		std::optional<epoch_material_identity> Material;
		epoch_cause                            Cause;
		observed_utc_milliseconds              ObservedAt;
		anchor_availability                    BaseAvailability = anchor_availability::unavailable;
		anchor_availability                    HeadAvailability = anchor_availability::unavailable;

		auto operator<=>(const review_epoch_summary&) const = default;
	};

	enum class comparison_anchor_kind : std::uint8_t {
		epoch_base,
		epoch_head,
		review_commit,
	};

	/// \brief Describes the comparison anchor identity record.
	struct comparison_anchor_identity
	{
		pull_request_authority_scope_id Authority;
		epoch_catalog_incarnation_id    Incarnation;
		comparison_anchor_kind          Kind = comparison_anchor_kind::epoch_base;
		epoch_ordinal                   Epoch;
		review_association_id           Review;

		auto operator<=>(const comparison_anchor_identity&) const = default;
	};

	/// \brief Describes the comparison anchor record.
	struct comparison_anchor
	{
		comparison_anchor_identity Identity;
		git_commit_oid             Commit;
		anchor_availability        Availability = anchor_availability::unavailable;

		auto operator<=>(const comparison_anchor&) const = default;
	};

	/// \brief Describes the comparison pair record.
	struct comparison_pair
	{
		comparison_anchor Left;
		comparison_anchor Right;

		auto operator<=>(const comparison_pair&) const = default;
	};

	enum class named_comparison : std::uint8_t {
		whole_pr,
		previous_upload_to_latest,
		my_last_reviewed_to_latest,
	};

	/// \brief Describes the named comparison selection record.
	struct named_comparison_selection
	{
		named_comparison Alias = named_comparison::whole_pr;

		auto operator<=>(const named_comparison_selection&) const = default;
	};

	/// \brief Describes the explicit comparison selection record.
	struct explicit_comparison_selection
	{
		comparison_anchor_identity Left;
		comparison_anchor_identity Right;

		auto operator<=>(const explicit_comparison_selection&) const = default;
	};

	using comparison_selection = std::variant<named_comparison_selection, explicit_comparison_selection>;

	/// \brief Computes the named selection result.
	[[nodiscard]] constexpr auto named_selection(
		const named_comparison aAlias) noexcept -> comparison_selection
	{
		return named_comparison_selection{ .Alias = aAlias };
	}

	/// \brief Computes the explicit selection result.
	[[nodiscard]] constexpr auto explicit_selection(
		const comparison_anchor_identity& aLeft,
		const comparison_anchor_identity& aRight) noexcept -> comparison_selection
	{
		return explicit_comparison_selection{
			.Left  = aLeft,
			.Right = aRight,
		};
	}

	/// \brief Describes the comparison candidates record.
	struct comparison_candidates
	{
		epoch_catalog_identity              Catalog;
		std::optional<review_epoch_summary> LatestLive;
		std::optional<review_epoch_summary> PreviousDistinctHead;
		std::optional<comparison_anchor>    CurrentViewerReview;
		std::optional<comparison_anchor>    ExplicitLeft;
		std::optional<comparison_anchor>    ExplicitRight;

		auto operator<=>(const comparison_candidates&) const = default;
	};

	enum class retention_rehydration_requirement : std::uint8_t {
		not_required,
		required,
	};

	/// \brief Describes the epoch preparation observation record.
	struct epoch_preparation_observation
	{
		std::optional<review_epoch_summary>       LatestLive;
		std::optional<catalog_review_association> CatalogCurrentViewerReview;
		std::optional<catalog_review_association> ProposedCurrentViewerReview;
		std::optional<epoch_ordinal>              ProposedOrdinal;
		observed_utc_milliseconds                 ObservedAt;
		head_lineage                              HeadLineage = head_lineage::not_applicable;
		std::optional<epoch_material_identity>    Material;
		retention_rehydration_requirement         RetentionRehydration = retention_rehydration_requirement::not_required;

		auto operator<=>(const epoch_preparation_observation&) const = default;
	};

	enum class epoch_catalog_mutation_kind : std::uint8_t {
		append_epoch,
		associate_review,
	};

	/// \brief Describes the append epoch mutation record.
	struct append_epoch_mutation
	{
		review_epoch_summary                      Epoch;
		std::optional<catalog_review_association> Review;

		auto operator<=>(const append_epoch_mutation&) const = default;
	};

	/// \brief Describes the associate review mutation record.
	struct associate_review_mutation
	{
		catalog_review_association Review;

		auto operator<=>(const associate_review_mutation&) const = default;
	};

	using epoch_catalog_mutation = std::variant<append_epoch_mutation, associate_review_mutation>;

	/// \brief Computes the mutation kind result.
	[[nodiscard]] constexpr auto mutation_kind(
		const epoch_catalog_mutation& aMutation) noexcept
		-> epoch_catalog_mutation_kind
	{
		return std::holds_alternative<append_epoch_mutation>(aMutation)
			? epoch_catalog_mutation_kind::append_epoch
			: epoch_catalog_mutation_kind::associate_review;
	}

	/// \brief Describes the epoch load context record.
	struct epoch_load_context
	{
		epoch_request_id               Request;
		application_session_id         ApplicationSession;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;
		epoch_generation               Generation;

		auto operator<=>(const epoch_load_context&) const = default;
	};

	/// \brief Describes the epoch operation context record.
	struct epoch_operation_context
	{
		epoch_request_id               Request;
		application_session_id         ApplicationSession;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;
		epoch_generation               Generation;
		epoch_catalog_incarnation_id   CatalogIncarnation;
		epoch_catalog_revision         ExpectedCatalogRevision;
		epoch_operation_id             Operation;

		auto operator<=>(const epoch_operation_context&) const = default;
	};

	/// \brief Describes the epoch preparation context record.
	struct epoch_preparation_context
	{
		epoch_request_id               Request;
		application_session_id         ApplicationSession;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;
		epoch_generation               Generation;
		epoch_catalog_incarnation_id   CatalogIncarnation;
		epoch_catalog_revision         ExpectedCatalogRevision;

		auto operator<=>(const epoch_preparation_context&) const = default;
	};

	/// \brief Describes the comparison context record.
	struct comparison_context
	{
		epoch_request_id             Request;
		application_session_id       ApplicationSession;
		pull_request_authority       Authority;
		pull_request_source_revision Source;
		epoch_catalog_incarnation_id CatalogIncarnation;
		epoch_catalog_revision       CatalogRevision;
		comparison_generation        Generation;
		comparison_selection         Selection;

		auto operator<=>(const comparison_context&) const = default;
	};

	enum class epoch_history_navigation : std::uint8_t {
		newest,
		older,
	};

	/// \brief Describes the epoch history page context record.
	struct epoch_history_page_context
	{
		epoch_request_id                    Request;
		application_session_id              ApplicationSession;
		pull_request_authority              Authority;
		pull_request_source_revision        Source;
		epoch_generation                    Generation;
		epoch_catalog_identity              Catalog;
		epoch_history_navigation            Navigation = epoch_history_navigation::newest;
		std::optional<epoch_history_cursor> Cursor;
		std::uint16_t                       Limit = maximum_epoch_history_page_size;

		auto operator<=>(const epoch_history_page_context&) const = default;
	};

	/// \brief Describes the epoch history page observation record.
	struct epoch_history_page_observation
	{
		pull_request_authority_scope_id     Authority;
		epoch_catalog_identity              Catalog;
		std::vector<review_epoch_summary>   Epochs;
		std::optional<epoch_history_cursor> Continuation;

		auto operator<=>(const epoch_history_page_observation&) const = default;
	};

	enum class epoch_maintenance_kind : std::uint8_t {
		prune_epochs,
		delete_repository_cache,
	};

	/// \brief Describes the epoch maintenance context record.
	struct epoch_maintenance_context
	{
		epoch_request_id                Request;
		application_session_id          ApplicationSession;
		pull_request_authority          Authority;
		pull_request_source_revision    Source;
		epoch_generation                Generation;
		epoch_catalog_identity          Catalog;
		epoch_maintenance_kind          Kind = epoch_maintenance_kind::prune_epochs;
		repository_cache_scope_id       Cache;
		std::vector<review_epoch_id>    Epochs;
		epoch_maintenance_target_digest TargetDigest;

		auto operator<=>(const epoch_maintenance_context&) const = default;
	};

	/// \brief Describes the pull request revision accepted event record.
	struct pull_request_revision_accepted_event
	{
		application_session_id         ApplicationSession;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;

		auto operator<=>(const pull_request_revision_accepted_event&) const = default;
	};

	/// \brief Describes the pull request revision invalidated event record.
	struct pull_request_revision_invalidated_event
	{
		application_session_id         ApplicationSession;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;
		epoch_generation               Generation;

		auto operator<=>(const pull_request_revision_invalidated_event&) const = default;
	};

	/// \brief Describes the epoch catalog loaded event record.
	struct epoch_catalog_loaded_event
	{
		epoch_load_context     Context;
		epoch_catalog_identity Catalog;

		auto operator<=>(const epoch_catalog_loaded_event&) const = default;
	};

	enum class epoch_failure : std::uint8_t {
		unavailable,
		malformed_observation,
		catalog_conflict,
		quota_exceeded,
		ordinal_exhausted,
		repair_required,
		/// \brief The Git subprocess this work depended on failed.
		///
		/// \details Distinct from `unavailable`, which it used to share with a
		/// cancellation, an invalid path, an unavailable lock, and a spool
		/// failure. Those are five different things to go and look at, and one
		/// code for all of them sent an operator to the wrong one.
		///
		/// BPR-0361 asked whether the epoch path should instead carry the text
		/// Git wrote, as the agent path does since BPR-0348. It should not: the
		/// functional core holds compact typed state, and threading a message
		/// through it to reach a person would put an owning payload where the
		/// architecture deliberately has none. A finer code says which layer
		/// failed, which is what the code is for; the text stays in the adapter
		/// that produced it.
		process_failure,
		/// \brief Installed Git refuses an object in the fetched range.
		///
		/// \details Separate from `process_failure` because it is not a failure
		/// of the command: Git ran, read the pack, and rejected an object its
		/// own fsck rules class as an error. Nothing is broken locally, so
		/// `repair_required` would send an operator to rebuild a cache that is
		/// fine. BPR-0880 keeps the same division of labour BPR-0361 settled:
		/// the code says the range cannot be accepted, and the adapter that ran
		/// Git retains the message class it named.
		unsupported_object,
		git_tool_unsupported,
		git_tool_probe_failed,
	};

	/// \brief Describes the epoch catalog load failed event record.
	struct epoch_catalog_load_failed_event
	{
		epoch_load_context Context;
		epoch_failure      Failure = epoch_failure::unavailable;

		auto operator<=>(const epoch_catalog_load_failed_event&) const = default;
	};

	/// \brief Describes the epoch catalog load cancelled event record.
	struct epoch_catalog_load_cancelled_event
	{
		epoch_load_context Context;

		auto operator<=>(const epoch_catalog_load_cancelled_event&) const = default;
	};

	/// \brief Describes the epoch revision prepared event record.
	struct epoch_revision_prepared_event
	{
		epoch_preparation_context     Context;
		epoch_operation_id            Operation;
		epoch_preparation_observation Observation;

		auto operator<=>(const epoch_revision_prepared_event&) const = default;
	};

	/// \brief Describes the epoch revision prepare failed event record.
	struct epoch_revision_prepare_failed_event
	{
		epoch_preparation_context Context;
		epoch_failure             Failure = epoch_failure::unavailable;

		auto operator<=>(const epoch_revision_prepare_failed_event&) const = default;
	};

	/// \brief Describes the epoch revision prepare cancelled event record.
	struct epoch_revision_prepare_cancelled_event
	{
		epoch_preparation_context Context;

		auto operator<=>(const epoch_revision_prepare_cancelled_event&) const = default;
	};

	/// \brief Describes the epoch catalog committed event record.
	struct epoch_catalog_committed_event
	{
		epoch_operation_context Context;
		epoch_catalog_mutation  Mutation;
		epoch_catalog_revision  NewRevision;

		auto operator<=>(const epoch_catalog_committed_event&) const = default;
	};

	/// \brief Adopts a supplementary historical append after its durable finalization.
	struct epoch_historical_catalog_advanced_event
	{
		application_session_id         ApplicationSession;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;
		epoch_catalog_identity         ExpectedCatalog;
		epoch_catalog_identity         NewCatalog;

		auto operator<=>(const epoch_historical_catalog_advanced_event&) const = default;
	};

	/// \brief Describes the epoch catalog commit failed event record.
	struct epoch_catalog_commit_failed_event
	{
		epoch_operation_context Context;
		epoch_catalog_mutation  Mutation;
		epoch_failure           Failure = epoch_failure::unavailable;

		auto operator<=>(const epoch_catalog_commit_failed_event&) const = default;
	};

	enum class epoch_commit_progress : std::uint8_t {
		before_ref_commit,
		ref_commit_in_progress,
	};

	/// \brief Describes the epoch catalog commit cancelled event record.
	struct epoch_catalog_commit_cancelled_event
	{
		epoch_operation_context Context;
		epoch_catalog_mutation  Mutation;
		epoch_commit_progress   Progress = epoch_commit_progress::before_ref_commit;

		auto operator<=>(const epoch_catalog_commit_cancelled_event&) const = default;
	};

	/// \brief Describes the comparison resolved event record.
	struct comparison_resolved_event
	{
		comparison_context    Context;
		comparison_candidates Candidates;

		auto operator<=>(const comparison_resolved_event&) const = default;
	};

	/// \brief Describes the comparison resolve failed event record.
	struct comparison_resolve_failed_event
	{
		comparison_context Context;
		epoch_failure      Failure = epoch_failure::unavailable;

		auto operator<=>(const comparison_resolve_failed_event&) const = default;
	};

	/// \brief Describes the comparison resolve cancelled event record.
	struct comparison_resolve_cancelled_event
	{
		comparison_context Context;

		auto operator<=>(const comparison_resolve_cancelled_event&) const = default;
	};

	/// \brief Describes the epoch history page loaded event record.
	struct epoch_history_page_loaded_event
	{
		epoch_history_page_context     Context;
		epoch_history_page_observation Page;

		auto operator<=>(const epoch_history_page_loaded_event&) const = default;
	};

	/// \brief Describes the epoch history page load failed event record.
	struct epoch_history_page_load_failed_event
	{
		epoch_history_page_context Context;
		epoch_failure              Failure = epoch_failure::unavailable;

		auto operator<=>(const epoch_history_page_load_failed_event&) const = default;
	};

	/// \brief Describes the epoch history page load cancelled event record.
	struct epoch_history_page_load_cancelled_event
	{
		epoch_history_page_context Context;

		auto operator<=>(const epoch_history_page_load_cancelled_event&) const = default;
	};

	/// \brief Describes the epoch maintenance completed event record.
	struct epoch_maintenance_completed_event
	{
		epoch_maintenance_context Context;
		epoch_catalog_identity    NewCatalog;

		auto operator<=>(const epoch_maintenance_completed_event&) const = default;
	};

	/// \brief Describes the epoch maintenance failed event record.
	struct epoch_maintenance_failed_event
	{
		epoch_maintenance_context Context;
		epoch_failure             Failure = epoch_failure::unavailable;

		auto operator<=>(const epoch_maintenance_failed_event&) const = default;
	};

	enum class epoch_maintenance_progress : std::uint8_t {
		before_point_of_no_return,
		after_point_of_no_return,
	};

	/// \brief Describes the epoch maintenance cancelled event record.
	struct epoch_maintenance_cancelled_event
	{
		epoch_maintenance_context  Context;
		epoch_maintenance_progress Progress = epoch_maintenance_progress::before_point_of_no_return;

		auto operator<=>(const epoch_maintenance_cancelled_event&) const = default;
	};

	/// \brief Describes the select named comparison intent record.
	struct select_named_comparison_intent
	{
		named_comparison       Alias = named_comparison::whole_pr;
		epoch_catalog_revision ExpectedCatalogRevision;

		auto operator<=>(const select_named_comparison_intent&) const = default;
	};

	/// \brief Describes the refresh epochs intent record.
	struct refresh_epochs_intent
	{
		auto operator<=>(const refresh_epochs_intent&) const = default;
	};

	/// \brief Describes the request epoch history page intent record.
	struct request_epoch_history_page_intent
	{
		epoch_catalog_identity              Catalog;
		epoch_history_navigation            Navigation = epoch_history_navigation::newest;
		std::optional<epoch_history_cursor> Cursor;
		std::uint16_t                       Limit = maximum_epoch_history_page_size;

		auto operator<=>(const request_epoch_history_page_intent&) const = default;
	};

	/// \brief Describes the prune review epochs intent record.
	struct prune_review_epochs_intent
	{
		epoch_catalog_identity       Catalog;
		std::vector<review_epoch_id> Epochs;

		auto operator<=>(const prune_review_epochs_intent&) const = default;
	};

	/// \brief Describes the delete repository cache intent record.
	struct delete_repository_cache_intent
	{
		epoch_catalog_identity    Catalog;
		repository_cache_scope_id Cache;

		auto operator<=>(const delete_repository_cache_intent&) const = default;
	};

	/// \brief Describes the cancel epoch maintenance intent record.
	struct cancel_epoch_maintenance_intent
	{
		auto operator<=>(const cancel_epoch_maintenance_intent&) const = default;
	};

	/// \brief Describes the select comparison anchors intent record.
	struct select_comparison_anchors_intent
	{
		comparison_anchor_identity Left;
		comparison_anchor_identity Right;
		epoch_catalog_revision     ExpectedCatalogRevision;

		auto operator<=>(const select_comparison_anchors_intent&) const = default;
	};

	enum class comparison_side : std::uint8_t {
		left,
		right,
	};

	/// \brief Selects a provider occurrence whose retained head must first be imported.
	struct select_epoch_occurrence_intent
	{
		comparison_side        Side = comparison_side::left;
		epoch_occurrence_id    Occurrence;
		epoch_catalog_identity ExpectedCatalog;
		auto                   operator<=>(const select_epoch_occurrence_intent&) const = default;
	};

	/// \brief Binds targeted historical work to one exact authority and selection.
	struct epoch_occurrence_import_context
	{
		application_session_id         ApplicationSession;
		epoch_request_id               Request;
		pull_request_authority         Authority;
		accepted_pull_request_revision Revision;
		epoch_generation               Generation;
		comparison_generation          ComparisonGeneration;
		epoch_catalog_identity         ExpectedCatalog;
		epoch_occurrence_id            Occurrence;
		comparison_side                Side                                                      = comparison_side::left;
		auto                           operator<=>(const epoch_occurrence_import_context&) const = default;
	};

	/// \brief Reports a durably linked occurrence's physically verified head.
	struct epoch_occurrence_imported_event
	{
		epoch_occurrence_import_context Context;
		epoch_catalog_identity          NewCatalog;
		comparison_anchor               Head;
		auto                            operator<=>(const epoch_occurrence_imported_event&) const = default;
	};

	/// \brief Terminates an exact occurrence import without changing selection.
	struct epoch_occurrence_import_failed_event
	{
		epoch_occurrence_import_context Context;
		epoch_failure                   Failure                                                        = epoch_failure::unavailable;
		auto                            operator<=>(const epoch_occurrence_import_failed_event&) const = default;
	};

	/// \brief Terminates cancelled occurrence work without adopting an anchor.
	struct epoch_occurrence_import_cancelled_event
	{
		epoch_occurrence_import_context Context;
		auto                            operator<=>(const epoch_occurrence_import_cancelled_event&) const = default;
	};

	/// \brief Describes the select comparison anchor intent record.
	struct select_comparison_anchor_intent
	{
		comparison_side            Side = comparison_side::left;
		comparison_anchor_identity Anchor;
		epoch_catalog_revision     ExpectedCatalogRevision;

		auto operator<=>(const select_comparison_anchor_intent&) const = default;
	};

	/// \brief Describes the cancel epoch refresh intent record.
	struct cancel_epoch_refresh_intent
	{
		auto operator<=>(const cancel_epoch_refresh_intent&) const = default;
	};
	/// \brief Retires only the exact comparison request superseded by navigation.
	struct cancel_comparison_resolution_intent
	{
		epoch_request_id      Request;
		comparison_generation Generation;
		auto                  operator<=>(const cancel_comparison_resolution_intent&) const = default;
	};

	using epoch_event = std::variant<
		pull_request_revision_accepted_event,
		pull_request_revision_invalidated_event,
		epoch_catalog_loaded_event,
		epoch_catalog_load_failed_event,
		epoch_catalog_load_cancelled_event,
		epoch_revision_prepared_event,
		epoch_revision_prepare_failed_event,
		epoch_revision_prepare_cancelled_event,
		epoch_catalog_committed_event,
		epoch_historical_catalog_advanced_event,
		epoch_catalog_commit_failed_event,
		epoch_catalog_commit_cancelled_event,
		comparison_resolved_event,
		comparison_resolve_failed_event,
		comparison_resolve_cancelled_event,
		epoch_history_page_loaded_event,
		epoch_history_page_load_failed_event,
		epoch_history_page_load_cancelled_event,
		epoch_maintenance_completed_event,
		epoch_maintenance_failed_event,
		epoch_maintenance_cancelled_event,
		refresh_epochs_intent,
		request_epoch_history_page_intent,
		prune_review_epochs_intent,
		delete_repository_cache_intent,
		cancel_epoch_maintenance_intent,
		select_named_comparison_intent,
		select_comparison_anchors_intent,
		select_comparison_anchor_intent,
		cancel_epoch_refresh_intent,
		cancel_comparison_resolution_intent,
		select_epoch_occurrence_intent,
		epoch_occurrence_imported_event,
		epoch_occurrence_import_failed_event,
		epoch_occurrence_import_cancelled_event>;

	/// \brief Describes the invalidate epoch authority request record.
	struct invalidate_epoch_authority_request
	{
		application_session_id                ApplicationSession;
		std::optional<pull_request_authority> PreviousAuthority;
		epoch_generation                      NewGeneration;

		auto operator<=>(const invalidate_epoch_authority_request&) const = default;
	};

	/// \brief Describes the load epoch catalog request record.
	struct load_epoch_catalog_request
	{
		epoch_load_context Context;

		auto operator<=>(const load_epoch_catalog_request&) const = default;
	};

	/// \brief Describes the prepare epoch revision request record.
	struct prepare_epoch_revision_request
	{
		epoch_preparation_context Context;

		auto operator<=>(const prepare_epoch_revision_request&) const = default;
	};

	/// \brief Describes the commit epoch catalog request record.
	struct commit_epoch_catalog_request
	{
		epoch_operation_context Context;
		epoch_catalog_mutation  Mutation;

		auto operator<=>(const commit_epoch_catalog_request&) const = default;
	};

	/// \brief Describes the discard epoch preparation request record.
	struct discard_epoch_preparation_request
	{
		epoch_operation_context Context;

		auto operator<=>(const discard_epoch_preparation_request&) const = default;
	};

	/// \brief Describes the resolve comparison request record.
	struct resolve_comparison_request
	{
		comparison_context Context;

		auto operator<=>(const resolve_comparison_request&) const = default;
	};

	/// \brief Describes the load epoch history page request record.
	struct load_epoch_history_page_request
	{
		epoch_history_page_context Context;

		auto operator<=>(const load_epoch_history_page_request&) const = default;
	};

	/// \brief Describes the perform epoch maintenance request record.
	struct perform_epoch_maintenance_request
	{
		epoch_maintenance_context Context;

		auto operator<=>(const perform_epoch_maintenance_request&) const = default;
	};

	/// \brief Describes the cancel epoch work request record.
	struct cancel_epoch_work_request
	{
		epoch_request_id                                      Request;
		std::variant<epoch_generation, comparison_generation> Generation;

		auto operator<=>(const cancel_epoch_work_request&) const = default;
	};

	/// \brief Requests only the named occurrence, never a history-wide import.
	struct import_epoch_occurrence_request
	{
		epoch_occurrence_import_context Context;
		auto                            operator<=>(const import_epoch_occurrence_request&) const = default;
	};

	using epoch_host_request = std::variant<
		invalidate_epoch_authority_request,
		load_epoch_catalog_request,
		prepare_epoch_revision_request,
		commit_epoch_catalog_request,
		discard_epoch_preparation_request,
		resolve_comparison_request,
		load_epoch_history_page_request,
		perform_epoch_maintenance_request,
		cancel_epoch_work_request,
		import_epoch_occurrence_request>;

	enum class epoch_reason_code : std::uint8_t {
		none,
		invalid_identity,
		invalid_oid,
		invalid_revision,
		invalid_catalog,
		invalid_history_cursor,
		invalid_maintenance_target,
		invalid_observation,
		invalid_selection,
		no_active_authority,
		no_catalog,
		no_resolved_comparison,
		catalog_revision_mismatch,
		stale_completion,
		duplicate_history_page,
		duplicate_maintenance,
		completion_identity_mismatch,
		request_counter_exhausted,
		epoch_generation_exhausted,
		comparison_generation_exhausted,
		epoch_ordinal_exhausted,
		catalog_revision_exhausted,
		comparison_revision_exhausted,
		unavailable,
		catalog_conflict,
		quota_exceeded,
		repair_required,
		process_failure,
		cancelled,
		cancellation_after_point_of_no_return,
		unsupported_object,
		git_tool_unsupported,
		git_tool_probe_failed,
	};

	/// \brief Computes the reason for result.
	[[nodiscard]] auto reason_for(epoch_failure aFailure) noexcept
		-> epoch_reason_code;

	enum class epoch_status : std::uint8_t {
		inactive,
		loading_catalog,
		preparing,
		committing,
		resolving,
		maintaining,
		ready,
		unavailable,
		failed,
		cancelled,
	};

	enum class epoch_command_id : std::uint8_t {
		refresh_epochs,
		select_comparison,
		prune_selected_epochs,
		delete_repository_cache,
		cancel_epoch_refresh,
		cancel_epoch_maintenance,
	};

	/// \brief Describes the epoch command projection record.
	struct epoch_command_projection
	{
		epoch_command_id  Id             = epoch_command_id::refresh_epochs;
		bool              Enabled        = false;
		epoch_reason_code DisabledReason = epoch_reason_code::no_active_authority;

		auto operator<=>(const epoch_command_projection&) const = default;
	};

	/// \brief Describes the epoch command delta record.
	struct epoch_command_delta
	{
		std::array<epoch_command_projection, 6> Commands{ };

		auto operator<=>(const epoch_command_delta&) const = default;
	};

	/// \brief Describes the epoch authority delta record.
	struct epoch_authority_delta
	{
		std::optional<pull_request_authority>         Authority;
		std::optional<accepted_pull_request_revision> Revision;
		epoch_generation                              Generation;

		auto operator<=>(const epoch_authority_delta&) const = default;
	};

	/// \brief Describes the epoch catalog delta record.
	struct epoch_catalog_delta
	{
		std::optional<epoch_catalog_identity> Catalog;
		std::optional<review_epoch_id>        InsertedEpoch;
		std::optional<review_association_id>  ChangedReviewAssociation;
		std::vector<review_epoch_id>          AvailabilityChangedEpochs;
		std::optional<anchor_availability>    AvailabilityChangedTo;
		std::optional<anchor_availability>    AllEpochAvailability;

		auto operator<=>(const epoch_catalog_delta&) const = default;
	};

	/// \brief Describes the accepted epoch delta record.
	struct accepted_epoch_delta
	{
		review_epoch_summary   Epoch;
		epoch_catalog_revision CatalogRevision;

		auto operator<=>(const accepted_epoch_delta&) const = default;
	};

	/// \brief Describes the comparison selection delta record.
	struct comparison_selection_delta
	{
		comparison_selection Selection;

		auto operator<=>(const comparison_selection_delta&) const = default;
	};

	/// \brief Describes the comparison resolution delta record.
	struct comparison_resolution_delta
	{
		comparison_selection           Selection;
		std::optional<comparison_pair> Pair;
		comparison_revision            Revision;
		bool                           AvailableForDiff = false;

		auto operator<=>(const comparison_resolution_delta&) const = default;
	};

	/// \brief Describes the epoch status delta record.
	struct epoch_status_delta
	{
		epoch_status Status = epoch_status::inactive;

		auto operator<=>(const epoch_status_delta&) const = default;
	};

	enum class epoch_history_page_phase : std::uint8_t {
		loading,
		loaded,
		failed,
		cancelled,
		invalidated,
	};

	/// \brief Describes the epoch history page delta record.
	struct epoch_history_page_delta
	{
		epoch_history_page_phase            Phase = epoch_history_page_phase::loading;
		epoch_catalog_identity              Catalog;
		epoch_history_navigation            Navigation = epoch_history_navigation::newest;
		std::optional<epoch_history_cursor> RequestedCursor;
		std::uint16_t                       Limit = maximum_epoch_history_page_size;
		std::vector<review_epoch_summary>   Epochs;
		std::optional<epoch_history_cursor> Continuation;
		epoch_reason_code                   Reason = epoch_reason_code::none;

		auto operator<=>(const epoch_history_page_delta&) const = default;
	};

	/// \brief Describes the epoch diagnostic record.
	struct epoch_diagnostic
	{
		epoch_reason_code Code = epoch_reason_code::none;

		auto operator<=>(const epoch_diagnostic&) const = default;
	};

	enum class epoch_changed_domain : std::uint16_t {
		none        = 0,
		authority   = 1U << 0U,
		catalog     = 1U << 1U,
		epoch       = 1U << 2U,
		selection   = 1U << 3U,
		comparison  = 1U << 4U,
		status      = 1U << 5U,
		diagnostics = 1U << 6U,
		commands    = 1U << 7U,
		history     = 1U << 8U,
		maintenance = 1U << 9U,
	};

	[[nodiscard]] constexpr auto operator|(
		const epoch_changed_domain aLeft, const epoch_changed_domain aRight) noexcept
		-> epoch_changed_domain
	{
		return static_cast<epoch_changed_domain>(
			static_cast<std::uint16_t>(aLeft) | static_cast<std::uint16_t>(aRight));
	}

	/// \brief Reports whether the has changed condition holds.
	[[nodiscard]] constexpr auto has_changed(
		const epoch_changed_domain aValue,
		const epoch_changed_domain aExpected) noexcept -> bool
	{
		return (static_cast<std::uint16_t>(aValue)
				& static_cast<std::uint16_t>(aExpected))
			!= 0U;
	}

	/// \brief Describes the epoch transition output record.
	struct epoch_transition_output
	{
		epoch_changed_domain                       Changed = epoch_changed_domain::none;
		std::vector<epoch_host_request>            Requests;
		std::optional<epoch_authority_delta>       Authority;
		std::optional<epoch_catalog_delta>         Catalog;
		std::optional<accepted_epoch_delta>        AcceptedEpoch;
		std::optional<comparison_selection_delta>  Selection;
		std::optional<comparison_resolution_delta> Comparison;
		std::optional<epoch_history_page_delta>    History;
		std::optional<epoch_status_delta>          Status;
		std::optional<epoch_command_delta>         Commands;
		std::vector<epoch_diagnostic>              Diagnostics;

		auto operator<=>(const epoch_transition_output&) const = default;
	};

	enum class epoch_ingestion_phase : std::uint8_t {
		none,
		loading_catalog,
		preparing,
		committing,
	};

	/// \brief Describes the pending epoch ingestion record.
	struct pending_epoch_ingestion
	{
		epoch_request_id                      Request;
		epoch_generation                      Generation;
		epoch_ingestion_phase                 Phase                 = epoch_ingestion_phase::none;
		bool                                  CancellationRequested = false;
		epoch_operation_id                    Operation;
		std::optional<epoch_catalog_mutation> Mutation;

		auto operator<=>(const pending_epoch_ingestion&) const = default;
	};

	/// \brief Describes the pending comparison resolution record.
	struct pending_comparison_resolution
	{
		epoch_request_id      Request;
		comparison_generation Generation;

		auto operator<=>(const pending_comparison_resolution&) const = default;
	};

	/// \brief Describes the pending epoch history page record.
	struct pending_epoch_history_page
	{
		epoch_request_id                    Request;
		epoch_generation                    Generation;
		epoch_catalog_identity              Catalog;
		epoch_history_navigation            Navigation = epoch_history_navigation::newest;
		std::optional<epoch_history_cursor> Cursor;
		std::uint16_t                       Limit = maximum_epoch_history_page_size;

		auto operator<=>(const pending_epoch_history_page&) const = default;
	};

	/// \brief Describes the pending epoch maintenance record.
	struct pending_epoch_maintenance
	{
		epoch_request_id                Request;
		epoch_generation                Generation;
		epoch_catalog_identity          Catalog;
		epoch_maintenance_kind          Kind = epoch_maintenance_kind::prune_epochs;
		repository_cache_scope_id       Cache;
		std::uint16_t                   EpochCount = 0U;
		epoch_maintenance_target_digest TargetDigest;
		bool                            CancellationRequested = false;

		auto operator<=>(const pending_epoch_maintenance&) const = default;
	};

	/// \brief Keeps the import identity compact; authority remains in epoch state.
	struct pending_epoch_occurrence_import
	{
		epoch_request_id    Request;
		epoch_occurrence_id Occurrence;
		comparison_side     Side                                                      = comparison_side::left;
		auto                operator<=>(const pending_epoch_occurrence_import&) const = default;
	};

	/// \brief Describes the epoch state record.
	struct epoch_state
	{
		application_session_id                         ApplicationSession;
		epoch_request_id                               NextRequest{ 1 };
		bool                                           RequestCounterExhausted = false;
		epoch_generation                               EpochGeneration;
		comparison_generation                          ComparisonGeneration;
		std::optional<pull_request_authority>          Authority;
		std::optional<accepted_pull_request_revision>  AcceptedRevision;
		std::optional<epoch_catalog_identity>          Catalog;
		comparison_selection                           RequestedSelection = named_selection(named_comparison::whole_pr);
		std::optional<comparison_pair>                 AcceptedComparison;
		comparison_revision                            AcceptedComparisonRevision;
		std::optional<pending_epoch_ingestion>         PendingIngestion;
		std::optional<pending_comparison_resolution>   PendingComparison;
		std::optional<pending_epoch_occurrence_import> PendingOccurrence;
		std::optional<pending_epoch_history_page>      PendingHistory;
		std::optional<pending_epoch_maintenance>       PendingMaintenance;
		std::optional<epoch_catalog_identity>          HistoryCatalog;
		std::optional<epoch_history_navigation>        HistoryNavigation;
		std::optional<epoch_history_cursor>            HistoryContinuation;
		epoch_status                                   Status = epoch_status::inactive;

		auto operator<=>(const epoch_state&) const = default;
	};

	static_assert(std::is_trivially_copyable_v<epoch_state>);
	static_assert(std::is_trivially_destructible_v<epoch_state>);
	// The pending occurrence adds one constant-size request/id/side tuple, not history.
	static_assert(sizeof(epoch_state) <= 2'112U);
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_EPOCH_MODEL_HPP
