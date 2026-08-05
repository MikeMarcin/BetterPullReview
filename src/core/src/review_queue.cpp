#include "bpr/core/review_queue.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto matches_path(
			const std::string_view aPattern,
			const std::string_view aPath) noexcept -> bool
		{
			if (aPattern.empty()) {
				return false;
			}
			if (aPattern.back() == '*') {
				const auto prefix = aPattern.substr(0U, aPattern.size() - 1U);
				return aPath.starts_with(prefix);
			}
			if (aPath == aPattern) {
				return true;
			}
			// A directory pattern matches everything beneath it, and only
			// beneath it. Without the separator, `src/a` would match
			// `src/about.cpp`, which is a different file the reviewer did not
			// ask for.
			return aPath.starts_with(aPattern)
				&& (aPattern.back() == '/' || aPath[aPattern.size()] == '/');
		}
	} // namespace

	auto name(const github_role aRole) noexcept -> std::string_view
	{
		switch (aRole) {
		case github_role::none:
			return "none";
		case github_role::participant:
			return "participant";
		case github_role::reviewer:
			return "reviewer";
		case github_role::requested_reviewer:
			return "requested reviewer";
		case github_role::author:
			return "author";
		}
		return "none";
	}

	auto name(const observer_rule_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case observer_rule_kind::repository:
			return "repository";
		case observer_rule_kind::author:
			return "author";
		case observer_rule_kind::path:
			return "path";
		}
		return "repository";
	}

	auto matches(
		const observer_rule&            aRule,
		const pull_request_observation& aObservation) noexcept -> bool
	{
		switch (aRule.Kind) {
		case observer_rule_kind::repository:
			return !aRule.Pattern.empty()
				&& aRule.Pattern == aObservation.Repository;
		case observer_rule_kind::author:
			return !aRule.Pattern.empty()
				&& aRule.Pattern == aObservation.Author;
		case observer_rule_kind::path:
			return std::ranges::any_of(
				aObservation.ChangedPaths,
				[&aRule](const std::string& aPath) {
					return matches_path(aRule.Pattern, aPath);
				});
		}
		return false;
	}

	auto enrollment_for(
		const pull_request_observation&      aObservation,
		const std::span<const observer_rule> aRules) -> queue_membership
	{
		auto membership = queue_membership{ .Role = aObservation.Role };

		// Exclusions are read first and beat any other rule. A reviewer who
		// said "not the generated tree" said something more specific than
		// "this repository", and resolving the other way would make an
		// exclusion useless the moment any broader rule existed.
		//
		// The role is left standing. Observer rules add pull requests GitHub
		// did not put in front of the reviewer, and exclusions stop a broad
		// rule from adding too many; neither is a way to hide a review someone
		// was actually asked for. A reviewer who missed an assigned review
		// because of a path rule would have no reason to go looking, since as
		// far as they knew they were never asked.
		for (const auto& rule : aRules) {
			if (rule.Excluding && matches(rule, aObservation)) {
				membership.Rule = rule;
				return membership;
			}
		}

		for (const auto& rule : aRules) {
			if (!rule.Excluding && matches(rule, aObservation)) {
				membership.MatchedRule = true;
				membership.Rule        = rule;
				break;
			}
		}
		return membership;
	}
} // namespace bpr::core
