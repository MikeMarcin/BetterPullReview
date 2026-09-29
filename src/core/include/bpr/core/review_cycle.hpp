#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_HPP

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/profile_window.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// \file
/// \brief Review Cycles (RFC-BPR-0053): a local review subject with its own
/// identities, independent of pull requests, accounts and agents.
/// \details The core holds identities, revisions and decisions only. Source
/// bytes, manifests and history pages stay with the host; the core sees
/// bounded summaries of them.
namespace bpr::core {
	struct review_cycle_id_tag;
	struct cycle_source_revision_id_tag;
	struct cycle_feedback_item_id_tag;
	struct cycle_round_draft_id_tag;
	struct cycle_round_id_tag;
	struct cycle_response_id_tag;
	struct cycle_verification_id_tag;
	struct cycle_publication_id_tag;

	using review_cycle_id           = strong_id<review_cycle_id_tag>;
	using cycle_source_revision_id  = strong_id<cycle_source_revision_id_tag>;
	using cycle_feedback_item_id    = strong_id<cycle_feedback_item_id_tag>;
	using cycle_round_draft_id      = strong_id<cycle_round_draft_id_tag>;
	using cycle_round_id            = strong_id<cycle_round_id_tag>;
	using cycle_response_id         = strong_id<cycle_response_id_tag>;
	using cycle_verification_id     = strong_id<cycle_verification_id_tag>;
	using cycle_publication_id      = strong_id<cycle_publication_id_tag>;
	using cycle_catalog_incarnation = fixed_identity_256<struct cycle_catalog_incarnation_tag>;
	using cycle_material_digest     = fixed_identity_256<struct cycle_material_digest_tag>;
	using cycle_round_digest        = fixed_identity_256<struct cycle_round_digest_tag>;

	/// Every counter stays inside SQLite's signed integer domain.
	inline constexpr std::uint64_t maximum_cycle_identity         = std::numeric_limits<std::int64_t>::max();
	inline constexpr std::size_t   maximum_cycle_title_bytes      = 256U;
	inline constexpr std::size_t   maximum_cycle_scope_paths      = 1024U;
	inline constexpr std::size_t   maximum_cycle_scope_path_bytes = 4096U;
	inline constexpr std::uint64_t maximum_cycle_capture_entries  = 250'000U;

	/// \brief The local authority a cycle lives under: one BPR profile's cycle
	/// catalog. A replacement catalog has a new incarnation, so its identities
	/// never resolve against records of an older one.
	struct local_cycle_authority
	{
		bpr_profile_id            Profile;
		cycle_catalog_incarnation Incarnation;
		auto                      operator<=>(const local_cycle_authority&) const = default;
	};

	/// \brief A local review cycle as a review subject.
	/// \details Generation distinguishes a cycle recreated under a reused
	/// catalog row from the one a stale request named.
	struct local_cycle_subject
	{
		local_cycle_authority Authority;
		review_cycle_id       Cycle;
		std::uint64_t         Generation                                    = 0;
		auto                  operator<=>(const local_cycle_subject&) const = default;
	};

	/// \brief What a review is about: an exact pull request under its existing
	/// GitHub authority, or a local cycle. Neither alternative is a degraded
	/// form of the other; a cycle carries no provider identity.
	using review_subject = std::variant<pull_request_authority, local_cycle_subject>;

	/// \brief A record identity bound to the cycle that allocated it.
	template <typename TTag>
	struct cycle_record_ref
	{
		review_cycle_id Cycle;
		strong_id<TTag> Id;
		auto            operator<=>(const cycle_record_ref&) const = default;
	};
	using cycle_source_revision_ref = cycle_record_ref<cycle_source_revision_id_tag>;
	using cycle_feedback_item_ref   = cycle_record_ref<cycle_feedback_item_id_tag>;
	using cycle_round_draft_ref     = cycle_record_ref<cycle_round_draft_id_tag>;
	using cycle_round_ref           = cycle_record_ref<cycle_round_id_tag>;
	using cycle_response_ref        = cycle_record_ref<cycle_response_id_tag>;
	using cycle_verification_ref    = cycle_record_ref<cycle_verification_id_tag>;
	using cycle_publication_ref     = cycle_record_ref<cycle_publication_id_tag>;

