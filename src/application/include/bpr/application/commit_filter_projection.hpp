#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_PROJECTION_HPP

#include "bpr/core/commit_filter.hpp"
#include "bpr/core/generated_files.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// RFC-BPR-0089 M1. What the Commits tab, the filter chip and the filtered
/// counts show. Provider-free: a commit is named by its hexadecimal ID, and
/// every decision in it is the core's.
namespace bpr::application {
	/// \brief Where the chain's trees are, which the filter's file set needs.
	enum class commit_filter_trees : std::uint8_t {
		/// Not asked for yet: no filter needs them and the tab has not shown.
		idle,
		loading,
		ready,
		/// The chain's commits are not in this review's local objects.
		objects_unavailable,
		/// A commit changes more files than one tree diff reports.
		limit_exceeded,
		failed,
	};

	/// \brief RFC-BPR-0089 M2 ([[BPR-1692]]). Whether the head's
	/// `.gitattributes` were read for `linguist-generated`, or why not.
	enum class generated_attributes_read : std::uint8_t {
		/// Not asked for yet, or being read.
		pending,
		read,
		/// The review's paths sit in more directories than one read looks in.
		too_many_directories,
		/// More than 64 `.gitattributes` files apply.
		too_many_files,
		/// One is larger than 512 KiB.
		file_too_large,
		/// They hold more rules than the reader keeps.
		too_many_rules,
		/// The head's objects are not in this review's local objects.
		objects_unavailable,
		failed,
	};

	/// \brief One listed commit, in GitHub's order.
	struct commit_filter_row
	{
		/// The full commit ID; the identity a gesture names.
		std::string Commit;
		std::string ShortCommit;
		std::string Headline;
		/// The author's login, or empty when GitHub named none.
		std::string               Author;
		core::filter_commit_role  Role  = core::filter_commit_role::chain;
		core::filter_commit_state State = core::filter_commit_state::shown;
		/// Files the commit's first-parent diff changes, once the trees are
		/// read. A side commit is filtered through its merge and has none.
		std::optional<std::size_t> Files;
		/// For a side commit: the short ID of the merge it sits under.
		std::string Merge;
		/// RFC-BPR-0089 M2. The commit changes `core::bulk_commit_files` files
		/// or more. A hint only; it filters nothing.
		bool Bulk = false;

		auto operator<=>(const commit_filter_row&) const = default;
	};

	/// \brief RFC-BPR-0089 M2. One file's generated state and the gestures
	/// its context menu offers, every one decided by the core.
	struct generated_file_row
	{
		std::string                          Path;
		core::generated_reason               Reason       = core::generated_reason::unspecified;
		bool                                 Generated    = false;
		bool                                 ReviewAnyway = false;
		std::uint64_t                        Additions    = 0U;
		std::uint64_t                        Deletions    = 0U;
		std::vector<core::generated_gesture> Gestures;

		auto operator<=>(const generated_file_row&) const = default;
	};

	/// \brief A hidden commit a force push removed, named for the chip.
	struct commit_filter_gone
	{
		std::string ShortCommit;
		std::string Headline;

		auto operator<=>(const commit_filter_gone&) const = default;
	};

	/// \brief RFC-BPR-0089 M3. A listed commit offered in place of a hidden
	/// commit a force push removed. Offered only; the reviewer chooses it.
	struct commit_filter_rematch_row
	{
		/// Full IDs, the identities the gesture names.
		std::string Gone;
		std::string Candidate;
		std::string GoneShort;
		std::string GoneHeadline;
		std::string CandidateShort;
		std::string CandidateHeadline;
		/// `same_headline_and_author` is a guess and is labelled one.
		core::filter_rematch_basis Basis = core::filter_rematch_basis::same_change;

		auto operator<=>(const commit_filter_rematch_row&) const = default;
	};

