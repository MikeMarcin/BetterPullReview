#ifndef BPR_TESTS_GITHUB_CONTRACT_WINDOWS_CREDENTIAL_MANAGER_TEST_SUPPORT_HPP
#define BPR_TESTS_GITHUB_CONTRACT_WINDOWS_CREDENTIAL_MANAGER_TEST_SUPPORT_HPP

#include "bpr/application/github_credential_vault.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::tests::windows_credential_manager {
	inline void expect(
		const bool             condition,
		const std::string_view message)
	{
		if (!condition) {
			std::cerr << "FAILED: " << message << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] inline auto same_bytes(
		const std::span<const std::byte> actual,
		const std::span<const std::byte> expected) noexcept -> bool
	{
		return std::ranges::equal(actual, expected);
	}

	[[nodiscard]] inline auto material_from(
		const std::span<const std::byte> bytes)
		-> application::github_credential_material
	{
		auto material = application::github_credential_material::create(bytes);
		expect(material.has_value(), "credential material is representable");
		return std::move(*material);
	}

	[[nodiscard]] inline auto authority(
		const std::uint64_t                       profile          = 1U,
		const std::uint64_t                       profile_revision = 2U,
		const std::uint64_t                       host             = 3U,
		const std::uint64_t                       account          = 4U,
		const application::github_server_contract server           = application::github_server_contract::
			github_com_2022_11_28,
		const std::string_view hostname = { })
		-> application::github_credential_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ profile },
			.ProfileRevision = application::github_profile_revision{ profile_revision },
			.Host            = core::git_host_id{ host },
			.Hostname        = application::github_canonical_hostname{
				hostname.empty()
					? (server
							   == application::github_server_contract::
								   github_com_2022_11_28
						   ? "github.com"
						   : "ghe.example.invalid")
					: hostname },
			.Account        = core::github_account_id{ account },
			.ServerContract = server,
		};
	}

	[[nodiscard]] inline auto context(
		const application::github_credential_authority_key& key        = authority(),
		const std::uint64_t                                 request    = 10U,
		const std::uint64_t                                 session    = 11U,
		const std::uint64_t                                 generation = 12U)
		-> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ request },
			.ApplicationSession = core::application_session_id{ session },
			.Authority          = key,
			.IdentityGeneration = core::github_identity_generation{ generation },
		};
	}
}

#endif // BPR_TESTS_GITHUB_CONTRACT_WINDOWS_CREDENTIAL_MANAGER_TEST_SUPPORT_HPP
