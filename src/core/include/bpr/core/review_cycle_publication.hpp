#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_PUBLICATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_PUBLICATION_HPP

#include "bpr/core/git_objects.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_candidate.hpp"
#include "bpr/core/review_cycle_outgoing.hpp"
#include "bpr/core/review_cycle_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// \file
/// \brief The finite publication plan, its authorization and the recovery
/// journal (RFC-BPR-0059, BPR-1174).
/// \details A plan binds everything that goes public — the exact candidate,
/// the destination host, account, repository, base and new branch, and the
/// pull request's title, body and draft choice — into one digest. Only the
/// reviewer's explicit confirmation of that digest, under the plan's own
/// account, authorizes it; any change is a new plan with a new digest, so an
/// older confirmation authorizes nothing. The journal records each possible
/// remote effect before it is attempted and what was observed after. An
/// effect whose outcome was not observed is uncertain: after a restart or a
/// lost answer, recovery only reads, and any remaining write needs a fresh
/// authorization. Nothing here claims a rollback, and cancellation undoes no
/// remote effect.
namespace bpr::core {
	inline constexpr std::size_t maximum_publication_host_bytes   = 253U;
	inline constexpr std::size_t maximum_publication_login_bytes  = 39U;
	inline constexpr std::size_t maximum_publication_owner_bytes  = 39U;
	inline constexpr std::size_t maximum_publication_name_bytes   = 100U;
	inline constexpr std::size_t maximum_publication_branch_bytes = 244U;
	inline constexpr std::size_t maximum_publication_body_bytes   = 65536U;
	inline constexpr std::size_t maximum_publication_effects      = 4096U;
	inline constexpr std::size_t maximum_publication_remote_bytes = 2048U;

	/// \brief Where the result goes, as the reviewer confirmed it.
	struct publication_destination
	{
		/// The canonical host name of the account's GitHub or GHES host.
		std::string       Host;
		github_account_id Account;
		std::string       Login;
		std::string       Owner;
		std::string       Repository;
		/// The existing branch the pull request targets.
		std::string BaseBranch;
		/// The branch to create. It must not exist; nothing overwrites it.
		std::string NewBranch;
		auto        operator<=>(const publication_destination&) const = default;
	};

	/// \brief One finite publication: exactly what goes out, where, as whom.
	struct publication_plan
	{
		cycle_source_revision_id              Start;
		cycle_source_revision_id              Final;
		outgoing_selection_mode               Mode   = outgoing_selection_mode::starting_work_and_improvements;
		git_hash_algorithm                    Format = git_hash_algorithm::invalid;
		git_commit_oid                        Parent;
		git_tree_oid                          Tree;
		git_commit_oid                        Commit;
		git_commit_content                    Content;
		std::vector<candidate_blob>           Blobs;
		std::vector<std::string>              Deleted;
		std::vector<candidate_transformation> Transformations;
		/// The target head the plan was made against, when one was observed.
		std::optional<git_commit_oid> TargetHead;
		publication_destination       Destination;
		std::string                   PullTitle;
		std::string                   PullBody;
		bool                          Draft                                      = false;
		auto                          operator<=>(const publication_plan&) const = default;
	};

