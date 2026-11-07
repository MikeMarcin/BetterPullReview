#ifndef BPR_APP_GITHUB_APPLICATION_PROFILE_HOSTS_HPP
#define BPR_APP_GITHUB_APPLICATION_PROFILE_HOSTS_HPP
#include "bpr/app/github_application.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/github_model.hpp"
#include "github_host_profile.hpp"

#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/core/model.hpp"
	#include "github_application_fixture_access.hpp"
	#include "native_https_transport.hpp"
	#include <string_view>
#endif

// The host profiles a profile runtime offers: native and fixture GitHub
// profiles, their platform roots and the unavailable vault.
namespace bpr::app {
	[[nodiscard]] auto native_application_data_root(
		const github_application_config& aConfig) -> std::filesystem::path;

	[[nodiscard]] auto native_git_executable(
		const github_application_config& aConfig) -> std::filesystem::path;
#if defined(BPR_BUILD_TESTING)
	/// \brief Names the host profile one public-path fixture is served on.
	struct fixture_host_profile
	{
		core::host_profile_id               Profile;
		core::git_host_id                   Host;
		application::github_server_contract Server;
		std::string_view                    Hostname;
		std::string_view                    Label;
	};

	/// \brief Selects the host profile the named fixture is served on.
	///
	/// \details A switch rather than an enterprise-or-not test, because
	/// `-Wswitch` catches a switch that has not been told about a new fixture
	/// and cannot catch an `if` that quietly answers "the other one".
	[[nodiscard]] constexpr auto fixture_host(
		const detail::github_public_path_fixture aFixture) noexcept
		-> fixture_host_profile
	{
		switch (aFixture) {
		case detail::github_public_path_fixture::github_com:
		case detail::github_public_path_fixture::large_review_v1:
		case detail::github_public_path_fixture::profile_window_restoration_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Recorded native HTTPS",
			};
		case detail::github_public_path_fixture::ghes_3_18:
			return {
				.Profile = core::host_profile_id{ 3U },
				.Host    = core::git_host_id{ 2U },
				.Server  = application::github_server_contract::
					ghes_3_18_2022_11_28,
				.Hostname = "ghe.example.invalid",
				.Label    = "GHES 3.18 — Recorded native HTTPS",
			};
		// The recorded pull request lives on github.com, so it is served on
		// the same host profile the synthetic github.com fixture uses. Only
		// the label differs, and the label is not what the active-host text
		// is built from - that is the hostname and the product.
		case detail::github_public_path_fixture::uiforetw_74:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Recorded google/UIforETW#74",
			};
		case detail::github_public_path_fixture::cpp_identifiers_v1:
		case detail::github_public_path_fixture::header_source_v1:
		case detail::github_public_path_fixture::source_find_v1:
		case detail::github_public_path_fixture::source_pr_search_v1:
		case detail::github_public_path_fixture::source_pr_search_large_v1:
		case detail::github_public_path_fixture::nested_folders_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed nested folders",
			};
		// [[BPR-1334]]. Served like nested-folders: github.com, one head.
		case detail::github_public_path_fixture::line_details_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed long lines",
			};
		// [[BPR-1537]]. Served like nested-folders: github.com, one head.
		case detail::github_public_path_fixture::intraline_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed intraline changes",
			};
		// [[BPR-1560]], [[BPR-1572]]. Served like nested-folders: github.com,
		// one head.
		case detail::github_public_path_fixture::merge_base_v1:
		case detail::github_public_path_fixture::stacked_merge_base_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed merge base",
			};
		// RFC-BPR-0089. Served like nested-folders: github.com, one head.
		case detail::github_public_path_fixture::commit_filter_v1:
		case detail::github_public_path_fixture::commit_filter_attributes_v1:
		case detail::github_public_path_fixture::commit_filter_split_v1:
		case detail::github_public_path_fixture::commit_filter_comment_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed commit filter",
			};
		case detail::github_public_path_fixture::conventional_comments_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed Conventional Comments",
			};
		case detail::github_public_path_fixture::inline_drawer_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed inline drawer",
			};
		case detail::github_public_path_fixture::insights_budget_v1:
		case detail::github_public_path_fixture::overview_labels_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed Overview labels",
			};
		case detail::github_public_path_fixture::markdown_images_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed inline images",
			};
		case detail::github_public_path_fixture::markdown_parity_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed Markdown parity",
			};
		case detail::github_public_path_fixture::markdown_html_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed Markdown HTML",
			};
		case detail::github_public_path_fixture::status_attention_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed Status attention",
			};
		case detail::github_public_path_fixture::markdown_code_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed fenced code",
			};
		case detail::github_public_path_fixture::conversations_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed conversations",
			};
		case detail::github_public_path_fixture::promo_v1:
			return {
				.Profile = core::host_profile_id{ 1U },
				.Host    = core::git_host_id{ 1U },
				.Server  = application::github_server_contract::
					github_com_2022_11_28,
				.Hostname = "github.com",
				.Label    = "GitHub.com — Constructed promo",
			};
		}
		return { };
	}
