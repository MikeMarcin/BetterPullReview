#include "bpr/app/github_application.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/github_conversation_model.hpp"

#include "bpr/app/worker_executor.hpp"

#include "github_host_profile.hpp"
#include "github_provider.hpp"

#if defined(__APPLE__)
	#include "macos_keychain.hpp"
#elif defined(_WIN32)
	#include "windows_credential_manager.hpp"
#elif defined(__linux__)
	#include "linux_secret_service.hpp"
#endif

#if defined(BPR_BUILD_TESTING)
	#include "github_public_path_fixture.hpp"
#endif

#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/defect_correlation.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/application/workbench_snapshot_observation.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/sensitive_text_input.hpp"
#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
	#include <windows.h>
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::app {
	namespace {
		using adapters::github::github_host_profile_candidate;
		using adapters::github::github_profile_provenance;
		using adapters::github::validate_github_host_profile;
		using adapters::github::validated_github_host_profile;
		using adapters::github::detail::github_provider;
		using adapters::github::detail::github_wire_transport_factory_port;
		using adapters::github::detail::make_github_provider;
		using adapters::github::detail::native_github_wire_transport_factory;

		[[nodiscard]] auto native_filesystem_path(const QString& aValue)
			-> std::filesystem::path
		{
#if defined(_WIN32)
			return std::filesystem::path{ aValue.toStdWString() };
#else
			return std::filesystem::path{ aValue.toStdString() };
#endif
		}

		struct local_defect_identity
		{
			std::uint64_t Conversation = 0U;
			std::uint64_t Defect       = 0U;

			auto operator<=>(const local_defect_identity&) const = default;
		};

		[[nodiscard]] auto durable_identity_half(
			const std::array<std::uint8_t, 32>& aDigest,
			const std::size_t                   aOffset) noexcept -> std::uint64_t
		{
			auto result = std::uint64_t{ 0U };
			for (auto index = std::size_t{ 0U }; index < 8U; ++index) {
				result = (result << 8U) | aDigest[aOffset + index];
			}
			result &= std::uint64_t{ 0x7fff'ffff'ffff'ffffULL };
			return result == 0U ? 1U : result;
		}

		[[nodiscard]] auto defect_identity(
			const std::string_view aIdentity) noexcept -> local_defect_identity
		{
			const auto bytes = std::span{
				reinterpret_cast<const std::uint8_t*>(aIdentity.data()),
				aIdentity.size(),
			};
			const auto digest = core::sha256(bytes);
			return {
				.Conversation = durable_identity_half(digest, 0U),
				.Defect       = durable_identity_half(digest, 8U),
			};
		}

		[[nodiscard]] auto native_application_data_root(
			const github_application_config& aConfig) -> std::filesystem::path
		{
			if (aConfig.ApplicationDataRoot) {
				return aConfig.ApplicationDataRoot->lexically_normal();
			}
			return native_filesystem_path(QStandardPaths::writableLocation(
				QStandardPaths::AppLocalDataLocation));
		}

		[[nodiscard]] auto native_git_executable(
			const github_application_config& aConfig) -> std::filesystem::path
		{
			if (aConfig.GitExecutable) {
				return aConfig.GitExecutable->lexically_normal();
			}
			return native_filesystem_path(
				QFileInfo{ QStandardPaths::findExecutable(QStringLiteral("git")) }
					.canonicalFilePath());
		}

		[[nodiscard]] auto queue_observation_for(
			const application::accepted_pull_request_handoff& aHandoff,
			const application::github_pull_request_snapshot&  aSnapshot)
			-> core::pull_request_observation
		{
			auto paths = std::vector<std::string>{ };
			paths.reserve(aSnapshot.Files.size());
			std::ranges::transform(
				aSnapshot.Files,
				std::back_inserter(paths),
				[](const application::github_file_record& aFile) {
					return aFile.Path;
				});
			return {
				.Ref = {
					.Host       = std::string{ aHandoff.Hostname.value() },
					.Account    = aSnapshot.Viewer.Login,
					.Repository = aSnapshot.Repository.Owner + "/"
						+ aSnapshot.Repository.Name,
					.Number = aSnapshot.PullRequest.Number.value(),
				},
				.Author = aSnapshot.PullRequest.Author.has_value() ? aSnapshot.PullRequest.Author->Login : std::string{ },
				// The accepted snapshot does not carry GitHub's explicit queue-role
				// authority. Review and comment history are evidence of activity, not
				// a substitute for an assignment supplied by the provider.
				.Role         = core::github_role::none,
				.ChangedPaths = std::move(paths),
			};
		}

		class composition_queue_refresh final
			: public application::queue_refresh_effect_port
		{
		public:
			void retain(
				application::queue_session_id     aSession,
				core::pull_request_observation    aObservation,
				application::queue_refresh_status aStatus
				= application::queue_refresh_status::changed)
			{
				const auto lock = std::scoped_lock{ Mutex };
				Retained        = application::queue_refresh_completion{
					.Session     = std::move(aSession),
					.Ref         = aObservation.Ref,
					.Status      = aStatus,
					.Observation = std::move(aObservation),
				};
			}

			void clear()
			{
				const auto lock = std::scoped_lock{ Mutex };
				Retained.reset();
			}

			auto refresh(
				const application::queue_refresh_request& aRequest,
				std::stop_token                           aStop) -> application::queue_refresh_completion override
			{
				if (aStop.stop_requested()) {
					return {
						.Session = aRequest.Session,
						.Ref     = aRequest.Ref,
						.Status  = application::queue_refresh_status::cancelled,
					};
				}
				const auto lock = std::scoped_lock{ Mutex };
				if (!Retained.has_value() || Retained->Session != aRequest.Session
					|| Retained->Ref != aRequest.Ref) {
					return {
						.Session = aRequest.Session,
						.Ref     = aRequest.Ref,
						.Status  = application::queue_refresh_status::cancelled,
					};
				}
				// This port replays an already accepted snapshot; it does not own a
				// provider ETag. A head OID is not a substitute because review and
				// comment activity can change while the head stays fixed.
				return *Retained;
			}

		private:
			std::mutex                                           Mutex;
			std::optional<application::queue_refresh_completion> Retained;
		};

		[[nodiscard]] auto preserves_selected_diff(
			const core::epoch_state&                          aState,
			const application::accepted_pull_request_handoff& aHandoff) noexcept
			-> bool
		{
			if (
				!aState.Authority.has_value()
				|| !aState.AcceptedRevision.has_value()
				|| !aState.AcceptedComparison.has_value()) {
				return false;
			}
			const auto& authority = *aState.Authority;
			const auto& revision  = *aState.AcceptedRevision;
			return aState.ApplicationSession == aHandoff.ApplicationSession
				&& authority.Profile == aHandoff.Profile
				&& authority.ProfileRevision == aHandoff.ProfileRevision
				&& authority.Host == aHandoff.Host
				&& authority.HostAuthority == aHandoff.HostAuthority
				&& authority.Account == aHandoff.Account
				&& authority.Viewer == aHandoff.Viewer
				&& authority.Repository == aHandoff.Repository
				&& authority.PullRequest == aHandoff.PullRequest
				&& authority.Number == aHandoff.Number
				&& revision.Source.IdentityGeneration
				== aHandoff.IdentityGeneration
				&& revision.Origin == aHandoff.Origin
				&& revision.BaseBranch == aHandoff.BaseBranch
				&& revision.Base == aHandoff.Base
				&& revision.Head == aHandoff.Head
				&& revision.CurrentViewerReview == aHandoff.CurrentViewerReview
				&& revision.Base.algorithm() == aHandoff.ObjectFormat;
		}

		[[nodiscard]] auto workbench_authority(
			const application::accepted_pull_request_handoff& aHandoff) noexcept
			-> application::workbench_pull_request_authority
		{
			return {
				.ApplicationSession    = aHandoff.ApplicationSession,
				.Profile               = aHandoff.Profile,
				.ProfileRevision       = aHandoff.ProfileRevision,
				.Host                  = aHandoff.Host,
				.HostAuthority         = aHandoff.HostAuthority,
				.Account               = aHandoff.Account,
				.Viewer                = aHandoff.Viewer,
				.Repository            = aHandoff.Repository,
				.PullRequest           = aHandoff.PullRequest,
				.Number                = aHandoff.Number,
				.Snapshot              = aHandoff.Snapshot,
				.IdentityGeneration    = aHandoff.IdentityGeneration,
				.PullRequestGeneration = aHandoff.PullRequestGeneration,
			};
		}

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
			}
			return { };
		}

		[[nodiscard]] auto clone_without_smart_git(
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
			};
		}
#endif

		[[nodiscard]] auto prepare_application_data_root(
			const std::filesystem::path& aRoot) -> std::filesystem::path
		{
			if (aRoot.empty() || !aRoot.is_absolute()) {
				return { };
			}
			std::error_code error;
			static_cast<void>(std::filesystem::create_directories(aRoot, error));
			if (error) {
				return { };
			}
			std::filesystem::permissions(aRoot,
										 std::filesystem::perms::owner_all,
										 std::filesystem::perm_options::replace,
										 error);
			if (error) {
				return { };
			}
			const auto canonical = std::filesystem::canonical(aRoot, error);
			return !error && canonical == aRoot.lexically_normal() ? canonical
																   : std::filesystem::path{ };
		}

		[[nodiscard]] auto windows_system_root()
			-> std::optional<std::filesystem::path>
		{
#if defined(_WIN32)
			auto       value  = std::array<wchar_t, MAX_PATH + 1U>{ };
			const auto length = GetWindowsDirectoryW(
				value.data(), static_cast<UINT>(value.size()));
			if (length == 0U || length >= value.size()) {
				return std::nullopt;
			}
			return std::filesystem::path{ value.data() };
#else
			return std::nullopt;
#endif
		}

		class unavailable_credential_vault final
			: public application::github_credential_vault_port
		{
		public:
			explicit unavailable_credential_vault(
				application::effect_executor& aEffects)
				: Effects{ aEffects }
			{
			}

			void store(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				const auto context = aEnrollment.Context;
				auto       pending = std::make_shared<
					application::github_credential_enrollment>(
					std::move(aEnrollment));
				Effects.post(
					[pending = std::move(pending),
					 context,
					 completion = std::move(aCompletion)]() mutable {
						pending->Credential.reset();
						completion(application::github_credential_store_error{
							.Context = context,
							.Failure = application::github_credential_vault_failure::
								unavailable,
						});
					});
			}

			void retrieve(
				const application::github_credential_operation_context     aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
				override
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

			void delete_credential(
				const application::github_credential_operation_context   aContext,
				application::github_credential_delete_completion_handler aCompletion)
				override
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

			void cancel(
				const application::github_credential_operation_context) override
			{
			}

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
			const github_profile_provenance                    aProvenance)
			-> github_host_profile_candidate
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

		[[nodiscard]] auto profile_option(
			const validated_github_host_profile& aProfile,
			std::string                          aLabel) -> application::github_profile_option
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
			std::deque<validated_github_host_profile>&       aProfiles,
			std::vector<application::github_profile_option>& aOptions,
			const github_host_profile_candidate&             aCandidate,
			std::string                                      aLabel)
		{
			const auto profile = validate_github_host_profile(aCandidate);
			if (!profile.has_value()) {
				throw std::invalid_argument{ "Invalid GitHub host profile" };
			}
			aProfiles.push_back(*profile);
			aOptions.push_back(profile_option(*profile, std::move(aLabel)));
		}

		[[nodiscard]] auto make_platform_vault(
			application::effect_executor& aEffects)
			-> std::unique_ptr<application::github_credential_vault_port>
		{
#if defined(__APPLE__)
			auto vault = adapters::github::make_macos_keychain_vault(aEffects);
#elif defined(_WIN32)
			auto vault = adapters::github::make_windows_credential_manager_vault(aEffects);
#elif defined(__linux__)
			auto vault = adapters::github::make_linux_secret_service_vault(aEffects);
#else
			static_cast<void>(aEffects);
			return { };
#endif
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
			return vault.has_value() ? std::move(*vault) : nullptr;
#endif
		}

		[[nodiscard]] auto initial_github_state() -> core::github_state
		{
			return {
				.ApplicationSession    = core::application_session_id{ 1U },
				.NextRequest           = core::request_id{ 1U },
				.ActiveProfile         = std::nullopt,
				.IdentityGeneration    = { },
				.PendingSession        = std::nullopt,
				.Host                  = std::nullopt,
				.Account               = std::nullopt,
				.Capabilities          = { },
				.CapabilityRevision    = { },
				.PullRequestGeneration = { },
				.PendingPullRequest    = std::nullopt,
				.ActiveLocator         = std::nullopt,
				.ActiveNumber          = std::nullopt,
				.Repository            = std::nullopt,
				.PullRequest           = std::nullopt,
				.AcceptedSnapshot      = std::nullopt,
			};
		}

		/// \brief Keeps the appearance host usable without claiming persistence.
		///
		/// \details A missing record is the System-mode composed default. Writes
		/// report unavailable because the host's `persisted_settings` counter is
		/// explicitly a durable-success counter; returning success here would turn
		/// a session-only change into false persistence evidence.
		class session_only_appearance_settings final
			: public application::appearance_settings_port
		{
		public:
			[[nodiscard]] auto load(const std::stop_token aStop)
				-> application::appearance_settings_result<
					std::optional<application::appearance_settings>> override
			{
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::appearance_settings_error{
							.Failure = application::appearance_settings_failure::cancelled,
						}
					};
				}
				return std::nullopt;
			}

			[[nodiscard]] auto store(
				const application::appearance_settings&,
				const std::stop_token aStop)
				-> application::appearance_settings_result<void> override
			{
				return std::unexpected{ application::appearance_settings_error{
					.Failure = aStop.stop_requested()
						? application::appearance_settings_failure::cancelled
						: application::appearance_settings_failure::unavailable,
				} };
			}
		};

		/// \brief Keeps the workbench usable when its durable layout cannot open.
		///
		/// \details Unlike appearance persistence evidence, the workbench host's
		/// completed-store counter is not a durable-write marker. A successful
		/// store here therefore means exactly what the fallback promises: later
		/// workbench gestures in this process observe the session's last layout,
		/// while a restart begins from the composed defaults again.
		class session_workbench_settings final
			: public application::workbench_settings_port
		{
		public:
			[[nodiscard]] auto load(const std::stop_token aStop)
				-> application::workbench_settings_result<
					application::workbench_layout_settings> override
			{
				if (aStop.stop_requested()) {
					return std::unexpected{ application::workbench_settings_error{
						.Failure = application::workbench_settings_failure::cancelled,
					} };
				}
				const auto lock = std::scoped_lock{ Mutex };
				return Settings;
			}

			[[nodiscard]] auto store(
				const application::workbench_layout_settings& aSettings,
				const std::stop_token                         aStop)
				-> application::workbench_settings_result<void> override
			{
				if (aStop.stop_requested()) {
					return std::unexpected{ application::workbench_settings_error{
						.Failure = application::workbench_settings_failure::cancelled,
					} };
				}
				const auto lock = std::scoped_lock{ Mutex };
				Settings        = aSettings;
				return { };
			}

		private:
			std::mutex                             Mutex;
			application::workbench_layout_settings Settings;
		};

		[[nodiscard]] auto unavailable_theme_document()
			-> application::theme_document_result<std::string>
		{
			return std::unexpected{ application::theme_document_error{ } };
		}

		[[nodiscard]] auto cancelled_theme_document()
			-> application::theme_document_result<std::string>
		{
			return std::unexpected{ application::theme_document_error{
				.Failure = application::theme_document_failure::cancelled,
			} };
		}

#if defined(_WIN32)
		class theme_file_handle final
		{
		public:
			explicit theme_file_handle(void* aHandle) noexcept
				: Handle{ aHandle }
			{
			}
			theme_file_handle(const theme_file_handle&)                    = delete;
			auto operator=(const theme_file_handle&) -> theme_file_handle& = delete;
			theme_file_handle(theme_file_handle&&)                         = delete;
			auto operator=(theme_file_handle&&) -> theme_file_handle&      = delete;
			~theme_file_handle()
			{
				if (valid()) {
					(void)::CloseHandle(Handle);
				}
			}

			[[nodiscard]] auto get() const noexcept -> void*
			{
				return Handle;
			}

			[[nodiscard]] auto valid() const noexcept -> bool
			{
				return Handle != nullptr && Handle != INVALID_HANDLE_VALUE;
			}

		private:
			void* Handle = INVALID_HANDLE_VALUE;
		};
#else
		class theme_file_descriptor final
		{
		public:
			explicit theme_file_descriptor(const int aDescriptor) noexcept
				: Descriptor{ aDescriptor }
			{
			}
			theme_file_descriptor(const theme_file_descriptor&) = delete;
			auto operator=(const theme_file_descriptor&)
				-> theme_file_descriptor&                  = delete;
			theme_file_descriptor(theme_file_descriptor&&) = delete;
			auto operator=(theme_file_descriptor&&)
				-> theme_file_descriptor& = delete;
			~theme_file_descriptor()
			{
				if (Descriptor >= 0) {
					(void)::close(Descriptor);
				}
			}

			[[nodiscard]] auto get() const noexcept -> int
			{
				return Descriptor;
			}

		private:
			int Descriptor = -1;
		};
