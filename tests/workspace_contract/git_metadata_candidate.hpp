#ifndef BPR_TESTS_WORKSPACE_CONTRACT_GIT_METADATA_CANDIDATE_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_GIT_METADATA_CANDIDATE_HPP

/// \file
/// \brief The questions RFC-BPR-0083's differential harness asks Git and
/// each candidate implementation of the Git metadata components C1-C6.
/// \details Answers use only standard types, so no candidate's own types
/// (libgit2's included) cross into the harness. A candidate that declines a
/// repository returns its reason as the unexpected value; the harness
/// classifies that refusal under H1 rather than counting it as agreement.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bpr::tests::workspace_contract {
	/// \brief A candidate's reason for declining, as words for the report.
	using candidate_refusal = std::string;

	/// \brief C1: one index entry as `git ls-files --stage` and `--debug`
	/// report it. Sparse-directory entries keep their trailing `/`.
	struct index_answer_entry
	{
		std::string   Path;
		std::uint32_t Mode = 0;
		/// Lower-case hexadecimal object name.
		std::string  Object;
		std::uint8_t Stage                                        = 0;
		bool         SkipWorktree                                 = false;
		bool         IntentToAdd                                  = false;
		auto         operator<=>(const index_answer_entry&) const = default;
	};

	struct index_answer
	{
		std::vector<index_answer_entry> Entries;
		/// The candidate knows its entry list is incomplete (a split index read
		/// without its shared file).
		bool Partial = false;
	};

	/// \brief C2: what the repository's config says.
	struct format_answer
	{
		/// `sha1` or `sha256`, as `git rev-parse --show-object-format` prints.
		std::string ObjectFormat;
		/// `files` or `reftable`, as `--show-ref-format` prints.
		std::string ReferenceFormat;
		bool        IgnoreCase = false;
	};

	/// \brief C3: what HEAD names and resolves to.
	struct head_answer
	{
		/// HEAD's own symbolic target, as `git symbolic-ref --no-recurse -q
		/// HEAD` prints it; absent when detached.
		std::optional<std::string> Symbolic;
		/// Lower-case hexadecimal commit, as `git rev-parse --verify -q HEAD`
		/// prints it; absent for an unborn branch.
		std::optional<std::string> Commit;
	};

	/// \brief C4: the repository directories for a checkout root. A
	/// candidate that knows paths fills them; the harness compares by file
	/// identity, so either absolute paths or opened identities compare.
	/// \brief A directory's identity on its volume, for a candidate that
	/// knows the directory it opened but not a path to it.
	struct file_identity
	{
		std::uint64_t Device                                  = 0;
		std::uint64_t Object                                  = 0;
		auto          operator<=>(const file_identity&) const = default;
	};

	struct discovery_answer
	{
		std::filesystem::path GitDirectory;
		std::filesystem::path CommonDirectory;
		/// True for a linked worktree.
		bool LinkedWorktree = false;
		/// Filled instead of the paths by a candidate that holds the opened
		/// directories (K); the harness then compares these with the
		/// identities of Git's paths.
		std::optional<file_identity> GitDirectoryIdentity;
		std::optional<file_identity> CommonDirectoryIdentity;
	};

	/// \brief One path asked about in C5 or C6, root-relative with `/`.
	struct path_question
	{
		std::string Path;
		bool        Directory = false;
	};

	/// \brief C6: a `filter` attribute state as `git check-attr` prints it:
	/// `set`, `unset`, `unspecified` or the value.
	using attribute_answer = std::string;

	/// \brief One implementation of the Git metadata components.
	class git_metadata_candidate
	{
	public:
		git_metadata_candidate()                                                 = default;
		git_metadata_candidate(const git_metadata_candidate&)                    = delete;
		auto operator=(const git_metadata_candidate&) -> git_metadata_candidate& = delete;
		git_metadata_candidate(git_metadata_candidate&&)                         = delete;
		auto operator=(git_metadata_candidate&&) -> git_metadata_candidate&      = delete;
		virtual ~git_metadata_candidate()                                        = default;

		/// \brief `K` (first party), `L` (libgit2) or another label.
		[[nodiscard]] virtual auto name() const -> std::string = 0;

		/// \brief C1, for the checkout at `aRoot`.
		virtual auto index(const std::filesystem::path& aRoot) -> std::expected<index_answer, candidate_refusal> = 0;
		/// \brief C2.
		virtual auto format(const std::filesystem::path& aRoot) -> std::expected<format_answer, candidate_refusal> = 0;
		/// \brief C3, for HEAD.
		virtual auto head(const std::filesystem::path& aRoot) -> std::expected<head_answer, candidate_refusal> = 0;
		/// \brief C3, for a full reference name: its commit, or nothing when
		/// it names none.
		virtual auto reference(const std::filesystem::path& aRoot, const std::string& aName) -> std::expected<std::optional<std::string>, candidate_refusal> = 0;
		/// \brief C4.
		virtual auto discover(const std::filesystem::path& aRoot) -> std::expected<discovery_answer, candidate_refusal> = 0;
		/// \brief C5: whether each path is ignored, as `git check-ignore
		/// --no-index` decides, a path beneath an ignored directory included.
		/// Global and system excludes files are never consulted.
		virtual auto ignored(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) -> std::expected<std::vector<bool>, candidate_refusal> = 0;
		/// \brief C6: each path's `filter` attribute.
		virtual auto filter(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) -> std::expected<std::vector<attribute_answer>, candidate_refusal> = 0;
		/// \brief [[BPR-1692]]. Each path's `linguist-generated`, from the
		/// committed `.gitattributes` files only, as RFC-BPR-0089 reads them at
		/// a pull request's head. A candidate that does not offer it refuses.
		virtual auto linguist_generated(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) -> std::expected<std::vector<attribute_answer>, candidate_refusal>
		{
			static_cast<void>(aRoot);
			static_cast<void>(aPaths);
			return std::unexpected(candidate_refusal{ "not offered" });
		}
	};

	/// \brief The first-party readers (K) composed as capture composes them.
	[[nodiscard]] auto make_first_party_candidate() -> std::unique_ptr<git_metadata_candidate>;

#if defined(BPR_HAVE_LIBGIT2_CANDIDATE)
	/// \brief libgit2 (L), built only behind the evaluation option.
	[[nodiscard]] auto make_libgit2_candidate() -> std::unique_ptr<git_metadata_candidate>;
#endif
}

#endif
