#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_HPP

#include "bpr/core/clone_discovery.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/draft_evidence.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle_workspace.hpp"

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
/// \brief A local PR draft: the author's preparation of a pull request from
/// a Git worktree before any pull request exists (RFC-BPR-0108).
/// \details The core decides the draft's identity, how a worktree's remotes
/// resolve to the repository the pull request would target, which target and
/// comparison base an observation supports, which drafts an opened worktree
/// matches, and how candidates, notes and associations change. The host
/// observes refs, configuration, objects and working files and hands them in
/// as data; nothing here reads a file or runs Git. A draft is not a Review
/// Cycle, a review-comment draft or a tour draft, and it carries no account,
/// pull request number or provider identity until an association is recorded.
namespace bpr::core {
	/// \brief The most local drafts one profile keeps. A full store refuses a
	/// new draft; nothing is evicted.
	inline constexpr std::size_t maximum_local_drafts = 64U;
	/// \brief The most candidates one draft retains. A full draft refuses a
	/// new candidate; nothing is evicted.
	inline constexpr std::size_t maximum_local_draft_candidates = 32U;
	/// \brief The most paths one candidate's comparison lists.
	inline constexpr std::size_t maximum_local_draft_changes = 4096U;
	/// \brief The most working files one candidate includes.
	inline constexpr std::size_t maximum_local_draft_working_files = 512U;
	/// \brief The most bytes of one file a candidate keeps.
	inline constexpr std::uint64_t maximum_local_draft_file_bytes = 16U * 1024U * 1024U;
	/// \brief The most bytes one candidate keeps across its files.
	inline constexpr std::uint64_t maximum_local_draft_candidate_bytes = 256U * 1024U * 1024U;
	/// \brief The most notes one draft keeps.
	inline constexpr std::size_t maximum_local_draft_notes       = 512U;
	inline constexpr std::size_t maximum_local_draft_note_bytes  = 16U * 1024U;
	inline constexpr std::size_t maximum_local_draft_title_bytes = 256U;
	/// \brief Local file-path remotes followed from a checkout to the clone
	/// that names a hosted repository.
	inline constexpr std::size_t maximum_local_remote_hops = 4U;
	/// \brief The most remote branches offered when a target must be chosen.
	inline constexpr std::size_t maximum_local_draft_branch_choices = 256U;

	/// \brief A draft's stable local identity: 32 lowercase hex digits drawn
	/// by the host. It names no account, repository or pull request.
	struct local_draft_id
	{
		std::string Value;
		auto        operator<=>(const local_draft_id&) const = default;
	};
	[[nodiscard]] auto valid(const local_draft_id& aId) noexcept -> bool;

	// --- Where the pull request would go -------------------------------------

	/// \brief A remote URL that names another clone on this computer: an
	/// absolute path, a `file://` URL, or a path relative to the clone that
	/// names it. Absent for a hosted URL or anything else.
	[[nodiscard]] auto local_remote_path(std::string_view aUrl) -> std::optional<std::string>;

	/// \brief One clone followed through a local remote.
	struct local_remote_hop
	{
		/// The remote followed from the previous clone.
		std::string Remote;
		/// The clone it names, as the host resolved it.
		std::string Location;
		auto        operator<=>(const local_remote_hop&) const = default;
	};

	/// \brief One hosted remote of the clone the chain reached.
	struct hosted_destination
	{
		std::string     Remote;
		remote_location Repository;
		auto            operator<=>(const hosted_destination&) const = default;
	};

	enum class remote_chain_refusal : std::uint8_t {
		/// A clone on the chain names one already followed.
		cycle,
		/// A clone on the chain could not be opened or its config read.
		unreadable,
		/// More than `maximum_local_remote_hops` local remotes.
		too_many_hops,
		/// The last clone names neither a hosted repository nor a local clone.
		no_hosted_remote,
		/// A clone with no hosted remote names more than one local clone.
		several_local_remotes,
	};
	[[nodiscard]] auto name(remote_chain_refusal aRefusal) noexcept -> std::string_view;