	enum class publication_refusal : std::uint8_t {
		/// A field is malformed or out of bounds.
		invalid_plan,
		/// Another plan's digest: the plan changed after it was confirmed.
		stale_plan,
		/// Only the reviewer, through the publication confirmation, authorizes.
		not_reviewer,
		/// The account is not the plan's.
		authority_changed,
		/// No current authorization for this write.
		unauthorized,
		/// The effect is out of order or names something the plan does not.
		out_of_order,
		/// That effect was already confirmed; it is never sent twice.
		duplicate,
		/// An effect is still unresolved: recover by reading first.
		unresolved,
		/// The publication is linked, failed or cancelled.
		closed,
		/// The sequence names no effect in the needed state.
		unknown_effect,
	};
	[[nodiscard]] auto name(publication_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Validates destination fields: host and names by GitHub's rules,
	/// branches as safe new reference names that differ.
	[[nodiscard]] auto valid(const publication_destination& aDestination) -> bool;
	/// \brief A branch name Git and GitHub both accept as a new reference.
	[[nodiscard]] auto valid_publication_branch(std::string_view aName) -> bool;
	/// \brief An account login or repository owner by GitHub's rules.
	[[nodiscard]] auto valid_publication_owner(std::string_view aName) -> bool;
	/// \brief A repository name by GitHub's rules.
	[[nodiscard]] auto valid_publication_repository(std::string_view aName) -> bool;

	/// \brief Binds a built candidate to a destination and pull request text.
	[[nodiscard]] auto make_publication_plan(const outgoing_plan& aPlan, const publication_candidate& aCandidate, const publication_destination& aDestination,
											 std::string aPullTitle, std::string aPullBody, bool aDraft) -> std::expected<publication_plan, publication_refusal>;

	/// \brief The plan's canonical bytes and their SHA-256, which a
	/// confirmation must name.
	[[nodiscard]] auto encode_publication_plan(const publication_plan& aPlan) -> std::vector<std::uint8_t>;
	[[nodiscard]] auto decode_publication_plan(std::span<const std::uint8_t> aBytes) -> std::optional<publication_plan>;
	[[nodiscard]] auto publication_plan_digest(const publication_plan& aPlan) -> cycle_material_digest;

	enum class publication_phase : std::uint8_t {
		/// Recorded; nothing authorized.
		prepared,
		/// Authorized; nothing attempted yet.
		authorized,
		uploading_objects,
		branch_created,
		pr_creation_pending,
		/// The pull request exists and is associated.
		linked,
		/// The server definitively refused a write.
		failed,
		/// An attempted effect's outcome is unknown; recover by reading first.
		uncertain,
		/// The branch name is taken by something else, or the target moved
		/// so that the plan no longer describes what would be published.
		target_drift,
		/// The reviewer cancelled. Confirmed remote effects remain.
		cancelled,
		/// A newer plan for the cycle replaced this one before anything was
		/// attempted; its confirmation authorizes nothing.
		superseded,
	};
	[[nodiscard]] auto name(publication_phase aPhase) noexcept -> std::string_view;

	enum class publication_effect_kind : std::uint8_t {
		blob,
		tree,
		commit,
		branch,
		pull_request,
	};
	[[nodiscard]] auto name(publication_effect_kind aKind) noexcept -> std::string_view;

	enum class publication_effect_state : std::uint8_t {
		/// Recorded before the attempt; no outcome yet.
		intended,
		/// The remote effect exists and matches the plan.
		confirmed,
		/// The server definitively refused it.
		refused,
		/// It never left the machine.
		not_sent,
		/// It may have taken effect; nobody knows yet.
		uncertain,
		/// A read after uncertainty found no such effect.
		absent,
		/// Something else holds the name (a branch at another commit, a pull
		/// request nobody can tie to this plan).
		foreign,
	};
	[[nodiscard]] auto name(publication_effect_state aState) noexcept -> std::string_view;

	struct publication_effect
	{
		std::uint64_t           Sequence = 0;
		publication_effect_kind Kind     = publication_effect_kind::blob;
		/// The object identity, branch name or `pull`.
		std::string              Subject;
		publication_effect_state State = publication_effect_state::intended;
		/// The authorization it was attempted under.
		std::uint64_t Generation = 0;
		/// What the provider named: a created object or reference, a pull
		/// request's number and address.
		std::string Remote;
		auto        operator<=>(const publication_effect&) const = default;
	};

	/// \brief The pull request as last observed.
	struct publication_link
	{
		std::uint64_t  Number = 0;
		std::string    NodeId;
		std::string    Url;
		git_commit_oid Head;
		std::string    BaseBranch;
		/// The observed head or base is not what the plan named.
		bool Drifted                                    = false;
		auto operator<=>(const publication_link&) const = default;
	};

	struct publication_journal
	{
		cycle_material_digest PlanDigest;
		publication_phase     Phase = publication_phase::prepared;
		/// The current authorization; zero when none is current.
		std::uint64_t                   Generation     = 0;
		std::uint64_t                   NextGeneration = 1;
		std::vector<publication_effect> Effects;
		std::optional<publication_link> Link;
		auto                            operator<=>(const publication_journal&) const = default;
	};

	[[nodiscard]] auto encode_publication_journal(const publication_journal& aJournal) -> std::vector<std::uint8_t>;
	[[nodiscard]] auto decode_publication_journal(std::span<const std::uint8_t> aBytes) -> std::optional<publication_journal>;

	/// \brief A new journal for a recorded plan.
	[[nodiscard]] auto start_publication(const publication_plan& aPlan) -> publication_journal;

	/// \brief The reviewer's explicit confirmation of one plan digest in the
	/// publication flow, under the account now authenticated. An imported
	/// approval, a granted consumer or a digest of another plan authorizes
	/// nothing.
	struct authorize_publication
	{
		/// The cycle the publication belongs to.
		local_cycle_subject   Subject;
		cycle_actor           Actor = cycle_actor::imported_content;
		cycle_material_digest PlanDigest;
		github_account_id     Account;
	};
	/// \brief Records an effect before it is attempted, under a current
	/// authorization and the account now authenticated.
	struct begin_publication_effect
	{
		publication_effect_kind Kind = publication_effect_kind::blob;
		std::string             Subject;
		std::uint64_t           Generation = 0;
		github_account_id       Account;
	};
	/// \brief What the attempt observed. A pull request's confirmation carries its link.
	struct complete_publication_effect
	{
		std::uint64_t                   Sequence = 0;
		publication_effect_state        Outcome  = publication_effect_state::uncertain;
		std::string                     Remote;
		std::optional<publication_link> Link;
	};
	/// \brief What a read after uncertainty found for one effect.
	struct observe_publication_effect
	{
		std::uint64_t                   Sequence = 0;
		publication_effect_state        Observed = publication_effect_state::uncertain;
		std::string                     Remote;
		std::optional<publication_link> Link;
	};
	/// \brief After a restart: unobserved attempts become uncertain and no
	/// authorization survives.
	struct restart_publication
	{
	};
	struct cancel_publication
	{
	};
	/// \brief A newer plan replaced this one. Only a plan with no attempted
	/// effect is superseded; one with effects is cancelled or recovered.
	struct supersede_publication
	{
	};
	/// \brief A later read of the linked pull request.
	struct refresh_publication_link
	{
		publication_link Link;
	};
	using publication_command = std::variant<authorize_publication, begin_publication_effect, complete_publication_effect, observe_publication_effect, restart_publication,
											 cancel_publication, refresh_publication_link, supersede_publication>;

	struct publication_outcome
	{
		publication_journal Journal;
		/// The sequence `begin_publication_effect` recorded.
		std::uint64_t Sequence = 0;
	};

	/// \brief Decides one command. A refusal changes nothing.
	[[nodiscard]] auto apply_publication(const publication_plan& aPlan, const publication_journal& aJournal, const publication_command& aCommand)
		-> std::expected<publication_outcome, publication_refusal>;

	/// \brief The subjects still to be written, in order, for the current state.
	struct publication_step
	{
		publication_effect_kind Kind = publication_effect_kind::blob;
		std::string             Subject;
		auto                    operator<=>(const publication_step&) const = default;
	};
	[[nodiscard]] auto next_publication_step(const publication_plan& aPlan, const publication_journal& aJournal) -> std::optional<publication_step>;

	/// \brief Everything the reviewer confirms, in words: the three
	/// baselines, the exact candidate and its attribution, the destination
	/// and the account, the pull request's title, body and draft choice, and
	/// the digest a confirmation names.
	[[nodiscard]] auto render_publication_preview(const publication_plan& aPlan) -> std::string;
}

#endif
