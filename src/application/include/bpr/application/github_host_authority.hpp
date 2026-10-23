#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_HOST_AUTHORITY_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_HOST_AUTHORITY_HPP

#include "bpr/core/github_hostname.hpp"
#include "bpr/core/github_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bpr::application {
	/// \brief Provides the github revision contract.
	template <typename Tag>
	class github_revision
	{
	public:
		constexpr github_revision() = default;
		explicit constexpr github_revision(const std::uint64_t aValue) noexcept
			: Value{ aValue }
		{
		}

		[[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t
		{
			return Value;
		}

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			return Value != 0U;
		}

		auto operator<=>(const github_revision&) const = default;

	private:
		std::uint64_t Value = 0U;
	};

	struct github_profile_revision_tag;
	struct github_probe_contract_revision_tag;

	using github_profile_revision        = github_revision<github_profile_revision_tag>;
	using github_probe_contract_revision = github_revision<github_probe_contract_revision_tag>;

	using core::maximum_github_hostname_bytes;
	using core::maximum_github_hostname_label_bytes;
	using core::valid_github_canonical_hostname;

	/// \brief Provides the github canonical hostname contract.
	class github_canonical_hostname final
	{
	public:
		constexpr github_canonical_hostname() = default;

		explicit constexpr github_canonical_hostname(
			const std::string_view aHostname) noexcept
		{
			if (!valid_github_canonical_hostname(aHostname)) {
				return;
			}
			for (std::size_t index = 0U; index < aHostname.size(); ++index) {
				Bytes[index] = aHostname[index];
			}
			Size = static_cast<std::uint8_t>(aHostname.size());
		}

		[[nodiscard]] constexpr auto value() const noexcept -> std::string_view
		{
			return { Bytes.data(), Size };
		}

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			return Size != 0U;
		}

		auto operator<=>(const github_canonical_hostname&) const = default;

	private:
		std::array<char, maximum_github_hostname_bytes> Bytes{ };
		std::uint8_t                                    Size = 0U;
	};

	/// \brief Provides the github freshness epoch contract.
	class github_freshness_epoch
	{
	public:
		constexpr github_freshness_epoch() = default;
		explicit constexpr github_freshness_epoch(
			const std::uint64_t aValue) noexcept
			: Value{ aValue }
		{
		}

		[[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t
		{
			return Value;
		}

		auto operator<=>(const github_freshness_epoch&) const = default;

	private:
		std::uint64_t Value = 0U;
	};

	enum class github_server_contract : std::uint8_t {
		github_com_2022_11_28,
		ghes_3_18_2022_11_28,
	};

	enum class github_product_kind : std::uint8_t {
		unknown,
		github_com,
		github_enterprise_server,
	};

	enum class github_authentication_transport : std::uint8_t {
		github_cli,
		native_https,
	};

	enum class github_proxy_mode : std::uint8_t {
		system,
	};

	enum class github_trust_mode : std::uint8_t {
		system,
	};

	/// \brief Describes the github profile authority record.
	struct github_profile_authority
	{
		core::host_profile_id           Profile;
		github_profile_revision         ProfileRevision;
		core::git_host_id               Host;
		github_canonical_hostname       Hostname;
		github_server_contract          ServerContract = github_server_contract::github_com_2022_11_28;
		github_authentication_transport Authentication = github_authentication_transport::native_https;
		github_probe_contract_revision  ProbeContractRevision;

		auto operator<=>(const github_profile_authority&) const = default;
	};

	/// \brief Describes the github read shape record.
	struct github_read_shape
	{
		bool ReviewThreads       = false;
		bool ViewedFileState     = false;
		bool ResolvePermission   = false;
		bool UnresolvePermission = false;
		/// [[BPR-1746]]. Each review thread carries GitHub's
		/// `viewerCanReply`; needs `ReviewThreads`.
		bool ReplyPermission = false;
		/// [[BPR-1746]]. The conversation read carries the pull
		/// request's `locked`; needs `ReviewThreads`.
		bool ConversationLock = false;

		auto operator<=>(const github_read_shape&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_server_contract aContract) noexcept -> bool
	{
		switch (aContract) {
		case github_server_contract::github_com_2022_11_28:
		case github_server_contract::ghes_3_18_2022_11_28:
			return true;
		}
		return false;
	}

	/// \brief Computes the valid github authority hostname result.
	[[nodiscard]] constexpr auto valid_github_authority_hostname(
		const github_server_contract     aContract,
		const github_canonical_hostname& aHostname) noexcept -> bool
	{
		if (!valid(aContract) || !aHostname.valid()) {
			return false;
		}
		constexpr std::string_view githubHostname = "github.com";
		switch (aContract) {
		case github_server_contract::github_com_2022_11_28:
			return aHostname.value() == githubHostname;
		case github_server_contract::ghes_3_18_2022_11_28:
			return aHostname.value() != githubHostname
				&& !aHostname.value().ends_with(
					std::string_view{ ".github.com" });
		}
		return false;
	}

	// The compiled authority a GitHub.com device-authorization verification
	// address must sit under. It is a constant rather than a setting because an
	// operator who can edit it can be talked into editing it, and the whole
	// point of the check is that the address the server sent is not the one
	// that decides.
	//
	// One declaration, two independent checks, deliberately: the GitHub
	// adapter's wire parser refuses an off-authority `verification_uri` before
	// the address ever reaches the host, and the host refuses it again against
	// the authority composition handed it before any browser is invoked. Both
	// checks stay; what was two spellings of one string is now one, so a
	// registration that ever moved the device endpoints cannot leave half the
	// product still trusting the old authority.
	//
	// It is deliberately not `valid_github_authority_hostname`'s literal above,
	// which answers a different question — which hostname the GitHub.com server
	// contract names — and would have to keep answering it if these ever
	// diverged.
	inline constexpr std::string_view default_device_verification_authority
		= "github.com";

	/// \brief Computes the product kind result.
	[[nodiscard]] constexpr auto product_kind(
		const github_server_contract aContract) noexcept
		-> github_product_kind
	{
		switch (aContract) {
		case github_server_contract::github_com_2022_11_28:
			return github_product_kind::github_com;
		case github_server_contract::ghes_3_18_2022_11_28:
			return github_product_kind::github_enterprise_server;
		}
		return github_product_kind::unknown;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_product_kind aProduct) noexcept -> bool
	{
		return aProduct == github_product_kind::github_com
			|| aProduct == github_product_kind::github_enterprise_server;
	}

	/// \brief Computes the rest api version result.
	[[nodiscard]] constexpr auto rest_api_version(
		const github_server_contract aContract) noexcept -> std::string_view
	{
		return valid(aContract) ? "2022-11-28" : std::string_view{ };
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_authentication_transport aTransport) noexcept -> bool
	{
		switch (aTransport) {
		case github_authentication_transport::github_cli:
		case github_authentication_transport::native_https:
			return true;
		}
		return false;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_proxy_mode aMode) noexcept -> bool
	{
		return aMode == github_proxy_mode::system;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_trust_mode aMode) noexcept -> bool
	{
		return aMode == github_trust_mode::system;
	}

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_profile_authority& aAuthority) noexcept -> bool
	{
		return aAuthority.Profile.value() != 0U
			&& aAuthority.ProfileRevision.valid()
			&& aAuthority.Host.value() != 0U
			&& valid_github_authority_hostname(
				   aAuthority.ServerContract, aAuthority.Hostname)
			&& valid(aAuthority.Authentication)
			&& aAuthority.ProbeContractRevision.valid();
	}
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_HOST_AUTHORITY_HPP
