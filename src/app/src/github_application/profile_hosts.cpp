#include "github_application/profile_hosts.hpp"

#include "bpr/app/native_filesystem_path.hpp"

#include <QDesktopServices>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

#include <expected>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
#endif

namespace bpr::app {
	auto native_application_data_root(
		const github_application_config& aConfig) -> std::filesystem::path
	{
		if (aConfig.ApplicationDataRoot) {
			return aConfig.ApplicationDataRoot->lexically_normal();
		}
		return native_filesystem_path(QStandardPaths::writableLocation(
			QStandardPaths::AppLocalDataLocation));
	}

	auto native_git_executable(
		const github_application_config& aConfig) -> std::filesystem::path
	{
		if (aConfig.GitExecutable) {
			return aConfig.GitExecutable->lexically_normal();
		}
		return native_filesystem_path(
			QFileInfo{ QStandardPaths::findExecutable(QStringLiteral("git")) }
				.canonicalFilePath());
	}

	auto clone_without_smart_git(
		const application::accepted_pull_request_handoff& aHandoff)
		-> application::accepted_pull_request_handoff
	{
		return {
			.ApplicationSession    = aHandoff.ApplicationSession,
			.Profile               = aHandoff.Profile,
			.ProfileRevision       = aHandoff.ProfileRevision,
			.Host                  = aHandoff.Host,
			.HostAuthority         = aHandoff.HostAuthority,
			.Hostname              = aHandoff.Hostname,
			.ServerContract        = aHandoff.ServerContract,
			.Account               = aHandoff.Account,
			.Viewer                = aHandoff.Viewer,
			.Repository            = aHandoff.Repository,
			.Locator               = aHandoff.Locator,
			.PullRequest           = aHandoff.PullRequest,
			.Number                = aHandoff.Number,
			.Snapshot              = aHandoff.Snapshot,
			.IdentityGeneration    = aHandoff.IdentityGeneration,
			.PullRequestGeneration = aHandoff.PullRequestGeneration,
			.Origin                = aHandoff.Origin,
			.ObjectFormat          = aHandoff.ObjectFormat,
			.BaseBranch            = aHandoff.BaseBranch,
			.BaseRef               = aHandoff.BaseRef,
			.Base                  = aHandoff.Base,
			.Head                  = aHandoff.Head,
			.CurrentViewerReview   = aHandoff.CurrentViewerReview,
			.RecordedBase          = aHandoff.RecordedBase,
			.BaseBasis             = aHandoff.BaseBasis,
		};
	}

	auto windows_system_root()
		-> std::optional<std::filesystem::path>
	{
#if defined(_WIN32)
		auto       value  = std::array<wchar_t, MAX_PATH + 1U>{ };
		const auto length = GetWindowsDirectoryW(
			value.data(), static_cast<UINT>(value.size()));
		if (length == 0U || length >= value.size()) {
			return std::nullopt;
		}
		// The API spells it `C:\WINDOWS` while the directory is
		// `C:\Windows`, and the adapter accepts a system root only in its
		// canonical spelling; the uncanonical one made every Git process
		// refuse its configuration, which the epoch pipeline reported as
		// `failed` with nothing to name (BPR-0441).
		std::error_code error;
		auto            canonical = std::filesystem::canonical(
			std::filesystem::path{ value.data() }, error);
		if (error) {
			return std::nullopt;
		}
		return canonical;
#else
		return std::nullopt;
#endif
	}

	unavailable_credential_vault::unavailable_credential_vault(
		application::effect_executor& aEffects)
		: Effects{ aEffects }
	{
	}

	void unavailable_credential_vault::store(
		application::github_credential_enrollment               aEnrollment,
		application::github_credential_store_completion_handler aCompletion)

	{
		const auto context = aEnrollment.Context;
		auto       pending = std::make_shared<
			application::github_credential_enrollment>(
			std::move(aEnrollment));
		Effects.post(
			[pendingEnrollment = std::move(pending),
			 context,
			 completion = std::move(aCompletion)]() mutable {
				pendingEnrollment->Credential.reset();
				completion(application::github_credential_store_error{
					.Context = context,
					.Failure = application::github_credential_vault_failure::
						unavailable,
				});
			});
	}