	/// \brief Checks the positive, non-wrapping durable identity domain.
	template <typename TTag>
	[[nodiscard]] constexpr auto valid_cycle_id(const strong_id<TTag> aId) noexcept -> bool
	{
		return aId.value() > 0 && aId.value() <= maximum_cycle_identity;
	}
	[[nodiscard]] auto valid(const local_cycle_authority& aAuthority) noexcept -> bool;
	[[nodiscard]] auto valid(const local_cycle_subject& aSubject) noexcept -> bool;
	/// \brief True when the reference is valid and was allocated by this cycle.
	template <typename TTag>
	[[nodiscard]] constexpr auto belongs_to(const cycle_record_ref<TTag>& aRef, const local_cycle_subject& aSubject) noexcept -> bool
	{
		return aRef.Cycle == aSubject.Cycle && valid_cycle_id(aRef.Id);
	}

	/// \brief Next unused identity of each record kind. Counters only advance;
	/// an identity is never reissued, even after its record is removed.
	struct cycle_identity_counters
	{
		std::uint64_t SourceRevision                                    = 1;
		std::uint64_t FeedbackItem                                      = 1;
		std::uint64_t RoundDraft                                        = 1;
		std::uint64_t Round                                             = 1;
		std::uint64_t Response                                          = 1;
		std::uint64_t Verification                                      = 1;
		std::uint64_t Publication                                       = 1;
		auto          operator<=>(const cycle_identity_counters&) const = default;
	};

	/// \brief The review scope: the whole selected root, or a named path set.
	/// \details Paths are root-relative, `/`-separated, sorted and unique, with
	/// no empty, `.` or `..` segment. Every change is a new version, so a
	/// narrower scope never reads as deletion of what it no longer names.
	struct cycle_scope_specification
	{
		std::uint64_t            Version = 1;
		std::vector<std::string> Paths;
		auto                     operator<=>(const cycle_scope_specification&) const = default;
	};

	enum class cycle_capture_coverage : std::uint8_t {
		/// Every admitted path in the scope was enumerated.
		complete,
		/// Some of the scope was enumerated; unlisted paths are unknown.
		partial,
	};
	enum class cycle_capture_quality : std::uint8_t {
		/// Repeated reads agreed; not a point-in-time snapshot.
		observed_stable,
		/// A qualified snapshot backend vouched for one instant.
		atomic_snapshot,
	};

	/// \brief Bounded host summary of one capture. The manifest itself stays
	/// in the host's content store, named by its material digest.
	struct cycle_capture_observation
	{
		std::uint64_t          ScopeVersion = 0;
		cycle_material_digest  Material;
		std::uint64_t          Entries                                             = 0;
		std::uint64_t          ExplicitDeletions                                   = 0;
		std::uint64_t          Unavailable                                         = 0;
		cycle_capture_coverage Coverage                                            = cycle_capture_coverage::partial;
		cycle_capture_quality  Quality                                             = cycle_capture_quality::observed_stable;
		auto                   operator<=>(const cycle_capture_observation&) const = default;
	};

	/// \brief One immutable captured source revision.
	/// \details Identity orders observations; the material digest says what
	/// bytes they hold. Two revisions may share material and stay distinct.
	struct cycle_source_revision
	{
		cycle_source_revision_ref               Id;
		std::optional<cycle_source_revision_id> Parent;
		cycle_capture_observation               Capture;
		auto                                    operator<=>(const cycle_source_revision&) const = default;
	};

	enum class cycle_lifecycle : std::uint8_t {
		active,
		archived,
	};

	/// \brief Compact cycle state held by the core.
	/// \details `Initial` is absent until the first capture: an uncaptured
	/// cycle is not an empty one. Revision is the optimistic record revision
	/// every mutation must name.
	struct review_cycle_state
	{
		local_cycle_subject                  Subject;
		std::uint64_t                        Revision = 1;
		std::string                          Title;
		cycle_lifecycle                      Lifecycle = cycle_lifecycle::active;
		cycle_scope_specification            Scope;
		std::optional<cycle_source_revision> Initial;
		std::optional<cycle_source_revision> Latest;
		cycle_identity_counters              Next;
		auto                                 operator<=>(const review_cycle_state&) const = default;
	};

	/// \brief One profile catalog's cycle allocator.
	struct review_cycle_catalog
	{
		local_cycle_authority Authority;
		std::uint64_t         NextCycle                                      = 1;
		auto                  operator<=>(const review_cycle_catalog&) const = default;
	};

