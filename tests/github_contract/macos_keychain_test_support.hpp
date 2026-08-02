#ifndef BPR_TESTS_GITHUB_CONTRACT_MACOS_KEYCHAIN_TEST_SUPPORT_HPP
#define BPR_TESTS_GITHUB_CONTRACT_MACOS_KEYCHAIN_TEST_SUPPORT_HPP

#include "bpr/application/github_credential_vault.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::tests::macos_keychain {
	inline void expect(
		const bool             aCondition,
		const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	inline void erase(std::span<std::byte> aBytes) noexcept
	{
		auto* cursor    = reinterpret_cast<volatile unsigned char*>(aBytes.data());
		auto  remaining = aBytes.size();
		while (remaining != 0U) {
			*cursor = 0U;
			++cursor;
			--remaining;
		}
	}

	[[nodiscard]] inline auto same_bytes(
		const std::span<const std::byte> aActual,
		const std::span<const std::byte> aExpected) noexcept -> bool
	{
		return std::ranges::equal(aActual, aExpected);
	}

	[[nodiscard]] inline auto material_from(
		const std::span<const std::byte> aBytes)
		-> application::github_credential_material
	{
		auto material = application::github_credential_material::create(aBytes);
		expect(material.has_value(), "credential material is representable");
		return std::move(*material);
	}

	[[nodiscard]] inline auto authority(
		const std::uint64_t                       aProfile         = 1U,
		const std::uint64_t                       aProfileRevision = 2U,
		const std::uint64_t                       aHost            = 3U,
		const std::uint64_t                       aAccount         = 4U,
		const application::github_server_contract aServer          = application::github_server_contract::
			github_com_2022_11_28,
		const std::string_view aHostname = { })
		-> application::github_credential_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ aProfile },
			.ProfileRevision = application::github_profile_revision{ aProfileRevision },
			.Host            = core::git_host_id{ aHost },
			.Hostname        = application::github_canonical_hostname{
				aHostname.empty()
					? (aServer
							   == application::github_server_contract::
								   github_com_2022_11_28
						   ? "github.com"
						   : "ghe.example.invalid")
					: aHostname },
			.Account        = core::github_account_id{ aAccount },
			.ServerContract = aServer,
		};
	}

	[[nodiscard]] inline auto context(
		const application::github_credential_authority_key& aKey        = authority(),
		const std::uint64_t                                 aRequest    = 10U,
		const std::uint64_t                                 aSession    = 11U,
		const std::uint64_t                                 aGeneration = 12U)
		-> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ aRequest },
			.ApplicationSession = core::application_session_id{ aSession },
			.Authority          = aKey,
			.IdentityGeneration = core::github_identity_generation{ aGeneration },
		};
	}
}

#endif // BPR_TESTS_GITHUB_CONTRACT_MACOS_KEYCHAIN_TEST_SUPPORT_HPP
