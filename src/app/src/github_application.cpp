#include "bpr/app/github_application.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"

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
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/application/workbench_snapshot_observation.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/sensitive_text_input.hpp"
#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPalette>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
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
		using presentation_sink = std::function<void()>;
		using refusal_sink      = std::function<void(
			const application::appearance_failure_publication&)>;

		void set_span_sink(span_sink aSink)
		{
			SpanSink = std::move(aSink);
		}

		void set_presentation_sink(presentation_sink aSink)
		{
			PresentationSink = std::move(aSink);
		}

		void set_refusal_sink(refusal_sink aSink)
		{
			RefusalSink = std::move(aSink);
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
				PresentationSink();
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
		span_sink         SpanSink;
		presentation_sink PresentationSink;
		refusal_sink      RefusalSink;
		std::size_t       Published = 0U;
		std::size_t       Failed    = 0U;
	};

	/// \brief Holds the workbench projections until a surface consumes them.
	///
	/// \details BPR-0190 composes the workbench host; BPR-0191 makes the shell
	/// the application's window. Between the two, the host is real - it
	/// restores a durable layout, runs its effects, and quiesces on shutdown -
	/// but nothing renders what it publishes, because `github_window` does not
	/// implement `workbench_projection_port` and `workbench_window` is not the
	/// application's window yet.
	///
	/// This sink exists so that intermediate state is stated rather than
	/// disguised. It retains the last projection and counts what it received,
	/// which is what the composition test asserts against. It is deliberately
	/// not a second presentation authority: it renders nothing and decides
	/// nothing, and BPR-0191 replaces it with the shell.
	class composition_workbench_projections final
		: public application::workbench_projection_port
	{
	public:
		void publish(const application::workbench_projection& aProjection) override
		{
			Last = aProjection;
			++Published;
		}

		void fail(const application::workbench_failure_publication& aFailure) override
		{
			LastFailure = aFailure;
			++Failed;
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
		std::optional<application::workbench_projection>          Last;
		std::optional<application::workbench_failure_publication> LastFailure;
		std::size_t                                               Published = 0U;
		std::size_t                                               Failed    = 0U;
	};

	class github_application::implementation final
		: public application::github_epoch_handoff_port
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

			// The review ledger is a separate database beside the catalog, so
			// deleting private review state cannot take the derived git cache
			// with it. Composition failure is not fatal to the application:
			// the ledger holds private progress, and a reviewer with no ledger
			// can still read a pull request.
			auto ledger = adapters::sqlite::make_review_ledger_store({
				.StorageRoot = applicationDataRoot,
				.Host        = "github.com",
				.Account     = "composition",
				.Repository  = "composition",
				.PullRequest = 1,
			});
			if (ledger) {
				ReviewLedger = std::move(*ledger);
			}
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
				WorkbenchSettings = std::move(*layout);
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
				qt::workbench_intent_dispatch{ },
				qt::workbench_change_dispatch{ },
				qt::workbench_drawer_width_dispatch{ },
				ProfileOptions,
				[this](application::github_intent aIntent) { dispatch_intent(std::move(aIntent)); });

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
				*Window->epoch_selectors());
			Window->epoch_selectors()->set_dispatch(
				[this](application::epoch_intent aIntent) { dispatch_epoch_intent(std::move(aIntent)); });
			DiffHost = std::make_unique<application::diff_application_host>(
				core::diff_state{
					.ApplicationSession = core::application_session_id{ 1U },
				},
				*EpochHost,
				DiffEffects,
				DecisionLane,
				*Window->comparison_surface());

			// The workbench observes the review's files through the read model
			// the provider already produces. The supplier is read at call time
			// rather than captured, because the accepted snapshot changes as
			// the reviewer opens pull requests.
			WorkbenchObservations = std::make_unique<
				application::snapshot_workbench_observation>(
				[this](const core::pull_request_authority&)
					-> std::optional<
						std::vector<application::github_file_record>> {
					const auto& snapshot = Host->accepted_snapshot();
					if (!snapshot) {
						// Not held is not the same as no files. The port keeps
						// that distinction and so must this.
						return std::nullopt;
					}
					return snapshot->Files;
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

			// Composed last, because its observation supplier reads `Host`.
			// Only when a durable layout store exists: the host requires a
			// settings port, and a reviewer whose layout cannot be stored is
			// better served by the composed defaults than by no workbench.
			if (WorkbenchSettings) {
				WorkbenchHost = std::make_unique<
					application::workbench_application_host>(
					core::workbench_interaction_state{ },
					*WorkbenchObservations,
					*WorkbenchSettings,
					WorkbenchEffects,
					DecisionLane,
					WorkbenchProjections,
					core::application_session_id{ 1U });
				// Reads the durable layout. The host's own contract is that
				// restoring never writes it back, so a launch that only looks
				// leaves a fresh installation looking fresh.
				WorkbenchHost->restore();
			}

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
				[this] { adopt_appearance_settings(); });
			AppearanceProjections.set_refusal_sink(
				[this](const application::appearance_failure_publication&
						   aFailure) { report_appearance_refusal(aFailure); });
			AppearanceHost = std::make_unique<
				application::appearance_application_host>(
				qt::default_appearance_settings(),
				*AppearanceSettings,
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

		void accept(application::accepted_pull_request_handoff aHandoff) override
		{
			if (!Closing && EpochHost != nullptr) {
#if defined(BPR_BUILD_TESTING)
				LastAcceptedHandoff = clone_without_smart_git(aHandoff);
#endif
				if (!preserves_selected_diff(EpochHost->state(), aHandoff)) {
					clear_diff_selection();
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

		void clear_diff_selection()
		{
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
					} else {
						AppearanceHost->apply(std::move(aValue.Typography));
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
		/// The palette is deliberately not pushed. No card has designed a
		/// palette-customisation gesture, so the profile's built-in palette for
		/// the resolved variant is still the whole story, and pretending
		/// otherwise here would invent a path nothing sends.
		void adopt_appearance_settings()
		{
			if (Closing || AppearanceHost == nullptr || !AppearanceProfile) {
				return;
			}
			const auto settings = AppearanceHost->settings();
			static_cast<void>(AppearanceProfile->select_mode(settings.Mode));
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
					panel->adopt(settings);
				}
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
				if (!AppearancePersistenceAvailable
					&& aFailure.Failure
						== application::appearance_host_failure::settings_unavailable) {
					panel->report_persistence_unavailable();
				} else {
					panel->report(aFailure.Rejections);
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
				|| (AppearanceHost && !AppearanceHost->is_quiescent())) {
				schedule_shutdown_poll();
				return;
			}

			ShutdownCleanupFailed         = Host->shutdown_cleanup_failed();
			EpochProjectionDeliveryFailed = EpochHost->projection_delivery_failed();
			AppearanceEffects.close_and_drain();
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
#endif
		std::unique_ptr<application::github_credential_vault_port>   OwnedVault;
		std::shared_ptr<github_wire_transport_factory_port>          TransportFactory;
		std::unique_ptr<github_provider>                             Provider;
		qt::event_loop_executor                                      DecisionLane;
		std::unique_ptr<qt::workbench_window>                        Window;
		application::github_credential_binding_catalog               CredentialBindings;
		std::unique_ptr<application::git_cache_port>                 GitCaches;
		std::unique_ptr<application::epoch_catalog_port>             EpochCatalogs;
		std::unique_ptr<application::review_ledger_store_port>       ReviewLedger;
		std::unique_ptr<application::workbench_settings_port>        WorkbenchSettings;
		std::unique_ptr<application::snapshot_workbench_observation> WorkbenchObservations;
		std::unique_ptr<application::appearance_settings_port>       AppearanceSettings;
		// Built once and shared. The catalog is immutable after construction
		// and the classifier holds no theme, palette, or font, so neither has
		// to be rebuilt when appearance changes.
		std::unique_ptr<adapters::syntax::packaged_definition_catalog> SyntaxDefinitions;
		std::unique_ptr<composition_blob_source>                       SyntaxBlobs;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>  SyntaxClassification;
		composition_appearance_projections                             AppearanceProjections;
		std::unique_ptr<qt::appearance_profile>                        AppearanceProfile;
		std::string                                                    AppliedSelectionColor;
		std::string                                                    AppliedBaseColor;
		bool                                                           AppliedFromProfile             = false;
		bool                                                           AppearancePersistenceAvailable = false;
		std::unique_ptr<application::epoch_entropy_port>               EpochEntropy;
		std::unique_ptr<application::epoch_clock_port>                 EpochClock;
		worker_executor                                                EpochEffects;
		worker_executor                                                DiffEffects;
		worker_executor                                                WorkbenchEffects;
		worker_executor                                                AppearanceEffects;
		composition_workbench_projections                              WorkbenchProjections;
		std::unique_ptr<application::epoch_application_host>           EpochHost;
		std::unique_ptr<application::diff_application_host>            DiffHost;
		std::unique_ptr<application::workbench_application_host>       WorkbenchHost;
		std::unique_ptr<application::appearance_application_host>      AppearanceHost;
		std::unique_ptr<application::github_application_host>          Host;
		bool                                                           Closing                       = false;
		bool                                                           ShutdownPollScheduled         = false;
		bool                                                           ShutdownComplete              = false;
		bool                                                           ShutdownCleanupFailed         = false;
		bool                                                           NativePlatformVaultSelected   = false;
		bool                                                           EpochProjectionDeliveryFailed = false;
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
		/// \brief The fixed identity the ledger probe writes and reads back.
		[[nodiscard]] auto probe_receipt() -> core::file_receipt
		{
			auto file     = std::array<std::uint8_t, 32>{ };
			auto material = std::array<std::uint8_t, 32>{ };
			file.fill(0xA7U);
			material.fill(0x5CU);
			return core::file_receipt{
				.File     = core::file_identity{ file },
				.Material = core::file_material_id{ material },
				.Read     = core::file_read_state::read,
				.Accepted = true,
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
		if (aApplication.Implementation == nullptr
			|| aApplication.Implementation->ReviewLedger == nullptr) {
			return false;
		}
		const auto loaded
			= aApplication.Implementation->ReviewLedger->load(probe_receipt().File);
		return loaded.has_value() && loaded->has_value()
			&& **loaded == probe_receipt();
	}

	auto detail::github_application_fixture_access::review_ledger_round_trip(
		github_application& aApplication) -> bool
	{
		if (aApplication.Implementation == nullptr
			|| aApplication.Implementation->ReviewLedger == nullptr) {
			return false;
		}
		auto&      ledger  = *aApplication.Implementation->ReviewLedger;
		const auto receipt = probe_receipt();
		if (!ledger.commit(application::receipt_persistence_request{
				.Receipt = receipt })) {
			return false;
		}
		const auto loaded = ledger.load(receipt.File);
		return loaded.has_value() && loaded->has_value() && **loaded == receipt;
	}

	auto detail::github_application_fixture_access::request_fixture_diff(
		github_application& aApplication,
		const bool          aUseSecondHead) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
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