	enum class cycle_refusal : std::uint8_t {
		none,
		/// Zero, overflowing, foreign-profile or foreign-cycle identity.
		invalid_identity,
		/// Expected revision is not the current one.
		conflict,
		/// Title, path or count exceeds its bound.
		bounds,
		/// Identity space exhausted; nothing was allocated.
		exhausted,
		/// Scope path is not a normalized root-relative path set.
		invalid_scope,
		/// Capture named another scope version or is internally inconsistent.
		invalid_capture,
		/// Only unarchiving is accepted on an archived cycle.
		archived,
	};

	struct create_review_cycle
	{
		local_cycle_authority     Authority;
		std::string               Title;
		cycle_scope_specification Scope;
	};
	struct created_review_cycle
	{
		review_cycle_catalog Catalog;
		review_cycle_state   Cycle;
	};
	/// \brief Creates a local cycle with no source captured yet. Needs no pull
	/// request, remote, commit, account or agent.
	[[nodiscard]] auto create(const review_cycle_catalog& aCatalog, const create_review_cycle& aCommand)
		-> std::expected<created_review_cycle, cycle_refusal>;

	struct rename_review_cycle
	{
		std::string Title;
	};
	struct archive_review_cycle
	{
	};
	struct unarchive_review_cycle
	{
	};
	struct change_cycle_scope
	{
		std::vector<std::string> Paths;
	};
	/// \brief Records a capture. Identical material to the latest revision is a
	/// no-op unless `Deliberate`, which records another handoff observation.
	struct record_cycle_capture
	{
		cycle_capture_observation Capture;
		bool                      Deliberate = false;
	};
	using review_cycle_change = std::variant<rename_review_cycle, archive_review_cycle, unarchive_review_cycle, change_cycle_scope, record_cycle_capture>;

	struct review_cycle_command
	{
		local_cycle_subject Subject;
		std::uint64_t       ExpectedRevision = 0;
		review_cycle_change Change;
	};
	struct review_cycle_outcome
	{
		review_cycle_state                       Accepted;
		cycle_refusal                            Refusal = cycle_refusal::none;
		bool                                     Changed = false;
		std::optional<cycle_source_revision_ref> Captured;
	};
	/// \brief Applies one command. A refusal or no-op returns the input state.
	[[nodiscard]] auto apply(const review_cycle_state& aCycle, const review_cycle_command& aCommand) -> review_cycle_outcome;

	/// \brief Validates a scope path set; `none` when acceptable.
	[[nodiscard]] auto validate_scope_paths(const std::vector<std::string>& aPaths) -> cycle_refusal;
	/// \brief True when a normalized path lies in the scope.
	[[nodiscard]] auto scope_contains(const cycle_scope_specification& aScope, std::string_view aPath) -> bool;

	/// \brief What one capture says about one path.
	struct cycle_path_observation
	{
		bool Listed            = false;
		bool ExplicitlyDeleted = false;
		bool Unavailable       = false;
	};
	enum class cycle_path_state : std::uint8_t {
		present,
		/// Listed as deleted by the capture.
		deleted,
		/// Unlisted by a complete capture of a scope that contains it.
		absent,
		/// Unlisted by a partial capture; nothing is known.
		unknown,
		/// Outside the capture's scope; nothing is known.
		outside_scope,
		/// Seen but not readable or admitted.
		unavailable,
	};
	/// \brief Classifies one path in one capture.
	[[nodiscard]] auto classify_path(const cycle_scope_specification& aScope, cycle_capture_coverage aCoverage,
									 std::string_view aPath, const cycle_path_observation& aObservation) -> cycle_path_state;

	enum class cycle_path_change : std::uint8_t {
		unchanged,
		modified,
		added,
		deleted,
		/// One side does not know the path; no change is claimed.
		unknown,
	};
	/// \brief Compares one path across two captures. Only a known presence
	/// against a known absence or deletion is a deletion.
	[[nodiscard]] auto compare_path(cycle_path_state aBefore, const std::optional<cycle_material_digest>& aBeforeBytes,
									cycle_path_state aAfter, const std::optional<cycle_material_digest>& aAfterBytes) -> cycle_path_change;