#endif

		[[nodiscard]] auto read_bounded_regular_theme_document(
			const std::string_view aPath,
			const std::size_t      aMaximumBytes,
			const std::stop_token  aStop)
			-> application::theme_document_result<std::string>
		{
			if (aStop.stop_requested()) {
				return cancelled_theme_document();
			}
			if (aPath.empty() || aPath.find('\0') != std::string_view::npos) {
				return unavailable_theme_document();
			}

#if defined(_WIN32)
			const auto path = std::filesystem::u8path(aPath.begin(), aPath.end());
			auto       file = theme_file_handle{ ::CreateFileW(
				path.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				nullptr) };
			if (!file.valid() || ::GetFileType(file.get()) != FILE_TYPE_DISK) {
				return unavailable_theme_document();
			}
			auto information = BY_HANDLE_FILE_INFORMATION{ };
			if (::GetFileInformationByHandle(file.get(), &information) == 0
				|| (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
				return unavailable_theme_document();
			}
#else
			const auto path = std::filesystem::path{ aPath };
			auto       file = theme_file_descriptor{
				::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK)
			};
			if (file.get() < 0) {
				return unavailable_theme_document();
			}
			struct stat information{ };
			if (::fstat(file.get(), &information) != 0
				|| !S_ISREG(information.st_mode)) {
				return unavailable_theme_document();
			}
#endif

			auto document = std::string(aMaximumBytes, '\0');
			auto offset   = std::size_t{ 0U };
			while (offset < document.size()) {
				if (aStop.stop_requested()) {
					return cancelled_theme_document();
				}
				const auto remaining = document.size() - offset;
				const auto chunk
					= std::min(remaining, std::size_t{ 64U * 1'024U });
#if defined(_WIN32)
				auto readBytes = DWORD{ 0U };
				if (::ReadFile(
						file.get(),
						document.data() + offset,
						static_cast<DWORD>(chunk),
						&readBytes,
						nullptr)
					== 0) {
					return unavailable_theme_document();
				}
				if (readBytes == 0U) {
					break;
				}
				offset += static_cast<std::size_t>(readBytes);
#else
				const auto readBytes
					= ::read(file.get(), document.data() + offset, chunk);
				if (readBytes == 0) {
					break;
				}
				if (readBytes < 0) {
					if (errno == EINTR) {
						continue;
					}
					return unavailable_theme_document();
				}
				offset += static_cast<std::size_t>(readBytes);
#endif
			}
			document.resize(offset);
			if (aStop.stop_requested()) {
				return cancelled_theme_document();
			}
			return document;
		}

		/// \brief Performs the only local filesystem read in theme import.
		///
		/// \details Qt supplies a path and the application host supplies the byte
		/// bound. Reading one byte beyond the core limit is intentional: the core
		/// parser then returns its typed `document_too_large` refusal instead of
		/// receiving a silently truncated document.
		class bounded_theme_document_reader final
			: public application::theme_document_port
		{
		public:
			[[nodiscard]] auto read(
				const std::string_view aPath,
				const std::size_t      aMaximumBytes,
				const std::stop_token  aStop)
				-> application::theme_document_result<std::string> override
			{
				return read_bounded_regular_theme_document(
					aPath, aMaximumBytes, aStop);
			}
		};

		[[nodiscard]] auto prompt_for_credential(
			QWidget* aParent) -> std::optional<application::github_credential_material>
		{
			auto input = qt::prompt_for_sensitive_text(
				application::maximum_github_credential_bytes,
				aParent);
			if (!input.has_value()) {
				return std::nullopt;
			}

			auto material = application::github_credential_material::create(input->bytes());
			input->reset();
			if (!material.has_value()) {
				return std::nullopt;
			}
			return std::move(*material);
		}
	}

	/// \brief Supplies blob bytes to the classifier through the epoch host.
	///
	/// \details The classifier reads no repository. It asks for one blob's
	/// bytes, and the only component allowed to produce them is the epoch
	/// host, whose cache session is lifetime-exclusive - a second reader
	/// opening its own Git cache writer is the failure this indirection
	/// exists to prevent.
	///
	/// `materialize` needs the request context that names the authority its
	/// cache session was opened for, so the comparison currently being
	/// classified is retained here. It is set on the decision lane and read on
	/// the appearance effect executor, which is why it is guarded.
	///
	/// The retained value is a comparison, never a classification: this class
	/// holds no span, decides no role, and would be useless to anything that
	/// wanted to skip the classifier.
	class composition_blob_source final : public adapters::syntax::blob_source
	{
	public:
		explicit composition_blob_source(application::diff_blob_port& aBlobs)
			: Blobs{ aBlobs }
		{
		}

		void set_comparison(const core::diff_request_context& aContext)
		{
			const auto guard = std::lock_guard{ Mutex };
			Context          = aContext;
		}

		void clear_comparison()
		{
			const auto guard = std::lock_guard{ Mutex };
			Context.reset();
		}

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid& aBlob)
			-> std::optional<std::string> override
		{
			const auto context = [this] {
				const auto guard = std::lock_guard{ Mutex };
				return Context;
			}();
			if (!context.has_value()) {
				return std::nullopt;
			}
			// Side is reported back rather than acted on, so it is derived
			// from the comparison instead of guessed. A blob that belongs to
			// neither side is not this comparison's to read.
			const auto side = context->Key.LeftBlob == std::optional{ aBlob }
				? core::diff_side::left
				: core::diff_side::right;
			if (
				context->Key.LeftBlob != std::optional{ aBlob }
				&& context->Key.RightBlob != std::optional{ aBlob }) {
				return std::nullopt;
			}
			const auto observation = Blobs.materialize(
				application::diff_blob_request{
					.Context      = *context,
					.Side         = side,
					.Blob         = aBlob,
					.MaximumBytes = adapters::syntax::maximum_classified_blob_bytes,
				},
				std::stop_token{ });
			if (
				observation.Status != application::diff_blob_status::available) {
				// Absent rather than empty. An empty string would classify as
				// a file with no tokens, which is a different and wrong
				// answer from "these bytes were not available".
				return std::nullopt;
			}
			return std::string{
				observation.Bytes.begin(), observation.Bytes.end()
			};
		}

	private:
		application::diff_blob_port&              Blobs;
		std::mutex                                Mutex;
		std::optional<core::diff_request_context> Context;
	};

	/// \brief Routes appearance projections to the surfaces that consume them.
	///
	/// \details BPR-0182 composed the host, BPR-0183 made the appearance
	/// profile drive the composed window, and BPR-0185 gives the classified
	/// spans somewhere to go. `github_window` still does not implement
	/// `appearance_projection_port`, so the presentation invalidation half of
	/// a projection is counted rather than routed; the span half is handed to
	/// the sink the composition installs.
	class composition_appearance_projections final
		: public application::appearance_projection_port
	{
	public:
		using span_sink
			= std::function<void(const application::token_span_publication&)>;
		using presentation_sink     = std::function<void(
			const application::appearance_presentation_invalidation&)>;
		using theme_preview_sink    = std::function<void(
			const application::theme_preview_publication&)>;
		using palette_adoption_sink = std::function<void(
			const application::palette_adoption_publication&)>;
		using refusal_sink          = std::function<void(
			const application::appearance_failure_publication&)>;

		void set_span_sink(span_sink aSink)
		{
			SpanSink = std::move(aSink);
		}

		void set_presentation_sink(presentation_sink aSink)
		{
			PresentationSink = std::move(aSink);
		}

		void set_theme_preview_sink(theme_preview_sink aSink)
		{
			ThemePreviewSink = std::move(aSink);
		}

		void set_refusal_sink(refusal_sink aSink)
		{
			RefusalSink = std::move(aSink);
		}

		void set_palette_adoption_sink(palette_adoption_sink aSink)
		{
			PaletteAdoptionSink = std::move(aSink);
		}

		void publish(const application::appearance_projection& aProjection) override
		{
			++Published;
			if (aProjection.Spans.has_value() && SpanSink) {
				SpanSink(*aProjection.Spans);
			}
			// The host publishes a bounded invalidation naming which domains
			// changed. What actually changed is read from the host's own
			// settings rather than reconstructed from the invalidation,
			// because the invalidation says a domain moved and not what it
			// moved to - and the record is the thing a surface has to show.
			if (aProjection.Presentation.has_value() && PresentationSink) {
				PresentationSink(*aProjection.Presentation);
			}
			if (aProjection.ThemePreview.has_value() && ThemePreviewSink) {
				ThemePreviewSink(*aProjection.ThemePreview);
			}
			if (aProjection.PaletteAdoption.has_value() && PaletteAdoptionSink) {
				PaletteAdoptionSink(*aProjection.PaletteAdoption);
			}
		}

		void fail(const application::appearance_failure_publication& aFailure) override
		{
			++Failed;
			if (RefusalSink) {
				RefusalSink(aFailure);
			}
		}

		[[nodiscard]] auto published() const noexcept -> std::size_t
		{
			return Published;
		}

		[[nodiscard]] auto failed() const noexcept -> std::size_t
		{
			return Failed;
		}

	private:
		span_sink             SpanSink;
		presentation_sink     PresentationSink;
		theme_preview_sink    ThemePreviewSink;
		refusal_sink          RefusalSink;
		palette_adoption_sink PaletteAdoptionSink;
		std::size_t           Published = 0U;
		std::size_t           Failed    = 0U;
	};

	/// \brief Retains the exact accepted review observed by the workbench host.
	///
	/// \details The GitHub host owns the accepted snapshot, but the workbench
	/// observation runs on its own effect lane. Copying the already bounded file
	/// list here gives that worker immutable facts without reading mutable host
	/// state across lanes. The complete pull-request authority is compared on
	/// every read; a request for any other identity receives no files.
	class composition_workbench_review final
	{
	public:
		void adopt(
			const application::accepted_pull_request_handoff&               aHandoff,
			const std::optional<application::github_pull_request_snapshot>& aSnapshot) noexcept
		{
			auto files     = std::vector<application::github_file_record>{ };
			auto available = false;
			try {
				if (aSnapshot.has_value()
					&& aSnapshot->Profile == aHandoff.Profile
					&& aSnapshot->Host == aHandoff.Host
					&& aSnapshot->Account == aHandoff.Account
					&& aSnapshot->Snapshot == aHandoff.Snapshot
					&& aSnapshot->Repository.Repository == aHandoff.Repository
					&& aSnapshot->PullRequest.PullRequest == aHandoff.PullRequest
					&& aSnapshot->PullRequest.Number == aHandoff.Number) {
					files     = aSnapshot->Files;
					available = true;
				}
			} catch (...) {
				available = false;
				files.clear();
			}

			const auto lock = std::scoped_lock{ Mutex };
			Review          = accepted_review{
				.Authority = workbench_authority(aHandoff),
				.Files     = std::move(files),
				.Available = available,
				.Active    = true,
			};
		}

		[[nodiscard]] auto files(
			const application::workbench_pull_request_authority& aAuthority) const
			-> std::optional<std::vector<application::github_file_record>>
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (!Review.has_value() || Review->Authority != aAuthority
				|| !Review->Available || !Review->Active) {
				return std::nullopt;
			}
			return Review->Files;
		}

		/// \brief Tombstones one exact authority and returns it for host invalidation.
		[[nodiscard]] auto invalidate(
			const application::pull_request_authority_invalidation& aInvalidation) noexcept
			-> std::optional<application::workbench_pull_request_authority>
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (!matches(aInvalidation)) {
				return std::nullopt;
			}
			Review->Files.clear();
			Review->Available = false;
			Review->Active    = false;
			return Review->Authority;
		}

#if defined(BPR_BUILD_TESTING)
		[[nodiscard]] auto active() const noexcept -> bool
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Review.has_value() && Review->Active;
		}

		[[nodiscard]] auto exact(
			const application::accepted_pull_request_handoff& aHandoff) const noexcept
			-> bool
		{
			const auto lock = std::scoped_lock{ Mutex };
			return Review.has_value() && Review->Active
				&& Review->Authority == workbench_authority(aHandoff)
				&& Review->Available;
		}
		void make_unavailable(
			const application::workbench_pull_request_authority& aAuthority) noexcept
		{
			const auto lock = std::scoped_lock{ Mutex };
			if (Review.has_value() && Review->Authority == aAuthority) {
				Review->Available = false;
				Review->Files.clear();
			}
		}
#endif

	private:
		struct accepted_review
		{
			application::workbench_pull_request_authority Authority;
			std::vector<application::github_file_record>  Files;
			bool                                          Available = false;
			bool                                          Active    = false;
		};

		[[nodiscard]] auto matches(
			const application::pull_request_authority_invalidation& aInvalidation) const noexcept
			-> bool
		{
			if (!Review.has_value()) {
				return false;
			}
			const auto& authority = Review->Authority;
			return authority.ApplicationSession == aInvalidation.ApplicationSession
				&& authority.IdentityGeneration == aInvalidation.IdentityGeneration
				&& authority.Profile == aInvalidation.Profile
				&& authority.ProfileRevision == aInvalidation.ProfileRevision
				&& authority.Host == aInvalidation.Host
				&& authority.HostAuthority == aInvalidation.HostAuthority
				&& authority.Account == aInvalidation.Account
				&& authority.Viewer == aInvalidation.Viewer
				&& authority.Repository == aInvalidation.Repository
				&& authority.PullRequest == aInvalidation.PullRequest;
		}

		mutable std::mutex             Mutex;
		std::optional<accepted_review> Review;
	};

	/// \brief Keeps one failed ledger authority typed and fail-closed.
	class unavailable_review_ledger_store final
		: public application::review_ledger_store_port
	{
	public:
		explicit unavailable_review_ledger_store(
			const application::ledger_store_failure aFailure) noexcept
			: Failure{ aFailure }
		{
		}

		auto commit(const application::receipt_persistence_request&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto load(const core::file_identity&) -> std::expected<
			std::optional<core::file_receipt>,
			application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto store_note(const application::local_note_record&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto erase_note(application::local_note_id)
			-> std::expected<void, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto store_last_reviewed_head(const core::git_commit_oid&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto erase_last_reviewed_head()
			-> std::expected<void, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto store_defect(const application::durable_defect_record&)
			-> std::expected<void, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto erase_defect(core::defect_id)
			-> std::expected<void, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto load_private() -> std::expected<
			application::private_ledger_observation,
			application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

	private:
		application::ledger_store_failure Failure
			= application::ledger_store_failure::unavailable;
	};

	/// \brief Forwards selected diff terminals and observes accepted availability.
	class composition_diff_projection final
		: public application::diff_projection_port
	{
	public:
		using publication_sink = std::function<void(
			const application::diff_publication&)>;

		composition_diff_projection(
			application::diff_projection_port& aTarget,
			publication_sink                   aPublication)
			: Target{ &aTarget }
			, Publication{ std::move(aPublication) }
		{
		}

		void publish(const application::diff_publication& aPublication) override
		{
			Target->publish(aPublication);
			if (Publication) {
				Publication(aPublication);
			}
		}

		void fail(const application::diff_failure_publication& aFailure) override
		{
			Target->fail(aFailure);
		}

		void invalidate(
			const core::pull_request_authority& aAuthority) override
		{
			Target->invalidate(aAuthority);
		}

	private:
		application::diff_projection_port* Target = nullptr;
		publication_sink                   Publication;
	};

	class github_application::implementation final
		: public application::github_epoch_handoff_port,
		  public application::epoch_projection_port,
		  public application::workbench_selected_diff_projection_port,
		  public application::receipt_projection_port
	{
	public:
		explicit implementation(
			github_application_config aConfig
#if defined(BPR_BUILD_TESTING)
			,
			const std::optional<detail::github_public_path_fixture> aFixture        = std::nullopt,
			detail::github_public_path_fixture_options              aFixtureOptions = { }
#endif
		)
		{
#if defined(BPR_BUILD_TESTING)
			if (aFixture.has_value()) {
				DisableWorkbenchSelectionWire
					= aFixtureOptions.DisableWorkbenchSelectionWire;
				DisableReviewLedgerGestureWire
					= aFixtureOptions.DisableReviewLedgerGestureWire;
				const auto host = fixture_host(*aFixture);
				append_profile(
					Profiles,
					ProfileOptions,
					native_profile_candidate(
						host.Profile,
						host.Host,
						application::github_profile_revision{ 1U },
						host.Server,
						application::github_authentication_transport::native_https,
						std::string{ host.Hostname },
						github_profile_provenance::builtin),
					std::string{ host.Label });
				Fixture = std::make_unique<
					detail::github_public_path_fixture_components>(
					*aFixture,
					Profiles.front(),
					VaultEffects,
					std::move(aFixtureOptions));
			} else
#endif
			{
				append_profile(
					Profiles,
					ProfileOptions,
					native_profile_candidate(
						core::host_profile_id{ 1U },
						core::git_host_id{ 1U },
						application::github_profile_revision{ 1U },
						application::github_server_contract::
							github_com_2022_11_28,
						application::github_authentication_transport::native_https,
						"github.com",
						github_profile_provenance::builtin),
					"GitHub.com — Native HTTPS");
				append_profile(
					Profiles,
					ProfileOptions,
					native_profile_candidate(
						core::host_profile_id{ 2U },
						core::git_host_id{ 1U },
						application::github_profile_revision{ 1U },
						application::github_server_contract::
							github_com_2022_11_28,
						application::github_authentication_transport::github_cli,
						"github.com",
						github_profile_provenance::builtin),
					"GitHub.com — GitHub CLI (unavailable)");
				if (aConfig.EnterpriseHostname.has_value()) {
					append_profile(
						Profiles,
						ProfileOptions,
						native_profile_candidate(
							core::host_profile_id{ 3U },
							core::git_host_id{ 2U },
							application::github_profile_revision{ 1U },
							application::github_server_contract::
								ghes_3_18_2022_11_28,
							application::github_authentication_transport::
								native_https,
							*aConfig.EnterpriseHostname,
							github_profile_provenance::explicit_user),
						"GHES 3.18 — Native HTTPS");
					append_profile(
						Profiles,
						ProfileOptions,
						native_profile_candidate(
							core::host_profile_id{ 4U },
							core::git_host_id{ 2U },
							application::github_profile_revision{ 1U },
							application::github_server_contract::
								ghes_3_18_2022_11_28,
							application::github_authentication_transport::
								github_cli,
							*aConfig.EnterpriseHostname,
							github_profile_provenance::explicit_user),
						"GHES 3.18 — GitHub CLI (unavailable)");
				}
			}

#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr) {
				TransportFactory = Fixture->transport_factory();
			} else
#endif
			{
				TransportFactory = std::make_shared<native_github_wire_transport_factory>();
			}

			auto profilePointers = std::vector<const validated_github_host_profile*>{ };
			profilePointers.reserve(Profiles.size());
			for (const auto& profile : Profiles) {
				profilePointers.push_back(&profile);
			}
			auto provider = make_github_provider(
				std::span<const validated_github_host_profile* const>{
					profilePointers },
				ProviderEffects,
				TransportFactory);
			if (!provider.has_value()) {
				throw std::runtime_error{ "GitHub provider composition failed" };
			}
			Provider = std::move(*provider);

#if defined(BPR_BUILD_TESTING)
			if (Fixture == nullptr)
#endif
			{
				OwnedVault                  = make_platform_vault(VaultEffects);
				NativePlatformVaultSelected = OwnedVault != nullptr;
				if (OwnedVault == nullptr) {
					OwnedVault = std::make_unique<unavailable_credential_vault>(
						VaultEffects);
				}
			}

			auto applicationDataRoot = std::filesystem::path{ };
			auto gitExecutable       = std::filesystem::path{ };
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr) {
				applicationDataRoot = Fixture->application_data_root();
				gitExecutable       = Fixture->git_executable();
			} else
#endif
			{
				applicationDataRoot = prepare_application_data_root(
					native_application_data_root(aConfig));
				gitExecutable = native_git_executable(aConfig);
			}
			if (applicationDataRoot.empty() || gitExecutable.empty()) {
				throw std::runtime_error{
					"Application data or installed Git is unavailable"
				};
			}
			ApplicationDataRoot = applicationDataRoot;

			auto cache   = adapters::git::make_installed_git_cache({
				.GitExecutable     = gitExecutable,
				.StorageRoot       = applicationDataRoot,
				.WindowsSystemRoot = windows_system_root(),
			});
			auto catalog = adapters::sqlite::make_epoch_catalog({
				.StorageRoot = applicationDataRoot,
			});
			if (!cache || !catalog) {
				throw std::runtime_error{
					"Git cache or epoch catalog composition failed"
				};
			}
			GitCaches     = std::move(*cache);
			EpochCatalogs = std::move(*catalog);

			// The workbench layout is a third database beside the catalog and
			// the ledger, for the same reason those two are apart: deleting one
			// kind of local state must not destroy another. Losing a drawer
			// width must not lose reading progress. Composition failure is not
			// fatal - a reviewer whose layout cannot be stored still gets the
			// composed defaults.
			auto layout = adapters::sqlite::make_workbench_settings_store({
				.StorageRoot = applicationDataRoot,
			});
			if (layout) {
				WorkbenchSettings             = std::move(*layout);
				WorkbenchPersistenceAvailable = true;
			} else {
				WorkbenchSettings
					= std::make_unique<session_workbench_settings>();
			}

			// A fourth database, kept apart for the same reason as the rest.
			// Composition failure is not fatal: a reviewer whose theme cannot
			// be stored still gets the composed defaults.
			auto appearance = adapters::sqlite::make_appearance_settings_store({
				.StorageRoot = applicationDataRoot,
			});
			if (appearance) {
				AppearanceSettings             = std::move(*appearance);
				AppearancePersistenceAvailable = true;
			} else {
				AppearanceSettings
					= std::make_unique<session_only_appearance_settings>();
			}

			EpochEntropy = application::make_system_epoch_entropy();
			EpochClock   = application::make_system_epoch_clock();
			if (!EpochEntropy || !EpochClock) {
				throw std::runtime_error{ "Epoch runtime ports are unavailable" };
			}

			// BPR-0191: the application shows RFC-BPR-0005's shell. Until this
			// line changed, the shell existed only in its own tests and every
			// reviewer saw the RFC-BPR-0002 window instead.
			Window = std::make_unique<qt::workbench_window>(
				[this](core::workbench_interaction_intent aIntent) {
#if defined(BPR_BUILD_TESTING)
					if (DisableWorkbenchSelectionWire
						&& std::holds_alternative<
							core::select_workbench_file_intent>(aIntent)) {
						return;
					}
#endif
					dispatch_workbench_intent(std::move(aIntent));
				},
				[this](core::navigate_diff_change_intent aIntent) {
					dispatch_workbench_change(aIntent);
				},
				[this](const core::workbench_drawer aDrawer,
					   const std::uint32_t          aWidth) {
					dispatch_workbench_width(aDrawer, aWidth);
				},
				ProfileOptions,
				[this](application::github_intent aIntent) { dispatch_intent(std::move(aIntent)); });
			Window->set_receipt_dispatch(
				[this](core::file_receipt_command aCommand) {
#if defined(BPR_BUILD_TESTING)
					if (DisableReviewLedgerGestureWire) {
						return;
					}
#endif
					dispatch_receipt(std::move(aCommand));
				});
			Window->set_review_draft_dispatch(
				[this](core::review_draft_command aCommand) {
					dispatch_review_draft(std::move(aCommand));
				});
			Window->set_defect_dispatch(
				[this](core::defect_command aCommand) {
					dispatch_defect(std::move(aCommand));
				});
			Window->set_defect_classification_dispatch(
				[this](std::string aConversation) {
					classify_defect(std::move(aConversation));
				});

			QueueStore   = adapters::sqlite::make_queue_enrollment_store({
				.StorageRoot = applicationDataRoot,
			});
			QueueRefresh = std::make_unique<composition_queue_refresh>();
			QueueHost    = std::make_unique<application::review_queue_application_host>(
				*QueueStore,
				*QueueRefresh,
				QueueEffects,
				DecisionLane,
				*Window,
				application::queue_session_id{ });
			Window->set_queue_rule_dispatch(
				[this](application::queue_rule_intent aIntent) {
					if (!Closing && QueueHost != nullptr) {
						QueueHost->apply(std::move(aIntent));
					}
				});
			Window->set_queue_preferences_dispatch(
				[this](application::queue_preferences_intent aIntent) {
					if (!Closing && QueueHost != nullptr) {
						QueueHost->apply(std::move(aIntent));
					}
				});
			QueueHost->restore();

			EpochHost = std::make_unique<application::epoch_application_host>(
				core::epoch_state{
					.ApplicationSession = core::application_session_id{ 1U },
				},
				*GitCaches,
				*EpochCatalogs,
				*EpochEntropy,
				*EpochClock,
				EpochEffects,
				DecisionLane,
				*this);
			Window->epoch_selectors()->set_dispatch(
				[this](application::epoch_intent aIntent) { dispatch_epoch_intent(std::move(aIntent)); });
			DiffProjections = std::make_unique<composition_diff_projection>(
				*Window->comparison_surface(),
				[this](const application::diff_publication& aPublication) {
					observe_selected_material(aPublication);
				});
			DiffHost = std::make_unique<application::diff_application_host>(
				core::diff_state{
					.ApplicationSession = core::application_session_id{ 1U },
				},
				*EpochHost,
				DiffEffects,
				DecisionLane,
				*DiffProjections);

			// The workbench observes the review's files through the read model
			// the provider already produces. The supplier is read at call time
			// rather than captured, because the accepted snapshot changes as
			// the reviewer opens pull requests.
			WorkbenchObservations = std::make_unique<
				application::snapshot_workbench_observation>(
				[this](const application::workbench_pull_request_authority& aAuthority)
					-> std::optional<
						std::vector<application::github_file_record>> {
					return WorkbenchReview.files(aAuthority);
				},
				[this](
					const application::workbench_pull_request_authority& aAuthority,
					const core::file_identity                            aFile)
					-> std::optional<core::actionability_input> {
					const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
					if (!ReviewLedgerAuthority.has_value()
						|| *ReviewLedgerAuthority != aAuthority
						|| !ReviewLedgerReceipt.has_value()
						|| ReviewLedgerReceipt->File != aFile) {
						return std::nullopt;
					}
					return core::actionability_input{
						.Receipt = *ReviewLedgerReceipt,
					};
				});

			Host = std::make_unique<application::github_application_host>(
				initial_github_state(),
				*Provider,
				*Provider,
				*Provider,
				*Provider,
				credential_vault(),
				CredentialBindings,
				DecisionLane,
				*Window,
				application::github_credential_shutdown_policy::
					delete_retained_on_shutdown,
				this);

			// Composed last, because its observation supplier receives accepted
			// snapshots from `Host`. The settings port is always present: either
			// the durable adapter opened or composition supplied session defaults.
			// The real window is the projection port, so success and failure both
			// terminate at the shipped surface rather than at an intermediate sink.
			WorkbenchHost = std::make_unique<
				application::workbench_application_host>(
				core::workbench_interaction_state{ },
				*WorkbenchObservations,
				*WorkbenchSettings,
				WorkbenchEffects,
				DecisionLane,
				*Window,
				core::application_session_id{ 1U },
				application::workbench_host_limits{ },
				EpochHost.get(),
				this);
			// Reads the current durable or session layout. Restoring never writes
			// it back, so merely launching still leaves a fresh store fresh.
			WorkbenchHost->restore();

			// The queue notification surface. Composed here so the delivery
			// path exists in the product rather than only in its suite; what
			// still does not exist is a composed review queue to produce
			// requests, which RFC-BPR-0010's own composition owns. Every
			// delivery in this build therefore comes from the fixture probe.
			NotificationSink = std::make_unique<qt::system_tray_notification_sink>(
				Window.get());
			Notifications = std::make_unique<application::review_queue_notifier>(
				*NotificationSink,
				[this](application::queue_notification_target aTarget) {
					// The notifier selects nothing; this is where a chosen
					// review would be opened. RFC-BPR-0010's queue surface
					// owns that, so the target is recorded and the separation
					// is kept rather than short-circuited.
					ActivatedNotification = std::move(aTarget);
				});
			NotificationSink->set_activation_sink(
				[this](application::queue_notification_target aTarget) {
					if (Notifications) {
						Notifications->activate(aTarget);
					}
				});

			// Composed before the appearance host, which needs a live
			// classification port. The blob source reads through the epoch
			// host rather than opening a second Git cache session.
			SyntaxDefinitions = std::make_unique<
				adapters::syntax::packaged_definition_catalog>();
			SyntaxBlobs          = std::make_unique<composition_blob_source>(*EpochHost);
			SyntaxClassification = std::make_unique<
				adapters::syntax::packaged_syntax_classifier>(
				*SyntaxDefinitions,
				*SyntaxBlobs);

			AppearanceProjections.set_span_sink(
				[this](const application::token_span_publication& aSpans) {
					adopt_classification(aSpans);
				});
			AppearanceProjections.set_presentation_sink(
				[this](const application::appearance_presentation_invalidation&
					   /*aInvalidation*/) { adopt_appearance_settings(); });
			AppearanceProjections.set_palette_adoption_sink(
				[this](const application::palette_adoption_publication&) {
					adopt_appearance_settings(true);
				});
			AppearanceProjections.set_theme_preview_sink(
				[this](const application::theme_preview_publication& aPreview) {
					preview_imported_theme(aPreview.Palette);
				});
			AppearanceProjections.set_refusal_sink(
				[this](const application::appearance_failure_publication&
						   aFailure) { report_appearance_refusal(aFailure); });
			AppearanceHost = std::make_unique<
				application::appearance_application_host>(
				qt::default_appearance_settings(),
				*AppearanceSettings,
				ThemeDocuments,
				*SyntaxClassification,
				AppearanceEffects,
				DecisionLane,
				AppearanceProjections,
				core::application_session_id{ 1U });
			// Loads the durable record. An absent record keeps the composed
			// settings and is never written back, so a fresh installation
			// stays a fresh installation until the reviewer chooses
			// something.
			AppearanceHost->restore();

			// The Qt adapter is the presentation authority for appearance;
			// the host owns the durable record and the span cache. The
			// profile is therefore built *from* the host's settings after
			// restore, so a stored mode is the one presentation starts on
			// rather than one it is corrected to a frame later.
			AppearanceProfile = std::make_unique<qt::appearance_profile>(
				AppearanceHost->settings(),
				native_appearance_observation());
			AppearanceProfile->set_sink([this] { apply_appearance(); });

			// The native scheme is an *observation*, not a setting. This is
			// the inversion the whole slice exists for: before it, the
			// operating system decided presentation directly and an
			// explicit mode had nothing to ignore. Now the system reports
			// and `core::resolve_appearance` decides.
			if (auto* const hints = QGuiApplication::styleHints();
				hints != nullptr) {
				QObject::connect(
					hints,
					&QStyleHints::colorSchemeChanged,
					Window.get(),
					[this] { observe_native_appearance(); });
			}
			// The settings surface dispatches into the host, never into
			// presentation. That is what makes the host the single
			// authority on what is in effect: a gesture that reached the
			// profile directly would change the screen without changing
			// the record, and the next launch would silently disagree.
			if (auto* const panel = Window->appearance_settings();
				panel != nullptr) {
				if (!AppearancePersistenceAvailable) {
					panel->report_persistence_unavailable();
				}
				panel->set_dispatch(
					[this](application::appearance_intent aIntent) {
						dispatch_appearance_intent(std::move(aIntent));
					});
			}
			// Shows what restore() loaded, so the surface opens on the
			// settings actually in effect rather than on its own defaults.
			adopt_appearance_settings();
			apply_appearance();
		}

		~implementation() override
		{
			begin_shutdown();
			while (!ShutdownComplete) {
				poll_shutdown();
				QCoreApplication::processEvents(QEventLoop::AllEvents);
				std::this_thread::yield();
			}
		}

		[[nodiscard]] auto credential_vault() noexcept
			-> application::github_credential_vault_port&
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr) {
				return Fixture->credential_vault();
			}
#endif
			return *OwnedVault;
		}

		void dispatch_workbench_intent(
			core::workbench_interaction_intent aIntent)
		{
			if (Closing || WorkbenchHost == nullptr) {
				return;
			}
#if defined(BPR_BUILD_TESTING)
			++WorkbenchIntentDispatches;
#endif
			WorkbenchHost->apply(std::move(aIntent));
		}

		void dispatch_workbench_change(
			const core::navigate_diff_change_intent aIntent)
		{
			if (Closing || Window == nullptr
				|| Window->comparison_surface() == nullptr) {
				return;
			}
#if defined(BPR_BUILD_TESTING)
			++WorkbenchChangeDispatches;
#endif
			auto* const button = aIntent.Direction
					== core::change_navigation_direction::previous
				? Window->comparison_surface()->previous_change_button()
				: Window->comparison_surface()->next_change_button();
			if (button != nullptr) {
				button->click();
			}
		}

		void dispatch_workbench_width(
			const core::workbench_drawer aDrawer,
			const std::uint32_t          aWidth)
		{
			if (Closing || WorkbenchHost == nullptr) {
				return;
			}
#if defined(BPR_BUILD_TESTING)
			++WorkbenchWidthDispatches;
#endif
			WorkbenchHost->resize(aDrawer, aWidth);
		}

		void dispatch_receipt(core::file_receipt_command aCommand)
		{
			if (Closing || ReviewLedgerHost == nullptr) {
				return;
			}
			static_cast<void>(ReviewLedgerHost->submit(aCommand));
		}

		[[nodiscard]] static auto thread_identity(
			const application::github_review_thread_record& aThread) -> std::string
		{
			return "review-thread:" + std::string{ aThread.Thread.value() };
		}

		[[nodiscard]] auto accepted_thread(const std::string_view aIdentity) const
			-> const application::github_review_thread_record*
		{
			if (Host == nullptr || !Host->accepted_snapshot().has_value()) {
				return nullptr;
			}
			const auto&                                     threads = Host->accepted_snapshot()->ReviewThreads;
			const application::github_review_thread_record* result  = nullptr;
			for (const auto& thread : threads) {
				const auto identity = thread_identity(thread);
				if (identity != aIdentity) {
					continue;
				}
				if (result != nullptr) {
					return nullptr;
				}
				result = &thread;
			}
			return result;
		}

		void present_defects()
		{
			if (Window == nullptr) {
				return;
			}
			auto histories = std::vector<core::defect_history>{ };
			histories.reserve(DurableDefects.size());
			for (const auto& defect : DurableDefects) {
				histories.push_back(defect.History);
			}
			Window->present_defects(std::move(histories));
		}

		void correlate_defects(
			const std::span<const core::source_line_ref> aChangedAnchors = { })
		{
			if (ReviewLedgerHost == nullptr || Host == nullptr
				|| !Host->accepted_snapshot().has_value()) {
				return;
			}
			const auto& snapshot = *Host->accepted_snapshot();
			auto        records  = std::vector<core::defect_record>{ };
			auto        links    = std::vector<application::defect_thread_link>{ };
			auto        observations
				= std::vector<application::defect_thread_observation>{ };
			records.reserve(DurableDefects.size());
			links.reserve(DurableDefects.size());
			observations.reserve(DurableDefects.size());
			auto changedAnchors = std::vector<core::source_line_ref>{
				aChangedAnchors.begin(), aChangedAnchors.end()
			};
			for (auto& durable : DurableDefects) {
				const application::github_review_thread_record* found     = nullptr;
				bool                                            collision = false;
				for (const auto& thread : snapshot.ReviewThreads) {
					const auto identity = defect_identity(thread_identity(thread));
					if (identity.Conversation != durable.Conversation.value()
						|| identity.Defect != durable.History.Defect.Id.value()) {
						continue;
					}
					if (found != nullptr) {
						collision = true;
						break;
					}
					found = &thread;
				}
				if (collision) {
					records.push_back(durable.History.Defect);
					continue;
				}
				if (found == nullptr) {
					const auto history = core::lose_anchor(
						durable.History,
						core::defect_anchor_availability::thread_deleted);
					if (history != durable.History) {
						auto changed    = durable;
						changed.History = history;
						if (ReviewLedgerHost->persist(changed)) {
							durable = std::move(changed);
						}
					}
					records.push_back(durable.History.Defect);
					continue;
				}
				const auto identity = thread_identity(*found);
				links.push_back(application::defect_thread_link{
					.Defect = durable.History.Defect.Id, .ThreadId = identity });
				const auto author        = snapshot.PullRequest.Author.has_value()
					? snapshot.PullRequest.Author->Login
					: std::string{ };
				const auto authorReplied = found->Comments.size() > 1U
					&& std::ranges::any_of(
											   std::next(found->Comments.begin()),
											   found->Comments.end(),
											   [&author](const application::github_review_comment_record& aComment) {
												   return !author.empty() && aComment.Author.has_value()
													   && aComment.Author->Login == author;
											   });
				observations.push_back(application::defect_thread_observation{
					.ThreadId           = identity,
					.Resolved           = found->Resolved,
					.AuthorRepliedSince = authorReplied,
				});
				const auto availability = found->Outdated
					? core::defect_anchor_availability::outdated
					: core::defect_anchor_availability::available;
				if (found->Outdated) {
					changedAnchors.push_back(durable.History.Defect.Anchor);
				}
				if (availability != durable.History.Availability) {
					auto changed                 = durable;
					changed.History.Availability = availability;
					if (ReviewLedgerHost->persist(changed)) {
						durable = std::move(changed);
					}
				}
				records.push_back(durable.History.Defect);
			}
			const auto correlated
				= application::correlate(records, links, observations, changedAnchors);
			for (const auto& record : correlated.Defects) {
				const auto found = std::ranges::find(
					DurableDefects,
					record.Id,
					[](const application::durable_defect_record& aDefect) {
						return aDefect.History.Defect.Id;
					});
				if (found == DurableDefects.end()
					|| found->History.Defect == record) {
					continue;
				}
				auto changed           = *found;
				changed.History.Defect = record;
				if (ReviewLedgerHost->persist(changed)) {
					*found = std::move(changed);
				}
			}
			present_defects();
		}

		void load_defects()
		{
			DurableDefects.clear();
			if (ReviewLedgerHost == nullptr) {
				if (Window != nullptr) {
					Window->clear_defects();
				}
				return;
			}
			if (auto privateState = ReviewLedgerHost->private_observation();
				privateState.has_value()) {
				DurableDefects = std::move(privateState->Defects);
			}
			correlate_defects();
			present_defects();
		}

		void classify_defect(std::string aConversation)
		{
			if (Closing || ReviewLedgerHost == nullptr
				|| !SelectedDiffPublication.has_value()) {
				return;
			}
			const auto* const thread = accepted_thread(aConversation);
			if (thread == nullptr
				|| thread->Subject
					!= application::github_review_thread_subject::line
				|| !thread->Line.has_value() || thread->Path.empty()
				|| core::file_identity_of(thread->Path)
					!= SelectedDiffPublication->Request.File.File) {
				return;
			}
			const auto identity = defect_identity(aConversation);
			if (std::ranges::any_of(
					DurableDefects,
					[identity](const application::durable_defect_record& aDefect) {
						return aDefect.Conversation.value() == identity.Conversation
							&& aDefect.History.Defect.Id.value() == identity.Defect;
					})) {
				return;
			}
			const auto anchor = core::source_line_ref{
				.Comparison = core::comparison_digest(SelectedDiffPublication->Key),
				.Side       = core::diff_side::right,
				.Line       = *thread->Line,
			};
			auto durable = application::durable_defect_record{
				.File         = SelectedDiffPublication->Request.File.File,
				.Conversation = core::review_conversation_id{ identity.Conversation },
				.History      = core::defect_history{
					.Defect = core::defect_record{
						.Id = core::defect_id{ identity.Defect }, .Anchor = anchor },
					.OriginalAnchor = anchor,
				},
			};
			if (!ReviewLedgerHost->persist(durable)) {
				return;
			}
			DurableDefects.push_back(std::move(durable));
			correlate_defects();
		}

		void dispatch_defect(core::defect_command aCommand)
		{
			if (Closing || ReviewLedgerHost == nullptr) {
				return;
			}
			const auto found = std::ranges::find(
				DurableDefects,
				aCommand.Id,
				[](const application::durable_defect_record& aDefect) {
					return aDefect.History.Defect.Id;
				});
			if (found == DurableDefects.end()) {
				return;
			}
			const auto outcome = core::apply(found->History.Defect, aCommand);
			if (outcome.Rejection != core::defect_rejection::none
				|| !outcome.Changed) {
				return;
			}
			auto changed           = *found;
			changed.History.Defect = outcome.Defect;
			if (!ReviewLedgerHost->persist(changed)) {
				return;
			}
			*found = std::move(changed);
			present_defects();
		}

		[[nodiscard]] static auto review_mutation_target(
			const application::accepted_pull_request_handoff& aHandoff)
			-> core::review_mutation_target
		{
			return {
				.Host         = aHandoff.Host,
				.Repository   = aHandoff.Repository,
				.PullRequest  = aHandoff.PullRequest,
				.ExpectedHead = aHandoff.Head,
			};
		}

		void compose_review_mutation(
			const application::accepted_pull_request_handoff& aHandoff)
		{
			ReviewMutationCoordinator.reset();
			ReviewDraft.reset();
			ReviewProjection.clear();
#if defined(BPR_BUILD_TESTING)
			if (Fixture == nullptr || Window == nullptr
				|| Window->review_editor() == nullptr) {
				return;
			}
			const auto target = review_mutation_target(aHandoff);
			Fixture->prepare_review_mutation(target);
			ReviewMutationCoordinator = std::make_unique<
				application::review_mutation_coordinator>(
				Fixture->review_mutation_provider(), target, aHandoff.Account);
			auto* const editor = Window->review_editor();
			editor->describe_target(QStringLiteral("%1/%2#%3 at %4 as account %5")
										.arg(QString::fromStdString(aHandoff.Locator.Owner))
										.arg(QString::fromStdString(aHandoff.Locator.Name))
										.arg(aHandoff.Number.value())
										.arg(QString::fromUtf8(
											core::canonical_hex(aHandoff.Head).view()))
										.arg(aHandoff.Account.value()));
			editor->present(ReviewMutationCoordinator->capabilities());
#else
			static_cast<void>(aHandoff);
#endif
		}

		void refresh_review_projection()
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture == nullptr) {
				return;
			}
			const auto observed  = Fixture->review_comments();
			ReviewReconciliation = application::reconcile(
				ReviewProjection, observed, { });
			ReviewProjection = ReviewReconciliation.Projection;
