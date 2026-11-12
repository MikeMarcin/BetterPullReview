#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_DISCOVERY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_DISCOVERY_HPP

#include "bpr/application/local_draft_git_port.hpp"
#include "bpr/application/local_draft_store.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/review_cycle_git.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief What a chosen worktree proposes (RFC-BPR-0108, [[BPR-2019]],
/// [[BPR-2020]]): its source, the target its remotes support, the pull
/// request's comparison, its working-tree edits, and a captured candidate.
/// \details Refs, configuration, the index and working files are read as
/// data through the read-only workspace port; commits, trees and blobs through
/// the isolated local-draft Git reader. Nothing runs a hook or filter, stages,
/// commits, switches a branch, fetches or writes any clone. The core decides
/// every rule; this file only observes. Runs on an effect lane.
namespace bpr::application {
	enum class local_draft_open_failure : std::uint8_t {
		/// The workspace port refused the location; see `Workspace`.
		workspace,
		/// A folder or file with no Git repository: Review Cycles reviews it.
		not_git,
		/// The repository is one the readers decline; see `Repository`.
		repository,
		cancelled,
	};

	struct local_draft_open_error
	{
		local_draft_open_failure                    Failure = local_draft_open_failure::workspace;
		std::optional<workspace_failure>            Workspace;
		std::optional<core::git_repository_problem> Repository;
	};

	/// \brief Bounds on one working-tree scan.
	inline constexpr std::uint64_t maximum_local_draft_scan_entries = 200'000U;
	inline constexpr std::uint64_t maximum_local_draft_scan_bytes   = 2ULL * 1024ULL * 1024ULL * 1024ULL;
	inline constexpr std::uint64_t maximum_local_draft_hashed_file  = 128U * 1024U * 1024U;
	inline constexpr std::size_t   maximum_local_draft_listed_edits = 2'000U;

	/// \brief Everything one open of a worktree observed.
	struct local_draft_discovery
	{
		core::workspace_root_observation Root;
		core::git_hash_algorithm         Algorithm = core::git_hash_algorithm::invalid;
		core::source_state               Source    = core::source_state::unreadable;
		/// Without `refs/heads/`; empty unless `Source` is `branch`.
		std::string                         Branch;
		std::optional<core::git_commit_oid> Head;

		/// The chain to the hosted repository, or why it was not reached.
		std::optional<core::remote_chain_resolved> Chain;
		std::optional<core::remote_chain_refused>  ChainRefusal;
		std::vector<core::destination_facts>       Destinations;
		std::optional<core::chosen_target>         Target;
		std::optional<core::target_selection>      Selection;
		/// The observed tip of the target's remote-tracking branch.
		std::optional<core::git_commit_oid> TargetTip;
		core::local_draft_comparison        Comparison;
		/// Why Git could not answer, when a read failed.
		std::optional<publication_git_error> Git;

		std::vector<core::git_tree_entry>    BaseEntries;
		std::vector<core::draft_path_change> Committed;

		/// Working-tree edits against HEAD, in path order, at most
		/// `maximum_local_draft_listed_edits`.
		std::vector<core::working_file> Working;
		/// Edits past the listed bound, or a scan stopped at its bounds.
		bool ScanIncomplete = false;
		/// Paths skipped by a sparse checkout.
		bool Sparse = false;
		/// `core.autocrlf` is set: working bytes may differ only in line endings.
		bool LineEndings = false;
		/// Tracked files a `filter` attribute transforms, not compared.
		std::uint64_t FilteredUncompared = 0;

		[[nodiscard]] auto repository() const -> local_draft_repository;
		[[nodiscard]] auto ready() const noexcept -> bool
		{
			return Source == core::source_state::branch && Head && Target && TargetTip && Comparison.State == core::comparison_state::ready;
		}
	};

	/// \brief Opens `aLocation` and observes it. `aPrior` is the target a
	/// resumed draft chose; it is kept while its repository is still reachable.
	[[nodiscard]] auto discover_local_draft(workspace_source_port& aSource, local_draft_git_port& aGit, std::string_view aLocation, const std::optional<core::chosen_target>& aPrior,
											std::stop_token aStop) -> std::expected<local_draft_discovery, local_draft_open_error>;

	/// \brief Applies an explicit target choice to an observation: the tip,
	/// comparison and committed change are observed again for it.
	[[nodiscard]] auto retarget_discovery(workspace_source_port& aSource, local_draft_git_port& aGit, local_draft_discovery aDiscovery, const core::chosen_target& aTarget,
										  std::stop_token aStop) -> std::expected<local_draft_discovery, local_draft_open_error>;

	enum class local_draft_capture_failure : std::uint8_t {
		/// The comparison is not ready: no branch, target or single base.
		not_ready,
		/// The core refused the selection; see `Refusal`.
		refused,
		/// A blob could not be read; see `Git`.
		git,
		/// A selected working file changed since it was listed: observe again.
		changed,
		/// The candidate's material is past its bound.
		too_large,
		cancelled,
	};

	struct local_draft_capture_error
	{
		local_draft_capture_failure                  Failure = local_draft_capture_failure::not_ready;
		std::optional<core::draft_candidate_refusal> Refusal;
		std::optional<publication_git_error>         Git;
		std::string                                  Path;
		auto operator<=>(const local_draft_capture_error&) const = default;
	};

	/// \brief One captured candidate and the exact bytes it keeps.
	struct local_draft_capture
	{
		core::local_draft_candidate     Candidate;
		std::vector<local_draft_object> Objects;
	};

	/// \brief Captures the comparison with exactly the included working
	/// files: both sides of every changed path, the working bytes read again
	/// and checked against what was listed.
	[[nodiscard]] auto capture_local_draft(workspace_source_port& aSource, local_draft_git_port& aGit, const local_draft_discovery& aDiscovery,
										   std::span<const std::string> aIncluded, std::int64_t aNow, std::stop_token aStop)
		-> std::expected<local_draft_capture, local_draft_capture_error>;

	/// \brief The worktree identity a draft records for this observation.
	[[nodiscard]] auto worktree_of(const local_draft_discovery& aDiscovery) -> core::local_draft_worktree;
}

#endif