	/// \brief A walk from the checkout through local remotes, one clone at a
	/// time. The host opens each clone the walk asks for and hands its remotes
	/// back; the walk fetches nothing and changes no clone.
	struct remote_chain_walk
	{
		/// Every clone read so far, the checkout first, as the host resolved them.
		std::vector<std::string>      Visited;
		std::vector<local_remote_hop> Hops;
		auto                          operator<=>(const remote_chain_walk&) const = default;
	};
	/// \brief Read this clone next: `Path` is the remote's path as written,
	/// to resolve against `From`.
	struct remote_chain_follow
	{
		std::string From;
		std::string Remote;
		std::string Path;
		auto        operator<=>(const remote_chain_follow&) const = default;
	};
	/// \brief The chain reached a clone that names hosted repositories.
	struct remote_chain_resolved
	{
		std::vector<local_remote_hop>   Hops;
		std::vector<hosted_destination> Destinations;
		auto                            operator<=>(const remote_chain_resolved&) const = default;
	};
	struct remote_chain_refused
	{
		remote_chain_refusal Reason = remote_chain_refusal::unreadable;
		/// The clone the refusal concerns.
		std::string Location;
		auto        operator<=>(const remote_chain_refused&) const = default;
	};
	using remote_chain_step = std::variant<remote_chain_follow, remote_chain_resolved, remote_chain_refused>;

	/// \brief One step of the walk: `aLocation` is the clone just read (the
	/// checkout first, then each `remote_chain_follow` resolved by the host),
	/// `aRemotes` its remotes, or nothing when it could not be read.
	/// \details A clone with any hosted remote ends the walk with every hosted
	/// remote it names. A clone with none follows its one local remote; two
	/// or more are refused rather than guessed between. A location already
	/// visited is a cycle, and a fifth hop is refused.
	[[nodiscard]] auto step_remote_chain(remote_chain_walk& aWalk, std::string_view aLocation, const std::optional<std::vector<git_remote>>& aRemotes) -> remote_chain_step;

	/// \brief What the reached clone knows about one hosted remote's branches.
	struct destination_facts
	{
		hosted_destination Destination;
		/// The branch its `refs/remotes/REMOTE/HEAD` names, when it names one.
		std::optional<std::string> DefaultBranch;
		/// Remote-tracking branches it holds, for a choice.
		std::vector<std::string> Branches;
		auto                     operator<=>(const destination_facts&) const = default;
	};

	/// \brief The repository and branch the pull request would merge into.
	struct local_draft_target
	{
		/// The hosted remote in the reached clone, by its name there.
		std::string     Remote;
		remote_location Repository;
		std::string     Branch;
		auto            operator<=>(const local_draft_target&) const = default;
	};

	enum class target_origin : std::uint8_t {
		/// The author chose it.
		chosen,
		/// The only hosted repository's default branch.
		remote_default,
	};
	[[nodiscard]] auto name(target_origin aOrigin) noexcept -> std::string_view;

	enum class target_question : std::uint8_t {
		/// More than one hosted repository (a fork and its parent, say):
		/// which one is the destination is not guessed, `origin` included.
		several_repositories,
		/// The repository's default branch is not known here; `main` is not assumed.
		no_default_branch,
		/// The draft's chosen repository is not among this worktree's remotes.
		chosen_repository_missing,
	};
	[[nodiscard]] auto name(target_question aQuestion) noexcept -> std::string_view;

	struct chosen_target
	{
		local_draft_target Target;
		target_origin      Origin                                  = target_origin::remote_default;
		auto               operator<=>(const chosen_target&) const = default;
	};
	/// \brief A target the author must choose, and what to choose from.
	struct target_selection
	{
		target_question                Question = target_question::several_repositories;
		std::vector<destination_facts> Choices;
		auto                           operator<=>(const target_selection&) const = default;
	};

	/// \brief Decides the target from what the reached clone knows, never from
	/// the branch's upstream (`branch.NAME.merge`), which names where the
	/// branch is pushed, not where it would merge.
	/// \param aPrior The draft's earlier choice, kept while its repository is
	/// still among the destinations.
	[[nodiscard]] auto choose_target(std::span<const destination_facts> aFacts, const std::optional<chosen_target>& aPrior) -> std::expected<chosen_target, target_selection>;
	/// \brief An explicit choice: refused unless the repository is one of the
	/// destinations and the branch is a valid branch name.
	[[nodiscard]] auto choose_target_explicitly(std::span<const destination_facts> aFacts, const local_draft_target& aChoice) -> std::optional<chosen_target>;

	// --- The comparison --------------------------------------------------------

	enum class source_state : std::uint8_t {
		branch,
		/// HEAD names a commit, not a branch.
		detached,
		/// HEAD names a branch with no commit yet.
		unborn,
		/// HEAD could not be read.
		unreadable,
	};
	[[nodiscard]] auto name(source_state aState) noexcept -> std::string_view;

