#ifndef BPR_CORE_MERGE_BLOCKERS_HPP
#define BPR_CORE_MERGE_BLOCKERS_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-2327]]. What stops a pull request from merging, in the order
/// the Overview's Merge section names it, and the heading that follows.
///
/// \details The owner, 2026-11-20, comparing GitHub's merge box with the
/// Overview: "can we improve our status for conflicts? GitHub is much more
/// useful here." Merge's heading said "Conflicts" over a line about a
/// required check that had not reported. The heading is now decided from
/// every blocker: the most serious kind leads, the blockers of that kind are
/// counted together, and the rest are "more requirements".
namespace bpr::core {
	/// \brief One kind of blocker, most serious first.
	enum class merge_blocker_kind : std::uint8_t {
		/// GitHub reports merge conflicts with the base.
		conflicts,
		/// The pull request is a draft.
		draft,
		/// A reviewer requested changes.
		changes_requested,
		/// A required check concluded without passing.
		check_failed,
		/// The base branch's rules require a review that is missing.
		review_required,
		/// A required check has not reported a result on the head.
		check_waiting,
		/// GitHub reports the branch is behind its base.
		behind,
		/// GitHub reports merging blocked and names no reason this app sees.
		blocked,
	};

	/// \brief One blocker; `Subject` names a check, empty for the others.
	struct merge_blocker
	{
		merge_blocker_kind Kind = merge_blocker_kind::blocked;
		std::string        Subject;

		auto operator<=>(const merge_blocker&) const = default;
	};

	/// \brief The blockers in the order Merge names them: by kind, most
	/// serious first, and in the order given within a kind. A duplicate is
	/// named once.
	[[nodiscard]] auto order_merge_blockers(std::vector<merge_blocker> aBlockers) -> std::vector<merge_blocker>;

	/// \brief What Merge's heading says: the leading kind, how many blockers
	/// share it, and how many others follow.
	struct merge_headline
	{
		/// Absent when nothing blocks the merge.
		std::optional<merge_blocker_kind> Lead;
		std::size_t                       LeadCount = 0U;
		std::size_t                       Others    = 0U;

		auto operator<=>(const merge_headline&) const = default;
	};

	/// \brief The heading for blockers `order_merge_blockers` ordered.
	[[nodiscard]] auto decide_merge_headline(std::span<const merge_blocker> aOrdered) -> merge_headline;
} // namespace bpr::core

#endif