#endif
		}

		void dispatch_review_draft(core::review_draft_command aCommand)
		{
			if (Closing || !ReviewDraft.has_value()
				|| ReviewMutationCoordinator == nullptr || Window == nullptr
				|| Window->review_editor() == nullptr) {
				return;
			}
			const auto outcome = core::apply(*ReviewDraft, aCommand);
			if (outcome.Rejection != core::review_draft_rejection::none) {
				return;
			}
			ReviewDraft        = outcome.Draft;
			auto* const editor = Window->review_editor();
			editor->present(*ReviewDraft);
			if (aCommand.Kind != core::review_draft_command_kind::begin_publish) {
				return;
			}
			const auto proposal = core::propose_publication(
				*ReviewDraft, core::review_disposition::comment);
			if (!proposal.has_value()) {
				return;
			}
			const auto publication = ReviewMutationCoordinator->publish(*proposal, false);
			refresh_review_projection();
			if (!publication.has_value()) {
				ReviewDraft = core::apply(
								  *ReviewDraft,
								  core::review_draft_command{
									  .Id   = ReviewDraft->Id,
									  .Kind = core::review_draft_command_kind::fail_publish,
								  })
								  .Draft;
				editor->present(*ReviewDraft);
				editor->present(publication.error());
				return;
			}
			ReviewDraft = core::apply(
							  *ReviewDraft,
							  core::review_draft_command{
								  .Id   = ReviewDraft->Id,
								  .Kind = core::review_draft_command_kind::complete_publish,
							  })
							  .Draft;
			editor->present(*ReviewDraft);
			editor->present(*publication, ReviewReconciliation);
		}

		void present_review_draft(const core::file_comparison_digest aComparison)
		{
			if (ReviewMutationCoordinator == nullptr || Window == nullptr
				|| Window->review_editor() == nullptr || !aComparison.valid()) {
				return;
			}
			if (ReviewDraft.has_value()
				&& ReviewDraft->Anchor.Comparison == aComparison
				&& (ReviewDraft->State == core::review_draft_state::drafting
					|| ReviewDraft->State == core::review_draft_state::publishing)) {
				return;
			}
			ReviewDraft = core::review_comment_draft{
				.Id     = core::review_draft_id{ NextReviewDraft++ },
				.Anchor = {
					.Comparison = aComparison,
					.Side       = core::diff_side::right,
					.Line       = 1U,
				},
			};
			Window->review_editor()->present(*ReviewDraft);
		}

		void retire_review_ledger()
		{
			DurableDefects.clear();
			ReviewLedgerHost.reset();
			ReviewLedger.reset();
			{
				const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
				ReviewLedgerAuthority.reset();
				ReviewLedgerReceipt.reset();
				ReviewLedgerAvailable = false;
			}
			if (Window != nullptr) {
				Window->clear_review_ledger();
				Window->clear_defects();
			}
		}

		void compose_review_ledger(
			const application::accepted_pull_request_handoff& aHandoff)
		{
			retire_review_ledger();
			const auto repository
				= aHandoff.Locator.Owner + "/" + aHandoff.Locator.Name;
			auto       opened    = adapters::sqlite::make_review_ledger_store({
				.StorageRoot = ApplicationDataRoot,
				.Host        = std::string{ aHandoff.Hostname.value() },
				.Account     = std::to_string(aHandoff.Account.value()),
				.Repository  = repository,
				.PullRequest = static_cast<std::int64_t>(aHandoff.Number.value()),
			});
			const auto available = opened.has_value();
			if (available) {
				ReviewLedger = std::move(*opened);
			} else {
				ReviewLedger = std::make_unique<unavailable_review_ledger_store>(
					opened.error());
			}
			ReviewLedgerHost = std::make_unique<
				application::review_ledger_application_host>(
				*ReviewLedger,
				*this);
			{
				const auto lock       = std::scoped_lock{ ReviewLedgerStateMutex };
				ReviewLedgerAuthority = workbench_authority(aHandoff);
				ReviewLedgerAvailable = available;
			}
			if (Window != nullptr) {
				Window->clear_review_ledger(!available);
			}
		}

		[[nodiscard]] auto matches_review_ledger(
			const application::pull_request_authority_invalidation& aInvalidation) const
			-> bool
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (!ReviewLedgerAuthority.has_value()) {
				return false;
			}
			const auto& authority = *ReviewLedgerAuthority;
			return authority.ApplicationSession == aInvalidation.ApplicationSession
				&& authority.Profile == aInvalidation.Profile
				&& authority.ProfileRevision == aInvalidation.ProfileRevision
				&& authority.Host == aInvalidation.Host
				&& authority.HostAuthority == aInvalidation.HostAuthority
				&& authority.Account == aInvalidation.Account
				&& authority.Viewer == aInvalidation.Viewer
				&& authority.Repository == aInvalidation.Repository
				&& authority.PullRequest == aInvalidation.PullRequest
				&& authority.IdentityGeneration
				== aInvalidation.IdentityGeneration;
		}

		void observe_selected_material(
			const application::diff_publication& aPublication)
		{
			if (Closing || ReviewLedgerHost == nullptr
				|| !SelectedDiffPublication.has_value()
				|| aPublication.Result == nullptr
				|| aPublication.Context.Key != SelectedDiffPublication->Key
				|| aPublication.Result->Alignment.Key
					!= SelectedDiffPublication->Key
				|| WorkbenchHost == nullptr
				|| WorkbenchHost->generation()
					!= SelectedDiffPublication->Request.Context.Generation
				|| WorkbenchHost->selected()
					!= std::optional{
						SelectedDiffPublication->Request.File.File }) {
				return;
			}
			{
				const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
				if (!ReviewLedgerAuthority.has_value()
					|| *ReviewLedgerAuthority
						!= SelectedDiffPublication->Request.WorkbenchAuthority) {
					return;
				}
			}
			const auto material = core::comparison_digest(
				SelectedDiffPublication->Key);
			if (!material.valid()) {
				return;
			}
			static_cast<void>(ReviewLedgerHost->observe_material(
				core::file_material_observation{
					.File     = SelectedDiffPublication->Request.File.File,
					.Material = material,
				}));
			present_review_draft(material);
		}

		void publish(const application::receipt_projection& aProjection) override
		{
			auto authority = std::optional<
				application::workbench_pull_request_authority>{ };
			auto actionabilityChanged = false;
			{
				const auto lock          = std::scoped_lock{ ReviewLedgerStateMutex };
				const auto nowActionable = core::actionable(core::reason_for(
					core::actionability_input{ .Receipt = aProjection.Receipt }));
				actionabilityChanged     = ReviewLedgerReceipt.has_value()
					? ReviewLedgerReceipt->File != aProjection.Receipt.File
						|| core::actionable(core::reason_for(core::actionability_input{
							   .Receipt = *ReviewLedgerReceipt,
						   }))
							!= nowActionable
					: !nowActionable;
				ReviewLedgerReceipt      = aProjection.Receipt;
				authority                = ReviewLedgerAuthority;
			}
			if (Window != nullptr) {
				Window->publish(application::project_file_receipt_actions(
					aProjection.Receipt));
			}
			if (WorkbenchHost != nullptr && authority.has_value()
				&& actionabilityChanged) {
				WorkbenchHost->refresh_actionability();
			}
		}

		void fail(
			const application::receipt_failure_publication& aFailure) override
		{
			if (Window != nullptr) {
				Window->fail(aFailure);
			}
		}

		void accept(application::accepted_pull_request_handoff aHandoff) override
		{
			if (!Closing && EpochHost != nullptr) {
				const auto authority = workbench_authority(aHandoff);
				const auto snapshot  = Host != nullptr
					? Host->accepted_snapshot()
					: std::optional<application::github_pull_request_snapshot>{ };
				compose_review_ledger(aHandoff);
				compose_review_mutation(aHandoff);
				WorkbenchReview.adopt(
					aHandoff,
					snapshot);
				if (snapshot.has_value() && QueueHost != nullptr
					&& QueueRefresh != nullptr) {
					auto observation = queue_observation_for(aHandoff, *snapshot);
					auto session     = application::queue_session_id{
						.Host       = observation.Ref.Host,
						.Account    = observation.Ref.Account,
						.Generation = aHandoff.IdentityGeneration.value(),
					};
					QueueHost->adopt(session);
					QueueRefresh->retain(
						std::move(session),
						observation);
					QueueHost->refresh(
						observation.Ref,
						core::queue_activity_observation{
							.ObservedActivity = true,
						});
				}
				load_defects();
#if defined(BPR_BUILD_TESTING)
				LastAcceptedHandoff = clone_without_smart_git(aHandoff);
#endif
				if (!preserves_selected_diff(EpochHost->state(), aHandoff)) {
					clear_diff_selection();
				}
				if (WorkbenchHost != nullptr) {
					WorkbenchHost->observe(authority);
				}
				EpochHost->accept(std::move(aHandoff));
			}
		}

		void invalidate(
			application::pull_request_authority_invalidation aInvalidation) override
		{
			if (Closing || EpochHost == nullptr) {
				return;
			}
			if (QueueHost != nullptr) {
				QueueHost->cancel();
			}
			if (QueueRefresh != nullptr) {
				QueueRefresh->clear();
			}
			const auto retiresLedger = matches_review_ledger(aInvalidation);
			if (auto authority = WorkbenchReview.invalidate(aInvalidation);
				authority.has_value() && WorkbenchHost != nullptr) {
#if defined(BPR_BUILD_TESTING)
				++WorkbenchInvalidations;
#endif
				WorkbenchHost->invalidate(*authority);
			}
			invalidate_diff_if([&aInvalidation](const auto& aAuthority) {
				return aAuthority.Profile == aInvalidation.Profile
					&& aAuthority.ProfileRevision == aInvalidation.ProfileRevision
					&& aAuthority.Host == aInvalidation.Host
					&& aAuthority.HostAuthority == aInvalidation.HostAuthority
					&& aAuthority.Account == aInvalidation.Account
					&& aAuthority.Viewer == aInvalidation.Viewer
					&& aAuthority.Repository == aInvalidation.Repository
					&& aAuthority.PullRequest == aInvalidation.PullRequest;
			});
			if (retiresLedger) {
				retire_review_ledger();
				ReviewMutationCoordinator.reset();
				ReviewDraft.reset();
				ReviewProjection.clear();
			}
			EpochHost->invalidate(std::move(aInvalidation));
		}

		[[nodiscard]] auto find_profile(const core::host_profile_id aProfile) const
			-> const validated_github_host_profile*
		{
			const auto found = std::ranges::find_if(
				Profiles,
				[aProfile](const validated_github_host_profile& aCandidate) { return aCandidate.profile() == aProfile; });
			return found == Profiles.end() ? nullptr : &*found;
		}

		void dispatch_intent(application::github_intent aIntent)
		{
			if (Closing || Host == nullptr) {
				return;
			}
			std::visit(
				[this](auto aValue) {
					using intent_type = decltype(aValue);
					if constexpr (std::is_same_v<
									  intent_type,
									  core::activate_host_profile_intent>) {
						const auto* profile = find_profile(aValue.Profile);
						if (profile == nullptr) {
							return;
						}
						if (profile->authentication_transport()
							== application::github_authentication_transport::
								github_cli) {
							Host->dispatch(core::github_event{ aValue });
							return;
						}

						auto credential = std::optional<
							application::github_credential_material>{ };
#if defined(BPR_BUILD_TESTING)
						if (Fixture != nullptr) {
							credential.emplace(
								Fixture->take_enrollment_credential());
						} else
#endif
						{
							credential = prompt_for_credential(Window.get());
						}
						if (credential.has_value()) {
							Host->enroll_profile(
								aValue.Profile, std::move(*credential));
						}
					} else {
						Host->dispatch(core::github_event{ std::move(aValue) });
					}
				},
				std::move(aIntent));
		}

		void dispatch_epoch_intent(application::epoch_intent aIntent)
		{
			if (Closing || EpochHost == nullptr) {
				return;
			}
#if defined(BPR_BUILD_TESTING)
			if (std::holds_alternative<core::prune_review_epochs_intent>(aIntent)) {
				++EpochPruneIntentDispatchCount;
			} else if (std::holds_alternative<core::delete_repository_cache_intent>(
						   aIntent)) {
				++EpochDeleteIntentDispatchCount;
			} else if (std::holds_alternative<core::cancel_epoch_maintenance_intent>(
						   aIntent)) {
				++EpochCancelIntentDispatchCount;
			}
#endif
			if (
				std::holds_alternative<core::select_named_comparison_intent>(aIntent)
				|| std::holds_alternative<core::select_comparison_anchors_intent>(aIntent)
				|| std::holds_alternative<core::select_comparison_anchor_intent>(aIntent)) {
				clear_diff_selection();
			}
			if (const auto* deletion = std::get_if<core::delete_repository_cache_intent>(&aIntent)) {
				invalidate_diff_if([deletion](const auto& aAuthority) {
					return aAuthority.Cache == deletion->Cache;
				});
			}
			EpochHost->dispatch(std::move(aIntent));
		}

		void publish(
			const application::epoch_presentation_update& aUpdate) override
		{
			if (Closing || Window == nullptr) {
				return;
			}
			Window->publish(aUpdate);
			const auto* comparison = aUpdate.Transition.has_value()
					&& aUpdate.Transition->Comparison.has_value()
				? &*aUpdate.Transition->Comparison
				: nullptr;
			if (WorkbenchHost != nullptr && comparison != nullptr
				&& comparison->Pair.has_value()
				&& comparison->AvailableForDiff) {
				WorkbenchHost->refresh_selected_diff();
			}
		}

		void clear_diff_selection()
		{
			SelectedDiffPublication.reset();
			if (DiffHost != nullptr) {
				DiffHost->cancel();
			}
			if (SyntaxBlobs != nullptr) {
				// A classification still in flight now has no comparison to
				// read for. Dropping the context makes it report unavailable
				// rather than materialize bytes for a file the reviewer has
				// left.
				SyntaxBlobs->clear_comparison();
			}
			if (Window != nullptr) {
				Window->comparison_surface()->clear();
			}
		}

		void begin(
			const application::workbench_selected_diff_request& aRequest) override
		{
			if (Closing || EpochHost == nullptr
				|| EpochHost->context(aRequest.WorkbenchAuthority)
					!= std::optional{ aRequest.Diff }) {
				return;
			}
			clear_diff_selection();
			if (Window != nullptr) {
				const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
				Window->clear_review_ledger(!ReviewLedgerAvailable);
			}
#if defined(BPR_BUILD_TESTING)
			++SelectedDiffBegins;
#endif
		}

		void publish(
			const application::workbench_selected_diff_publication& aPublication) override
		{
			if (Closing || EpochHost == nullptr || WorkbenchHost == nullptr
				|| WorkbenchHost->generation()
					!= aPublication.Request.Context.Generation
				|| WorkbenchHost->selected()
					!= std::optional{ aPublication.Request.File.File }
				|| EpochHost->context(aPublication.Request.WorkbenchAuthority)
					!= std::optional{ aPublication.Request.Diff }) {
				return;
			}
			SelectedDiffPublication = aPublication;
			request_diff(
				aPublication.Key,
				aPublication.Request.Diff.Options,
				aPublication.Request.File.PresentationPath);
#if defined(BPR_BUILD_TESTING)
			++SelectedDiffPublications;
			LastSelectedDiff = aPublication;
#endif
		}

		void fail(
			const application::workbench_selected_diff_observation& aFailure) override
		{
			if (Closing || Window == nullptr) {
				return;
			}
			const auto failure = [&aFailure] {
				switch (aFailure.Status) {
				case application::workbench_selected_diff_status::binary:
					return application::workbench_host_failure::selected_file_binary;
				case application::workbench_selected_diff_status::missing_left:
					return application::workbench_host_failure::selected_file_missing_left;
				case application::workbench_selected_diff_status::missing_right:
					return application::workbench_host_failure::selected_file_missing_right;
				case application::workbench_selected_diff_status::both_missing:
					return application::workbench_host_failure::selected_file_missing_both;
				case application::workbench_selected_diff_status::ambiguous_left:
				case application::workbench_selected_diff_status::ambiguous_right:
				case application::workbench_selected_diff_status::ambiguous_both:
					return application::workbench_host_failure::selected_file_ambiguous;
				case application::workbench_selected_diff_status::cancelled:
					return application::workbench_host_failure::selected_file_cancelled;
				case application::workbench_selected_diff_status::stale:
					return application::workbench_host_failure::selected_file_stale;
				case application::workbench_selected_diff_status::available:
				case application::workbench_selected_diff_status::failure:
					return application::workbench_host_failure::host_failure;
				}
				return application::workbench_host_failure::host_failure;
			}();
			Window->fail(application::workbench_failure_publication{
				.Context = aFailure.Request.Context,
				.Failure = failure,
			});
#if defined(BPR_BUILD_TESTING)
			++SelectedDiffFailures;
#endif
		}

		template <typename Predicate>
		void invalidate_diff_if(Predicate&& aPredicate)
		{
			if (DiffHost == nullptr) {
				return;
			}
			const auto& state   = DiffHost->state();
			const auto* context = state.Pending.has_value()
				? &*state.Pending
				: state.Current.has_value() ? &*state.Current
											: nullptr;
			if (context != nullptr && aPredicate(context->Key.Authority)) {
				DiffHost->invalidate(context->Key.Authority);
			}
		}

		/// \brief Requests one comparison and the classification of its blobs.
		///
		/// \details The path is not part of the comparison key and deliberately
		/// stays out of it: the key identifies a file by digest, and a second
		/// spelling of the same file inside it would be a second identity. It
		/// is carried alongside because choosing a syntax definition is the one
		/// thing that needs the name a file was given.
		void request_diff(
			core::file_comparison_key aKey,
			core::diff_options        aOptions,
			const std::string_view    aPath)
		{
			if (Closing || DiffHost == nullptr) {
				return;
			}
			DiffHost->request(std::move(aKey), std::move(aOptions));
			if (!DiffHost->state().Pending.has_value()) {
				return;
			}
			const auto pending = *DiffHost->state().Pending;
			// Clears the previous comparison's spans before the new ones are
			// asked for, so a classification that lands first is not thrown
			// away by the surface it was meant for.
			Window->comparison_surface()->set_loading(pending);
			classify_comparison(pending, aPath);
		}

		/// \brief Asks the appearance host to classify both sides of one
		/// comparison.
		///
		/// \details Nothing here classifies. The host answers from its cache
		/// when the blob and definition are already known and reaches the
		/// classifier only when they are not, which is the whole reason the
		/// cache key carries no colour.
		void classify_comparison(
			const core::diff_request_context& aContext,
			const std::string_view            aPath)
		{
			if (AppearanceHost == nullptr) {
				return;
			}
			SyntaxBlobs->set_comparison(aContext);
			const auto definition = SyntaxDefinitions->definition_for_path(aPath);
			for (const auto& blob :
				 { aContext.Key.LeftBlob, aContext.Key.RightBlob }) {
				if (!blob.has_value()) {
					continue;
				}
				AppearanceHost->classify(application::token_span_cache_key{
					.Blob       = *blob,
					.Definition = definition,
				});
			}
		}

		/// \brief Hands one published classification to the side it describes.
		///
		/// \details The side is decided by blob identity, which is the only
		/// thing that can decide it: a classification names a blob, and a
		/// comparison is the only thing that knows which side that blob is on.
		/// A classification for neither side belongs to a comparison the
		/// reviewer has already left, and is dropped.
		void adopt_classification(
			const application::token_span_publication& aSpans)
		{
			if (Closing || Window == nullptr || DiffHost == nullptr) {
				return;
			}
			const auto& state   = DiffHost->state();
			const auto* context = state.Current.has_value()
				? &*state.Current
				: state.Pending.has_value() ? &*state.Pending
											: nullptr;
			if (context == nullptr) {
				return;
			}
			const auto side = context->Key.LeftBlob == std::optional{ aSpans.Key.Blob }
				? std::optional{ core::diff_side::left }
				: context->Key.RightBlob == std::optional{ aSpans.Key.Blob }
				? std::optional{ core::diff_side::right }
				: std::nullopt;
			if (!side.has_value() || !aSpans.Spans) {
				return;
			}
			if (Window->comparison_surface()->adopt_token_spans(
					*side, aSpans.Spans)) {
				++AdoptedClassifications;
			}
		}

		/// \brief Reads the operating system's appearance as a bounded fact.
		[[nodiscard]] static auto native_appearance_observation() noexcept
			-> core::appearance_observation
		{
			auto* const hints = QGuiApplication::styleHints();
			if (hints == nullptr) {
				return { };
			}
			auto observation = core::appearance_observation{ };
			switch (hints->colorScheme()) {
			case Qt::ColorScheme::Light:
				observation.Scheme = core::os_color_scheme::light;
				break;
			case Qt::ColorScheme::Dark:
				observation.Scheme = core::os_color_scheme::dark;
				break;
			case Qt::ColorScheme::Unknown:
				observation.Scheme = core::os_color_scheme::unknown;
				break;
			}
			return observation;
		}

		/// \brief Reports a changed operating-system appearance to both owners.
		void observe_native_appearance()
		{
			if (Closing || !AppearanceProfile) {
				return;
			}
			const auto observation = native_appearance_observation();
			// Both, because they own different things: the host records the
			// observation against its durable settings, and the profile
			// re-resolves what presentation should look like. Telling only one
			// leaves them disagreeing about the same fact.
			if (AppearanceHost) {
				AppearanceHost->observe(observation);
			}
			AppearanceProfile->observe(observation);
		}

		/// \brief Applies the resolved appearance to the composed window.
		/// \brief Routes one settings gesture to the appearance host.
		///
		/// \details The host decides. It validates, refuses whole, persists
		/// what it accepted, and publishes; nothing here judges a value or
		/// touches presentation, so a gesture cannot change the screen without
		/// changing the record.
		void dispatch_appearance_intent(application::appearance_intent aIntent)
		{
			if (Closing || AppearanceHost == nullptr) {
				return;
			}
			std::visit(
				[this](auto&& aValue) {
					using intent = std::decay_t<decltype(aValue)>;
					if constexpr (std::is_same_v<
									  intent,
									  application::select_appearance_mode_intent>) {
						AppearanceHost->select(aValue.Mode);
					} else if constexpr (std::is_same_v<
											 intent,
											 application::apply_typography_intent>) {
						AppearanceHost->apply(std::move(aValue.Typography));
					} else if constexpr (std::is_same_v<
											 intent,
											 application::preview_appearance_palette_intent>) {
						if (AppearanceProfile) {
							AppearanceProfile->preview_palette(std::move(aValue.Palette));
						}
					} else if constexpr (std::is_same_v<
											 intent,
											 application::commit_appearance_palette_intent>) {
						AppearanceHost->apply(std::move(aValue.Palette));
					} else if constexpr (std::is_same_v<
											 intent,
											 application::cancel_appearance_palette_preview_intent>) {
						if (AppearanceProfile) {
							AppearanceProfile->cancel_palette_preview();
						}
					} else {
						AppearanceHost->import_theme_file(std::move(aValue.Path));
					}
				},
				std::move(aIntent));
		}

		/// \brief Puts the host's accepted settings into presentation.
		///
		/// \details Read off the host rather than off the intent that caused
		/// the change: an intent is what was asked for, and the host is the
		/// only thing that knows what was accepted. Offering the intent to
		/// presentation would show a refused value as though it had applied.
		///
		void adopt_appearance_settings(const bool aPaletteCommitted = false)
		{
			if (Closing || AppearanceHost == nullptr || !AppearanceProfile) {
				return;
			}
			const auto settings = AppearanceHost->settings();
			static_cast<void>(AppearanceProfile->select_mode(settings.Mode));
			if (settings.PaletteStored) {
				static_cast<void>(AppearanceProfile->adopt_palette(
					settings.Palette,
					aPaletteCommitted ? qt::palette_preview_adoption::commit
									  : qt::palette_preview_adoption::retain));
			}
			const auto selection
				= AppearanceProfile->select_typography(settings.Typography);
			if (!selection.accepted()) {
				// The host validated with the same core function, so this
				// cannot happen without the two disagreeing about legality.
				// Counted rather than ignored, because a silent divergence
				// here is exactly the kind of thing that stays invisible.
				++AppearanceDivergences;
			}
			if (Window != nullptr) {
				if (auto* const panel = Window->appearance_settings();
					panel != nullptr) {
					auto activePreview
						= std::optional<core::appearance_palette>{ };
					if (AppearanceProfile->previewing_palette()) {
						activePreview = AppearanceProfile->palette();
					}
					panel->adopt(
						settings,
						AppearanceProfile->settings().Palette,
						std::move(activePreview));
				}
			}
		}

		void preview_imported_theme(const core::appearance_palette& aPalette)
		{
			if (Closing || !AppearanceProfile || Window == nullptr) {
				return;
			}
			AppearanceProfile->preview_palette(aPalette);
			if (auto* const panel = Window->appearance_settings(); panel != nullptr) {
				panel->preview_import(aPalette);
			}
		}

		void report_appearance_refusal(
			const application::appearance_failure_publication& aFailure)
		{
			if (Closing || Window == nullptr) {
				return;
			}
			if (auto* const panel = Window->appearance_settings();
				panel != nullptr) {
				switch (aFailure.Failure) {
				case application::appearance_host_failure::settings_unavailable:
					if (!AppearancePersistenceAvailable) {
						panel->report_persistence_unavailable();
					} else {
						panel->report(std::span<const core::typography_rejection>{ });
					}
					break;
				case application::appearance_host_failure::settings_rejected:
					panel->report(aFailure.Rejections);
					break;
				case application::appearance_host_failure::theme_document_unavailable:
					panel->report_theme_document_unavailable();
					break;
				case application::appearance_host_failure::theme_import_rejected:
					panel->report(aFailure.ThemeRejections);
					break;
				case application::appearance_host_failure::palette_contrast_rejected:
					panel->report(aFailure.ContrastRejections);
					break;
				case application::appearance_host_failure::unknown_appearance_mode:
				case application::appearance_host_failure::classification_unavailable:
				case application::appearance_host_failure::classification_limit:
				case application::appearance_host_failure::invalid_classification_key:
				case application::appearance_host_failure::host_failure:
					panel->report(std::span<const core::typography_rejection>{ });
					break;
				}
			}
		}

		void apply_appearance()
		{
			if (Closing || !AppearanceProfile || Window == nullptr) {
				return;
			}
			// Applied to the window, so every composed surface inherits it.
			// `derive` maps the core's semantic roles onto Qt's palette roles,
			// which is how `code_selection_background` becomes
			// `QPalette::Highlight` - the selection colour BPR-0117 fitted for
			// contrast and which, until now, nothing applied.
			AppearanceProfile->apply_to(*Window);
			// QPalette carries no role for "an added line", so the comparison
			// surface is given the semantic palette directly. Without this the
			// panes paint no line backgrounds at all, which is visibly wrong
			// rather than quietly wrong.
			if (auto* const workbench = Window->comparison_surface();
				workbench != nullptr) {
				workbench->adopt_appearance(*AppearanceProfile);
			}

			// Read back off the window rather than off the profile. The claim
			// is that the palette reached the window; asking the profile what
			// it would have applied proves only that it decided something.
			const auto& applied   = Window->palette();
			AppliedSelectionColor = applied.color(QPalette::Highlight).name().toStdString();
			AppliedBaseColor      = applied.color(QPalette::Base).name().toStdString();

			// And whether those colours actually came from the profile. Without
			// this the evidence is only that *some* colours were reported, which
			// stays true when the window keeps the platform's own palette - a
			// break that removed the application entirely still produced
			// well-formed output. Compared against the profile's semantic roles,
			// so the answer is no when presentation is still the platform's.
			AppliedFromProfile = applied.color(QPalette::Highlight)
					== AppearanceProfile->color(
						core::appearance_role::code_selection_background)
				&& applied.color(QPalette::Base)
					== AppearanceProfile->color(
						core::appearance_role::code_background);
		}

		void begin_shutdown()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			// Every composed host must be told, or the shutdown poll waits on a
			// host that was never asked to stop and the application never
			// exits. Adding a host to the composition without adding it here is
			// a hang, not a leak, and it presents as a test timeout rather than
			// as anything that names the cause.
			if (WorkbenchHost) {
				WorkbenchHost->shutdown();
			}
			if (QueueHost) {
				QueueHost->shutdown();
			}
			if (AppearanceHost) {
				AppearanceHost->shutdown();
			}
			DiffHost->shutdown();
			EpochHost->shutdown();
			Host->shutdown();
			schedule_shutdown_poll();
		}

		void schedule_shutdown_poll()
		{
			if (ShutdownComplete || ShutdownPollScheduled || Window == nullptr) {
				return;
			}
			ShutdownPollScheduled = true;
			QTimer::singleShot(
				0,
				Window.get(),
				[this] {
					ShutdownPollScheduled = false;
					poll_shutdown();
				});
		}

		void poll_shutdown()
		{
			if (!Closing || ShutdownComplete) {
				return;
			}
			if (
				!DiffHost->is_quiescent() || !EpochHost->is_quiescent()
				|| !Host->is_quiescent()
				|| (WorkbenchHost && !WorkbenchHost->is_quiescent())
				|| (QueueHost && !QueueHost->is_quiescent())
				|| (AppearanceHost && !AppearanceHost->is_quiescent())) {
				schedule_shutdown_poll();
				return;
			}

			ShutdownCleanupFailed         = Host->shutdown_cleanup_failed();
			EpochProjectionDeliveryFailed = EpochHost->projection_delivery_failed();
			AppearanceEffects.close_and_drain();
			QueueEffects.close_and_drain();
			WorkbenchEffects.close_and_drain();
			DiffEffects.close_and_drain();
			EpochEffects.close_and_drain();
			Provider->shutdown();
			ProviderEffects.close_and_drain();
			VaultEffects.close_and_drain();
			ShutdownComplete = true;
		}

		std::deque<validated_github_host_profile>       Profiles;
		std::vector<application::github_profile_option> ProfileOptions;
		worker_executor                                 ProviderEffects;
		worker_executor                                 VaultEffects;
