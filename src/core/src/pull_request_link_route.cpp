#include "bpr/core/pull_request_link_route.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto same_ascii(const std::string_view aLeft, const std::string_view aRight) -> bool
		{
			const auto lower = [](const char aChar) { return aChar >= 'A' && aChar <= 'Z' ? static_cast<char>(aChar + ('a' - 'A')) : aChar; };
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [&](const char aA, const char aB) { return lower(aA) == lower(aB); });
		}

		[[nodiscard]] auto names_scope(const setup_destination& aLink, const pull_request_link_scope& aScope) -> bool
		{
			return aLink.Kind == setup_destination_kind::pull_request && !aScope.Hostname.empty() && !aScope.Owner.empty() && !aScope.Repository.empty()
				&& aScope.Number.valid() && aLink.Number == aScope.Number && same_ascii(aLink.Hostname, aScope.Hostname)
				&& same_ascii(aLink.Repository.Owner, aScope.Owner) && same_ascii(aLink.Repository.Name, aScope.Repository);
		}
	}

	auto route_pull_request_link(const setup_destination&                   aLink,
								 const pull_request_link_scope&             aScope,
								 const std::span<const tour_remote_binding> aRoots,
								 const pull_request_location_facts&         aFacts) -> pull_request_link_decision
	{
		if (!names_scope(aLink, aScope)) {
			return { };
		}
		const auto& target = aLink.Target;
		if ((target.Kind == pull_request_location_kind::review || target.Kind == pull_request_location_kind::issue_comment) && target.Identity != 0U) {
			const auto kind = target.Kind == pull_request_location_kind::review ? tour_remote_kind::review : tour_remote_kind::conversation_comment;
			if (const auto root = std::ranges::find_if(aRoots, [&](const tour_remote_binding& aRoot) { return aRoot.Kind == kind && aRoot.DatabaseId == target.Identity; });
				root != aRoots.end()) {
				return { .Route = pull_request_link_route::tour, .Tour = static_cast<std::size_t>(root - aRoots.begin()) };
			}
		}
		if (target.Kind == pull_request_location_kind::pull_request) {
			if (aLink.Location.empty()) {
				return { .Route = pull_request_link_route::conversation_page };
			}
			if (aLink.Location == "/files") {
				return { .Route = pull_request_link_route::files_page };
			}
			if (aLink.Location == "/commits") {
				return { .Route = pull_request_link_route::commits_page };
			}
			return { };
		}
		if (target.Kind == pull_request_location_kind::commit) {
			// GitHub's own links name the whole commit; a shorter name names
			// one only when it is unambiguous among this pull request's.
			const auto prefix  = std::string_view{ target.Commit };
			const auto matches = [prefix](const std::string& aCommit) {
				return prefix.size() >= 7U && aCommit.size() >= prefix.size() && same_ascii(std::string_view{ aCommit }.substr(0, prefix.size()), prefix);
			};
			const auto found = std::ranges::find_if(aScope.Commits, matches);
			if (found == aScope.Commits.end() || std::ranges::count_if(aScope.Commits, matches) != 1) {
				return { };
			}
			auto decision   = pull_request_link_decision{ };
			decision.Route  = pull_request_link_route::commit;
			decision.Commit = *found;
			return decision;
		}
		auto resolved = resolve_pull_request_location(target, aFacts);
		if (resolved.Action != pull_request_location_action::reveal_conversation && resolved.Action != pull_request_location_action::select_file) {
			return { };
		}
		return { .Route = pull_request_link_route::location, .Target = std::move(resolved) };
	}
} // namespace bpr::core
