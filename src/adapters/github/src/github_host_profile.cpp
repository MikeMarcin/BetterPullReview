#include "github_host_profile.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace bpr::adapters::github {
	namespace {
		constexpr std::size_t      maximum_redirect_target_bytes = 2U * 1024U;
		constexpr std::string_view github_com_rest_root          = "https://api.github.com";
		constexpr std::string_view github_com_graphql_root       = "https://api.github.com/graphql";
		constexpr std::string_view https_prefix                  = "https://";

		[[nodiscard]] constexpr auto ascii_lower_letter(const char aValue) noexcept
			-> bool
		{
			return aValue >= 'a' && aValue <= 'z';
		}

		[[nodiscard]] constexpr auto ascii_digit(const char aValue) noexcept -> bool
		{
			return aValue >= '0' && aValue <= '9';
		}

		[[nodiscard]] constexpr auto valid_provenance(
			const github_profile_provenance aProvenance) noexcept -> bool
		{
			switch (aProvenance) {
			case github_profile_provenance::explicit_user:
			case github_profile_provenance::builtin:
			case github_profile_provenance::managed:
			case github_profile_provenance::provider_content:
			case github_profile_provenance::redirect:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto trusted_provenance(
			const github_profile_provenance aProvenance) noexcept -> bool
		{
			return aProvenance == github_profile_provenance::explicit_user
				|| aProvenance == github_profile_provenance::builtin
				|| aProvenance == github_profile_provenance::managed;
		}

		[[nodiscard]] auto enterprise_rest_root(const std::string_view aHostname)
			-> std::string
		{
			return std::string{ https_prefix } + std::string{ aHostname } + "/api/v3";
		}

		[[nodiscard]] auto enterprise_graphql_root(const std::string_view aHostname)
			-> std::string
		{
			return std::string{ https_prefix } + std::string{ aHostname } + "/api/graphql";
		}

		[[nodiscard]] constexpr auto valid_request_surface(
			const github_request_surface aSurface) noexcept -> bool
		{
			return aSurface == github_request_surface::rest
				|| aSurface == github_request_surface::graphql;
		}

		[[nodiscard]] constexpr auto valid_path_character(const char aValue) noexcept
			-> bool
		{
			return ascii_lower_letter(aValue)
				|| (aValue >= 'A' && aValue <= 'Z') || ascii_digit(aValue)
				|| aValue == '/' || aValue == '-' || aValue == '_'
				|| aValue == '.' || aValue == '~';
		}

		[[nodiscard]] auto canonical_redirect_path(const std::string_view aPath) noexcept
			-> bool
		{
			if (aPath.empty() || aPath.front() != '/' || aPath.starts_with("//")) {
				return false;
			}
			if (!std::ranges::all_of(aPath, valid_path_character)) {
				return false;
			}

			std::size_t segmentBegin = 1U;
			while (segmentBegin <= aPath.size()) {
				const auto segmentEnd = aPath.find('/', segmentBegin);
				const auto end        = segmentEnd == std::string_view::npos ? aPath.size() : segmentEnd;
				const auto segment    = aPath.substr(segmentBegin, end - segmentBegin);
				if (segment == "." || segment == "..") {
					return false;
				}
				if (segmentEnd == std::string_view::npos) {
					break;
				}
				if (segment.empty()) {
					return false;
				}
				segmentBegin = segmentEnd + 1U;
			}
			return true;
		}

		[[nodiscard]] auto within_namespace(
			const std::string_view aPath,
			const std::string_view aPrefix) noexcept -> bool
		{
			if (aPrefix.empty()) {
				return true;
			}
			return aPath == aPrefix
				|| (aPath.size() > aPrefix.size()
					&& aPath.starts_with(aPrefix)
					&& aPath[aPrefix.size()] == '/');
		}
	}

	validated_github_host_profile::validated_github_host_profile(
		const github_host_profile_candidate& aCandidate)
		: Profile{ aCandidate.Profile }
		, Host{ aCandidate.Host }
		, Revision{ aCandidate.Revision }
		, Server{ aCandidate.Server }
		, Authentication{ aCandidate.Authentication }
		, Proxy{ aCandidate.Proxy }
		, Trust{ aCandidate.Trust }
		, Provenance{ aCandidate.Provenance }
		, Hostname{ aCandidate.Hostname }
		, RestRoot{ aCandidate.RestRoot }
		, GraphqlRoot{ aCandidate.GraphqlRoot }
		, CliAccountLogin{ aCandidate.CliAccountLogin }
	{
	}

	auto validated_github_host_profile::cli_account_login() const noexcept -> std::string_view
	{
		return CliAccountLogin;
	}

	auto valid_cli_account_login(const std::string_view aLogin) noexcept -> bool
	{
		const auto alphanumeric = [](const unsigned char aByte) {
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9');
		};
		return !aLogin.empty() && aLogin.size() <= 64U && alphanumeric(static_cast<unsigned char>(aLogin.front()))
			&& std::ranges::all_of(aLogin, [&](const unsigned char aByte) {
				   return alphanumeric(aByte) || aByte == '-' || aByte == '_';
			   });
	}

	auto validated_github_host_profile::profile() const noexcept
		-> core::host_profile_id
	{
		return Profile;
	}

	auto validated_github_host_profile::host() const noexcept -> core::git_host_id
	{
		return Host;
	}

	auto validated_github_host_profile::revision() const noexcept
		-> application::github_profile_revision
	{
		return Revision;
	}

	auto validated_github_host_profile::server_contract() const noexcept
		-> application::github_server_contract
	{
		return Server;
	}

	auto validated_github_host_profile::product_kind() const noexcept
		-> application::github_product_kind
	{
		return application::product_kind(Server);
	}

	auto validated_github_host_profile::authentication_transport() const noexcept
		-> application::github_authentication_transport
	{
		return Authentication;
	}

	auto validated_github_host_profile::proxy_mode() const noexcept
		-> application::github_proxy_mode
	{
		return Proxy;
	}

	auto validated_github_host_profile::trust_mode() const noexcept
		-> application::github_trust_mode
	{
		return Trust;
	}

	auto validated_github_host_profile::provenance() const noexcept
		-> github_profile_provenance
	{
		return Provenance;
	}

	auto validated_github_host_profile::hostname() const noexcept
		-> std::string_view
	{
		return Hostname;
	}

	auto validated_github_host_profile::rest_root() const noexcept
		-> std::string_view
	{
		return RestRoot;
	}

	auto validated_github_host_profile::graphql_root() const noexcept
		-> std::string_view
	{
		return GraphqlRoot;
	}

	auto validated_github_host_profile::rest_authority() const noexcept
		-> std::string_view
	{
		return Server
				== application::github_server_contract::github_com_2022_11_28
			? std::string_view{ "api.github.com" }
			: std::string_view{ Hostname };
	}

	auto validated_github_host_profile::rest_route_prefix() const noexcept
		-> std::string_view
	{
		return Server
				== application::github_server_contract::github_com_2022_11_28
			? std::string_view{ }
			: std::string_view{ "/api/v3" };
	}

	auto validated_github_host_profile::graphql_route() const noexcept
		-> std::string_view
	{
		return Server
				== application::github_server_contract::github_com_2022_11_28
			? std::string_view{ "/graphql" }
			: std::string_view{ "/api/graphql" };
	}

	auto validated_github_host_profile::rest_api_version() const noexcept
		-> std::string_view
	{
		return application::rest_api_version(Server);
	}

	auto validate_github_host_profile(
		const github_host_profile_candidate& aCandidate) -> github_profile_result
	{
		if (aCandidate.Profile.value() == 0U) {
			return std::unexpected{
				github_profile_failure::invalid_profile_identity
			};
		}
		if (aCandidate.Host.value() == 0U) {
			return std::unexpected{
				github_profile_failure::invalid_host_identity
			};
		}
		if (!aCandidate.Revision.valid()) {
			return std::unexpected{
				github_profile_failure::invalid_profile_revision
			};
		}
		if (!application::valid(aCandidate.Server)) {
			return std::unexpected{
				github_profile_failure::invalid_server_contract
			};
		}
		if (!application::valid(aCandidate.Authentication)) {
			return std::unexpected{
				github_profile_failure::invalid_authentication_transport
			};
		}
		if (!application::valid(aCandidate.Proxy)) {
			return std::unexpected{ github_profile_failure::invalid_proxy_mode };
		}
		if (!application::valid(aCandidate.Trust)) {
			return std::unexpected{ github_profile_failure::invalid_trust_mode };
		}
		if (!valid_provenance(aCandidate.Provenance)) {
			return std::unexpected{
				github_profile_failure::invalid_provenance
			};
		}
		if (!trusted_provenance(aCandidate.Provenance)) {
			return std::unexpected{
				github_profile_failure::untrusted_provenance
			};
		}
		if (!application::valid_github_canonical_hostname(aCandidate.Hostname)) {
			return std::unexpected{ github_profile_failure::invalid_hostname };
		}

		std::string expectedRestRoot;
		std::string expectedGraphqlRoot;
		if (
			aCandidate.Server
			== application::github_server_contract::github_com_2022_11_28) {
			if (!application::valid_github_authority_hostname(
					aCandidate.Server,
					application::github_canonical_hostname{
						aCandidate.Hostname })) {
				return std::unexpected{ github_profile_failure::invalid_hostname };
			}
			expectedRestRoot    = github_com_rest_root;
			expectedGraphqlRoot = github_com_graphql_root;
		} else {
			if (!application::valid_github_authority_hostname(
					aCandidate.Server,
					application::github_canonical_hostname{
						aCandidate.Hostname })) {
				return std::unexpected{ github_profile_failure::invalid_hostname };
			}
			expectedRestRoot    = enterprise_rest_root(aCandidate.Hostname);
			expectedGraphqlRoot = enterprise_graphql_root(aCandidate.Hostname);
		}

		if (aCandidate.RestRoot != expectedRestRoot) {
			return std::unexpected{ github_profile_failure::invalid_rest_root };
		}
		if (aCandidate.GraphqlRoot != expectedGraphqlRoot) {
			return std::unexpected{ github_profile_failure::invalid_graphql_root };
		}
		if (!aCandidate.CliAccountLogin.empty()
			&& (aCandidate.Authentication != application::github_authentication_transport::github_cli
				|| !valid_cli_account_login(aCandidate.CliAccountLogin))) {
			return std::unexpected{ github_profile_failure::invalid_cli_account };
		}
		const validated_github_host_profile profile{ aCandidate };
		return github_profile_result{ profile };
	}

	approved_github_redirect_target::approved_github_redirect_target(
		std::string aUrl)
		: Url{ std::move(aUrl) }
	{
	}

	auto approved_github_redirect_target::url() const noexcept -> std::string_view
	{
		return Url;
	}

	auto validate_github_redirect_target(
		const validated_github_host_profile& aProfile,
		const github_request_surface         aSurface,
		const std::string_view               aTarget) -> github_redirect_result
	{
		if (!valid_request_surface(aSurface)) {
			return std::unexpected{
				github_redirect_failure::invalid_surface
			};
		}
		if (aSurface == github_request_surface::graphql) {
			return std::unexpected{
				github_redirect_failure::redirects_not_permitted
			};
		}
		if (aTarget.size() > maximum_redirect_target_bytes) {
			return std::unexpected{
				github_redirect_failure::target_too_long
			};
		}
		if (!aTarget.starts_with(https_prefix)) {
			return std::unexpected{ github_redirect_failure::invalid_target };
		}

		const auto authorityBegin = https_prefix.size();
		const auto pathBegin      = aTarget.find('/', authorityBegin);
		const auto authority      = aTarget.substr(
			authorityBegin,
			pathBegin == std::string_view::npos
				? aTarget.size() - authorityBegin
				: pathBegin - authorityBegin);
		if (
			authority.empty() || authority.find('@') != std::string_view::npos
			|| authority.find(':') != std::string_view::npos
			|| authority.find('%') != std::string_view::npos
			|| authority.find('[') != std::string_view::npos
			|| authority.find(']') != std::string_view::npos) {
			return std::unexpected{ github_redirect_failure::invalid_target };
		}
		if (authority != aProfile.rest_authority()) {
			return std::unexpected{
				github_redirect_failure::authority_mismatch
			};
		}

		const auto path = pathBegin == std::string_view::npos
			? std::string_view{ "/" }
			: aTarget.substr(pathBegin);
		if (!canonical_redirect_path(path)) {
			return std::unexpected{ github_redirect_failure::invalid_target };
		}
		if (!within_namespace(path, aProfile.rest_route_prefix())) {
			return std::unexpected{
				github_redirect_failure::outside_api_namespace
			};
		}
		const approved_github_redirect_target approved{ std::string{ aTarget } };
		return github_redirect_result{ approved };
	}
}