#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<detail::github_public_path_fixture_components> Fixture;
		std::optional<application::accepted_pull_request_handoff>      LastAcceptedHandoff;
		std::size_t                                                    EpochPruneIntentDispatchCount  = 0U;
		std::size_t                                                    EpochDeleteIntentDispatchCount = 0U;
		std::size_t                                                    EpochCancelIntentDispatchCount = 0U;
		struct appearance_theme_probe_state
		{
			enum class phase {
				awaiting_stable_comparison,
				awaiting_manual_preview,
				awaiting_preview_adoption,
				awaiting_preview_restoration,
				awaiting_first_preview,
				awaiting_second_preview,
				awaiting_store,
				awaiting_stored_preview,
				awaiting_stored_preview_adoption,
				awaiting_stored_preview_restoration,
				awaiting_equal_stored_preview,
				awaiting_equal_stored_commit,
				passed,
				failed,
			};

			phase                                           Phase = phase::awaiting_stable_comparison;
			std::string                                     Path;
			application::appearance_settings                BeforeSettings;
			core::appearance_palette                        BeforePalette{ };
			core::appearance_palette                        ManualCandidate{ };
			core::appearance_palette                        Candidate{ };
			core::appearance_palette                        StoredCandidate{ };
			core::appearance_observation                    NativeObservation;
			std::shared_ptr<const core::computed_file_diff> Result;
			std::optional<core::diff_request_context>       Context;
			std::optional<core::semantic_selection>         Selection;
			application::shared_token_spans                 LeftSpans;
			application::shared_token_spans                 RightSpans;
			qt::diff_workbench_statistics                   Statistics;
			double                                          Position                  = 0.0;
			std::uint64_t                                   ProfileGeneration         = 0U;
			std::size_t                                     ClassificationSubmissions = 0U;
			std::size_t                                     PersistedSettings         = 0U;
		};
		std::optional<appearance_theme_probe_state> AppearanceThemeProbe;
		struct workbench_composition_probe_state
		{
			enum class phase {
				awaiting_observation,
				awaiting_file_projection,
				awaiting_diff,
				awaiting_change_projection,
				awaiting_width_store,
				awaiting_invalidation,
				awaiting_failure,
				passed,
				failed,
			};

			phase                                         Phase = phase::awaiting_observation;
			detail::workbench_composition_probe_evidence  Evidence;
			application::workbench_pull_request_authority Authority;
			std::vector<core::workbench_file_id>          ExpectedFiles;
			std::optional<core::semantic_selection>       Selection;
			qt::workbench_window_statistics               WindowStatistics;
			core::request_generation                      Generation;
			std::size_t                                   IntentDispatches         = 0U;
			std::size_t                                   ChangeDispatches         = 0U;
			std::size_t                                   SelectedDiffBegins       = 0U;
			std::size_t                                   SelectedDiffPublications = 0U;
			std::size_t                                   SelectedDiffFailures     = 0U;
			std::size_t                                   WidthDispatches          = 0U;
			std::size_t                                   Invalidations            = 0U;
			std::size_t                                   PersistedLayouts         = 0U;
			std::uint32_t                                 InitialFilesWidth        = 0U;
		};
		std::optional<workbench_composition_probe_state> WorkbenchCompositionProbe;
		struct review_ledger_composition_probe_state
		{
			enum class phase {
				awaiting_observation,
				awaiting_diff,
				awaiting_actionability,
				awaiting_material_actionability,
				passed,
				failed,
			};

			phase                                   Phase = phase::awaiting_observation;
			detail::review_ledger_composition_probe Course
				= detail::review_ledger_composition_probe::seed;
			detail::review_ledger_composition_probe_evidence Evidence;
			application::review_ledger_host_statistics       InitialStatistics;
			core::file_identity                              File;
			core::file_material_id                           Material;
			core::file_material_id                           ReopenedMaterial;
		};
		std::optional<review_ledger_composition_probe_state>
			ReviewLedgerCompositionProbe;
		struct review_round_trip_composition_probe_state
		{
			detail::review_round_trip_composition_probe_evidence Evidence;
			bool                                                 Complete = false;
			bool                                                 Failed   = false;
		};
		std::optional<review_round_trip_composition_probe_state>
			ReviewRoundTripCompositionProbe;
		struct defect_composition_probe_state
		{
			enum class phase {
				awaiting_ready,
				awaiting_diff,
				awaiting_reverse_right,
				awaiting_epoch_change,
				awaiting_shared_ready,
				passed,
				failed,
			};
			detail::defect_composition_probe_evidence Evidence;
			detail::defect_composition_probe          Course
				= detail::defect_composition_probe::seed;
			phase                        Phase = phase::awaiting_ready;
			core::file_comparison_digest BeforeComparison;
			int                          ReverseLeftRow  = -1;
			int                          ReverseRightRow = -1;
		};
		std::optional<defect_composition_probe_state> DefectCompositionProbe;
		std::size_t                                   WorkbenchIntentDispatches = 0U;
		std::size_t                                   WorkbenchChangeDispatches = 0U;
		std::size_t                                   WorkbenchWidthDispatches  = 0U;
		std::size_t                                   WorkbenchInvalidations    = 0U;
		std::size_t                                   SelectedDiffBegins        = 0U;
		std::size_t                                   SelectedDiffPublications  = 0U;
		std::size_t                                   SelectedDiffFailures      = 0U;
		std::optional<application::workbench_selected_diff_publication>
					LastSelectedDiff;
		std::size_t FixtureDiffHelperInvocations   = 0U;
		bool        DisableWorkbenchSelectionWire  = false;
		bool        DisableReviewLedgerGestureWire = false;
		bool        ReviewLedgerWasPresentOnEntry  = false;
		bool        ReviewLedgerRoundTripPassed    = false;
