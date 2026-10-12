#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_HPP

#include "bpr/core/model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	inline constexpr std::size_t maximum_observer_rule_pattern_bytes = 4U * 1024U;
	/// \brief Names how a reviewer stands to one pull request on GitHub.
	///
	/// \details These four come from GitHub and nowhere else. A reviewer is
	/// what the server says they are, and this tool never promotes someone into
	/// a role the server has not given them.
	///
	/// `requested_reviewer` is deliberately distinct from `reviewer`: the first
	/// has been asked and has not answered, the second has already reviewed.
	/// Collapsing them would lose the difference between work waiting and work
	/// done, which is most of what a queue is for.
	enum class github_role : std::uint8_t {
		none,
		participant,
		reviewer,
		requested_reviewer,
		author,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(github_role aRole) noexcept -> std::string_view;

	/// \brief Names what a local observer rule matches on.
	enum class observer_rule_kind : std::uint8_t {
		repository,
		author,
		path,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(observer_rule_kind aKind) noexcept
		-> std::string_view;

	/// \brief One local rule that enrolls pull requests in this queue.
	///
	/// \details A rule is private to this machine. It changes no reviewer
	/// assignment, no watch, no subscription, no label, and no CODEOWNERS
	/// entry, and there is no field here by which it could ask for one.
	///
	/// `Excluding` inverts the rule: a matching pull request is kept out rather
	/// than let in. Generated and vendored trees are the reason — a path rule
	/// on `src/` should not drag in every regenerated file underneath it.
	struct observer_rule
	{
		observer_rule_kind Kind = observer_rule_kind::repository;

		// The pattern this rule matches. For `repository` it is `owner/name`.
		// For `author` it is a login. For `path` it is a prefix, or a prefix
		// with a single trailing `*`, which is all the grammar this needs: a
		// reviewer watching a directory is watching everything under it.
		std::string Pattern;

		bool Excluding = false;

		auto operator<=>(const observer_rule&) const = default;
	};

	/// \brief Checks whether one local observer rule is bounded and meaningful.
	[[nodiscard]] auto valid(const observer_rule& aRule) noexcept -> bool;

	/// \brief Names one pull request under one account on one host.
	///
	/// \details The whole identity, because a number is unique only within a
	/// repository, a repository only within an account, and an account only
	/// within a host. It orders lexicographically, which is what lets a queue
	/// be totally ordered without inventing a ranking between two reviews.
	struct pull_request_ref
	{
		std::string  Host;
		std::string  Account;
		std::string  Repository;
		std::int64_t Number = 0;

		auto operator<=>(const pull_request_ref&) const = default;
	};

	/// \brief What the host observed about one pull request.
	///
	/// \details A bounded record the host builds from the provider. The core
	/// reaches nothing to obtain it and holds no provider that could.
	struct pull_request_observation
	{
		pull_request_ref Ref;

		std::string Author;
		github_role Role = github_role::none;

		// Paths the pull request touches, in the order the host read them.
		std::vector<std::string> ChangedPaths;

		// The title as the host read it ([[BPR-0659]]); presentation only.
		std::string Title;

		auto operator<=>(const pull_request_observation&) const = default;
	};

	/// \brief Why one pull request is in this reviewer's queue.
	///
	/// \details Either GitHub put it there, in which case `Role` says how, or a
	/// local rule did, in which case `Rule` says which. An item that is in the
	/// queue for both reasons keeps both, because removing the rule should not
	/// silently remove a pull request the reviewer was assigned.
	///
	/// [[BPR-0989]]. Personal discovery adds its own reasons beside the role and
	/// the rule: `Authored` and `Requested` when a search of a saved repository
	/// found the pull request, `Followed` when the reviewer chose to follow it
	/// here, and `Reviewed` once the reviewer's own review of it was observed.
	/// None overwrites another, and each is kept or withdrawn on its own.
	struct queue_membership
	{
		github_role Role = github_role::none;

		bool          MatchedRule = false;
		observer_rule Rule;

		bool Authored  = false;
		bool Requested = false;
		bool Followed  = false;
		bool Reviewed  = false;

		[[nodiscard]] auto enrolled() const noexcept -> bool
		{
			return Role != github_role::none || MatchedRule || Authored || Requested || Followed || Reviewed;
		}

		/// \brief Accepted authorship for this exact queue row, independent of other enrollment reasons.
		[[nodiscard]] auto authored() const noexcept -> bool
		{
			return Role == github_role::author || Authored;
		}

		/// \brief Whether a search of a saved repository put it here.
		[[nodiscard]] auto discovered() const noexcept -> bool
		{
			return Authored || Requested;
		}

		auto operator<=>(const queue_membership&) const = default;
	};

	/// \brief Decides whether one pull request belongs in this queue, and why.
	///
	/// \details The whole of the enrollment policy. It reads no clock, reaches
	/// no provider, and returns a decision rather than performing one.
	///
	/// Exclusions beat other *rules*. A reviewer who has said "not the
	/// generated tree" has said something more specific than "this repository",
	/// and resolving the other way would make an exclusion useless the moment
	/// any broader rule existed. The excluded item records the rule that kept
	/// it out, so a reviewer can find out why something they expected is
	/// missing.
	///
	/// When several inclusion rules match, the first in the supplied order is
	/// the one recorded. The list is the reviewer's own, so its order is a
	/// precedence they can edit — and whichever is recorded, the pull request
	/// is in the queue either way. What the order must never change is whether
	/// an item is enrolled at all.
	///
	/// An exclusion never suppresses a GitHub role. Observer rules exist to add
	/// pull requests GitHub did not put in front of the reviewer, and
	/// exclusions exist to stop a broad rule from adding too many. Letting one
	/// hide a review someone was actually asked for turns a tidying feature
	/// into a way to miss work you are accountable for — and the reviewer would
	/// have no reason to look for it, because as far as they know they were
	/// never asked.
	[[nodiscard]] auto enrollment_for(
		const pull_request_observation& aObservation,
		std::span<const observer_rule>  aRules) -> queue_membership;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_HPP