	/// \brief The Commits tab, the chip and the filtered counts.
	struct commit_filter_projection
	{
		std::vector<commit_filter_row> Rows;
		core::commit_filter_status     Status = core::commit_filter_status::inactive;
		core::commit_filter_suspension Suspension{ };
		/// Why the pull request has no filter, when it has none.
		std::optional<core::commit_filter_unavailable> Unavailable;
		std::size_t                                    Shown   = 0U;
		std::size_t                                    Spanned = 0U;
		std::vector<commit_filter_gone>                Gone;
		/// RFC-BPR-0089 M3. What each gone commit may be matched to.
		std::vector<commit_filter_rematch_row> Rematches;
		commit_filter_trees                    Trees = commit_filter_trees::idle;
		/// The files the filter leaves, while it applies to the file set.
		std::optional<std::size_t> FilteredFiles;
		/// The comparison's own file count.
		std::size_t ComparisonFiles = 0U;
		/// Filtered files a hidden commit splits (RFC-BPR-0089 Q2).
		std::size_t SplitFiles = 0U;
		/// Why the last gesture was refused, until the next gesture.
		std::optional<core::commit_filter_refusal> Refusal;
		/// RFC-BPR-0089 M2. Whether the commit filter narrows the file set now.
		bool CommitFiltered = false;
		/// Paths the reviewer's file filters name ("Hide files these commits
		/// changed"), and those of them the comparison's files leave out.
		std::size_t FileFilterPaths   = 0U;
		std::size_t FileFilteredFiles = 0U;
		/// Generated files set aside behind their own row, in review order,
		/// with their line totals.
		std::vector<generated_file_row> Generated;
		std::uint64_t                   GeneratedAdditions = 0U;
		std::uint64_t                   GeneratedDeletions = 0U;
		/// Whether the head's `.gitattributes` were read. While they are not,
		/// only the reviewer's marks set files aside.
		bool AttributesRead = false;
		/// Why they were not read, when a bound or the objects stopped it;
		/// `AttributesExceeded` names the directory of a file past the byte
		/// bound.
		generated_attributes_read  Attributes = generated_attributes_read::pending;
		std::optional<std::string> AttributesExceeded;
		/// Files in the review whose menu is not an ordinary file's: marked,
		/// or generated and reviewed anyway. Sorted by path.
		std::vector<generated_file_row> Marked;
		/// The gestures an ordinary file offers (Treat as generated).
		std::vector<core::generated_gesture> OrdinaryGestures;

		auto operator<=>(const commit_filter_projection&) const = default;
	};

	/// \brief Whether the chip shows: a filter applies, is suspended, names
	/// commits a force push removed, or a file filter names paths.
	[[nodiscard]] constexpr auto commit_filter_chip_shown(const commit_filter_projection& aProjection) noexcept -> bool
	{
		return aProjection.Status == core::commit_filter_status::active
			|| aProjection.Status == core::commit_filter_status::suspended
			|| !aProjection.Gone.empty()
			|| aProjection.FileFilterPaths != 0U;
	}

	/// \brief RFC-BPR-0089 M2. One generated-file gesture from a context menu.
	struct generated_file_gesture
	{
		std::string             Path;
		core::generated_gesture Gesture = core::generated_gesture::treat_as_generated;

		auto operator<=>(const generated_file_gesture&) const = default;
	};

	/// \brief RFC-BPR-0089 M3. The reviewer chose a rematch offer: hide
	/// `Candidate` in place of gone `Gone`. Full commit IDs.
	struct commit_filter_rematch_gesture
	{
		std::string Gone;
		std::string Candidate;

		auto operator<=>(const commit_filter_rematch_gesture&) const = default;
	};

	/// \brief RFC-BPR-0089 M3 (owner, Q2). One of a split file's per-commit
	/// diffs, as the strip over its diff names it.
	struct split_diff_part
	{
		/// The shown commit the diff is the change of; full and short IDs.
		std::string Commit;
		std::string ShortCommit;
		std::string Headline;
		/// This diff's own receipt: how far it is read.
		core::file_read_state Read = core::file_read_state::unread;
		/// Whether it is the diff on screen.
		bool Selected = false;
		/// Hidden commits that change the file after this diff and before the
		/// next one, named in place.
		std::vector<commit_filter_gone> HiddenAfter;

		auto operator<=>(const split_diff_part&) const = default;
	};

	/// \brief RFC-BPR-0089 M3. The strip over a split file's diff.
	struct split_diff_projection
	{
		std::string                  Path;
		std::vector<split_diff_part> Parts;

		auto operator<=>(const split_diff_projection&) const = default;
	};

	/// \brief One gesture from the Commits tab or the chip.
	struct commit_filter_gesture
	{
		core::commit_filter_operation_kind Kind = core::commit_filter_operation_kind::show_only;
		/// Full commit IDs of the selected rows; empty for Clear all filters.
		std::vector<std::string> Commits;

		auto operator<=>(const commit_filter_gesture&) const = default;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_PROJECTION_HPP