#endif
		std::unique_ptr<application::github_credential_vault_port>   OwnedVault;
		std::shared_ptr<github_wire_transport_factory_port>          TransportFactory;
		std::unique_ptr<github_provider>                             Provider;
		qt::event_loop_executor                                      DecisionLane;
		std::unique_ptr<qt::workbench_window>                        Window;
		application::github_credential_binding_catalog               CredentialBindings;
		std::unique_ptr<application::git_cache_port>                 GitCaches;
		std::unique_ptr<application::epoch_catalog_port>             EpochCatalogs;
		std::filesystem::path                                        ApplicationDataRoot;
		std::unique_ptr<application::review_ledger_store_port>       ReviewLedger;
		std::unique_ptr<application::review_ledger_application_host> ReviewLedgerHost;
		mutable std::mutex                                           ReviewLedgerStateMutex;
		std::optional<application::workbench_pull_request_authority> ReviewLedgerAuthority;
		std::optional<core::file_receipt>                            ReviewLedgerReceipt;
		bool                                                         ReviewLedgerAvailable = false;
		std::vector<application::durable_defect_record>              DurableDefects;
		std::unique_ptr<application::review_mutation_coordinator>    ReviewMutationCoordinator;
		std::optional<core::review_comment_draft>                    ReviewDraft;
		std::vector<application::review_comment_projection>          ReviewProjection;
		application::reconciliation_result                           ReviewReconciliation;
		std::uint64_t                                                NextReviewDraft = 1U;
		std::unique_ptr<application::workbench_settings_port>        WorkbenchSettings;
		composition_workbench_review                                 WorkbenchReview;
		std::unique_ptr<application::snapshot_workbench_observation> WorkbenchObservations;
		std::unique_ptr<application::appearance_settings_port>       AppearanceSettings;
		bounded_theme_document_reader                                ThemeDocuments;
		// Built once and shared. The catalog is immutable after construction
		// and the classifier holds no theme, palette, or font, so neither has
		// to be rebuilt when appearance changes.
		std::unique_ptr<adapters::syntax::packaged_definition_catalog>  SyntaxDefinitions;
		std::unique_ptr<composition_blob_source>                        SyntaxBlobs;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>   SyntaxClassification;
		composition_appearance_projections                              AppearanceProjections;
		std::unique_ptr<qt::appearance_profile>                         AppearanceProfile;
		std::string                                                     AppliedSelectionColor;
		std::string                                                     AppliedBaseColor;
		bool                                                            AppliedFromProfile             = false;
		bool                                                            WorkbenchPersistenceAvailable  = false;
		bool                                                            AppearancePersistenceAvailable = false;
		std::unique_ptr<application::epoch_entropy_port>                EpochEntropy;
		std::unique_ptr<application::epoch_clock_port>                  EpochClock;
		worker_executor                                                 EpochEffects;
		worker_executor                                                 DiffEffects;
		worker_executor                                                 WorkbenchEffects;
		worker_executor                                                 AppearanceEffects;
		worker_executor                                                 QueueEffects;
		std::unique_ptr<application::epoch_application_host>            EpochHost;
		std::unique_ptr<composition_diff_projection>                    DiffProjections;
		std::unique_ptr<application::diff_application_host>             DiffHost;
		std::unique_ptr<application::workbench_application_host>        WorkbenchHost;
		std::unique_ptr<application::appearance_application_host>       AppearanceHost;
		std::unique_ptr<application::queue_enrollment_store_port>       QueueStore;
		std::unique_ptr<composition_queue_refresh>                      QueueRefresh;
		std::unique_ptr<application::review_queue_application_host>     QueueHost;
		std::unique_ptr<application::github_application_host>           Host;
		std::optional<application::workbench_selected_diff_publication> SelectedDiffPublication;
		bool                                                            Closing                       = false;
		bool                                                            ShutdownPollScheduled         = false;
		bool                                                            ShutdownComplete              = false;
		bool                                                            ShutdownCleanupFailed         = false;
		bool                                                            NativePlatformVaultSelected   = false;
		bool                                                            EpochProjectionDeliveryFailed = false;
		// Classifications the panes actually took. Counted because "the
		// classifier is linked" and "a pane received its answer" are different
		// claims, and only the second one shows a reviewer anything.
		std::size_t AdoptedClassifications = 0U;
		// Times the presentation profile refused typography the host accepted.
		// Both validate with the same core function, so this must stay zero;
		// it exists so that if it ever does not, something says so.
		std::size_t                                           AppearanceDivergences = 0U;
		std::unique_ptr<qt::system_tray_notification_sink>    NotificationSink;
		std::unique_ptr<application::review_queue_notifier>   Notifications;
		std::optional<application::queue_notification_target> ActivatedNotification;
	};

	github_application::github_application(github_application_config aConfig)
		: Implementation{
			std::make_unique<implementation>(std::move(aConfig))
		}
	{
	}

	github_application::github_application(
		std::unique_ptr<implementation> aImplementation) noexcept
		: Implementation{ std::move(aImplementation) }
	{
	}

	github_application::~github_application()
	{
		// Before any member is destroyed, so a callback delivered by the pump
		// inside `~implementation` sees an expired token rather than a null
		// `Implementation`.
		Liveness.reset();
	}

	auto github_application::liveness() const noexcept -> std::weak_ptr<const void>
	{
		return Liveness;
	}

	void github_application::show()
	{
		Implementation->Window->show();
	}

	void github_application::begin_shutdown()
	{
		Implementation->begin_shutdown();
	}

	auto github_application::window() noexcept -> qt::workbench_window&
	{
		return *Implementation->Window;
	}

	auto github_application::status() const noexcept -> github_application_status
	{
		return {
			.Closing                        = Implementation->Closing,
			.ShutdownComplete               = Implementation->ShutdownComplete,
			.ShutdownCleanupFailed          = Implementation->ShutdownCleanupFailed,
			.NativePlatformVaultSelected    = Implementation->NativePlatformVaultSelected,
			.EpochProjectionDeliveryFailed  = Implementation->EpochProjectionDeliveryFailed,
			.ObservedScheme                 = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->observation().Scheme
				: core::os_color_scheme::unknown,
			.ResolvedVariant                = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.CustomisedRoles                = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->customised_roles()
				: 0U,
			.EpochHandoffDeliveryFailed     = Implementation->Host->epoch_handoff_delivery_failed(),
			.EpochStatus                    = Implementation->EpochHost->state().Status,
			.WorkbenchHostComposed          = Implementation->WorkbenchHost != nullptr,
			.AppearanceHostComposed         = Implementation->AppearanceHost != nullptr,
			.AppearancePersistenceAvailable = Implementation->AppearancePersistenceAvailable,
			.AppearanceDurableWrites        = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->persisted_settings()
				: 0U,
			.AppliedSelectionColor          = Implementation->AppliedSelectionColor,
			.AppliedBaseColor               = Implementation->AppliedBaseColor,
			.AppliedFromProfile             = Implementation->AppliedFromProfile,
			.SyntaxClassifierComposed       = Implementation->SyntaxClassification != nullptr,
			.AdoptedClassifications         = Implementation->AdoptedClassifications,
			.ClassificationSubmissions      = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->classification_submissions()
				: 0U,
			.AppearanceMode                 = Implementation->AppearanceHost != nullptr
				? std::string{ core::stable_name(
					  Implementation->AppearanceHost->settings().Mode) }
				: std::string{ },
			.AppearanceTabWidth             = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->settings().Typography.Code.TabWidth
				: 0U,
			.AppearanceDivergences          = Implementation->AppearanceDivergences,
			.ProfileMode                    = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->mode()
				: core::appearance_mode::system,
			.ProfileVariant                 = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.NotificationDeliveryComposed   = Implementation->Notifications != nullptr,
			.NotificationPermission         = Implementation->NotificationSink != nullptr
				? Implementation->NotificationSink->permission()
				: application::notification_permission::refused,
		};
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::make(
		const detail::github_public_path_fixture   aFixture,
		detail::github_public_path_fixture_options aOptions)
		-> std::unique_ptr<github_application>
	{
		return std::unique_ptr<github_application>{ new github_application{
			std::make_unique<github_application::implementation>(
				github_application_config{ }, aFixture, std::move(aOptions)) } };
	}

	auto detail::github_application_fixture_access::evidence(
		const github_application& aApplication) noexcept
		-> detail::github_public_path_fixture_evidence
	{
		return aApplication.Implementation->Fixture != nullptr
			? aApplication.Implementation->Fixture->evidence()
			: detail::github_public_path_fixture_evidence{ };
	}

	auto detail::github_application_fixture_access::review_mutation_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_mutation_fixture_evidence
	{
		return aApplication.Implementation->Fixture != nullptr
			? aApplication.Implementation->Fixture->review_mutation_evidence()
			: detail::review_mutation_fixture_evidence{ };
	}

	auto detail::github_application_fixture_access::maintenance_evidence(
		const github_application& aApplication,
		core::review_epoch_id     aTarget) noexcept
		-> detail::github_epoch_maintenance_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->EpochHost == nullptr) {
			return { };
		}

		const auto&     state                    = implementation->EpochHost->state();
		const auto      root                     = implementation->Fixture->application_data_root();
		auto            retiredRepositoryPresent = false;
		auto            retiredMarkerPresent     = false;
		std::error_code directoryError;
		auto            marker = std::filesystem::directory_iterator{
			root / "quarantine" / "v1", directoryError
		};
		while (!directoryError
			   && marker != std::filesystem::directory_iterator{ }) {
			if (marker->path().extension() == ".deleted") {
				retiredMarkerPresent = true;
				auto repository      = marker->path().stem();
				repository += ".git";
				std::error_code existsError;
				retiredRepositoryPresent = retiredRepositoryPresent
					|| std::filesystem::exists(
											   root / "repositories" / "v1" / repository,
											   existsError);
			}
			marker.increment(directoryError);
		}
		// Whole-cache deletion may leave a replacement authority projected while
		// shutdown is rejecting its unfinished rehydration.  An empty target asks
		// for durable evidence about the retired cache, so derive the stable
		// catalog binding from the captured smart authority instead of sampling
		// that transient projection.
		if (!state.Authority.has_value()
			|| (!core::valid(aTarget) && retiredMarkerPresent)) {
			auto       catalogRevision  = std::int64_t{ 0 };
			auto       baseCacheDeleted = false;
			auto       headCacheDeleted = false;
			auto       baseAvailability = -1;
			auto       headAvailability = -1;
			const auto smartAuthority   = implementation->Fixture->last_smart_authority();
			const auto fixtureEvidence  = implementation->Fixture->evidence();
			const auto base             = core::parse_git_commit_oid(fixtureEvidence.BaseOid);
			if (smartAuthority && base) {
				auto reopened = implementation->EpochCatalogs->open(
					application::epoch_catalog_open_request{
						.ApplicationSession = state.ApplicationSession,
						.Authority          = {
							.Profile         = smartAuthority->Profile,
							.ProfileRevision = smartAuthority->ProfileRevision,
							.Host            = smartAuthority->Host,
							.HostAuthority   = smartAuthority->HostAuthority,
							.Account         = smartAuthority->Account,
							.Viewer          = smartAuthority->Viewer,
							.Repository      = smartAuthority->Repository,
							.PullRequest     = smartAuthority->PullRequest,
							.Number          = smartAuthority->Number,
							.ObjectFormat    = base->algorithm(),
						},
					},
					{ });
				if (reopened && *reopened) {
					const auto& binding = (*reopened)->binding();
					catalogRevision     = binding.Catalog.Revision.value();
					aTarget             = core::review_epoch_id{
						.Authority   = binding.Authority.Scope,
						.Incarnation = binding.Catalog.Incarnation,
						.Ordinal     = core::epoch_ordinal{ 1 },
					};
					const auto anchors = (*reopened)->exact_anchors(
						application::exact_anchor_lookup_request{
							.Catalog = binding.Catalog,
							.Anchors = {
								core::comparison_anchor_identity{
									.Authority   = aTarget.Authority,
									.Incarnation = aTarget.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_base,
									.Epoch = aTarget.Ordinal,
								},
								core::comparison_anchor_identity{
									.Authority   = aTarget.Authority,
									.Incarnation = aTarget.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_head,
									.Epoch = aTarget.Ordinal,
								},
							},
						},
						{ });
					if (anchors && anchors->Anchors.size() == 2U) {
						baseAvailability = static_cast<int>(
							anchors->Anchors[0].Availability);
						headAvailability = static_cast<int>(
							anchors->Anchors[1].Availability);
						baseCacheDeleted = anchors->Anchors[0].Availability
							== core::anchor_availability::cache_deleted;
						headCacheDeleted = anchors->Anchors[1].Availability
							== core::anchor_availability::cache_deleted;
					}
				}
			}
			return {
				.PruneIntentDispatchCount          = implementation->EpochPruneIntentDispatchCount,
				.DeleteIntentDispatchCount         = implementation->EpochDeleteIntentDispatchCount,
				.CancelIntentDispatchCount         = implementation->EpochCancelIntentDispatchCount,
				.CatalogRevision                   = catalogRevision,
				.RepositoryPresent                 = retiredRepositoryPresent,
				.CacheDeletionMarkerPresent        = retiredMarkerPresent,
				.TargetBaseCacheDeleted            = baseCacheDeleted,
				.TargetHeadCacheDeleted            = headCacheDeleted,
				.RetiredRepositoryPresent          = retiredRepositoryPresent,
				.RetiredCacheDeletionMarkerPresent = retiredMarkerPresent,
				.TargetBaseAvailability            = baseAvailability,
				.TargetHeadAvailability            = headAvailability,
			};
		}

		const auto lowerHex = [](const std::span<const std::uint8_t> aBytes) {
			static constexpr std::string_view digits = "0123456789abcdef";
			auto                              result = std::string{ };
			result.reserve(aBytes.size() * 2U);
			for (const auto byte : aBytes) {
				result.push_back(digits[(byte >> 4U) & 0x0fU]);
				result.push_back(digits[byte & 0x0fU]);
			}
			return result;
		};

		const auto& authority = *state.Authority;
		if (!core::valid(aTarget) && state.Catalog) {
			aTarget = core::review_epoch_id{
				.Authority   = authority.Scope,
				.Incarnation = state.Catalog->Incarnation,
				.Ordinal     = core::epoch_ordinal{ 1 },
			};
		}
		const auto cache      = lowerHex(authority.Cache.bytes());
		const auto scope      = lowerHex(aTarget.Authority.bytes());
		const auto repository = root / "repositories" / "v1" / (cache + ".git");
		auto       ordinal    = std::to_string(aTarget.Ordinal.value());
		if (ordinal.size() < 16U) {
			ordinal.insert(0U, 16U - ordinal.size(), '0');
		}
		const auto epochRoot = repository / "refs" / "betterpullreview" / "v1"
			/ "scope" / scope / "epoch"
			/ ordinal;
		auto nonTargetOrdinal = std::to_string(2U);
		nonTargetOrdinal.insert(
			0U, 16U - nonTargetOrdinal.size(), '0');
		const auto nonTargetEpochRoot = repository / "refs"
			/ "betterpullreview" / "v1" / "scope" / scope / "epoch"
			/ nonTargetOrdinal;
		const auto      deletionMarker = root / "quarantine" / "v1" / (cache + ".deleted");
		std::error_code error;
		const auto      present = [&error](const std::filesystem::path& aPath) {
			error.clear();
			const auto result = std::filesystem::exists(aPath, error);
			return !error && result;
		};

		auto        baseCacheDeleted = false;
		auto        headCacheDeleted = false;
		auto        baseAvailability = -1;
		auto        headAvailability = -1;
		const auto& acceptedSnapshot = implementation->Host->accepted_snapshot();
		const auto  acceptedBase     = acceptedSnapshot
			? core::parse_git_commit_oid(
				  acceptedSnapshot->PullRequest.BaseOid.value())
			: std::nullopt;
		if (acceptedBase && !state.PendingMaintenance.has_value()) {
			auto reopened = implementation->EpochCatalogs->open(
				application::epoch_catalog_open_request{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = {
						.Profile         = authority.Profile,
						.ProfileRevision = authority.ProfileRevision,
						.Host            = authority.Host,
						.HostAuthority   = authority.HostAuthority,
						.Account         = authority.Account,
						.Viewer          = authority.Viewer,
						.Repository      = authority.Repository,
						.PullRequest     = authority.PullRequest,
						.Number          = authority.Number,
						.ObjectFormat    = acceptedBase->algorithm(),
					},
				},
				{ });
			if (reopened && *reopened) {
				const auto& binding = (*reopened)->binding();
				if (!core::valid(aTarget)) {
					aTarget = core::review_epoch_id{
						.Authority   = binding.Authority.Scope,
						.Incarnation = binding.Catalog.Incarnation,
						.Ordinal     = core::epoch_ordinal{ 1 },
					};
				}
				const auto anchors = (*reopened)->exact_anchors(
					application::exact_anchor_lookup_request{
						.Catalog = binding.Catalog,
						.Anchors = {
							core::comparison_anchor_identity{
								.Authority   = aTarget.Authority,
								.Incarnation = aTarget.Incarnation,
								.Kind        = core::comparison_anchor_kind::epoch_base,
								.Epoch       = aTarget.Ordinal,
							},
							core::comparison_anchor_identity{
								.Authority   = aTarget.Authority,
								.Incarnation = aTarget.Incarnation,
								.Kind        = core::comparison_anchor_kind::epoch_head,
								.Epoch       = aTarget.Ordinal,
							},
						},
					},
					{ });
				if (anchors && anchors->Anchors.size() == 2U) {
					baseAvailability = static_cast<int>(
						anchors->Anchors[0].Availability);
					headAvailability = static_cast<int>(
						anchors->Anchors[1].Availability);
					baseCacheDeleted = anchors->Anchors[0].Availability
						== core::anchor_availability::cache_deleted;
					headCacheDeleted = anchors->Anchors[1].Availability
						== core::anchor_availability::cache_deleted;
				}
			}
		}

		return {
			.PruneIntentDispatchCount          = implementation->EpochPruneIntentDispatchCount,
			.DeleteIntentDispatchCount         = implementation->EpochDeleteIntentDispatchCount,
			.CancelIntentDispatchCount         = implementation->EpochCancelIntentDispatchCount,
			.CatalogRevision                   = state.Catalog
				? state.Catalog->Revision.value()
				: 0,
			.TargetBaseRefPresent              = present(epochRoot / "base"),
			.TargetHeadRefPresent              = present(epochRoot / "head"),
			.NonTargetBaseRefPresent           = present(nonTargetEpochRoot / "base"),
			.NonTargetHeadRefPresent           = present(nonTargetEpochRoot / "head"),
			.RepositoryPresent                 = present(repository),
			.CacheDeletionMarkerPresent        = present(deletionMarker),
			.TargetBaseCacheDeleted            = baseCacheDeleted,
			.TargetHeadCacheDeleted            = headCacheDeleted,
			.RetiredRepositoryPresent          = retiredRepositoryPresent,
			.RetiredCacheDeletionMarkerPresent = retiredMarkerPresent,
			.TargetBaseAvailability            = baseAvailability,
			.TargetHeadAvailability            = headAvailability,
		};
	}

	namespace {
		[[nodiscard]] auto workbench_probe_invalidation(
			const application::accepted_pull_request_handoff& aHandoff,
			const core::pull_request_id                       aPullRequest)
			-> application::pull_request_authority_invalidation
		{
			return {
				.ApplicationSession = aHandoff.ApplicationSession,
				.Profile            = aHandoff.Profile,
				.ProfileRevision    = aHandoff.ProfileRevision,
				.Host               = aHandoff.Host,
				.HostAuthority      = aHandoff.HostAuthority,
				.Account            = aHandoff.Account,
				.Viewer             = aHandoff.Viewer,
				.Repository         = aHandoff.Repository,
				.PullRequest        = aPullRequest,
				.IdentityGeneration = aHandoff.IdentityGeneration,
			};
		}
	} // namespace

	auto detail::github_application_fixture_access::select_other_appearance_mode(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		const auto current = implementation->AppearanceHost->settings().Mode;
		const auto other   = current == core::appearance_mode::dark
			? core::appearance_mode::light
			: core::appearance_mode::dark;
		implementation->AppearanceHost->select(other);
		return implementation->AppearanceHost->settings().Mode == other;
	}

	auto detail::github_application_fixture_access::apply_appearance_gesture(
		github_application&         aApplication,
		const core::appearance_mode aMode,
		const std::uint32_t         aTabWidth) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		auto* const panel = implementation->Window->appearance_settings();
		if (panel == nullptr) {
			return false;
		}
		// Through the controls, which is the whole claim. Setting the combo box
		// index and the spin box value is what a reviewer's click and keystroke
		// produce; if the wiring between them and the host is missing, nothing
		// below moves.
		panel->mode_selector()->setCurrentIndex(static_cast<int>(aMode));
		panel->tab_width_editor()->setValue(static_cast<int>(aTabWidth));
		const auto& settings = implementation->AppearanceHost->settings();
		return settings.Mode == aMode
			&& settings.Typography.Code.TabWidth == aTabWidth;
	}

	auto detail::github_application_fixture_access::refuse_appearance_gesture(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		auto* const panel = implementation->Window->appearance_settings();
		if (panel == nullptr) {
			return false;
		}
		const auto before = implementation->AppearanceHost->settings();
		// An empty family is the one illegal value a control can express: the
		// numeric controls carry the core's own bounds, so they cannot.
		panel->code_family_editor()->setText(QString{ });
		Q_EMIT panel->code_family_editor()->editingFinished();
		const auto& after = implementation->AppearanceHost->settings();
		return after == before
			&& !implementation->AppearanceHost->last_rejections().empty()
			&& panel->status_text().startsWith(QStringLiteral("Refused"))
			&& panel->code_family_editor()->text().toStdString()
			== before.Typography.Code.Font.Family;
	}

	auto detail::github_application_fixture_access::
		exercise_appearance_theme_customization(
			github_application&    aApplication,
			const std::string_view aThemePath)
			-> detail::appearance_theme_customization_probe_result
	{
		using result               = detail::appearance_theme_customization_probe_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr
			|| implementation->AppearanceProfile == nullptr) {
			return result::failed;
		}
		auto* const panel     = implementation->Window->appearance_settings();
		auto* const workbench = implementation->Window->comparison_surface();
		if (panel == nullptr || workbench == nullptr) {
			return result::failed;
		}
		if (workbench->current_result() == nullptr
			|| !workbench->current_context().has_value()
			|| !workbench->selection().has_value()
			|| !workbench->token_spans(core::diff_side::left)
			|| !workbench->token_spans(core::diff_side::right)
			|| implementation->AppearanceHost->outstanding_requests() != 0U) {
			return result::pending;
		}
		const auto reportStage = [](const char* const aStage) {
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_STAGE:" << aStage << '\n';
			stream.flush();
		};

		if (!implementation->AppearanceThemeProbe.has_value()) {
			implementation->AppearanceThemeProbe
				= github_application::implementation::appearance_theme_probe_state{
					  .Path              = std::string{ aThemePath },
					  .BeforeSettings    = implementation->AppearanceHost->settings(),
					  .BeforePalette     = implementation->AppearanceProfile->palette(),
					  .NativeObservation = github_application::implementation::
						  native_appearance_observation(),
					  .Result            = workbench->current_result(),
					  .Context           = workbench->current_context(),
					  .Selection         = workbench->selection(),
					  .LeftSpans         = workbench->token_spans(core::diff_side::left),
					  .RightSpans        = workbench->token_spans(core::diff_side::right),
					  .Statistics        = workbench->statistics(),
					  .Position          = workbench->scroll_controller()->position(),
					  .ProfileGeneration = implementation->AppearanceProfile->generation(),
					  .ClassificationSubmissions
					  = implementation->AppearanceHost->classification_submissions(),
					  .PersistedSettings
					  = implementation->AppearanceHost->persisted_settings(),
				  };
			return result::pending;
		}

		auto& probe = *implementation->AppearanceThemeProbe;
		if (probe.Path != aThemePath
			|| probe.Phase
				== github_application::implementation::appearance_theme_probe_state::
					phase::failed) {
			return result::failed;
		}
		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::passed) {
			return result::passed;
		}

		const auto unchanged = [&]() {
			const auto statistics = workbench->statistics();
			return workbench->current_result() == probe.Result
				&& workbench->current_context() == probe.Context
				&& workbench->selection() == probe.Selection
				&& workbench->scroll_controller()->position() == probe.Position
				&& workbench->token_spans(core::diff_side::left) == probe.LeftSpans
				&& workbench->token_spans(core::diff_side::right) == probe.RightSpans
				&& statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds
				&& statistics.SemanticIndexBuilds == probe.Statistics.SemanticIndexBuilds
				&& statistics.SelectionChanges == probe.Statistics.SelectionChanges
				&& statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions
				&& implementation->AppearanceHost->classification_submissions()
				== probe.ClassificationSubmissions;
		};
		const auto reportIdentity = [&]() {
			const auto  statistics = workbench->statistics();
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_IDENTITY_FACTS:"
				   << (workbench->current_result() == probe.Result)
				   << (workbench->current_context() == probe.Context)
				   << (workbench->selection() == probe.Selection)
				   << (workbench->scroll_controller()->position() == probe.Position)
				   << (workbench->token_spans(core::diff_side::left) == probe.LeftSpans)
				   << (workbench->token_spans(core::diff_side::right) == probe.RightSpans)
				   << (statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds)
				   << (statistics.SemanticIndexBuilds == probe.Statistics.SemanticIndexBuilds)
				   << (statistics.SelectionChanges == probe.Statistics.SelectionChanges)
				   << (statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions)
				   << (implementation->AppearanceHost->classification_submissions()
					   == probe.ClassificationSubmissions)
				   << '\n';
			stream.flush();
		};
		const auto fail = [&]() {
			reportStage("failed");
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::failed;
			return result::failed;
		};

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_stable_comparison) {
			if (!unchanged()) {
				probe.Result     = workbench->current_result();
				probe.Context    = workbench->current_context();
				probe.Selection  = workbench->selection();
				probe.LeftSpans  = workbench->token_spans(core::diff_side::left);
				probe.RightSpans = workbench->token_spans(core::diff_side::right);
				probe.Statistics = workbench->statistics();
				probe.Position   = workbench->scroll_controller()->position();
				probe.ClassificationSubmissions
					= implementation->AppearanceHost->classification_submissions();
				return result::pending;
			}
			const auto role       = core::appearance_role::diff_added_background;
			probe.ManualCandidate = probe.BeforePalette;
			auto& color           = probe.ManualCandidate[core::index_of(role)];
			color.Red             = color.Red == 0U ? 1U : 0U;
			panel->role_selector()->setCurrentIndex(
				static_cast<int>(core::index_of(role)));
			panel->color_editor()->setText(
				qt::to_presentation_color(color).name(QColor::HexArgb));
			panel->preview_button()->click();
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_manual_preview;
			reportStage("manual-preview-requested");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_manual_preview) {
			const auto statistics = workbench->statistics();
			const auto profilePreviewed
				= implementation->AppearanceProfile->previewing_palette();
			const auto profileMatches = implementation->AppearanceProfile->palette()
				== probe.ManualCandidate;
			const auto panelMatches
				= panel->preview_palette() == std::optional{ probe.ManualCandidate };
			const auto settingsUnchanged
				= implementation->AppearanceHost->settings() == probe.BeforeSettings;
			const auto writesUnchanged
				= implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings;
			const auto generationChanged
				= implementation->AppearanceProfile->generation()
				> probe.ProfileGeneration;
			const auto paintChanged = statistics.AlignmentSegmentsVisited
				> probe.Statistics.AlignmentSegmentsVisited;
			const auto identityUnchanged = unchanged();
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_MANUAL_FACTS:"
					   << profilePreviewed << profileMatches << panelMatches
					   << settingsUnchanged << writesUnchanged << generationChanged
					   << paintChanged << identityUnchanged << '\n';
				stream.flush();
			}
			if (!profilePreviewed || !profileMatches || !panelMatches
				|| !settingsUnchanged || !writesUnchanged || !generationChanged
				|| !paintChanged || !identityUnchanged) {
				return fail();
			}
			reportStage("manual-preview");
			auto alternateObservation = probe.NativeObservation;
			alternateObservation.Scheme
				= alternateObservation.Scheme == core::os_color_scheme::dark
				? core::os_color_scheme::light
				: core::os_color_scheme::dark;
			implementation->AppearanceHost->observe(alternateObservation);
			implementation->AppearanceProfile->observe(alternateObservation);
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_preview_adoption;
			reportStage("unrelated-adoption-requested");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_preview_adoption) {
			const auto adoptionFacts
				= std::array{
					  implementation->AppearanceProfile->previewing_palette(),
					  implementation->AppearanceProfile->palette()
						  == probe.ManualCandidate,
					  panel->preview_palette()
						  == std::optional{ probe.ManualCandidate },
					  panel->commit_button()->isEnabled(),
					  panel->cancel_button()->isEnabled(),
					  implementation->AppearanceHost->settings()
						  == probe.BeforeSettings,
					  implementation->AppearanceHost->persisted_settings()
						  == probe.PersistedSettings,
					  unchanged(),
				  };
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_ADOPTION_FACTS:";
				for (const auto fact : adoptionFacts) {
					stream << fact;
				}
				stream << '\n';
				stream.flush();
			}
			if (!std::ranges::all_of(adoptionFacts, std::identity{ })) {
				return fail();
			}
			implementation->AppearanceHost->observe(probe.NativeObservation);
			implementation->AppearanceProfile->observe(probe.NativeObservation);
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_preview_restoration;
			reportStage("native-observation-restored");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_preview_restoration) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.ManualCandidate
				|| panel->preview_palette()
					!= std::optional{ probe.ManualCandidate }
				|| !panel->commit_button()->isEnabled()
				|| !panel->cancel_button()->isEnabled()
				|| implementation->AppearanceHost->settings()
					!= probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			panel->cancel_button()->click();
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette()
					!= probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			reportStage("manual-cancelled");
			probe.Statistics = workbench->statistics();
			probe.ProfileGeneration
				= implementation->AppearanceProfile->generation();
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_first_preview;
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("import-requested");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_first_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()) {
				return panel->status_text().contains(QStringLiteral("refused"), Qt::CaseInsensitive)
						|| panel->status_text().contains(QStringLiteral("failed"), Qt::CaseInsensitive)
					? fail()
					: result::pending;
			}
			if (!panel->preview_palette().has_value()) {
				return fail();
			}
			probe.Candidate = implementation->AppearanceProfile->palette();
			reportStage("first-preview");
			const auto statistics       = workbench->statistics();
			const auto candidateChanged = probe.Candidate != probe.BeforePalette;
			const auto panelMatches     = *panel->preview_palette() == probe.Candidate;
			const auto settingsUnchanged
				= implementation->AppearanceHost->settings() == probe.BeforeSettings;
			const auto writesUnchanged
				= implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings;
			const auto generationChanged
				= implementation->AppearanceProfile->generation()
				> probe.ProfileGeneration;
			const auto paintChanged = statistics.SyntaxBlocksFormatted
				> probe.Statistics.SyntaxBlocksFormatted;
			const auto identityUnchanged = unchanged();
			reportIdentity();
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_FIRST_FACTS:"
					   << candidateChanged << panelMatches << settingsUnchanged
					   << writesUnchanged << generationChanged << paintChanged
					   << identityUnchanged << '\n';
				stream.flush();
			}
			if (!candidateChanged || !panelMatches || !settingsUnchanged
				|| !writesUnchanged || !generationChanged || !paintChanged
				|| !identityUnchanged) {
				return fail();
			}
			panel->cancel_button()->click();
			reportStage("cancelled");
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette()
					!= probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_second_preview;
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("second-import-requested");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_second_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()) {
				return result::pending;
			}
			if (!panel->preview_palette().has_value()
				|| *panel->preview_palette() != probe.Candidate
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			reportStage("second-preview");
			panel->commit_button()->click();
			reportStage("commit-dispatched");
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.Candidate
				|| implementation->AppearanceHost->settings().Palette
					!= probe.Candidate
				|| !implementation->AppearanceHost->settings().PaletteStored
				|| !unchanged()) {
				return fail();
			}
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_store;
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_store) {
			if (implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings) {
				return result::pending;
			}
			if (implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| implementation->AppearanceHost->settings().Palette
					!= probe.Candidate
				|| !unchanged()) {
				return fail();
			}
			reportStage("persisted");
			probe.StoredCandidate = probe.Candidate;
			const auto role       = core::appearance_role::diff_removed_background;
			auto&      color      = probe.StoredCandidate[core::index_of(role)];
			color.Blue            = color.Blue == 0U ? 1U : 0U;
			panel->role_selector()->setCurrentIndex(
				static_cast<int>(core::index_of(role)));
			panel->color_editor()->setText(
				qt::to_presentation_color(color).name(QColor::HexArgb));
			panel->preview_button()->click();
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_stored_preview;
			reportStage("stored-palette-preview-requested");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_stored_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.StoredCandidate
				|| panel->preview_palette()
					!= std::optional{ probe.StoredCandidate }
				|| implementation->AppearanceHost->settings().Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			auto alternateObservation = probe.NativeObservation;
			alternateObservation.Scheme
				= alternateObservation.Scheme == core::os_color_scheme::dark
				? core::os_color_scheme::light
				: core::os_color_scheme::dark;
			implementation->AppearanceHost->observe(alternateObservation);
			implementation->AppearanceProfile->observe(alternateObservation);
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_stored_preview_adoption;
			reportStage("stored-palette-adoption-requested");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_stored_preview_adoption) {
			const auto adoptionFacts = std::array{
				implementation->AppearanceProfile->previewing_palette(),
				implementation->AppearanceProfile->palette()
					== probe.StoredCandidate,
				implementation->AppearanceProfile->settings().Palette
					== probe.Candidate,
				panel->preview_palette() == std::optional{ probe.StoredCandidate },
				panel->commit_button()->isEnabled(),
				panel->cancel_button()->isEnabled(),
				implementation->AppearanceHost->settings().Palette
					== probe.Candidate,
				implementation->AppearanceHost->persisted_settings()
					== probe.PersistedSettings + 1U,
				unchanged(),
			};
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_STORED_ADOPTION_FACTS:";
				for (const auto fact : adoptionFacts) {
					stream << fact;
				}
				stream << '\n';
				stream.flush();
			}
			if (!std::ranges::all_of(adoptionFacts, std::identity{ })) {
				return fail();
			}
			implementation->AppearanceHost->observe(probe.NativeObservation);
			implementation->AppearanceProfile->observe(probe.NativeObservation);
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_stored_preview_restoration;
			reportStage("stored-palette-observation-restored");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_stored_preview_restoration) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.StoredCandidate
				|| implementation->AppearanceProfile->settings().Palette
					!= probe.Candidate
				|| panel->preview_palette()
					!= std::optional{ probe.StoredCandidate }
				|| !panel->commit_button()->isEnabled()
				|| !panel->cancel_button()->isEnabled()
				|| implementation->AppearanceHost->settings().Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			panel->cancel_button()->click();
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.Candidate
				|| implementation->AppearanceProfile->settings().Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->settings().Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			reportStage("stored-palette-preview-cancelled");
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_equal_stored_preview;
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("equal-stored-import-requested");
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::appearance_theme_probe_state::
				phase::awaiting_equal_stored_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette() != probe.Candidate
				|| panel->preview_palette() != std::optional{ probe.Candidate }
				|| !panel->commit_button()->isEnabled()
				|| !panel->cancel_button()->isEnabled()
				|| implementation->AppearanceHost->settings().Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			panel->commit_button()->click();
			probe.Phase = github_application::implementation::
				appearance_theme_probe_state::phase::awaiting_equal_stored_commit;
			reportStage("equal-stored-commit-dispatched");
			return result::pending;
		}

		const auto equalCommitFacts = std::array{
			!implementation->AppearanceProfile->previewing_palette(),
			implementation->AppearanceProfile->palette() == probe.Candidate,
			implementation->AppearanceProfile->settings().Palette == probe.Candidate,
			!panel->preview_palette().has_value(),
			!panel->commit_button()->isEnabled(),
			!panel->cancel_button()->isEnabled(),
			implementation->AppearanceHost->settings().Palette == probe.Candidate,
			implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings + 1U,
			unchanged(),
		};
		{
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_EQUAL_COMMIT_FACTS:";
			for (const auto fact : equalCommitFacts) {
				stream << fact;
			}
			stream << '\n';
			stream.flush();
		}
		if (!std::ranges::all_of(equalCommitFacts, std::identity{ })) {
			return fail();
		}
		probe.Phase = github_application::implementation::
			appearance_theme_probe_state::phase::passed;
		reportStage("equal-stored-commit-acknowledged");
		return result::passed;
	}

	auto detail::github_application_fixture_access::exercise_workbench_composition(
		github_application& aApplication)
		-> detail::workbench_composition_probe_result
	{
		using result     = detail::workbench_composition_probe_result;
		using state_type = github_application::implementation::
			workbench_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return result::failed;
		}

		if (!implementation->WorkbenchCompositionProbe.has_value()) {
			implementation->WorkbenchCompositionProbe.emplace();
		}
		auto& probe = *implementation->WorkbenchCompositionProbe;
		probe.Evidence.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U;
		auto* const host       = implementation->WorkbenchHost.get();
		auto* const window     = implementation->Window.get();
		auto* const comparison = window->comparison_surface();
		const auto  fail       = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};

		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& handoff  = *implementation->LastAcceptedHandoff;
			const auto& snapshot = implementation->Host->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| !host->state().Navigation.has_value()) {
				return result::pending;
			}

			probe.Authority = workbench_authority(handoff);
			probe.ExpectedFiles.clear();
			for (const auto& file : snapshot->Files) {
				const auto identity = core::file_identity_of(file.Path);
				if (!identity.valid()) {
					return fail();
				}
				if (std::ranges::find(probe.ExpectedFiles, identity)
					== probe.ExpectedFiles.end()) {
					probe.ExpectedFiles.push_back(identity);
				}
			}

			probe.Evidence.ExactAuthority
				= application::valid(probe.Authority)
				&& implementation->WorkbenchReview.exact(handoff)
				&& probe.Authority.ApplicationSession
					== handoff.ApplicationSession
				&& probe.Authority.Profile == handoff.Profile
				&& probe.Authority.ProfileRevision == handoff.ProfileRevision
				&& probe.Authority.Host == handoff.Host
				&& probe.Authority.HostAuthority == handoff.HostAuthority
				&& probe.Authority.Account == handoff.Account
				&& probe.Authority.Viewer == handoff.Viewer
				&& probe.Authority.Repository == handoff.Repository
				&& probe.Authority.PullRequest == handoff.PullRequest
				&& probe.Authority.Number == handoff.Number
				&& probe.Authority.Snapshot == handoff.Snapshot
				&& probe.Authority.IdentityGeneration
					== handoff.IdentityGeneration
				&& probe.Authority.PullRequestGeneration
					== handoff.PullRequestGeneration;
			probe.Evidence.BoundedObservation
				= !probe.ExpectedFiles.empty()
				&& probe.ExpectedFiles.size()
					<= application::workbench_host_limits{ }.ActionableFiles
				&& std::ranges::equal(host->actionable(), probe.ExpectedFiles)
				&& host->state().Navigation.has_value();
			probe.Evidence.SessionSettingsFallback
				= !implementation->WorkbenchPersistenceAvailable
				&& implementation->WorkbenchSettings != nullptr
				&& host->layout() == application::workbench_layout_settings{ };
			if (!probe.Evidence.ExactAuthority
				|| !probe.Evidence.BoundedObservation
				|| !probe.Evidence.SessionSettingsFallback
				|| window->files_drawer() == nullptr
				|| window->files_drawer()->contents() == nullptr
				|| window->files_drawer()->contents_view() == nullptr
				|| window->files_drawer()->contents()->rowCount() != 1) {
				return fail();
			}

			probe.IntentDispatches   = implementation->WorkbenchIntentDispatches;
			probe.WindowStatistics   = window->statistics();
			probe.SelectedDiffBegins = implementation->SelectedDiffBegins;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			auto* const view           = window->files_drawer()->contents_view();
			view->setCurrentIndex(view->model()->index(0, 0));
			probe.Phase = state_type::phase::awaiting_file_projection;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_file_projection) {
			const auto statistics = window->statistics();
			if (implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches) {
				if (statistics.FileSelections
					== probe.WindowStatistics.FileSelections + 1U) {
					probe.Evidence.FileGesture = true;
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.IntentDispatch
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			probe.Evidence.FileGesture
				= statistics.FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			probe.Evidence.ProjectionDelivery
				= host->selected() == std::optional{ probe.ExpectedFiles.front() }
				&& statistics.Publications > probe.WindowStatistics.Publications
				&& statistics.CommandUpdates
					> probe.WindowStatistics.CommandUpdates
				&& !window->next_file_action()->isEnabled();
			if (!probe.Evidence.IntentDispatch || !probe.Evidence.FileGesture
				|| !probe.Evidence.ProjectionDelivery) {
				return fail();
			}
			probe.Phase = state_type::phase::awaiting_diff;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_diff) {
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (comparison == nullptr || comparison->current_result() == nullptr
				|| !comparison->selection().has_value()
				|| implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications
				|| !implementation->LastSelectedDiff.has_value()) {
				return result::pending;
			}
			const auto& publication = *implementation->LastSelectedDiff;
			const auto& request     = publication.Request;
			const auto& key         = publication.Key;
			const auto& epoch       = implementation->EpochHost->state();
			const auto  fixture     = implementation->Fixture->evidence();
			const auto  leftBlob    = core::parse_git_blob_oid(fixture.BaseBlobOid);
			const auto  rightBlob   = core::parse_git_blob_oid(fixture.HeadOneBlobOid);
			probe.Evidence.SelectedDiffRequest
				= implementation->SelectedDiffBegins
					== probe.SelectedDiffBegins + 1U
				&& implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications + 1U
				&& request.WorkbenchAuthority == probe.Authority
				&& request.Context.Generation == host->generation()
				&& request.File.File == probe.ExpectedFiles.front()
				&& request.File.PresentationPath == "review.cpp"
				&& request.File.CurrentPath == std::filesystem::path{ "review.cpp" }
				&& !request.File.PreviousPath.has_value()
				&& request.File.Change
					== application::workbench_file_change_kind::modified;
			probe.Evidence.ExactComparison
				= epoch.Authority.has_value()
				&& epoch.AcceptedComparison.has_value()
				&& leftBlob.has_value() && rightBlob.has_value()
				&& key.Authority == *epoch.Authority
				&& key.Repository == epoch.Authority->Repository
				&& key.PullRequest == epoch.Authority->PullRequest
				&& key.LeftAnchor == epoch.AcceptedComparison->Left.Identity
				&& key.RightAnchor == epoch.AcceptedComparison->Right.Identity
				&& key.LeftBlob == leftBlob
				&& key.RightBlob == rightBlob
				&& key.File == probe.ExpectedFiles.front()
				&& key.Options == request.Diff.OptionsDigest
				&& key.Algorithm == core::supported_diff_algorithm_revision;
			if (!probe.Evidence.SelectedDiffRequest
				|| !probe.Evidence.ExactComparison) {
				return fail();
			}
			auto* action = window->next_change_action();
			if (comparison->next_change_button() == nullptr
				|| !comparison->next_change_button()->isEnabled()) {
				action = window->previous_change_action();
				if (comparison->previous_change_button() == nullptr
					|| !comparison->previous_change_button()->isEnabled()) {
					return fail();
				}
			}
			if (action == nullptr) {
				return fail();
			}

			probe.Selection        = comparison->selection();
			probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
			action->setEnabled(true);
			action->trigger();
			probe.Phase = state_type::phase::awaiting_change_projection;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_change_projection) {
			window->previous_change_action()->setEnabled(false);
			window->next_change_action()->setEnabled(false);
			if (implementation->WorkbenchChangeDispatches
				== probe.ChangeDispatches) {
				return result::pending;
			}
			probe.Evidence.ChangeDispatch
				= implementation->WorkbenchChangeDispatches
					== probe.ChangeDispatches + 1U
				&& comparison != nullptr
				&& comparison->selection() != probe.Selection;
			if (!probe.Evidence.ChangeDispatch || window->files_drawer() == nullptr) {
				return fail();
			}

			probe.WidthDispatches   = implementation->WorkbenchWidthDispatches;
			probe.PersistedLayouts  = host->persisted_layouts();
			probe.InitialFilesWidth = host->layout().FilesWidth;
			probe.WindowStatistics  = window->statistics();
			const auto target       = static_cast<int>(std::min<std::uint32_t>(
				probe.InitialFilesWidth + 41U,
				application::maximum_workbench_drawer_width));
			window->files_drawer()->resize(
				target, std::max(1, window->files_drawer()->height()));
			probe.Phase = state_type::phase::awaiting_width_store;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_width_store) {
			const auto statistics = window->statistics();
			if (implementation->WorkbenchWidthDispatches
					== probe.WidthDispatches
				|| host->persisted_layouts() == probe.PersistedLayouts) {
				return result::pending;
			}
			probe.Evidence.DrawerWidthDispatch
				= implementation->WorkbenchWidthDispatches
					== probe.WidthDispatches + 1U
				&& host->layout().FilesWidth != probe.InitialFilesWidth
				&& host->persisted_layouts() == probe.PersistedLayouts + 1U
				&& statistics.LayoutUpdates
					> probe.WindowStatistics.LayoutUpdates;
			if (!probe.Evidence.DrawerWidthDispatch) {
				return fail();
			}

			const auto& handoff  = *implementation->LastAcceptedHandoff;
			auto        mismatch = workbench_probe_invalidation(
				handoff,
				core::pull_request_id{ handoff.PullRequest.value() + 1U });
			const auto selected      = host->selected();
			const auto generation    = host->generation();
			const auto invalidations = implementation->WorkbenchInvalidations;
			implementation->invalidate(std::move(mismatch));
			probe.Evidence.MismatchPreserved
				= implementation->WorkbenchInvalidations == invalidations
				&& implementation->WorkbenchReview.active()
				&& host->selected() == selected
				&& host->generation() == generation;
			if (!probe.Evidence.MismatchPreserved) {
				return fail();
			}

			probe.Generation       = generation;
			probe.Invalidations    = invalidations;
			probe.WindowStatistics = statistics;
			implementation->invalidate(
				workbench_probe_invalidation(handoff, handoff.PullRequest));
			probe.Phase = state_type::phase::awaiting_invalidation;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_invalidation) {
			const auto statistics = window->statistics();
			if (host->generation() == probe.Generation
				|| host->outstanding_requests() != 0U) {
				return result::pending;
			}
			probe.Evidence.InvalidationSuperseded
				= implementation->WorkbenchInvalidations
					== probe.Invalidations + 1U
				&& !implementation->WorkbenchReview.active()
				&& host->actionable().empty()
				&& !host->selected().has_value()
				&& !host->state().Navigation.has_value()
				&& statistics.Publications > probe.WindowStatistics.Publications
				&& !window->previous_file_action()->isEnabled()
				&& !window->next_file_action()->isEnabled();
			if (!probe.Evidence.InvalidationSuperseded) {
				return fail();
			}

			probe.WindowStatistics = statistics;
			implementation->WorkbenchReview.make_unavailable(probe.Authority);
			host->observe(probe.Authority);
			probe.Phase = state_type::phase::awaiting_failure;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_failure) {
			const auto statistics = window->statistics();
			if (statistics.Failures == probe.WindowStatistics.Failures
				|| host->outstanding_requests() != 0U) {
				return result::pending;
			}
			probe.Evidence.FailureDelivery
				= statistics.Failures == probe.WindowStatistics.Failures + 1U;
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			if (!probe.Evidence.FailureDelivery) {
				return fail();
			}

			const auto all = std::array{
				probe.Evidence.ExactAuthority,
				probe.Evidence.BoundedObservation,
				probe.Evidence.IntentDispatch,
				probe.Evidence.ProjectionDelivery,
				probe.Evidence.FileGesture,
				probe.Evidence.SelectedDiffRequest,
				probe.Evidence.ExactComparison,
				probe.Evidence.ChangeDispatch,
				probe.Evidence.DrawerWidthDispatch,
				probe.Evidence.SessionSettingsFallback,
				probe.Evidence.MismatchPreserved,
				probe.Evidence.InvalidationSuperseded,
				probe.Evidence.FailureDelivery,
				probe.Evidence.FixtureDiffHelperUnused,
			};
			if (!std::ranges::all_of(all, std::identity{ })) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::workbench_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::workbench_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->WorkbenchCompositionProbe.has_value()
			? implementation->WorkbenchCompositionProbe->Evidence
			: detail::workbench_composition_probe_evidence{ };
	}

	namespace {
		[[nodiscard]] auto review_ledger_probe_note(
			const core::file_identity          aFile,
			const core::file_comparison_digest aComparison)
			-> application::local_note_record
		{
			return {
				.Id         = application::local_note_id{ 228U },
				.Attachment = application::local_note_attachment{
					.File       = aFile,
					.Comparison = aComparison,
				},
				.Note = { .Text = "BPR-0228 host-only composition probe." },
			};
		}

		[[nodiscard]] auto review_ledger_probe_defect(
			const core::file_identity          aFile,
			const core::file_comparison_digest aComparison)
			-> application::durable_defect_record
		{
			const auto anchor = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = core::diff_side::right,
				.Line       = 1U,
			};
			return {
				.File         = aFile,
				.Conversation = core::review_conversation_id{ 228U },
				.History      = {
					.Defect = {
						.Id       = core::defect_id{ 228U },
						.Anchor   = anchor,
						.Severity = core::defect_severity::major,
						.Category = core::defect_category::correctness,
						.State    = core::defect_state::needs_verification,
					},
					.OriginalAnchor = anchor,
					.Availability   = core::defect_anchor_availability::available,
				},
			};
		}
	} // namespace

	auto detail::github_application_fixture_access::exercise_review_ledger_composition(
		github_application&                   aApplication,
		const review_ledger_composition_probe aCourse)
		-> detail::review_ledger_composition_probe_result
	{
		using result     = detail::review_ledger_composition_probe_result;
		using state_type = github_application::implementation::
			review_ledger_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return result::failed;
		}

		if (!implementation->ReviewLedgerCompositionProbe.has_value()) {
			implementation->ReviewLedgerCompositionProbe.emplace();
			implementation->ReviewLedgerCompositionProbe->Course = aCourse;
		}
		auto& probe = *implementation->ReviewLedgerCompositionProbe;
		if (probe.Course != aCourse
			|| probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		auto* const window = implementation->Window.get();
		auto* const host   = implementation->WorkbenchHost.get();
		auto* const ledger = implementation->ReviewLedgerHost.get();
		const auto  fail   = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& handoff  = *implementation->LastAcceptedHandoff;
			const auto& snapshot = implementation->Host->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| host->actionable().empty()) {
				return result::pending;
			}
			probe.Evidence.ExactAuthority
				= implementation->ReviewLedgerAuthority
				== std::optional{ workbench_authority(handoff) };
			probe.Evidence.HostComposed = ledger != nullptr;
			probe.Evidence.PublicPathAvailable
				= window->github_status_text() == "Pull request ready.";
			probe.Evidence.HostOnlyProbe = true;
			probe.InitialStatistics      = ledger->statistics();
			auto* const view             = window->files_drawer()->contents_view();
			if (!probe.Evidence.ExactAuthority || !probe.Evidence.HostComposed
				|| !probe.Evidence.PublicPathAvailable || view == nullptr
				|| view->model() == nullptr || view->model()->rowCount() == 0) {
				return fail();
			}
			view->setCurrentIndex(view->model()->index(0, 0));
			probe.Phase = state_type::phase::awaiting_diff;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_diff) {
			const auto* const comparison = window->comparison_surface();
			if (comparison == nullptr || comparison->current_result() == nullptr
				|| !implementation->LastSelectedDiff.has_value()) {
				return result::pending;
			}
			const auto& selected   = *implementation->LastSelectedDiff;
			const auto  material   = core::comparison_digest(selected.Key);
			const auto  retained   = ledger->retained();
			const auto  statistics = ledger->statistics();
			probe.File             = selected.Request.File.File;
			probe.Material         = material;
			probe.Evidence.RealFileGesture
				= window->statistics().FileSelections > 0U;
			probe.Evidence.RealSelectedDiff
				= selected.Request.File.File.valid()
				&& selected.Key == comparison->current_result()->Alignment.Key;
			probe.Evidence.ExactMaterial = material.valid()
				&& (aCourse == review_ledger_composition_probe::unavailable
					|| (retained.File == selected.Request.File.File
						&& retained.Material == material));
			probe.Evidence.ExpectedPreGesturePersistence
				= aCourse == review_ledger_composition_probe::seed
				? statistics.PersistenceRequests == 0U
					&& statistics.Completions == 0U
				: aCourse == review_ledger_composition_probe::reopen
				? statistics.Commands == 0U
					&& statistics.PersistenceRequests == 1U
					&& statistics.Completions == 1U
				: statistics.Commands == 0U
					&& statistics.PersistenceRequests == 0U
					&& statistics.Completions == 0U
					&& statistics.StoreFailures > 0U;
			probe.Evidence.ReceiptLoadedOnEntry
				= statistics.ReceiptLoadHits == 1U;
			implementation->ReviewLedgerWasPresentOnEntry
				= probe.Evidence.ReceiptLoadedOnEntry;

			if (!probe.Evidence.RealFileGesture
				|| !probe.Evidence.RealSelectedDiff
				|| !probe.Evidence.ExactMaterial
				|| !probe.Evidence.ExpectedPreGesturePersistence) {
				return fail();
			}

			if (aCourse == review_ledger_composition_probe::unavailable) {
				probe.Evidence.UnavailableVisible
					= window->status_text().contains(
						  QStringLiteral("Private review progress is unavailable."))
					&& !window->seen_action()->isEnabled()
					&& !window->read_action()->isEnabled()
					&& !window->accept_action()->isEnabled()
					&& !window->exclude_action()->isEnabled()
					&& statistics.StoreFailures > 0U;
				probe.Evidence.DurableReceipt          = true;
				probe.Evidence.PrivateRecords          = true;
				probe.Evidence.GestureSequence         = true;
				probe.Evidence.ActionabilityProjection = true;
				if (!probe.Evidence.UnavailableVisible) {
					return fail();
				}
				probe.Phase = state_type::phase::passed;
				return result::passed;
			}

			if (aCourse == review_ledger_composition_probe::seed) {
				const auto before  = ledger->statistics();
				const auto trigger = [](QAction* const aAction) {
					if (aAction == nullptr || !aAction->isEnabled()) {
						return false;
					}
					aAction->trigger();
					return true;
				};
				const auto gestures = std::array{
					trigger(window->seen_action()),
					trigger(window->read_action()),
					trigger(window->accept_action()),
					trigger(window->accept_action()),
					trigger(window->exclude_action()),
					trigger(window->exclude_action()),
					trigger(window->accept_action()),
				};
				const auto after   = ledger->statistics();
				const auto receipt = ledger->retained();
				probe.Evidence.GestureSequence
					= std::ranges::all_of(gestures, std::identity{ })
					&& after.Commands == before.Commands + gestures.size()
					&& after.PersistenceRequests
						== before.PersistenceRequests + gestures.size()
					&& after.Completions == before.Completions + gestures.size()
					&& after.StoreFailures == before.StoreFailures
					&& receipt.File == probe.File
					&& receipt.Material == probe.Material
					&& receipt.Read == core::file_read_state::read
					&& receipt.Accepted && !receipt.Excluded;
				if (!probe.Evidence.GestureSequence) {
					return fail();
				}

				const auto note = review_ledger_probe_note(
					probe.File, probe.Material);
				const auto defect = review_ledger_probe_defect(
					probe.File, probe.Material);
				const auto& head = implementation->LastAcceptedHandoff->Head;
				if (!ledger->persist(note)
					|| !ledger->persist_last_reviewed_head(head)
					|| !ledger->persist(defect)) {
					return fail();
				}
				const auto privateState       = ledger->private_observation();
				probe.Evidence.PrivateRecords = privateState.has_value()
					&& privateState->Notes == std::vector{ note }
					&& privateState->LastReviewedHead == std::optional{ head }
					&& privateState->Defects == std::vector{ defect };
				probe.Evidence.DurableReceipt               = true;
				implementation->ReviewLedgerRoundTripPassed = true;
			} else {
				const auto receipt      = ledger->retained();
				const auto privateState = ledger->private_observation();
				probe.Evidence.GestureSequence
					= ledger->statistics().Commands == 0U;
				probe.Evidence.DurableReceipt
					= probe.Evidence.ReceiptLoadedOnEntry
					&& receipt.File == probe.File
					&& receipt.Material == probe.Material
					&& receipt.Read == core::file_read_state::unread
					&& !receipt.Accepted && !receipt.Excluded;
				const auto priorHead = implementation->Fixture != nullptr
					? core::parse_git_commit_oid(
						  implementation->Fixture->evidence().HeadOneOid)
					: std::optional<core::git_commit_oid>{ };
				if (privateState.has_value() && privateState->Notes.size() == 1U
					&& privateState->Notes.front().Attachment.has_value()) {
					const auto storedComparison
						= privateState->Notes.front().Attachment->Comparison;
					probe.Evidence.PrivateRecords
						= storedComparison.valid()
						&& storedComparison != probe.Material
						&& privateState->Notes
							== std::vector{ review_ledger_probe_note(
								probe.File, storedComparison) }
						&& priorHead.has_value()
						&& privateState->LastReviewedHead == priorHead
						&& privateState->Defects
							== std::vector{ review_ledger_probe_defect(
								probe.File, storedComparison) }
						&& ledger->statistics().PrivateWrites == 0U;
				}
				implementation->ReviewLedgerRoundTripPassed
					= probe.Evidence.DurableReceipt;
			}
			if (!probe.Evidence.DurableReceipt
				|| !probe.Evidence.PrivateRecords) {
				return fail();
			}
			probe.Phase = state_type::phase::awaiting_actionability;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_actionability) {
			if (host->outstanding_requests() != 0U) {
				return result::pending;
			}
			const auto&       actionable        = host->actionable();
			const auto* const comparison        = window->comparison_surface();
			const auto        selectionRetained = host->selected() == probe.File
				&& implementation->LastSelectedDiff.has_value()
				&& comparison != nullptr && comparison->current_result() != nullptr
				&& comparison->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key;
			if (aCourse == review_ledger_composition_probe::seed) {
				if (!actionable.empty() || !selectionRetained) {
					return fail();
				}
				auto materialBytes   = probe.Material.bytes();
				materialBytes.back() = static_cast<std::uint8_t>(
					materialBytes.back() ^ 0x01U);
				probe.ReopenedMaterial = core::file_material_id{ materialBytes };
				if (!ledger->observe_material(core::file_material_observation{
						.File     = probe.File,
						.Material = probe.ReopenedMaterial,
					})) {
					return fail();
				}
				probe.Phase = state_type::phase::awaiting_material_actionability;
				return result::pending;
			}
			probe.Evidence.ActionabilityProjection = actionable.size() == 1U
				&& actionable.front() == probe.File && selectionRetained;
			if (!probe.Evidence.ActionabilityProjection) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		if (probe.Phase == state_type::phase::awaiting_material_actionability) {
			if (host->outstanding_requests() != 0U) {
				return result::pending;
			}
			const auto& actionable = host->actionable();
			const auto& receipt    = ledger->retained();
			const auto* comparison = window->comparison_surface();
			probe.Evidence.ActionabilityProjection
				= actionable.size() == 1U && actionable.front() == probe.File
				&& host->selected() == probe.File
				&& implementation->LastSelectedDiff.has_value()
				&& comparison != nullptr && comparison->current_result() != nullptr
				&& comparison->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key
				&& receipt.File == probe.File
				&& receipt.Material == probe.ReopenedMaterial
				&& receipt.Read == core::file_read_state::unread
				&& !receipt.Accepted && !receipt.Excluded;
			if (!probe.Evidence.ActionabilityProjection) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::review_ledger_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_ledger_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->ReviewLedgerCompositionProbe.has_value()
			? implementation->ReviewLedgerCompositionProbe->Evidence
			: detail::review_ledger_composition_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::exercise_review_round_trip_composition(
		github_application& aApplication)
		-> detail::review_round_trip_composition_probe_result
	{
		using result               = detail::review_round_trip_composition_probe_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReviewRoundTripCompositionProbe.has_value()) {
			implementation->ReviewRoundTripCompositionProbe.emplace();
		}
		auto& probe = *implementation->ReviewRoundTripCompositionProbe;
		if (probe.Failed) {
			return result::failed;
		}
		if (probe.Complete) {
			return result::passed;
		}
		if (implementation->Fixture != nullptr && implementation->Window != nullptr
			&& implementation->WorkbenchHost != nullptr
			&& implementation->ReviewMutationCoordinator != nullptr
			&& !implementation->ReviewDraft.has_value()
			&& implementation->WorkbenchHost->outstanding_requests() == 0U) {
			auto* const view = implementation->Window->files_drawer()->contents_view();
			if (view != nullptr && view->model() != nullptr
				&& view->model()->rowCount() > 0
				&& !view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
			}
			return result::pending;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->ReviewMutationCoordinator == nullptr
			|| !implementation->ReviewDraft.has_value()
			|| !implementation->LastAcceptedHandoff.has_value()
			|| !implementation->LastSelectedDiff.has_value()
			|| implementation->Window->review_editor() == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe] {
			probe.Failed = true;
			return result::failed;
		};
		auto* const editor     = implementation->Window->review_editor();
		const auto  designated = github_application::implementation::review_mutation_target(
			*implementation->LastAcceptedHandoff);
		probe.Evidence.ExactDesignatedTarget
			= implementation->ReviewMutationCoordinator->designated() == designated;
		probe.Evidence.EditorComposed = editor->shown_draft().has_value()
			&& editor->shown_draft()->Anchor.Comparison
				== core::comparison_digest(
					implementation->LastSelectedDiff->Key)
			&& editor->target_label()->text().contains(
				QString::number(implementation->LastAcceptedHandoff->Number.value()));
		probe.Evidence.CapabilityFallback
			= !implementation->ReviewMutationCoordinator->capabilities().ResolvesThreads
			&& !implementation->ReviewMutationCoordinator->capabilities()
					.RecordsViewedState
			&& editor->mutation_status_text().contains(
				QStringLiteral("unavailable"));
		if (!probe.Evidence.ExactDesignatedTarget
			|| !probe.Evidence.EditorComposed
			|| !probe.Evidence.CapabilityFallback) {
			return fail();
		}

		const auto draftIntents = editor->statistics().DraftIntents;
		editor->body_editor()->setPlainText(
			QStringLiteral("BPR-0229 fixture publication."));
		probe.Evidence.SemanticEdit
			= editor->statistics().DraftIntents == draftIntents + 1U
			&& implementation->ReviewDraft->Body
				== "BPR-0229 fixture publication.";
		if (!probe.Evidence.SemanticEdit) {
			return fail();
		}

		auto       moved      = designated;
		const auto secondHead = core::parse_git_commit_oid(
			implementation->Fixture->evidence().HeadTwoOid);
		if (!secondHead.has_value() || *secondHead == designated.ExpectedHead) {
			return fail();
		}
		moved.ExpectedHead = *secondHead;
		implementation->Fixture->report_review_mutation_target(moved);
		editor->publish_action()->trigger();
		probe.Evidence.StaleHeadRefused
			= implementation->ReviewDraft->State
				== core::review_draft_state::drafting
			&& implementation->ReviewMutationCoordinator->statistics().Submissions == 0U
			&& implementation->ReviewMutationCoordinator->statistics()
					.RefusedBeforeSubmit
				== 1U
			&& editor->mutation_status_text().contains(QStringLiteral("head moved"));
		if (!probe.Evidence.StaleHeadRefused) {
			return fail();
		}

		implementation->Fixture->report_review_mutation_target(designated);
		editor->publish_action()->trigger();
		const auto lost                    = implementation->Fixture->review_mutation_evidence();
		probe.Evidence.LostResultRefreshed = lost.LostResult
			&& lost.StoredComments == 1U
			&& implementation->ReviewProjection
				== implementation->Fixture->review_comments()
			&& implementation->ReviewDraft->State
				== core::review_draft_state::drafting
			&& editor->mutation_status_text().contains(
				QStringLiteral("retry is safe"));
		if (!probe.Evidence.LostResultRefreshed) {
			return fail();
		}

		editor->publish_action()->trigger();
		const auto provider = implementation->Fixture->review_mutation_evidence();
		const auto statistics
			= implementation->ReviewMutationCoordinator->statistics();
		probe.Evidence.RetryIdempotent = provider.Submissions == 2U
			&& provider.StoredComments == 1U && provider.DuplicateReturned
			&& statistics.Submissions == 2U && statistics.Duplicates == 1U
			&& implementation->ReviewDraft->State
				== core::review_draft_state::published;
		probe.Evidence.ServerAuthority
			= implementation->ReviewProjection
				== implementation->Fixture->review_comments()
			&& implementation->ReviewProjection.size() == 1U
			&& implementation->ReviewProjection.front().Body
				== "BPR-0229 fixture publication.";
		probe.Evidence.TypedQtProjection
			= editor->shown_draft() == implementation->ReviewDraft
			&& editor->mutation_status_text().contains(
				QStringLiteral("safe retry"))
			&& editor->statistics().MutationUpdates >= 4U;
		if (!probe.Evidence.RetryIdempotent || !probe.Evidence.ServerAuthority
			|| !probe.Evidence.TypedQtProjection) {
			return fail();
		}
		probe.Complete = true;
		return result::passed;
	}

	auto detail::github_application_fixture_access::review_round_trip_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_round_trip_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->ReviewRoundTripCompositionProbe.has_value()
			? implementation->ReviewRoundTripCompositionProbe->Evidence
			: detail::review_round_trip_composition_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::exercise_defect_composition(
		github_application&                    aApplication,
		const detail::defect_composition_probe aCourse)
		-> detail::defect_composition_probe_result
	{
		using result               = detail::defect_composition_probe_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->DefectCompositionProbe.has_value()) {
			implementation->DefectCompositionProbe.emplace();
			implementation->DefectCompositionProbe->Course = aCourse;
		}
		auto& probe = *implementation->DefectCompositionProbe;
		if (probe.Phase
			== github_application::implementation::defect_composition_probe_state::phase::failed) {
			return result::failed;
		}
		if (probe.Phase
			== github_application::implementation::defect_composition_probe_state::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture == nullptr || implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| implementation->ReviewMutationCoordinator == nullptr
			|| implementation->Window->defect_card() == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe] {
			probe.Phase = github_application::implementation::defect_composition_probe_state::phase::failed;
			return result::failed;
		};
		auto* const window               = implementation->Window.get();
		const auto  firstIdentity        = std::string{ "review-thread:PRRT_fixture_1" };
		const auto  secondIdentity       = std::string{ "review-thread:PRRT_fixture_2" };
		const auto  fileIdentity         = std::string{ "review-thread:PRRT_fixture_file" };
		const auto  firstDefectIdentity  = defect_identity(firstIdentity);
		const auto  secondDefectIdentity = defect_identity(secondIdentity);
		const auto  findDefect           = [implementation](const local_defect_identity aIdentity)
			-> const application::durable_defect_record* {
			const auto found = std::ranges::find_if(
				implementation->DurableDefects,
				[aIdentity](const application::durable_defect_record& aDefect) {
					return aDefect.Conversation.value() == aIdentity.Conversation
						&& aDefect.History.Defect.Id.value() == aIdentity.Defect;
				});
			return found == implementation->DurableDefects.end() ? nullptr : &*found;
		};

		if (aCourse == detail::defect_composition_probe::reopen) {
			const auto* const first  = findDefect(firstDefectIdentity);
			const auto* const second = findDefect(secondDefectIdentity);
			if (first == nullptr || second == nullptr) {
				return result::pending;
			}
			probe.Evidence.ExactThreads = implementation->accepted_thread(firstIdentity)
				&& implementation->accepted_thread(secondIdentity);
			probe.Evidence.LocalClassification
				= first->History.Defect.Severity == core::defect_severity::critical
				&& first->History.Defect.Category == core::defect_category::security;
			probe.Evidence.CorrelatedEvidence
				= second->History.Defect.State == core::defect_state::verified
				&& second->History.Availability
					== core::defect_anchor_availability::outdated;
			probe.Evidence.ExplicitVerification
				= first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State == core::defect_state::verified;
			probe.Evidence.DurableHistory
				= second->History.Availability
					== core::defect_anchor_availability::outdated
				&& second->History.Defect.Anchor == second->History.OriginalAnchor;
			const auto before = window->defect_card()->shown_defect();
			if (window->next_defect_action()->isEnabled()) {
				window->next_defect_action()->trigger();
			} else if (window->previous_defect_action()->isEnabled()) {
				window->previous_defect_action()->trigger();
			}
			probe.Evidence.MultiDefectNavigation = window->defect_count() == 2U
				&& before.has_value()
				&& window->defect_card()->shown_defect().has_value()
				&& window->defect_card()->shown_defect()->Id != before->Id;
			probe.Evidence.SharedActionSeparate
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			const auto statistics            = implementation->ReviewLedgerHost->statistics();
			probe.Evidence.RestartRestored   = statistics.PrivateLoads >= 1U;
			probe.Evidence.ZeroReopenWrites  = statistics.PrivateWrites == 0U;
			probe.Evidence.VisibleProjection = window->defect_count_label()->text().contains(
				QStringLiteral("/2"));
			if (probe.Evidence != detail::defect_composition_probe_evidence{ .ExactThreads = true, .LocalClassification = true, .CorrelatedEvidence = true, .ExplicitVerification = true, .DurableHistory = true, .MultiDefectNavigation = true, .SharedActionSeparate = true, .RestartRestored = true, .ZeroReopenWrites = true, .VisibleProjection = true }) {
				return fail();
			}
			probe.Phase = github_application::implementation::defect_composition_probe_state::phase::passed;
			return result::passed;
		}

		if (probe.Phase
			== github_application::implementation::defect_composition_probe_state::phase::awaiting_shared_ready) {
			auto* const editor = window->review_editor();
			if (editor == nullptr || !editor->shown_draft().has_value()) {
				return result::pending;
			}
			probe.Evidence.SharedActionSeparate
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			if (!probe.Evidence.SharedActionSeparate) {
				return fail();
			}
			probe.Evidence.RestartRestored  = false;
			probe.Evidence.ZeroReopenWrites = false;
			probe.Phase                     = github_application::implementation::
				defect_composition_probe_state::phase::passed;
			editor->body_editor()->setPlainText(
				QStringLiteral("Explicit shared follow-up after local classification."));
			editor->publish_action()->trigger();
			return result::passed;
		}

		auto* const model       = window->conversation_model();
		auto* const view        = window->conversation_view();
		const auto  firstIndex  = model->index_for_id(firstIdentity);
		const auto  secondIndex = model->index_for_id(secondIdentity);
		const auto  fileIndex   = model->index_for_id(fileIdentity);
		if (!firstIndex.isValid() || !secondIndex.isValid() || !fileIndex.isValid()) {
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::defect_composition_probe_state::phase::awaiting_ready) {
			if (!implementation->DurableDefects.empty()) {
				return fail();
			}
			auto* const files = window->files_drawer()->contents_view();
			if (implementation->WorkbenchHost == nullptr
				|| implementation->WorkbenchHost->outstanding_requests() != 0U
				|| implementation->WorkbenchHost->actionable().empty()
				|| files == nullptr || files->model() == nullptr
				|| files->model()->rowCount() == 0) {
				return result::pending;
			}
			files->setCurrentIndex(files->model()->index(0, 0));
			probe.Phase = github_application::implementation::
				defect_composition_probe_state::phase::awaiting_diff;
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::defect_composition_probe_state::phase::awaiting_diff) {
			if (!implementation->SelectedDiffPublication.has_value()) {
				return result::pending;
			}
			view->setCurrentIndex(firstIndex);
			if (!window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			view->setCurrentIndex(secondIndex);
			window->classify_defect_action()->trigger();
			view->setCurrentIndex(fileIndex);
			if (window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			const auto* const first     = findDefect(firstDefectIdentity);
			const auto* const second    = findDefect(secondDefectIdentity);
			probe.Evidence.ExactThreads = first != nullptr && second != nullptr;
			probe.Evidence.LocalClassification
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			probe.Evidence.CorrelatedEvidence = first != nullptr && second != nullptr
				&& first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State
					== core::defect_state::needs_verification;
			probe.Evidence.DurableHistory = second != nullptr
				&& second->History.Availability
					== core::defect_anchor_availability::outdated
				&& second->History.Defect.Anchor == second->History.OriginalAnchor;
			probe.Evidence.VisibleProjection = window->defect_count() == 2U
				&& window->defect_count_label()->text().contains(QStringLiteral("/2"));
			if (!probe.Evidence.ExactThreads || !probe.Evidence.LocalClassification
				|| !probe.Evidence.CorrelatedEvidence
				|| !probe.Evidence.DurableHistory
				|| !probe.Evidence.VisibleProjection) {
				return fail();
			}
			probe.BeforeComparison = core::comparison_digest(
				implementation->SelectedDiffPublication->Key);
			auto* const control  = window->epoch_selectors();
			const auto  leftRow  = control->left_selector()->currentIndex();
			const auto  rightRow = control->right_selector()->currentIndex();
			if (leftRow < 0 || rightRow < 0 || leftRow == rightRow) {
				return fail();
			}
			probe.ReverseLeftRow  = rightRow;
			probe.ReverseRightRow = leftRow;
			control->left_selector()->setCurrentIndex(rightRow);
			probe.Phase = github_application::implementation::
				defect_composition_probe_state::phase::awaiting_reverse_right;
			return result::pending;
		}

		if (probe.Phase
			== github_application::implementation::defect_composition_probe_state::phase::awaiting_reverse_right) {
			auto* const control = window->epoch_selectors();
			if (control->left_selector()->currentIndex() != probe.ReverseLeftRow) {
				control->left_selector()->setCurrentIndex(probe.ReverseLeftRow);
				return result::pending;
			}
			control->right_selector()->setCurrentIndex(probe.ReverseRightRow);
			probe.Phase = github_application::implementation::
				defect_composition_probe_state::phase::awaiting_epoch_change;
			return result::pending;
		}

		if (!implementation->SelectedDiffPublication.has_value()
			|| core::comparison_digest(implementation->SelectedDiffPublication->Key)
				== probe.BeforeComparison) {
			return result::pending;
		}
		const auto* first  = findDefect(firstDefectIdentity);
		const auto* second = findDefect(secondDefectIdentity);
		if (first == nullptr || second == nullptr
			|| first->History.Defect.State
				!= core::defect_state::open
			|| second->History.Defect.State
				!= core::defect_state::needs_verification) {
			return fail();
		}
		while (window->defect_card()->shown_defect().has_value()
			   && window->defect_card()->shown_defect()->Id != second->History.Defect.Id) {
			if (window->previous_defect_action()->isEnabled()) {
				window->previous_defect_action()->trigger();
			} else if (window->next_defect_action()->isEnabled()) {
				window->next_defect_action()->trigger();
			} else {
				return fail();
			}
		}
		window->defect_card()->verify_action()->trigger();
		first                               = findDefect(firstDefectIdentity);
		second                              = findDefect(secondDefectIdentity);
		probe.Evidence.ExplicitVerification = first != nullptr && second != nullptr
			&& first->History.Defect.State == core::defect_state::open
			&& second->History.Defect.State == core::defect_state::verified;
		if (!probe.Evidence.ExplicitVerification) {
			return fail();
		}
		while (window->defect_card()->shown_defect().has_value()
			   && window->defect_card()->shown_defect()->Id != first->History.Defect.Id) {
			if (window->previous_defect_action()->isEnabled()) {
				window->previous_defect_action()->trigger();
			} else if (window->next_defect_action()->isEnabled()) {
				window->next_defect_action()->trigger();
			} else {
				return fail();
			}
		}
		auto* const card = window->defect_card();
		card->severity_selector()->setCurrentIndex(
			card->severity_selector()->findData(
				static_cast<int>(core::defect_severity::critical)));
		card->category_selector()->setCurrentIndex(
			card->category_selector()->findData(
				static_cast<int>(core::defect_category::security)));
		card->reclassify_action()->trigger();
		first                         = findDefect(firstDefectIdentity);
		probe.Evidence.DurableHistory = probe.Evidence.DurableHistory
			&& first != nullptr
			&& first->History.Defect.Severity == core::defect_severity::critical
			&& first->History.Defect.Category == core::defect_category::security;
		const auto before = card->shown_defect()->Id;
		if (window->next_defect_action()->isEnabled()) {
			window->next_defect_action()->trigger();
		} else {
			window->previous_defect_action()->trigger();
		}
		probe.Evidence.MultiDefectNavigation
			= card->shown_defect().has_value() && card->shown_defect()->Id != before;
		probe.Phase = github_application::implementation::
			defect_composition_probe_state::phase::awaiting_shared_ready;
		return result::pending;
	}

	auto detail::github_application_fixture_access::defect_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::defect_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->DefectCompositionProbe.has_value()
			? implementation->DefectCompositionProbe->Evidence
			: detail::defect_composition_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::select_appearance_mode(
		github_application&         aApplication,
		const core::appearance_mode aMode) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		implementation->AppearanceHost->select(aMode);
		return implementation->AppearanceHost->settings().Mode == aMode;
	}

	auto detail::github_application_fixture_access::review_ledger_receipt_present(
		github_application& aApplication) -> bool
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
			&& implementation->ReviewLedgerWasPresentOnEntry;
	}

	auto detail::github_application_fixture_access::review_ledger_round_trip(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr) {
			return false;
		}
		if (implementation->ReviewLedgerRoundTripPassed) {
			return true;
		}

		auto* const window = implementation->Window.get();
		auto* const view   = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		if (!implementation->LastSelectedDiff.has_value()
			|| !implementation->ReviewLedgerHost->retained().Material.valid()) {
			if (view != nullptr && view->model() != nullptr
				&& view->model()->rowCount() > 0 && !view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
			}
			return false;
		}

		const auto statistics = implementation->ReviewLedgerHost->statistics();
		implementation->ReviewLedgerWasPresentOnEntry
			= statistics.ReceiptLoadHits > 0U;
		const auto& selected = *implementation->LastSelectedDiff;
		const auto  material = core::comparison_digest(selected.Key);
		const auto  before   = implementation->ReviewLedgerHost->retained();
		if (before.File != selected.Request.File.File
			|| before.Material != material) {
			return false;
		}
		if (before.Read == core::file_read_state::unread) {
			if (window->seen_action() == nullptr
				|| !window->seen_action()->isEnabled()) {
				return false;
			}
			window->seen_action()->trigger();
		}
		const auto after = implementation->ReviewLedgerHost->retained();
		implementation->ReviewLedgerRoundTripPassed
			= after.File == before.File && after.Material == before.Material
			&& after.Read != core::file_read_state::unread;
		return implementation->ReviewLedgerRoundTripPassed;
	}

	auto detail::github_application_fixture_access::request_fixture_diff(
		github_application& aApplication,
		const bool          aUseSecondHead) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation != nullptr) {
			++implementation->FixtureDiffHelperInvocations;
		}
		if (
			implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->DiffHost == nullptr) {
			return false;
		}
		const auto& state = implementation->EpochHost->state();
		if (!state.Authority.has_value() || !state.AcceptedComparison.has_value()) {
			return false;
		}
		const auto evidence     = implementation->Fixture->evidence();
		const auto expectedHead = aUseSecondHead ? evidence.HeadTwoOid : evidence.HeadOneOid;
		const auto objectFor    = [&evidence](const core::git_commit_oid aCommit)
			-> std::optional<core::git_blob_oid> {
			const auto rendered = core::canonical_hex(aCommit).view();
			if (rendered == evidence.BaseOid) {
				return core::parse_git_blob_oid(evidence.BaseBlobOid);
			}
			if (rendered == evidence.HeadOneOid) {
				return core::parse_git_blob_oid(evidence.HeadOneBlobOid);
			}
			if (rendered == evidence.HeadTwoOid) {
				return core::parse_git_blob_oid(evidence.HeadTwoBlobOid);
			}
			return std::nullopt;
		};
		if (
			core::canonical_hex(state.AcceptedComparison->Right.Commit).view()
			!= expectedHead) {
			return false;
		}
		const auto leftBlob  = objectFor(state.AcceptedComparison->Left.Commit);
		const auto rightBlob = objectFor(state.AcceptedComparison->Right.Commit);
		const auto options   = core::diff_options{ };
		const auto digest    = core::options_digest(options);
		if (!leftBlob.has_value() || !rightBlob.has_value() || !digest.has_value()) {
			return false;
		}
		auto                              fileBytes = std::array<std::uint8_t, 32U>{ };
		static constexpr std::string_view identity  = "fixture/review.cpp/v1";
		std::ranges::copy(identity, fileBytes.begin());
		implementation->request_diff(
			core::file_comparison_key{
				.Authority   = *state.Authority,
				.Repository  = state.Authority->Repository,
				.PullRequest = state.Authority->PullRequest,
				.LeftAnchor  = state.AcceptedComparison->Left.Identity,
				.RightAnchor = state.AcceptedComparison->Right.Identity,
				.LeftBlob    = leftBlob,
				.RightBlob   = rightBlob,
				.File        = core::file_identity{ fileBytes },
				.Options     = *digest,
				.Algorithm   = core::supported_diff_algorithm_revision,
			},
			options,
			// The fixture repository's single file. Named here because the
			// comparison key carries a digest and the syntax definition needs
			// a name, and BPR-0197 made that name a source file so the
			// classifier selects C++ rather than plain text.
			"review.cpp");
		return implementation->DiffHost->state().Pending.has_value();
	}

	auto detail::github_application_fixture_access::reaccept_fixture_revision(
		github_application& aApplication,
		const bool          aChangeHead) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Fixture == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return false;
		}
		auto handoff = clone_without_smart_git(
			*implementation->LastAcceptedHandoff);
		handoff.Snapshot = core::provider_snapshot_id{
			handoff.Snapshot.value() + 1U
		};
		handoff.PullRequestGeneration = core::request_generation{
			handoff.PullRequestGeneration.value() + 1U
		};
		if (aChangeHead) {
			const auto head = core::parse_git_commit_oid(
				implementation->Fixture->evidence().HeadTwoOid);
			if (!head.has_value()) {
				return false;
			}
			handoff.Head = *head;
		}
		if (!application::valid(handoff)) {
			return false;
		}
		implementation->accept(std::move(handoff));
		return true;
	}
#endif
}
