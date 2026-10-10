#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_HOST_PROFILE_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_HOST_PROFILE_HPP

#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/github_model.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace bpr::adapters::github {
	enum class github_profile_provenance : std::uint8_t {
		explicit_user,
		builtin,
		managed,
		provider_content,
		redirect,
	};

	struct github_host_profile_candidate
	{
		core::host_profile_id                        Profile;
		core::git_host_id                            Host;
		application::github_profile_revision         Revision;
		application::github_server_contract          Server         = application::github_server_contract::github_com_2022_11_28;
		application::github_authentication_transport Authentication = application::github_authentication_transport::github_cli;
		application::github_proxy_mode               Proxy          = application::github_proxy_mode::system;
		application::github_trust_mode               Trust          = application::github_trust_mode::system;
		github_profile_provenance                    Provenance     = github_profile_provenance::explicit_user;
		std::string                                  Hostname;
		std::string                                  RestRoot;
		std::string                                  GraphqlRoot;
		/// [[BPR-1373]]. The `gh` account a GitHub CLI profile is pinned to;
		/// empty for an unpinned CLI profile and for every other transport.
		std::string CliAccountLogin;

		auto operator==(const github_host_profile_candidate&) const -> bool = default;
	};

	/// \brief Whether \p aLogin may name a GitHub CLI profile's account. [[BPR-1373]]
	/// \details One to 64 ASCII letters, digits, `-` or `_`, beginning with a
	/// letter or digit: GitHub's login alphabet (GHES managed users add `_`).
	/// Nothing else is ever written into a private `gh` configuration.
	[[nodiscard]] auto valid_cli_account_login(std::string_view aLogin) noexcept -> bool;

	enum class github_profile_failure : std::uint8_t {
		invalid_profile_identity,
		invalid_host_identity,
		invalid_profile_revision,
		invalid_server_contract,
		invalid_authentication_transport,
		invalid_proxy_mode,
		invalid_trust_mode,
		invalid_provenance,
		untrusted_provenance,
		invalid_hostname,
		invalid_rest_root,
		invalid_graphql_root,
		invalid_cli_account,
	};

	class validated_github_host_profile final
	{
	public:
		validated_github_host_profile(const validated_github_host_profile&) = default;
		validated_github_host_profile(validated_github_host_profile&&)      = delete;
		auto operator=(const validated_github_host_profile&)
			-> validated_github_host_profile& = default;
		auto operator=(validated_github_host_profile&&)
			-> validated_github_host_profile& = delete;

		[[nodiscard]] auto profile() const noexcept -> core::host_profile_id;
		[[nodiscard]] auto host() const noexcept -> core::git_host_id;
		[[nodiscard]] auto revision() const noexcept
			-> application::github_profile_revision;
		[[nodiscard]] auto server_contract() const noexcept
			-> application::github_server_contract;
		[[nodiscard]] auto product_kind() const noexcept
			-> application::github_product_kind;
		[[nodiscard]] auto authentication_transport() const noexcept
			-> application::github_authentication_transport;
		[[nodiscard]] auto proxy_mode() const noexcept
			-> application::github_proxy_mode;
		[[nodiscard]] auto trust_mode() const noexcept
			-> application::github_trust_mode;
		[[nodiscard]] auto provenance() const noexcept
			-> github_profile_provenance;
		[[nodiscard]] auto hostname() const noexcept -> std::string_view;
		[[nodiscard]] auto rest_root() const noexcept -> std::string_view;
		[[nodiscard]] auto graphql_root() const noexcept -> std::string_view;
		[[nodiscard]] auto rest_authority() const noexcept -> std::string_view;
		[[nodiscard]] auto rest_route_prefix() const noexcept -> std::string_view;
		[[nodiscard]] auto graphql_route() const noexcept -> std::string_view;
		[[nodiscard]] auto rest_api_version() const noexcept -> std::string_view;
		/// \brief [[BPR-1373]]. The pinned `gh` login; empty when unpinned.
		[[nodiscard]] auto cli_account_login() const noexcept -> std::string_view;

	private:
		explicit validated_github_host_profile(
			const github_host_profile_candidate& aCandidate);

		core::host_profile_id                        Profile;
		core::git_host_id                            Host;
		application::github_profile_revision         Revision;
		application::github_server_contract          Server;
		application::github_authentication_transport Authentication;
		application::github_proxy_mode               Proxy;
		application::github_trust_mode               Trust;
		github_profile_provenance                    Provenance;
		std::string                                  Hostname;
		std::string                                  RestRoot;
		std::string                                  GraphqlRoot;
		std::string                                  CliAccountLogin;

		friend auto validate_github_host_profile(
			const github_host_profile_candidate& aCandidate)
			-> std::expected<validated_github_host_profile, github_profile_failure>;
	};

	using github_profile_result = std::expected<validated_github_host_profile, github_profile_failure>;

	[[nodiscard]] auto validate_github_host_profile(
		const github_host_profile_candidate& aCandidate) -> github_profile_result;

	enum class github_request_surface : std::uint8_t {
		rest,
		graphql,
	};

	enum class github_redirect_failure : std::uint8_t {
		invalid_surface,
		redirects_not_permitted,
		target_too_long,
		invalid_target,
		authority_mismatch,
		outside_api_namespace,
	};

	class approved_github_redirect_target final
	{
	public:
		approved_github_redirect_target(
			const approved_github_redirect_target&) = default;
		approved_github_redirect_target(
			approved_github_redirect_target&&) = delete;
		auto operator=(const approved_github_redirect_target&)
			-> approved_github_redirect_target& = default;
		auto operator=(approved_github_redirect_target&&)
			-> approved_github_redirect_target& = delete;

		[[nodiscard]] auto url() const noexcept -> std::string_view;

	private:
		explicit approved_github_redirect_target(std::string aUrl);

		std::string Url;

		friend auto validate_github_redirect_target(
			const validated_github_host_profile& aProfile,
			github_request_surface               aSurface,
			std::string_view                     aTarget)
			-> std::expected<
				approved_github_redirect_target,
				github_redirect_failure>;
	};

	using github_redirect_result = std::expected<
		approved_github_redirect_target,
		github_redirect_failure>;

	[[nodiscard]] auto validate_github_redirect_target(
		const validated_github_host_profile& aProfile,
		github_request_surface               aSurface,
		std::string_view                     aTarget) -> github_redirect_result;
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_HOST_PROFILE_HPP
