#include "bpr/core/workspace_git_isolation.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace bpr::core {
	namespace {
		/// \brief Each single-valued key the move's verbs (status, add,
		/// commit, checkout, rebase, reset, diff, log, merge-base, rev-parse,
		/// symbolic-ref, update-ref, cat-file, rev-list, pack-objects) can
		/// read to start a program or to change what the move observes.
		/// Transport keys (`core.sshCommand`, `core.gitProxy`, `remote.*`,
		/// `url.*`, URL-scoped credential helpers) are not listed: every
		/// protocol is refused and none of the verbs reaches a remote.
		constexpr auto fixed_overrides = std::to_array<std::pair<std::string_view, std::string_view>>({
			// The filesystem monitor is a program Git starts on every index refresh.
			{ "core.fsmonitor", "false" },
			// `:` is Git's own no-op editor; none of the verbs opens one.
			{ "core.editor", ":" },
			{ "sequence.editor", ":" },
			{ "core.askPass", "" },
			{ "credential.helper", "" },
			{ "credential.interactive", "never" },
			// A patch is never produced, but no external diff runs if one were.
			{ "diff.external", "" },
			// A signed commit in the listed history would start `gpg.program`.
			{ "log.showSignature", "false" },
			{ "commit.gpgSign", "false" },
			// The apply backend runs `am`; rerere would resolve a conflict
			// from resolutions the agent recorded.
			{ "rebase.backend", "merge" },
			{ "rebase.updateRefs", "false" },
			{ "rebase.autoStash", "false" },
			{ "rebase.autoSquash", "false" },
			{ "rerere.enabled", "false" },
			{ "rerere.autoUpdate", "false" },
			// The output is parsed.
			{ "color.ui", "false" },
			// A tree from the pull request is checked out.
			{ "core.protectHFS", "true" },
			{ "core.protectNTFS", "true" },
			{ "submodule.recurse", "false" },
			{ "gc.auto", "0" },
			{ "maintenance.auto", "false" },
			{ "protocol.allow", "never" },
			{ "protocol.file.allow", "never" },
			{ "protocol.http.allow", "never" },
			{ "protocol.https.allow", "never" },
			{ "protocol.ssh.allow", "never" },
			{ "protocol.git.allow", "never" },
			{ "protocol.ext.allow", "never" },
		});

		/// \brief The keys of a driver that name its programs, and the value
		/// each is given. An empty command runs nothing.
		constexpr auto filter_keys = std::to_array<std::pair<std::string_view, std::string_view>>({
			{ "clean", "" },
			{ "smudge", "" },
			{ "process", "" },
			{ "required", "false" },
		});
		constexpr auto diff_keys   = std::to_array<std::pair<std::string_view, std::string_view>>({
			{ "textconv", "" },
			{ "command", "" },
		});
		// An empty merge driver cannot be started: the path conflicts.
		constexpr auto merge_keys = std::to_array<std::pair<std::string_view, std::string_view>>({
			{ "driver", "" },
		});

		struct listed_key
		{
			std::string_view Section;
			std::string_view Subsection;
			bool             HasSubsection = false;
		};

		/// \brief Git prints `section[.subsection].key`; the subsection may
		/// itself hold dots, so the key follows the last one.
		[[nodiscard]] auto split_key(const std::string_view aName) -> std::optional<listed_key>
		{
			const auto first = aName.find('.');
			const auto last  = aName.rfind('.');
			if (first == std::string_view::npos || first == 0U || last + 1U == aName.size()) {
				return std::nullopt;
			}
			if (first == last) {
				return listed_key{ .Section = aName.substr(0U, first) };
			}
			return listed_key{ .Section = aName.substr(0U, first), .Subsection = aName.substr(first + 1U, last - first - 1U), .HasSubsection = true };
		}

		template <std::size_t TCount>
		auto append_driver(std::vector<git_config_override>& aOverrides, const std::string_view aSection, const std::string_view aDriver,
						   const std::array<std::pair<std::string_view, std::string_view>, TCount>& aKeys) -> void
		{
			for (const auto& [key, value] : aKeys) {
				auto name = std::string{ aSection };
				name += '.';
				name += aDriver;
				name += '.';
				name += key;
				aOverrides.push_back(git_config_override{ .Key = std::move(name), .Value = std::string{ value } });
			}
		}
	}

	auto fixed_workspace_git_overrides() -> std::vector<git_config_override>
	{
		auto result = std::vector<git_config_override>{ };
		result.reserve(fixed_overrides.size());
		for (const auto& [key, value] : fixed_overrides) {
			result.push_back(git_config_override{ .Key = std::string{ key }, .Value = std::string{ value } });
		}
		return result;
	}

	auto plan_workspace_git_isolation(const std::string_view aListing) -> workspace_git_isolation
	{
		auto result = workspace_git_isolation{ .Overrides = fixed_workspace_git_overrides() };
		if (!aListing.empty() && aListing.back() != '\0') {
			return workspace_git_isolation{ .Refusal = workspace_git_refusal::malformed_listing };
		}
		auto seen  = std::vector<std::pair<std::string_view, std::string_view>>{ };
		auto start = std::size_t{ 0U };
		while (start < aListing.size()) {
			const auto end  = aListing.find('\0', start);
			const auto name = aListing.substr(start, end - start);
			start           = end + 1U;
			const auto key  = split_key(name);
			if (!key || name.find('\n') != std::string_view::npos) {
				return workspace_git_isolation{ .Refusal = workspace_git_refusal::malformed_listing };
			}
			if (key->Section == "hook") {
				return workspace_git_isolation{ .Refusal = workspace_git_refusal::configured_hook };
			}
			const auto driven = key->Section == "filter" || key->Section == "diff" || key->Section == "merge";
			if (!driven || !key->HasSubsection) {
				continue;
			}
			const auto driver = std::pair{ key->Section, key->Subsection };
			if (std::ranges::find(seen, driver) != seen.end()) {
				continue;
			}
			if (seen.size() == maximum_workspace_git_drivers) {
				return workspace_git_isolation{ .Refusal = workspace_git_refusal::too_many_drivers };
			}
			seen.push_back(driver);
			if (key->Section == "filter") {
				append_driver(result.Overrides, key->Section, key->Subsection, filter_keys);
			} else if (key->Section == "diff") {
				append_driver(result.Overrides, key->Section, key->Subsection, diff_keys);
			} else {
				append_driver(result.Overrides, key->Section, key->Subsection, merge_keys);
			}
		}
		return result;
	}
}
