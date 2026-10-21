#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_COMMIT_FILTER_PROJECTION_HPP

#include "bpr/core/commit_filter.hpp"

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

		auto operator<=>(const commit_filter_row&) const = default;
	};

	/// \brief A hidden commit a force push removed, named for the chip.
	struct commit_filter_gone
	{
		std::string ShortCommit;
		std::string Headline;

		auto operator<=>(const commit_filter_gone&) const = default;
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
		commit_filter_trees                            Trees = commit_filter_trees::idle;
		/// The files the filter leaves, while it applies to the file set.
		std::optional<std::size_t> FilteredFiles;
		/// The comparison's own file count.
		std::size_t ComparisonFiles = 0U;
		/// Filtered files a hidden commit splits (RFC-BPR-0089 Q2).
		std::size_t SplitFiles = 0U;
		/// Why the last gesture was refused, until the next gesture.
		std::optional<core::commit_filter_refusal> Refusal;

		auto operator<=>(const commit_filter_projection&) const = default;
	};

	/// \brief Whether the chip shows: a filter applies, is suspended, or
	/// names commits a force push removed.
	[[nodiscard]] constexpr auto commit_filter_chip_shown(const commit_filter_projection& aProjection) noexcept -> bool
	{
		return aProjection.Status == core::commit_filter_status::active
			|| aProjection.Status == core::commit_filter_status::suspended
			|| !aProjection.Gone.empty();
	}

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