	enum class comparison_state : std::uint8_t {
		/// One merge base: the pull request's own comparison.
		ready,
		/// The target's tip is not known here, or its commit is not in the
		/// clone's objects. Nothing is fetched.
		target_unavailable,
		/// Source and target share no history.
		no_common_base,
		/// More than one merge base; none is chosen for the author.
		several_bases,
	};
	[[nodiscard]] auto name(comparison_state aState) noexcept -> std::string_view;

	/// \brief The pull request's comparison: the source head against its one
	/// merge base with the observed target tip (three-dot).
	struct local_draft_comparison
	{
		comparison_state              State = comparison_state::target_unavailable;
		std::optional<git_commit_oid> Base;
		/// Every merge base, when there are several.
		std::vector<git_commit_oid> Bases;
		auto                        operator<=>(const local_draft_comparison&) const = default;
	};
	/// \brief Decides from the merge bases Git listed for head and tip.
	[[nodiscard]] auto decide_comparison(std::span<const git_commit_oid> aMergeBases) -> local_draft_comparison;

	enum class draft_change : std::uint8_t {
		added,
		modified,
		deleted,
		/// The mode alone changed.
		mode,
	};
	[[nodiscard]] auto name(draft_change aChange) noexcept -> std::string_view;

	/// \brief Where one side's bytes came from.
	enum class draft_material_origin : std::uint8_t {
		/// A blob in the clone's objects.
		committed,
		/// The working file's exact bytes, read when the candidate was captured.
		working,
	};

	struct draft_side
	{
		std::uint32_t         Mode = 0;
		git_entry_oid         Object;
		std::uint64_t         Size                                 = 0;
		draft_material_origin Origin                               = draft_material_origin::committed;
		auto                  operator<=>(const draft_side&) const = default;
	};

	/// \brief One path the candidate changes against its base.
	struct draft_path_change
	{
		std::string               Path;
		draft_change              Change = draft_change::modified;
		std::optional<draft_side> Left;
		std::optional<draft_side> Right;
		auto                      operator<=>(const draft_path_change&) const = default;
	};

	/// \brief The committed change: every path that differs between the base
	/// tree and the head tree, in path-byte order.
	[[nodiscard]] auto compare_trees(std::span<const git_tree_entry> aBase, std::span<const git_tree_entry> aHead) -> std::vector<draft_path_change>;

	// --- Working-tree edits ----------------------------------------------------

	enum class working_edit : std::uint8_t {
		/// Tracked at HEAD and different in the working tree.
		modified,
		/// Tracked at HEAD and missing from the working tree.
		deleted,
		/// Not tracked and not ignored.
		untracked,
	};
	[[nodiscard]] auto name(working_edit aEdit) noexcept -> std::string_view;

	/// \brief Why a working file's bytes were not compared exactly.
	enum class working_qualification : std::uint8_t {
		none,
		/// A `filter` attribute (Git LFS, say) transforms it; the working
		/// bytes are not what Git would store.
		filtered,
		/// Larger than `maximum_local_draft_file_bytes`.
		too_large,
		/// Neither a regular file nor a symbolic link.
		unsupported,
	};
	[[nodiscard]] auto name(working_qualification aQualification) noexcept -> std::string_view;

	/// \brief One working-tree difference from HEAD, as the host observed it.
	struct working_file
	{
		std::string                  Path;
		working_edit                 Edit = working_edit::modified;
		std::uint32_t                Mode = 0;
		std::optional<git_entry_oid> Object;
		std::uint64_t                Size                                   = 0;
		working_qualification        Qualification                          = working_qualification::none;
		auto                         operator<=>(const working_file&) const = default;
	};

	/// \brief Qualifications a candidate carries as a whole.
	enum class candidate_qualification : std::uint8_t {
		/// It includes working files: it is not the branch head.
		includes_working_files,
		/// The checkout is sparse: paths outside it were not compared.
		sparse_checkout,
		/// The working-tree scan stopped at its bound.
		working_scan_incomplete,
		/// `core.autocrlf` or `eol` settings may make working bytes differ
		/// from what Git would store.
		line_ending_conversion,
	};
	[[nodiscard]] auto name(candidate_qualification aQualification) noexcept -> std::string_view;