	/// \brief Distinct identities, same bytes.
	[[nodiscard]] auto same_material(const cycle_source_revision& aLeft, const cycle_source_revision& aRight) noexcept -> bool;

	/// \brief A mutable next assignment. Freezing does not change it.
	struct cycle_round_draft
	{
		cycle_round_draft_ref Id;
		std::uint64_t         Revision                                    = 1;
		auto                  operator<=>(const cycle_round_draft&) const = default;
	};
	/// \brief An immutable frozen round and the exact draft revision it froze.
	struct cycle_round
	{
		cycle_round_ref       Id;
		cycle_round_draft_ref Draft;
		std::uint64_t         DraftRevision = 0;
		cycle_round_digest    Digest;
		auto                  operator<=>(const cycle_round&) const = default;
	};
	/// \brief Allocates a new round draft.
	[[nodiscard]] auto begin_round_draft(const review_cycle_state& aCycle, std::uint64_t aExpectedRevision)
		-> std::expected<std::pair<review_cycle_state, cycle_round_draft>, cycle_refusal>;
	/// \brief Advances a draft's revision for one confirmed edit.
	[[nodiscard]] auto edit_round_draft(const cycle_round_draft& aDraft, std::uint64_t aExpectedRevision)
		-> std::expected<cycle_round_draft, cycle_refusal>;
	struct frozen_cycle_round
	{
		review_cycle_state Cycle;
		cycle_round        Round;
		bool               Reused = false;
	};
	/// \brief Freezes a draft revision. Freezing the same draft revision with
	/// the same payload digest again reuses `aPrior`; any other repeat is a
	/// conflict. The round never changes after this.
	[[nodiscard]] auto freeze_round(const review_cycle_state& aCycle, const cycle_round_draft& aDraft,
									std::uint64_t aExpectedDraftRevision, const cycle_round_digest& aDigest,
									const std::optional<cycle_round>& aPrior) -> std::expected<frozen_cycle_round, cycle_refusal>;

	/// \brief How far a round's delivery is evidenced. Stronger states need
	/// stronger evidence; a saved file proves neither receipt nor reading.
	enum class cycle_delivery_state : std::uint8_t {
		copied,
		saved,
		retrieved,
		accepted_by_adapter,
		recipient_acknowledged,
		failed,
		uncertain,
	};
	/// \brief What a recipient claims about an item.
	enum class cycle_response_claim : std::uint8_t {
		addressed,
		partially_addressed,
		disagreed,
		blocked,
		question,
		not_attempted,
	};
	/// \brief The reviewer's own judgment of an item.
	enum class cycle_reviewer_decision : std::uint8_t {
		verified,
		needs_work,
		withdrawn,
		deferred,
	};
	/// \brief Recipient claims and reviewer judgment are separate dimensions.
	/// No response is not `not_attempted`, and no claim sets a decision.
	struct cycle_item_judgment
	{
		std::optional<cycle_response_claim>    Claim;
		std::optional<cycle_reviewer_decision> Decision;
		auto                                   operator<=>(const cycle_item_judgment&) const = default;
	};
	[[nodiscard]] auto record_claim(const cycle_item_judgment& aJudgment, cycle_response_claim aClaim) -> cycle_item_judgment;
	/// \brief Sets or, with `std::nullopt`, explicitly clears the decision.
	[[nodiscard]] auto record_decision(const cycle_item_judgment& aJudgment, std::optional<cycle_reviewer_decision> aDecision) -> cycle_item_judgment;

	/// \brief Bounded view of a cycle for presentation and CLI output.
	struct review_cycle_projection
	{
		local_cycle_subject                      Subject;
		std::uint64_t                            Revision = 0;
		std::string                              Title;
		cycle_lifecycle                          Lifecycle    = cycle_lifecycle::active;
		std::uint64_t                            ScopeVersion = 0;
		std::size_t                              ScopePaths   = 0;
		bool                                     Captured     = false;
		bool                                     InitialEmpty = false;
		std::optional<cycle_capture_coverage>    LatestCoverage;
		std::optional<cycle_source_revision_ref> Latest;
		std::uint64_t                            Revisions                                         = 0;
		auto                                     operator<=>(const review_cycle_projection&) const = default;
	};
	[[nodiscard]] auto project(const review_cycle_state& aCycle) -> review_cycle_projection;
}

#endif
