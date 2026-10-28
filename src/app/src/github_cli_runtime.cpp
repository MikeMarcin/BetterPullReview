#include "github_cli_runtime.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "gh_cli_git_smart_transport.hpp"
#include "github_cli_process_runner.hpp"
#include <QFileInfo>
#include <QStandardPaths>

#include <array>
#include <mutex>

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <ShlObj.h>
	#include <Windows.h>
#endif
namespace bpr::app {
#if defined(BPR_GITHUB_CLI_PLATFORM)
	using adapters::github::validated_github_host_profile;
	using adapters::github::detail::github_wire_transport_factory_port;
	using adapters::github::detail::native_github_wire_transport_factory;
	namespace {
		[[nodiscard]] auto canonical_executable(const QString& aFound) -> std::filesystem::path
		{
			return aFound.isEmpty() ? std::filesystem::path{ } : native_filesystem_path(QFileInfo{ aFound }.canonicalFilePath());
		}

	#if defined(_WIN32)
		/// [[BPR-1791]]. A Windows known folder as the shell resolves it, or
		/// empty when it has none.
		[[nodiscard]] auto known_folder(REFKNOWNFOLDERID aFolder) -> std::filesystem::path
		{
			PWSTR      raw    = nullptr;
			const auto result = ::SHGetKnownFolderPath(aFolder, KF_FLAG_DEFAULT, nullptr, &raw);
			auto       path   = std::filesystem::path{ };
			if (SUCCEEDED(result) && raw != nullptr) {
				path = std::filesystem::path{ raw };
			}
			::CoTaskMemFree(raw);
			return path;
		}
	#endif

	#if !defined(__APPLE__) && !defined(_WIN32)
		/// An absolute directory the session names, or empty. Discovery only:
		/// the child's environment is built by the transport.
		[[nodiscard]] auto environment_directory(const char* aName) -> QString
		{
			const auto value = qEnvironmentVariable(aName);
			return QDir::isAbsolutePath(value) ? QDir::cleanPath(value) : QString{ };
		}
	#endif

		/// [[BPR-1791]]. The facts `gh`'s environment is built from: the home
		/// directory, on Windows the shell's known folders, and on Linux the
		/// session's runtime directory and bus. The transport validates them.
		[[nodiscard]] auto platform_environment_facts() -> adapters::github::detail::gh_cli_platform_environment_facts
		{
			using namespace adapters::github::detail;
	#if defined(__APPLE__)
			return gh_cli_macos_environment_facts{ .HomeDirectory = native_filesystem_path(QDir::homePath()) };
	#elif defined(_WIN32)
			return gh_cli_windows_environment_facts{
				.UserProfileDirectory            = known_folder(FOLDERID_Profile),
				.RoamingApplicationDataDirectory = known_folder(FOLDERID_RoamingAppData),
				.LocalApplicationDataDirectory   = known_folder(FOLDERID_LocalAppData),
				.SystemRootDirectory             = known_folder(FOLDERID_Windows),
			};
	#else
			// The session's runtime directory is the session's own fact, and
			// gh reaches Secret Service through the session bus socket in it,
			// where systemd's user session puts it. A session without one
			// fails configuration and is reported, rather than gh waiting on
			// a keyring it cannot reach.
			const auto runtime = environment_directory("XDG_RUNTIME_DIR");
			auto       bus     = std::string{ };
			if (!runtime.isEmpty() && QFileInfo{ runtime + QStringLiteral("/bus") }.exists()) {
				const auto address = (QStringLiteral("unix:path=") + runtime + QStringLiteral("/bus")).toUtf8();
				bus.assign(address.constData(), static_cast<std::size_t>(address.size()));
			}
			return gh_cli_linux_environment_facts{
				.HomeDirectory     = native_filesystem_path(QDir::homePath()),
				.RuntimeDirectory  = native_filesystem_path(runtime),
				.SessionBusAddress = std::move(bus),
			};
	#endif
		}
	}

	/// [[BPR-1791]]. `PATH` first, then the platform's standard install
	/// locations, each canonicalised; the first that is really `gh` wins.
	[[nodiscard]] auto native_gh_executable() -> std::filesystem::path
	{
		using namespace adapters::github::detail;
		constexpr auto platform = *current_gh_cli_host_platform();
		auto           facts    = gh_cli_install_location_facts{ .HomeDirectory = native_filesystem_path(QDir::homePath()) };
	#if defined(_WIN32)
		facts.ProgramFilesDirectory         = known_folder(FOLDERID_ProgramFiles);
		facts.LocalApplicationDataDirectory = known_folder(FOLDERID_LocalAppData);
	#endif
		const auto name       = QStringLiteral("gh");
		auto       candidates = std::vector<std::filesystem::path>{ canonical_executable(QStandardPaths::findExecutable(name)) };
		for (const auto& directory : gh_cli_standard_install_directories(platform, facts)) {
			const auto utf8 = directory.u8string();
			candidates.push_back(canonical_executable(QStandardPaths::findExecutable(
				name, { QString::fromUtf8(reinterpret_cast<const char*>(utf8.data()), static_cast<qsizetype>(utf8.size())) })));
		}
		return select_gh_cli_executable(platform, candidates);
	}

