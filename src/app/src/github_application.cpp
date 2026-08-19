#include "bpr/app/github_application.hpp"

#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "review_mutation_execution_authority.hpp"

#include "bpr/app/worker_executor.hpp"

#include "designated_review_public_observer.hpp"
#include "github_host_profile.hpp"
#include "github_provider.hpp"
#include "live_review_mutation_provider.hpp"
#include "native_https_transport.hpp"

#if defined(__APPLE__)
	#include "macos_keychain.hpp"
#elif defined(_WIN32)
	#include "windows_credential_manager.hpp"
#elif defined(__linux__)
	#include "linux_secret_service.hpp"
#endif

#if defined(BPR_BUILD_TESTING)
	#include "github_public_path_fixture.hpp"
	#include "github_read_contract.hpp"
	#include "github_wire_transport.hpp"
#endif

#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/defect_correlation.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/application/workbench_snapshot_observation.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/sensitive_text_input.hpp"
#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainterPathStroker>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFormat>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
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
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::app {
	namespace {
		using adapters::github::github_host_profile_candidate;
		using adapters::github::github_profile_provenance;
		using adapters::github::make_live_review_mutation_provider;
		using adapters::github::validate_github_host_profile;
		using adapters::github::validated_github_host_profile;
		using adapters::github::detail::github_provider;
		using adapters::github::detail::github_wire_transport_factory_port;
		using adapters::github::detail::make_github_provider;
		using adapters::github::detail::native_github_wire_transport_factory;

		inline constexpr auto designated_review_hostname = std::string_view{ "github.com" };
		inline constexpr auto designated_review_owner    = std::string_view{ "MikeMarcin" };
		inline constexpr auto designated_review_name     = std::string_view{ "BPR-Test" };

#if defined(BPR_BUILD_TESTING)
		/// Holds only review-mutation terminals for deterministic fixture races.
		class gated_review_mutation_decision_executor final
			: public application::decision_executor
		{
		public:
			explicit gated_review_mutation_decision_executor(
				application::decision_executor& aTarget)
				: Target{ aTarget }
			{
			}

			void post(std::function<void()> aTask) override
			{
				{
					const auto lock = std::scoped_lock{ Mutex };
					if (Holding) {
						Held.push_back(std::move(aTask));
						return;
					}
				}
				Target.post(std::move(aTask));
			}

			void hold()
			{
				const auto lock = std::scoped_lock{ Mutex };
				Holding         = true;
			}

			void release()
			{
				auto held = std::deque<std::function<void()>>{ };
				{
					const auto lock = std::scoped_lock{ Mutex };
					Holding         = false;
					held.swap(Held);
				}
				for (auto& task : held) {
					Target.post(std::move(task));
				}
			}

			[[nodiscard]] auto pending() const -> std::size_t
			{
				const auto lock = std::scoped_lock{ Mutex };
				return Held.size();
			}

		private:
			application::decision_executor&   Target;
			mutable std::mutex                Mutex;
			std::deque<std::function<void()>> Held;
			bool                              Holding = false;
		};
#endif
		inline constexpr auto designated_review_repository_id     = std::uint64_t{ 1'328'277'631U };
		inline constexpr auto designated_review_repository_node   = std::string_view{ "R_kgDOTyvofw" };
		inline constexpr auto designated_review_pull_request_id   = std::uint64_t{ 4'246'446'933U };
		inline constexpr auto designated_review_pull_request_node = std::string_view{ "PR_kwDOTyvof879G6NV" };
		inline constexpr auto designated_review_number            = std::uint32_t{ 1U };
		inline constexpr auto designated_review_base_ref          = std::string_view{ "main" };
		inline constexpr auto designated_review_base_full_ref     = std::string_view{ "refs/heads/main" };
		inline constexpr auto designated_review_head_ref          = std::string_view{ "codex/disposable-live-review-20260810" };
		inline constexpr auto designated_review_base_oid          = std::string_view{ "e2a44aa8fef704de7373463222b1872e796918c6" };
		inline constexpr auto designated_review_head_oid          = std::string_view{ "17990ba0c2fb50338141f35c0717f0f99639c7ba" };
		[[nodiscard]] auto    designated_review_replay_namespace_path(
			const std::filesystem::path& aApplicationDataRoot)
			-> std::filesystem::path
		{
			return (aApplicationDataRoot
					/ std::filesystem::path{ application::detail::
												 designated_review_replay_namespace_child_name })
				.lexically_normal();
		}

		struct designated_review_mutation_selection
		{
			application::review_mutation_target_binding  Target;
			application::review_mutation_acting_identity ActingIdentity;
		};

		[[nodiscard, maybe_unused]] auto select_designated_review_mutation(
			const application::accepted_pull_request_handoff& aHandoff,
			const application::github_pull_request_snapshot&  aSnapshot)
			-> std::optional<designated_review_mutation_selection>
		{
			const auto expectedBase = core::parse_git_commit_oid(designated_review_base_oid);
			const auto expectedHead = core::parse_git_commit_oid(designated_review_head_oid);
			const auto snapshotBase = core::parse_git_commit_oid(
				aSnapshot.PullRequest.BaseOid.value());
			const auto snapshotHead = core::parse_git_commit_oid(
				aSnapshot.PullRequest.HeadOid.value());
			if (!expectedBase.has_value() || !expectedHead.has_value()
				|| !snapshotBase.has_value() || !snapshotHead.has_value()
				|| aHandoff.Hostname.value() != designated_review_hostname
				|| aHandoff.ServerContract
					!= application::github_server_contract::github_com_2022_11_28
				|| aHandoff.Locator.Owner != designated_review_owner
				|| aHandoff.Locator.Name != designated_review_name
				|| aHandoff.Repository.value() != designated_review_repository_id
				|| aHandoff.PullRequest.value() != designated_review_pull_request_id
				|| aHandoff.Number.value() != designated_review_number
				|| aHandoff.Base != *expectedBase || aHandoff.Head != *expectedHead
				|| aHandoff.BaseRef.view() != designated_review_base_full_ref
				|| aSnapshot.Profile != aHandoff.Profile
				|| aSnapshot.Host != aHandoff.Host
				|| aSnapshot.Account != aHandoff.Account
				|| aSnapshot.Viewer.Actor.value().empty()
				|| aSnapshot.Viewer.Login.empty()
				|| aSnapshot.Repository.Repository != aHandoff.Repository
				|| aSnapshot.Repository.Repository.value()
					!= designated_review_repository_id
				|| aSnapshot.Repository.Node.value() != designated_review_repository_node
				|| aSnapshot.Repository.Owner != designated_review_owner
				|| aSnapshot.Repository.Name != designated_review_name
				|| aSnapshot.PullRequest.PullRequest != aHandoff.PullRequest
				|| aSnapshot.PullRequest.PullRequest.value()
					!= designated_review_pull_request_id
				|| aSnapshot.PullRequest.Repository != aHandoff.Repository
				|| aSnapshot.PullRequest.Number != aHandoff.Number
				|| aSnapshot.PullRequest.Node.value()
					!= designated_review_pull_request_node
				|| aSnapshot.PullRequest.State
					!= application::github_pull_request_state::open
				|| *snapshotBase != *expectedBase || *snapshotHead != *expectedHead
				|| aSnapshot.PullRequest.BaseRef != designated_review_base_ref
				|| aSnapshot.PullRequest.HeadRef != designated_review_head_ref) {
				return std::nullopt;
			}

			auto selection = designated_review_mutation_selection{
				.Target = {
					.Semantic = {
						.Host         = aHandoff.Host,
						.Repository   = aHandoff.Repository,
						.PullRequest  = aHandoff.PullRequest,
						.ExpectedHead = *expectedHead,
					},
					.Hostname       = application::github_canonical_hostname{ designated_review_hostname },
					.ServerContract = application::github_server_contract::github_com_2022_11_28,
					.Repository     = {
						.Owner = std::string{ designated_review_owner },
						.Name  = std::string{ designated_review_name },
					},
					.Number                   = aHandoff.Number,
					.RepositoryDatabaseId     = aSnapshot.Repository.Repository.value(),
					.RepositoryNodeId         = std::string{ aSnapshot.Repository.Node.value() },
					.PullRequestDatabaseId    = aSnapshot.PullRequest.PullRequest.value(),
					.PullRequestNodeId        = std::string{ aSnapshot.PullRequest.Node.value() },
					.BaseRepositoryDatabaseId = aSnapshot.PullRequest.Repository.value(),
					.BaseOid                  = *expectedBase,
					.BaseRef                  = std::string{ designated_review_base_ref },
					.HeadRef                  = std::string{ designated_review_head_ref },
					.Open                     = true,
				},
				.ActingIdentity = {
					.Account    = aHandoff.Account,
					.DatabaseId = aSnapshot.Account.value(),
					.NodeId     = std::string{ aSnapshot.Viewer.Actor.value() },
					.Login      = aSnapshot.Viewer.Login,
				},
			};
			if (!application::valid(selection.Target)
				|| !application::valid(selection.ActingIdentity)) {
				return std::nullopt;
			}
			return selection;
		}

#if defined(BPR_BUILD_TESTING)
		[[nodiscard]] auto select_fixture_review_mutation(
			const application::accepted_pull_request_handoff& aHandoff,
			const application::github_pull_request_snapshot&  aSnapshot)
			-> std::optional<designated_review_mutation_selection>
		{
			const auto base = core::parse_git_commit_oid(
				aSnapshot.PullRequest.BaseOid.value());
			const auto head = core::parse_git_commit_oid(
				aSnapshot.PullRequest.HeadOid.value());
			if (!base.has_value() || !head.has_value()
				|| *base != aHandoff.Base || *head != aHandoff.Head
				|| aSnapshot.Profile != aHandoff.Profile
				|| aSnapshot.Host != aHandoff.Host
				|| aSnapshot.Account != aHandoff.Account
				|| aSnapshot.Repository.Repository != aHandoff.Repository
				|| aSnapshot.Repository.Owner != aHandoff.Locator.Owner
				|| aSnapshot.Repository.Name != aHandoff.Locator.Name
				|| aSnapshot.PullRequest.PullRequest != aHandoff.PullRequest
				|| aSnapshot.PullRequest.Repository != aHandoff.Repository
				|| aSnapshot.PullRequest.Number != aHandoff.Number
				|| aSnapshot.PullRequest.State
					!= application::github_pull_request_state::open) {
				return std::nullopt;
			}
			auto selection = designated_review_mutation_selection{
				.Target = {
					.Semantic = {
						.Host         = aHandoff.Host,
						.Repository   = aHandoff.Repository,
						.PullRequest  = aHandoff.PullRequest,
						.ExpectedHead = *head,
					},
					.Hostname                 = aHandoff.Hostname,
					.ServerContract           = aHandoff.ServerContract,
					.Repository               = aHandoff.Locator,
					.Number                   = aHandoff.Number,
					.RepositoryDatabaseId     = aSnapshot.Repository.Repository.value(),
					.RepositoryNodeId         = std::string{ aSnapshot.Repository.Node.value() },
					.PullRequestDatabaseId    = aSnapshot.PullRequest.PullRequest.value(),
					.PullRequestNodeId        = std::string{ aSnapshot.PullRequest.Node.value() },
					.BaseRepositoryDatabaseId = aSnapshot.PullRequest.Repository.value(),
					.BaseOid                  = *base,
					.BaseRef                  = aSnapshot.PullRequest.BaseRef,
					.HeadRef                  = aSnapshot.PullRequest.HeadRef,
					.Open                     = true,
				},
				.ActingIdentity = {
					.Account    = aHandoff.Account,
					.DatabaseId = aSnapshot.Account.value(),
					.NodeId     = std::string{ aSnapshot.Viewer.Actor.value() },
					.Login      = aSnapshot.Viewer.Login,
				},
			};
			return application::valid(selection.Target)
					&& application::valid(selection.ActingIdentity)
				? std::optional{ std::move(selection) }
				: std::nullopt;
		}
#endif

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

		struct queue_notification_transition
		{
			application::queue_session_id Session;
			core::pull_request_ref        Ref;
			core::attention_bucket        Bucket
				= core::attention_bucket::quiet;

			auto operator<=>(const queue_notification_transition&) const = default;
		};

		struct queue_activity_baseline
		{
			application::queue_session_id Session;
			core::pull_request_ref        Ref;
			core::git_commit_oid          Head;
		};

		struct queue_activity_fact
		{
			application::queue_session_id    Session;
			core::pull_request_ref           Ref;
			core::queue_activity_observation Activity;
		};

#if defined(BPR_BUILD_TESTING)
		class fixture_notification_sink final
			: public application::notification_sink_port
		{
		public:
			[[nodiscard]] auto permission()
				-> application::notification_permission override
			{
				return application::notification_permission::granted;
			}

			[[nodiscard]] auto request_permission()
				-> application::notification_permission override
			{
				return application::notification_permission::granted;
			}

			[[nodiscard]] auto deliver(
				const application::delivered_notification& aPayload)
				-> application::notification_submission_result override
			{
				if (Payload.has_value()) {
					return application::notification_submission_result::
						suppressed_unattributable_activation;
				}
				Payload = aPayload;
				return application::notification_submission_result::submitted;
			}

			void set_activation_sink(
				application::notification_activation_sink aSink)
			{
				Activation = std::move(aSink);
			}

			[[nodiscard]] auto activate() -> bool
			{
				if (!Payload.has_value() || ActivationConsumed || !Activation) {
					return false;
				}
				ActivationConsumed = true;
				Activation(Payload->Target);
				return true;
			}

			[[nodiscard]] auto submitted_payload() const
				-> std::optional<application::delivered_notification>
			{
				return Payload;
			}

		private:
			application::notification_activation_sink          Activation;
			std::optional<application::delivered_notification> Payload;
			bool                                               ActivationConsumed = false;
		};
#endif

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
			// `std::filesystem::u8path` is deprecated in C++20. A `char8_t` view
			// over the same bytes selects the identical UTF-8 interpretation.
			const auto utf8 = std::u8string_view{ reinterpret_cast<const char8_t*>(aPath.data()), aPath.size() };
			const auto path = std::filesystem::path{ utf8 };
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
	/// cache session was opened for, so each submitted blob is bound to the
	/// request that reads it. Registrations are written on the decision lane and
	/// read on the appearance effect executor, which is why they are guarded.
	///
	/// One binding per blob rather than one mutable comparison. The single
	/// retained context was latest-write-wins, so a classification that began
	/// late read whichever comparison had since become current instead of the one
	/// it was submitted for. Binding is insert-if-absent: a blob is
	/// content-addressed, so two comparisons naming it want identical bytes, and
	/// keeping the first binding means no later display can move a read that is
	/// already in flight. BPR-0390.
	///
	/// The retained values are comparisons, never classifications: this class
	/// holds no span, decides no role, and would be useless to anything that
	/// wanted to skip the classifier.
	class composition_blob_source final : public adapters::syntax::blob_source
	{
	public:
		explicit composition_blob_source(application::diff_blob_port& aBlobs)
			: Blobs{ aBlobs }
		{
		}

		/// \brief Binds each of a comparison's blobs to the request that reads it.
		void register_comparison(const core::diff_request_context& aContext)
		{
			const auto guard = std::lock_guard{ Mutex };
			for (const auto side :
				 { core::diff_side::left, core::diff_side::right }) {
				const auto& blob = side == core::diff_side::left
					? aContext.Key.LeftBlob
					: aContext.Key.RightBlob;
				if (!blob.has_value() || find_request(*blob) != Requests.end()) {
					continue;
				}
				if (Requests.size() >= MaximumRequests) {
					// Refused rather than evicted. Evicting would move a read
					// that is already in flight, which is the defect this
					// replaced; refusing makes the read report unavailable, the
					// classification fail, and a later display resubmit.
					continue;
				}
				Requests.push_back(application::diff_blob_request{
					.Context      = aContext,
					.Side         = side,
					.Blob         = *blob,
					.MaximumBytes = adapters::syntax::maximum_classified_blob_bytes,
				});
			}
		}

		/// \brief Releases one blob's binding once its classification is over.
		void release_blob(const core::git_blob_oid& aBlob)
		{
			const auto guard = std::lock_guard{ Mutex };
			if (const auto known = find_request(aBlob); known != Requests.end()) {
				Requests.erase(known);
			}
		}

		void clear_comparison()
		{
			const auto guard = std::lock_guard{ Mutex };
			Requests.clear();
		}

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid& aBlob)
			-> std::optional<std::string> override
		{
			const auto request = [this, &aBlob] {
				const auto guard = std::lock_guard{ Mutex };
				const auto known = find_request(aBlob);
				return known == Requests.end()
					? std::optional<application::diff_blob_request>{ }
					: std::optional{ *known };
			}();
			if (!request.has_value()) {
				// A blob nothing bound belongs to a comparison the reviewer has
				// left, or to one whose binding was refused at the bound. Side
				// is carried by the binding rather than derived here, so it can
				// no longer be answered from a comparison that arrived later.
				return std::nullopt;
			}
			const auto observation
				= Blobs.materialize(*request, std::stop_token{ });
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
		// Two blobs per comparison, so eight comparisons' worth of concurrent
		// classification. Past that a read is refused rather than mis-bound.
		static constexpr auto MaximumRequests = std::size_t{ 16U };

		[[nodiscard]] auto find_request(const core::git_blob_oid& aBlob)
			-> std::vector<application::diff_blob_request>::iterator
		{
			return std::ranges::find_if(
				Requests,
				[&aBlob](const application::diff_blob_request& aRequest) {
					return aRequest.Blob == aBlob;
				});
		}

		application::diff_blob_port&                Blobs;
		std::mutex                                  Mutex;
		std::vector<application::diff_blob_request> Requests;
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

		auto admit_agent_session(
			const application::agent_session_admission&)
			-> std::expected<core::agent_session, application::ledger_store_failure> override
		{
			return std::unexpected(Failure);
		}

		auto load_agent_session_catalog()
			-> std::expected<
				application::agent_session_catalog_state,
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
		  public application::review_mutation_projection_port,
		  public application::receipt_projection_port,
		  public application::queue_landing_projection_port,
		  public application::agent_session_catalog_port
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
			if (aConfig.AttendedDesignatedLiveApproval.has_value()) {
				throw std::invalid_argument{
					"attended live approval is unavailable in a tests-enabled composition"
				};
			}
#else
			if (aConfig.AttendedDesignatedLiveApproval.has_value()) {
				if (!aConfig.ApplicationDataRoot.has_value()) {
					throw std::runtime_error{ "explicit replay state unavailable" };
				}
				auto replay = application::detail::designated_review_replay_initializer::load(
					designated_review_replay_namespace_path(
						aConfig.ApplicationDataRoot->lexically_normal()));
				if (!replay.has_value()) {
					throw std::runtime_error{ "replay state unavailable" };
				}
				auto observer
					= adapters::github::detail::make_designated_review_public_observer();
				if (observer == nullptr) {
					throw std::runtime_error{ "public observer unavailable" };
				}
				auto authority = application::detail::designated_review_authority_bootstrap::run(
					application::detail::designated_review_bootstrap_request{
						.ApprovalPath       = *aConfig.AttendedDesignatedLiveApproval,
						.ExecutablePath     = aConfig.RunningExecutable,
						.SourceSha256       = aConfig.AttendedSourceSha256,
						.SupervisorSha256   = aConfig.AttendedSupervisorSha256,
						.InventorySha256    = aConfig.AttendedInventorySha256,
						.BuildConfiguration = aConfig.BuildConfiguration,
						.Replay             = std::move(*replay),
						.BuildTesting       = aConfig.BuildTesting,
					},
					*observer);
				if (!authority.has_value() || *authority == nullptr) {
					throw std::runtime_error{ "attended live approval refused" };
				}
				LiveReviewMutationGrant = std::move(*authority);
			}
#endif
			if (aConfig.ExistingCheckout.has_value()) {
				std::error_code checkoutError;
				const auto      canonical = std::filesystem::canonical(
					*aConfig.ExistingCheckout,
					checkoutError);
				if (!checkoutError && canonical.is_absolute()
					&& std::filesystem::is_directory(canonical, checkoutError)
					&& !checkoutError) {
					ExistingCheckout = application::checkout_observation{
						.Path        = canonical.string(),
						.Cleanliness = application::checkout_cleanliness::unknown,
					};
				}
			}
#if defined(BPR_BUILD_TESTING)
			if (aFixture.has_value()) {
				DisableWorkbenchSelectionWire
					= aFixtureOptions.DisableWorkbenchSelectionWire;
				DisableReviewLedgerGestureWire
					= aFixtureOptions.DisableReviewLedgerGestureWire;
				UseGrantedNotificationSink
					= aFixtureOptions.UseGrantedNotificationSink;
				CaptureSystemNotificationDisplay
					= aFixtureOptions.CaptureSystemNotificationDisplay;
				DisableNotificationRequestWire
					= aFixtureOptions.DisableNotificationRequestWire;
				DisableNotificationSelectionWire
					= aFixtureOptions.DisableNotificationSelectionWire;
				DisableAgentHostComposition
					= aFixtureOptions.DisableAgentHostComposition;
				DisableAgentGestureWire
					= aFixtureOptions.DisableAgentGestureWire;
				ExternalAgentExecutable
					= aFixtureOptions.ExternalAgentExecutable;
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
			Window->comparison_surface()->set_selection_sink(
				[this](const std::optional<core::semantic_selection>& aSelection) {
					observe_review_selection(aSelection);
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
				*this,
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

			// git pack-objects creates its temporary pack inside the --git-dir's
			// own object store and renames it onto the requested output base.
			// A workspace on another filesystem makes that rename EXDEV, and
			// git reports "Invalid cross-device link" rather than anything this
			// side can interpret. So the workspace has to share a filesystem
			// with the retained cache.
			//
			// The system temporary directory does not, on a stock Linux host:
			// /tmp is tmpfs and an ordinary checkout is not. macOS never showed
			// it because the per-user temporary directory and a checkout are on
			// one APFS volume.
			//
			// A sibling of the application data root shares that filesystem and
			// stays disjoint from the cache, which the guard below still checks
			// rather than assumes.
			const auto workspaceNeighbourhood
				= std::filesystem::weakly_canonical(ApplicationDataRoot)
					  .parent_path();
			const auto workspaceTemplate = workspaceNeighbourhood.empty()
				? std::filesystem::path{ QDir::tempPath().toStdString() }
					/ "betterpullreview-agent-XXXXXX"
				: workspaceNeighbourhood / "betterpullreview-agent-XXXXXX";
			AgentWorkspaceRoot           = std::make_unique<QTemporaryDir>(
				QString::fromStdString(workspaceTemplate.string()));
			if (!AgentWorkspaceRoot->isValid()) {
				throw std::runtime_error{ "Agent workspace root is unavailable" };
			}
			// Exact installed-Git cleanup must be the only recursive authority.
			// A generic temporary-directory sweep could delete a same-path
			// substitute after exact cleanup correctly refused it.
			AgentWorkspaceRoot->setAutoRemove(false);
			const auto agentWorkspacePath = std::filesystem::canonical(
				AgentWorkspaceRoot->path().toStdString());
			AgentWorkspaceParent
				= adapters::git::capture_installed_agent_workspace_parent(
					agentWorkspacePath);
			if (!AgentWorkspaceParent.has_value()) {
				throw std::runtime_error{
					"Agent workspace root identity is unavailable"
				};
			}
			const auto cachePath = std::filesystem::canonical(ApplicationDataRoot);
			const auto within    = [](const std::filesystem::path& aPath,
									  const std::filesystem::path& aPrefix) {
				auto path   = aPath.begin();
				auto prefix = aPrefix.begin();
				for (; prefix != aPrefix.end(); ++prefix, ++path) {
					if (path == aPath.end() || *path != *prefix) {
						return false;
					}
				}
				return true;
			};
			if (within(agentWorkspacePath, cachePath)
				|| within(cachePath, agentWorkspacePath)) {
				throw std::runtime_error{ "Agent workspace root crosses Git cache" };
			}
			auto agentProfiles = core::built_in_agent_profiles();
#if defined(BPR_BUILD_TESTING)
			if (ExternalAgentExecutable.has_value()) {
				const auto external = std::ranges::find(
					agentProfiles,
					core::external_terminal_profile_id,
					&core::agent_profile::Id);
				if (external != agentProfiles.end()) {
					external->Executable = ExternalAgentExecutable->string();
				}
			}
#endif
			AgentProcess = std::make_unique<adapters::agent::agent_process_launcher>();
#if defined(BPR_BUILD_TESTING)
			if (!DisableAgentHostComposition)
#endif
			{
				AgentHost = std::make_unique<application::agent_invocation_host>(
					std::move(agentProfiles),
					std::vector<core::agent_host_policy>{
						core::built_in_agent_policy(
							core::git_host_id{ 1U },
							core::built_in_agent_host_kind::github_com),
						core::built_in_agent_policy(
							core::git_host_id{ 2U },
							core::built_in_agent_host_kind::explicit_ghes),
					},
					agentWorkspacePath,
					*this,
					*EpochHost,
					*AgentProcess,
					[](const std::string& aName) -> std::optional<std::string> {
						if (!qEnvironmentVariableIsSet(aName.c_str())) {
							return std::nullopt;
						}
						return qEnvironmentVariable(aName.c_str()).toStdString();
					},
					AgentEffects,
					DecisionLane,
					*Window->agent_panel());
			}
			Window->set_agent_dispatch(
				[this](qt::agent_intent aIntent) {
#if defined(BPR_BUILD_TESTING)
					if (DisableAgentGestureWire) {
						return;
					}
#endif
					dispatch_agent_intent(std::move(aIntent));
				});
			Window->epoch_selectors()->set_dispatch(
				[this](application::epoch_intent aIntent) { dispatch_epoch_intent(std::move(aIntent)); });
			DiffProjections = std::make_unique<composition_diff_projection>(
				*Window->comparison_surface(),
				[this](const application::diff_publication& aPublication) {
					// Before the ledger observation, because it is about the
					// panes rather than about the review, and because
					// `observe_selected_material` returns early for reasons
					// that have nothing to do with whether the comparison on
					// screen is coloured.
					adopt_published_classification(aPublication);
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

			// Queue projections arrive at this composition first. It forwards the
			// accepted value unchanged to the real window, then asks this notifier
			// only for newly warranting session/ref/bucket transitions.
#if defined(BPR_BUILD_TESTING)
			if (UseGrantedNotificationSink) {
				auto sink               = std::make_unique<fixture_notification_sink>();
				FixtureNotificationSink = sink.get();
				NotificationSink        = std::move(sink);
			}
#endif
			if (NotificationSink == nullptr) {
				auto sink = std::unique_ptr<qt::system_tray_notification_sink>{ };
#if defined(BPR_BUILD_TESTING)
				if (CaptureSystemNotificationDisplay) {
					sink = std::make_unique<qt::system_tray_notification_sink>(
						[this](QSystemTrayIcon&, const QString&, const QString&) {
							++CapturedNotificationDisplays;
						},
						Window.get());
				}
#endif
				if (sink == nullptr) {
					sink = std::make_unique<qt::system_tray_notification_sink>(
						Window.get());
				}
				SystemNotificationSink = sink.get();
				NotificationSink       = std::move(sink);
#if defined(BPR_BUILD_TESTING)
#endif
			}
			Notifications = std::make_unique<application::review_queue_notifier>(
				*NotificationSink,
				[this](application::queue_notification_target aTarget) {
					if (Closing || Window == nullptr) {
						return;
					}
#if defined(BPR_BUILD_TESTING)
					if (DisableNotificationSelectionWire) {
						return;
					}
#endif
					if (Window->select_queue_item(aTarget.Ref)) {
						++NotificationSelections;
					} else {
						++NotificationSelectionFailures;
					}
				});
			const auto activation = [this](
										application::queue_notification_target aTarget) {
				if (Notifications) {
					Notifications->activate(aTarget);
				}
			};
			if (SystemNotificationSink != nullptr) {
				SystemNotificationSink->set_activation_sink(activation);
			}
#if defined(BPR_BUILD_TESTING)
			if (FixtureNotificationSink != nullptr) {
				FixtureNotificationSink->set_activation_sink(activation);
			}
#endif

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
			static_cast<void>(Window->comparison_surface()->navigate_change(
				aIntent.Direction, aIntent.Policy));
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

		struct retryable_review_draft_record
		{
			core::review_comment_draft                    Draft;
			application::workbench_pull_request_authority Authority;
		};

		[[nodiscard]] static auto exact_retryable_review_draft(
			const retryable_review_draft_record&                 aRecord,
			const core::semantic_selection&                      aSelection,
			const application::workbench_pull_request_authority& aAuthority)
			-> bool
		{
			return aRecord.Draft.State == core::review_draft_state::drafting
				&& aRecord.Draft.Anchor == aSelection.PreferredLine
				&& aRecord.Draft.Anchor.Comparison == aSelection.Comparison
				&& aRecord.Authority == aAuthority;
		}

		void retire_review_mutation_host()
		{
			if (ReviewMutationHost != nullptr) {
				ReviewMutationHost->shutdown();
				RetiredReviewMutationHosts.push_back(
					std::move(ReviewMutationHost));
			}
			std::erase_if(
				RetiredReviewMutationHosts,
				[](const auto& aHost) { return aHost->is_quiescent(); });
			PendingReviewMutation.reset();
			DeferredReviewSelection.reset();
			RetryableReviewDraft.reset();
		}

		void compose_review_mutation(
			const application::accepted_pull_request_handoff& aHandoff)
		{
			retire_review_mutation_host();
			if (Window != nullptr && Window->review_editor() != nullptr) {
				Window->review_editor()->present(
					qt::review_draft_editor_clear_reason::composition_retired);
			}
			ReviewDraft.reset();
			ReviewDraftRevision = 0U;
			SelectedSemanticSelection.reset();
			refresh_agent_private_context_choices();
			ReviewProjection.clear();
#if defined(BPR_BUILD_TESTING)
			if (Fixture == nullptr || Window == nullptr
				|| Window->review_editor() == nullptr || WorkbenchHost == nullptr
				|| Host == nullptr || !Host->accepted_snapshot().has_value()) {
				return;
			}
			if (!RetiredReviewMutationHosts.empty()) {
				auto retry = std::make_shared<application::accepted_pull_request_handoff>(
					clone_without_smart_git(aHandoff));
				QTimer::singleShot(0, Window.get(), [this, retry] {
					if (!Closing) {
						compose_review_mutation(*retry);
					}
				});
				return;
			}
			auto selection = select_fixture_review_mutation(
				aHandoff, *Host->accepted_snapshot());
			if (!selection.has_value()) {
				return;
			}
			const auto target = selection->Target.Semantic;
			Fixture->prepare_review_mutation(target);
			if (ReviewMutationDecisionGate == nullptr) {
				ReviewMutationDecisionGate = std::make_unique<
					gated_review_mutation_decision_executor>(DecisionLane);
			}
			ReviewMutationHost = std::make_unique<
				application::review_mutation_application_host>(
				Fixture->review_mutation_provider(),
				selection->Target,
				selection->ActingIdentity,
				aHandoff.ApplicationSession,
				WorkbenchHost->generation(),
				application::make_review_mutation_testing_grant(
					aHandoff.ApplicationSession,
					WorkbenchHost->generation(),
					selection->Target,
					selection->ActingIdentity),
				ReviewMutationEffects,
				*ReviewMutationDecisionGate,
				*this);
			FixtureReviewMutationHostComposed = true;
#else
			if (Host == nullptr || WorkbenchHost == nullptr
				|| !Host->accepted_snapshot().has_value()) {
				return;
			}
			auto selection = select_designated_review_mutation(
				aHandoff, *Host->accepted_snapshot());
			if (!selection.has_value()) {
				LiveReviewMutationGrant.reset();
				return;
			}
			if (LiveReviewMutationGrant == nullptr) {
				return;
			}
			auto lease = Host->accepted_review_mutation_transport();
			if (lease == nullptr) {
				return;
			}
			const auto repository = selection->Target.Repository.Owner + "/"
				+ selection->Target.Repository.Name;
			auto recovery = adapters::sqlite::make_review_mutation_recovery_store({
				.StorageRoot = ApplicationDataRoot,
				.Host        = std::string{ selection->Target.Hostname.value() },
				.Account     = std::to_string(selection->ActingIdentity.Account.value()),
				.Repository  = repository,
				.PullRequest = static_cast<std::int64_t>(
					selection->Target.Number.value()),
			});
			if (!recovery.has_value()) {
				LiveReviewMutationGrant.reset();
				return;
			}
			auto provider = make_live_review_mutation_provider(
				std::move(lease),
				selection->Target,
				selection->ActingIdentity,
				std::move(*recovery));
			if (provider.has_value()) {
				auto erased = std::unique_ptr<application::review_mutation_port>{
					std::move(*provider)
				};
				ReviewMutationHost = std::make_unique<
					application::review_mutation_application_host>(
					std::move(erased),
					std::move(selection->Target),
					std::move(selection->ActingIdentity),
					aHandoff.ApplicationSession,
					WorkbenchHost->generation(),
					std::move(LiveReviewMutationGrant),
					ReviewMutationEffects,
					DecisionLane,
					*this);
			} else {
				LiveReviewMutationGrant.reset();
			}
#endif
			if (ReviewMutationHost == nullptr || Window == nullptr
				|| Window->review_editor() == nullptr) {
				return;
			}
			auto* const editor = Window->review_editor();
			editor->describe_target(QStringLiteral("%1/%2#%3 at %4 as account %5")
										.arg(QString::fromStdString(aHandoff.Locator.Owner))
										.arg(QString::fromStdString(aHandoff.Locator.Name))
										.arg(aHandoff.Number.value())
										.arg(QString::fromUtf8(
											core::canonical_hex(aHandoff.Head).view()))
										.arg(aHandoff.Account.value()));
			editor->present(ReviewMutationHost->capabilities());
		}

		void refresh_review_projection(
			const application::review_mutation_terminal& aTerminal)
		{
#if defined(BPR_BUILD_TESTING)
			static_cast<void>(aTerminal);
			if (Fixture == nullptr) {
				return;
			}
			const auto observed  = Fixture->review_comments();
			ReviewReconciliation = application::reconcile(
				ReviewProjection, observed, { });
			ReviewProjection = ReviewReconciliation.Projection;
#else
			if (!aTerminal.Reconciliation.has_value()) {
				return;
			}
			ReviewReconciliation = application::reconcile(
				ReviewProjection, *aTerminal.Reconciliation, { });
			ReviewProjection = ReviewReconciliation.Projection;
#endif
		}

		void dispatch_review_draft(core::review_draft_command aCommand)
		{
			if (Closing || !ReviewDraft.has_value()
				|| ReviewMutationHost == nullptr || Window == nullptr
				|| Window->review_editor() == nullptr) {
				return;
			}
			const auto outcome = core::apply(*ReviewDraft, aCommand);
			if (outcome.Rejection != core::review_draft_rejection::none) {
				return;
			}
			ReviewDraft = outcome.Draft;
			if (outcome.Changed) {
				++ReviewDraftRevision;
			}
			if (RetryableReviewDraft.has_value()
				&& RetryableReviewDraft->Draft.Id == ReviewDraft->Id) {
				if (ReviewDraft->State == core::review_draft_state::discarded) {
					RetryableReviewDraft.reset();
				} else {
					RetryableReviewDraft->Draft = *ReviewDraft;
				}
			}
			auto* const editor = Window->review_editor();
			editor->present(*ReviewDraft);
			refresh_agent_private_context_choices();
			if (aCommand.Kind != core::review_draft_command_kind::begin_publish) {
				return;
			}
			const auto proposal = core::propose_publication(
				*ReviewDraft, core::review_disposition::comment);
			if (!proposal.has_value()) {
				return;
			}
			if (!SelectedDiffPublication.has_value()
				|| !SelectedSemanticSelection.has_value()) {
				ReviewDraft = core::apply(
								  *ReviewDraft,
								  core::review_draft_command{
									  .Id   = ReviewDraft->Id,
									  .Kind = core::review_draft_command_kind::fail_publish,
								  })
								  .Draft;
				++ReviewDraftRevision;
				editor->present(*ReviewDraft);
				refresh_agent_private_context_choices();
				editor->present(
					application::review_mutation_failure::undesignated_target);
				return;
			}
			const auto context = ReviewMutationHost->publish(
				application::review_mutation_publication_request{
					.Proposal     = *proposal,
					.Selection    = *SelectedSemanticSelection,
					.SelectedDiff = *SelectedDiffPublication,
				});
			if (context.Request.value() == 0U) {
				ReviewDraft = core::apply(
								  *ReviewDraft,
								  core::review_draft_command{
									  .Id   = ReviewDraft->Id,
									  .Kind = core::review_draft_command_kind::fail_publish,
								  })
								  .Draft;
				++ReviewDraftRevision;
				editor->present(*ReviewDraft);
				editor->present(application::review_mutation_failure::unavailable);
				return;
			}
			PendingReviewMutation = context;
		}

		void adopt_review_selection(
			const std::optional<core::semantic_selection>& aSelection,
			const qt::review_draft_editor_clear_reason     aReason)
		{
			SelectedSemanticSelection = aSelection;
			ReviewDraft.reset();
			ReviewDraftRevision = 0U;
			if (Window != nullptr && Window->review_editor() != nullptr) {
				auto* const editor = Window->review_editor();
				editor->present(aReason);
				if (aSelection.has_value() && ReviewMutationHost != nullptr) {
					editor->present(ReviewMutationHost->capabilities());
				}
			}
			if (aSelection.has_value()) {
				present_review_draft(*aSelection);
			} else {
				refresh_agent_private_context_choices();
			}
		}

		void observe_review_selection(
			const std::optional<core::semantic_selection>& aSelection)
		{
			observe_change_navigation(aSelection);
			if (PendingReviewMutation.has_value()) {
				if (!DeferredReviewSelection.has_value()
					&& aSelection == SelectedSemanticSelection) {
					return;
				}
				const auto firstDivergence
					= !DeferredReviewSelection.has_value()
					&& aSelection != SelectedSemanticSelection;
				DeferredReviewSelection.emplace(aSelection);
				if (firstDivergence && ReviewMutationHost != nullptr) {
					ReviewMutationHost->cancel(*PendingReviewMutation);
				}
				return;
			}
			if (aSelection == SelectedSemanticSelection) {
				return;
			}
			adopt_review_selection(
				aSelection,
				qt::review_draft_editor_clear_reason::selection_cleared);
		}

		void observe_change_navigation(
			const std::optional<core::semantic_selection>& aSelection)
		{
			if (WorkbenchHost == nullptr || Window == nullptr
				|| !SelectedDiffPublication.has_value()) {
				return;
			}
			const auto* const comparison = Window->comparison_surface();
			const auto&       result     = comparison->current_result();
			const auto&       selected   = *SelectedDiffPublication;
			if (!result || result->Alignment.Key != selected.Key
				|| (aSelection.has_value()
					&& aSelection->Comparison != result->Alignment.Digest)) {
				WorkbenchHost->observe_change_navigation(
					selected.Request, std::nullopt);
				return;
			}

			const auto& groups = result->Alignment.Groups;
			if (groups.size() > std::numeric_limits<std::uint32_t>::max()) {
				WorkbenchHost->observe_change_navigation(
					selected.Request, std::nullopt);
				return;
			}
			auto cursor = core::workbench_change_cursor{
				.ChangeCount = static_cast<std::uint32_t>(groups.size()),
			};
			if (!aSelection.has_value() || !aSelection->Group.has_value()) {
				cursor.PreviousAvailable = !groups.empty();
				cursor.NextAvailable     = !groups.empty();
			} else {
				cursor.Group     = aSelection->Group;
				const auto found = std::ranges::find(
					groups, *aSelection->Group, &core::change_group::Id);
				if (found != groups.end()) {
					cursor.Ordinal = static_cast<std::uint32_t>(
						std::ranges::distance(groups.begin(), found) + 1);
					cursor.PreviousAvailable = cursor.Ordinal > 1U;
					cursor.NextAvailable     = cursor.Ordinal < cursor.ChangeCount;
				}
			}
			WorkbenchHost->observe_change_navigation(selected.Request, cursor);
		}

		void present_review_draft(const core::semantic_selection& aSelection)
		{
			if (ReviewMutationHost == nullptr || Window == nullptr
				|| Window->review_editor() == nullptr
				|| WorkbenchHost == nullptr || !SelectedDiffPublication.has_value()
				|| aSelection.Comparison
					!= core::comparison_digest(SelectedDiffPublication->Key)
				|| aSelection.PreferredLine.Comparison != aSelection.Comparison
				|| WorkbenchHost->generation()
					!= SelectedDiffPublication->Request.Context.Generation
				|| WorkbenchHost->selected()
					!= std::optional{ SelectedDiffPublication->Request.File.File }) {
				return;
			}
			if (ReviewDraft.has_value()
				&& ReviewDraft->Anchor == aSelection.PreferredLine
				&& (ReviewDraft->State == core::review_draft_state::drafting
					|| ReviewDraft->State == core::review_draft_state::publishing)) {
				return;
			}
			if (RetryableReviewDraft.has_value()
				&& exact_retryable_review_draft(
					*RetryableReviewDraft,
					aSelection,
					SelectedDiffPublication->Request.WorkbenchAuthority)) {
				ReviewDraft         = RetryableReviewDraft->Draft;
				ReviewDraftRevision = 1U;
				Window->review_editor()->present(*ReviewDraft);
				refresh_agent_private_context_choices();
				return;
			}
			ReviewDraft = core::review_comment_draft{
				.Id     = core::review_draft_id{ NextReviewDraft++ },
				.Anchor = aSelection.PreferredLine,
			};
			ReviewDraftRevision = 1U;
			Window->review_editor()->present(*ReviewDraft);
			refresh_agent_private_context_choices();
		}

		void publish(
			const application::review_mutation_terminal& aTerminal) override
		{
			if (Closing
				|| PendingReviewMutation != std::optional{ aTerminal.Context }) {
				return;
			}
			PendingReviewMutation.reset();
			refresh_review_projection(aTerminal);
			const auto deferred = std::move(DeferredReviewSelection);
			DeferredReviewSelection.reset();
			if (!ReviewDraft.has_value()
				|| ReviewDraft->State != core::review_draft_state::publishing) {
				if (deferred.has_value()) {
					adopt_review_selection(
						*deferred,
						qt::review_draft_editor_clear_reason::selection_cleared);
				}
				return;
			}
			ReviewDraft = core::apply(
							  *ReviewDraft,
							  core::review_draft_command{
								  .Id   = ReviewDraft->Id,
								  .Kind = aTerminal.Result.has_value()
									  ? core::review_draft_command_kind::complete_publish
									  : core::review_draft_command_kind::fail_publish,
							  })
							  .Draft;
			++ReviewDraftRevision;
			const auto sameRetry = RetryableReviewDraft.has_value()
				&& RetryableReviewDraft->Draft.Id == ReviewDraft->Id;
			if (aTerminal.Result.has_value()) {
				if (sameRetry) {
					RetryableReviewDraft.reset();
				}
			} else if (
				aTerminal.Result.error()
					== application::review_mutation_failure::unavailable
				&& SelectedDiffPublication.has_value()
				&& ReviewDraft->Anchor.Comparison
					== core::comparison_digest(SelectedDiffPublication->Key)) {
				RetryableReviewDraft = retryable_review_draft_record{
					.Draft     = *ReviewDraft,
					.Authority = SelectedDiffPublication->Request.WorkbenchAuthority,
				};
			} else if (sameRetry) {
				RetryableReviewDraft.reset();
			}
			refresh_agent_private_context_choices();
			if (deferred.has_value()) {
				adopt_review_selection(
					*deferred,
					qt::review_draft_editor_clear_reason::selection_cleared);
				return;
			}
			if (Window == nullptr || Window->review_editor() == nullptr) {
				return;
			}
			auto* const editor = Window->review_editor();
			editor->present(*ReviewDraft);
			if (aTerminal.Result.has_value()) {
				editor->present(*aTerminal.Result, ReviewReconciliation);
			} else {
				editor->present(aTerminal.Result.error());
			}
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
				Window->agent_panel()->offer_private_context({ }, std::nullopt);
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
			if (SelectedSemanticSelection.has_value()) {
				present_review_draft(*SelectedSemanticSelection);
			}
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

		void publish(
			const application::queue_landing_projection& aProjection) override
		{
			if (Closing || Window == nullptr) {
				return;
			}

			// Presentation receives the exact accepted projection before any
			// notification can activate back into it.
			Window->publish(aProjection);

			auto active = std::vector<queue_notification_transition>{ };
			active.reserve(aProjection.Items.size());
			for (const auto& item : aProjection.Items) {
				const auto trustedActivity = QueueActivity.has_value()
					&& QueueActivity->Session == aProjection.Session
					&& QueueActivity->Ref == item.Ref
					&& item.Bucket
						== core::bucket_for(
							item.Membership, QueueActivity->Activity);
				if (!trustedActivity
					|| !application::warrants_notification(item.Bucket)) {
					continue;
				}
				const auto transition = queue_notification_transition{
					.Session = aProjection.Session,
					.Ref     = item.Ref,
					.Bucket  = item.Bucket,
				};
				active.push_back(transition);
				if (std::ranges::binary_search(
						ActiveQueueNotifications, transition)) {
					continue;
				}
#if defined(BPR_BUILD_TESTING)
				if (DisableNotificationRequestWire) {
					continue;
				}
#endif
				const auto unseen  = QueueActivity->Activity.UnseenComments;
				const auto request = application::notification_for(item, unseen);
				if (!request.has_value()) {
					continue;
				}
				++NotificationRequestsAttempted;
				if (Notifications != nullptr) {
					static_cast<void>(Notifications->deliver(*request));
				}
			}
			std::ranges::sort(active);
			const auto unique = std::ranges::unique(active);
			active.erase(unique.begin(), unique.end());
			ActiveQueueNotifications = std::move(active);
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
				AgentReview = agent_review_facts{
					.Host        = aHandoff.Host,
					.Repository  = aHandoff.Repository,
					.PullRequest = aHandoff.PullRequest,
					.Number      = aHandoff.Number,
					.Hostname    = std::string{ aHandoff.Hostname.value() },
					.Locator     = aHandoff.Locator,
				};
				const auto authority = workbench_authority(aHandoff);
				const auto snapshot  = Host != nullptr
					? Host->accepted_snapshot()
					: std::optional<application::github_pull_request_snapshot>{ };
				compose_review_ledger(aHandoff);
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
					auto       activity     = core::queue_activity_observation{ };
					const auto sameBaseline = QueueActivityBaseline.has_value()
						&& QueueActivityBaseline->Session == session
						&& QueueActivityBaseline->Ref == observation.Ref;
					if (sameBaseline) {
						activity.ReviewedByMe
							= aHandoff.CurrentViewerReview.has_value();
						activity.HeadMovedSinceMyReview
							= activity.ReviewedByMe
							&& QueueActivityBaseline->Head != aHandoff.Head
							&& aHandoff.CurrentViewerReview->Commit != aHandoff.Head;
					} else {
						QueueActivityBaseline = queue_activity_baseline{
							.Session = session,
							.Ref     = observation.Ref,
							.Head    = aHandoff.Head,
						};
						ActiveQueueNotifications.clear();
					}
					QueueActivity = queue_activity_fact{
						.Session  = session,
						.Ref      = observation.Ref,
						.Activity = activity,
					};
					QueueHost->adopt(session);
					QueueRefresh->retain(
						std::move(session),
						observation);
					QueueHost->refresh(
						observation.Ref,
						activity);
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
				compose_review_mutation(aHandoff);
				EpochHost->accept(std::move(aHandoff));
			}
		}

		void invalidate(
			application::pull_request_authority_invalidation aInvalidation) override
		{
			if (Closing || EpochHost == nullptr) {
				return;
			}
			AgentReview.reset();
			if (AgentHost != nullptr) {
				AgentHost->clear();
			}
			if (QueueHost != nullptr) {
				QueueHost->cancel();
			}
			if (QueueRefresh != nullptr) {
				QueueRefresh->clear();
			}
			QueueActivity.reset();
			QueueActivityBaseline.reset();
			ActiveQueueNotifications.clear();
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
				retire_review_mutation_host();
				ReviewDraft.reset();
				ReviewDraftRevision = 0U;
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

		[[nodiscard]] auto admit(
			const application::agent_session_admission& aAdmission)
			-> std::expected<core::agent_session, application::ledger_store_failure> override
		{
			if (Closing || ReviewLedgerHost == nullptr) {
				return std::unexpected(
					application::ledger_store_failure::unavailable);
			}
			return ReviewLedgerHost->admit(aAdmission);
		}

		[[nodiscard]] auto session_catalog()
			-> std::expected<
				application::agent_session_catalog_state,
				application::ledger_store_failure> override
		{
			if (Closing || ReviewLedgerHost == nullptr) {
				return std::unexpected(
					application::ledger_store_failure::unavailable);
			}
			return ReviewLedgerHost->session_catalog();
		}

		[[nodiscard]] auto current_agent_draft_observation() const
			-> std::optional<application::agent_draft_observation>
		{
			if (!ReviewDraft.has_value() || ReviewDraftRevision == 0U) {
				return std::nullopt;
			}
			return application::agent_draft_observation{
				.Choice = {
					.Id = ReviewDraft->Id, .Revision = ReviewDraftRevision },
				.Draft = *ReviewDraft,
			};
		}

		void refresh_agent_private_context_choices()
		{
			if (Window == nullptr || Window->agent_panel() == nullptr) {
				return;
			}
			auto notes = std::vector<application::local_note_id>{ };
			if (ReviewLedgerHost != nullptr) {
				if (auto observed = ReviewLedgerHost->private_observation();
					observed.has_value()) {
					notes.reserve(observed->Notes.size());
					for (const auto& note : observed->Notes) {
						notes.push_back(note.Id);
					}
				}
			}
			auto draft = current_agent_draft_observation();
			if (draft.has_value()
				&& draft->Draft.State != core::review_draft_state::drafting) {
				draft.reset();
			}
			Window->agent_panel()->offer_private_context(
				notes,
				draft.has_value()
					? std::optional{ draft->Choice }
					: std::nullopt);
		}

		void refresh_agent_authority()
		{
			if (AgentHost == nullptr || EpochHost == nullptr || Window == nullptr) {
				return;
			}
			const auto& state = EpochHost->state();
			if (!state.Authority.has_value() || !state.AcceptedRevision.has_value()
				|| !state.AcceptedComparison.has_value() || !AgentReview.has_value()) {
				AgentHost->clear();
				return;
			}
			const auto& authority  = *state.Authority;
			const auto& revision   = *state.AcceptedRevision;
			const auto& comparison = *state.AcceptedComparison;
			if (authority.Host != AgentReview->Host
				|| authority.Repository != AgentReview->Repository
				|| authority.PullRequest != AgentReview->PullRequest) {
				AgentHost->clear();
				return;
			}
			AgentHost->observe(
				state.ApplicationSession,
				authority,
				core::agent_revision_binding{
					.Host        = authority.Host,
					.Repository  = authority.Repository,
					.PullRequest = authority.PullRequest,
					.BaseOid     = revision.Base,
					.HeadOid     = revision.Head,
					.LeftEpoch   = comparison.Left.Identity.Epoch,
					.RightEpoch  = comparison.Right.Identity.Epoch,
				});
			refresh_agent_private_context_choices();
			Window->agent_panel()->disclose_scope(
				QStringLiteral(
					"The selected agent receives the exact accepted PR/diff identities, "
					"durable defects, current semantic selection, and only private notes or "
					"the current draft checked for this invocation. It runs in a disposable exact-head workspace "
					"outside the Git cache.%1")
					.arg(ExistingCheckout.has_value()
							 ? QStringLiteral(
								   " Choose the working-copy option only for this invocation; its state is disclosed as unknown.")
							 : QString{ }));
		}

		void dispatch_agent_intent(qt::agent_intent aIntent)
		{
			if (Closing || AgentHost == nullptr || EpochHost == nullptr
				|| Window == nullptr) {
				return;
			}
			if (std::holds_alternative<qt::cancel_agent_intent>(aIntent)) {
				AgentHost->cancel();
				return;
			}
			const auto  start = std::get<qt::start_agent_intent>(aIntent);
			const auto& state = EpochHost->state();
			if (!state.Authority.has_value() || !state.AcceptedRevision.has_value()
				|| !state.AcceptedComparison.has_value() || !AgentReview.has_value()
				|| ReviewLedgerHost == nullptr) {
				Window->agent_panel()->refuse("no exact accepted review context");
				return;
			}
			const auto& authority          = *state.Authority;
			const auto& revision           = *state.AcceptedRevision;
			const auto& comparison         = *state.AcceptedComparison;
			auto        privateObservation = application::private_ledger_observation{ };
			if (ReviewLedgerHost != nullptr) {
				auto observed = ReviewLedgerHost->private_observation();
				if (!observed.has_value()) {
					Window->agent_panel()->refuse("private review context is unavailable");
					return;
				}
				privateObservation = std::move(*observed);
			}
			const auto binding = core::agent_revision_binding{
				.Host        = authority.Host,
				.Repository  = authority.Repository,
				.PullRequest = authority.PullRequest,
				.BaseOid     = revision.Base,
				.HeadOid     = revision.Head,
				.LeftEpoch   = comparison.Left.Identity.Epoch,
				.RightEpoch  = comparison.Right.Identity.Epoch,
			};
			auto observation = application::agent_context_observation{
				.ApplicationSession = state.ApplicationSession,
				.Binding            = binding,
				.PullRequestUrl     = "https://" + AgentReview->Hostname + "/"
					+ AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name
					+ "/pull/" + std::to_string(AgentReview->Number.value()),
				.Private       = std::move(privateObservation),
				.IncludedNotes = start.IncludedNotes,
				.IncludedDraft = start.IncludedDraft,
			};
			observation.CurrentDraft = current_agent_draft_observation();
			if (SelectedDiffPublication.has_value()) {
				const auto& selected = *SelectedDiffPublication;
				const auto  lock     = std::scoped_lock{ ReviewLedgerStateMutex };
				if (ReviewLedgerReceipt.has_value()
					&& ReviewLedgerReceipt->File == selected.Request.File.File) {
					observation.Files.push_back(application::agent_file_context_input{
						.Path               = selected.Request.File.PresentationPath,
						.Receipt            = *ReviewLedgerReceipt,
						.ChangedSinceReview = observation.Private.LastReviewedHead.has_value()
							&& *observation.Private.LastReviewedHead != revision.Head,
					});
				}
				const auto* const workbench = Window->comparison_surface();
				if (workbench->selection().has_value()
					&& workbench->current_result() != nullptr
					&& workbench->current_result()->Alignment.Key == selected.Key
					&& workbench->selection()->Comparison
						== workbench->current_result()->Alignment.Digest) {
					observation.Selection = core::supplement_semantic_context{
						.File       = selected.Request.File.File,
						.Selection  = *workbench->selection(),
						.Confidence = core::supplement_mapping_confidence::exact,
						.Segments   = workbench->current_result()->Alignment.Segments,
						.Groups     = workbench->current_result()->Alignment.Groups,
					};
				}
			}
			AgentHost->dispatch(application::agent_invocation_request{
				.ApplicationSession     = state.ApplicationSession,
				.Authority              = authority,
				.Profile                = start.Profile,
				.Context                = std::move(observation),
				.ExistingCheckoutChosen = start.ExistingCheckoutChosen,
				.ExistingCheckout       = ExistingCheckout,
			});
		}

		void publish(
			const application::epoch_presentation_update& aUpdate) override
		{
			if (Closing || Window == nullptr) {
				return;
			}
			Window->publish(aUpdate);
			refresh_agent_authority();
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
			RetryableReviewDraft.reset();
			if (ReviewMutationHost != nullptr
				&& PendingReviewMutation.has_value()) {
				DeferredReviewSelection.emplace(std::nullopt);
				ReviewMutationHost->cancel(*PendingReviewMutation);
			} else {
				adopt_review_selection(
					std::nullopt,
					qt::review_draft_editor_clear_reason::selection_invalidated);
			}
			SelectedDiffPublication.reset();
			if (DiffHost != nullptr) {
				DiffHost->cancel();
			}
			if (SyntaxBlobs != nullptr) {
				// A classification still in flight now has no comparison to
				// read for. Dropping the bindings makes it report unavailable
				// rather than materialize bytes for a file the reviewer has
				// left.
				SyntaxBlobs->clear_comparison();
			}
			// The definitions go with them: nothing on screen can be adopted for
			// a selection that has been cleared, and keeping the choices would
			// only let a later publication colour a pane the reviewer has left.
			ClassifiedDefinitions.clear();
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
			if (AppearanceHost == nullptr || SyntaxBlobs == nullptr) {
				return;
			}
			SyntaxBlobs->register_comparison(aContext);
			const auto definition = SyntaxDefinitions->definition_for_path(aPath);
			for (const auto side :
				 { core::diff_side::left, core::diff_side::right }) {
				const auto& blob = side == core::diff_side::left
					? aContext.Key.LeftBlob
					: aContext.Key.RightBlob;
				if (!blob.has_value()) {
					continue;
				}
				// Retained so the display path can rebuild this exact cache key
				// without the file's name. The name is only needed to choose a
				// definition, and that choice has already been made here.
				retain_definition(*blob, definition);
				AppearanceHost->classify(application::token_span_cache_key{
					.Blob       = *blob,
					.Definition = definition,
				});
			}
		}

		/// \brief Remembers which definition a blob was classified against.
		///
		/// \details Bounded and insert-if-absent for the same reason as the blob
		/// bindings: a blob is content-addressed, so the definition chosen for it
		/// once is the definition for it. BPR-0390.
		void retain_definition(
			const core::git_blob_oid&                     aBlob,
			const application::syntax_definition_version& aDefinition)
		{
			const auto known = std::ranges::find_if(
				ClassifiedDefinitions,
				[&aBlob](const auto& aEntry) { return aEntry.first == aBlob; });
			if (known != ClassifiedDefinitions.end()) {
				return;
			}
			if (ClassifiedDefinitions.size() >= MaximumClassifiedDefinitions) {
				ClassifiedDefinitions.erase(ClassifiedDefinitions.begin());
			}
			ClassifiedDefinitions.emplace_back(aBlob, aDefinition);
		}

		/// \brief Colours the comparison the panes are actually showing.
		///
		/// \details Adoption used to depend entirely on delivery order. A
		/// classification and a diff result are two unordered publications into
		/// one pane, `set_loading` clears spans on every new request, and
		/// `adopt_classification` drops anything whose blob is no longer the
		/// current comparison's — so a classification that arrived a moment too
		/// early was wiped and one that arrived a moment too late was refused.
		/// Measured on the recorded course, four of seven publications were
		/// dropped and both panes ended unclassified while every composition
		/// marker stayed green.
		///
		/// This asks the opposite question at the one moment it can be answered:
		/// the comparison is on screen now, so if its spans are already known,
		/// adopt them now. Guarded by what the surface itself reports rather than
		/// by the retained selection, because `composition_diff_projection`
		/// forwards every publication to this sink while `diff_workbench::publish`
		/// accepts only the current one. BPR-0390.
		void adopt_published_classification(
			const application::diff_publication& aPublication)
		{
			if (
				Closing || Window == nullptr || AppearanceHost == nullptr
				|| aPublication.Result == nullptr) {
				return;
			}
			auto* const surface = Window->comparison_surface();
			const auto& shown   = surface->current_result();
			if (!shown || shown->Alignment.Key != aPublication.Context.Key) {
				return;
			}
			for (const auto side :
				 { core::diff_side::left, core::diff_side::right }) {
				const auto& blob = side == core::diff_side::left
					? aPublication.Context.Key.LeftBlob
					: aPublication.Context.Key.RightBlob;
				if (!blob.has_value()) {
					continue;
				}
				const auto known = std::ranges::find_if(
					ClassifiedDefinitions,
					[&blob](const auto& aEntry) {
						return aEntry.first == *blob;
					});
				if (known == ClassifiedDefinitions.end()) {
					continue;
				}
				// A synchronous read of what the host already holds. The
				// submission in `classify_comparison` is deliberately kept as
				// well: `cached` does not refresh the retention order, so a
				// repeatedly displayed blob would become evictable if this
				// replaced it rather than joining it.
				auto spans = AppearanceHost->cached(
					application::token_span_cache_key{
						.Blob       = *blob,
						.Definition = known->second,
					});
				if (!spans) {
					continue;
				}
				if (surface->adopt_token_spans(side, std::move(spans))) {
					++DisplayAdoptedClassifications;
				}
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
			// A terminal has arrived for this blob, so whatever read it needed is
			// over and its binding can go. Released before the guards below,
			// because the binding's lifetime is the classification's rather than
			// this adoption's, and a dropped adoption still ends the read.
			// BPR-0390.
			if (SyntaxBlobs != nullptr) {
				SyntaxBlobs->release_blob(aSpans.Key.Blob);
			}
			if (Closing || Window == nullptr || DiffHost == nullptr) {
				return;
			}
			const auto& state   = DiffHost->state();
			const auto* context = state.Current.has_value()
				? &*state.Current
				: state.Pending.has_value() ? &*state.Pending
											: nullptr;
			if (context == nullptr) {
				++DroppedClassifications;
				return;
			}
			const auto side = context->Key.LeftBlob == std::optional{ aSpans.Key.Blob }
				? std::optional{ core::diff_side::left }
				: context->Key.RightBlob == std::optional{ aSpans.Key.Blob }
				? std::optional{ core::diff_side::right }
				: std::nullopt;
			if (!side.has_value() || !aSpans.Spans) {
				++DroppedClassifications;
				return;
			}
			if (Window->comparison_surface()->adopt_token_spans(
					*side, aSpans.Spans)) {
				++AdoptedClassifications;
			} else {
				++DroppedClassifications;
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
			RetryableReviewDraft.reset();
			if (AgentHost) {
				AgentHost->shutdown();
			}
			if (!AgentHost || AgentHost->is_quiescent()) {
				begin_other_shutdown();
			}
			schedule_shutdown_poll();
		}

		void begin_other_shutdown()
		{
			if (OtherShutdownBegun) {
				return;
			}
			OtherShutdownBegun = true;
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
			if (ReviewMutationHost) {
				ReviewMutationHost->shutdown();
			}
			for (auto& host : RetiredReviewMutationHosts) {
				host->shutdown();
			}
			DiffHost->shutdown();
			EpochHost->shutdown();
			Host->shutdown();
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
			if (AgentHost && !AgentHost->is_quiescent()) {
				AgentHost->retry_cleanup();
				schedule_shutdown_poll();
				return;
			}
			if (!AgentWorkspaceCleanupAttempted) {
				AgentWorkspaceCleanupAttempted = true;
				AgentWorkspaceCleanupFailed    = !AgentWorkspaceParent.has_value()
					|| !adapters::git::remove_installed_agent_workspace_parent(
						*AgentWorkspaceParent);
			}
			begin_other_shutdown();
			if (
				!DiffHost->is_quiescent() || !EpochHost->is_quiescent()
				|| !Host->is_quiescent()
				|| (WorkbenchHost && !WorkbenchHost->is_quiescent())
				|| (QueueHost && !QueueHost->is_quiescent())
				|| (AppearanceHost && !AppearanceHost->is_quiescent())
				|| (ReviewMutationHost && !ReviewMutationHost->is_quiescent())
				|| std::ranges::any_of(
					RetiredReviewMutationHosts,
					[](const auto& aHost) { return !aHost->is_quiescent(); })) {
				schedule_shutdown_poll();
				return;
			}

			ShutdownCleanupFailed         = Host->shutdown_cleanup_failed();
			EpochProjectionDeliveryFailed = EpochHost->projection_delivery_failed();
			ReviewMutationHost.reset();
			RetiredReviewMutationHosts.clear();
			AppearanceEffects.close_and_drain();
			AgentEffects.close_and_drain();
			QueueEffects.close_and_drain();
			WorkbenchEffects.close_and_drain();
			DiffEffects.close_and_drain();
			ReviewMutationEffects.close_and_drain();
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
		worker_executor                                 ReviewMutationEffects;
#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<detail::github_public_path_fixture_components> Fixture;
		std::unique_ptr<gated_review_mutation_decision_executor>
																  ReviewMutationDecisionGate;
		std::optional<application::accepted_pull_request_handoff> LastAcceptedHandoff;
		std::size_t                                               EpochPruneIntentDispatchCount  = 0U;
		std::size_t                                               EpochDeleteIntentDispatchCount = 0U;
		std::size_t                                               EpochCancelIntentDispatchCount = 0U;
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
		struct uiforetw_selected_diff_probe_state
		{
			enum class phase {
				awaiting_observation,
				awaiting_resource_diff,
				awaiting_support_diff,
				awaiting_resource_return,
				awaiting_rc_diff,
				awaiting_focused_layout,
				passed,
				failed,
			};

			phase                                         Phase = phase::awaiting_observation;
			detail::uiforetw_selected_diff_probe_evidence Evidence;
			qt::workbench_window_statistics               WindowStatistics;
			std::size_t                                   SelectedDiffPublications = 0U;
			std::size_t                                   SelectedDiffFailures     = 0U;
			std::size_t                                   IntentDispatches         = 0U;
			std::size_t                                   ChangeDispatches         = 0U;
			std::size_t                                   Waits                    = 0U;
		};
		std::optional<uiforetw_selected_diff_probe_state>
			UIforEtwSelectedDiffProbe;
		struct native_appearance_selected_diff_probe_state
		{
			enum class phase {
				awaiting_observation,
				awaiting_selection_projection,
				awaiting_diff,
				ready,
				refused,
			};

			phase                                                  Phase = phase::awaiting_observation;
			detail::native_appearance_selected_diff_probe_evidence Evidence;
			application::workbench_pull_request_authority          Authority;
			core::workbench_file_id                                File;
			qt::workbench_window_statistics                        WindowStatistics;
			std::size_t                                            IntentDispatches         = 0U;
			std::size_t                                            SelectedDiffBegins       = 0U;
			std::size_t                                            SelectedDiffPublications = 0U;
			std::size_t                                            SelectedDiffFailures     = 0U;
		};
		std::optional<native_appearance_selected_diff_probe_state>
			NativeAppearanceSelectedDiffProbe;
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
			enum class phase {
				awaiting_editor,
				awaiting_stale_refusal,
				awaiting_lost_effect,
				awaiting_deferred_terminal,
				awaiting_retry,
				passed,
				failed,
			};

			detail::review_round_trip_composition_probe_evidence Evidence;
			phase                                                Phase = phase::awaiting_editor;
			std::optional<core::semantic_selection>              PublishingSelection;
			std::optional<core::semantic_selection>              DeferredSelection;
			std::optional<core::review_comment_draft>            PublishingDraft;
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
		std::size_t                          FixtureDiffHelperInvocations   = 0U;
		bool                                 DisableWorkbenchSelectionWire  = false;
		bool                                 DisableReviewLedgerGestureWire = false;
		bool                                 DisableAgentHostComposition    = false;
		bool                                 DisableAgentGestureWire        = false;
		std::optional<std::filesystem::path> ExternalAgentExecutable;
		bool                                 ReviewLedgerWasPresentOnEntry     = false;
		bool                                 ReviewLedgerRoundTripPassed       = false;
		bool                                 FixtureReviewMutationHostComposed = false;
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
		std::unique_ptr<application::review_mutation_application_host>
													 ReviewMutationHost;
		application::review_mutation_execution_grant LiveReviewMutationGrant;
		std::vector<std::unique_ptr<application::review_mutation_application_host>>
			RetiredReviewMutationHosts;
		std::optional<application::review_mutation_request_context>
			PendingReviewMutation;
		std::optional<std::optional<core::semantic_selection>>
																	 DeferredReviewSelection;
		std::optional<core::semantic_selection>                      SelectedSemanticSelection;
		std::optional<core::review_comment_draft>                    ReviewDraft;
		std::optional<retryable_review_draft_record>                 RetryableReviewDraft;
		std::uint64_t                                                ReviewDraftRevision = 0U;
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
		std::unique_ptr<adapters::syntax::packaged_definition_catalog> SyntaxDefinitions;
		std::unique_ptr<composition_blob_source>                       SyntaxBlobs;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>  SyntaxClassification;
		composition_appearance_projections                             AppearanceProjections;
		std::unique_ptr<qt::appearance_profile>                        AppearanceProfile;
		std::string                                                    AppliedSelectionColor;
		std::string                                                    AppliedBaseColor;
		bool                                                           AppliedFromProfile             = false;
		bool                                                           WorkbenchPersistenceAvailable  = false;
		bool                                                           AppearancePersistenceAvailable = false;
		std::unique_ptr<application::epoch_entropy_port>               EpochEntropy;
		std::unique_ptr<application::epoch_clock_port>                 EpochClock;
		worker_executor                                                EpochEffects;
		worker_executor                                                DiffEffects;
		worker_executor                                                AgentEffects;
		worker_executor                                                WorkbenchEffects;
		worker_executor                                                AppearanceEffects;
		worker_executor                                                QueueEffects;
		std::unique_ptr<application::epoch_application_host>           EpochHost;
		std::unique_ptr<QTemporaryDir>                                 AgentWorkspaceRoot;
		std::optional<adapters::git::installed_agent_workspace_parent>
																		AgentWorkspaceParent;
		std::unique_ptr<adapters::agent::agent_process_launcher>        AgentProcess;
		std::unique_ptr<application::agent_invocation_host>             AgentHost;
		std::unique_ptr<composition_diff_projection>                    DiffProjections;
		std::unique_ptr<application::diff_application_host>             DiffHost;
		std::unique_ptr<application::workbench_application_host>        WorkbenchHost;
		std::unique_ptr<application::appearance_application_host>       AppearanceHost;
		std::unique_ptr<application::queue_enrollment_store_port>       QueueStore;
		std::unique_ptr<composition_queue_refresh>                      QueueRefresh;
		std::unique_ptr<application::review_queue_application_host>     QueueHost;
		std::optional<queue_activity_baseline>                          QueueActivityBaseline;
		std::optional<queue_activity_fact>                              QueueActivity;
		std::vector<queue_notification_transition>                      ActiveQueueNotifications;
		std::unique_ptr<application::github_application_host>           Host;
		std::optional<application::workbench_selected_diff_publication> SelectedDiffPublication;
		struct agent_review_facts
		{
			core::git_host_id         Host;
			core::repository_id       Repository;
			core::pull_request_id     PullRequest;
			core::pull_request_number Number;
			std::string               Hostname;
			core::repository_locator  Locator;
		};
		std::optional<agent_review_facts>                AgentReview;
		std::optional<application::checkout_observation> ExistingCheckout;
		bool                                             Closing                        = false;
		bool                                             OtherShutdownBegun             = false;
		bool                                             ShutdownPollScheduled          = false;
		bool                                             ShutdownComplete               = false;
		bool                                             ShutdownCleanupFailed          = false;
		bool                                             AgentWorkspaceCleanupAttempted = false;
		bool                                             AgentWorkspaceCleanupFailed    = false;
		bool                                             NativePlatformVaultSelected    = false;
		bool                                             EpochProjectionDeliveryFailed  = false;
		// Classifications the panes actually took. Counted because "the
		// classifier is linked" and "a pane received its answer" are different
		// claims, and only the second one shows a reviewer anything.
		std::size_t AdoptedClassifications = 0U;
		std::size_t DroppedClassifications = 0U;
		// Adoptions taken on the display turn from what the host already held,
		// counted apart from the publication path so neither number absorbs the
		// other. BPR-0390.
		std::size_t DisplayAdoptedClassifications = 0U;
		// Two blobs per comparison, so eight comparisons' worth of remembered
		// definition choices. The oldest is dropped past that; a blob whose
		// choice has been forgotten is simply not adopted on display, and its
		// classification still publishes through the ordinary path.
		static constexpr auto MaximumClassifiedDefinitions = std::size_t{ 16U };
		std::vector<
			std::pair<core::git_blob_oid, application::syntax_definition_version>>
			ClassifiedDefinitions;
		// Times the presentation profile refused typography the host accepted.
		// Both validate with the same core function, so this must stay zero;
		// it exists so that if it ever does not, something says so.
		std::size_t                                          AppearanceDivergences = 0U;
		std::unique_ptr<application::notification_sink_port> NotificationSink;
		qt::system_tray_notification_sink*                   SystemNotificationSink = nullptr;
		std::unique_ptr<application::review_queue_notifier>  Notifications;
		std::size_t                                          NotificationRequestsAttempted = 0U;
		std::size_t                                          NotificationSelections        = 0U;
		std::size_t                                          NotificationSelectionFailures = 0U;
#if defined(BPR_BUILD_TESTING)
		fixture_notification_sink* FixtureNotificationSink          = nullptr;
		bool                       UseGrantedNotificationSink       = false;
		bool                       CaptureSystemNotificationDisplay = false;
		bool                       DisableNotificationRequestWire   = false;
		bool                       DisableNotificationSelectionWire = false;
		std::size_t                CapturedNotificationDisplays     = 0U;
#endif
	};

	auto initialize_or_reset_designated_live_replay_state(
		const std::filesystem::path& aApplicationDataRoot) noexcept -> bool
	{
		try {
			if (aApplicationDataRoot.empty() || !aApplicationDataRoot.is_absolute()
				|| aApplicationDataRoot.lexically_normal() != aApplicationDataRoot) {
				return false;
			}
			return application::detail::designated_review_replay_initializer::initialize_or_reset(
					   designated_review_replay_namespace_path(aApplicationDataRoot))
				.has_value();
		} catch (...) {
			return false;
		}
	}

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
		// How many spans the pane on screen carries right now. Read here rather
		// than counted as it is adopted, because the question is about the
		// present state of the surface and a count kept alongside would only
		// agree with it by luck. BPR-0390.
		const auto displayed_span_count
			= [this](const core::diff_side aSide) noexcept -> std::size_t {
			if (Implementation->Window == nullptr) {
				return 0U;
			}
			const auto& spans
				= Implementation->Window->comparison_surface()->token_spans(aSide);
			return spans ? spans->size() : 0U;
		};
		return {
			.Closing                        = Implementation->Closing,
			.ShutdownComplete               = Implementation->ShutdownComplete,
			.ShutdownCleanupFailed          = Implementation->ShutdownCleanupFailed,
			.AgentWorkspaceCleanupFailed    = Implementation->AgentWorkspaceCleanupFailed,
			.NativePlatformVaultSelected    = Implementation->NativePlatformVaultSelected,
			.EpochProjectionDeliveryFailed  = Implementation->EpochProjectionDeliveryFailed,
			.EpochHandoffDeliveryFailed     = Implementation->Host->epoch_handoff_delivery_failed(),
			.WorkbenchHostComposed          = Implementation->WorkbenchHost != nullptr,
			.AppearanceHostComposed         = Implementation->AppearanceHost != nullptr,
			.AgentHostComposed              = Implementation->AgentHost != nullptr,
			.AppearancePersistenceAvailable = Implementation->AppearancePersistenceAvailable,
			.AppearanceDurableWrites        = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->persisted_settings()
				: 0U,
			.AppliedSelectionColor          = Implementation->AppliedSelectionColor,
			.AppliedBaseColor               = Implementation->AppliedBaseColor,
			.AppliedFromProfile             = Implementation->AppliedFromProfile,
			.ObservedScheme                 = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->observation().Scheme
				: core::os_color_scheme::unknown,
			.ResolvedVariant                = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.CustomisedRoles                = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->customised_roles()
				: 0U,
			.SyntaxClassifierComposed       = Implementation->SyntaxClassification != nullptr,
			.AdoptedClassifications         = Implementation->AdoptedClassifications,
			.ClassificationSubmissions      = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->classification_submissions()
				: 0U,
			.ClassificationCacheHits        = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->cache_hits()
				: 0U,
			.CoalescedClassifications       = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->coalesced_classifications()
				: 0U,
			.DroppedClassifications         = Implementation->DroppedClassifications,
			.ClassificationFailures         = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->classification_failures()
				: 0U,
			.ClassificationTerminals        = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->classification_terminals()
				: 0U,
			.DisplayedLeftClassified        = Implementation->Window != nullptr
				&& static_cast<bool>(Implementation->Window->comparison_surface()
										 ->token_spans(core::diff_side::left)),
			.DisplayedRightClassified = Implementation->Window != nullptr
				&& static_cast<bool>(Implementation->Window->comparison_surface()
										 ->token_spans(core::diff_side::right)),
			.DisplayedLeftSpans = displayed_span_count(
				core::diff_side::left),
			.DisplayedRightSpans = displayed_span_count(
				core::diff_side::right),
			.DisplayedComparison = Implementation->Window != nullptr
				&& static_cast<bool>(Implementation->Window->comparison_surface()
										 ->current_result()),
			.DisplayAdoptedClassifications
			= Implementation->DisplayAdoptedClassifications,
			.AppearanceMode                = Implementation->AppearanceHost != nullptr
				? std::string{ core::stable_name(
					  Implementation->AppearanceHost->settings().Mode) }
				: std::string{ },
			.AppearanceTabWidth            = Implementation->AppearanceHost != nullptr
				? Implementation->AppearanceHost->settings().Typography.Code.TabWidth
				: 0U,
			.AppearanceDivergences         = Implementation->AppearanceDivergences,
			.ProfileMode                   = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->mode()
				: core::appearance_mode::system,
			.ProfileVariant                = Implementation->AppearanceProfile
				? Implementation->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.NotificationDeliveryComposed  = Implementation->Notifications != nullptr,
			.NotificationPermission        = Implementation->NotificationSink != nullptr
				? Implementation->NotificationSink->permission()
				: application::notification_permission::refused,
			.NotificationRequestsAttempted = Implementation->NotificationRequestsAttempted,
			.NotificationRequestsSubmitted = Implementation->Notifications != nullptr
				? Implementation->Notifications->statistics().Submitted
				: 0U,
			.NotificationRequestsSuppressedUnattributable
			= Implementation->Notifications != nullptr
				? Implementation->Notifications->statistics()
					  .SuppressedUnattributableActivation
				: 0U,
			.NotificationActivations       = Implementation->Notifications != nullptr
				? Implementation->Notifications->statistics().Activations
				: 0U,
			.NotificationSelections        = Implementation->NotificationSelections,
			.NotificationSelectionFailures = Implementation->NotificationSelectionFailures,
			.EpochStatus                   = Implementation->EpochHost->state().Status,
		};
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::network_isolation_probe()
		-> bool
	{
		using namespace adapters::github::detail;
		const auto profile = validate_github_host_profile(native_profile_candidate(
			core::host_profile_id{ 901U },
			core::git_host_id{ 902U },
			application::github_profile_revision{ 903U },
			application::github_server_contract::ghes_3_18_2022_11_28,
			application::github_authentication_transport::native_https,
			"example.com",
			github_profile_provenance::managed));
		if (!profile.has_value()) {
			return false;
		}
		const auto wire = compile_read_request({
			.Server    = server_contract::ghes_3_18,
			.Operation = read_operation::get_viewer_identity,
			.Page      = 1U,
		});
		if (!wire.has_value()) {
			return false;
		}
		const auto context = core::github_session_request_context{
			.Request            = core::request_id{ 904U },
			.ApplicationSession = core::application_session_id{ 905U },
			.Profile            = profile->profile(),
			.IdentityGeneration = core::github_identity_generation{ 906U },
		};
		constexpr auto credentialText = std::string_view{
			"bpr-network-isolation-probe-not-a-credential"
		};
		auto credential = application::github_credential_material::create(
			std::as_bytes(std::span{ credentialText }));
		if (!credential.has_value()) {
			return false;
		}
		auto transport = make_native_https_bootstrap_transport(
			*profile, context, std::move(*credential));
		if (!transport.has_value()) {
			return false;
		}
		const auto        completion = (*transport)->perform(github_wire_request{
																 .Correlation = {
																	 .Parent          = context,
																	 .ProfileRevision = profile->revision(),
																	 .Host            = profile->host(),
																	 .Operation       = read_operation::get_viewer_identity,
																	 .Page            = 1U,
																 },
																 .Wire = *wire,
															 },
															 std::stop_token{ });
		const auto* const failure    = std::get_if<github_wire_error>(&completion);
		return failure != nullptr
			&& failure->Error.Failure
			== application::github_provider_failure::network;
	}

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

	auto detail::github_application_fixture_access::queue_notification_evidence(
		const github_application& aApplication) noexcept
		-> detail::queue_notification_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		const auto statistics    = implementation->Notifications != nullptr
			? implementation->Notifications->statistics()
			: application::notification_delivery_statistics{ };
		auto       lastSubmitted = std::optional<application::delivered_notification>{ };
		if (implementation->FixtureNotificationSink != nullptr) {
			lastSubmitted
				= implementation->FixtureNotificationSink->submitted_payload();
		} else if (implementation->SystemNotificationSink != nullptr) {
			lastSubmitted
				= implementation->SystemNotificationSink->submitted_payload();
		}
		return {
			.RequestsAttempted  = implementation->NotificationRequestsAttempted,
			.Submitted          = statistics.Submitted,
			.DisplaySubmissions = implementation->CapturedNotificationDisplays,
			.SuppressedUnattributableActivation
			= statistics.SuppressedUnattributableActivation,
			.Activations       = statistics.Activations,
			.Selections        = implementation->NotificationSelections,
			.SelectionFailures = implementation->NotificationSelectionFailures,
			.QueueQuiescent    = implementation->QueueHost != nullptr
				&& implementation->QueueHost->is_quiescent(),
			.LastSubmitted = std::move(lastSubmitted),
		};
	}

	auto detail::github_application_fixture_access::activate_fixture_notification(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
			&& implementation->FixtureNotificationSink != nullptr
			&& implementation->FixtureNotificationSink->activate();
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
			const auto& groups   = comparison->current_result()->Alignment.Groups;
			const auto  selected = comparison->selection().has_value()
					&& comparison->selection()->Group.has_value()
				? std::ranges::find(
					  groups,
					  *comparison->selection()->Group,
					  &core::change_group::Id)
				: groups.end();
			auto*       action   = selected == groups.end()
					|| std::next(selected) != groups.end()
				? window->next_change_action()
				: window->previous_change_action();
			if (action == nullptr || !action->isEnabled()) {
				return fail();
			}

			probe.Selection        = comparison->selection();
			probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
			action->trigger();
			probe.Phase = state_type::phase::awaiting_change_projection;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_change_projection) {
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

			const auto& groups = comparison->current_result()->Alignment.Groups;
			if (groups.size() < 2U) {
				return fail();
			}
			for (auto attempts = std::size_t{ 0U };
				 attempts < groups.size()
				 && (!comparison->selection().has_value()
					 || comparison->selection()->Group != groups.back().Id);
				 ++attempts) {
				if (!comparison->navigate_change(
						core::change_navigation_direction::next,
						core::change_navigation_policy::stop)) {
					return fail();
				}
			}
			if (!comparison->selection().has_value()
				|| comparison->selection()->Group != groups.back().Id) {
				return fail();
			}
			auto wrapCommands = core::workbench_command_delta{ };
			for (auto index = std::size_t{ 0U };
				 index < wrapCommands.Commands.size(); ++index) {
				wrapCommands.Commands[index] = core::workbench_command_projection{
					.Id             = static_cast<core::workbench_command_id>(index),
					.Enabled        = true,
					.DisabledReason = core::workbench_reason_code::none,
				};
			}
			wrapCommands.Policy = core::change_navigation_policy::wrap;
			window->publish(application::workbench_projection{
				.Context  = implementation->LastSelectedDiff->Request.Context,
				.Commands = wrapCommands,
			});
			const auto wrapDispatches = implementation->WorkbenchChangeDispatches;
			window->next_change_action()->trigger();
			probe.Evidence.ChangeDispatch
				= implementation->WorkbenchChangeDispatches == wrapDispatches + 1U
				&& comparison->selection().has_value()
				&& comparison->selection()->Group == groups.front().Id;
			if (!probe.Evidence.ChangeDispatch) {
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
		inline constexpr std::string_view uiforetw_base_oid
			= "8b70ee179fe55b705ee4e613ecbc89a0acebc1bc";
		inline constexpr std::string_view uiforetw_head_oid
			= "e40058be7d5fe319342dffb567ce30e42bc6f4de";
		inline constexpr std::string_view uiforetw_resource_path
			= "UIforETW/Resource.h";
		inline constexpr std::string_view uiforetw_resource_base_blob
			= "7da9aac464237ee5f1e2c9bc821598ff1f02b352";
		inline constexpr std::string_view uiforetw_resource_head_blob
			= "79a81a859f1b281c99e3c624b3263e618d7e1652";
		inline constexpr std::string_view uiforetw_support_path
			= "UIforETW/Support.cpp";
		inline constexpr std::string_view uiforetw_rc_path
			= "UIforETW/UIforETW.rc";
		inline constexpr std::string_view uiforetw_rc_base_blob
			= "b995e189fb58da146945e1fb22380cdf4a7f4e47";
		inline constexpr std::string_view uiforetw_rc_head_blob
			= "6335c1b2e14cd5e63142ce1dfc283fcf538fb134";

		[[nodiscard]] auto decoded_line(
			const core::decoded_text_document& aDocument,
			const std::uint32_t                aLine) -> std::string_view
		{
			if (aLine >= aDocument.Lines.size()) {
				return { };
			}
			const auto& span = aDocument.Lines[aLine].Content;
			if (span.End < span.Begin || span.End > aDocument.Bytes.size()) {
				return { };
			}
			return {
				reinterpret_cast<const char*>(aDocument.Bytes.data() + span.Begin),
				static_cast<std::size_t>(span.End - span.Begin),
			};
		}

		struct intraline_pair_location
		{
			std::uint32_t                        Left  = 0U;
			std::uint32_t                        Right = 0U;
			core::byte_span                      LeftSpan;
			core::byte_span                      RightSpan;
			core::alignment_segment_id           Segment;
			std::optional<core::change_group_id> Group;
		};

		[[nodiscard]] auto find_intraline_pair(
			const core::computed_file_diff& aResult,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment)
			-> std::optional<intraline_pair_location>
		{
			if (!aResult.LeftDocument.has_value()
				|| !aResult.RightDocument.has_value()) {
				return std::nullopt;
			}
			for (const auto& edit : aResult.Intraline) {
				if (edit.Left.empty() || edit.Right.empty()) {
					continue;
				}
				const auto pair = std::ranges::find_if(
					aResult.Alignment.Pairs,
					[&edit](const core::line_pair& aPair) {
						return aPair.Id == edit.Pair;
					});
				if (pair == aResult.Alignment.Pairs.end()) {
					continue;
				}
				const auto left           = decoded_line(*aResult.LeftDocument, pair->LeftLine);
				const auto right          = decoded_line(*aResult.RightDocument, pair->RightLine);
				const auto leftAt         = left.find(aLeftFragment);
				const auto rightAt        = right.find(aRightFragment);
				const auto insideFragment = [](const core::byte_span aSpan,
											   const std::size_t     aBegin,
											   const std::size_t     aExtent) {
					return aBegin != std::string_view::npos
						&& aSpan.Begin >= aBegin
						&& aSpan.End <= aBegin + aExtent;
				};
				if (insideFragment(edit.Left, leftAt, aLeftFragment.size())
					&& insideFragment(
						edit.Right, rightAt, aRightFragment.size())) {
					const auto segment = std::ranges::find_if(
						aResult.Alignment.Segments,
						[&pair](const core::alignment_segment& aSegment) {
							return aSegment.Id == pair->Segment;
						});
					if (segment == aResult.Alignment.Segments.end()) {
						continue;
					}
					return intraline_pair_location{
						.Left      = pair->LeftLine,
						.Right     = pair->RightLine,
						.LeftSpan  = edit.Left,
						.RightSpan = edit.Right,
						.Segment   = pair->Segment,
						.Group     = segment->Group,
					};
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto character_offset(
			const std::span<const std::uint8_t> aBytes,
			const std::uint64_t                 aOffset) -> int
		{
			const auto bounded = std::min<std::size_t>(
				static_cast<std::size_t>(aOffset), aBytes.size());
			return static_cast<int>(
				QString::fromUtf8(
					reinterpret_cast<const char*>(aBytes.data()),
					static_cast<qsizetype>(bounded))
					.size());
		}

		[[nodiscard]] auto intraline_selection_rect(
			QPlainTextEdit&                    aPane,
			const core::decoded_text_document& aDocument,
			const std::uint32_t                aLine,
			const core::byte_span              aSpan) -> std::optional<QRect>
		{
			if (aLine >= aDocument.Lines.size() || aSpan.empty()) {
				return std::nullopt;
			}
			const auto& decoded = aDocument.Lines[aLine];
			if (decoded.Content.End < decoded.Content.Begin
				|| decoded.Content.End > aDocument.Bytes.size()) {
				return std::nullopt;
			}
			const auto bytes = std::span<const std::uint8_t>{
				aDocument.Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(decoded.Content.End - decoded.Content.Begin)
			};
			const auto from  = character_offset(bytes, aSpan.Begin);
			const auto to    = character_offset(bytes, aSpan.End);
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid() || from >= to) {
				return std::nullopt;
			}
			auto start = QTextCursor{ block };
			start.setPosition(block.position() + from);
			auto finish = QTextCursor{ block };
			finish.setPosition(block.position() + to);
			return aPane.cursorRect(start).united(aPane.cursorRect(finish));
		}

		[[nodiscard]] auto reveal_intraline_pairs(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const intraline_pair_location&  aFirst,
			const intraline_pair_location&  aSecond) -> bool;

		[[nodiscard]] auto navigate_to_group(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const core::change_group_id&    aTarget) -> bool
		{
			const auto target = std::ranges::find(
				aResult.Alignment.Groups, aTarget, &core::change_group::Id);
			if (target == aResult.Alignment.Groups.end()) {
				return false;
			}
			const auto targetIndex = static_cast<std::size_t>(
				std::distance(aResult.Alignment.Groups.begin(), target));
			for (auto attempt = std::size_t{ 0U };
				 attempt <= aResult.Alignment.Groups.size(); ++attempt) {
				const auto selection = aWorkbench.selection();
				if (selection.has_value() && selection->Group == aTarget) {
					return true;
				}
				const auto current      = selection.has_value()
						&& selection->Group.has_value()
					? std::ranges::find(
						  aResult.Alignment.Groups,
						  *selection->Group,
						  &core::change_group::Id)
					: aResult.Alignment.Groups.end();
				const auto currentIndex = current != aResult.Alignment.Groups.end()
					? std::optional<std::size_t>{ static_cast<std::size_t>(
						  std::distance(aResult.Alignment.Groups.begin(), current)) }
					: std::nullopt;
				const auto direction    = !currentIndex.has_value()
						|| *currentIndex < targetIndex
					? core::change_navigation_direction::next
					: core::change_navigation_direction::previous;
				if (!aWorkbench.navigate_change(
						direction, core::change_navigation_policy::stop)) {
					return false;
				}
				QApplication::processEvents();
			}
			return false;
		}

		[[nodiscard]] auto reveal_intraline_pair(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment) -> bool
		{
			const auto location = find_intraline_pair(
				aResult, aLeftFragment, aRightFragment);
			if (!location.has_value()) {
				return false;
			}
			return location->Group.has_value()
				&& navigate_to_group(aWorkbench, aResult, *location->Group)
				&& reveal_intraline_pairs(
					   aWorkbench, aResult, *location, *location);
		}

		[[nodiscard]] auto reveal_intraline_pairs(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const intraline_pair_location&  aFirst,
			const intraline_pair_location&  aSecond) -> bool
		{
			const auto reveal = [](
									QPlainTextEdit&                    aPane,
									const core::decoded_text_document& aDocument,
									const std::uint32_t                aFirstLine,
									const core::byte_span              aFirstSpan,
									const std::uint32_t                aSecondLine,
									const core::byte_span              aSecondSpan) {
				const auto first = intraline_selection_rect(
					aPane, aDocument, aFirstLine, aFirstSpan);
				const auto second = intraline_selection_rect(
					aPane, aDocument, aSecondLine, aSecondSpan);
				if (!first.has_value() || !second.has_value()) {
					return false;
				}
				auto* const horizontal   = aPane.horizontalScrollBar();
				const auto  documentLeft = std::min(first->left(), second->left())
					+ horizontal->value();
				const auto documentRight = std::max(first->right(), second->right())
					+ horizontal->value();
				const auto target = std::clamp(
					((documentLeft + documentRight) / 2)
						- (aPane.viewport()->width() / 2),
					horizontal->minimum(),
					horizontal->maximum());
				horizontal->setValue(target);
				return true;
			};
			if (aSecond.Left < aFirst.Left || aSecond.Right < aFirst.Right
				|| !reveal(
					*aWorkbench.left_pane(),
					*aResult.LeftDocument,
					aFirst.Left,
					aFirst.LeftSpan,
					aSecond.Left,
					aSecond.LeftSpan)
				|| !reveal(
					*aWorkbench.right_pane(),
					*aResult.RightDocument,
					aFirst.Right,
					aFirst.RightSpan,
					aSecond.Right,
					aSecond.RightSpan)) {
				return false;
			}
			QApplication::processEvents();
			const auto visible = [](
									 QPlainTextEdit&                    aPane,
									 const core::decoded_text_document& aDocument,
									 const std::uint32_t                aLine,
									 const core::byte_span              aSpan) {
				const auto rect = intraline_selection_rect(
					aPane, aDocument, aLine, aSpan);
				return rect.has_value() && !rect->isEmpty()
					&& QRect{ QPoint{ 0, 0 }, aPane.viewport()->size() }.contains(
						*rect, true);
			};
			return visible(
					   *aWorkbench.left_pane(),
					   *aResult.LeftDocument,
					   aFirst.Left,
					   aFirst.LeftSpan)
				&& visible(
					   *aWorkbench.left_pane(),
					   *aResult.LeftDocument,
					   aSecond.Left,
					   aSecond.LeftSpan)
				&& visible(
					   *aWorkbench.right_pane(),
					   *aResult.RightDocument,
					   aFirst.Right,
					   aFirst.RightSpan)
				&& visible(
					   *aWorkbench.right_pane(),
					   *aResult.RightDocument,
					   aSecond.Right,
					   aSecond.RightSpan);
		}

		/// \brief The colours one intraline role can legitimately paint as.
		///
		/// \details BPR-0321 composites the row's selection wash into the
		/// emphasis, so one semantic role renders as its palette colour on an
		/// unselected row and as that colour under the wash on a selected one.
		/// Both are the role. A check that accepted only the bare colour would
		/// read a correctly washed mark as a missing one, which is exactly how
		/// this course failed when the wash first landed.
		[[nodiscard]] auto role_renderings(
			const QPlainTextEdit&          aPane,
			const QColor&                  aRole,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant) -> std::array<QColor, 3U>
		{
			const auto highlight = aPane.palette().color(QPalette::Highlight);
			const auto blended   = core::composited(
				core::appearance_color{ static_cast<std::uint8_t>(highlight.red()),
										static_cast<std::uint8_t>(highlight.green()),
										static_cast<std::uint8_t>(highlight.blue()),
										255U },
				core::appearance_color{ static_cast<std::uint8_t>(aRole.red()),
										static_cast<std::uint8_t>(aRole.green()),
										static_cast<std::uint8_t>(aRole.blue()),
										255U },
				core::selection_wash_alpha);
			// The third rendering is what a selected row shows: the band that
			// row wears, with the wash over it, strengthened in its own hue.
			auto derived = QColor{ };
			if (aRowBand.isValid()) {
				const auto washedBand = core::composited(
					core::appearance_color{
						static_cast<std::uint8_t>(highlight.red()),
						static_cast<std::uint8_t>(highlight.green()),
						static_cast<std::uint8_t>(highlight.blue()),
						255U },
					core::appearance_color{
						static_cast<std::uint8_t>(aRowBand.red()),
						static_cast<std::uint8_t>(aRowBand.green()),
						static_cast<std::uint8_t>(aRowBand.blue()),
						255U },
					core::selection_wash_alpha);
				const auto mark
					= core::derived_emphasis_fill(washedBand, aVariant);
				derived = QColor{ mark.Red, mark.Green, mark.Blue };
			}
			return { aRole,
					 QColor{ blended.Red, blended.Green, blended.Blue },
					 derived };
		}

		[[nodiscard]] auto role_paint_count(
			const QPlainTextEdit&          aPane,
			const QColor&                  aExpected,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant) -> std::size_t
		{
			// BPR-0323 marks the changed characters with a rule as well as a
			// fill, and drops the fill on a selected row because a tint there
			// read as a foreign colour. The pane already fixes which side a
			// span belongs to, so an underlined span is the role just as much
			// as a filled one; requiring the fill would read the new mark as
			// a missing one.
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			return static_cast<std::size_t>(std::ranges::count_if(
				aPane.extraSelections(),
				[&accepted](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& (std::ranges::contains(
								accepted,
								aSelection.format.background().color())
							|| aSelection.format.fontUnderline())
						&& aSelection.cursor.hasSelection();
				}));
		}

		[[nodiscard]] auto intraline_roles_visible(
			const qt::diff_workbench&     aWorkbench,
			const qt::appearance_profile& aProfile,
			const std::size_t             aMinimum) -> bool
		{
			return role_paint_count(
					   *aWorkbench.left_pane(),
					   aProfile.color(
						   core::appearance_role::diff_intraline_removed_background),
					   aProfile.color(core::appearance_role::diff_removed_background),
					   aProfile.variant())
				>= aMinimum
				&& role_paint_count(
					   *aWorkbench.right_pane(),
					   aProfile.color(
						   core::appearance_role::diff_intraline_added_background),
					   aProfile.color(core::appearance_role::diff_added_background),
					   aProfile.variant())
				>= aMinimum;
		}

		struct intraline_render_observation
		{
			intraline_pair_location Pair;
			QRect                   LeftRect;
			QRect                   RightRect;
			std::size_t             LeftRolePixels  = 0U;
			std::size_t             RightRolePixels = 0U;
		};

		[[nodiscard]] auto role_pixel_count(
			QPlainTextEdit&                aPane,
			const QRect&                   aRect,
			const QColor&                  aExpected,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant) -> std::size_t
		{
			const auto image = aPane.viewport()->grab(aRect).toImage().convertToFormat(
				QImage::Format_ARGB32);
			if (image.isNull() || !aExpected.isValid()) {
				return 0U;
			}
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			auto       count    = std::size_t{ 0U };
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto pixel = image.pixel(x, y);
					if (std::ranges::any_of(
							accepted,
							[pixel](const QColor& aCandidate) {
								return aCandidate.isValid()
									&& aCandidate.rgba() == pixel;
							})) {
						++count;
					}
				}
			}
			return count;
		}

		/// \brief Records the actual normal and disabled pixels of one toolbar icon.
		struct navigation_icon_contrast_observation
		{
			QColor       Background;
			QColor       NormalForeground;
			QColor       DisabledForeground;
			std::size_t  NormalForegroundPixels     = 0U;
			std::size_t  DisabledForegroundPixels   = 0U;
			std::int64_t NormalContrastHundredths   = 0;
			std::int64_t DisabledContrastHundredths = 0;
			bool         StatesDistinct             = false;
		};

		[[nodiscard]] auto exact_icon_foreground_pixels(
			const QIcon&      aIcon,
			const QSize&      aSize,
			const qreal       aScale,
			const QIcon::Mode aMode,
			const QColor&     aExpected) -> std::size_t
		{
			const auto image = aIcon.pixmap(aSize, aScale, aMode, QIcon::Off)
								   .toImage()
								   .convertToFormat(QImage::Format_ARGB32);
			if (image.isNull() || !aExpected.isValid()) {
				return 0U;
			}
			constexpr auto rgbComponentMask = QRgb{ 0x00FF'FFFFU };
			const auto     expected         = aExpected.rgb() & rgbComponentMask;
			auto           count            = std::size_t{ 0U };
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto pixel = image.pixel(x, y);
					if (qAlpha(pixel) >= 224
						&& (pixel & rgbComponentMask) == expected) {
						++count;
					}
				}
			}
			return count;
		}

		[[nodiscard]] auto observe_navigation_icon_contrast(
			const QToolButton& aButton)
			-> navigation_icon_contrast_observation
		{
			const auto background = aButton.palette().color(
				QPalette::Active, QPalette::Button);
			const auto normal = aButton.palette().color(
				QPalette::Active, QPalette::ButtonText);
			const auto disabled = aButton.palette().color(
				QPalette::Disabled, QPalette::ButtonText);
			const auto size          = aButton.iconSize().expandedTo(QSize{ 16, 16 });
			const auto scale         = aButton.devicePixelRatioF();
			const auto normalImage   = aButton.icon()
										   .pixmap(size, scale, QIcon::Normal, QIcon::Off)
										   .toImage();
			const auto disabledImage = aButton.icon()
										   .pixmap(size, scale, QIcon::Disabled, QIcon::Off)
										   .toImage();
			return {
				.Background             = background,
				.NormalForeground       = normal,
				.DisabledForeground     = disabled,
				.NormalForegroundPixels = exact_icon_foreground_pixels(
					aButton.icon(), size, scale, QIcon::Normal, normal),
				.DisabledForegroundPixels = exact_icon_foreground_pixels(
					aButton.icon(), size, scale, QIcon::Disabled, disabled),
				.NormalContrastHundredths   = static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(normal)))),
				.DisabledContrastHundredths = static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(disabled)))),
				.StatesDistinct             = !normalImage.isNull()
					&& !disabledImage.isNull() && normalImage != disabledImage,
			};
		}

		[[nodiscard]] auto exact_role_selection_present(
			const QPlainTextEdit&              aPane,
			const core::decoded_text_document& aDocument,
			const std::uint32_t                aLine,
			const core::byte_span              aSpan,
			const QColor&                      aExpected,
			const QColor&                      aRowBand,
			const core::appearance_variant     aVariant) -> bool
		{
			if (aLine >= aDocument.Lines.size()) {
				return false;
			}
			const auto& decoded = aDocument.Lines[aLine];
			if (decoded.Content.End < decoded.Content.Begin
				|| decoded.Content.End > aDocument.Bytes.size()) {
				return false;
			}
			const auto bytes = std::span<const std::uint8_t>{
				aDocument.Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(decoded.Content.End - decoded.Content.Begin)
			};
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid()) {
				return false;
			}
			const auto from     = block.position() + character_offset(bytes, aSpan.Begin);
			const auto to       = block.position() + character_offset(bytes, aSpan.End);
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			return std::ranges::any_of(
				aPane.extraSelections(),
				[&accepted, from, to](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& (std::ranges::contains(
								accepted,
								aSelection.format.background().color())
							|| aSelection.format.fontUnderline())
						&& aSelection.cursor.selectionStart() == from
						&& aSelection.cursor.selectionEnd() == to;
				});
		}

		[[nodiscard]] auto observe_intraline_render(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const qt::appearance_profile&   aProfile,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment)
			-> std::optional<intraline_render_observation>
		{
			const auto pair = find_intraline_pair(
				aResult, aLeftFragment, aRightFragment);
			if (!pair.has_value()) {
				return std::nullopt;
			}
			auto* const left     = aWorkbench.left_pane();
			auto* const right    = aWorkbench.right_pane();
			const auto  leftRect = intraline_selection_rect(
				*left, *aResult.LeftDocument, pair->Left, pair->LeftSpan);
			const auto rightRect = intraline_selection_rect(
				*right, *aResult.RightDocument, pair->Right, pair->RightSpan);
			if (!leftRect.has_value() || !rightRect.has_value()
				|| leftRect->isEmpty() || rightRect->isEmpty()
				|| !QRect{ QPoint{ 0, 0 }, left->viewport()->size() }.contains(
					*leftRect, true)
				|| !QRect{ QPoint{ 0, 0 }, right->viewport()->size() }.contains(
					*rightRect, true)) {
				return std::nullopt;
			}
			const auto removed = aProfile.color(
				core::appearance_role::diff_intraline_removed_background);
			const auto added = aProfile.color(
				core::appearance_role::diff_intraline_added_background);
			if (!exact_role_selection_present(
					*left,
					*aResult.LeftDocument,
					pair->Left,
					pair->LeftSpan,
					removed,
					aProfile.color(core::appearance_role::diff_removed_background),
					aProfile.variant())
				|| !exact_role_selection_present(
					*right,
					*aResult.RightDocument,
					pair->Right,
					pair->RightSpan,
					added,
					aProfile.color(core::appearance_role::diff_added_background),
					aProfile.variant())) {
				return std::nullopt;
			}
			return intraline_render_observation{
				.Pair           = *pair,
				.LeftRect       = *leftRect,
				.RightRect      = *rightRect,
				.LeftRolePixels = role_pixel_count(
					*left,
					*leftRect,
					removed,
					aProfile.color(core::appearance_role::diff_removed_background),
					aProfile.variant()),
				.RightRolePixels = role_pixel_count(
					*right,
					*rightRect,
					added,
					aProfile.color(core::appearance_role::diff_added_background),
					aProfile.variant()),
			};
		}

		struct connector_pixel_observation
		{
			QColor       Background;
			QColor       Dominant;
			std::size_t  SampledPixels            = 0U;
			std::size_t  ContrastPixels           = 0U;
			std::size_t  DominantPixels           = 0U;
			std::int64_t MedianContrastHundredths = 0;
		};

		struct connector_render_observation
		{
			QRectF                      Bounds;
			QPointF                     Start;
			QPointF                     End;
			connector_pixel_observation Pixels;
			bool                        Selected      = false;
			bool                        LeftEndpoint  = false;
			bool                        RightEndpoint = false;
			bool                        LeftOverlap   = false;
			bool                        RightOverlap  = false;
		};

		[[nodiscard]] auto dominant_connector_pixel(std::vector<QRgb> aPixels)
			-> std::pair<QColor, std::size_t>
		{
			if (aPixels.empty()) {
				return { QColor{ }, 0U };
			}
			std::ranges::sort(aPixels);
			auto bestPixel = aPixels.front();
			auto bestCount = std::size_t{ 0U };
			for (auto first = aPixels.begin(); first != aPixels.end();) {
				const auto last = std::ranges::find_if(
					first, aPixels.end(), [first](const QRgb aPixel) {
						return aPixel != *first;
					});
				const auto count = static_cast<std::size_t>(
					std::distance(first, last));
				if (count > bestCount) {
					bestPixel = *first;
					bestCount = count;
				}
				first = last;
			}
			return { QColor::fromRgba(bestPixel), bestCount };
		}

		[[nodiscard]] auto observe_connector_pixels(
			qt::diff_link_map&           aMap,
			const qt::visible_diff_link& aTarget)
			-> connector_pixel_observation
		{
			const auto image = aMap.grab().toImage().convertToFormat(
				QImage::Format_ARGB32);
			const auto scale = image.devicePixelRatio();
			if (image.isNull() || scale <= 0.0) {
				return { };
			}

			const auto links   = aMap.visible_links();
			auto       stroker = QPainterPathStroker{ };
			stroker.setWidth(3.0);
			const auto targetMask
				= aTarget.Path.united(stroker.createStroke(aTarget.Path));
			auto backgroundPixels = std::vector<QRgb>{ };
			auto targetPixels     = std::vector<QRgb>{ };
			backgroundPixels.reserve(
				static_cast<std::size_t>(image.sizeInBytes() / 4));
			targetPixels.reserve(backgroundPixels.capacity() / 4U);
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto point = QPointF{
						(static_cast<qreal>(x) + 0.5) / scale,
						(static_cast<qreal>(y) + 0.5) / scale,
					};
					const auto inAny = std::ranges::any_of(
						links,
						[point](const qt::visible_diff_link& aLink) {
							return aLink.Path.contains(point);
						});
					if (targetMask.contains(point)) {
						const auto overlapsAnother = std::ranges::any_of(
							links,
							[&aTarget, point](const qt::visible_diff_link& aLink) {
								return &aLink != &aTarget
									&& aLink.Path.contains(point);
							});
						if (!overlapsAnother) {
							targetPixels.push_back(image.pixel(x, y));
						}
					} else if (!inAny) {
						backgroundPixels.push_back(image.pixel(x, y));
					}
				}
			}

			const auto [background, backgroundCount]
				= dominant_connector_pixel(std::move(backgroundPixels));
			const auto [dominant, dominantCount]
				= dominant_connector_pixel(targetPixels);
			if (!background.isValid() || backgroundCount == 0U
				|| !dominant.isValid() || targetPixels.empty()) {
				return { };
			}

			auto contrasts = std::vector<std::int64_t>{ };
			contrasts.reserve(targetPixels.size());
			for (const auto pixel : targetPixels) {
				contrasts.push_back(static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(QColor::fromRgba(pixel))))));
			}
			std::ranges::sort(contrasts);
			return {
				.Background     = background,
				.Dominant       = dominant,
				.SampledPixels  = targetPixels.size(),
				.ContrastPixels = static_cast<std::size_t>(
					std::ranges::count_if(
						contrasts,
						[](const auto aContrast) { return aContrast >= 300; })),
				.DominantPixels           = dominantCount,
				.MedianContrastHundredths = contrasts[contrasts.size() / 2U],
			};
		}

		[[nodiscard]] auto observe_connector_render(
			qt::diff_workbench&                 aWorkbench,
			const intraline_render_observation& aTarget)
			-> std::optional<connector_render_observation>
		{
			auto* const map   = aWorkbench.link_map();
			const auto  found = std::ranges::find_if(
				map->visible_links(),
				[&aTarget](const qt::visible_diff_link& aLink) {
					return aLink.Identity.Segment == aTarget.Pair.Segment;
				});
			if (found == map->visible_links().end()) {
				return std::nullopt;
			}
			const auto mapTop   = map->mapToGlobal(QPoint{ 0, 0 }).y();
			const auto relative = [mapTop](
									  QPlainTextEdit& aPane,
									  const QRect&    aRect) {
				const auto top = aPane.viewport()->mapToGlobal(aRect.topLeft()).y()
					- mapTop;
				return std::pair{ static_cast<qreal>(top),
								  static_cast<qreal>(top + aRect.height()) };
			};
			const auto [leftTop, leftBottom] = relative(
				*aWorkbench.left_pane(), aTarget.LeftRect);
			const auto [rightTop, rightBottom] = relative(
				*aWorkbench.right_pane(), aTarget.RightRect);
			const auto overlaps = [](const qreal aFirstTop,
									 const qreal aFirstBottom,
									 const qreal aSecondTop,
									 const qreal aSecondBottom) {
				return std::max(aFirstTop, aSecondTop)
					<= std::min(aFirstBottom, aSecondBottom);
			};
			const auto startElement = found->Path.elementAt(0);
			const auto endElement   = found->Path.elementAt(3);
			const auto start        = QPointF{ startElement.x, startElement.y };
			const auto end          = QPointF{ endElement.x, endElement.y };
			const auto selection    = map->selected();
			const auto isSelected   = selection.has_value()
				&& (selection->Group.has_value()
							&& found->Identity.Group.has_value()
						? selection->Group == found->Identity.Group
						: selection->Segment == found->Identity.Segment);
			return connector_render_observation{
				.Bounds        = found->Path.boundingRect(),
				.Start         = start,
				.End           = end,
				.Pixels        = observe_connector_pixels(*map, *found),
				.Selected      = isSelected,
				.LeftEndpoint  = std::abs(start.x()) <= 0.5,
				.RightEndpoint = std::abs(end.x() - map->width()) <= 0.5,
				.LeftOverlap   = overlaps(
					found->LeftTop, found->LeftBottom, leftTop, leftBottom),
				.RightOverlap = overlaps(
					found->RightTop, found->RightBottom, rightTop, rightBottom),
			};
		}

		[[nodiscard]] auto rectangle_json(const QRect& aRect) -> QJsonObject
		{
			return {
				{ QStringLiteral("x"), aRect.x() },
				{ QStringLiteral("y"), aRect.y() },
				{ QStringLiteral("width"), aRect.width() },
				{ QStringLiteral("height"), aRect.height() },
			};
		}

		[[nodiscard]] auto rectangle_json(const QRectF& aRect) -> QJsonObject
		{
			return {
				{ QStringLiteral("x"), aRect.x() },
				{ QStringLiteral("y"), aRect.y() },
				{ QStringLiteral("width"), aRect.width() },
				{ QStringLiteral("height"), aRect.height() },
			};
		}

		[[nodiscard]] auto point_json(const QPointF& aPoint) -> QJsonObject
		{
			return {
				{ QStringLiteral("x"), aPoint.x() },
				{ QStringLiteral("y"), aPoint.y() },
			};
		}

		[[nodiscard]] auto drawer_row(
			const qt::workbench_window& aWindow,
			const std::string_view      aPath) -> QModelIndex
		{
			if (aWindow.files_drawer() == nullptr
				|| aWindow.files_drawer()->contents_view() == nullptr
				|| aWindow.files_drawer()->contents_view()->model() == nullptr) {
				return { };
			}
			const auto* const model    = aWindow.files_drawer()->contents_view()->model();
			const auto        expected = QString::fromUtf8(
				aPath.data(), static_cast<qsizetype>(aPath.size()));
			for (auto row = 0; row < model->rowCount(); ++row) {
				const auto index = model->index(row, 0);
				if (index.data(Qt::DisplayRole).toString() == expected) {
					return index;
				}
			}
			return { };
		}

		[[nodiscard]] auto selected_diff_publication_matches(
			const auto&            aImplementation,
			const std::string_view aPath,
			const std::string_view aBaseBlob,
			const std::string_view aHeadBlob) -> bool
		{
			if (!aImplementation.LastSelectedDiff.has_value()) {
				return false;
			}
			const auto  left        = core::parse_git_blob_oid(aBaseBlob);
			const auto  right       = core::parse_git_blob_oid(aHeadBlob);
			const auto& publication = *aImplementation.LastSelectedDiff;
			return left.has_value() && right.has_value()
				&& publication.Request.File.PresentationPath == aPath
				&& publication.Request.File.CurrentPath
				== std::filesystem::path{ aPath }
			&& publication.Request.File.File == core::file_identity_of(aPath)
				&& publication.Key.LeftBlob == left
				&& publication.Key.RightBlob == right
				&& publication.Key.File == core::file_identity_of(aPath);
		}

		[[nodiscard]] auto selected_diff_matches(
			const auto&            aImplementation,
			const std::string_view aPath,
			const std::string_view aBaseBlob,
			const std::string_view aHeadBlob) -> bool
		{
			return selected_diff_publication_matches(
					   aImplementation, aPath, aBaseBlob, aHeadBlob)
				&& aImplementation.Window != nullptr
				&& aImplementation.Window->comparison_surface() != nullptr
				&& aImplementation.Window->comparison_surface()->current_result()
				!= nullptr
				&& aImplementation.Window->comparison_surface()
					   ->current_result()
					   ->Alignment.Key
				== aImplementation.LastSelectedDiff->Key;
		}

		[[nodiscard]] auto appearance_mode_name(
			const core::appearance_mode aMode) -> QString
		{
			switch (aMode) {
			case core::appearance_mode::system:
				return QStringLiteral("system");
			case core::appearance_mode::light:
				return QStringLiteral("light");
			case core::appearance_mode::dark:
				return QStringLiteral("dark");
			case core::appearance_mode::high_contrast_system:
				return QStringLiteral("high-contrast-system");
			case core::appearance_mode::high_contrast_light:
				return QStringLiteral("high-contrast-light");
			case core::appearance_mode::high_contrast_dark:
				return QStringLiteral("high-contrast-dark");
			}
			return QStringLiteral("unknown");
		}
	} // namespace

	auto detail::github_application_fixture_access::
		exercise_native_appearance_selected_diff(
			github_application& aApplication)
			-> detail::native_appearance_selected_diff_probe_result
	{
		using result     = detail::native_appearance_selected_diff_probe_result;
		using reason     = detail::native_appearance_selected_diff_reason;
		using state_type = github_application::implementation::
			native_appearance_selected_diff_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::refused;
		}
		if (!implementation->NativeAppearanceSelectedDiffProbe.has_value()) {
			implementation->NativeAppearanceSelectedDiffProbe.emplace();
		}
		auto&      probe   = *implementation->NativeAppearanceSelectedDiffProbe;
		const auto pending = [&probe](const reason aReason) {
			probe.Evidence.Reason = aReason;
			return result::pending;
		};
		const auto refuse = [&probe](const reason aReason) {
			probe.Evidence.Reason = aReason;
			probe.Phase           = state_type::phase::refused;
			return result::refused;
		};

		probe.Evidence.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U;
		if (!probe.Evidence.FixtureDiffHelperUnused) {
			return refuse(reason::fixture_diff_helper_invoked);
		}
		if (probe.Phase == state_type::phase::refused) {
			return result::refused;
		}
		if (probe.Phase == state_type::phase::ready) {
			return result::ready;
		}
		if (implementation->Fixture == nullptr
			|| implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return refuse(reason::composition_unavailable);
		}

		auto* const host      = implementation->WorkbenchHost.get();
		auto* const window    = implementation->Window.get();
		auto* const workbench = window->comparison_surface();
		auto* const filesView = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		const auto& fixture   = implementation->Fixture->evidence();

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& snapshot = implementation->Host->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| !host->state().Navigation.has_value()) {
				return pending(reason::awaiting_workbench_observation);
			}
			if (snapshot->Files.size() != 1U
				|| snapshot->Files.front().Path != "review.cpp"
				|| filesView == nullptr || filesView->model() == nullptr
				|| filesView->model()->rowCount() != 1) {
				return refuse(reason::file_projection_mismatch);
			}

			const auto& handoff     = *implementation->LastAcceptedHandoff;
			const auto& epoch       = implementation->EpochHost->state();
			probe.Authority         = workbench_authority(handoff);
			probe.File              = core::file_identity_of("review.cpp");
			const auto context      = implementation->EpochHost->context(probe.Authority);
			const auto expectedBase = core::parse_git_commit_oid(fixture.BaseOid);
			const auto expectedHead = core::parse_git_commit_oid(fixture.HeadOneOid);
			const auto expectedLeftBlob
				= core::parse_git_blob_oid(fixture.BaseBlobOid);
			const auto expectedRightBlob
				= core::parse_git_blob_oid(fixture.HeadOneBlobOid);
			const auto expectedOptions = core::diff_options{ };
			const auto expectedDigest  = core::options_digest(expectedOptions);

			probe.Evidence.ExactAuthority
				= application::valid(probe.Authority)
				&& implementation->WorkbenchReview.exact(handoff)
				&& context.has_value() && epoch.Authority.has_value()
				&& context->ApplicationSession == handoff.ApplicationSession
				&& context->Authority == *epoch.Authority
				&& probe.Authority == workbench_authority(handoff);
			probe.Evidence.ExactRevisions
				= expectedBase.has_value() && expectedHead.has_value()
				&& epoch.AcceptedRevision.has_value()
				&& epoch.AcceptedComparison.has_value()
				&& epoch.AcceptedRevision->Base == *expectedBase
				&& epoch.AcceptedRevision->Head == *expectedHead
				&& epoch.AcceptedComparison->Left.Commit == *expectedBase
				&& epoch.AcceptedComparison->Right.Commit == *expectedHead
				&& context.has_value()
				&& context->Comparison == *epoch.AcceptedComparison;
			const auto actionable    = host->actionable();
			probe.Evidence.ExactFile = probe.File.valid()
				&& actionable.size() == 1U && actionable.front() == probe.File;
			probe.Evidence.ExactBlobs
				= expectedLeftBlob.has_value() && expectedRightBlob.has_value();
			probe.Evidence.ExactOptions
				= context.has_value() && expectedDigest.has_value()
				&& context->Options == expectedOptions
				&& context->OptionsDigest == expectedDigest
				&& context->Algorithm
					== core::supported_diff_algorithm_revision;
			if (!probe.Evidence.ExactAuthority) {
				return refuse(reason::fixture_authority_mismatch);
			}
			if (!probe.Evidence.ExactRevisions || !probe.Evidence.ExactBlobs
				|| !probe.Evidence.ExactOptions) {
				return refuse(reason::fixture_material_mismatch);
			}
			if (!probe.Evidence.ExactFile) {
				return refuse(reason::file_projection_mismatch);
			}

			probe.IntentDispatches   = implementation->WorkbenchIntentDispatches;
			probe.WindowStatistics   = window->statistics();
			probe.SelectedDiffBegins = implementation->SelectedDiffBegins;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			filesView->setCurrentIndex(filesView->model()->index(0, 0));
			probe.Phase = state_type::phase::awaiting_selection_projection;
			return pending(reason::awaiting_selection_projection);
		}

		if (probe.Phase
			== state_type::phase::awaiting_selection_projection) {
			const auto statistics = window->statistics();
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return refuse(reason::selected_diff_failed);
			}
			if (implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches) {
				if (statistics.FileSelections
					== probe.WindowStatistics.FileSelections + 1U) {
					probe.Evidence.SelectionGesture = true;
					return refuse(reason::selection_wire_unavailable);
				}
				return pending(reason::awaiting_selection_projection);
			}
			probe.Evidence.SelectionGesture
				= statistics.FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			probe.Evidence.SelectionIntent
				= implementation->WorkbenchIntentDispatches
					== probe.IntentDispatches + 1U
				&& host->selected() == std::optional{ probe.File };
			if (!probe.Evidence.SelectionGesture
				|| !probe.Evidence.SelectionIntent) {
				return refuse(reason::file_projection_mismatch);
			}
			probe.Phase = state_type::phase::awaiting_diff;
			return pending(reason::awaiting_selected_diff_publication);
		}

		if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
			return refuse(reason::selected_diff_failed);
		}
		if (implementation->SelectedDiffPublications
			== probe.SelectedDiffPublications) {
			return pending(reason::awaiting_selected_diff_publication);
		}
		if (implementation->SelectedDiffBegins != probe.SelectedDiffBegins + 1U
			|| implementation->SelectedDiffPublications
				!= probe.SelectedDiffPublications + 1U
			|| !implementation->LastSelectedDiff.has_value()) {
			return refuse(reason::selected_diff_mismatch);
		}

		const auto& publication = *implementation->LastSelectedDiff;
		const auto& request     = publication.Request;
		const auto& key         = publication.Key;
		const auto& epoch       = implementation->EpochHost->state();
		const auto  expectedLeftBlob
			= core::parse_git_blob_oid(fixture.BaseBlobOid);
		const auto expectedRightBlob
			= core::parse_git_blob_oid(fixture.HeadOneBlobOid);
		const auto expectedDigest = core::options_digest(core::diff_options{ });
		probe.Evidence.ExactAuthority
			= probe.Evidence.ExactAuthority
			&& request.WorkbenchAuthority == probe.Authority
			&& epoch.Authority.has_value() && key.Authority == *epoch.Authority
			&& key.Repository == epoch.Authority->Repository
			&& key.PullRequest == epoch.Authority->PullRequest;
		probe.Evidence.ExactRevisions
			= probe.Evidence.ExactRevisions
			&& epoch.AcceptedComparison.has_value()
			&& key.LeftAnchor == epoch.AcceptedComparison->Left.Identity
			&& key.RightAnchor == epoch.AcceptedComparison->Right.Identity;
		probe.Evidence.ExactFile = probe.Evidence.ExactFile
			&& request.Context.Generation == host->generation()
			&& request.File.File == probe.File
			&& request.File.PresentationPath == "review.cpp"
			&& request.File.CurrentPath == std::filesystem::path{ "review.cpp" }
			&& !request.File.PreviousPath.has_value()
			&& request.File.Change
				== application::workbench_file_change_kind::modified
			&& key.File == probe.File;
		probe.Evidence.ExactBlobs = probe.Evidence.ExactBlobs
			&& expectedLeftBlob.has_value() && expectedRightBlob.has_value()
			&& key.LeftBlob == *expectedLeftBlob
			&& key.RightBlob == *expectedRightBlob;
		probe.Evidence.ExactOptions = probe.Evidence.ExactOptions
			&& expectedDigest.has_value()
			&& request.Diff.Options == core::diff_options{ }
			&& request.Diff.OptionsDigest == *expectedDigest
			&& key.Options == *expectedDigest
			&& key.Algorithm == core::supported_diff_algorithm_revision;
		if (!probe.Evidence.ExactAuthority || !probe.Evidence.ExactRevisions
			|| !probe.Evidence.ExactFile || !probe.Evidence.ExactBlobs
			|| !probe.Evidence.ExactOptions) {
			return refuse(reason::selected_diff_mismatch);
		}
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !workbench->selection().has_value()) {
			return pending(reason::awaiting_diff_presentation);
		}
		probe.Evidence.PresentationReady
			= workbench->current_result()->Alignment.Key == key
			&& workbench->selection()->Comparison
				== workbench->current_result()->Alignment.Digest
			&& workbench->status_text().startsWith(QStringLiteral("Diff ready"));
		if (!probe.Evidence.PresentationReady) {
			return refuse(reason::selected_diff_mismatch);
		}

		probe.Evidence.Reason = reason::none;
		probe.Phase           = state_type::phase::ready;
		return result::ready;
	}

	auto detail::github_application_fixture_access::
		native_appearance_selected_diff_evidence(
			const github_application& aApplication) noexcept
		-> detail::native_appearance_selected_diff_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return detail::native_appearance_selected_diff_probe_evidence{
				.Reason = detail::native_appearance_selected_diff_reason::
					composition_unavailable,
			};
		}
		if (implementation->NativeAppearanceSelectedDiffProbe.has_value()) {
			return implementation->NativeAppearanceSelectedDiffProbe->Evidence;
		}
		return detail::native_appearance_selected_diff_probe_evidence{
			.Reason = detail::native_appearance_selected_diff_reason::
				awaiting_workbench_observation,
			.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U,
		};
	}

	auto detail::github_application_fixture_access::exercise_uiforetw_selected_diff(
		github_application& aApplication)
		-> detail::uiforetw_selected_diff_probe_result
	{
		using result     = detail::uiforetw_selected_diff_probe_result;
		using state_type = github_application::implementation::
			uiforetw_selected_diff_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->Host == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->Window == nullptr
			|| implementation->AppearanceProfile == nullptr) {
			return result::failed;
		}
		if (!implementation->UIforEtwSelectedDiffProbe.has_value()) {
			implementation->UIforEtwSelectedDiffProbe.emplace();
		}
		auto&      probe = *implementation->UIforEtwSelectedDiffProbe;
		const auto fail  = [&probe, implementation] {
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		auto* const window             = implementation->Window.get();
		auto* const workbench          = window->comparison_surface();
		auto* const view               = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		const auto  fixture            = implementation->Fixture->evidence();
		probe.Evidence.ExactCommitPair = fixture.BaseOid == uiforetw_base_oid
			&& fixture.HeadOneOid == uiforetw_head_oid
			&& fixture.HeadTwoOid.empty();

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& snapshot = implementation->Host->accepted_snapshot();
			const auto  resource = drawer_row(*window, uiforetw_resource_path);
			const auto  rc       = drawer_row(*window, uiforetw_rc_path);
			if (!snapshot.has_value()
				|| implementation->WorkbenchHost->outstanding_requests() != 0U
				|| !implementation->WorkbenchHost->state().Navigation.has_value()
				|| !resource.isValid() || !rc.isValid() || view == nullptr) {
				return result::pending;
			}
			if (!probe.Evidence.ExactCommitPair) {
				return fail();
			}
			probe.WindowStatistics = window->statistics();
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			window->resize(1'280, 1'000);
			QApplication::processEvents();
			view->setCurrentIndex(resource);
			probe.Phase = state_type::phase::awaiting_resource_diff;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_resource_diff) {
			probe.Evidence.ResourceGesture
				= window->statistics().FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				if (++probe.Waits > 512U && probe.Evidence.ResourceGesture) {
					return fail();
				}
				return result::pending;
			}
			const auto resourcePublication
				= implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications + 1U;
			if (!resourcePublication
				|| !selected_diff_publication_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				return fail();
			}
			if (!probe.Evidence.ResourcePublication) {
				probe.Evidence.ResourcePublication = true;
				probe.Waits                        = 0U;
			}
			if (!selected_diff_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				if (++probe.Waits > 512U) {
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.ResourceExactBlobs    = true;
			probe.Waits                          = 0U;
			const auto& computed                 = *workbench->current_result();
			probe.Evidence.ResourceIntralinePair = find_intraline_pair(
													   computed,
													   "_APS_NEXT_RESOURCE_VALUE        136",
													   "_APS_NEXT_RESOURCE_VALUE        137")
													   .has_value();
			if (!probe.Evidence.ResourceGesture
				|| !probe.Evidence.ResourcePublication
				|| !probe.Evidence.ResourceExactBlobs
				|| !probe.Evidence.ResourceIntralinePair
				|| !reveal_intraline_pair(
					*workbench,
					computed,
					"_APS_NEXT_RESOURCE_VALUE        136",
					"_APS_NEXT_RESOURCE_VALUE        137")) {
				return fail();
			}
			probe.Evidence.ResourceRolePaint = intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 1U);
			if (!probe.Evidence.ResourceRolePaint) {
				if (++probe.Waits > 64U) {
					return fail();
				}
				return result::pending;
			}
			auto* const nextFile = window->next_file_action();
			if (nextFile == nullptr || !nextFile->isEnabled()) {
				return fail();
			}
			probe.IntentDispatches = implementation->WorkbenchIntentDispatches;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			nextFile->trigger();
			probe.Phase = state_type::phase::awaiting_support_diff;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_support_diff) {
			if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				if (++probe.Waits > 512U) {
					return fail();
				}
				return result::pending;
			}
			const auto exactPublication
				= implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications + 1U
				&& implementation->LastSelectedDiff.has_value()
				&& implementation->LastSelectedDiff->Request.File.PresentationPath
					== uiforetw_support_path;
			if (!exactPublication) {
				return fail();
			}
			const auto exactSupport = workbench->current_result() != nullptr
				&& workbench->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key;
			if (!exactSupport) {
				if (++probe.Waits > 512U) {
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.NextFileNavigation
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			auto* const previousFile = window->previous_file_action();
			if (!probe.Evidence.NextFileNavigation || previousFile == nullptr
				|| !previousFile->isEnabled()) {
				return fail();
			}
			probe.IntentDispatches = implementation->WorkbenchIntentDispatches;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			previousFile->trigger();
			probe.Phase = state_type::phase::awaiting_resource_return;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_resource_return) {
			if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				if (++probe.Waits > 512U) {
					return fail();
				}
				return result::pending;
			}
			if (implementation->SelectedDiffPublications
					!= probe.SelectedDiffPublications + 1U
				|| !implementation->LastSelectedDiff.has_value()
				|| implementation->LastSelectedDiff->Request.File.PresentationPath
					!= uiforetw_resource_path) {
				return fail();
			}
			if (!selected_diff_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				if (++probe.Waits > 512U) {
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.PreviousFileNavigation
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			const auto rc = drawer_row(*window, uiforetw_rc_path);
			if (!probe.Evidence.PreviousFileNavigation || !rc.isValid()
				|| view == nullptr) {
				return fail();
			}
			probe.WindowStatistics = window->statistics();
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			view->setCurrentIndex(rc);
			probe.Phase = state_type::phase::awaiting_rc_diff;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_rc_diff) {
			probe.Evidence.RcGesture = window->statistics().FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				if (++probe.Waits > 512U && probe.Evidence.RcGesture) {
					return fail();
				}
				return result::pending;
			}
			const auto rcPublication = implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications + 1U;
			if (!rcPublication
				|| !selected_diff_publication_matches(
					*implementation,
					uiforetw_rc_path,
					uiforetw_rc_base_blob,
					uiforetw_rc_head_blob)) {
				return fail();
			}
			if (!probe.Evidence.RcPublication) {
				probe.Evidence.RcPublication = true;
				probe.Waits                  = 0U;
			}
			if (!selected_diff_matches(
					*implementation,
					uiforetw_rc_path,
					uiforetw_rc_base_blob,
					uiforetw_rc_head_blob)) {
				if (++probe.Waits > 512U) {
					return fail();
				}
				return result::pending;
			}
			if (!probe.Evidence.RcExactBlobs) {
				probe.Evidence.RcExactBlobs = true;
				probe.Waits                 = 0U;
			}
			const auto& computed = *workbench->current_result();
			const auto  pairOne  = find_intraline_pair(
				computed, "WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
			const auto pairTwo = find_intraline_pair(
				computed, "WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
			probe.Evidence.RcIntralinePairs
				= pairOne.has_value() && pairTwo.has_value();
			if (!probe.Evidence.RcIntralinePairs || !pairOne->Group.has_value()) {
				return fail();
			}
			if (!probe.Evidence.NextChangeNavigation) {
				auto* const nextChange     = window->next_change_action();
				auto* const previousChange = window->previous_change_action();
				const auto  before         = workbench->selection();
				if (nextChange == nullptr || previousChange == nullptr
					|| !nextChange->isEnabled() || !before.has_value()) {
					return fail();
				}
				probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
				nextChange->trigger();
				probe.Evidence.NextChangeNavigation
					= implementation->WorkbenchChangeDispatches
						== probe.ChangeDispatches + 1U
					&& workbench->selection().has_value()
					&& workbench->selection()->Group != before->Group
					&& previousChange->isEnabled();
				if (!probe.Evidence.NextChangeNavigation) {
					return fail();
				}
				previousChange->trigger();
				probe.Evidence.PreviousChangeNavigation
					= implementation->WorkbenchChangeDispatches
						== probe.ChangeDispatches + 2U
					&& workbench->selection() == before;
				if (!probe.Evidence.PreviousChangeNavigation) {
					return fail();
				}
			}
			if (!workbench->selection().has_value()
				|| workbench->selection()->Group != pairOne->Group) {
				if (probe.Waits++ > computed.Alignment.Groups.size()) {
					return fail();
				}
				if (!workbench->navigate_change(
						core::change_navigation_direction::next,
						core::change_navigation_policy::stop)) {
					return fail();
				}
				return result::pending;
			}
			probe.Waits = 0U;
			if (!probe.Evidence.RcGesture || !probe.Evidence.RcPublication
				|| !probe.Evidence.RcExactBlobs
				|| !probe.Evidence.RcIntralinePairs
				|| !reveal_intraline_pair(
					*workbench,
					computed,
					"WS_TABSTOP,464,28,67,10",
					"WS_TABSTOP,464,24,67,10")) {
				return fail();
			}
			probe.Evidence.RcRolePaint = intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 2U);
			if (!probe.Evidence.RcRolePaint) {
				if (++probe.Waits > 64U) {
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			const auto semantic = std::array{
				probe.Evidence.ExactCommitPair,
				probe.Evidence.ResourceGesture,
				probe.Evidence.ResourceExactBlobs,
				probe.Evidence.ResourceIntralinePair,
				probe.Evidence.ResourceRolePaint,
				probe.Evidence.ResourcePublication,
				probe.Evidence.RcGesture,
				probe.Evidence.RcExactBlobs,
				probe.Evidence.RcIntralinePairs,
				probe.Evidence.RcRolePaint,
				probe.Evidence.RcPublication,
				probe.Evidence.PreviousFileNavigation,
				probe.Evidence.NextFileNavigation,
				probe.Evidence.PreviousChangeNavigation,
				probe.Evidence.NextChangeNavigation,
				probe.Evidence.FixtureDiffHelperUnused,
			};
			if (!std::ranges::all_of(semantic, std::identity{ })) {
				return fail();
			}
			auto* const files = window->files_drawer_toggle_action();
			auto* const conversations
				= window->conversations_drawer_toggle_action();
			auto* const details = window->review_details_toggle_action();
			if (files == nullptr || conversations == nullptr || details == nullptr
				|| !files->isEnabled() || !conversations->isEnabled()) {
				return fail();
			}
			if (files->isChecked()) {
				const auto dispatches = implementation->WorkbenchIntentDispatches;
				files->trigger();
				probe.Evidence.FilesDrawerToggle
					= implementation->WorkbenchIntentDispatches == dispatches + 1U
					&& !files->isChecked();
			}
			if (conversations->isChecked()) {
				const auto dispatches = implementation->WorkbenchIntentDispatches;
				conversations->trigger();
				probe.Evidence.ConversationsDrawerToggle
					= implementation->WorkbenchIntentDispatches == dispatches + 1U
					&& !conversations->isChecked();
			}
			if (details->isChecked()) {
				details->trigger();
			}
			if (!probe.Evidence.FilesDrawerToggle
				|| !probe.Evidence.ConversationsDrawerToggle) {
				return fail();
			}
			probe.Waits = 0U;
			probe.Phase = state_type::phase::awaiting_focused_layout;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_focused_layout) {
			auto* const header = window->findChild<QScrollArea*>(
				QStringLiteral("workbenchReviewHeaderScroll"));
			auto* const detail  = workbench->detail_pane();
			const auto  focused = !window->files_drawer()->isVisible()
				&& !window->conversations_drawer()->isVisible()
				&& !window->files_drawer_toggle_action()->isChecked()
				&& !window->conversations_drawer_toggle_action()->isChecked()
				&& !window->review_details_toggle_action()->isChecked()
				&& header != nullptr && !header->isVisible()
				&& !window->epoch_selectors()->isVisible()
				&& detail != nullptr && !detail->isVisible();
			if (!focused) {
				if (++probe.Waits > 64U) {
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.FocusedReviewLayout = true;
			probe.Phase                        = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::uiforetw_selected_diff_evidence(
		const github_application& aApplication) noexcept
		-> detail::uiforetw_selected_diff_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->UIforEtwSelectedDiffProbe.has_value()
			? implementation->UIforEtwSelectedDiffProbe->Evidence
			: detail::uiforetw_selected_diff_probe_evidence{ };
	}

	auto detail::github_application_fixture_access::
		capture_uiforetw_selected_diff_evidence(
			github_application&         aApplication,
			const std::string_view      aDirectory,
			const core::appearance_mode aMode) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr
			|| implementation->AppearanceProfile == nullptr
			|| !implementation->UIforEtwSelectedDiffProbe.has_value()
			|| implementation->UIforEtwSelectedDiffProbe->Phase
				!= github_application::implementation::
					uiforetw_selected_diff_probe_state::phase::passed
			|| implementation->AppearanceHost->settings().Mode != aMode
			|| aDirectory.empty()) {
			return false;
		}
		auto* const       window     = implementation->Window.get();
		auto* const       workbench  = window->comparison_surface();
		const auto* const result     = workbench != nullptr
			? workbench->current_result().get()
			: nullptr;
		const auto        firstPair  = result != nullptr
			? find_intraline_pair(
				  *result,
				  "WS_TABSTOP,464,28,67,10",
				  "WS_TABSTOP,464,24,67,10")
			: std::optional<intraline_pair_location>{ };
		const auto        secondPair = result != nullptr
			? find_intraline_pair(
				  *result,
				  "WS_TABSTOP,464,42,98,10",
				  "WS_TABSTOP,464,36,98,10")
			: std::optional<intraline_pair_location>{ };
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !firstPair.has_value() || !secondPair.has_value()
			|| !firstPair->Group.has_value() || !secondPair->Group.has_value()
			|| !selected_diff_matches(
				*implementation,
				uiforetw_rc_path,
				uiforetw_rc_base_blob,
				uiforetw_rc_head_blob)) {
			return false;
		}
		const auto drain = [] {
			for (auto tick = 0; tick < 4; ++tick) {
				QApplication::processEvents();
			}
		};
		const auto selected = [workbench](const intraline_pair_location& aPair) {
			return aPair.Group.has_value()
				&& workbench->selection().has_value()
				&& workbench->selection()->Segment == aPair.Segment
				&& workbench->selection()->Group == aPair.Group
				&& workbench->link_map()->selected()
				== qt::diff_link_map_identity{
					   .Segment = aPair.Segment,
					   .Group   = aPair.Group,
				   };
		};
		const auto observeTarget = [workbench, result, implementation](
									   const std::string_view aLeft,
									   const std::string_view aRight) {
			return observe_intraline_render(
				*workbench,
				*result,
				*implementation->AppearanceProfile,
				aLeft,
				aRight);
		};
		const auto connectorValid = [](const connector_render_observation& aValue) {
			return aValue.LeftEndpoint && aValue.RightEndpoint
				&& aValue.LeftOverlap && aValue.RightOverlap
				&& aValue.Pixels.Background.isValid()
				&& aValue.Pixels.Dominant.isValid()
				&& aValue.Pixels.SampledPixels >= 24U
				&& aValue.Pixels.ContrastPixels >= 24U
				&& aValue.Pixels.DominantPixels >= 24U;
		};
		const auto selectedReadable = [&connectorValid](
										  const connector_render_observation& aValue) {
			return aValue.Selected && connectorValid(aValue);
		};
		const auto unselectedReadable = [&connectorValid](
											const connector_render_observation& aValue) {
			return !aValue.Selected && connectorValid(aValue)
				&& aValue.Pixels.ContrastPixels * 2U
				>= aValue.Pixels.SampledPixels
				&& aValue.Pixels.MedianContrastHundredths >= 300;
		};
		const auto statesDistinct = [](const connector_render_observation& aSelected,
									   const connector_render_observation& aUnselected) {
			return aSelected.Pixels.Background == aUnselected.Pixels.Background
				&& aSelected.Pixels.Dominant != aUnselected.Pixels.Dominant;
		};

		if (!navigate_to_group(*workbench, *result, *firstPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto firstSelected = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto secondUnselected = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstSelectedConnector    = firstSelected.has_value()
			? observe_connector_render(*workbench, *firstSelected)
			: std::optional<connector_render_observation>{ };
		const auto secondUnselectedConnector = secondUnselected.has_value()
			? observe_connector_render(*workbench, *secondUnselected)
			: std::optional<connector_render_observation>{ };
		if (!firstSelected.has_value() || !selected(*firstPair)
			|| firstSelected->LeftRolePixels == 0U
			|| firstSelected->RightRolePixels == 0U
			|| !secondUnselected.has_value()
			|| !firstSelectedConnector.has_value()
			|| !secondUnselectedConnector.has_value()
			|| !selectedReadable(*firstSelectedConnector)
			|| !unselectedReadable(*secondUnselectedConnector)) {
			QTextStream{ stderr } << "UIFORETW_CAPTURE_FIRST_TARGET_FAILED\n";
			return false;
		}

		if (!navigate_to_group(*workbench, *result, *secondPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto secondSelected = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstUnselected = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto secondSelectedConnector  = secondSelected.has_value()
			? observe_connector_render(*workbench, *secondSelected)
			: std::optional<connector_render_observation>{ };
		const auto firstUnselectedConnector = firstUnselected.has_value()
			? observe_connector_render(*workbench, *firstUnselected)
			: std::optional<connector_render_observation>{ };
		if (!secondSelected.has_value() || !selected(*secondPair)
			|| secondSelected->LeftRolePixels == 0U
			|| secondSelected->RightRolePixels == 0U
			|| !firstUnselected.has_value()
			|| !secondSelectedConnector.has_value()
			|| !firstUnselectedConnector.has_value()
			|| !selectedReadable(*secondSelectedConnector)
			|| !unselectedReadable(*firstUnselectedConnector)
			|| !statesDistinct(
				*firstSelectedConnector, *firstUnselectedConnector)
			|| !statesDistinct(
				*secondSelectedConnector, *secondUnselectedConnector)) {
			QTextStream{ stderr } << "UIFORETW_CAPTURE_SECOND_TARGET_FAILED\n";
			return false;
		}

		if (!navigate_to_group(*workbench, *result, *firstPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto first = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto second = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstConnector  = first.has_value()
			? observe_connector_render(*workbench, *first)
			: std::optional<connector_render_observation>{ };
		const auto secondConnector = second.has_value()
			? observe_connector_render(*workbench, *second)
			: std::optional<connector_render_observation>{ };
		if (!intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 2U)
			|| !first.has_value() || !second.has_value()
			|| first->LeftRolePixels == 0U || first->RightRolePixels == 0U
			|| second->LeftRolePixels == 0U || second->RightRolePixels == 0U
			|| !firstConnector.has_value() || !secondConnector.has_value()
			|| !selectedReadable(*firstConnector)
			|| !unselectedReadable(*secondConnector) || !selected(*firstPair)) {
			QTextStream{ stderr } << "UIFORETW_CAPTURE_FINAL_TARGET_FAILED\n";
			return false;
		}

		auto* const header = window->findChild<QScrollArea*>(
			QStringLiteral("workbenchReviewHeaderScroll"));
		auto* const detail            = workbench->detail_pane();
		auto* const comparisonSurface = workbench->comparison_splitter();
		auto* const linkMap           = workbench->link_map();
		auto* const toolbar           = window->primary_toolbar();
		auto* const filesAction       = window->files_drawer_toggle_action();
		auto* const previousFile      = window->previous_file_action();
		auto* const nextFile          = window->next_file_action();
		auto* const previousChange    = window->previous_change_action();
		auto* const nextChange        = window->next_change_action();
		auto* const conversationsAction
			= window->conversations_drawer_toggle_action();
		auto* const reviewDetails  = window->review_details_toggle_action();
		auto* const reviewControls = window->review_controls_button();
		if (header == nullptr || header->isVisible()
			|| window->epoch_selectors()->isVisible()
			|| detail == nullptr || detail->isVisible()
			|| window->files_drawer()->isVisible()
			|| window->conversations_drawer()->isVisible()
			|| comparisonSurface == nullptr || linkMap == nullptr
			|| toolbar == nullptr || filesAction == nullptr
			|| previousFile == nullptr || nextFile == nullptr
			|| previousChange == nullptr || nextChange == nullptr
			|| conversationsAction == nullptr || reviewDetails == nullptr
			|| reviewControls == nullptr) {
			return false;
		}
		const auto requiredActions = std::array{
			filesAction,
			previousFile,
			nextFile,
			previousChange,
			nextChange,
			conversationsAction,
		};
		auto requiredPresenters = std::array<QToolButton*, 6U>{ };
		for (std::size_t index = 0U; index < requiredActions.size(); ++index) {
			requiredPresenters[index] = qobject_cast<QToolButton*>(
				toolbar->widgetForAction(requiredActions[index]));
			if (requiredPresenters[index] == nullptr) {
				return false;
			}
		}
		const auto navigationIconObservations = std::array{
			observe_navigation_icon_contrast(*requiredPresenters[1]),
			observe_navigation_icon_contrast(*requiredPresenters[2]),
			observe_navigation_icon_contrast(*requiredPresenters[3]),
			observe_navigation_icon_contrast(*requiredPresenters[4]),
		};
		const auto normalIconContrastFloor
			= aMode == core::appearance_mode::high_contrast_light
				|| aMode == core::appearance_mode::high_contrast_dark
			? std::int64_t{ 700 }
			: std::int64_t{ 450 };
		const auto navigationIconsPaletteAware = std::ranges::all_of(
			navigationIconObservations,
			[normalIconContrastFloor](
				const navigation_icon_contrast_observation& aObservation) {
				return aObservation.NormalForegroundPixels > 0U
					&& aObservation.DisabledForegroundPixels > 0U
					&& aObservation.NormalContrastHundredths
					>= normalIconContrastFloor
					&& aObservation.DisabledContrastHundredths >= 300
					&& aObservation.StatesDistinct;
			});
		auto* const spacer = toolbar->findChild<QWidget*>(
			QStringLiteral("workbenchNavigationSpacer"), Qt::FindDirectChildrenOnly);
		const auto toolbarActions       = toolbar->actions();
		const auto reviewControlsAction = std::ranges::find_if(
			toolbarActions,
			[toolbar, reviewControls](QAction* const aAction) {
				return toolbar->widgetForAction(aAction) == reviewControls;
			});
		const auto spacerAction = std::ranges::find_if(
			toolbarActions,
			[toolbar, spacer](QAction* const aAction) {
				return toolbar->widgetForAction(aAction) == spacer;
			});
		const auto exactRowOrder = toolbarActions.size() == 10U
			&& toolbarActions[0] == filesAction
			&& toolbarActions[1] == previousFile
			&& toolbarActions[2] == nextFile
			&& toolbarActions[3] == previousChange
			&& toolbarActions[4] == nextChange
			&& toolbarActions[5]->isSeparator()
			&& toolbarActions[6] == reviewDetails
			&& reviewControlsAction == toolbarActions.begin() + 7
			&& spacerAction == toolbarActions.begin() + 8
			&& toolbarActions[9] == conversationsAction;
		auto iconOnly    = toolbar->toolButtonStyle() == Qt::ToolButtonIconOnly;
		auto nativeIcons = true;
		auto accessible  = true;
		auto uniqueNames = true;
		for (std::size_t index = 0U; index < requiredActions.size(); ++index) {
			const auto* const action = requiredActions[index];
			const auto* const button = requiredPresenters[index];
			iconOnly                 = iconOnly
				&& button->toolButtonStyle() == Qt::ToolButtonIconOnly;
			nativeIcons = nativeIcons && !action->icon().isNull()
				&& !button->icon().isNull();
			accessible = accessible && !button->accessibleName().isEmpty()
				&& !button->accessibleDescription().isEmpty()
				&& !action->toolTip().isEmpty() && !action->shortcut().isEmpty()
				&& button->focusPolicy() == Qt::StrongFocus;
			for (std::size_t other = index + 1U;
				 other < requiredActions.size(); ++other) {
				uniqueNames = uniqueNames
					&& requiredPresenters[index]->accessibleName()
						!= requiredPresenters[other]->accessibleName()
					&& requiredActions[index]->objectName()
						!= requiredActions[other]->objectName();
			}
		}
		const auto inWindow = [window](const QWidget& aWidget) {
			return QRect{
				aWidget.mapTo(window, QPoint{ 0, 0 }),
				aWidget.size(),
			};
		};
		const auto comparisonRect = inWindow(*comparisonSurface);
		const auto leftPaneRect   = inWindow(*workbench->left_pane()->viewport());
		const auto rightPaneRect  = inWindow(*workbench->right_pane()->viewport());
		const auto gutterRect     = inWindow(*linkMap);
		const auto widthRatio     = (1'000 * comparisonRect.width())
			/ std::max(1, window->width());
		const auto heightRatio = (1'000 * comparisonRect.height())
			/ std::max(1, window->height());
		if (widthRatio < 750 || heightRatio < 850) {
			QTextStream{ stderr }
				<< "UIFORETW_CAPTURE_LAYOUT_FAILED:" << widthRatio << ':'
				<< heightRatio << '\n';
			return false;
		}

		const auto path = QString::fromUtf8(
			aDirectory.data(), static_cast<qsizetype>(aDirectory.size()));
		if (!QDir{ }.mkpath(path)) {
			return false;
		}
		const auto directory  = QDir{ path };
		const auto full       = window->grab();
		const auto comparison = comparisonSurface->grab();
		if (full.isNull() || comparison.isNull()
			|| !full.save(directory.filePath(QStringLiteral("full-window.png")), "PNG")
			|| !comparison.save(
				directory.filePath(QStringLiteral("intraline-comparison.png")),
				"PNG")) {
			return false;
		}

		const auto removed = implementation->AppearanceProfile->color(
			core::appearance_role::diff_intraline_removed_background);
		const auto added = implementation->AppearanceProfile->color(
			core::appearance_role::diff_intraline_added_background);
		const auto& key      = implementation->LastSelectedDiff->Key;
		const auto  blobText = [](const core::git_blob_oid& aOid) {
			const auto text = core::canonical_hex(aOid).view();
			return QString::fromUtf8(
				text.data(), static_cast<qsizetype>(text.size()));
		};
		const auto identityText = []<typename Identity>(const Identity& aIdentity) {
			const auto& bytes = aIdentity.bytes();
			return QString::fromLatin1(
				QByteArray{
					reinterpret_cast<const char*>(bytes.data()),
					static_cast<qsizetype>(bytes.size()) }
					.toHex());
		};
		const auto connectorJson = [](const connector_render_observation& aValue) {
			return QJsonObject{
				{ QStringLiteral("state"),
				  aValue.Selected ? QStringLiteral("selected")
								  : QStringLiteral("unselected") },
				{ QStringLiteral("gutter_background"),
				  aValue.Pixels.Background.name(QColor::HexArgb) },
				{ QStringLiteral("dominant_path_pixel"),
				  aValue.Pixels.Dominant.name(QColor::HexArgb) },
				{ QStringLiteral("sampled_path_pixels"),
				  static_cast<qint64>(aValue.Pixels.SampledPixels) },
				{ QStringLiteral("contrast_pixels"),
				  static_cast<qint64>(aValue.Pixels.ContrastPixels) },
				{ QStringLiteral("dominant_path_pixels"),
				  static_cast<qint64>(aValue.Pixels.DominantPixels) },
				{ QStringLiteral("contrast_floor_hundredths"), 300 },
				{ QStringLiteral("median_contrast_hundredths"),
				  static_cast<qint64>(aValue.Pixels.MedianContrastHundredths) },
				{ QStringLiteral("bounds"), rectangle_json(aValue.Bounds) },
				{ QStringLiteral("start"), point_json(aValue.Start) },
				{ QStringLiteral("end"), point_json(aValue.End) },
				{ QStringLiteral("left_endpoint_at_pane_edge"), aValue.LeftEndpoint },
				{ QStringLiteral("right_endpoint_at_pane_edge"), aValue.RightEndpoint },
				{ QStringLiteral("left_target_row_overlap"), aValue.LeftOverlap },
				{ QStringLiteral("right_target_row_overlap"), aValue.RightOverlap },
			};
		};
		const auto firstGroup = std::ranges::find(
			result->Alignment.Groups, *firstPair->Group, &core::change_group::Id);
		const auto secondGroup = std::ranges::find(
			result->Alignment.Groups, *secondPair->Group, &core::change_group::Id);
		if (firstGroup == result->Alignment.Groups.end()
			|| secondGroup == result->Alignment.Groups.end()) {
			return false;
		}
		const auto targetJson = [&identityText, &connectorJson](
									const intraline_render_observation& aTarget,
									const intraline_render_observation& aSelectedTarget,
									const connector_render_observation& aConnector,
									const connector_render_observation& aSelectedConnector,
									const connector_render_observation& aUnselectedConnector,
									const qint64                        aChangeIndex) {
			return QJsonObject{
				{ QStringLiteral("segment"), identityText(aTarget.Pair.Segment) },
				{ QStringLiteral("group"),
				  aTarget.Pair.Group.has_value()
					  ? identityText(*aTarget.Pair.Group)
					  : QString{ } },
				{ QStringLiteral("left_line"), static_cast<qint64>(aTarget.Pair.Left) },
				{ QStringLiteral("right_line"), static_cast<qint64>(aTarget.Pair.Right) },
				{ QStringLiteral("left_rect_in_pane"), rectangle_json(aTarget.LeftRect) },
				{ QStringLiteral("right_rect_in_pane"), rectangle_json(aTarget.RightRect) },
				{ QStringLiteral("left_role_pixels"), static_cast<qint64>(aTarget.LeftRolePixels) },
				{ QStringLiteral("right_role_pixels"), static_cast<qint64>(aTarget.RightRolePixels) },
				{ QStringLiteral("selected_observed"), true },
				{ QStringLiteral("selected_left_role_pixels"), static_cast<qint64>(aSelectedTarget.LeftRolePixels) },
				{ QStringLiteral("selected_right_role_pixels"), static_cast<qint64>(aSelectedTarget.RightRolePixels) },
				{ QStringLiteral("change_index"), aChangeIndex },
				{ QStringLiteral("connector"), connectorJson(aConnector) },
				{ QStringLiteral("selected_connector"),
				  connectorJson(aSelectedConnector) },
				{ QStringLiteral("unselected_connector"),
				  connectorJson(aUnselectedConnector) },
			};
		};
		const auto firstChangeIndex = static_cast<qint64>(
			std::distance(result->Alignment.Groups.begin(), firstGroup));
		const auto secondChangeIndex = static_cast<qint64>(
			std::distance(result->Alignment.Groups.begin(), secondGroup));
		const auto iconContrastJson = [](
										  const navigation_icon_contrast_observation& aValue) {
			return QJsonObject{
				{ QStringLiteral("background"),
				  aValue.Background.name(QColor::HexArgb) },
				{ QStringLiteral("normal_foreground"),
				  aValue.NormalForeground.name(QColor::HexArgb) },
				{ QStringLiteral("disabled_foreground"),
				  aValue.DisabledForeground.name(QColor::HexArgb) },
				{ QStringLiteral("normal_foreground_pixels"),
				  static_cast<qint64>(aValue.NormalForegroundPixels) },
				{ QStringLiteral("disabled_foreground_pixels"),
				  static_cast<qint64>(aValue.DisabledForegroundPixels) },
				{ QStringLiteral("normal_contrast_hundredths"),
				  static_cast<qint64>(aValue.NormalContrastHundredths) },
				{ QStringLiteral("disabled_contrast_hundredths"),
				  static_cast<qint64>(aValue.DisabledContrastHundredths) },
				{ QStringLiteral("states_distinct"), aValue.StatesDistinct },
			};
		};
		auto facts = QJsonObject{
			{ QStringLiteral("schema"), QStringLiteral("bpr.uiforetw_intraline_capture.v4") },
			{ QStringLiteral("appearance"), appearance_mode_name(aMode) },
			{ QStringLiteral("qt_platform"), QGuiApplication::platformName() },
			{ QStringLiteral("qt_version"), QString::fromLatin1(qVersion()) },
			{ QStringLiteral("style"), QString::fromLatin1(window->style()->metaObject()->className()) },
			{ QStringLiteral("device_pixel_ratio"), full.devicePixelRatio() },
			{ QStringLiteral("logical_width"), window->width() },
			{ QStringLiteral("logical_height"), window->height() },
			{ QStringLiteral("pixel_width"), full.width() },
			{ QStringLiteral("pixel_height"), full.height() },
			{ QStringLiteral("comparison_pixel_width"), comparison.width() },
			{ QStringLiteral("comparison_pixel_height"), comparison.height() },
			{ QStringLiteral("selected_path"), QString::fromUtf8(uiforetw_rc_path.data(), static_cast<qsizetype>(uiforetw_rc_path.size())) },
			{ QStringLiteral("base_commit"), QString::fromUtf8(uiforetw_base_oid.data(), static_cast<qsizetype>(uiforetw_base_oid.size())) },
			{ QStringLiteral("head_commit"), QString::fromUtf8(uiforetw_head_oid.data(), static_cast<qsizetype>(uiforetw_head_oid.size())) },
			{ QStringLiteral("base_blob"), blobText(*key.LeftBlob) },
			{ QStringLiteral("head_blob"), blobText(*key.RightBlob) },
			{ QStringLiteral("removed_role"), removed.name(QColor::HexArgb) },
			{ QStringLiteral("added_role"), added.name(QColor::HexArgb) },
			{ QStringLiteral("intraline_edit_count"), static_cast<qint64>(workbench->current_result()->Intraline.size()) },
			{ QStringLiteral("fixture_diff_helper_invocations"), static_cast<qint64>(implementation->FixtureDiffHelperInvocations) },
			{ QStringLiteral("previous_file_navigation_exercised"), implementation->UIforEtwSelectedDiffProbe->Evidence.PreviousFileNavigation },
			{ QStringLiteral("next_file_navigation_exercised"), implementation->UIforEtwSelectedDiffProbe->Evidence.NextFileNavigation },
			{ QStringLiteral("previous_change_navigation_exercised"), implementation->UIforEtwSelectedDiffProbe->Evidence.PreviousChangeNavigation },
			{ QStringLiteral("next_change_navigation_exercised"), implementation->UIforEtwSelectedDiffProbe->Evidence.NextChangeNavigation },
			{ QStringLiteral("files_drawer_toggle_exercised"), implementation->UIforEtwSelectedDiffProbe->Evidence.FilesDrawerToggle },
			{ QStringLiteral("conversations_drawer_toggle_exercised"), implementation->UIforEtwSelectedDiffProbe->Evidence.ConversationsDrawerToggle },
			{ QStringLiteral("navigation_row_exact_order"), exactRowOrder },
			{ QStringLiteral("navigation_actions_icon_only"), iconOnly },
			{ QStringLiteral("navigation_actions_have_native_icons"), nativeIcons },
			{ QStringLiteral("navigation_icons_palette_aware"), navigationIconsPaletteAware },
			{ QStringLiteral("navigation_icon_contrast"),
			  QJsonObject{
				  { QStringLiteral("previous_file"),
					iconContrastJson(navigationIconObservations[0]) },
				  { QStringLiteral("next_file"),
					iconContrastJson(navigationIconObservations[1]) },
				  { QStringLiteral("previous_change"),
					iconContrastJson(navigationIconObservations[2]) },
				  { QStringLiteral("next_change"),
					iconContrastJson(navigationIconObservations[3]) },
			  } },
			{ QStringLiteral("navigation_actions_accessible"), accessible },
			{ QStringLiteral("navigation_accessible_names_unique"), uniqueNames },
			{ QStringLiteral("files_drawer_visible"), false },
			{ QStringLiteral("conversations_drawer_visible"), false },
			{ QStringLiteral("review_details_visible"), false },
			{ QStringLiteral("epoch_controls_visible"), false },
			{ QStringLiteral("selected_change_detail_visible"), false },
			{ QStringLiteral("full_window_rect"), rectangle_json(window->rect()) },
			{ QStringLiteral("navigation_row_rect_in_window"), rectangle_json(inWindow(*toolbar)) },
			{ QStringLiteral("files_toggle_rect_in_row"), rectangle_json(requiredPresenters[0]->geometry()) },
			{ QStringLiteral("previous_file_rect_in_row"), rectangle_json(requiredPresenters[1]->geometry()) },
			{ QStringLiteral("next_file_rect_in_row"), rectangle_json(requiredPresenters[2]->geometry()) },
			{ QStringLiteral("previous_change_rect_in_row"), rectangle_json(requiredPresenters[3]->geometry()) },
			{ QStringLiteral("next_change_rect_in_row"), rectangle_json(requiredPresenters[4]->geometry()) },
			{ QStringLiteral("review_details_rect_in_row"), rectangle_json(toolbar->actionGeometry(reviewDetails)) },
			{ QStringLiteral("private_progress_rect_in_row"), rectangle_json(reviewControls->geometry()) },
			{ QStringLiteral("conversations_toggle_rect_in_row"), rectangle_json(requiredPresenters[5]->geometry()) },
			{ QStringLiteral("comparison_rect_in_window"), rectangle_json(comparisonRect) },
			{ QStringLiteral("left_pane_rect_in_window"), rectangle_json(leftPaneRect) },
			{ QStringLiteral("right_pane_rect_in_window"), rectangle_json(rightPaneRect) },
			{ QStringLiteral("gutter_rect_in_window"), rectangle_json(gutterRect) },
			{ QStringLiteral("files_drawer_rect_in_splitter"), rectangle_json(window->files_drawer()->geometry()) },
			{ QStringLiteral("conversations_drawer_rect_in_splitter"), rectangle_json(window->conversations_drawer()->geometry()) },
			{ QStringLiteral("review_details_rect_in_window"), rectangle_json(inWindow(*header)) },
			{ QStringLiteral("comparison_width_ratio_permille"), widthRatio },
			{ QStringLiteral("comparison_height_ratio_permille"), heightRatio },
			{ QStringLiteral("left_horizontal_offset"), workbench->left_pane()->horizontalScrollBar()->value() },
			{ QStringLiteral("right_horizontal_offset"), workbench->right_pane()->horizontalScrollBar()->value() },
			{ QStringLiteral("selected_change_index"), firstChangeIndex },
			{ QStringLiteral("selected_segment"), identityText(first->Pair.Segment) },
			{ QStringLiteral("selected_group"), identityText(*first->Pair.Group) },
			{ QStringLiteral("replacement_28_to_24"), targetJson(*first, *firstSelected, *firstConnector, *firstSelectedConnector, *firstUnselectedConnector, firstChangeIndex) },
			{ QStringLiteral("replacement_42_to_36"), targetJson(*second, *secondSelected, *secondConnector, *secondSelectedConnector, *secondUnselectedConnector, secondChangeIndex) },
		};
		auto file = QFile{ directory.filePath(QStringLiteral("capture-facts.json")) };
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		const auto bytes = QJsonDocument{ facts }.toJson(QJsonDocument::Indented);
		return file.write(bytes) == bytes.size() && file.flush();
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

		[[nodiscard]] constexpr auto review_ledger_probe_thread_identity() noexcept
			-> std::string_view
		{
			return "review-thread:PRRT_fixture_1";
		}

		[[nodiscard]] auto review_ledger_probe_thread_matches_file(
			const core::file_identity                       aFile,
			const application::github_review_thread_record& aThread) -> bool
		{
			const auto acceptedIdentity
				= "review-thread:" + std::string{ aThread.Thread.value() };
			return acceptedIdentity == review_ledger_probe_thread_identity()
				&& aThread.Subject
				== application::github_review_thread_subject::line
				&& aThread.Line.has_value() && *aThread.Line != 0U
				&& !aThread.Path.empty()
				&& core::file_identity_of(aThread.Path) == aFile;
		}

		[[nodiscard]] auto review_ledger_probe_defect(
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread)
			-> std::optional<application::durable_defect_record>
		{
			if (!review_ledger_probe_thread_matches_file(aFile, aThread)) {
				return std::nullopt;
			}
			const auto identity = defect_identity(
				review_ledger_probe_thread_identity());
			const auto anchor = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = core::diff_side::right,
				.Line       = *aThread.Line,
			};
			return application::durable_defect_record{
				.File         = aFile,
				.Conversation = core::review_conversation_id{ identity.Conversation },
				.History      = {
					.Defect = {
						.Id       = core::defect_id{ identity.Defect },
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

		[[nodiscard]] auto review_ledger_probe_defect_matches_thread(
			const application::durable_defect_record&       aDefect,
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread) -> bool
		{
			if (!review_ledger_probe_thread_matches_file(aFile, aThread)) {
				return false;
			}
			const auto identity = defect_identity(
				review_ledger_probe_thread_identity());
			const auto anchor = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = core::diff_side::right,
				.Line       = *aThread.Line,
			};
			return aDefect.File == aFile
				&& aDefect.Conversation.value() == identity.Conversation
				&& aDefect.History.Defect.Id.value() == identity.Defect
				&& aDefect.History.Defect.Anchor == anchor
				&& aDefect.History.OriginalAnchor == anchor;
		}

		[[nodiscard]] auto review_ledger_probe_defect_match_is_non_vacuous(
			const core::file_identity                       aFile,
			const core::file_comparison_digest              aComparison,
			const application::github_review_thread_record& aThread) -> bool
		{
			const auto expected = review_ledger_probe_defect(
				aFile, aComparison, aThread);
			if (!expected.has_value()) {
				return false;
			}
			auto       wrongLine = *expected;
			const auto counterexampleLine
				= wrongLine.History.Defect.Anchor.Line == 1U ? 2U : 1U;
			wrongLine.History.Defect.Anchor.Line  = counterexampleLine;
			wrongLine.History.OriginalAnchor.Line = counterexampleLine;
			return review_ledger_probe_defect_matches_thread(
					   *expected, aFile, aComparison, aThread)
				&& !review_ledger_probe_defect_matches_thread(
					   wrongLine, aFile, aComparison, aThread);
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
			const auto* const defectThread = implementation->accepted_thread(
				review_ledger_probe_thread_identity());
			if (defectThread == nullptr
				|| !review_ledger_probe_thread_matches_file(
					probe.File, *defectThread)) {
				return fail();
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
					probe.File, probe.Material, *defectThread);
				if (!defect.has_value()
					|| !review_ledger_probe_defect_match_is_non_vacuous(
						probe.File, probe.Material, *defectThread)) {
					return fail();
				}
				const auto& head = implementation->LastAcceptedHandoff->Head;
				if (!ledger->persist(note)
					|| !ledger->persist_last_reviewed_head(head)
					|| !ledger->persist(*defect)) {
					return fail();
				}
				const auto privateState       = ledger->private_observation();
				probe.Evidence.PrivateRecords = privateState.has_value()
					&& privateState->Notes == std::vector{ note }
					&& privateState->LastReviewedHead == std::optional{ head }
					&& privateState->Defects == std::vector{ *defect }
					&& review_ledger_probe_defect_matches_thread(
													privateState->Defects.front(),
													probe.File,
													probe.Material,
													*defectThread);
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
					const auto defect = review_ledger_probe_defect(
						probe.File, storedComparison, *defectThread);
					probe.Evidence.PrivateRecords
						= defect.has_value()
						&& review_ledger_probe_defect_match_is_non_vacuous(
							  probe.File, storedComparison, *defectThread)
						&& storedComparison.valid()
						&& storedComparison != probe.Material
						&& privateState->Notes
							== std::vector{ review_ledger_probe_note(
								probe.File, storedComparison) }
						&& priorHead.has_value()
						&& privateState->LastReviewedHead == priorHead
						&& privateState->Defects
							== std::vector{ *defect }
						&& review_ledger_probe_defect_matches_thread(
							  privateState->Defects.front(),
							  probe.File,
							  storedComparison,
							  *defectThread)
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
		using result     = detail::review_round_trip_composition_probe_result;
		using state_type = github_application::implementation::
			review_round_trip_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::failed;
		}
		if (!implementation->ReviewRoundTripCompositionProbe.has_value()) {
			implementation->ReviewRoundTripCompositionProbe.emplace();
		}
		auto& probe = *implementation->ReviewRoundTripCompositionProbe;
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}
		if (implementation->Fixture != nullptr && implementation->Window != nullptr
			&& implementation->WorkbenchHost != nullptr
			&& implementation->ReviewMutationHost != nullptr
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
			|| implementation->ReviewMutationHost == nullptr
			|| !implementation->ReviewDraft.has_value()
			|| !implementation->LastAcceptedHandoff.has_value()
			|| !implementation->LastSelectedDiff.has_value()
			|| implementation->Window->review_editor() == nullptr) {
			return result::pending;
		}
		const auto fail = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		auto* const editor     = implementation->Window->review_editor();
		const auto  designated = github_application::implementation::review_mutation_target(
			*implementation->LastAcceptedHandoff);
		const auto clickRightLine = [implementation](const std::uint32_t aLine) {
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || comparison->right_pane() == nullptr) {
				return false;
			}
			auto* const pane  = comparison->right_pane();
			const auto  block = pane->document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid()) {
				return false;
			}
			const auto point = pane->cursorRect(QTextCursor{ block }).center();
			auto       press = QMouseEvent{
				QEvent::MouseButtonPress,
				QPointF{ point },
				QPointF{ point },
				QPointF{ point },
				Qt::LeftButton,
				Qt::LeftButton,
				Qt::NoModifier,
			};
			auto release = QMouseEvent{
				QEvent::MouseButtonRelease,
				QPointF{ point },
				QPointF{ point },
				QPointF{ point },
				Qt::LeftButton,
				Qt::NoButton,
				Qt::NoModifier,
			};
			QCoreApplication::sendEvent(pane->viewport(), &press);
			QCoreApplication::sendEvent(pane->viewport(), &release);
			return true;
		};

		if (probe.Phase == state_type::phase::awaiting_editor) {
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || !comparison->selection().has_value()) {
				return result::pending;
			}
			if (comparison->selection()->PreferredLine.Side
				!= core::diff_side::right) {
				auto* const pane  = comparison->right_pane();
				const auto  block = pane->document()->findBlockByNumber(0);
				const auto  point = pane->cursorRect(QTextCursor{ block }).center();
				auto        press = QMouseEvent{
					QEvent::MouseButtonPress,
					QPointF{ point },
					QPointF{ point },
					QPointF{ point },
					Qt::LeftButton,
					Qt::LeftButton,
					Qt::NoModifier,
				};
				auto release = QMouseEvent{
					QEvent::MouseButtonRelease,
					QPointF{ point },
					QPointF{ point },
					QPointF{ point },
					Qt::LeftButton,
					Qt::NoButton,
					Qt::NoModifier,
				};
				QCoreApplication::sendEvent(pane->viewport(), &press);
				QCoreApplication::sendEvent(pane->viewport(), &release);
				return result::pending;
			}
			probe.Evidence.ExactDesignatedTarget
				= implementation->ReviewMutationHost->designated() == designated;
			probe.Evidence.EditorComposed = editor->shown_draft().has_value()
				&& editor->shown_draft()->Anchor
					== comparison->selection()->PreferredLine
				&& editor->shown_draft()->Anchor.Side == core::diff_side::right
				&& editor->shown_draft()->Anchor.Comparison
					== core::comparison_digest(
						implementation->LastSelectedDiff->Key)
				&& editor->target_label()->text().contains(
					QString::number(implementation->LastAcceptedHandoff->Number.value()));
			probe.Evidence.CapabilityFallback
				= !implementation->ReviewMutationHost->capabilities().ResolvesThreads
				&& !implementation->ReviewMutationHost->capabilities()
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
			probe.Phase = state_type::phase::awaiting_stale_refusal;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_stale_refusal) {
			const auto provider = implementation->Fixture->review_mutation_evidence();
			probe.Evidence.StaleHeadRefused
				= implementation->ReviewDraft->State
					== core::review_draft_state::drafting
				&& provider.Submissions == 0U
				&& editor->mutation_status_text().contains(QStringLiteral("head moved"));
			if (!probe.Evidence.StaleHeadRefused) {
				return fail();
			}
			const auto* const comparison
				= implementation->Window->comparison_surface();
			if (comparison == nullptr || !comparison->selection().has_value()
				|| implementation->ReviewMutationDecisionGate == nullptr) {
				return fail();
			}
			probe.PublishingSelection = comparison->selection();
			probe.PublishingDraft     = implementation->ReviewDraft;
			implementation->Fixture->report_review_mutation_target(designated);
			implementation->ReviewMutationDecisionGate->hold();
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_lost_effect;
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_lost_effect) {
			const auto lost = implementation->Fixture->review_mutation_evidence();
			if (!lost.LostResult || lost.StoredComments != 1U
				|| implementation->ReviewMutationDecisionGate == nullptr
				|| implementation->ReviewMutationDecisionGate->pending() == 0U) {
				return result::pending;
			}
			if (!probe.PublishingSelection.has_value()
				|| !implementation->PendingReviewMutation.has_value()
				|| !implementation->ReviewDraft.has_value()
				|| implementation->ReviewDraft->State
					!= core::review_draft_state::publishing) {
				return fail();
			}
			auto* const comparison = implementation->Window->comparison_surface();
			if (comparison == nullptr || comparison->right_pane() == nullptr) {
				return fail();
			}
			for (auto line = 0; line < comparison->right_pane()->document()->blockCount();
				 ++line) {
				if (static_cast<std::uint32_t>(line)
					== probe.PublishingSelection->PreferredLine.Line) {
					continue;
				}
				if (!clickRightLine(static_cast<std::uint32_t>(line))) {
					continue;
				}
				if (comparison->selection().has_value()
					&& comparison->selection()->PreferredLine.Side
						== core::diff_side::right
					&& comparison->selection()->PreferredLine
						!= probe.PublishingSelection->PreferredLine) {
					probe.DeferredSelection = comparison->selection();
					break;
				}
			}
			const auto intermediateDeferred
				= probe.DeferredSelection.has_value()
				&& implementation->DeferredReviewSelection.has_value()
				&& *implementation->DeferredReviewSelection
					== probe.DeferredSelection
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& editor->shown_draft() == implementation->ReviewDraft;
			if (!intermediateDeferred
				|| !clickRightLine(probe.PublishingSelection->PreferredLine.Line)) {
				return fail();
			}
			probe.Evidence.SelectionDeferred = true;
			probe.Evidence.LatestSelectionWins
				= comparison->selection() == probe.PublishingSelection
				&& implementation->DeferredReviewSelection.has_value()
				&& *implementation->DeferredReviewSelection
					== probe.PublishingSelection
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& implementation->PendingReviewMutation.has_value()
				&& probe.PublishingDraft.has_value()
				&& implementation->ReviewDraft->Id == probe.PublishingDraft->Id
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& implementation->ReviewDraft->State
					== core::review_draft_state::publishing;
			if (!probe.Evidence.LatestSelectionWins) {
				return fail();
			}
			implementation->ReviewMutationDecisionGate->release();
			probe.Phase = state_type::phase::awaiting_deferred_terminal;
			return result::pending;
		}

		if (implementation->ReviewMutationHost->outstanding_requests() != 0U
			|| implementation->ReviewDraft->State
				== core::review_draft_state::publishing) {
			return result::pending;
		}

		if (probe.Phase == state_type::phase::awaiting_deferred_terminal) {
			const auto        lost = implementation->Fixture->review_mutation_evidence();
			const auto* const comparison
				= implementation->Window->comparison_surface();
			auto retryIsolation = false;
			if (probe.PublishingSelection.has_value()
				&& probe.DeferredSelection.has_value()
				&& implementation->RetryableReviewDraft.has_value()
				&& implementation->SelectedDiffPublication.has_value()) {
				auto foreign     = implementation->RetryableReviewDraft->Authority;
				foreign.Snapshot = core::provider_snapshot_id{
					foreign.Snapshot.value() + 1U
				};
				retryIsolation
					= !github_application::implementation::exact_retryable_review_draft(
						  *implementation->RetryableReviewDraft,
						  *probe.DeferredSelection,
						  implementation->SelectedDiffPublication->Request.WorkbenchAuthority)
					&& !github_application::implementation::exact_retryable_review_draft(
						*implementation->RetryableReviewDraft,
						*probe.PublishingSelection,
						foreign);
			}
			probe.Evidence.LostResultRefreshed = lost.LostResult
				&& lost.StoredComments == 1U
				&& implementation->ReviewProjection
					== implementation->Fixture->review_comments();
			probe.Evidence.PendingCleared
				= !implementation->PendingReviewMutation.has_value();
			probe.Evidence.DeferredDraftExact
				= comparison != nullptr && probe.PublishingDraft.has_value()
				&& implementation->SelectedSemanticSelection
					== probe.PublishingSelection
				&& comparison->selection() == probe.PublishingSelection
				&& implementation->ReviewDraft->State
					== core::review_draft_state::drafting
				&& implementation->ReviewDraft->Anchor
					== probe.PublishingSelection->PreferredLine
				&& implementation->ReviewDraft->Id == probe.PublishingDraft->Id
				&& implementation->ReviewDraft->Body == probe.PublishingDraft->Body
				&& editor->shown_draft() == implementation->ReviewDraft
				&& implementation->RetryableReviewDraft.has_value()
				&& implementation->ReviewDraft
					== std::optional{ implementation->RetryableReviewDraft->Draft }
				&& retryIsolation;
			probe.Evidence.StaleTerminalWithheld
				= editor->mutation_status_text().contains(
					  QStringLiteral("unavailable"))
				&& !editor->mutation_status_text().contains(
					QStringLiteral("retry is safe"))
				&& !editor->mutation_status_text().contains(
					QStringLiteral("cancelled"));
			if (!probe.Evidence.LostResultRefreshed
				|| !probe.Evidence.PendingCleared
				|| !probe.Evidence.DeferredDraftExact
				|| !probe.Evidence.StaleTerminalWithheld
				|| !probe.PublishingSelection.has_value()) {
				return fail();
			}
			if (comparison == nullptr || !comparison->selection().has_value()
				|| comparison->selection()->PreferredLine
					!= probe.PublishingSelection->PreferredLine
				|| !implementation->ReviewDraft.has_value()
				|| !implementation->RetryableReviewDraft.has_value()
				|| *implementation->ReviewDraft
					!= implementation->RetryableReviewDraft->Draft) {
				return fail();
			}
			editor->body_editor()->setPlainText(
				QStringLiteral("BPR-0229 fixture publication."));
			if (implementation->ReviewDraft->Body
				!= "BPR-0229 fixture publication.") {
				return fail();
			}
			editor->publish_action()->trigger();
			probe.Phase = state_type::phase::awaiting_retry;
			return result::pending;
		}

		const auto provider            = implementation->Fixture->review_mutation_evidence();
		const auto statistics          = implementation->ReviewMutationHost->statistics();
		probe.Evidence.RetryIdempotent = provider.Submissions == 2U
			&& provider.StoredComments == 1U && provider.DuplicateReturned
			&& statistics.EffectsQueued == 3U
			&& statistics.TerminalsPublished == 3U
			&& !implementation->RetryableReviewDraft.has_value()
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
			&& editor->mutation_status_text().contains(QStringLiteral("safe retry"))
			&& editor->statistics().MutationUpdates >= 3U;
		if (!probe.Evidence.RetryIdempotent || !probe.Evidence.ServerAuthority
			|| !probe.Evidence.TypedQtProjection
			|| !probe.Evidence.SelectionDeferred
			|| !probe.Evidence.LatestSelectionWins
			|| !probe.Evidence.PendingCleared
			|| !probe.Evidence.DeferredDraftExact
			|| !probe.Evidence.StaleTerminalWithheld) {
			return fail();
		}
		probe.Phase = state_type::phase::passed;
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

	auto detail::github_application_fixture_access::dormant_live_provider_selection_evidence_for(
		const github_application& aApplication)
		-> detail::dormant_live_provider_selection_evidence
	{
		auto  evidence       = detail::dormant_live_provider_selection_evidence{ };
		auto* implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return evidence;
		}
		auto       handoff  = clone_without_smart_git(*implementation->LastAcceptedHandoff);
		auto       snapshot = application::github_pull_request_snapshot{ };
		const auto base     = core::parse_git_commit_oid(designated_review_base_oid);
		const auto head     = core::parse_git_commit_oid(designated_review_head_oid);
		const auto baseRef  = application::parse_git_base_branch_ref(
			designated_review_base_full_ref);
		if (!base.has_value() || !head.has_value() || !baseRef.has_value()) {
			return evidence;
		}

		handoff.Hostname = application::github_canonical_hostname{
			designated_review_hostname
		};
		handoff.ServerContract = application::github_server_contract::github_com_2022_11_28;
		handoff.Repository     = core::repository_id{ designated_review_repository_id };
		handoff.Locator        = {
			.Owner = std::string{ designated_review_owner },
			.Name  = std::string{ designated_review_name },
		};
		handoff.PullRequest = core::pull_request_id{ designated_review_pull_request_id };
		handoff.Number      = core::pull_request_number{ designated_review_number };
		handoff.BaseRef     = *baseRef;
		handoff.Base        = *base;
		handoff.Head        = *head;

		snapshot.Profile = handoff.Profile;
		snapshot.Host    = handoff.Host;
		snapshot.Account = handoff.Account;
		snapshot.Viewer  = {
			.Actor = application::github_actor_id{ "MDQ6VXNlcjIy" },
			.Login = "fixture-reviewer",
		};
		snapshot.Repository.Repository   = handoff.Repository;
		snapshot.Repository.Node         = application::github_node_id{ std::string{ designated_review_repository_node } };
		snapshot.Repository.Owner        = std::string{ designated_review_owner };
		snapshot.Repository.Name         = std::string{ designated_review_name };
		snapshot.PullRequest.PullRequest = handoff.PullRequest;
		snapshot.PullRequest.Repository  = handoff.Repository;
		snapshot.PullRequest.Number      = handoff.Number;
		snapshot.PullRequest.Node        = application::github_node_id{ std::string{ designated_review_pull_request_node } };
		snapshot.PullRequest.State       = application::github_pull_request_state::open;
		snapshot.PullRequest.BaseOid     = application::commit_oid{ std::string{ designated_review_base_oid } };
		snapshot.PullRequest.HeadOid     = application::commit_oid{ std::string{ designated_review_head_oid } };
		snapshot.PullRequest.BaseRef     = std::string{ designated_review_base_ref };
		snapshot.PullRequest.HeadRef     = std::string{ designated_review_head_ref };

		const auto exactSelection = select_designated_review_mutation(
			handoff, snapshot);
		evidence.ExactTupleAccepted = exactSelection.has_value();
		auto rejects                = [&handoff, &snapshot, &evidence](auto aMutate) {
			auto changedHandoff  = clone_without_smart_git(handoff);
			auto changedSnapshot = snapshot;
			aMutate(changedHandoff, changedSnapshot);
			evidence.MismatchedTuplesRejected += static_cast<std::size_t>(
				!select_designated_review_mutation(
					 changedHandoff, changedSnapshot)
					 .has_value());
		};
		rejects([](auto& aHandoff, auto&) { aHandoff.Hostname = application::github_canonical_hostname{ "ghe.example.invalid" }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Locator.Owner = "Other"; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Locator.Name = "Other"; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Repository = core::repository_id{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.PullRequest = core::pull_request_id{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Number = core::pull_request_number{ 9U }; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Base = aHandoff.Head; });
		rejects([](auto& aHandoff, auto&) { aHandoff.Head = aHandoff.Base; });
		rejects([](auto& aHandoff, auto&) { aHandoff.BaseRef = application::git_base_branch_ref{ }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Profile = core::host_profile_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Host = core::git_host_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Account = core::github_account_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Viewer.Actor = application::github_actor_id{ }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Viewer.Login.clear(); });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Repository = core::repository_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Node = application::github_node_id{ "other" }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Owner = "Other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.Repository.Name = "Other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.PullRequest = core::pull_request_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Repository = core::repository_id{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Number = core::pull_request_number{ 9U }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.Node = application::github_node_id{ "other" }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.State = application::github_pull_request_state::closed; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.BaseOid = application::commit_oid{ std::string{ designated_review_head_oid } }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.HeadOid = application::commit_oid{ std::string{ designated_review_base_oid } }; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.BaseRef = "other"; });
		rejects([](auto&, auto& aSnapshot) { aSnapshot.PullRequest.HeadRef = "other"; });
		evidence.ExpectedMismatchRejections = 28U;
		evidence.FixtureProviderSelected
			= implementation->FixtureReviewMutationHostComposed;
		evidence.LiveProviderAbsent = true;
		return evidence;
	}

	auto detail::github_application_fixture_access::review_mutation_provider_marker(
		const github_application& aApplication) noexcept -> std::string_view
	{
		const auto* implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->Fixture != nullptr
			? std::string_view{ "fixture" }
			: std::string_view{ "none" };
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
			|| implementation->ReviewMutationHost == nullptr
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

	auto detail::github_application_fixture_access::seed_agent_private_context_canary(
		github_application&    aApplication,
		const std::string_view aSelectedNote,
		const std::string_view aUnselectedNote) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->EpochHost == nullptr || aSelectedNote.empty()
			|| aUnselectedNote.empty()) {
			return false;
		}
		const auto& state = implementation->EpochHost->state();
		if (!state.AcceptedRevision.has_value()) {
			return false;
		}
		const auto selected = application::local_note_record{
			.Id   = application::local_note_id{ 246U },
			.Note = { .Text = std::string{ aSelectedNote } },
		};
		const auto unselected = application::local_note_record{
			.Id   = application::local_note_id{ 247U },
			.Note = { .Text = std::string{ aUnselectedNote } },
		};
		const auto persisted
			= implementation->ReviewLedgerHost->persist(selected).has_value()
			&& implementation->ReviewLedgerHost->persist(unselected).has_value()
			&& implementation->ReviewLedgerHost
				   ->persist_last_reviewed_head(state.AcceptedRevision->Head)
				   .has_value();
		if (persisted) {
			implementation->refresh_agent_private_context_choices();
		}
		return persisted;
	}

	auto detail::github_application_fixture_access::agent_invocation_context(
		github_application& aApplication)
		-> std::optional<detail::agent_invocation_context_evidence>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->EpochHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| implementation->Window == nullptr
			|| !implementation->AgentReview.has_value()
			|| !implementation->SelectedDiffPublication.has_value()) {
			return std::nullopt;
		}
		const auto& state    = implementation->EpochHost->state();
		const auto& selected = *implementation->SelectedDiffPublication;
		if (
			!state.Authority.has_value() || !state.AcceptedRevision.has_value()
			|| !state.AcceptedComparison.has_value()) {
			return std::nullopt;
		}
		auto receipt = std::optional<core::file_receipt>{ };
		{
			const auto lock = std::scoped_lock{
				implementation->ReviewLedgerStateMutex
			};
			receipt = implementation->ReviewLedgerReceipt;
		}
		const auto privateState
			= implementation->ReviewLedgerHost->private_observation();
		const auto sessionCatalog
			= implementation->ReviewLedgerHost->session_catalog();
		auto* const workbench = implementation->Window->comparison_surface();
		if (
			!receipt.has_value() || !privateState.has_value()
			|| !sessionCatalog.has_value() || !sessionCatalog->Next.has_value()
			|| receipt->File != selected.Request.File.File || workbench == nullptr
			|| !workbench->selection().has_value()
			|| workbench->current_result() == nullptr
			|| workbench->current_result()->Alignment.Key != selected.Key
			|| workbench->selection()->Comparison
				!= workbench->current_result()->Alignment.Digest) {
			return std::nullopt;
		}
		const auto& authority  = *state.Authority;
		const auto& revision   = *state.AcceptedRevision;
		const auto& comparison = *state.AcceptedComparison;
		return detail::agent_invocation_context_evidence{
			.ApplicationSession = state.ApplicationSession,
			.Authority          = authority,
			.NextSession        = *sessionCatalog->Next,
			.Binding            = {
				.Host        = authority.Host,
				.Repository  = authority.Repository,
				.PullRequest = authority.PullRequest,
				.BaseOid     = revision.Base,
				.HeadOid     = revision.Head,
				.LeftEpoch   = comparison.Left.Identity.Epoch,
				.RightEpoch  = comparison.Right.Identity.Epoch,
			},
			.PullRequestUrl = "https://" + implementation->AgentReview->Hostname + "/" + implementation->AgentReview->Locator.Owner + "/" + implementation->AgentReview->Locator.Name + "/pull/" + std::to_string(implementation->AgentReview->Number.value()),
			.Files          = {
				application::agent_file_context_input{
					.Path               = selected.Request.File.PresentationPath,
					.Receipt            = *receipt,
					.ChangedSinceReview = privateState->LastReviewedHead.has_value() && *privateState->LastReviewedHead != revision.Head,
				},
			},
			.Private   = *privateState,
			.Selection = core::supplement_semantic_context{
				.File       = selected.Request.File.File,
				.Selection  = *workbench->selection(),
				.Confidence = core::supplement_mapping_confidence::exact,
				.Segments   = workbench->current_result()->Alignment.Segments,
				.Groups     = workbench->current_result()->Alignment.Groups,
			},
			.CurrentDraft = implementation->current_agent_draft_observation(),
		};
	}

	auto detail::github_application_fixture_access::agent_session_catalog(
		github_application& aApplication)
		-> std::optional<detail::agent_session_catalog_evidence>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr) {
			return std::nullopt;
		}
		auto catalog = implementation->ReviewLedgerHost->session_catalog();
		if (!catalog.has_value()) {
			return std::nullopt;
		}
		return detail::agent_session_catalog_evidence{
			.Catalog    = std::move(*catalog),
			.Statistics = implementation->ReviewLedgerHost->statistics(),
		};
	}

	auto detail::github_application_fixture_access::agent_invocation_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_invocation_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		return {
			.HostComposed = implementation->AgentHost != nullptr,
			.Quiescent    = implementation->AgentHost == nullptr
				|| implementation->AgentHost->is_quiescent(),
			.Statistics    = implementation->AgentHost != nullptr
				? implementation->AgentHost->statistics()
				: application::agent_invocation_host_statistics{ },
			.WorkspaceRoot = implementation->AgentWorkspaceRoot != nullptr
				? std::filesystem::path{
					  implementation->AgentWorkspaceRoot->path().toStdString() }
				: std::filesystem::path{ },
		};
	}
#endif
}
