#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_CANDIDATE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_CANDIDATE_HPP

#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle_outgoing.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The squashed outgoing candidate and its exact transformation
/// limits (RFC-BPR-0059, BPR-1173).
/// \details A buildable plan becomes one commit with P as its only parent
/// and the plan's candidate tree, with author, committer, times and message
/// bound explicitly. The only byte-changing transformation is a named,
/// explicitly chosen CRLF-to-LF normalization of a selected text path, and
/// every one is recorded with its before and after identities. Anything the
/// native path cannot produce exactly — a Git filter such as LFS, a
/// submodule, required signing — is a refusal that names the paths, never a
/// silently different candidate; external preparation then describes the
/// selected bytes for the reviewer's own Git tools and says what those tools
/// must still do.
namespace bpr::core {
	inline constexpr std::size_t maximum_candidate_title_bytes = 256U;

	/// \brief The commit's public text and identities, confirmed by the reviewer.
	struct candidate_metadata
	{
		git_signature Author;
		git_signature Committer;
		/// One line.
		std::string Title;
		/// Optional; kept exactly apart from a final line feed.
		std::string Body;
		auto        operator<=>(const candidate_metadata&) const = default;
	};

	/// \brief What the destination demands of a commit, as observed.
	struct candidate_requirements
	{
		/// Commits must be signed. No native signing path is qualified.
		bool SigningRequired                                  = false;
		auto operator<=>(const candidate_requirements&) const = default;
	};

	struct candidate_request
	{
		/// Selected text paths whose CRLF line endings become LF. Named, never implied.
		std::vector<std::string> NormalizeLineEndings;
		candidate_metadata       Metadata;
		candidate_requirements   Requirements;
		auto                     operator<=>(const candidate_request&) const = default;
	};

	/// \brief One object the candidate adds or changes, and where its bytes come from.
	struct candidate_blob
	{
		std::string           Path;
		std::uint32_t         Mode = 0;
		git_entry_oid         Object;
		cycle_material_digest Content;
		std::uint64_t         Size = 0;
		/// The retained bytes' CRLF pairs become LF before hashing.
		bool LineFeed                                 = false;
		auto operator<=>(const candidate_blob&) const = default;
	};

	/// \brief A recorded byte-changing transformation.
	struct candidate_transformation
	{
		std::string           Path;
		cycle_material_digest Before;
		git_entry_oid         BeforeObject;
		git_entry_oid         After;
		auto                  operator<=>(const candidate_transformation&) const = default;
	};

	struct publication_candidate
	{
		git_hash_algorithm Format = git_hash_algorithm::invalid;
		git_commit_oid     Parent;
		git_tree_oid       Tree;
		git_commit_oid     Commit;
		git_commit_content Content;
		/// The complete outgoing listing.
		std::vector<git_tree_entry> Entries;
		/// Objects the candidate adds or changes relative to P, by path.
		std::vector<candidate_blob> Blobs;
		/// Paths P holds that the candidate removes.
		std::vector<std::string>              Deleted;
		std::vector<candidate_transformation> Transformations;
		auto                                  operator<=>(const publication_candidate&) const = default;
	};

	enum class candidate_limit : std::uint8_t {
		/// The candidate is P's tree.
		no_change,
		/// An improvement over omitted starting work has no choice yet.
		undecided,
		/// A Git filter (such as LFS) makes the committed object.
		filter_managed,
		/// A submodule boundary changed.
		gitlink,
		/// A file and a folder would share a path.
		path_conflict,
		/// The destination requires signed commits.
		signing_required,
		/// Normalization was named for a path that is not a selected text file
		/// with CRLF line endings, or named twice.
		normalization,
		/// Author, committer, times or message are not a valid commit.
		metadata,
		/// SHA-1 collision detection refused a tree or the commit; no
		/// identity was made ([[BPR-1450]]).
		collision_detected,
	};
	[[nodiscard]] auto name(candidate_limit aLimit) noexcept -> std::string_view;

	struct candidate_refusal
	{
		candidate_limit          Limit = candidate_limit::no_change;
		std::vector<std::string> Paths;
		auto                     operator<=>(const candidate_refusal&) const = default;
	};

	/// \brief The commit message from a title and body: exactly what the
	/// reviewer confirmed, with no added trailer.
	[[nodiscard]] auto candidate_message(std::string_view aTitle, std::string_view aBody) -> std::optional<std::string>;

	/// \brief Binds a plan to one squashed commit. `aIdentities` supplies the
	/// LF identities normalization needs.
	[[nodiscard]] auto prepare_candidate(const outgoing_plan& aPlan, std::span<const outgoing_identity> aIdentities, const candidate_request& aRequest)
		-> std::expected<publication_candidate, candidate_refusal>;

	/// \brief What the built commit is: parent, tree, identities, message
	/// and every transformation, appended to the plan's preview.
	[[nodiscard]] auto render_candidate_preview(const publication_candidate& aCandidate) -> std::string;

	/// \brief The selected bytes for the reviewer's own Git tools.
	struct external_preparation
	{
		git_commit_oid Parent;
		/// P with every selected path's captured bytes, filter-managed ones
		/// included as bytes, not as the object a filter would make.
		std::vector<git_tree_entry> Entries;
		git_tree_oid                Tree;
		std::vector<candidate_blob> Blobs;
		std::vector<std::string>    Deleted;
		/// Paths the reviewer's own tooling must still process (filters).
		std::vector<std::string> ToolingPaths;
		bool                     SigningRequired = false;
		/// Plain instructions: apply at P, then stage and commit with one's
		/// own tools. It says this is not native publication.
		std::string Instructions;
	};

	/// \brief Describes the selection for external preparation. Refused only
	/// when no tree can hold it (a path conflict or a submodule change) or
	/// nothing would change.
	[[nodiscard]] auto prepare_external(const outgoing_plan& aPlan, const candidate_requirements& aRequirements) -> std::expected<external_preparation, candidate_refusal>;
}

#endif
