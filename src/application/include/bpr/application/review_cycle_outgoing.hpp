#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_OUTGOING_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_OUTGOING_HPP

#include "bpr/application/publication_candidate_port.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/review_material_bridge.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/review_cycle_candidate.hpp"
#include "bpr/core/review_cycle_outgoing.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief Planning a cycle's outgoing content over an explicit Git base
/// (RFC-BPR-0059, BPR-1172).
/// \details The starting and final captures come from the cycle store,
/// verified against their recorded summaries; their bytes come only from
/// retained material, never from the workspace. P and T are named by the
/// reviewer: a reference is read as data from the bound checkout's Git
/// directory, and P's complete tree through the publication candidate port,
/// checked to hash to the commit asked for. Planning writes nothing anywhere.
namespace bpr::application {
	struct cycle_outgoing_request
	{
		/// R0; the cycle's first capture when absent.
		std::optional<core::cycle_source_revision_id> Start;
		/// Rn; the latest capture when absent.
		std::optional<core::cycle_source_revision_id> Final;
		/// P: `HEAD`, a `refs/` name, or an exact commit identity.
		std::string Base;
		/// T: a `refs/` name in the checkout, read as a local observation.
		std::optional<std::string>           Target;
		core::outgoing_selection_mode        Mode = core::outgoing_selection_mode::starting_work_and_improvements;
		std::vector<core::outgoing_override> Overrides;
	};

	enum class cycle_outgoing_status : std::uint8_t {
		planned,
		/// The cycle has no workspace binding.
		not_bound,
		/// A named capture does not exist, or nothing was captured yet.
		not_captured,
		/// The location answers with another root; an explicit rebind is needed.
		root_changed,
		/// The workspace port refused; see `Workspace`.
		workspace,
		/// A named reference is malformed or does not resolve.
		reference_unavailable,
		/// Local Git refused; see `Git`.
		repository,
		/// Retained material is missing or corrupt, or the store refused.
		material,
		/// The core refused; see `Refusal`.
		refused,
		/// The candidate cannot be built natively, or nothing would change;
		/// see the candidate or external outcome's `Limit`.
		limited,
		cancelled,
	};

	struct cycle_outgoing_outcome
	{
		cycle_outgoing_status                 Status = cycle_outgoing_status::cancelled;
		std::optional<workspace_failure>      Workspace;
		std::optional<publication_git_error>  Git;
		std::optional<review_material_status> Material;
		std::optional<core::outgoing_refusal> Refusal;
		std::optional<core::outgoing_plan>    Plan;
		/// The readable preview of `Plan`.
		std::string Preview;
	};

	/// \brief Resolves a reference name to a commit, reading only the
	/// checkout's `HEAD`, loose references and `packed-refs` as data.
	/// \details `HEAD`, names under `refs/` and one level of symbolic
	/// reference are followed; anything else, including reference storage
	/// this build does not read, is unavailable rather than guessed.
	[[nodiscard]] auto resolve_reference(workspace_root_session& aSession, std::string_view aName, core::git_hash_algorithm aAlgorithm, std::stop_token aStop)
		-> std::expected<core::git_commit_oid, std::optional<workspace_failure>>;

	struct cycle_candidate_request
	{
		cycle_outgoing_request  Outgoing;
		core::candidate_request Candidate;
	};

	/// \brief A built candidate: every object is in the private store and
	/// Git computed exactly the identities the core did.
	struct cycle_candidate_outcome
	{
		cycle_outgoing_outcome                     Outcome;
		std::optional<core::candidate_refusal>     Limit;
		std::optional<core::publication_candidate> Candidate;
	};

	/// \brief The selection as a patch for the reviewer's own Git tools.
	struct cycle_external_outcome
	{
		cycle_outgoing_outcome                    Outcome;
		std::optional<core::candidate_refusal>    Limit;
		std::optional<core::external_preparation> Preparation;
		/// A binary patch from P's tree to the prepared tree.
		std::vector<std::uint8_t> Patch;
	};

	/// \brief Plans and builds outgoing content. Runs on the effect lane; the
	/// source, store and candidate port must outlive it.
	/// \details Building writes only the BPR-owned private store: retained
	/// bytes are checked against the planned identity before Git sees them,
	/// the tree is made in a private index, and the one-parent commit carries
	/// exactly the confirmed metadata with no signature and no hook.
	class review_cycle_outgoing_host
	{
	public:
		review_cycle_outgoing_host(workspace_source_port& aSource, review_cycle_store& aStore, publication_candidate_port& aGit) noexcept;

		auto plan(const core::review_cycle_state& aCycle, const cycle_outgoing_request& aRequest, std::stop_token aStop) -> cycle_outgoing_outcome;
		auto build(const core::review_cycle_state& aCycle, const cycle_candidate_request& aRequest, std::stop_token aStop) -> cycle_candidate_outcome;
		auto prepare_external(const core::review_cycle_state& aCycle, const cycle_outgoing_request& aRequest, const core::candidate_requirements& aRequirements, std::stop_token aStop)
			-> cycle_external_outcome;

	private:
		struct assembled_plan
		{
			core::outgoing_request Request;
			core::outgoing_plan    Plan;
			publication_repository Repository;
		};

		auto assemble(const core::review_cycle_state& aCycle, const cycle_outgoing_request& aRequest, const std::stop_token& aStop) -> std::expected<assembled_plan, cycle_outgoing_outcome>;
		auto write_blobs(const core::review_cycle_state& aCycle, const assembled_plan& aAssembled, const std::vector<core::candidate_blob>& aBlobs, const std::stop_token& aStop)
			-> std::optional<cycle_outgoing_outcome>;

		workspace_source_port&      Source;
		review_cycle_store&         Store;
		publication_candidate_port& Git;
	};
}

#endif
