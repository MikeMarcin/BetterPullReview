// The Git cache namespace is discriminated by access mode. Signed out and
// signed in are two namespaces over the same repository, and the on-disk
// manifest that gates reuse is what keeps them apart: reopening a cache under
// the other mode is a typed refusal, not a reuse of its objects and refs.
//
// The second half of the course is about what installed Git is handed. The
// process adapter builds its own environment and never inherits a credential
// helper, an askpass hook, or a system or global configuration file, so
// credential discovery is structurally impossible rather than merely
// unconfigured. A signed-out materialization runs under exactly that
// environment.
#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "windows_system_root.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
	namespace application = bpr::application;
	namespace core        = bpr::core;
	namespace git_adapter = bpr::adapters::git;

	void require(const bool aCondition, const std::string_view aMessage)
	{
		bpr::tests::expect(aCondition, aMessage);
	}

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	class temporary_root final
	{
	public:
		temporary_root()
		{
			// [[BPR-1128]], [[BPR-1138]]. Made at a name nothing had, owner-private from creation.
			Path = bpr::tests::make_scratch_directory("bpr-git-anon");
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Path };
			std::filesystem::permissions(
				Path,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
			Path = std::filesystem::canonical(Path);
			made.release();
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		~temporary_root()
		{
			std::error_code error;
			std::filesystem::remove_all(Path, error);
		}

		std::filesystem::path Path;
	};

	[[nodiscard]] auto authenticated_request(const std::uint8_t aCacheByte)
		-> application::git_cache_open_request
	{
		auto result = application::git_cache_open_request{
			.ApplicationSession = core::application_session_id{ 1U },
			.Authority          = {
				.Profile         = core::host_profile_id{ 2U },
				.ProfileRevision = core::github_profile_revision{ 3U },
				.Host            = core::git_host_id{ 4U },
				.HostAuthority   = identity<core::host_authority_fingerprint>(5U),
				.Account         = core::github_account_id{ 6U },
				.Viewer          = core::github_viewer_actor_id{ 7U },
				.Repository      = core::repository_id{ 8U },
				.Cache           = identity<core::repository_cache_scope_id>(
					aCacheByte),
				.PullRequest = core::pull_request_id{ 9U },
				.Number      = core::pull_request_number{ 10U },
				.Scope       = identity<core::pull_request_authority_scope_id>(11U),
				.Mode        = core::github_access_mode::authenticated,
			},
			.ObjectFormat = core::git_hash_algorithm::sha1,
		};
		require(application::valid(result), "authenticated cache request is valid");
		return result;
	}

	// The same profile, host, and repository, read signed out. Account and
	// viewer are absent rather than zeroed by convention: `core::valid` refuses
	// a public-preview authority that carries either one.
	[[nodiscard]] auto anonymous_request(const std::uint8_t aCacheByte)
		-> application::git_cache_open_request
	{
		auto result              = authenticated_request(aCacheByte);
		result.Authority.Account = core::github_account_id{ };
		result.Authority.Viewer  = core::github_viewer_actor_id{ };
		result.Authority.Mode    = core::github_access_mode::public_preview;
		require(
			application::valid(result),
			"public-preview cache request is valid");
		return result;
	}

	[[nodiscard]] auto git_executable() -> std::filesystem::path
	{
		return std::filesystem::path{ BPR_TEST_GIT_EXECUTABLE };
	}

	[[nodiscard]] auto make_cache(const std::filesystem::path& aRoot)
	{
		auto cache = git_adapter::make_installed_git_cache({
			.GitExecutable     = git_executable(),
			.StorageRoot       = aRoot,
			.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
		});
		require(cache.has_value(), "installed Git cache is available");
		return std::move(*cache);
	}

	// The identity rule the authority itself enforces. A signed-out authority
	// carrying an account is not normalized into a signed-in one; it is not a
	// representable authority at all, so nothing downstream has to defend
	// against it.
	void authority_identity_contract()
	{
		auto wearingAnAccount              = anonymous_request(0x21U);
		wearingAnAccount.Authority.Account = core::github_account_id{ 6U };
		require(
			!application::valid(wearingAnAccount),
			"a public-preview authority naming an account is refused");
		auto wearingAViewer             = anonymous_request(0x21U);
		wearingAViewer.Authority.Viewer = core::github_viewer_actor_id{ 7U };
		require(
			!application::valid(wearingAViewer),
			"a public-preview authority naming a viewer is refused");
		auto missingItsAccount              = authenticated_request(0x21U);
		missingItsAccount.Authority.Account = core::github_account_id{ };
		require(
			!application::valid(missingItsAccount),
			"an authenticated authority without an account is refused");
	}

	// The aliasing course. One cache scope, two modes: whichever materializes
	// first owns the on-disk repository, and the other is refused rather than
	// handed the first one's objects.
	void cache_namespace_contract()
	{
		{
			auto       root  = temporary_root{ };
			const auto cache = make_cache(root.Path);
			{
				auto anonymous = cache->open(anonymous_request(0x31U), { });
				require(
					anonymous.has_value(),
					"a signed-out authority materializes its own cache");
			}
			auto authenticated
				= cache->open(authenticated_request(0x31U), { });
			require(
				!authenticated.has_value()
					&& authenticated.error()
						== application::git_cache_failure::invalid_manifest,
				"a signed-in authority cannot reuse a signed-out cache scope");
			auto anonymousAgain = cache->open(anonymous_request(0x31U), { });
			require(
				anonymousAgain.has_value(),
				"the signed-out authority still reopens its own cache");
		}
		{
			auto       root  = temporary_root{ };
			const auto cache = make_cache(root.Path);
			{
				auto authenticated
					= cache->open(authenticated_request(0x32U), { });
				require(
					authenticated.has_value(),
					"a signed-in authority materializes its own cache");
			}
			auto anonymous = cache->open(anonymous_request(0x32U), { });
			require(
				!anonymous.has_value()
					&& anonymous.error()
						== application::git_cache_failure::invalid_manifest,
				"a signed-out authority cannot reuse a signed-in cache scope");
		}
		{
			// Given distinct scopes - which is what the catalog allocates for
			// the two modes - both materialize, side by side, in separate
			// repositories.
			auto       root      = temporary_root{ };
			const auto cache     = make_cache(root.Path);
			auto       anonymous = cache->open(anonymous_request(0x33U), { });
			require(
				anonymous.has_value(),
				"a signed-out authority opens its own scope");
			auto authenticated
				= cache->open(authenticated_request(0x34U), { });
			require(
				authenticated.has_value(),
				"a signed-in authority opens its own scope alongside it");
			auto repositories = std::vector<std::filesystem::path>{ };
			for (const auto& entry : std::filesystem::directory_iterator{
					 root.Path / "repositories" / "v1" }) {
				if (entry.path().extension() == ".git") {
					repositories.push_back(entry.path());
				}
			}
			require(
				repositories.size() == 2U
					&& repositories[0] != repositories[1],
				"the two modes occupy two separate on-disk repositories");
		}
	}

	// Installed Git runs under an environment this adapter builds, and that
	// environment names no credential mechanism at all. This is what makes a
	// signed-out fetch credential-free by construction rather than by the
	// caller remembering not to configure one.
	void credential_free_process_contract()
	{
		// The signed-out materialization itself, not a stand-in for it: this
		// is the same cache open the course above drives, and what it leaves
		// on disk is what installed Git actually ran against.
		auto       root      = temporary_root{ };
		const auto cache     = make_cache(root.Path);
		auto       anonymous = cache->open(anonymous_request(0x41U), { });
		require(
			anonymous.has_value(),
			"a signed-out authority materializes through installed Git");

		// Every name a credential could reach Git through. None may appear in
		// the configuration the repository this adapter created ends up with.
		constexpr std::array<std::string_view, 7U> forbidden{
			"credential",
			"askpass",
			"askPass",
			"helper",
			"extraHeader",
			"username",
			"password",
		};
		auto configurations = 0U;
		for (const auto& entry : std::filesystem::recursive_directory_iterator{
				 root.Path / "repositories" / "v1" }) {
			if (entry.path().filename() != "config") {
				continue;
			}
			++configurations;
			auto file = std::ifstream{ entry.path(), std::ios::binary };
			require(
				static_cast<bool>(file),
				"the materialized repository has a readable configuration");
			const auto text = std::string{
				std::istreambuf_iterator<char>{ file },
				std::istreambuf_iterator<char>{ }
			};
			for (const auto name : forbidden) {
				require(
					text.find(name) == std::string::npos,
					"the materialized Git configuration names no credential "
					"mechanism");
			}
		}
		require(
			configurations == 1U,
			"the signed-out materialization leaves exactly one repository");

		// And nothing inside the isolation root that Git could discover one
		// from if it did look.
		for (const auto& candidate : { std::string{ ".gitconfig" },
									   std::string{ ".git-credentials" },
									   std::string{ "etc" },
									   std::string{ ".config" } }) {
			std::error_code error;
			require(
				!std::filesystem::exists(root.Path / candidate, error),
				"the isolation root holds no discoverable credential source");
		}
	}
} // namespace

auto main(int aCount, char** aArguments) -> int
try {
	QCoreApplication application{ aCount, aArguments };
	authority_identity_contract();
	cache_namespace_contract();
	credential_free_process_contract();
	std::cout << "git-anonymous-public-read-tests:pass\n";
	return 0;
} catch (...) {
	return bpr::tests::failing_exit();
}