	/// \brief One immutable candidate: the comparison it was captured against
	/// and the exact material it holds.
	struct local_draft_candidate
	{
		/// 1-based within the draft; never reused.
		std::uint64_t  Ordinal = 0;
		git_commit_oid Head;
		git_commit_oid TargetTip;
		git_commit_oid Base;
		/// The working files the author included, whole files.
		std::vector<std::string>             Included;
		std::vector<draft_path_change>       Changes;
		std::vector<candidate_qualification> Qualifications;
		/// Seconds since the epoch, from the host's clock.
		std::int64_t CapturedAt                                      = 0;
		auto         operator<=>(const local_draft_candidate&) const = default;
	};

	enum class draft_candidate_refusal : std::uint8_t {
		/// An included path is not among the working edits observed.
		unknown_working_file,
		/// An included working file is qualified and cannot be captured exactly.
		qualified_working_file,
		/// More paths or bytes than one candidate keeps.
		bounds,
	};
	[[nodiscard]] auto name(draft_candidate_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The candidate's changes: the committed change with the included
	/// working files laid over the head's side. Only included paths enter; an
	/// untracked file is added only when included.
	[[nodiscard]] auto compose_candidate_changes(std::span<const draft_path_change> aCommitted, std::span<const git_tree_entry> aBase, std::span<const working_file> aWorking,
												 std::span<const std::string> aIncluded) -> std::expected<std::vector<draft_path_change>, draft_candidate_refusal>;

	// --- The draft -------------------------------------------------------------

	/// \brief Which worktree a draft belongs to: the identities of its shared
	/// and per-worktree Git directories, and where it was last opened.
	struct local_draft_worktree
	{
		std::string               Location;
		workspace_object_identity Repository;
		workspace_object_identity Worktree;
		auto                      operator<=>(const local_draft_worktree&) const = default;
	};

	enum class note_origin : std::uint8_t {
		author,
		/// Context an authoring harness handed over with `bpr draft open`.
		harness,
	};

	struct local_draft_note
	{
		std::uint64_t Id = 0;
		std::string   Text;
		/// The path it is about; empty for the whole change.
		std::string   Path;
		std::uint64_t Candidate                                  = 0;
		note_origin   Origin                                     = note_origin::author;
		auto          operator<=>(const local_draft_note&) const = default;
	};

	/// \brief A pull request the draft was verified to have become.
	struct local_draft_association
	{
		std::string   Host;
		std::string   Owner;
		std::string   Name;
		std::uint64_t Number = 0;
		std::string   HeadRef;
		auto          operator<=>(const local_draft_association&) const = default;
	};

	struct local_draft
	{
		local_draft_id       Id;
		std::uint64_t        Version = 0;
		std::string          Title;
		local_draft_worktree Worktree;
		/// The branch it prepares, without `refs/heads/`.
		std::string                            Branch;
		chosen_target                          Target;
		std::vector<local_remote_hop>          Chain;
		std::vector<local_draft_candidate>     Candidates;
		std::vector<local_draft_note>          Notes;
		std::uint64_t                          NextNote = 1;
		std::optional<local_draft_association> Association;
		/// RFC-BPR-0109: claims, checks, results and questions.
		draft_evidence Evidence;
		std::int64_t   Created                               = 0;
		std::int64_t   Updated                               = 0;
		auto           operator<=>(const local_draft&) const = default;
	};
	/// \brief Every field within its bound and every identity well formed.
	[[nodiscard]] auto valid(const local_draft& aDraft) -> bool;
	[[nodiscard]] auto latest_candidate(const local_draft& aDraft) noexcept -> const local_draft_candidate*;

	/// \brief How an opened worktree relates to one draft.
	enum class worktree_match : std::uint8_t {
		/// The same Git directories at the same location, on the same branch.
		same,
		/// The same worktree, now on another branch: the draft is not
		/// retargeted; another draft serves the other branch.
		branch_switched,
		/// The same Git directories at another location: an explicit rebind
		/// with a material comparison is needed.
		moved,
		/// Another repository or worktree, even at the same path.
		other,
	};
	[[nodiscard]] auto name(worktree_match aMatch) noexcept -> std::string_view;
	[[nodiscard]] auto match_worktree(const local_draft& aDraft, const workspace_root_observation& aObserved, std::string_view aBranch) -> worktree_match;

	/// \brief What opening a worktree resumes.
	struct worktree_resolution
	{
		/// The draft to resume, by index in the given list.
		std::optional<std::size_t> Resume;
		/// A draft for this branch whose worktree moved here: offered for an
		/// explicit rebind, never taken silently.
		std::optional<std::size_t> Moved;
		/// Drafts of the same worktree for other branches, left as they are.
		std::vector<std::size_t> OtherBranches;
		auto                     operator<=>(const worktree_resolution&) const = default;
	};
	/// \brief Finds what an opened worktree resumes, by Git-directory identity
	/// and branch, never by path alone. A draft associated with a pull request
	/// is still resumed.
	[[nodiscard]] auto resolve_worktree(std::span<const local_draft> aDrafts, const workspace_root_observation& aObserved, std::string_view aBranch) -> worktree_resolution;

	enum class draft_refusal : std::uint8_t {
		/// The draft changed since it was read; read it again.
		stale,
		/// `maximum_local_drafts` drafts exist.
		drafts_full,
		/// `maximum_local_draft_candidates` candidates exist.
		candidates_full,
		notes_full,
		note_too_long,
		title_invalid,
		unknown_note,
		/// The candidate repeats the latest exactly.
		unchanged,
		invalid,
		/// No evidence record has that identity, or it is not of that kind.
		unknown_record,
		/// The evidence holds as many records of that kind as it keeps.
		evidence_full,
	};
	[[nodiscard]] auto name(draft_refusal aRefusal) noexcept -> std::string_view;

	/// \brief A new draft for an opened worktree, with its first candidate.
	[[nodiscard]] auto create_local_draft(std::span<const local_draft> aExisting, local_draft_id aId, local_draft_worktree aWorktree, std::string aBranch, chosen_target aTarget,
										  std::vector<local_remote_hop> aChain, local_draft_candidate aFirst, std::int64_t aNow) -> std::expected<local_draft, draft_refusal>;
	/// \brief A successor candidate. The previous ones stay; a full draft
	/// refuses rather than evict, and a write against an older version is stale.
	[[nodiscard]] auto add_candidate(const local_draft& aDraft, std::uint64_t aExpectedVersion, local_draft_candidate aCandidate, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	[[nodiscard]] auto add_note(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string aText, std::string aPath, note_origin aOrigin, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	[[nodiscard]] auto remove_note(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aNote, std::int64_t aNow) -> std::expected<local_draft, draft_refusal>;
	[[nodiscard]] auto rename_draft(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string aTitle, std::int64_t aNow) -> std::expected<local_draft, draft_refusal>;
	/// \brief A target the author chose; later candidates compare against it.
	[[nodiscard]] auto retarget_draft(const local_draft& aDraft, std::uint64_t aExpectedVersion, chosen_target aTarget, std::vector<local_remote_hop> aChain, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	/// \brief An explicit rebind after a move, confirmed by the author.
	[[nodiscard]] auto rebind_draft(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string aLocation, std::int64_t aNow) -> std::expected<local_draft, draft_refusal>;
	/// \brief Records the verified pull request: only one with this draft's
	/// branch as its head and the target's repository and branch as its base.
	[[nodiscard]] auto associate_draft(const local_draft& aDraft, std::uint64_t aExpectedVersion, local_draft_association aAssociation, std::string_view aBaseRef,
									   std::int64_t aNow) -> std::expected<local_draft, draft_refusal>;

	/// \brief The title a new draft starts with: the branch, words separated.
	[[nodiscard]] auto default_draft_title(std::string_view aBranch) -> std::string;

	/// \brief Home's In preparation: drafts not associated with a pull
	/// request, most recently changed first. Indexes into `aDrafts`.
	[[nodiscard]] auto drafts_in_preparation(std::span<const local_draft> aDrafts) -> std::vector<std::size_t>;
	/// \brief The draft associated with one pull request, for its Home row's mark.
	[[nodiscard]] auto draft_for_pull_request(std::span<const local_draft> aDrafts, std::string_view aHost, std::string_view aOwner, std::string_view aName, std::uint64_t aNumber)
		-> std::optional<std::size_t>;

	/// \brief The diff-engine identity of one path of one candidate,
	/// domain-separated from every pull-request and cycle comparison. Absent
	/// when neither side has content.
	[[nodiscard]] auto local_draft_file_comparison(const local_draft_id& aDraft, std::uint64_t aCandidate, std::string_view aPath, const std::optional<git_entry_oid>& aLeft,
												   const std::optional<git_entry_oid>& aRight, const diff_options& aOptions) -> std::optional<material_comparison_key>;

	/// \brief The draft as stored: compact JSON, without working bytes.
	[[nodiscard]] auto encode_local_draft(const local_draft& aDraft) -> std::string;
	/// \brief Reads a stored draft; absent for anything malformed or out of bounds.
	[[nodiscard]] auto decode_local_draft(std::string_view aText) -> std::optional<local_draft>;
}

#endif
