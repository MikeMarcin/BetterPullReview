#include "github_cli_runtime.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "gh_cli_git_smart_transport.hpp"
#include "github_cli_process_runner.hpp"
#include <QFileInfo>
#include <QStandardPaths>
namespace bpr::app {
#if defined(__APPLE__)
	using adapters::github::validated_github_host_profile;
	using adapters::github::detail::github_wire_transport_factory_port;
	using adapters::github::detail::native_github_wire_transport_factory;
	[[nodiscard]] auto native_gh_executable() -> std::filesystem::path
	{
		auto executable = QStandardPaths::findExecutable(QStringLiteral("gh"));
		if (executable.isEmpty()) {
			executable = QStandardPaths::findExecutable(
				QStringLiteral("gh"),
				{
					QStringLiteral("/opt/homebrew/bin"),
					QStringLiteral("/usr/local/bin"),
					QStringLiteral("/opt/local/bin"),
				});
		}
		return native_filesystem_path(QFileInfo{ executable }.canonicalFilePath());
	}

	[[nodiscard]] auto live_gh_cli_configuration()
		-> std::optional<adapters::github::detail::validated_gh_cli_configuration>
	{
		using namespace adapters::github::detail;
		const auto executable = native_gh_executable();
		const auto candidate  = gh_cli_configuration_candidate{
			.Executable              = executable,
			.NeutralWorkingDirectory = native_filesystem_path(QDir::tempPath()),
			.Platform                = gh_cli_macos_environment_facts{
				.HomeDirectory = native_filesystem_path(QDir::homePath()),
			},
			.Https = { },
		};
		const auto configuration = validate_gh_cli_configuration(candidate);
		return configuration ? std::optional{ *configuration } : std::nullopt;
	}

	[[nodiscard]] auto probe_supported_gh_cli_version(
		adapters::github::detail::github_cli_process_port&              aProcess,
		const adapters::github::detail::validated_gh_cli_configuration& aConfiguration)
		-> std::expected<void, gh_cli_version_probe_failure>
	{
		using namespace adapters::github::detail;
		const auto version = aProcess.run(
			compile_gh_cli_version_request(aConfiguration), std::stop_token{ });
		if (!version || version->ExitCode != 0) {
			return std::unexpected{ gh_cli_version_probe_failure::process_failed };
		}
		if (!supported_gh_cli_auth_status_version(version->StandardOutput)) {
			return std::unexpected{ gh_cli_version_probe_failure::unsupported_version };
		}
		return { };
	}

	class owned_cli_read_transport final
		: public adapters::github::detail::github_wire_transport_port,
		  public adapters::github::detail::github_mutation_wire_transport,
		  public application::git_smart_transport_factory_port,
		  public std::enable_shared_from_this<owned_cli_read_transport>
	{
	public:
		owned_cli_read_transport(
			const validated_github_host_profile&                               aProfile,
			const adapters::github::detail::validated_gh_cli_configuration&    aConfiguration,
			std::unique_ptr<adapters::github::detail::github_cli_process_port> aProcess,
			core::github_session_request_context aSession, std::filesystem::path aGit)
			: Process{ std::move(aProcess) }
			, Transport{ aProfile, aConfiguration, *Process }
			, Profile{ aProfile }
			, Configuration{ aConfiguration }
			, Session{ aSession }
			, Git{ std::move(aGit) }
		{
		}

		auto acquire(const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<application::git_smart_transport> override
		{
			return adapters::github::detail::make_gh_cli_git_smart_transport(Profile, Configuration, Git, Session, aContext,
																			 [self = shared_from_this()] { return self->Transport.configuration_unchanged(); });
		}

		[[nodiscard]] auto supports_sparse_commit_trees() const noexcept -> bool override
		{
			return true;
		}

		[[nodiscard]] auto perform(
			const adapters::github::detail::github_wire_request& aRequest,
			std::stop_token                                      aStop)
			-> adapters::github::detail::github_wire_completion override
		{
			return Transport.perform(aRequest, aStop);
		}

		[[nodiscard]] auto perform_capability_probe(
			const adapters::github::detail::github_capability_probe_wire_request& aRequest,
			std::stop_token                                                       aStop)
			-> adapters::github::detail::github_capability_probe_wire_completion override
		{
			return Transport.perform_capability_probe(aRequest, aStop);
		}

		[[nodiscard]] auto perform_review_mutation(
			const adapters::github::detail::github_mutation_wire_request&  aRequest,
			std::stop_token                                                aStop,
			adapters::github::detail::github_mutation_transmission_witness aWitness)
			-> adapters::github::detail::github_mutation_wire_completion override
		{
			return Transport.perform_review_mutation(aRequest, aStop, std::move(aWitness));
		}

	private:
		std::unique_ptr<adapters::github::detail::github_cli_process_port> Process;
		adapters::github::detail::gh_cli_transport                         Transport;
		validated_github_host_profile                                      Profile;
		adapters::github::detail::validated_gh_cli_configuration           Configuration;
		core::github_session_request_context                               Session;
		std::filesystem::path                                              Git;
	};

	class opted_in_cli_transport_factory final
		: public github_wire_transport_factory_port
	{
	public:
		[[nodiscard]] auto make_bootstrap(
			const validated_github_host_profile&    aProfile,
			core::github_session_request_context    aContext,
			application::github_credential_material aCredential) noexcept
			-> adapters::github::detail::github_wire_transport_factory_result override
		{
			return Native.make_bootstrap(aProfile, aContext, std::move(aCredential));
		}

		[[nodiscard]] auto make_account(
			const validated_github_host_profile&            aProfile,
			application::github_credential_retrieve_success aCredential) noexcept
			-> adapters::github::detail::github_wire_transport_factory_result override
		{
			return Native.make_account(aProfile, std::move(aCredential));
		}

		[[nodiscard]] auto make_anonymous(
			const validated_github_host_profile& aProfile,
			core::github_session_request_context aContext) noexcept
			-> adapters::github::detail::github_wire_transport_factory_result override
		{
			return Native.make_anonymous(aProfile, aContext);
		}

		[[nodiscard]] auto supports_cli() const noexcept -> bool override
		{
			return true;
		}

		[[nodiscard]] auto make_cli(
			const validated_github_host_profile& aProfile,
			core::github_session_request_context aContext) noexcept
			-> adapters::github::detail::github_wire_transport_factory_result override
		{
			using namespace adapters::github::detail;
			static_cast<void>(aContext);
			const auto unavailable = []() -> github_wire_transport_factory_result {
				return std::unexpected{ application::github_provider_error{
					.Failure           = application::github_provider_failure::cli_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				} };
			};
			if (aProfile.authentication_transport()
				!= application::github_authentication_transport::github_cli) {
				return unavailable();
			}
			try {
				const auto configuration = live_gh_cli_configuration();
				if (!configuration) {
					return unavailable();
				}
				auto process = make_github_cli_process_runner();
				if (!process
					|| !probe_supported_gh_cli_version(*process, *configuration)) {
					return unavailable();
				}
				return std::shared_ptr<github_wire_transport_port>{
					std::make_shared<owned_cli_read_transport>(aProfile, *configuration,
															   std::move(process), aContext,
															   native_filesystem_path(QFileInfo{ QStandardPaths::findExecutable(QStringLiteral("git")) }.canonicalFilePath()))
				};
			} catch (...) {
				return unavailable();
			}
		}

	private:
		native_github_wire_transport_factory Native;
	};
	std::shared_ptr<adapters::github::detail::github_wire_transport_factory_port> make_opted_in_cli_transport_factory() { return std::make_shared<opted_in_cli_transport_factory>(); }
#endif
}