	[[nodiscard]] auto gh_cli_executable_available() -> bool
	{
		return !native_gh_executable().empty();
	}

	[[nodiscard]] auto live_gh_cli_configuration()
		-> std::optional<adapters::github::detail::validated_gh_cli_configuration>
	{
		using namespace adapters::github::detail;
		const auto executable = native_gh_executable();
		const auto candidate  = gh_cli_configuration_candidate{
			.Executable              = executable,
			.NeutralWorkingDirectory = native_filesystem_path(QDir::tempPath()),
			.Platform                = platform_environment_facts(),
			.Https                   = { },
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

		void set_insights_channel(std::shared_ptr<adapters::github::detail::github_insights_channel> aChannel) override
		{
			github_wire_transport_port::set_insights_channel(aChannel);
			Transport.set_insights_channel(aChannel);
			Process->set_insights_channel(aChannel);
			const std::scoped_lock lock{ BackgroundMutex };
			for (auto& lane : Background) {
				if (lane.Process) {
					lane.Transport->set_insights_channel(aChannel);
					lane.Process->set_insights_channel(aChannel);
				}
			}
		}
		[[nodiscard]] auto strict_cli() const noexcept -> bool override { return true; }
		[[nodiscard]] auto begin_operation(std::stop_token aStop) -> adapters::github::detail::github_wire_operation override { return Transport.begin_operation(aStop); }
		/// [[BPR-1680]]. Two background lanes: each its own gh runner, with its
		/// own serialized lane, quarantine and process-group kill, over the
		/// same pinned configuration. Home's discovery reads go there, so a
		/// pull request read never waits behind one.
		[[nodiscard]] auto background_lanes() const noexcept -> std::size_t override { return background_lane_count; }
		[[nodiscard]] auto begin_background_operation(const std::size_t aLane, std::stop_token aStop) -> adapters::github::detail::github_wire_operation override
		{
			auto* const transport = background_transport(aLane);
			if (transport == nullptr) {
				return std::unexpected{ application::github_provider_error{ .Failure = application::github_provider_failure::process_startup } };
			}
			auto operation = transport->begin_operation(aStop);
			if (!operation || *operation) {
				return operation;
			}
			// No scoped credential: the lane's own transport serves, kept
			// alive by this one.
			return std::shared_ptr<adapters::github::detail::github_wire_transport_port>{ shared_from_this(), transport };
		}
		[[nodiscard]] auto begin_mutation_operation(std::stop_token aStop) -> adapters::github::detail::github_mutation_operation override { return Transport.begin_mutation_operation(aStop); }

		auto acquire(const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<application::git_smart_transport> override
		{
			return adapters::github::detail::make_gh_cli_git_smart_transport(Profile, Configuration, Git, Session, aContext, [self = shared_from_this()] { return self->Transport.configuration_unchanged(); }, insights_channel());
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
		static constexpr std::size_t background_lane_count = 2U;

		/// The background lane's transport, made on first use.
		[[nodiscard]] auto background_transport(const std::size_t aLane) -> adapters::github::detail::gh_cli_transport*
		{
			if (aLane >= Background.size()) {
				return nullptr;
			}
			const std::scoped_lock lock{ BackgroundMutex };
			auto&                  lane = Background[aLane];
			if (!lane.Process) {
				try {
					auto process = adapters::github::detail::make_github_cli_process_runner();
					if (!process) {
						return nullptr;
					}
					lane.Transport = std::make_unique<adapters::github::detail::gh_cli_transport>(Profile, Configuration, *process);
					lane.Process   = std::move(process);
					if (insights_channel()) {
						lane.Transport->set_insights_channel(insights_channel());
						lane.Process->set_insights_channel(insights_channel());
					}
				} catch (...) {
					return nullptr;
				}
			}
			return lane.Transport.get();
		}

		struct background_lane
		{
			std::unique_ptr<adapters::github::detail::github_cli_process_port> Process;
			std::unique_ptr<adapters::github::detail::gh_cli_transport>        Transport;
		};

		std::unique_ptr<adapters::github::detail::github_cli_process_port> Process;
		adapters::github::detail::gh_cli_transport                         Transport;
		validated_github_host_profile                                      Profile;
		adapters::github::detail::validated_gh_cli_configuration           Configuration;
		core::github_session_request_context                               Session;
		std::filesystem::path                                              Git;
		std::mutex                                                         BackgroundMutex;
		// Destroyed first: no background runner outlives this transport.
		std::array<background_lane, background_lane_count> Background;
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
					!= application::github_authentication_transport::github_cli
				|| aProfile.cli_account_login().empty()) {
				return unavailable();
			}
			try {
				const auto live = live_gh_cli_configuration();
				// [[BPR-1373]]. A profile chosen for one gh account runs every gh
				// call through a private configuration naming only that account.
				const auto configuration = !live || aProfile.cli_account_login().empty()
					? live
					: pin_gh_cli_configuration(*live, aProfile.hostname(), aProfile.cli_account_login());
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