	void unavailable_credential_vault::retrieve(
		const application::github_credential_operation_context     aContext,
		application::github_credential_retrieve_completion_handler aCompletion)

	{
		Effects.post(
			[aContext, completion = std::move(aCompletion)]() mutable {
				completion(application::github_credential_retrieve_error{
					.Context = aContext,
					.Failure = application::github_credential_vault_failure::
						unavailable,
				});
			});
	}

	void unavailable_credential_vault::delete_credential(
		const application::github_credential_operation_context   aContext,
		application::github_credential_delete_completion_handler aCompletion)

	{
		Effects.post(
			[aContext, completion = std::move(aCompletion)]() mutable {
				completion(application::github_credential_delete_error{
					.Context = aContext,
					.Failure = application::github_credential_vault_failure::
						unavailable,
				});
			});
	}

	void unavailable_credential_vault::store_refresh(
		application::github_credential_enrollment               aEnrollment,
		application::github_credential_store_completion_handler aCompletion)

	{
		store(std::move(aEnrollment), std::move(aCompletion));
	}

	void unavailable_credential_vault::retrieve_refresh(
		const application::github_credential_operation_context     aContext,
		application::github_credential_retrieve_completion_handler aCompletion)

	{
		retrieve(aContext, std::move(aCompletion));
	}

	void unavailable_credential_vault::delete_refresh(
		const application::github_credential_operation_context   aContext,
		application::github_credential_delete_completion_handler aCompletion)

	{
		delete_credential(aContext, std::move(aCompletion));
	}

	void unavailable_credential_vault::cancel(
		const application::github_credential_operation_context)
	{
	}

	auto native_profile_candidate(
		const core::host_profile_id                        aProfile,
		const core::git_host_id                            aHost,
		const application::github_profile_revision         aRevision,
		const application::github_server_contract          aServer,
		const application::github_authentication_transport aAuthentication,
		std::string                                        aHostname,
		const adapters::github::github_profile_provenance  aProvenance)
		-> adapters::github::github_host_profile_candidate
	{
		const auto cloud = aServer
			== application::github_server_contract::github_com_2022_11_28;
		auto restRoot    = cloud ? std::string{ "https://api.github.com" }
								 : std::string{ "https://" } + aHostname + "/api/v3";
		auto graphqlRoot = cloud ? std::string{ "https://api.github.com/graphql" }
								 : std::string{ "https://" } + aHostname + "/api/graphql";
		return {
			.Profile        = aProfile,
			.Host           = aHost,
			.Revision       = aRevision,
			.Server         = aServer,
			.Authentication = aAuthentication,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = aProvenance,
			.Hostname       = std::move(aHostname),
			.RestRoot       = std::move(restRoot),
			.GraphqlRoot    = std::move(graphqlRoot),
		};
	}

	auto profile_option(
		const adapters::github::validated_github_host_profile& aProfile,
		std::string                                            aLabel) -> application::github_profile_option
	{
		return {
			.Profile   = aProfile.profile(),
			.Label     = std::move(aLabel),
			.Hostname  = std::string{ aProfile.hostname() },
			.Product   = aProfile.product_kind()
					== application::github_product_kind::github_com
				? application::github_profile_product::github_com
				: application::github_profile_product::
					  github_enterprise_server,
			.Transport = aProfile.authentication_transport()
					== application::github_authentication_transport::
						github_cli
				? application::github_profile_transport::github_cli
				: application::github_profile_transport::native_https,
		};
	}

	void append_profile(
		std::deque<adapters::github::validated_github_host_profile>& aProfiles,
		std::vector<application::github_profile_option>&             aOptions,
		const adapters::github::github_host_profile_candidate&       aCandidate,
		std::string                                                  aLabel)
	{
		const auto profile = adapters::github::validate_github_host_profile(aCandidate);
		if (!profile.has_value()) {
			throw std::invalid_argument{ "Invalid GitHub host profile" };
		}
		aProfiles.push_back(*profile);
		aOptions.push_back(profile_option(*profile, std::move(aLabel)));
	}
#if defined(BPR_BUILD_TESTING)
	auto denied_loopback_network() noexcept
		-> adapters::github::detail::native_https_test_configuration
	{
		return { .ProxyHost = "127.0.0.1", .ProxyPort = 1U };
	}
#endif
}
