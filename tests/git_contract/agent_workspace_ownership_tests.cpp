#include "bpr/adapters/git/agent_workspace_ownership.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
	namespace application = bpr::application;
	namespace core        = bpr::core;
	namespace git         = bpr::adapters::git;
	std::filesystem::path root;
	std::string           phase;

	template <typename Identity>
	auto identity(std::uint8_t aByte) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes.fill(aByte);
		return Identity{ bytes };
	}

	auto request() -> application::agent_workspace_materialization_request
	{
		const core::pull_request_authority authority{
			.Profile         = core::host_profile_id{ 1U },
			.ProfileRevision = core::github_profile_revision{ 1U },
			.Host            = core::git_host_id{ 1U },
			.HostAuthority   = identity<core::host_authority_fingerprint>(1U),
			.Account         = core::github_account_id{ 1U },
			.Viewer          = core::github_viewer_actor_id{ 1U },
			.Repository      = core::repository_id{ 1U },
			.Cache           = identity<core::repository_cache_scope_id>(2U),
			.PullRequest     = core::pull_request_id{ 1U },
			.Number          = core::pull_request_number{ 1U },
			.Scope           = identity<core::pull_request_authority_scope_id>(3U),
		};
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0] = 1U;
		const core::git_commit_oid head{ core::git_hash_algorithm::sha1, bytes };
		return {
			.ApplicationSession = core::application_session_id{ 1U },
			.Authority          = authority,
			.Session            = {
				.Id      = core::agent_session_id{ 1U },
				.Profile = core::external_terminal_profile_id,
				.Binding = {
					.Host        = authority.Host,
					.Repository  = authority.Repository,
					.PullRequest = authority.PullRequest,
					.BaseOid     = head,
					.HeadOid     = head,
					.LeftEpoch   = core::epoch_ordinal{ 1U },
					.RightEpoch  = core::epoch_ordinal{ 1U },
				},
			},
			.ParentDirectory = root / "neighbourhood",
		};
	}
}

namespace bpr::adapters::git {
	void installed_workspace_checkpoint(std::string_view aName)
	{
		if (aName != phase) {
			return;
		}
		std::ofstream marker{ root / "kill-ready" };
		marker << phase;
		marker.close();
		static_cast<void>(std::cin.get());
		throw std::runtime_error{ "checkpoint resumed without process death" };
	}
}

auto main(int argc, char** argv) -> int
{
	try {
		if (argc != 3) {
			return 2;
		}
		phase               = argv[1];
		root                = std::filesystem::canonical(argv[2]);
		const auto registry = root / "registry";
		if (phase == "recover" || phase == "cancelled") {
			std::stop_source stop;
			if (phase == "cancelled") {
				stop.request_stop();
			}
			const auto result = git::recover_installed_agent_workspaces(registry, root / "neighbourhood", stop.get_token());
			if (result) {
				std::cout << "recovered=" << result->Recovered << " live=" << result->Live << '\n';
			} else {
				std::cout << "failure=" << static_cast<int>(result.error()) << '\n';
			}
			return 0;
		}
		auto result = git::prepare_installed_agent_workspace_ownership(registry, request());
		if (!result) {
			std::cout << "failure=" << static_cast<int>(result.error()) << '\n';
			return 0;
		}
		auto       owner  = std::move(*result);
		const auto layout = owner->layout();
		if (phase == "abandon") {
			std::cout << "prepared\n";
			return 0;
		}
		std::filesystem::create_directory(layout.Workspace / ".git");
		std::filesystem::permissions(layout.Workspace / ".git", std::filesystem::perms::owner_all);
		{
			std::ofstream marker{ layout.Workspace / ".git" / "bpr-owned-workspace" };
			marker << layout.OwnershipMarker;
		}
		std::filesystem::permissions(layout.Workspace / ".git" / "bpr-owned-workspace", std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
		if (!owner->mark_ready()) {
			throw std::runtime_error{ "mark ready failed" };
		}
		if (phase == "busy-cleanup") {
			const auto guard = ::open(layout.Guard.Path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
			if (guard < 0 || ::flock(guard, LOCK_SH | LOCK_NB) != 0) {
				throw std::runtime_error{ "independent guard lock failed" };
			}
			const auto removed = owner->cleanup();
			static_cast<void>(::close(guard));
			if (removed) {
				throw std::runtime_error{ "cleanup bypassed independent live owner" };
			}
			const auto recovered = git::recover_installed_agent_workspaces(registry, root / "neighbourhood");
			if (!recovered || recovered->Live != 1U || recovered->Recovered != 0U) {
				throw std::runtime_error{ "failed cleanup lost shared ownership" };
			}
		}
		git::installed_workspace_checkpoint("live");
		if (phase == "ready-abandon") {
			std::cout << "ready\n";
			return 0;
		}
		if (!owner->cleanup()) {
			throw std::runtime_error{ "cleanup failed" };
		}
		std::cout << "cleaned\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