#endif
	[[nodiscard]] auto clone_without_smart_git(
		const application::accepted_pull_request_handoff& aHandoff)
		-> application::accepted_pull_request_handoff;

	[[nodiscard]] auto windows_system_root()
		-> std::optional<std::filesystem::path>;

	class unavailable_credential_vault final
		: public application::github_credential_vault_port
	{
	public:
		explicit unavailable_credential_vault(
			application::effect_executor& aEffects);

		void store(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override;

		void retrieve(
			const application::github_credential_operation_context     aContext,
			application::github_credential_retrieve_completion_handler aCompletion)
			override;

		void delete_credential(
			const application::github_credential_operation_context   aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override;

		void store_refresh(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override;

		void retrieve_refresh(
			const application::github_credential_operation_context     aContext,
			application::github_credential_retrieve_completion_handler aCompletion)
			override;

		void delete_refresh(
			const application::github_credential_operation_context   aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override;

		void cancel(
			const application::github_credential_operation_context) override;

	private:
		application::effect_executor& Effects;
	};

	[[nodiscard]] auto native_profile_candidate(
		const core::host_profile_id                        aProfile,
		const core::git_host_id                            aHost,
		const application::github_profile_revision         aRevision,
		const application::github_server_contract          aServer,
		const application::github_authentication_transport aAuthentication,
		std::string                                        aHostname,
		const adapters::github::github_profile_provenance  aProvenance)
		-> adapters::github::github_host_profile_candidate;

	[[nodiscard]] auto profile_option(
		const adapters::github::validated_github_host_profile& aProfile,
		std::string                                            aLabel) -> application::github_profile_option;

	void append_profile(
		std::deque<adapters::github::validated_github_host_profile>& aProfiles,
		std::vector<application::github_profile_option>&             aOptions,
		const adapters::github::github_host_profile_candidate&       aCandidate,
		std::string                                                  aLabel);
#if defined(BPR_BUILD_TESTING)
	// [[BPR-0515]]. The one local denial the dev-tree gate composes.
	//
	// `application_device_authorization_unavailable` drives the shipped
	// device selection and needs the connect to fail on the host, not at
	// GitHub. macOS and Linux got that from the harness - sandbox-exec and
	// a loopback-only network namespace - but [[BPR-0362]] exempts this
	// test from the Windows AppContainer, so on Windows nothing stood in
	// front of the network and the real flow reached `awaiting_authorization`
	// against github.com once per run. Denying at the socket sidesteps the
	// token the container could not grant: the wire is composed against a
	// closed loopback port instead of the discovered system proxy, the
	// connect is refused immediately, and all three platforms land on the
	// same `failed` terminal without the expectation depending on what the
	// host can reach.
	//
	// Port 1 is the denial. It is reserved, never bound by anything this
	// application would talk to, and `proxy_candidates` under this policy
	// offers it alone - no direct candidate and no fallback port - so there
	// is no route by which a request could still leave the machine.
	[[nodiscard]] auto denied_loopback_network() noexcept
		-> adapters::github::detail::native_https_test_configuration;
#endif
}
#endif
