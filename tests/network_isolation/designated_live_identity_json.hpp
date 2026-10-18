#ifndef BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_IDENTITY_JSON_HPP
#define BPR_TESTS_NETWORK_ISOLATION_DESIGNATED_LIVE_IDENTITY_JSON_HPP

#include "designated_live_mutation_gate.hpp"

#include <glaze/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::tests::network_isolation::identity_json {
	// Own the original scalar tokens and select final object members before
	// decoding types. This retains the previous DOM's last-member policy without
	// rounding a numeric identity through a floating-point intermediate.
	using object = std::map<std::string, glz::raw_json, std::less<>>;
	struct options : glz::opts
	{
		bool validate_skipped             = true; // NOLINT(readability-identifier-naming): Glaze option names.
		bool validate_trailing_whitespace = true; // NOLINT(readability-identifier-naming): Glaze option names.
		constexpr options() { null_terminated = false; }
	};
	template <typename Value>
	[[nodiscard]] auto required(const object& aParent, const std::string_view aName)
		-> std::optional<Value>
	{
		const auto member = aParent.find(aName);
		if (member == aParent.end()) {
			return std::nullopt;
		}
		Value value{ };
		if (glz::read<options{ }>(value, member->second.str)) {
			return std::nullopt;
		}
		return value;
	}
}

namespace bpr::tests::network_isolation {
	// Parsing is separate from the live reader so fixtures can exercise the exact
	// identity boundary without creating a network manager or consuming approval.
	[[nodiscard]] inline auto parse_designated_live_identity(const std::string_view aBody)
		-> std::optional<designated_live_identity>
	{
		using identity_json::object;
		using identity_json::required;
		object root;
		if (glz::read<identity_json::options{ }>(root, aBody)) {
			return std::nullopt;
		}
		const auto head = required<object>(root, "head");
		const auto base = required<object>(root, "base");
		if (!head || !base) {
			return std::nullopt;
		}
		const auto repository = required<object>(*base, "repo");
		if (!repository) {
			return std::nullopt;
		}
		const auto owner = required<object>(*repository, "owner");
		if (!owner) {
			return std::nullopt;
		}
		const auto repositoryId     = required<std::uint64_t>(*repository, "id");
		const auto repositoryNodeId = required<std::string>(*repository, "node_id");
		const auto repositoryName   = required<std::string>(*repository, "name");
		const auto repositoryOwner  = required<std::string>(*owner, "login");
		const auto number           = required<std::uint64_t>(root, "number");
		const auto id               = required<std::uint64_t>(root, "id");
		const auto nodeId           = required<std::string>(root, "node_id");
		const auto state            = required<std::string>(root, "state");
		const auto headSha          = required<std::string>(*head, "sha");
		const auto headBranch       = required<std::string>(*head, "ref");
		const auto baseSha          = required<std::string>(*base, "sha");
		if (!repositoryId || !repositoryNodeId || !repositoryName || !repositoryOwner
			|| !number || !id || !nodeId || !state || !headSha || !headBranch || !baseSha) {
			return std::nullopt;
		}
		return designated_live_identity{
			.Host                     = "github.com",
			.Owner                    = *repositoryOwner,
			.Repository               = *repositoryName,
			.RepositoryDatabaseId     = *repositoryId,
			.RepositoryNodeId         = *repositoryNodeId,
			.PullRequestNumber        = *number,
			.PullRequestDatabaseId    = *id,
			.PullRequestNodeId        = *nodeId,
			.BaseRepositoryDatabaseId = *repositoryId,
			.State                    = *state,
			.HeadSha                  = *headSha,
			.BaseSha                  = *baseSha,
			.HeadBranch               = *headBranch,
		};
	}
}

#endif
