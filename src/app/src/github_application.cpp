#include "bpr/app/github_application.hpp"
#include "bpr/logging/quill_log.hpp"

#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "live_review_submission_provider.hpp"
#include "review_mutation_execution_authority.hpp"
#include "review_submission_execution_authority.hpp"
#include "review_submission_workflow.hpp"
#include "system_browser_opener.hpp"

#include "bpr/app/worker_executor.hpp"

#include "designated_review_public_observer.hpp"
#include "github_device_authorization_transport.hpp"
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
	#include "review_submission_fixture.hpp"
#endif

#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/mcp/mcp_context_helper.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/agent_chat_session_host.hpp"
#include "bpr/application/agent_comment_workshop_host.hpp"
#include "bpr/application/agent_finding_host.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_realignment.hpp"
#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/defect_correlation.hpp"
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/mcp_context_helper_supervisor.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/review_ledger_application_host.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/application/workbench_snapshot_observation.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/event_loop_device_timer.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/sensitive_text_input.hpp"
#include "bpr/qt/side_chat_panel.hpp"
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
#include <QVariant>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <fstream>
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
		/// \brief Observes the real composed launcher without replacing its effect.
		class composition_agent_process_launcher_probe final
			: public application::agent_process_port
		{
		public:
			explicit composition_agent_process_launcher_probe(
				adapters::agent::agent_process_launcher& aLauncher) noexcept
				: Launcher{ aLauncher }
			{
			}

			[[nodiscard]] auto run(
				const application::agent_launch_request& aRequest,
				const int                                aTimeoutMilliseconds,
				const application::agent_output_sink&    aSink = { },
				const std::stop_token                    aStop = { })
				-> application::agent_run_result override
			{
				Invocations.fetch_add(1U, std::memory_order_relaxed);
				return Launcher.run(aRequest, aTimeoutMilliseconds, aSink, aStop);
			}

			void cancel() noexcept override
			{
				Launcher.cancel();
			}

			[[nodiscard]] auto invocations() const noexcept -> std::size_t
			{
				return Invocations.load(std::memory_order_relaxed);
			}

		private:
			adapters::agent::agent_process_launcher& Launcher;
			std::atomic_size_t                       Invocations = 0U;
		};

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

		[[nodiscard]] auto select_observed_review_mutation(
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
			case detail::github_public_path_fixture::large_review_v1:
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
			// The stores refuse a root another local principal can reach, and
			// what private means is the adapter's definition: mode 0700 on
			// POSIX, and on Windows an owner-only protected DACL that
			// std::filesystem::permissions cannot express (BPR-0436).
			if (!adapters::sqlite::make_storage_root_owner_private(aRoot)) {
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

			void store_refresh(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				store(std::move(aEnrollment), std::move(aCompletion));
			}

			void retrieve_refresh(
				const application::github_credential_operation_context     aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
				override
			{
				retrieve(aContext, std::move(aCompletion));
			}

			void delete_refresh(
				const application::github_credential_operation_context   aContext,
				application::github_credential_delete_completion_handler aCompletion)
				override
			{
				delete_credential(aContext, std::move(aCompletion));
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

#if defined(BPR_BUILD_TESTING)
		// Counts what crosses the enrollment boundary, and forwards it unchanged.
		//
		// The vault's own store count cannot answer "exactly one enrollment": the
		// GitHub host collapses a repeated enrollment for the same profile before
		// the vault ever sees a second operation, so a double call still reports
		// one store. That collapse is correct, and it is also exactly why the
		// count has to be taken here — at the boundary the RFC's criterion is
		// about — rather than downstream of something that hides duplicates.
		class counting_device_authorization_enrollment final
			: public application::device_authorization_enrollment_port
		{
		public:
			explicit counting_device_authorization_enrollment(
				application::device_authorization_enrollment_port& aInner) noexcept
				: Inner{ &aInner }
			{
			}

			void enroll(
				const core::host_profile_id             aProfile,
				application::github_credential_material aCredential) override
			{
				++Enrollments;
				Inner->enroll(aProfile, std::move(aCredential));
			}

			void enroll_grant(core::host_profile_id aProfile, application::github_credential_material aCredential, application::device_binding_lifetime aLifetime, bool aSettled) override
			{
				++Enrollments;
				Inner->enroll_grant(aProfile, std::move(aCredential), aLifetime, aSettled);
			}
			void settle_binding(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_binding_lifetime aLifetime) override
			{
				Inner->settle_binding(std::move(aAuthority), aGeneration, aLifetime);
			}
			void prepare_refresh(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion) override
			{
				Inner->prepare_refresh(std::move(aAuthority), aGeneration, std::move(aCompletion));
			}
			void invalidate_grant(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration) override { Inner->invalidate_grant(std::move(aAuthority), aGeneration); }

			[[nodiscard]] auto enrollments() const noexcept -> std::size_t
			{
				return Enrollments;
			}

		private:
			application::device_authorization_enrollment_port* Inner       = nullptr;
			std::size_t                                        Enrollments = 0U;
		};
#endif

		// Records what presentation was told. The device panel is a Qt surface
		// [[BPR-0280]] owns and tests at the adapter boundary; what composition
		// needs is the same published record, so the public self-test can state
		// the typed refusal a reviewer would see rather than infer it.
		class device_authorization_projection_sink final
			: public application::device_authorization_projection_port
		{
		public:
			void publish(
				const application::device_authorization_presentation& aPresentation)
				override
			{
				Last = aPresentation;
				++Published;
				// The host clears the handoff on the next stage change, so a
				// reader that arrives after the flow moved on sees
				// `not_attempted` and cannot tell an opener that answered
				// from one that was never asked. This keeps the answer
				// instead of the moment it was true: what a self-test needs
				// at shutdown is which fate the last attempt met, and only a
				// new attempt or a new run replaces it.
				if (
					aPresentation.Handoff
					!= application::device_authorization_handoff::not_attempted) {
					LastAttempt = aPresentation.Handoff;
				}
			}

			// The renewal lifecycle the host published last. Recorded for the
			// same reason the presentation is: a composed course has to state
			// what a reviewer would be told about renewal rather than infer it
			// from a rotation count nobody surfaced.
			void publish_refresh(
				const application::device_authorization_refresh_observation&
					aObservation) override
			{
				LastRefresh = aObservation;
				++RefreshPublished;
			}

			[[nodiscard]] auto last() const noexcept
				-> const application::device_authorization_presentation&
			{
				return Last;
			}

			[[nodiscard]] auto last_refresh() const noexcept
				-> const application::device_authorization_refresh_observation&
			{
				return LastRefresh;
			}

			[[nodiscard]] auto refresh_published() const noexcept -> std::size_t
			{
				return RefreshPublished;
			}

			[[nodiscard]] auto last_attempt() const noexcept
				-> application::device_authorization_handoff
			{
				return LastAttempt;
			}

			[[nodiscard]] auto published() const noexcept -> std::size_t
			{
				return Published;
			}

		private:
			application::device_authorization_presentation        Last;
			application::device_authorization_refresh_observation LastRefresh;
			application::device_authorization_handoff             LastAttempt
				= application::device_authorization_handoff::not_attempted;
			std::size_t Published        = 0U;
			std::size_t RefreshPublished = 0U;
		};

		// The device host publishes to exactly one port, and [[BPR-0526]] gave
		// the presentation a second reader without taking the first one away.
		// So this stands where the sink stood and hands every publication to
		// both: the sink first and unconditionally, then the mounted panel if
		// one was composed.
		//
		// Unconditionally is the whole point. Six executable contracts read
		// `DeviceProjections` through the self-test markers, and they have to
		// read exactly what they read before a window was in the picture --
		// including in a graph whose panel composition was cut. `publish()` is
		// synchronous and returns nothing, so a second reader cannot reorder,
		// delay, or veto what the first one sees.
		class composition_device_authorization_projection final
			: public application::device_authorization_projection_port
		{
		public:
			using presented_sink = std::function<void(
				const application::device_authorization_presentation&)>;
			using renewed_sink   = std::function<void(
				const application::device_authorization_refresh_observation&)>;

			composition_device_authorization_projection(
				application::device_authorization_projection_port& aRecorded,
				presented_sink                                     aPresented,
				renewed_sink                                       aRenewed)
				: Recorded{ &aRecorded }
				, Presented{ std::move(aPresented) }
				, Renewed{ std::move(aRenewed) }
			{
			}

			void publish(
				const application::device_authorization_presentation& aPresentation)
				override
			{
				Recorded->publish(aPresentation);
				if (Presented) {
					Presented(aPresentation);
				}
			}

			// The renewal lifecycle fans out on the same terms and in the same
			// order: the sink first and unconditionally, then whoever
			// composition bound to watch renewal. [[BPR-0536]].
			void publish_refresh(
				const application::device_authorization_refresh_observation&
					aObservation) override
			{
				Recorded->publish_refresh(aObservation);
				if (Renewed) {
					Renewed(aObservation);
				}
			}

		private:
			application::device_authorization_projection_port* Recorded = nullptr;
			presented_sink                                     Presented;
			renewed_sink                                       Renewed;
		};

		// The session projection, with composition listening behind the window.
		// [[BPR-0536]] needs one fact the window has no use for: that an
		// authenticated `/user` observation was accepted, which is the first
		// moment the vault authority a refresh entry is keyed by exists. The
		// window is told first and unchanged, so every existing reader sees
		// exactly the sequence it saw before; composition only reads.
		class composition_github_projection final
			: public application::github_projection_port
		{
		public:
			using accepted_sink = std::function<void(
				const application::github_presentation_update&)>;

			composition_github_projection(
				application::github_projection_port& aPresented,
				accepted_sink                        aAccepted)
				: Presented{ &aPresented }
				, Accepted{ std::move(aAccepted) }
			{
			}

			void publish(
				const application::github_presentation_update& aUpdate) override
			{
				Presented->publish(aUpdate);
				if (Accepted) {
					Accepted(aUpdate);
				}
			}

		private:
			application::github_projection_port* Presented = nullptr;
			accepted_sink                        Accepted;
		};

		// Records what the signed-out presentation was told. The window is the
		// reader [[BPR-0304]] built and tests at the adapter boundary; what
		// composition needs is the same published record, so the public
		// self-test can state the mode and the typed refusal a reviewer would
		// see rather than infer them from a picture.
		class public_preview_projection_sink final
			: public application::public_preview_projection_port
		{
		public:
			void publish(
				const application::public_preview_presentation& aPresentation)
				override
			{
				Last = aPresentation;
				++Published;
			}

			[[nodiscard]] auto last() const noexcept
				-> const application::public_preview_presentation&
			{
				return Last;
			}

			[[nodiscard]] auto published() const noexcept -> std::size_t
			{
				return Published;
			}

		private:
			application::public_preview_presentation Last;
			std::size_t                              Published = 0U;
		};

		// The same fan-out the device projection above uses, for the same
		// reason: the host publishes to exactly one port, the self-test
		// contracts read the recording sink, and the window is a second reader
		// that must not reorder, delay, or veto what the first one sees. The
		// sink is told first and unconditionally.
		class composition_public_preview_projection final
			: public application::public_preview_projection_port
		{
		public:
			using presented_sink = std::function<void(
				const application::public_preview_presentation&)>;

			composition_public_preview_projection(
				application::public_preview_projection_port& aRecorded,
				presented_sink                               aPresented)
				: Recorded{ &aRecorded }
				, Presented{ std::move(aPresented) }
			{
			}

			void publish(
				const application::public_preview_presentation& aPresentation)
				override
			{
				Recorded->publish(aPresentation);
				if (Presented) {
					Presented(aPresentation);
				}
			}

		private:
			application::public_preview_projection_port* Recorded = nullptr;
			presented_sink                               Presented;
		};

		// Counts every credential-free pull-request read that reached the
		// anonymous authority. Composed whether or not the selection hands
		// that authority to the host, so an unavailable build reports a number
		// rather than an absence: zero is the claim that the mode's gate
		// refused before a request was built, and any other value is the gate
		// being bypassed.
		class counting_anonymous_pull_requests final
			: public application::github_pull_request_port
		{
		public:
			explicit counting_anonymous_pull_requests(
				application::github_pull_request_port& aInner) noexcept
				: Inner{ &aInner }
			{
			}

			void fetch_pull_request(
				application::github_pull_request_provider_request aRequest,
				application::github_pull_request_completion_handler
					aCompletion) override
			{
				++Reads;
				Inner->fetch_pull_request(
					std::move(aRequest), std::move(aCompletion));
			}

			void cancel_pull_request(
				application::github_pull_request_provider_request aRequest)
				override
			{
				Inner->cancel_pull_request(std::move(aRequest));
			}

			[[nodiscard]] auto reads() const noexcept -> std::size_t
			{
				return Reads;
			}

		private:
			application::github_pull_request_port* Inner = nullptr;
			std::size_t                            Reads = 0U;
		};

		// [[BPR-0316]]. What the signed-out transport forwarded, in order,
		// and what came back for each request. The count below answers "how
		// much of the shared hourly budget did this read cost"; only this
		// answers "which operation failed, how, and how long it waited",
		// which is the question the first authorized read could not be asked.
		// Shared and lock-guarded rather than atomic, because the transport is
		// driven from a worker while the reporting thread copies the whole
		// record.
		class anonymous_wire_operation_log final
		{
		public:
			void record(
				std::string         aOperation,
				const std::uint32_t aPage,
				std::string         aOutcome,
				const std::uint64_t aElapsedMilliseconds)
			{
				const std::scoped_lock lock{ Mutex };
				Records.push_back(public_preview_wire_operation_record{
					.Operation           = std::move(aOperation),
					.Page                = aPage,
					.Outcome             = std::move(aOutcome),
					.ElapsedMilliseconds = aElapsedMilliseconds,
				});
			}

			// Latched by the first request that came back a failure, and never
			// cleared. The host retries a `network` or `timeout` provider
			// failure up to `maximum_github_provider_attempts`, so a candidate
			// read allowed to fail costs three times what it was authorized
			// to; the transport answering the retry itself, without
			// forwarding it, is the whole of the single-attempt mechanism.
			void latch_failure()
			{
				const std::scoped_lock lock{ Mutex };
				Failed = true;
			}

			[[nodiscard]] auto failed() const -> bool
			{
				const std::scoped_lock lock{ Mutex };
				return Failed;
			}

			[[nodiscard]] auto records() const
				-> std::vector<public_preview_wire_operation_record>
			{
				const std::scoped_lock lock{ Mutex };
				return Records;
			}

		private:
			mutable std::mutex                                Mutex;
			std::vector<public_preview_wire_operation_record> Records;
			bool                                              Failed = false;
		};

		// The enumerator's own name, so a failure is reported as the kind the
		// provider contract calls it rather than as a number a reader has to
		// look up.
		[[nodiscard]] auto anonymous_wire_failure_name(
			const application::github_provider_failure aFailure) noexcept
			-> std::string_view
		{
			using failure = application::github_provider_failure;
			switch (aFailure) {
			case failure::authentication:
				return "authentication";
			case failure::permission:
				return "permission";
			case failure::network:
				return "network";
			case failure::api_version:
				return "api_version";
			case failure::unsupported_capability:
				return "unsupported_capability";
			case failure::rate_limit:
				return "rate_limit";
			case failure::certificate:
				return "certificate";
			case failure::redirect:
				return "redirect";
			case failure::timeout:
				return "timeout";
			case failure::process_startup:
				return "process_startup";
			case failure::cli_unavailable:
				return "cli_unavailable";
			case failure::vault:
				return "vault";
			case failure::malformed_response:
				return "malformed_response";
			case failure::response_limit:
				return "response_limit";
			case failure::unavailable:
				return "unavailable";
			}
			return "unknown";
		}

		// The whole of what a completion is allowed to say on a card: a status
		// line, or a failure kind with the two numbers a failed read is
		// diagnosed from. No body, no URL, no header other than the
		// rate-limit triple the failure itself named.
		[[nodiscard]] auto anonymous_wire_outcome_text(
			const adapters::github::detail::github_wire_completion& aCompletion)
			-> std::string
		{
			if (const auto* const success = std::get_if<
					adapters::github::detail::github_wire_success>(&aCompletion);
				success != nullptr) {
				return "http_" + std::to_string(success->Response.Status);
			}
			const auto* const failure
				= std::get_if<adapters::github::detail::github_wire_error>(
					&aCompletion);
			if (failure == nullptr) {
				return "cancelled";
			}
			auto text = std::string{ "error_" };
			text += anonymous_wire_failure_name(failure->Error.Failure);
			if (failure->Error.RetryAfterSeconds.has_value()) {
				text += ".retry_after=";
				text += std::to_string(*failure->Error.RetryAfterSeconds);
			}
			if (failure->Error.RateLimitCause.has_value()) {
				text += ".rate=";
				text += std::to_string(failure->Error.RateLimitCause->Limit);
				text += '/';
				text += std::to_string(failure->Error.RateLimitCause->Remaining);
				text += '/';
				text += std::to_string(
					failure->Error.RateLimitCause->ResetUnixSeconds);
			}
			return text;
		}

		// [[BPR-0316]]. Counts what the anonymous transport itself put on the
		// wire. The wrapper above counts pull-request reads offered to the
		// authority; one such read is several REST operations, and the number
		// a live candidate has to report is the second one. The counters are
		// shared and atomic because the transport is driven from a worker and
		// read from the reporting thread.
		class counting_anonymous_wire_transport final
			: public adapters::github::detail::github_wire_transport_port
		{
		public:
			counting_anonymous_wire_transport(
				std::shared_ptr<
					adapters::github::detail::github_wire_transport_port>
															  aInner,
				std::shared_ptr<std::atomic<std::size_t>>     aRequests,
				std::shared_ptr<std::atomic<std::size_t>>     aCapabilityProbes,
				std::shared_ptr<anonymous_wire_operation_log> aLog) noexcept
				: Inner{ std::move(aInner) }
				, Requests{ std::move(aRequests) }
				, CapabilityProbes{ std::move(aCapabilityProbes) }
				, Log{ std::move(aLog) }
			{
			}

			[[nodiscard]] auto perform(
				const adapters::github::detail::github_wire_request& aRequest,
				std::stop_token                                      aStop)
				-> adapters::github::detail::github_wire_completion override
			{
				// One attempt. Once a request for this read has come back a
				// failure, every later one is answered here -- as the
				// cancellation the host already knows how to end a read on --
				// rather than forwarded, so the retry costs nothing outside
				// the process. A refused request is neither counted nor
				// recorded, because the count is what reached the wire.
				// Pagination is part of the same budget: nine operations can
				// otherwise issue arbitrarily many successful page requests.
				if (Log->failed() || !reserve_request()) {
					return adapters::github::detail::github_wire_cancelled{
						.Correlation = aRequest.Correlation,
					};
				}
				const auto started    = std::chrono::steady_clock::now();
				auto       completion = Inner->perform(aRequest, aStop);
				const auto elapsed    = std::chrono::duration_cast<
					std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - started);
				Log->record(
					std::string{ adapters::github::detail::operation_name(
						aRequest.Correlation.Operation) },
					aRequest.Correlation.Page,
					anonymous_wire_outcome_text(completion),
					static_cast<std::uint64_t>(
						elapsed.count() < 0 ? 0 : elapsed.count()));
				if (std::holds_alternative<
						adapters::github::detail::github_wire_error>(
						completion)) {
					Log->latch_failure();
				}
				return completion;
			}

			[[nodiscard]] auto perform_capability_probe(
				const adapters::github::detail::
					github_capability_probe_wire_request& aRequest,
				std::stop_token                           aStop)
				-> adapters::github::detail::
					github_capability_probe_wire_completion override
			{
				CapabilityProbes->fetch_add(1U, std::memory_order_relaxed);
				return Inner->perform_capability_probe(
					aRequest, std::move(aStop));
			}

		private:
			[[nodiscard]] auto reserve_request() noexcept -> bool
			{
				auto count = Requests->load(std::memory_order_relaxed);
				while (count < 9U) {
					if (Requests->compare_exchange_weak(
							count, count + 1U, std::memory_order_relaxed)) {
						return true;
					}
				}
				return false;
			}

			std::shared_ptr<
				adapters::github::detail::github_wire_transport_port>
														  Inner;
			std::shared_ptr<std::atomic<std::size_t>>     Requests;
			std::shared_ptr<std::atomic<std::size_t>>     CapabilityProbes;
			std::shared_ptr<anonymous_wire_operation_log> Log;
		};

		// [[BPR-0316]]'s live-candidate REST authority. Every transport it
		// issues comes from `make_native_https_anonymous_read_transport` --
		// the shipped entry, with system proxy discovery and system trust --
		// and it has no credentialed form at all: both credentialed entries
		// refuse and count the attempt, so a composition that reached for an
		// authorization header shows up as a number rather than as a header
		// nobody looked for. The GHES refusal is the shipped factory's, made
		// before a transport exists.
		class shipped_anonymous_wire_transport_factory final
			: public github_wire_transport_factory_port
		{
		public:
			[[nodiscard]] auto make_bootstrap(
				const validated_github_host_profile&    aProfile,
				core::github_session_request_context    aContext,
				application::github_credential_material aCredential) noexcept
				-> adapters::github::detail::
					github_wire_transport_factory_result override
			{
				static_cast<void>(aProfile);
				static_cast<void>(aContext);
				static_cast<void>(aCredential);
				CredentialedRefusals->fetch_add(1U, std::memory_order_relaxed);
				return std::unexpected{ refusal() };
			}

			[[nodiscard]] auto make_account(
				const validated_github_host_profile&            aProfile,
				application::github_credential_retrieve_success aCredential) noexcept
				-> adapters::github::detail::
					github_wire_transport_factory_result override
			{
				static_cast<void>(aProfile);
				static_cast<void>(aCredential);
				CredentialedRefusals->fetch_add(1U, std::memory_order_relaxed);
				return std::unexpected{ refusal() };
			}

			[[nodiscard]] auto make_anonymous(
				const validated_github_host_profile& aProfile,
				core::github_session_request_context aContext) noexcept
				-> adapters::github::detail::
					github_wire_transport_factory_result override
			{
				auto transport = adapters::github::detail::
					make_native_https_anonymous_read_transport(
						aProfile, std::move(aContext));
				if (!transport.has_value()) {
					return std::unexpected{ refusal() };
				}
				try {
					return std::shared_ptr<adapters::github::detail::
											   github_wire_transport_port>{
						std::make_shared<counting_anonymous_wire_transport>(
							std::shared_ptr<adapters::github::detail::
												github_wire_transport_port>{
								std::move(*transport) },
							Requests,
							CapabilityProbes,
							Log)
					};
				} catch (...) {
					return std::unexpected{ refusal() };
				}
			}

			[[nodiscard]] auto requests() const noexcept -> std::size_t
			{
				return Requests->load(std::memory_order_relaxed);
			}

			[[nodiscard]] auto capability_probes() const noexcept -> std::size_t
			{
				return CapabilityProbes->load(std::memory_order_relaxed);
			}

			[[nodiscard]] auto credentialed_refusals() const noexcept
				-> std::size_t
			{
				return CredentialedRefusals->load(std::memory_order_relaxed);
			}

			// What each forwarded request was for and how it ended, in the
			// order it was forwarded.
			[[nodiscard]] auto operations() const
				-> std::vector<public_preview_wire_operation_record>
			{
				return Log->records();
			}

		private:
			[[nodiscard]] static auto refusal() noexcept
				-> application::github_provider_error
			{
				return application::github_provider_error{
					.Failure = application::github_provider_failure::
						unsupported_capability,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				};
			}

			std::shared_ptr<std::atomic<std::size_t>> Requests
				= std::make_shared<std::atomic<std::size_t>>(0U);
			std::shared_ptr<std::atomic<std::size_t>> CapabilityProbes
				= std::make_shared<std::atomic<std::size_t>>(0U);
			std::shared_ptr<std::atomic<std::size_t>> CredentialedRefusals
				= std::make_shared<std::atomic<std::size_t>>(0U);
			std::shared_ptr<anonymous_wire_operation_log> Log
				= std::make_shared<anonymous_wire_operation_log>();
		};

		// The credential-free Git smart authority. It takes no credential and
		// has no entry that could accept one: every lease it issues comes from
		// the shipped anonymous factory, which refuses an authenticated
		// authority and refuses an enterprise profile before a request exists.
		// Constructed in both profiles, so a shipped graph reaches the
		// production factory by construction rather than through a fixture.
		class anonymous_git_smart_transport_factory final
			: public application::git_smart_transport_factory_port
		{
		public:
			explicit anonymous_git_smart_transport_factory(
				const validated_github_host_profile& aProfile) noexcept
				: Profile{ &aProfile }
			{
			}

			[[nodiscard]] auto acquire(
				const application::git_smart_request_context& aContext)
				-> application::git_smart_transport_result<
					application::git_smart_transport> override
			{
				++Acquisitions;
				auto wire = adapters::github::detail::
					make_native_https_anonymous_smart_transport(
						*Profile,
						core::github_session_request_context{
							.Request            = core::request_id{ 917U },
							.ApplicationSession = aContext.ApplicationSession,
							.Profile            = Profile->profile(),
							.IdentityGeneration
							= core::github_identity_generation{ 918U },
						});
				if (!wire.has_value()) {
					return std::unexpected{
						application::git_smart_transport_error{
							.Failure = application::
								git_smart_transport_failure::unavailable,
						}
					};
				}
				return adapters::github::detail::make_native_git_smart_transport(
					std::shared_ptr<
						adapters::github::detail::native_https_transport>{
						std::move(*wire) },
					aContext);
			}

			[[nodiscard]] auto acquisitions() const noexcept -> std::size_t
			{
				return Acquisitions;
			}

		private:
			const validated_github_host_profile* Profile      = nullptr;
			std::size_t                          Acquisitions = 0U;
		};

#if defined(BPR_BUILD_TESTING)
		// The fixture profile's opener. A tests-enabled graph never binds the
		// real one: `system_browser_opener` hands the address to the desktop,
		// and a suite that launched a browser on a reviewer's machine — or on
		// a station running unattended — would be doing the one thing no test
		// here is allowed to do. This records what it was given and answers
		// yes, which is what the handoff contract needs to observe `opened`
		// without any program starting.
		class recording_device_authorization_browser final
			: public application::device_authorization_browser_port
		{
		public:
			[[nodiscard]] auto open(const std::string_view aAddress)
				-> bool override
			{
				LastAddress.assign(aAddress);
				++Opens;
				return true;
			}

			[[nodiscard]] auto opens() const noexcept -> std::size_t
			{
				return Opens;
			}

			[[nodiscard]] auto last_address() const noexcept
				-> const std::string&
			{
				return LastAddress;
			}

		private:
			std::string LastAddress;
			std::size_t Opens = 0U;
		};
#endif

		// No registered application identity means there is nothing to talk to.
		// This is not a fixture and not a stub standing in for a real transport: it
		// is the honest shape of a build that has no client to authenticate as. The
		// host's capability gate refuses before any submission reaches it, so every
		// method here is unreachable, and each one answers a typed transport error
		// rather than succeeding quietly if the gate were ever bypassed.
		//
		// [[BPR-0318]] records a distributable identity; until it does, every
		// shipped platform composes this.
		class unregistered_device_authorization_transport final
			: public application::device_authorization_transport_port
		{
		public:
			void acquire_device_code(
				core::device_authorization_request_context  aContext,
				application::device_code_completion_handler aCompletion) override
			{
				++Reached;
				aCompletion(application::device_authorization_transport_error{
					.Context = aContext,
					.Failure = application::device_authorization_transport_failure::
						network,
				});
			}

			void exchange_device_token(
				application::device_token_exchange           aExchange,
				application::device_token_completion_handler aCompletion) override
			{
				refuse(aExchange, std::move(aCompletion));
			}

			void refresh_access_token(
				application::device_token_exchange           aExchange,
				application::device_token_completion_handler aCompletion) override
			{
				refuse(aExchange, std::move(aCompletion));
			}

			void cancel(core::device_authorization_request_context) override
			{
			}

			// Zero on every shipped build, and the public self-test says so. A
			// non-zero value would mean the capability gate let a submission
			// through, which is the exact failure this composition exists to make
			// impossible.
			[[nodiscard]] auto reached() const noexcept -> std::size_t
			{
				return Reached;
			}

		private:
			void refuse(
				const application::device_token_exchange&    aExchange,
				application::device_token_completion_handler aCompletion)
			{
				++Reached;
				aCompletion(application::device_authorization_transport_error{
					.Context = aExchange.Context,
					.Failure = application::device_authorization_transport_failure::
						network,
				});
			}

			std::size_t Reached = 0U;
		};

		// The recorded distributable production client, public by design: a
		// GitHub App client identifier carries no secret, and the shipped
		// artifact is required to contain exactly this one. Its registration,
		// permissions, and read-only course evidence live under
		// docs/verification/reviews/rfc0016-production-app-registration-v001/
		// (record commit c82e51c2c933887632fb0bb35db876375a151de9), and
		// scripts/verify_rfc0016_production_identity.py --release-binary
		// requires the enabled artifact to select it and to omit the private
		// BPR-0276 verification client.
		constexpr auto production_device_client_id
			= std::string_view{ "Iv23liitiYN1WSSbag50" };

		// macOS is registered under the recorded distributable identity since
		// [[BPR-0318]], Linux since [[BPR-0310]], and Windows since [[BPR-0311]];
		// each flipped its own platform in its own slice, against its own
		// target-native course, and neither flipped another's. Moving a card
		// between folders does not enable a capability, and neither does
		// building on a different operating system: this constant is the only
		// thing that does.
		[[nodiscard]] constexpr auto shipped_device_authorization_capability() noexcept
			-> application::device_authorization_capability
		{
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
			return { .Registered = true };
#else
			return { .Registered = false };
#endif
		}

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
			-> adapters::github::detail::native_https_test_configuration
		{
			return { .ProxyHost = "127.0.0.1", .ProxyPort = 1U };
		}
#endif

		/// \brief Describes whether this build offers the signed-out preview.
		struct public_preview_capability
		{
			bool Available = false;
		};

		// BPR-0316 enables macOS after its native candidate proof. Linux and
		// Windows retain their verified unavailable selection; enabling either
		// requires its own target-native acceptance slice.
		[[nodiscard]] constexpr auto shipped_public_preview_capability() noexcept
			-> public_preview_capability
		{
#if defined(__APPLE__)
			return { .Available = true };
#elif defined(__linux__)
			return { .Available = false };
#elif defined(_WIN32)
			return { .Available = false };
#else
			return { .Available = false };
#endif
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

		[[nodiscard]] auto initial_device_authorization_state()
			-> core::device_authorization_state
		{
			return {
				.ApplicationSession = core::application_session_id{ 1U },
				.NextRequest        = core::request_id{ 1U },
			};
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
			auto departed = std::stop_source{ };
			{
				const auto guard = std::lock_guard{ Mutex };
				Requests.clear();
				// Swapped out under the lock and stopped outside it, so a read
				// waiting on the old token is released without holding the mutex
				// its own completion path may want.
				departed  = std::move(Departure);
				Departure = std::stop_source{ };
			}
			departed.request_stop();
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
			// A real token, where this used to pass a default-constructed one that
			// could never signal. `clear_comparison` stops it, so a read for a
			// comparison the reviewer has left is abandoned promptly rather than
			// only when its bound elapses. BPR-0390.
			const auto observation = Blobs.materialize(*request, departure_token());
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

		[[nodiscard]] auto departure_token() -> std::stop_token
		{
			const auto guard = std::lock_guard{ Mutex };
			return Departure.get_token();
		}

		application::diff_blob_port&                Blobs;
		std::mutex                                  Mutex;
		std::vector<application::diff_blob_request> Requests;
		// Stopped when the reviewer leaves the comparison. One source per
		// selection rather than one per read: every outstanding read for a
		// departed comparison should end, and they end together.
		std::stop_source Departure;
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

	/// \brief Names one core promotion refusal for a reviewer to read.
	///
	/// \details Presentation wording lives at this boundary because the core
	/// publishes none: `core::agent_finding_promotion_refusal` is a decision,
	/// and how it is worded to a reviewer is not the core's business. Every
	/// declared enumerator has a case, so a refusal added later fails to
	/// compile here rather than reaching the reviewer as an empty string.
	[[nodiscard]] constexpr auto promotion_refusal_name(
		const core::agent_finding_promotion_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case core::agent_finding_promotion_refusal::none:
			return "none";
		case core::agent_finding_promotion_refusal::unknown_choice:
			return "unknown_choice";
		case core::agent_finding_promotion_refusal::dismissed:
			return "dismissed";
		case core::agent_finding_promotion_refusal::unanchored:
			return "unanchored";
		case core::agent_finding_promotion_refusal::stale:
			return "stale";
		case core::agent_finding_promotion_refusal::foreign_verdict:
			return "foreign_verdict";
		case core::agent_finding_promotion_refusal::malformed_finding:
			return "malformed_finding";
		case core::agent_finding_promotion_refusal::unnamed_target:
			return "unnamed_target";
		case core::agent_finding_promotion_refusal::anchor_elsewhere:
			return "anchor_elsewhere";
		case core::agent_finding_promotion_refusal::boundary_anchor:
			return "boundary_anchor";
		case core::agent_finding_promotion_refusal::no_suggested_comment:
			return "no_suggested_comment";
		}
		return "unknown";
	}

	/// \brief The Qt action a composed finding gesture asks the host for.
	[[nodiscard]] constexpr auto composed_finding_action(
		const qt::finding_queue_action aAction) noexcept
		-> application::agent_finding_action
	{
		switch (aAction) {
		case qt::finding_queue_action::open_evidence:
			return application::agent_finding_action::open_evidence;
		case qt::finding_queue_action::ask_follow_up:
			return application::agent_finding_action::ask_follow_up;
		case qt::finding_queue_action::convert_to_draft_comment:
			return application::agent_finding_action::convert_to_draft_comment;
		case qt::finding_queue_action::convert_to_draft_defect:
			return application::agent_finding_action::convert_to_draft_defect;
		case qt::finding_queue_action::add_to_private_notes:
			return application::agent_finding_action::add_to_private_notes;
		case qt::finding_queue_action::dismiss:
			return application::agent_finding_action::dismiss;
		}
		return application::agent_finding_action::open_evidence;
	}

	static_assert(
		qt::finding_queue_action_count == application::agent_finding_action_count,
		"the composed finding vocabulary and the queue's must stay one list");

	/// \brief Writes one frozen review projection the MCP helper may serve.
	///
	/// \details The same values the invocation request freezes for anchor
	/// validation, written once so the helper answers about exactly what the
	/// agent was asked about. The layout is the one
	/// `adapters::mcp::mcp_context_helper` reads: a `binding` line, an
	/// `alignment` record, and a `files` directory the helper resolves inside.
	[[nodiscard]] auto freeze_mcp_snapshot(
		const std::filesystem::path&                          aRoot,
		const std::string&                                    aBinding,
		const std::vector<core::agent_finding_snapshot_file>& aFiles) -> bool
	{
		auto error = std::error_code{ };
		std::filesystem::remove_all(aRoot, error);
		std::filesystem::create_directories(aRoot / "files", error);
		if (error) {
			return false;
		}
		const auto hex = [](const auto& aIdentity) {
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto           text   = std::string{ };
			for (const auto byte : aIdentity.bytes()) {
				text.push_back(digits[static_cast<std::size_t>(byte) >> 4U]);
				text.push_back(digits[static_cast<std::size_t>(byte) & 0x0FU]);
			}
			return text;
		};
		auto binding = std::ofstream{ aRoot / "binding", std::ios::binary };
		binding << aBinding << '\n';
		auto alignment = std::ofstream{ aRoot / "alignment", std::ios::binary };
		for (const auto& file : aFiles) {
			alignment << "file\t" << file.Path << '\n'
					  << "comparison\t" << hex(file.Comparison) << '\n'
					  << "left_line_count\t" << file.LeftLineCount << '\n'
					  << "right_line_count\t" << file.RightLineCount << '\n';
			for (const auto& segment : file.Segments) {
				alignment << "segment\t" << hex(segment) << '\n';
			}
			for (const auto& group : file.Groups) {
				alignment << "group\t" << hex(group) << '\n';
			}
			for (const auto& move : file.Moves) {
				alignment << "move\t" << hex(move) << '\n';
			}
		}
		return binding.good() && alignment.good();
	}

	/// \brief Carries every invocation projection onward, findings included.
	///
	/// \details The launch panel stays the invocation surface it already is;
	/// this adds one edge, from the host's typed finding delivery to the
	/// composed finding host. It is deliberately the *only* new edge out of the
	/// agent host: there is no path from here to the review-mutation
	/// coordinator, and RFC-BPR-0012's "no private backchannel" is that absence
	/// rather than a check somewhere downstream.
	class composition_agent_invocation_projection final
		: public application::agent_invocation_projection_port
	{
	public:
		using finding_sink   = std::function<void(
			const application::agent_invocation_projection_identity&,
			const application::agent_finding_delivery&)>;
		using session_sink   = std::function<void(const core::agent_session&)>;
		using workspace_sink = std::function<void(const application::prepared_workspace&)>;
		/// \brief BPR-0538. One turn, ended, with how it ended.
		///
		/// \details A press learns that its turn is over from the turn itself. A poll
		/// would have to decide how often to ask and would still be wrong
		/// between two asks.
		using run_sink = std::function<void(
			const application::agent_invocation_projection_identity&,
			const application::agent_run_result&)>;

		composition_agent_invocation_projection(
			application::agent_invocation_projection_port& aPanel,
			finding_sink                                   aFindings,
			session_sink                                   aSessions,
			run_sink                                       aRuns,
			workspace_sink                                 aWorkspaces)
			: Panel{ aPanel }
			, Findings{ std::move(aFindings) }
			, Sessions{ std::move(aSessions) }
			, Runs{ std::move(aRuns) }
			, Workspaces{ std::move(aWorkspaces) }
		{
		}

		void offer(const std::span<const core::agent_profile> aProfiles) override
		{
			Panel.offer(aProfiles);
		}

		void refuse(const std::string_view aReason) override
		{
			Panel.refuse(aReason);
		}

		void admit(
			const application::agent_invocation_projection_identity& aIdentity)
			override
		{
			Panel.admit(aIdentity);
			// The one place an agent session identity is allocated. The chat
			// host learns the session its turn actually runs under from here
			// rather than keeping one of its own, so the delivery this run
			// produces is keyed to the session the chat is holding.
			if (Sessions) {
				Sessions(aIdentity.Session);
			}
		}

		void start(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::prepared_workspace&                   aWorkspace) override
		{
			Panel.start(aIdentity, aWorkspace);
			if (Workspaces) {
				Workspaces(aWorkspace);
			}
		}

		void stream(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_stream_event&                   aEvent) override
		{
			Panel.stream(aIdentity, aEvent);
		}

		void finish(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_run_result&                     aResult) override
		{
			Panel.finish(aIdentity, aResult);
			// The one place a turn is known to be over. A press taken out on
			// this turn is answered by now or is never going to be, and either
			// way the turn it was waiting for has stopped running.
			if (Runs) {
				Runs(aIdentity, aResult);
			}
		}

		void deliver_findings(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_finding_delivery&               aDelivery) override
		{
			Panel.deliver_findings(aIdentity, aDelivery);
			if (Findings) {
				Findings(aIdentity, aDelivery);
			}
		}

		void present_session_catalog(
			const application::agent_session_catalog_projection& aProjection)
			override
		{
			Panel.present_session_catalog(aProjection);
		}

	private:
		application::agent_invocation_projection_port& Panel;
		finding_sink                                   Findings;
		session_sink                                   Sessions;
		run_sink                                       Runs;
		workspace_sink                                 Workspaces;
	};

	/// \brief Forwards chat state and chat outcomes to their surfaces.
	///
	/// \details The same shape `composition_agent_finding_projection` already
	/// uses: the host decides, this carries the decision to the widget that
	/// shows it. There is no branch here that starts anything.
	class composition_agent_chat_projection final
		: public application::agent_chat_projection_port
	{
	public:
		using state_sink
			= std::function<void(const application::agent_chat_projection&)>;
		using outcome_sink
			= std::function<void(const application::agent_chat_outcome&)>;
		using clear_sink = std::function<void()>;

		composition_agent_chat_projection(
			state_sink aState, outcome_sink aOutcome, clear_sink aClear)
			: State{ std::move(aState) }
			, Outcome{ std::move(aOutcome) }
			, Clear{ std::move(aClear) }
		{
		}

		void present(
			const application::agent_chat_projection& aProjection) override
		{
			if (State) {
				State(aProjection);
			}
		}

		void present(const application::agent_chat_outcome& aOutcome) override
		{
			if (Outcome) {
				Outcome(aOutcome);
			}
		}

		void clear() override
		{
			if (Clear) {
				Clear();
			}
		}

	private:
		state_sink   State;
		outcome_sink Outcome;
		clear_sink   Clear;
	};

	/// \brief Forwards finding deliveries and outcomes to their surfaces.
	class composition_agent_finding_projection final
		: public application::agent_finding_projection_port
	{
	public:
		using delivery_sink
			= std::function<void(const application::agent_finding_delivery&)>;
		using outcome_sink
			= std::function<void(const application::agent_finding_outcome&)>;
		using clear_sink = std::function<void()>;

		composition_agent_finding_projection(
			delivery_sink aDelivered, outcome_sink aAnswered, clear_sink aCleared)
			: Delivered{ std::move(aDelivered) }
			, Answered{ std::move(aAnswered) }
			, Cleared{ std::move(aCleared) }
		{
		}

		void present(const application::agent_finding_delivery& aDelivery) override
		{
			if (Delivered) {
				Delivered(aDelivery);
			}
		}

		void present(const application::agent_finding_outcome& aOutcome) override
		{
			if (Answered) {
				Answered(aOutcome);
			}
		}

		void clear() override
		{
			if (Cleared) {
				Cleared();
			}
		}

	private:
		delivery_sink Delivered;
		outcome_sink  Answered;
		clear_sink    Cleared;
	};

	/// \brief One comment-workshop press, and the turn it started.
	///
	/// \details BPR-0533. The reviewer's question, held from the press until
	/// the answer to *that* turn arrives. `Draft` and `Operation` are what was
	/// asked and about what; `Session` is which agent turn was started to
	/// answer it, learned from the one place a session identity is allocated
	/// and absent only in the window between the dispatch and that admission.
	///
	/// The session is what makes the pairing explicit. Without it, the next
	/// revision to arrive from anywhere — a finding a reviewer promoted while
	/// the workshop turn was still running — would be labelled with an
	/// operation that never produced it, and the reviewer would read a
	/// before/after answering a question nobody asked about that text.
	struct pending_comment_workshop
	{
		core::review_draft_id                 Draft;
		core::workshop_operation              Operation = core::workshop_operation::clarify;
		std::optional<core::agent_session_id> Session;

		/// BPR-0538. True while the turn this press started is still running.
		/// It is what shuts the six controls, and it stops being true when
		/// that turn ends — not when its answer is converted, which may never
		/// happen. A record that outlives its turn still names which operation
		/// the outcome came from; it no longer claims a turn is running.
		bool Running = true;

		/// True once that turn has delivered a finding of its own: something
		/// the reviewer can still convert into the revision this press asked
		/// for. A turn that ends without one has produced no revision and none
		/// is coming, which is the whole of what this flag decides.
		bool Delivered = false;

		auto operator<=>(const pending_comment_workshop&) const = default;
	};

	/// \brief Forwards one proposed revision, or the reason there is none.
	class composition_comment_workshop_projection final
		: public application::agent_comment_workshop_projection_port
	{
	public:
		using proposal_sink = std::function<void(
			const application::agent_comment_workshop_proposal&)>;
		using refusal_sink
			= std::function<void(application::agent_comment_workshop_refusal)>;

		composition_comment_workshop_projection(
			proposal_sink aProposed, refusal_sink aRefused)
			: Proposed{ std::move(aProposed) }
			, Refused{ std::move(aRefused) }
		{
		}

		void present(
			const application::agent_comment_workshop_proposal& aProposal) override
		{
			if (Proposed) {
				Proposed(aProposal);
			}
		}

		void refuse(
			const application::agent_comment_workshop_refusal aRefusal) override
		{
			if (Refused) {
				Refused(aRefusal);
			}
		}

	private:
		proposal_sink Proposed;
		refusal_sink  Refused;
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

#if defined(BPR_BUILD_TESTING)
	// How many renewals the refresh course means to drive, which is how many
	// waits the clock below is allowed to skip. Two, because one rotation
	// succeeds whether or not the rotation keeps the material it stored, and
	// only the second says the renewal chain survived the first.
	constexpr auto device_refresh_course_rotations = std::size_t{ 2U };

	// How far one of those steps moves the clock: the whole wait a renewal is
	// armed for, less one second. The wait is the fixture grant's eight-hour
	// access-token lifetime less the host's default five-minute renewal lead,
	// so a step of that size exactly leaves the renewal due a second from now.
	//
	// [[BPR-0547]]. The second is deliberate and is the difference between
	// testing the lifecycle and timing it. A rotation enrolls its new access
	// token and arms the next renewal in the same turn, while the enrollment it
	// just made is still travelling: the profile activates, the old session is
	// invalidated, and the vault operations that follow decide what is left in
	// the refresh slot. A renewal made due in that same instant outruns all of
	// it and reads a slot nothing has had the chance to touch yet, which is a
	// race the shipped product never runs -- there the next renewal is eight
	// hours after the supersession has long settled. A second is a thousandfold
	// margin over the few posts that supersession costs, and it costs the two
	// rotations this course drives two seconds.
	//
	// If either constant it is derived from moves, this becomes an ordinary
	// step: too large and the renewal is due at once and races again, too small
	// and the course waits out its budget and says so.
	constexpr auto device_refresh_course_step = std::chrono::seconds{
		28'800 - 300 - 1,
	};

	// A one-way step forward, applied once per renewal the host arms and at
	// most `Steps` times. The port requires a monotonic reading -- a clock that
	// stepped back would let a poll outrun its interval -- so each offset
	// latches and stays rather than being added and removed around a call.
	//
	// [[BPR-0547]]. It steps per arming rather than once, because a course that
	// proves renewal works more than once needs a second wait skipped and the
	// second one is armed by the host itself rather than by composition. Every
	// re-arm runs the same three steps in one turn: capture `AuthorizedAt` from
	// this clock, raise the rotation count, then read this clock again to work
	// out how much of the new lifetime is already spent. So the armings this
	// composition can count -- the first bind, and every rotation after it --
	// each fall strictly inside that window, and the offset that lands there
	// makes the second read a whole lifetime past the capture beside it: the
	// renewal is due now. Stepping on every rotation forever would rotate for
	// as long as the run lasts and no count could be pinned, so the steps are
	// capped at what the course drives; the arming after the last one keeps its
	// real eight hours, which is what leaves the run reporting `scheduled`
	// instead of being read mid-rotation.
	class composition_device_refresh_clock final
		: public application::device_authorization_clock_port
	{
	public:
		composition_device_refresh_clock(
			std::unique_ptr<application::device_authorization_clock_port> aInner,
			std::function<std::size_t()>                                  aArmings,
			const std::chrono::milliseconds                               aOffset,
			const std::size_t                                             aSteps)
			: Inner{ std::move(aInner) }
			, Armings{ std::move(aArmings) }
			, Offset{ aOffset }
			, Steps{ aSteps }
		{
		}

		[[nodiscard]] auto observe_wall_seconds() noexcept -> std::int64_t override { return Inner->observe_wall_seconds(); }
		[[nodiscard]] auto observe_monotonic() noexcept
			-> std::chrono::milliseconds override
		{
			const auto observed = Inner->observe_monotonic();
			if (Armings) {
				// Never fewer steps than are already applied: the reading owes
				// its monotonicity to this clock, not to what it counts.
				Applied = std::min(std::max(Applied, Armings()), Steps);
			}
			return observed
				+ Offset * static_cast<std::chrono::milliseconds::rep>(Applied);
		}

	private:
		std::unique_ptr<application::device_authorization_clock_port> Inner;
		std::function<std::size_t()>                                  Armings;
		std::chrono::milliseconds                                     Offset{ 0 };
		std::size_t                                                   Steps   = 0U;
		std::size_t                                                   Applied = 0U;
	};
	class composition_device_wall_clock final : public application::device_authorization_clock_port
	{
	public:
		composition_device_wall_clock(std::unique_ptr<application::device_authorization_clock_port> aInner, std::int64_t aOffset)
			: Inner{ std::move(aInner) }
			, Offset{ aOffset }
		{ }
		[[nodiscard]] auto observe_monotonic() noexcept -> std::chrono::milliseconds override { return Inner->observe_monotonic(); }
		[[nodiscard]] auto observe_wall_seconds() noexcept -> std::int64_t override { return Inner->observe_wall_seconds() + Offset; }

	private:
		std::unique_ptr<application::device_authorization_clock_port> Inner;
		std::int64_t                                                  Offset;
	};

#endif

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
						.BuildConfiguration = aConfig.BuildConfiguration,
						.SupervisorSha256   = aConfig.AttendedSupervisorSha256,
						.InventorySha256    = aConfig.AttendedInventorySha256,
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
			// [[BPR-0515]]. Adopted outside the fixture branch below, because
			// the gate this exists for selects no fixture: it drives the
			// shipped composition and only wants the connect denied.
			NetworkDenied = aConfig.NetworkDenied;
			const auto diffComputeLimits = aFixture.has_value()
				? aFixtureOptions.DiffComputeLimits : core::diff_compute_limits{ };
			// [[BPR-0316]]. Read outside the fixture branch below, because
			// this is the one option that means something without a fixture:
			// it forces the shipped selection available over the production
			// composition rather than over recorded traffic.
			PublicPreviewSelectionAvailable
				= aFixtureOptions.PublicPreviewSelectionAvailable
				&& !aFixture.has_value();
			if (aFixture.has_value()) {
				ReviewSubmissionServer = aFixtureOptions.ReviewSubmissionServer ? aFixtureOptions.ReviewSubmissionServer : std::make_shared<detail::review_submission_fixture_state>();
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
				DisableAgentProcessLauncherComposition
					= aFixtureOptions.DisableAgentProcessLauncherComposition;
				DisableAgentHostComposition
					= aFixtureOptions.DisableAgentHostComposition;
				DisableAgentGestureWire
					= aFixtureOptions.DisableAgentGestureWire;
				DisableAgentFindingHostComposition
					= aFixtureOptions.DisableAgentFindingHostComposition;
				DisableChatSessionHostComposition
					= aFixtureOptions.DisableChatSessionHostComposition;
				DisableCommentWorkshopComposition
					= aFixtureOptions.DisableCommentWorkshopComposition;
				DisableMcpHelperComposition
					= aFixtureOptions.DisableMcpHelperComposition;
				DisableSideChatPanelComposition
					= aFixtureOptions.DisableSideChatPanelComposition;
				DisableFindingQueueComposition
					= aFixtureOptions.DisableFindingQueueComposition;
				ExternalAgentExecutable
					= aFixtureOptions.ExternalAgentExecutable;
				CodexAgentExecutable    = aFixtureOptions.CodexAgentExecutable;
				RegisteredDeviceCourse  = aFixtureOptions.RegisteredDeviceCourse;
				DeviceRefreshCourse     = aFixtureOptions.DeviceRefreshCourse;
				RememberDeviceSignIn    = aFixtureOptions.PersistDeviceBindings;
				DeviceWallOffsetSeconds = aFixtureOptions.DeviceWallOffsetSeconds;
				SuppressDeviceBrowser   = aFixtureOptions.SuppressDeviceBrowser;
				DisableDeviceAuthorizationPanelComposition
					= aFixtureOptions
						  .DisableDeviceAuthorizationPanelComposition;
				PublicPreviewEnabledCourse
					= aFixtureOptions.PublicPreviewEnabledCourse;
				PublicPreviewGhesCourse = aFixtureOptions.PublicPreviewGhesCourse;
				const auto host         = fixture_host(*aFixture);
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
			DiagnosticLog       = aConfig.DiagnosticLog;
			if (!DiagnosticLog) {
				try {
					OwnedDiagnosticLog = std::make_shared<adapters::logging::quill_log>(applicationDataRoot);
					DiagnosticLog      = OwnedDiagnosticLog;
				} catch (...) { /* Diagnostics allocation must not prevent application startup. */
				}
			}
			if (DiagnosticLog) {
				static_cast<void>(DiagnosticLog->record({ .Operation = application::diagnostic_operation::application, .Outcome = application::diagnostic_outcome::started }));
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
				TransportFactory, adapters::github::detail::github_provider_read_authority::authenticated, DiagnosticLog);
			if (!provider.has_value()) {
				throw std::runtime_error{ "GitHub provider composition failed" };
			}
			Provider = std::move(*provider);

			// RFC-BPR-0017's credential-free read authority, over the same
			// profiles and the same transport factory. A second provider
			// rather than a mode on the first one: the authority is fixed at
			// construction, so no request can decay from authenticated into
			// anonymous or back, and the GHES, viewer-identity, GraphQL and
			// mutation refusals happen inside it before a request is built.
			//
			// Constructed in both profiles and on every platform, including
			// the ones whose selection is unavailable. Construction sends
			// nothing; what the selection below decides is whether the host is
			// ever given this authority at all.
			//
			// [[BPR-0316]]. The live candidate replaces the factory for this
			// second provider alone, with one that names the shipped
			// anonymous entry, counts what reaches the wire, and refuses both
			// credentialed entries. The authenticated provider above keeps the
			// factory it had, so this changes what the signed-out authority is
			// built from and nothing else. What the anonymous provider was
			// actually constructed with is kept below, so the evidence is a
			// pointer identity rather than a claim about a call site.
			AnonymousTransportFactory = TransportFactory;
#if defined(BPR_BUILD_TESTING)
			if (PublicPreviewSelectionAvailable) {
				ShippedAnonymousTransports
					= std::make_shared<shipped_anonymous_wire_transport_factory>();
				AnonymousTransportFactory = ShippedAnonymousTransports;
			}
#endif
			auto anonymousProvider = make_github_provider(
				std::span<const validated_github_host_profile* const>{
					profilePointers },
				ProviderEffects,
				AnonymousTransportFactory,
				adapters::github::detail::github_provider_read_authority::
					anonymous_public);
			if (!anonymousProvider.has_value()) {
				throw std::runtime_error{
					"Anonymous GitHub provider composition failed"
				};
			}
			AnonymousProvider = std::move(*anonymousProvider);
			CountedAnonymousPullRequests
				= std::make_unique<counting_anonymous_pull_requests>(
					*AnonymousProvider);
			// One profile carries the signed-out mode, and it is the
			// GitHub.com one: `make_native_https_anonymous_smart_transport`
			// refuses every other server contract before a transport exists.
			AnonymousSmartGit
				= std::make_unique<anonymous_git_smart_transport_factory>(
					Profiles.front());

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

			auto cache = adapters::git::make_installed_git_cache({
				.GitExecutable     = gitExecutable,
				.StorageRoot       = applicationDataRoot,
				.WindowsSystemRoot = windows_system_root(),
#if defined(Q_OS_UNIX)
				.WorkspaceOwnershipRegistry = applicationDataRoot / "agent-workspace-ownership-v1",
#endif
			});
			auto catalog = adapters::sqlite::make_epoch_catalog({
				.StorageRoot = applicationDataRoot,
			});
			// Two stores, two refusals; a developer build reading
			// STARTUP_FAILED_CAUSE needs to know which one (BPR-0436).
			if (!cache) {
				throw std::runtime_error{ "Git cache composition failed" };
			}
			if (!catalog) {
				throw std::runtime_error{
					"epoch catalog composition failed ("
					+ std::to_string(static_cast<unsigned int>(catalog.error())) + ")"
				};
			}
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
#if defined(BPR_BUILD_TESTING)
			ReviewSubmissionDecisionGate = std::make_unique<gated_review_mutation_decision_executor>(DecisionLane);
			auto& reviewDecisions        = *ReviewSubmissionDecisionGate;
#else
			auto& reviewDecisions = DecisionLane;
#endif
#if defined(BPR_BUILD_TESTING)
			review_submission_workflow::grant_factory issueReviewGrant = [this](auto aSession, auto aGeneration, const auto& aRequest) {
				return Fixture ? application::make_review_submission_testing_grant(aSession, aGeneration, aRequest) : application::review_submission_execution_grant{ };
			};
#else
			review_submission_workflow::grant_factory issueReviewGrant = application::detail::review_submission_user_authority_issuer::issue;
#endif
			ReviewSubmission = std::make_unique<review_submission_workflow>(ReviewSubmissionEffects, reviewDecisions,
																			*Window->review_submission_editor_view(), std::move(issueReviewGrant));
			Window->set_review_submission_dispatch([this](qt::review_submission_editor_intent aIntent) {
				if (!Closing && ReviewSubmission) {
					ReviewSubmission->dispatch(std::move(aIntent));
				}
			});
			Window->set_review_draft_dispatch(
				[this](core::review_draft_command aCommand) {
					dispatch_review_draft(std::move(aCommand));
				});
			// BPR-0533. The six comment-workshop controls reach the composed
			// workshop host and nothing else: the editor asks for an operation
			// on one exact draft, and this composition is what decides whether
			// there is anything to answer with. The widget holds no host, no
			// agent, and no revision of its own.
			if (Window->review_editor() != nullptr) {
				Window->review_editor()->set_workshop_dispatch(
					[this](qt::comment_workshop_request aRequest) {
						ask_comment_workshop(aRequest);
					});
			}
			Window->comparison_surface()->set_selection_sink(
				[this](const std::optional<core::semantic_selection>& aSelection) {
					observe_review_selection(aSelection);
				});
			Window->set_defect_dispatch(
				[this](core::defect_command aCommand) {
					dispatch_defect(std::move(aCommand));
				});
			Window->set_defect_classification_dispatch(
				[this](qt::workbench_defect_classification aClassification) {
					classify_defect(std::move(aClassification));
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
				*this, application::epoch_host_limits{ }, DiagnosticLog);

#if defined(Q_OS_UNIX)
			// Persistent private neighbourhood on the cache filesystem, outside
			// its isolation root. Only effect workers create or recover children.
			const auto agentWorkspacePath = ApplicationDataRoot.parent_path()
				/ (ApplicationDataRoot.filename().string() + "-agent-workspaces-v1");
#else
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
#endif
			AgentWorkspaceNeighbourhood = agentWorkspacePath;
			// The one new edge out of the agent host. The launch panel still
			// receives every projection it already did; this adds the typed
			// finding delivery, and nothing else. There is deliberately no
			// path from here to the review-mutation coordinator: RFC-BPR-0012's
			// "no private backchannel" is that absence rather than a check
			// somewhere downstream.
			AgentProjections
				= std::make_unique<composition_agent_invocation_projection>(
					*Window->agent_panel(),
					[this](
						const application::
							agent_invocation_projection_identity&  aIdentity,
						const application::agent_finding_delivery& aDelivery) {
						static_cast<void>(aIdentity);
						if (Closing) {
							return;
						}
						if (FindingHost != nullptr) {
							FindingHost->deliver(aDelivery);
						}
						// The chat's reply is this same validated value, at
						// this same seam. There is no second delivery edge for
						// a chat turn, so nothing a chat receives can bypass
						// the finding bounds this one enforces.
						if (ChatHost != nullptr) {
							ChatHost->deliver(aDelivery);
						}
						// BPR-0538. Whether the turn a press is waiting on has
						// produced anything the reviewer could still convert.
						// Recorded from the delivery rather than inferred at
						// the terminal, because by then the payload is gone.
						if (PendingWorkshopOperation.has_value()
							&& PendingWorkshopOperation->Session.has_value()
							&& *PendingWorkshopOperation->Session
								== aDelivery.Session
							&& !aDelivery.Findings.empty()) {
							PendingWorkshopOperation->Delivered = true;
						}
					},
					[this](const core::agent_session& aSession) {
						if (Closing) {
							return;
						}
						if (ChatHost != nullptr) {
							ChatHost->admitted(aSession);
						}
						// The same seam, for the same reason. A workshop press
						// learns which turn is answering it here rather than
						// keeping an identity of its own, so the outcome the
						// operation is spent on is the outcome of the turn the
						// press actually started. Only a press still waiting
						// for a turn takes one: a session admitted for the
						// launch panel or the chat while a workshop turn is
						// already running belongs to neither.
						if (PendingWorkshopOperation.has_value()
							&& !PendingWorkshopOperation->Session.has_value()) {
							PendingWorkshopOperation->Session = aSession.Id;
						}
					},
					// BPR-0538. The third sink, and the one this composition
					// was missing: the turn a press started has ended. Until
					// it existed the six controls stayed shut behind "a
					// workshop turn is already running" long after the turn
					// stopped running, and only an unrelated draft edit
					// reopened them.
					[this](
						const application::agent_invocation_projection_identity&
															 aIdentity,
						const application::agent_run_result& aResult) {
						if (Closing) {
							return;
						}
						settle_comment_workshop(
							aIdentity.Session.Id, aResult.Outcome);
					},
					[this](const application::prepared_workspace& aWorkspace) {
						LastAgentWorkspacePath = aWorkspace.Path;
					});
			auto agentProfiles = core::built_in_agent_profiles();
			// Stated once and given to both hosts. The chat host asks
			// `core::decide_agent_action` the same host-policy question the
			// launch host already asks, and two lists written apart are two
			// answers waiting to differ.
			const auto agentPolicies = std::vector<core::agent_host_policy>{
				core::built_in_agent_policy(
					core::git_host_id{ 1U },
					core::built_in_agent_host_kind::github_com),
				core::built_in_agent_policy(
					core::git_host_id{ 2U },
					core::built_in_agent_host_kind::explicit_ghes),
			};
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
			if (CodexAgentExecutable.has_value()) {
				const auto codex = std::ranges::find(
					agentProfiles,
					core::codex_jsonl_profile_id,
					&core::agent_profile::Id);
				if (codex != agentProfiles.end()) {
					codex->Executable = CodexAgentExecutable->string();
				}
			}
#endif
#if defined(BPR_BUILD_TESTING)
			if (!DisableAgentProcessLauncherComposition) {
#endif
				AgentProcess
					= std::make_unique<adapters::agent::agent_process_launcher>();
#if defined(BPR_BUILD_TESTING)
			}
			if (AgentProcess != nullptr) {
				AgentProcessProbe
					= std::make_unique<composition_agent_process_launcher_probe>(
						*AgentProcess);
			}
			if (!DisableAgentHostComposition && AgentProcessProbe != nullptr)
#endif
			{
				// BPR-0532. The re-alignment a continued turn asks for, served
				// by the same two ports a fresh alignment resolves and
				// materializes through, so one revision cannot be aligned two
				// ways. Composed with the chat host, because it is only a
				// continued turn of that host's that ever asks for one.
				Realigner
					= std::make_unique<application::composed_agent_realignment>(
						*EpochHost, *EpochHost);
				AgentHost = std::make_unique<application::agent_invocation_host>(
					agentProfiles,
					agentPolicies,
					agentWorkspacePath,
					*this,
					*EpochHost,
#if defined(BPR_BUILD_TESTING)
					*AgentProcessProbe,
#else
					*AgentProcess,
#endif
					[](const std::string& aName) -> std::optional<std::string> {
						if (!qEnvironmentVariableIsSet(aName.c_str())) {
							return std::nullopt;
						}
						return qEnvironmentVariable(aName.c_str()).toStdString();
					},
					AgentEffects,
					DecisionLane,
					*AgentProjections);
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
			// RFC-BPR-0012 composition, here rather than anywhere else and
			// under the same fixture and production split the agent host and
			// the review-mutation host already use. A surface built in one of
			// the two graphs is a claim about the other one nobody checks.
			auto mcpHelper = std::filesystem::path{ };
#if defined(BPR_BUILD_TESTING)
			mcpHelper = std::filesystem::path{ BPR_MCP_HELPER_EXECUTABLE };
#else
			mcpHelper = std::filesystem::path{
				QCoreApplication::applicationDirPath().toStdString()
			} / "tool_better_pull_review_mcp_context_helper";
#endif
			// Its own launcher rather than the agent's. `cancel` is a launcher
			// operation, and one shared between an agent run and a helper
			// lifetime would stop whichever the reviewer did not mean.
			McpProcess      = std::make_unique<adapters::agent::agent_process_launcher>();
			McpSnapshotRoot = ApplicationDataRoot / "mcp-snapshot";
#if defined(BPR_BUILD_TESTING)
			if (!DisableMcpHelperComposition)
#endif
			{
				McpHelper = std::make_unique<
					application::mcp_context_helper_supervisor>(
					std::move(mcpHelper),
					*McpProcess,
					McpEffects,
					DecisionLane);
			}
#if defined(BPR_BUILD_TESTING)
			if (!DisableSideChatPanelComposition)
#endif
			{
				// BPR-0513: the window places both panels in its conversations
				// drawer, so the composition here is the one edge that matters
				// — the dispatch. A panel this branch skips stays on screen and
				// stays inert, because a widget nobody handed a dispatch to has
				// nowhere to send anything.
				SideChat = Window->side_chat();
				SideChat->set_dispatch([this](qt::side_chat_intent aIntent) {
					dispatch_side_chat_intent(std::move(aIntent));
				});
			}
#if defined(BPR_BUILD_TESTING)
			if (!DisableFindingQueueComposition)
#endif
			{
				FindingQueue = Window->finding_queue();
				FindingQueue->set_dispatch(
					[this](qt::finding_action_intent aIntent) {
						dispatch_finding_intent(std::move(aIntent));
					});
			}
#if defined(BPR_BUILD_TESTING)
			if (!DisableDeviceAuthorizationPanelComposition)
#endif
			{
				// [[BPR-0526]], on exactly the terms above. The window mounted
				// the panel in its review header; the composition adopts it and
				// wires its two edges — gestures out through the device host,
				// presentations in through the fan-out below, which reads
				// `DevicePanel` on every publication. Skipping this branch
				// leaves a panel that is drawn, is inert, and is never told
				// anything.
				DevicePanel = Window->device_authorization();
				Window->set_device_authorization_dispatch(
					[this](application::device_authorization_gesture aGesture) {
						dispatch_device_authorization_gesture(aGesture);
					});
			}
			FindingProjections
				= std::make_unique<composition_agent_finding_projection>(
					[this](const application::agent_finding_delivery& aDelivery) {
						if (FindingQueue != nullptr) {
							FindingQueue->present(aDelivery);
						}
					},
					[this](const application::agent_finding_outcome& aOutcome) {
						present_finding_outcome(aOutcome);
					},
					[this]() {
						if (FindingQueue != nullptr) {
							FindingQueue->present(
								application::agent_finding_delivery{ });
						}
					});
			WorkshopProjections
				= std::make_unique<composition_comment_workshop_projection>(
					[this](
						const application::agent_comment_workshop_proposal&
							aProposal) {
						if (Window == nullptr
							|| Window->review_editor() == nullptr) {
							return;
						}
						++CommentWorkshopProposals;
						Window->review_editor()->present(
							qt::comment_workshop_proposal{
								.Draft     = aProposal.Draft,
								.Original  = aProposal.Original,
								.Proposed  = aProposal.Proposed,
								.Operation = aProposal.Operation,
							});
					},
					[this](
						const application::agent_comment_workshop_refusal
							aRefusal) {
						present_agent_refusal(
							"comment workshop", application::name(aRefusal));
					});
#if defined(BPR_BUILD_TESTING)
			if (!DisableCommentWorkshopComposition)
#endif
			{
				WorkshopHost = std::make_unique<
					application::agent_comment_workshop_host>(
					*WorkshopProjections);
			}
#if defined(BPR_BUILD_TESTING)
			if (!DisableAgentFindingHostComposition)
#endif
			{
				FindingHost = std::make_unique<application::agent_finding_host>(
					[this]() -> application::review_ledger_store_port* {
						return ReviewLedger.get();
					},
					*FindingProjections);
			}
			// RFC-BPR-0012's chat *session*, under the same fixture and
			// production split every other agent host here uses. The turn it
			// produces is handed to `dispatch_agent_invocation` — the one
			// launch path in this file — so a chat send and a launch-panel
			// start are the same launch, the same protocol, and the same
			// frozen snapshot findings are delivered from.
			ChatProjections
				= std::make_unique<composition_agent_chat_projection>(
					[this](const application::agent_chat_projection& aChat) {
						present_side_chat(aChat);
					},
					[this](const application::agent_chat_outcome& aOutcome) {
						present_chat_outcome(aOutcome);
					},
					[this]() {
						if (SideChat != nullptr) {
							++SideChatPresentations;
							SideChat->present(qt::side_chat_projection{ });
						}
					});
#if defined(BPR_BUILD_TESTING)
			if (!DisableChatSessionHostComposition)
#endif
			{
				ChatHost
					= std::make_unique<application::agent_chat_session_host>(
						agentProfiles,
						agentPolicies,
						core::codex_jsonl_profile_id,
						[this](const application::agent_chat_turn_request&
								   aTurn) -> bool {
#if defined(BPR_BUILD_TESTING)
							LastChatTurn = aTurn;
#endif
							// The reviewer's own words, from the send intent
							// the chat host validated, handed to the same
							// launch path a start gesture uses. This is the
							// only call site that passes a message.
							return dispatch_agent_invocation(
								aTurn.Session.Profile,
								aTurn.IncludeNotes
									? all_private_note_ids()
									: std::vector<application::local_note_id>{ },
								aTurn.IncludeDraft ? current_agent_draft_choice()
												   : std::nullopt,
								false,
								aTurn.Message,
								aTurn.Session,
								core::agent_entry_point::ask_about_selection,
								std::nullopt);
						},
						[this]() -> application::agent_chat_private_prose {
							return current_chat_private_prose();
						},
						*ChatProjections);
			}
			Window->epoch_selectors()->set_dispatch(
				[this](application::epoch_intent aIntent) { dispatch_epoch_intent(std::move(aIntent)); });
			DiffProjections = std::make_unique<composition_diff_projection>(
				*Window->comparison_surface(),
				[this](const application::diff_publication& aPublication) {
					if (SelectedDiffPublication.has_value()) {
						Window->set_conversation_endpoint_placement(
							false,
							conversation_endpoint_is_current(
								SelectedDiffPublication->Request.Diff.Comparison,
								core::diff_side::right));
					}
					// Before the ledger observation, because it is about the
					// panes rather than about the review, and because
					// `observe_selected_material` returns early for reasons
					// that have nothing to do with whether the comparison on
					// screen is coloured.
					adopt_published_classification(aPublication);
					observe_selected_material(aPublication);
					correlate_defects();
				});
			DiffHost = std::make_unique<application::diff_application_host>(
				core::diff_state{
					.ApplicationSession = core::application_session_id{ 1U },
				},
				*EpochHost,
				DiffEffects,
				DecisionLane,
				*DiffProjections
#if defined(BPR_BUILD_TESTING)
				, application::diff_host_limits{ }, diffComputeLimits
#endif
			);

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

			// One port, two readers, exactly as the device projection below:
			// the recording sink is told first and unconditionally, so every
			// self-test marker reads what it read before a window was in the
			// picture, and the window is told the same thing afterwards.
			//
			// Composed on every platform, registered or not. The signed-out
			// mode being unavailable is a typed answer a reviewer must see,
			// not a silence, so the projection is bound even where the
			// authorities below are withheld.
			PublicPreviewProjectionFanOut = std::make_unique<
				composition_public_preview_projection>(
				PublicPreviewProjections,
				[this](const application::public_preview_presentation&
						   aPresentation) {
					if (Window == nullptr) {
						return;
					}
					Window->set_public_preview_now(public_preview_now());
					Window->publish(aPresentation);
				});
			// A platform whose shipped selection is unavailable supplies no
			// anonymous authority, so the host refuses before any request.
			auto publicPreview = shipped_public_preview_capability();
#if defined(BPR_BUILD_TESTING)
			// The forced courses. A tests-enabled build with the recorded
			// fixture overrides the shipped selection so the composed
			// anonymous authority runs through the real host, the real
			// provider and the real anonymous admission rules, which is the
			// only place "no header, no vault, and a public snapshot" can be
			// observed rather than argued.
			if ((PublicPreviewEnabledCourse || PublicPreviewGhesCourse)
				&& Fixture != nullptr) {
				publicPreview = public_preview_capability{ .Available = true };
			}
			// [[BPR-0316]]. The same override with no fixture underneath it:
			// the authority handed to the host is the production one, so a
			// read issued through it leaves the machine. Nothing here issues
			// one; the run driving the composition decides that.
			if (PublicPreviewSelectionAvailable && Fixture == nullptr) {
				publicPreview = public_preview_capability{ .Available = true };
			}
#endif
			PublicPreviewComposed = publicPreview.Available;
			// The session fan-out. The window keeps its place at the front, and
			// composition reads acceptances behind it so the device host's
			// refresh authority can be bound the moment one exists.
			GithubProjectionFanOut = std::make_unique<composition_github_projection>(
				*Window,
				[this](const application::github_presentation_update& aUpdate) {
					bind_device_refresh_authority(aUpdate);
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
				*GithubProjectionFanOut,
				RememberDeviceSignIn ? application::github_credential_shutdown_policy::retain_accepted_device_grants : application::github_credential_shutdown_policy::delete_retained_on_shutdown,
				this,
				PublicPreviewComposed
					? static_cast<application::github_pull_request_port*>(
						  CountedAnonymousPullRequests.get())
					: nullptr,
				PublicPreviewComposed
					? static_cast<application::git_smart_transport_factory_port*>(
						  AnonymousSmartGit.get())
					: nullptr,
				PublicPreviewProjectionFanOut.get());
			// The switch-to-sign-in gesture, end to end. The ring in the
			// review header dispatches it, the host names the profile the
			// signed-out session was activated for, and the device flow for
			// that same profile begins -- so sign-in returns the reviewer to
			// the pull request they were reading rather than to a picker.
			Host->set_public_preview_sign_in(
				[this](const core::host_profile_id aProfile) {
					begin_public_preview_sign_in(aProfile);
				});
			if (Window->session_panel() != nullptr) {
				Window->session_panel()->set_public_preview_dispatch(
					[this](application::public_preview_gesture aGesture) {
						static_cast<void>(aGesture);
						Host->request_public_preview_sign_in();
					});
			}

			// The device-authorization path. It is composed on every platform,
			// registered or not, because the visible choice and its typed
			// answer are the product behaviour the RFC asks for: on an
			// unregistered platform selecting device sign-in must say
			// `device_flow_unavailable`, not do nothing, and the capability
			// gate refuses before a request is built.
			DeviceEnrollment = std::make_unique<
				application::github_host_device_authorization_enrollment>(*Host);
#if defined(BPR_BUILD_TESTING)
			CountedDeviceEnrollment = std::make_unique<
				counting_device_authorization_enrollment>(*DeviceEnrollment);
			auto& deviceEnrollment
				= static_cast<application::device_authorization_enrollment_port&>(
					*CountedDeviceEnrollment);
#else
			auto& deviceEnrollment
				= static_cast<application::device_authorization_enrollment_port&>(
					*DeviceEnrollment);
#endif
			DeviceClock = application::make_system_device_authorization_clock();
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr && RememberDeviceSignIn) {
				DeviceClock = std::make_unique<composition_device_wall_clock>(std::move(DeviceClock), DeviceWallOffsetSeconds);
			}
#endif

#if defined(BPR_BUILD_TESTING)
			// [[BPR-0536]]. The one input a fixture cannot supply is time.
			// A renewal is armed a lead before an eight-hour access token
			// expires, and no course can wait that out; the transport seam
			// beside this one replaces the network, and this replaces the
			// wait. Tests-enabled only, and composed only under the refresh
			// course, so a shipped image has one clock and it is the system's.
			//
			// It steps on the binding rather than on the authorized stage. The
			// host captures `AuthorizedAt` from this same clock at the moment
			// the stage turns, so an offset that stepped there would move both
			// ends of `elapsed` and change nothing. Composition's
			// `bind_refresh_authority` falls strictly between that capture and
			// the `schedule_refresh` that reads the clock again, which is the
			// only window where a one-way step shortens the wait to zero.
			//
			// [[BPR-0547]]. The host arms every renewal after the first one for
			// itself, and composition is told about it only once the timer is
			// already set -- too late to shorten anything. What it can read in
			// time is the host's own live rotation count, which the re-arm
			// raises inside that same window, between the capture and the read.
			// So the armings are the bind plus the rotations, and each one buys
			// one more step until the course has the rotations it drives.
			if (DeviceRefreshCourse && Fixture != nullptr) {
				DeviceClock = std::make_unique<composition_device_refresh_clock>(
					std::move(DeviceClock),
					[this]() noexcept -> std::size_t {
						const auto bound = DeviceRefreshBindings > 0U
							? std::size_t{ 1U }
							: std::size_t{ 0U };
						if (DeviceHost == nullptr) {
							return bound;
						}
						return bound
							+ static_cast<std::size_t>(
								   DeviceHost->refresh_observation().Rotations);
					},
					device_refresh_course_step,
					device_refresh_course_rotations);
			}
#endif
			// The shipped selection. A registered platform composes the real
			// credential-free wire bound to the recorded distributable client;
			// an unregistered platform composes the refusing transport whose
			// reached() count proves the gate held. A composition failure
			// fails closed to the refusal: a half-wired flow must answer
			// `device_flow_unavailable`, not invent one.
			auto* deviceTransport
				= static_cast<application::device_authorization_transport_port*>(
					&UnregisteredDeviceTransport);
			auto deviceCapability
				= application::device_authorization_capability{ .Registered
																= false };
			if constexpr (shipped_device_authorization_capability().Registered) {
				const auto deviceContext = core::github_session_request_context{
					.Request            = core::request_id{ 907U },
					.ApplicationSession = core::application_session_id{ 908U },
					.Profile            = Profiles.front().profile(),
					.IdentityGeneration = core::github_identity_generation{ 909U },
				};
				auto deviceWire = [this, &deviceContext]() {
#if defined(BPR_BUILD_TESTING)
					// [[BPR-0515]]. The same binding, the same credential-free
					// request, the same headers - only the proxy policy differs,
					// and it differs into a closed port. Selected by nothing but
					// `--network-denied`, which exists only here.
					if (NetworkDenied) {
						return adapters::github::detail::
							make_native_https_credential_free_transport_for_test(
								Profiles.front(),
								deviceContext,
								denied_loopback_network());
					}
#endif
					return adapters::github::detail::
						make_native_https_credential_free_transport(
							Profiles.front(), deviceContext);
				}();
				if (deviceWire.has_value()) {
					auto deviceAdapter = adapters::github::detail::
						make_github_device_authorization_transport(
							std::shared_ptr<
								adapters::github::detail::native_https_transport>{
								std::move(*deviceWire) },
							std::string{ production_device_client_id },
							DeviceTransportEffects);
					if (deviceAdapter.has_value()) {
						DeviceWireTransport = std::move(*deviceAdapter);
						deviceTransport     = DeviceWireTransport.get();
						deviceCapability
							= shipped_device_authorization_capability();
						// The identity is registered for one host, and the
						// wire above is bound to that host's profile. The
						// capability names the same profile so the host can
						// refuse a begin for any other one before it builds
						// a request ([[BPR-0535]]); the picker can bind the
						// mounted panel to a GHES profile, and this is what
						// keeps that gesture out of GitHub.com's flow.
						deviceCapability.Profile
							= Profiles.front().profile();
					}
				}
			}
#if defined(BPR_BUILD_TESTING)
			// The registered course. A tests-enabled build with the recorded
			// fixture overrides the shipped selection with BPR-0276's private
			// recorded grant: it runs through the real host, the real
			// enrollment boundary, and the real vault, which is the only place
			// "exactly one enrollment, and no settings or SQLite copy" can be
			// observed rather than argued.
			if (RegisteredDeviceCourse && Fixture != nullptr
				&& !Profiles.empty()) {
				deviceTransport  = &Fixture->device_authorization_transport();
				deviceCapability = application::device_authorization_capability{
					.Registered = true,
					// The same profile the course begins for, by the same
					// rule as the shipped selection above.
					.Profile = Profiles.front().profile(),
				};
			}
#endif
			DeviceCapabilityRegistered = deviceCapability.Registered;
			// The browser handoff, under the same seam as the transport above
			// and for the same reason: the shipped graph gets the real
			// capability, a tests-enabled graph gets a recording one. The
			// seam is total here rather than fixture-conditional, because a
			// fixture transport that is reached by mistake writes a line to a
			// log and a browser that is opened by mistake opens a browser.
			//
			// `SystemBrowser` is constructed either way. Its constructor is
			// the anchor `verify_composed_hosts.py` requires in both
			// profiles, so a build that stopped composing the opener fails
			// the audit rather than silently going back to answering
			// `unavailable` forever, which is the defect [[BPR-0516]] found.
#if defined(BPR_BUILD_TESTING)
			auto* deviceBrowser = SuppressDeviceBrowser
				? nullptr
				: static_cast<application::device_authorization_browser_port*>(
					  &RecordingBrowser);
#else
			auto* deviceBrowser
				= static_cast<application::device_authorization_browser_port*>(
					&SystemBrowser);
#endif
			DeviceBrowserComposed = deviceBrowser != nullptr;
			// One port, two readers. The recording sink keeps its place at the
			// front of the fan-out, so every existing device contract still
			// observes the identical sequence of publications; the mounted
			// panel is told the same thing afterwards when the composition
			// adopted one.
			DeviceProjectionFanOut = std::make_unique<
				composition_device_authorization_projection>(
				DeviceProjections,
				[this](
					const application::device_authorization_presentation&
						aPresentation) {
					observe_device_stage(aPresentation);
					if (DevicePanel != nullptr) {
						DevicePanel->apply(aPresentation);
					}
				},
				[this](
					const application::device_authorization_refresh_observation&
						aObservation) {
					observe_device_refresh(aObservation);
				});
			DeviceHost = std::make_unique<
				application::device_authorization_application_host>(
				initial_device_authorization_state(),
				*deviceTransport,
				credential_vault(),
				deviceEnrollment,
				*DeviceClock,
				DeviceTimer,
				DecisionLane,
				*DeviceProjectionFanOut,
				application::device_authorization_host_limits{ },
				application::device_authorization_browser_handoff{
					// Null only under the construction cut, where the host
					// reports `unavailable` rather than inventing an address.
					// The authority travels with the pointer so a profile
					// that is not GitHub.com cannot inherit GitHub.com's.
					.Browser   = deviceBrowser,
					.Authority = std::string{
						application::default_device_verification_authority },
				},
				deviceCapability);
			if (RememberDeviceSignIn) {
				DeviceBindingStore = adapters::sqlite::make_device_binding_store(applicationDataRoot);
				DevicePersistence  = std::make_unique<application::device_binding_persistence>(*DeviceBindingStore, DeviceBindingEffects, DecisionLane);
				Host->configure_device_persistence(*DevicePersistence, [this] { return DeviceClock->observe_wall_seconds(); });
				const auto& profile = Profiles.front();
				Host->restore_device_profile(application::github_profile_authority{
					.Profile = profile.profile(), .ProfileRevision = profile.revision(), .Host = profile.host(), .Hostname = application::github_canonical_hostname{ profile.hostname() }, .ServerContract = profile.server_contract(), .Authentication = profile.authentication_transport(), .ProbeContractRevision = application::github_probe_contract_revision{ 1U } });
			}

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
				// A fixture run also takes the captured boundary when the
				// platform truthfully reports no tray — the offscreen
				// platform on Windows — because the courses that leave
				// capture off assert what reaches the sink, not the native
				// display mechanics; where a tray exists, nothing changes
				// (BPR-0442). Production composition is untouched.
				if (CaptureSystemNotificationDisplay
					|| (Fixture != nullptr
						&& !QSystemTrayIcon::isSystemTrayAvailable())) {
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
#if defined(Q_OS_UNIX)
			AgentWorkspaceRecoveryPending.store(true, std::memory_order_release);
			try {
				EpochEffects.post([this, registry = ApplicationDataRoot / "agent-workspace-ownership-v1", neighbourhood = AgentWorkspaceNeighbourhood] {
					auto recovered = adapters::git::recover_installed_agent_workspaces(registry, neighbourhood);
					try {
						DecisionLane.post([this, result = std::move(recovered)] {
							AgentWorkspaceCleanupFailed = !result.has_value();
							if (!result && !Closing && Window && Window->agent_panel()) {
								Window->agent_panel()->refuse("Disposable workspace recovery needs repair or has reached its limit.");
							}
							AgentWorkspaceRecoveryPending.store(false, std::memory_order_release);
						});
					} catch (...) {
						AgentWorkspaceRecoveryPostFailed.store(true, std::memory_order_release);
						AgentWorkspaceRecoveryPending.store(false, std::memory_order_release);
					}
				});
			} catch (...) {
				AgentWorkspaceCleanupFailed = true;
				AgentWorkspaceRecoveryPending.store(false, std::memory_order_release);
			}
#endif
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

		// Registered-course evidence. Zero outside a tests-enabled build with
		// the recorded grant, because nothing else can enroll.
		[[nodiscard]] auto device_vault_stores() const noexcept -> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr && RegisteredDeviceCourse) {
				return Fixture->evidence().VaultStores;
			}
#endif
			return 0U;
		}

		// [[BPR-0539]]. What that one store holds. The count alone is
		// satisfied by any enrollment, and before this course stopped
		// connecting first the enrollment it counted was the fixture's own
		// connect credential -- so the identity of the stored material is
		// reported beside the count, and the comparison against the recorded
		// grant is made where both are known.
		[[nodiscard]] auto device_vault_stored_digest() const -> std::string
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr && RegisteredDeviceCourse) {
				return Fixture->evidence().VaultStoredDigest;
			}
#endif
			return { };
		}

		[[nodiscard]] auto device_vault_stored_is_grant() const -> bool
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr && RegisteredDeviceCourse) {
				return Fixture->evidence().VaultStoredIsDeviceGrant;
			}
#endif
			return false;
		}

		// Only the recording opener counts. The shipped opener's evidence is a
		// browser window a person sees on a station, which this process has no
		// way to observe and must not claim to.
		[[nodiscard]] auto device_browser_opens() const noexcept -> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			return RecordingBrowser.opens();
#else
			return 0U;
#endif
		}

		[[nodiscard]] auto device_enrollments() const noexcept -> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			if (CountedDeviceEnrollment != nullptr) {
				return CountedDeviceEnrollment->enrollments();
			}
#endif
			return 0U;
		}

		// RFC-BPR-0017 evidence. Three exclusive answers, read from the host
		// rather than inferred from the presentation: the composition refusal,
		// the core policy's refusal, and the mode actually entered.
		[[nodiscard]] auto public_preview_unavailable() const noexcept -> bool
		{
			if (Host == nullptr) {
				return true;
			}
			const auto& refusal = Host->public_preview_refusal();
			return refusal.has_value()
				&& refusal->Cause
				== application::github_public_preview_refusal_cause::
					public_preview_unavailable;
		}

		[[nodiscard]] auto public_preview_policy_refused() const noexcept -> bool
		{
			if (Host == nullptr) {
				return false;
			}
			const auto& refusal = Host->public_preview_refusal();
			return refusal.has_value()
				&& refusal->Cause
				== application::github_public_preview_refusal_cause::
					policy_refused;
		}

		[[nodiscard]] auto public_preview_signed_out() const noexcept -> bool
		{
			return Host != nullptr
				&& Host->access_mode()
				== core::github_access_mode::public_preview;
		}

		[[nodiscard]] auto public_preview_requests() const noexcept -> std::size_t
		{
			return CountedAnonymousPullRequests == nullptr
				? 0U
				: CountedAnonymousPullRequests->reads();
		}

		// Authenticated transports the session selected during the run. A
		// signed-out read builds neither, so any non-zero value here is an
		// authorization header this mode is not allowed to have sent.
		[[nodiscard]] auto public_preview_header_effects() const noexcept
			-> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr) {
				const auto evidence = Fixture->evidence();
				return (evidence.BootstrapTransportSelected ? 1U : 0U)
					+ (evidence.AccountTransportSelected ? 1U : 0U);
			}
#endif
			return 0U;
		}

		// Credential operations that crossed the vault boundary during the
		// run. A signed-out read consults no vault, so this is zero or the
		// mode reached for a credential it must not have.
		[[nodiscard]] auto public_preview_vault_effects() const noexcept
			-> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr) {
				const auto evidence = Fixture->evidence();
				return evidence.VaultStores
					+ (evidence.CredentialRetrieved ? 1U : 0U)
					+ (evidence.CredentialDeleted ? 1U : 0U);
			}
#endif
			return 0U;
		}

		// [[BPR-0316]] live-candidate evidence. The claim is about what the
		// anonymous provider was built from, so it is answered by comparing
		// what that provider was constructed with against the shipped factory
		// this composition made. A composition that handed the provider any
		// other object -- a fixture seam, a loopback twin -- fails the
		// comparison without anyone having to remember to update a flag.
		[[nodiscard]] auto public_preview_production_factories() const noexcept
			-> bool
		{
#if defined(BPR_BUILD_TESTING)
			return ShippedAnonymousTransports != nullptr
				&& AnonymousTransportFactory.get()
				== static_cast<github_wire_transport_factory_port*>(
					   ShippedAnonymousTransports.get())
				&& AnonymousSmartGit != nullptr && Fixture == nullptr;
#else
			return false;
#endif
		}

		[[nodiscard]] auto public_preview_wire_requests() const noexcept
			-> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			return ShippedAnonymousTransports == nullptr
				? 0U
				: ShippedAnonymousTransports->requests();
#else
			return 0U;
#endif
		}

		[[nodiscard]] auto public_preview_wire_capability_probes() const noexcept
			-> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			return ShippedAnonymousTransports == nullptr
				? 0U
				: ShippedAnonymousTransports->capability_probes();
#else
			return 0U;
#endif
		}

		[[nodiscard]] auto public_preview_wire_credentialed_refusals()
			const noexcept -> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			return ShippedAnonymousTransports == nullptr
				? 0U
				: ShippedAnonymousTransports->credentialed_refusals();
#else
			return 0U;
#endif
		}

		[[nodiscard]] auto public_preview_wire_operations() const
			-> std::vector<public_preview_wire_operation_record>
		{
#if defined(BPR_BUILD_TESTING)
			return ShippedAnonymousTransports == nullptr
				? std::vector<public_preview_wire_operation_record>{ }
				: ShippedAnonymousTransports->operations();
#else
			return { };
#endif
		}

		// The shared hourly core budget, taken from the rate-limit headers the
		// session carries rather than from any body.
		[[nodiscard]] auto public_preview_budget() const noexcept
			-> std::optional<application::github_rate_limit_record>
		{
			return Host == nullptr ? std::nullopt : Host->rate_limits().Core;
		}

		// The accepted snapshot's own discriminant, not the session's: a
		// snapshot accepted under an authenticated authority answers false
		// here even where the session says signed out.
		[[nodiscard]] auto public_preview_snapshot_signed_out() const noexcept
			-> bool
		{
			if (Host == nullptr) {
				return false;
			}
			const auto& snapshot = Host->accepted_snapshot();
			return snapshot.has_value()
				&& snapshot->Mode == core::github_access_mode::public_preview;
		}

		[[nodiscard]] auto public_preview_snapshot_head_oid() const
			-> std::string
		{
			if (Host == nullptr) {
				return { };
			}
			const auto& snapshot = Host->accepted_snapshot();
			return snapshot.has_value()
				? std::string{ snapshot->PullRequest.HeadOid.value() }
				: std::string{ };
		}

		[[nodiscard]] auto device_transport_submissions() const noexcept
			-> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr && RegisteredDeviceCourse) {
				return Fixture->evidence().DeviceTransportSubmissions;
			}
#endif
			return 0U;
		}

		// Every regular file under the application data root that contains the
		// enrolled token. The vault is the only place a credential belongs, so
		// the expected answer is none; settings and SQLite are the two places a
		// second copy would plausibly appear, and reading every file is
		// cheaper than guessing which.
		[[nodiscard]] auto device_durable_copies() const -> std::size_t
		{
#if defined(BPR_BUILD_TESTING)
			if (Fixture == nullptr || !RegisteredDeviceCourse) {
				return 0U;
			}
			const auto token = Fixture->device_authorization_access_token();
			if (token.empty()) {
				return 0U;
			}
			const auto&     root = Fixture->application_data_root();
			std::error_code error;
			if (root.empty() || !std::filesystem::is_directory(root, error)) {
				return 0U;
			}
			auto copies = std::size_t{ 0U };
			for (const auto& entry :
				 std::filesystem::recursive_directory_iterator{
					 root, std::filesystem::directory_options::skip_permission_denied,
					 error }) {
				if (error || !entry.is_regular_file(error) || error) {
					error.clear();
					continue;
				}
				auto file = std::ifstream{ entry.path(), std::ios::binary };
				if (!file) {
					continue;
				}
				const auto contents = std::string{
					std::istreambuf_iterator<char>{ file },
					std::istreambuf_iterator<char> { }
				};
				if (contents.find(token) != std::string::npos) {
					++copies;
				}
			}
			return copies;
#else
			return 0U;
#endif
		}

		// [[BPR-0536]]. The one place the device host learns which vault slot
		// its renewable material belongs to, and the only caller that may name
		// one: the key is minted inside `github_application_host` when an
		// authenticated `/user` observation is accepted, and composition reads
		// it rather than assembling one.
		//
		// Bound only after this run's device flow authorized. A profile signed
		// in with a token the reviewer pasted has no renewable material and no
		// renewal chain to lose, and telling its panel to renew would end in a
		// vault that answers `absent` and a reviewer told to authorize again
		// for no reason.
		//
		// An acceptance with no authorized stage behind it is the restart: the
		// credential was re-established from what the vault already held, so
		// renewal continues from the entry rather than starting over -- but
		// only once the host says an entry is there. The guard is exact key
		// equality against a key we actually acted on, so a republication that
		// names the same slot does nothing once armed and everything before.
		// [[BPR-0550]]. The memo `bind_device_refresh_authority` keeps records
		// which slot this composition already bound the device host to, so a
		// republication naming the same one costs nothing. A second grant in one
		// run establishes its own session, and for the same account that session
		// names the same slot -- the memo would early return, and the material
		// the new grant parked in the host would never be flushed, because the
		// host released the previous binding when the new flow began.
		//
		// Reaching `authorized` is the moment a grant exists that no binding has
		// been made for. The memo is dropped there, on the transition rather
		// than on the level, so the acceptance that follows rebinds exactly once
		// and a stage that merely stays authorized rebinds nothing.
		void observe_device_stage(
			const application::device_authorization_presentation& aPresentation)
		{
			const auto authorized = aPresentation.Stage
				== core::device_authorization_stage::authorized;
			if (authorized && !DeviceStageAuthorized) {
				DeviceRefreshAuthority.reset();
			}
			DeviceStageAuthorized = authorized;
		}

		void bind_device_refresh_authority(
			const application::github_presentation_update& aUpdate)
		{
			if (
				Closing || Host == nullptr || DeviceHost == nullptr
				|| Profiles.empty() || !aUpdate.Session.has_value()) {
				return;
			}
			if (aUpdate.Session->Profile != Profiles.front().profile()) {
				return;
			}
			const auto authority = Host->accepted_credential_authority();
			if (!authority.has_value()) {
				return;
			}
			// [[BPR-0546]]. The slot's authority and the identity generation
			// that owns it travel together. The device host's own state carries
			// the generation this composition seeded the device flow with,
			// which is zero and stays zero, and every platform vault refuses a
			// context carrying it -- so before this the composed refresh
			// lifecycle stored nothing on any platform.
			const auto generation = Host->accepted_identity_generation();
			if (generation.value() == 0U) {
				return;
			}
			if (
				DeviceRefreshAuthority.has_value()
				&& *DeviceRefreshAuthority == *authority && DeviceRefreshGeneration == generation) {
				return;
			}
			if (DeviceRefreshAuthority == authority && Host->accepted_device_lifetime()) {
				// A rotation established a new access session for this same chain.
				// Rebind its generation; this is not another startup resume.
				DeviceRefreshGeneration = generation;
				DeviceHost->bind_refresh_authority(*authority, generation);
				return;
			}
			if (!Host->accepted_device_lifetime()) {
				DeviceRefreshAuthority.reset();
				DeviceHost->release_refresh_authority();
				return;
			}
			if (
				DeviceHost->state().Stage
				== core::device_authorization_stage::authorized) {
				DeviceRefreshAuthority  = *authority;
				DeviceRefreshGeneration = generation;
				++DeviceRefreshBindings;
				DeviceHost->bind_refresh_authority(*authority, generation);
				return;
			}
			// The restart. Nothing about this run says a device flow happened,
			// so the question goes to the host that owns the vault: is there
			// material beside this credential that renews it? Only a device
			// grant leaves any, so an affirmative answer is the enrollment
			// evidence the stage cannot supply after a restart.
			if (!Host->accepted_credential_renewable()) {
				// A pasted token, or an answer the vault has not given yet.
				// Nothing is recorded either way, so the republication that
				// follows a settled answer is still free to act.
				return;
			}
			DeviceRefreshAuthority  = *authority;
			DeviceRefreshGeneration = generation;
			++DeviceRefreshResumes;
			DeviceHost->resume_refresh(*authority, generation, Host->accepted_device_lifetime());
		}

		// Renewal, as presentation is told it. The panel is the reader that
		// matters; the counters are what a composed course states.
		void observe_device_refresh(
			const application::device_authorization_refresh_observation&
				aObservation)
		{
			DeviceRefreshObservation = aObservation;
			++DeviceRefreshPublications;
			if (DevicePanel != nullptr) {
				DevicePanel->apply_refresh(aObservation);
			}
		}

		// The mounted panel's one way out. Beginning and cancelling are core
		// intents and reach the host's transition lane; opening a browser is
		// not an intent and reaches the host's handoff directly. Qt never opens
		// an address itself, so this is the only place a device gesture becomes
		// an effect.
		void dispatch_device_authorization_gesture(
			const application::device_authorization_gesture& aGesture)
		{
			if (Closing || DeviceHost == nullptr) {
				return;
			}
			if (const auto* const begin
				= std::get_if<core::begin_device_authorization_intent>(
					&aGesture)) {
				DeviceHost->dispatch(*begin);
				return;
			}
			if (const auto* const cancel
				= std::get_if<core::cancel_device_authorization_intent>(
					&aGesture)) {
				DeviceHost->dispatch(*cancel);
				return;
			}
			DeviceHost->open_verification_address();
		}

		// The same typed intent the device panel dispatches. Composition owns
		// the profile it names, because the panel is given one rather than
		// choosing one.
		void select_device_authorization()
		{
			if (Closing || DeviceHost == nullptr || Profiles.empty()) {
				return;
			}
			DeviceHost->dispatch(core::begin_device_authorization_intent{
				.Profile = Profiles.front().profile(),
			});
		}

		// Automation uses the same selected-profile control as an ordinary
		// launch. Select native HTTPS explicitly; CLI remains unavailable.
		void activate_public_preview(const bool aEnterprise)
		{
			if (Closing || Host == nullptr || Window == nullptr) {
				return;
			}
			const auto selected = std::ranges::find_if(
				Profiles,
				[aEnterprise](const validated_github_host_profile& aProfile) {
					const auto enterprise = aProfile.server_contract()
						!= application::github_server_contract::github_com_2022_11_28;
					return enterprise == aEnterprise
						&& aProfile.authentication_transport()
						== application::github_authentication_transport::native_https;
				});
			if (selected == Profiles.end() || Window->session_panel() == nullptr) {
				return;
			}
			auto* const selector = Window->profile_selector();
			const auto  index    = selector->findData(
				QVariant::fromValue<qulonglong>(selected->profile().value()));
			if (index < 0) {
				return;
			}
			selector->setCurrentIndex(index);
			Window->session_panel()->public_preview_button()->click();
		}

		// One write and one triage intent, offered to the mode and answered by
		// the core policy alone. `public_preview_legality` is a pure question:
		// it reaches no provider, asks for no transport and builds no request,
		// so the refusals counted here cost exactly the zero requests the
		// markers beside them report.
		void attempt_public_preview_writes()
		{
			if (Host == nullptr) {
				return;
			}
			constexpr std::array intents{
				core::anonymous_read_intent::publish_review_comment,
				core::anonymous_read_intent::set_viewed_file_state,
			};
			for (const auto intent : intents) {
				if (std::holds_alternative<core::anonymous_intent_refused>(
						Host->public_preview_legality(intent))) {
					++PublicPreviewWriteRefusals;
				}
			}
		}

		// The instant the signed-out presentation is measured from. The window
		// reads no clock of its own, so composition hands it one on every
		// publication; a presentation that carries the host's own instant
		// overrides it. Seconds since the epoch, because that is what the
		// budget's reset is expressed in.
		[[nodiscard]] static auto public_preview_now() noexcept -> std::uint64_t
		{
			const auto now = std::chrono::duration_cast<std::chrono::seconds>(
								 std::chrono::system_clock::now()
									 .time_since_epoch())
								 .count();
			return now <= 0 ? 0U : static_cast<std::uint64_t>(now);
		}

		// Where the switch-to-sign-in gesture lands. The host names the
		// profile the signed-out session was activated for, so the flow that
		// begins is the one that returns the reviewer to the pull request they
		// were already reading. It is the same typed intent the device panel
		// dispatches; nothing here knows how the flow works.
		void begin_public_preview_sign_in(const core::host_profile_id aProfile)
		{
			++PublicPreviewSignInRequests;
			if (Closing || DeviceHost == nullptr || aProfile.value() == 0U) {
				return;
			}
			DeviceHost->dispatch(core::begin_device_authorization_intent{
				.Profile = aProfile,
			});
		}

		// The same typed intent the device panel dispatches for the user's
		// cancel. The probe uses it to end a registered platform's real flow
		// at `awaiting_authorization`, which no unregistered composition can
		// reach.
		void cancel_device_authorization()
		{
			if (Closing || DeviceHost == nullptr) {
				return;
			}
			DeviceHost->dispatch(core::cancel_device_authorization_intent{ });
		}

		// The same request the device panel's "Open browser" action makes.
		// Not a core intent: opening a browser decides nothing about the
		// grant, so it reaches the host directly and the host republishes
		// what happened without a transition ([[BPR-0280]]).
		void open_device_verification_address()
		{
			if (Closing || DeviceHost == nullptr) {
				return;
			}
			DeviceHost->open_verification_address();
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

		[[nodiscard]] auto persist_correlated_defect(
			const application::durable_defect_record& aDefect) -> bool
		{
#if defined(BPR_BUILD_TESTING)
			if (DefectCompositionProbe.has_value()
				&& DefectCompositionProbe->RefuseCorrelatedPersistence) {
				++DefectCompositionProbe->RefusedPersistenceAttempts;
				return false;
			}
#endif
			return ReviewLedgerHost != nullptr
				&& ReviewLedgerHost->persist(aDefect).has_value();
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
						static_cast<void>(persist_correlated_defect(changed));
						durable = std::move(changed);
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
				const auto availability = defect_anchor_is_current(durable, *found)
					? core::defect_anchor_availability::available
					: core::defect_anchor_availability::outdated;
				if (found->Outdated) {
					changedAnchors.push_back(durable.History.Defect.Anchor);
				}
				if (availability != durable.History.Availability) {
					auto changed                 = durable;
					changed.History.Availability = availability;
					static_cast<void>(persist_correlated_defect(changed));
					durable = std::move(changed);
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

		[[nodiscard]] auto conversation_endpoint_is_current(
			const core::comparison_pair& aComparison,
			const core::diff_side        aSide) const -> bool
		{
			if (aSide == core::diff_side::left || Host == nullptr) {
				return false;
			}
			const auto& snapshot = Host->accepted_snapshot();
			const auto  head     = snapshot.has_value()
				? core::parse_git_commit_oid(snapshot->PullRequest.HeadOid.value())
				: std::nullopt;
			return head.has_value() && aComparison.Right.Commit == *head;
		}

		[[nodiscard]] auto current_thread_anchor(
			const application::github_review_thread_record& aThread) const
			-> std::optional<core::source_line_ref>
		{
			if (aThread.Subject != application::github_review_thread_subject::line
				|| aThread.Outdated || !aThread.Line.has_value()
				|| *aThread.Line == 0U || aThread.Path.empty()
				|| !SelectedDiffPublication.has_value() || Window == nullptr
				|| Window->comparison_surface() == nullptr
				|| Window->comparison_surface()->current_result() == nullptr) {
				return std::nullopt;
			}
			const auto  side     = aThread.DiffSide == application::github_diff_side::left
				? std::optional{ core::diff_side::left }
				: aThread.DiffSide == application::github_diff_side::right
				? std::optional{ core::diff_side::right }
				: std::nullopt;
			const auto& selected = *SelectedDiffPublication;
			if (!side.has_value()
				|| core::file_identity_of(aThread.Path)
					!= selected.Request.File.File
				|| !conversation_endpoint_is_current(
					selected.Request.Diff.Comparison, *side)) {
				return std::nullopt;
			}
			const auto anchor = core::source_line_ref{
				.Comparison = core::comparison_digest(selected.Key),
				.Side       = *side,
				.Line       = *aThread.Line - 1U,
			};
			const auto& alignment
				= Window->comparison_surface()->current_result()->Alignment;
			const auto lineCount = anchor.Side == core::diff_side::left
				? alignment.LeftLineCount
				: alignment.RightLineCount;
			return alignment.Digest == anchor.Comparison && anchor.Line < lineCount
				? std::optional{ anchor }
				: std::nullopt;
		}

		[[nodiscard]] auto defect_anchor_is_current(
			const application::durable_defect_record&       aDefect,
			const application::github_review_thread_record& aThread) const -> bool
		{
			const auto anchor = current_thread_anchor(aThread);
			return anchor.has_value()
				&& aDefect.File == core::file_identity_of(aThread.Path)
				&& aDefect.History.Defect.Anchor == *anchor
				&& aDefect.History.OriginalAnchor == *anchor;
		}

		void classify_defect(qt::workbench_defect_classification aClassification)
		{
			if (Closing || ReviewLedgerHost == nullptr
				|| !SelectedDiffPublication.has_value() || Window == nullptr
				|| Window->comparison_surface() == nullptr
				|| Window->comparison_surface()->current_result() == nullptr) {
				return;
			}
			const auto* const thread = accepted_thread(aClassification.Conversation);
			const auto        anchor = thread == nullptr
				? std::nullopt
				: current_thread_anchor(*thread);
			if (!anchor.has_value() || aClassification.Anchor != *anchor) {
				return;
			}
			const auto identity = defect_identity(aClassification.Conversation);
			if (std::ranges::any_of(
					DurableDefects,
					[identity](const application::durable_defect_record& aDefect) {
						return aDefect.Conversation.value() == identity.Conversation
							&& aDefect.History.Defect.Id.value() == identity.Defect;
					})) {
				return;
			}
			auto durable = application::durable_defect_record{
				.File         = SelectedDiffPublication->Request.File.File,
				.Conversation = core::review_conversation_id{ identity.Conversation },
				.History      = core::defect_history{
					.Defect = core::defect_record{
						.Id = core::defect_id{ identity.Defect }, .Anchor = *anchor },
					.OriginalAnchor = *anchor,
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

		void compose_review_submission(const application::accepted_pull_request_handoff& aHandoff)
		{
			if (!ReviewSubmission || !Host || !Host->accepted_snapshot()) {
				return;
			}
#if defined(BPR_BUILD_TESTING)
			if (!Fixture) {
				ReviewSubmission->invalidate();
				return;
			}
#endif
			auto selection = select_observed_review_mutation(aHandoff, *Host->accepted_snapshot());
			if (!selection) {
				ReviewSubmission->invalidate();
				return;
			}
			auto binding       = application::review_submission_live_binding{ selection->Target, selection->ActingIdentity };
			auto configuration = adapters::sqlite::review_mutation_recovery_configuration{
				.StorageRoot = ApplicationDataRoot, .Host = std::string{ binding.Target.Hostname.value() }, .Account = std::to_string(binding.ActingIdentity.Account.value()), .Repository = binding.Target.Repository.Owner + "/" + binding.Target.Repository.Name, .PullRequest = static_cast<std::int64_t>(binding.Target.Number.value())
			};
#if defined(BPR_BUILD_TESTING)
			auto factory = [configuration, binding, server = ReviewSubmissionServer]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> {
				auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
				if (!store) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				return detail::make_review_submission_fixture_provider(server, std::move(*store), binding);
			};
#else
			auto acceptedLease = Host->accepted_review_mutation_transport();
			if (!acceptedLease) {
				ReviewSubmission->invalidate();
				return;
			}
			auto factory = [configuration, binding, lease = std::move(acceptedLease)]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> {
				auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
				if (!store) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				auto provider = adapters::github::make_live_review_submission_provider(lease, binding.Target, binding.ActingIdentity, std::move(*store));
				if (!provider) {
					return std::unexpected(provider.error());
				}
				return std::unique_ptr<application::review_submission_port>{ std::move(*provider) };
			};
#endif
			ReviewSubmission->adopt(std::move(binding), workbench_authority(aHandoff), std::move(factory));
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
			auto selection = select_observed_review_mutation(
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
				compose_review_submission(aHandoff);
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
			if (ReviewSubmission) {
				ReviewSubmission->invalidate(aInvalidation);
			}
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
							// CLI selection retains its unavailable path and cannot
							// acquire an anonymous mode through this control.
							DeviceHost->release_refresh_authority();
							DeviceRefreshAuthority.reset();
							Host->dispatch(core::github_event{
								core::activate_host_profile_intent{ .Profile = aValue.Profile },
							});
							return;
						}
						if (aValue.Mode == core::github_access_mode::public_preview) {
							DeviceHost->release_refresh_authority();
							DeviceRefreshAuthority.reset();
							Host->activate_public_preview(aValue.Profile);
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
							DeviceHost->release_refresh_authority();
							DeviceRefreshAuthority.reset();
							Host->enroll_profile(
								aValue.Profile, std::move(*credential));
						}
					} else {
						if constexpr (std::is_same_v<intent_type, core::deactivate_github_identity_intent>) {
							DeviceHost->release_refresh_authority();
							DeviceRefreshAuthority.reset();
						}
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

		/// \brief What the reviewer is pointing at, in RFC-BPR-0004 identities.
		///
		/// \details Read from the alignment on screen, and only when the
		/// selection and the published comparison are the same one. **There is
		/// no coordinate here and none reachable from here**: a file identity
		/// and a semantic selection are the whole of it.
		[[nodiscard]] auto current_agent_chat_selection() const
			-> std::optional<core::agent_selection>
		{
			if (Window == nullptr || !SelectedDiffPublication.has_value()) {
				return std::nullopt;
			}
			const auto&       selected  = *SelectedDiffPublication;
			const auto* const workbench = Window->comparison_surface();
			if (
				workbench == nullptr || !workbench->selection().has_value()
				|| workbench->current_result() == nullptr
				|| workbench->current_result()->Alignment.Key != selected.Key
				|| workbench->selection()->Comparison
					!= workbench->current_result()->Alignment.Digest) {
				return std::nullopt;
			}
			return core::agent_selection{
				.File      = selected.Request.File.File,
				.Selection = *workbench->selection(),
			};
		}

		void clear_side_chat()
		{
			if (ChatHost != nullptr) {
				ChatHost->clear();
			}
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
				clear_side_chat();
				return;
			}
			const auto& authority  = *state.Authority;
			const auto& revision   = *state.AcceptedRevision;
			const auto& comparison = *state.AcceptedComparison;
			if (authority.Host != AgentReview->Host
				|| authority.Repository != AgentReview->Repository
				|| authority.PullRequest != AgentReview->PullRequest) {
				AgentHost->clear();
				clear_side_chat();
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
			const auto observedBinding = core::agent_revision_binding{
				.Host        = authority.Host,
				.Repository  = authority.Repository,
				.PullRequest = authority.PullRequest,
				.BaseOid     = revision.Base,
				.HeadOid     = revision.Head,
				.LeftEpoch   = comparison.Left.Identity.Epoch,
				.RightEpoch  = comparison.Right.Identity.Epoch,
			};
			if (ChatHost != nullptr) {
				// Recorded, never retargeted. The chat host presents itself
				// through its own projection, so a head that moved makes the
				// session stale on screen instead of quietly re-pointing it.
				ChatHost->observe_selection(current_agent_chat_selection());
				ChatHost->observe(observedBinding);
			} else {
				present_side_chat(observedBinding);
			}
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
			const auto start = std::get<qt::start_agent_intent>(aIntent);
			// A start gesture is not a chat turn, so it asks nothing: the
			// supplement's message stays empty on this path.
			static_cast<void>(dispatch_agent_invocation(
				start.Profile,
				start.IncludedNotes,
				start.IncludedDraft,
				start.ExistingCheckoutChosen,
				std::string_view{ },
				std::nullopt,
				core::agent_entry_point::ask_about_selection,
				std::nullopt));
		}

		/// \brief The one launch path, and there is deliberately no second.
		///
		/// \details Both the launch panel's start gesture and the side chat's
		/// send arrive here, so the agent host's own `dispatch` has exactly one
		/// call site in this file. A chat that had grown its own launch would have its own
		/// workspace, its own frozen snapshot, and its own answer about the
		/// same revision; `application_agent_chat_composition_contract` reads
		/// this file and fails if a second call site appears.
		///
		/// `aChatMessage` is what the reviewer typed for this turn, and is
		/// empty for every gesture that is not a chat send. It is an argument
		/// rather than something read from the chat host here, so the one place
		/// a question enters an invocation is a value passed at a call site a
		/// reader can see.
		///
		/// `aTurnSession` is the session a chat turn is sent under, and is
		/// absent for the launch panel's start gesture. When that session is
		/// bound to a revision the review has moved away from — which is what
		/// RFC-BPR-0012's `continue_against_old_revision` leaves behind — the
		/// revision *it* names is the one everything below composes from.
		///
		/// `aEntryPoint` is the reviewer gesture this turn is for, and
		/// `aWorkshopOperation` is which of the six comment-workshop
		/// operations was pressed. Both are arguments for the same reason
		/// `aChatMessage` is: the one place a gesture's intent enters an
		/// invocation is a value passed at a call site a reader can see.
		/// `build_agent_context_supplement` refuses an operation the entry
		/// point does not admit, so a workshop operation cannot ride along on
		/// a start gesture or a chat send.
		///
		/// \returns Whether exactly one invocation was dispatched. A refusal is
		/// already named on the launch panel before this returns `false`.
		[[nodiscard]] auto dispatch_agent_invocation(
			const core::agent_profile_id                          aProfile,
			const std::vector<application::local_note_id>&        aIncludedNotes,
			const std::optional<application::agent_draft_choice>& aIncludedDraft,
			const bool                                            aExistingCheckoutChosen,
			const std::string_view                                aChatMessage,
			const std::optional<core::agent_session>&             aTurnSession,
			const core::agent_entry_point                         aEntryPoint,
			const std::optional<core::workshop_operation>&        aWorkshopOperation)
			-> bool
		{
			if (Closing || AgentHost == nullptr || EpochHost == nullptr
				|| Window == nullptr) {
				return false;
			}
			const auto& state = EpochHost->state();
			if (!state.Authority.has_value() || !state.AcceptedRevision.has_value()
				|| !state.AcceptedComparison.has_value() || !AgentReview.has_value()
				|| ReviewLedgerHost == nullptr) {
				Window->agent_panel()->refuse("no exact accepted review context");
				return false;
			}
			const auto& authority          = *state.Authority;
			const auto& revision           = *state.AcceptedRevision;
			const auto& comparison         = *state.AcceptedComparison;
			auto        privateObservation = application::private_ledger_observation{ };
			if (ReviewLedgerHost != nullptr) {
				auto observed = ReviewLedgerHost->private_observation();
				if (!observed.has_value()) {
					Window->agent_panel()->refuse("private review context is unavailable");
					return false;
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
			// RFC-BPR-0012's `continue_against_old_revision`, composed and not
			// only kept. A chat turn whose session is bound to a revision the
			// epoch host has moved past is a question about *that* revision,
			// so the revision it names — not the one under review — is what
			// this invocation is built from. The next line is the whole of it;
			// without it the turn would be framed from the current head while
			// still naming the older session, and `agent_invocation_host`
			// refuses that disagreement as
			// `agent_continuation_binding_mismatch`.
			const auto continuing
				= aTurnSession.has_value() && aTurnSession->Binding != binding;
			auto turnBinding = binding;
			if (continuing) {
				turnBinding = aTurnSession->Binding;
			}
			auto observation = application::agent_context_observation{
				.ApplicationSession = state.ApplicationSession,
				.Binding            = turnBinding,
				.PullRequestUrl     = "https://" + AgentReview->Hostname + "/"
					+ AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name
					+ "/pull/" + std::to_string(AgentReview->Number.value()),
				.Private           = std::move(privateObservation),
				.IncludedNotes     = aIncludedNotes,
				.IncludedDraft     = aIncludedDraft,
				.ChatMessage       = std::string{ aChatMessage },
				.EntryPoint        = aEntryPoint,
				.WorkshopOperation = aWorkshopOperation,
			};
			observation.CurrentDraft = current_agent_draft_observation();
			// Which file, and where in it. The two are told apart because
			// only one of them names a comparison. The file is a path and a
			// review receipt, which belong to the file rather than to any pair
			// of commits, so a continuation may say them: the turn is answered
			// against the frozen snapshot of its own revision, and a finding
			// that names no file cannot be anchored to one. BPR-0532.
			//
			// The semantic selection below is read off the alignment the
			// reviewer is looking at, and its segments, groups, and comparison
			// digest belong to the revision under review. Under a continuation
			// that is a different revision than the one being asked about, so
			// none of it is sent: carrying a newer comparison's identities
			// beneath an older binding is the silent re-anchoring BPR-0521
			// refused, and this card does not un-refuse it. What a
			// continuation carries instead is the position the reviewer is
			// pointing at — a side and a line and nothing identified — held
			// here and said with the old revision's own identities once the
			// re-alignment below has produced them.
			auto pointedAt = std::optional<continuation_anchor>{ };
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
				const auto        pointing  = workbench->selection().has_value()
					&& workbench->current_result() != nullptr
					&& workbench->current_result()->Alignment.Key == selected.Key
					&& workbench->selection()->Comparison
						== workbench->current_result()->Alignment.Digest;
				if (pointing && !continuing) {
					observation.Selection = core::supplement_semantic_context{
						.File       = selected.Request.File.File,
						.Selection  = *workbench->selection(),
						.Confidence = core::supplement_mapping_confidence::exact,
						.Segments   = workbench->current_result()->Alignment.Segments,
						.Groups     = workbench->current_result()->Alignment.Groups,
					};
				} else if (pointing) {
					pointedAt = continuation_anchor{
						.File = selected.Request.File.File,
						.Side = workbench->selection()->PreferredLine.Side,
						.Line = workbench->selection()->PreferredLine.Line,
					};
				}
			}
			// A continuation asks for the old revision's alignment before it
			// asks the agent anything, and asks for it off this gesture:
			// recomputing an alignment queues behind whatever epoch work the
			// review is already doing, and a gesture that waited for it would
			// freeze the window for seconds. The reviewer's send returns here;
			// the dispatch follows the answer.
			if (continuing) {
				return begin_continuation_realignment(
					aProfile,
					authority,
					state.ApplicationSession,
					std::move(observation),
					aExistingCheckoutChosen,
					*aTurnSession,
					pointedAt);
			}
			return launch_agent_invocation(
				aProfile,
				authority,
				state.ApplicationSession,
				std::move(observation),
				aExistingCheckoutChosen,
				std::nullopt,
				frozen_snapshot_files());
		}

		/// \brief Freezes one turn's identities, pins them, and dispatches it.
		///
		/// \details The tail every gesture reaches, whether it got here
		/// straight from the composition above or from a re-alignment that
		/// answered later. The agent host's own dispatch is called here and
		/// nowhere else, which is the property
		/// `application_agent_chat_composition_contract` reads this file for
		/// by counting that call site — so this sentence deliberately does not
		/// spell the token it counts.
		///
		/// The frozen snapshot is stated once and used twice: the host
		/// validates every returned finding against it, and the MCP helper is
		/// pinned to the same values on disk. Two answers about one revision
		/// cannot disagree if they are one value. An empty list is a complete
		/// answer rather than a missing one — every finding then arrives
		/// unanchored — and it is what a continuation whose re-alignment was
		/// refused is dispatched with.
		[[nodiscard]] auto launch_agent_invocation(
			const core::agent_profile_id                   aProfile,
			const core::pull_request_authority&            aAuthority,
			const core::application_session_id             aApplicationSession,
			application::agent_context_observation         aObservation,
			const bool                                     aExistingCheckoutChosen,
			const std::optional<core::agent_session>&      aContinuedSession,
			std::vector<core::agent_finding_snapshot_file> aFrozen) -> bool
		{
			if (Closing || AgentHost == nullptr) {
				return false;
			}
			if (McpHelper != nullptr) {
				const auto encoded = adapters::mcp::encode_revision_binding(
					aObservation.Binding);
				if (freeze_mcp_snapshot(McpSnapshotRoot, encoded, aFrozen)) {
					++FrozenSnapshotWrites;
					McpHelper->pin(McpSnapshotRoot, encoded);
				}
			}
			AgentHost->dispatch(application::agent_invocation_request{
				.ApplicationSession     = aApplicationSession,
				.Authority              = aAuthority,
				.Profile                = aProfile,
				.Context                = std::move(aObservation),
				.ExistingCheckoutChosen = aExistingCheckoutChosen,
				.ExistingCheckout       = ExistingCheckout,
				.FrozenFiles            = std::move(aFrozen),
				.ContinuedSession       = aContinuedSession,
			});
			return true;
		}

		/// \brief Where the reviewer is pointing, as a continuation may say it.
		///
		/// \details A file, a side, and a line number: the position, with
		/// every identity of the revision on screen deliberately left behind.
		/// The comparison digest, the segment, and the group that position
		/// belongs to are the old revision's to state, and they are stated
		/// once the re-alignment has recomputed them.
		struct continuation_anchor
		{
			core::file_identity File;
			core::diff_side     Side = core::diff_side::left;
			std::uint32_t       Line = 0U;
		};

		/// \brief What one continued turn is waiting on its alignment to send.
		struct pending_continuation
		{
			core::agent_profile_id                 Profile;
			core::pull_request_authority           Authority;
			core::application_session_id           ApplicationSession;
			application::agent_context_observation Observation;
			bool                                   ExistingCheckoutChosen = false;
			core::agent_session                    Session;
			std::optional<continuation_anchor>     PointedAt;
		};

		/// \brief Asks for the kept revision's own alignment, then dispatches.
		///
		/// \details BPR-0532. The reviewer kept a session bound to a revision
		/// the review has moved past, so the identities this turn must be
		/// answered against belong to that revision and not to the comparison
		/// on screen. They are recomputed rather than retained: the request
		/// below carries the current selection's authority, options, and file
		/// and nothing else, and `realignment_lookup` retargets it onto the
		/// kept binding's epoch pair.
		///
		/// Two bounds, both stated in the application layer rather than here:
		/// `maximum_agent_realignment_files` and the request's own compute
		/// limits — the limits a fresh alignment has — cap the work, and
		/// `maximum_agent_realignment_wait` caps the wait. Past the wait the
		/// request is stopped and the turn is dispatched with nothing frozen,
		/// which is the same answer any other refusal produces. Nothing here
		/// blocks and nothing here waits twice: the dispatch happens in the
		/// completion below, once.
		[[nodiscard]] auto begin_continuation_realignment(
			const core::agent_profile_id              aProfile,
			const core::pull_request_authority&       aAuthority,
			const core::application_session_id        aApplicationSession,
			application::agent_context_observation    aObservation,
			const bool                                aExistingCheckoutChosen,
			const core::agent_session&                aTurnSession,
			const std::optional<continuation_anchor>& aPointedAt) -> bool
		{
			LastContinuationRealignmentRetargeted = false;
			auto realignment
				= std::optional<application::agent_realignment_request>{ };
			if (Realigner != nullptr && SelectedDiffPublication.has_value()) {
				const auto& selected = *SelectedDiffPublication;
				// The warrant, asked of the host that owns the cache the
				// recompute will read. The reviewer's own selected diff is
				// authorized for the comparison on screen and nothing else, so
				// a turn about an older pair is refused as stale unless the
				// host has decided to answer for that pair; asking for the
				// decision is what this is, and a host that declines leaves
				// this turn on the documented empty freeze.
				const auto warrant = EpochHost->realignment_context(
					selected.Request.WorkbenchAuthority, aTurnSession.Binding);
				if (!warrant.has_value()) {
					return refuse_continuation_realignment(
						pending_continuation{
							.Profile                = aProfile,
							.Authority              = aAuthority,
							.ApplicationSession     = aApplicationSession,
							.Observation            = std::move(aObservation),
							.ExistingCheckoutChosen = aExistingCheckoutChosen,
							.Session                = aTurnSession,
							.PointedAt              = aPointedAt,
						},
						application::agent_realignment_refusal::host_failure);
				}
				realignment = application::agent_realignment_request{
					.Binding            = aTurnSession.Binding,
					.Context            = selected.Request.Context,
					.WorkbenchAuthority = selected.Request.WorkbenchAuthority,
					.Diff               = *warrant,
					.Files              = { selected.Request.File },
					.Options            = warrant->Options,
					.Limits             = core::diff_compute_limits{ },
				};
				// Whether the retarget is a retarget at all. When the kept
				// binding's pair is the pair already on screen this asks for
				// what is already there, and a course that proved anything
				// about re-alignment from such a turn would be proving
				// nothing.
				LastContinuationRealignmentRetargeted
					= application::realignment_lookup(
						  *realignment, realignment->Files.front())
					!= selected.Request;
			}
			auto turn = pending_continuation{
				.Profile                = aProfile,
				.Authority              = aAuthority,
				.ApplicationSession     = aApplicationSession,
				.Observation            = std::move(aObservation),
				.ExistingCheckoutChosen = aExistingCheckoutChosen,
				.Session                = aTurnSession,
				.PointedAt              = aPointedAt,
			};
			if (!realignment.has_value() || Window == nullptr) {
				return refuse_continuation_realignment(
					std::move(turn),
					application::agent_realignment_refusal::host_failure);
			}
			auto        stop      = std::make_shared<std::stop_source>();
			const auto  token     = stop->get_token();
			auto* const realigner = Realigner.get();
			auto* const lane      = &DecisionLane;
			ContinuationRealignmentsInFlight.fetch_add(
				1U, std::memory_order_release);
			try {
				AgentEffects.post(
					[this,
					 lane,
					 realigner,
					 token,
					 asked   = *realignment,
					 waiting = std::move(turn)]() mutable {
						auto outcome = realigner->realign(asked, token);
						try {
							lane->post([this,
										produced = std::move(outcome),
										resumed  = std::move(waiting)]() mutable {
								finish_continuation_realignment(
									std::move(resumed), std::move(produced));
							});
						} catch (...) {
							ContinuationRealignmentsInFlight.fetch_sub(
								1U, std::memory_order_release);
						}
					});
			} catch (...) {
				ContinuationRealignmentsInFlight.fetch_sub(
					1U, std::memory_order_release);
				return refuse_continuation_realignment(
					std::move(turn),
					application::agent_realignment_refusal::host_failure);
			}
			LastContinuationRealignment = "pending";
			ContinuationRealignmentStop = stop;
			// The wall-clock bound, armed on the gesture and disarmed by the
			// completion resetting the handle it compares against. A timer
			// that arrives after its own turn finished belongs to no request
			// and stops nothing.
			QTimer::singleShot(
				static_cast<int>(
					application::maximum_agent_realignment_wait.count()),
				Window.get(),
				[this, stop] {
					if (ContinuationRealignmentStop != stop) {
						return;
					}
					ContinuationRealignmentDeadlineFired = true;
					static_cast<void>(stop->request_stop());
				});
			return true;
		}

		/// \brief Dispatches a continued turn whose alignment was refused.
		[[nodiscard]] auto refuse_continuation_realignment(
			pending_continuation                         aTurn,
			const application::agent_realignment_refusal aRefusal) -> bool
		{
			LastContinuationRealignment
				= std::string{ application::stable_name(aRefusal) };
			return launch_agent_invocation(
				aTurn.Profile,
				aTurn.Authority,
				aTurn.ApplicationSession,
				std::move(aTurn.Observation),
				aTurn.ExistingCheckoutChosen,
				aTurn.Session,
				{ });
		}

		/// \brief Dispatches the continued turn its alignment answered for.
		void finish_continuation_realignment(
			pending_continuation                   aTurn,
			application::agent_realignment_outcome aOutcome)
		{
			ContinuationRealignmentsInFlight.fetch_sub(
				1U, std::memory_order_release);
			const auto expired = ContinuationRealignmentDeadlineFired;
			ContinuationRealignmentStop.reset();
			ContinuationRealignmentDeadlineFired = false;
			// The reason, recorded whether or not anything was frozen. A turn
			// that froze nothing and a turn nobody asked to freeze anything
			// are different events, and only a recorded reason tells them
			// apart afterwards.
			LastContinuationRealignment = expired
				? std::string{ "deadline" }
				: std::string{ application::stable_name(aOutcome.Refusal) };
			if (Closing) {
				return;
			}
			// Where the reviewer is pointing, said in the old revision's own
			// terms. The position crossed over by line and side, which is a
			// positional mapping and not an identity one, so the confidence
			// stated is `approximate` and no segment or group identity is
			// claimed: this composition did not compute one, and inventing an
			// identity for a line is the whole failure mode here. What it does
			// state is the re-aligned pair's own comparison digest, segments,
			// and groups, so every identity the turn carries and every
			// identity it is validated against come from the one alignment the
			// re-alignment produced. Out of the old revision's range the
			// position is simply not said - that revision has no such line,
			// and the turn is still answered against its frozen identities.
			if (!aOutcome.Alignments.empty() && aTurn.PointedAt.has_value()) {
				const auto& realigned = aOutcome.Alignments.front();
				const auto  anchor    = core::source_line_ref{
					.Comparison = realigned.Digest,
					.Side       = aTurn.PointedAt->Side,
					.Line       = aTurn.PointedAt->Line,
				};
				if (core::valid(
						anchor,
						realigned.LeftLineCount,
						realigned.RightLineCount)) {
					aTurn.Observation.Selection
						= core::supplement_semantic_context{
							  .File      = aTurn.PointedAt->File,
							  .Selection = core::semantic_selection{
								  .Comparison    = realigned.Digest,
								  .Kind          = core::semantic_selection_kind::line,
								  .PreferredLine = anchor,
							  },
							  .Confidence = core::supplement_mapping_confidence::approximate,
							  .Segments   = realigned.Segments,
							  .Groups     = realigned.Groups,
						  };
				}
			}
			static_cast<void>(launch_agent_invocation(
				aTurn.Profile,
				aTurn.Authority,
				aTurn.ApplicationSession,
				std::move(aTurn.Observation),
				aTurn.ExistingCheckoutChosen,
				aTurn.Session,
				std::move(aOutcome.Files)));
		}

		/// \brief The frozen alignment facts one invocation is answered against.
		///
		/// \details Read from the alignment the reviewer is actually looking
		/// at, once, and carried as values. An empty list is a complete answer
		/// rather than a missing one: every finding then arrives unanchored,
		/// which is what a run that froze nothing has evidence for.
		[[nodiscard]] auto frozen_snapshot_files() const
			-> std::vector<core::agent_finding_snapshot_file>
		{
			auto files = std::vector<core::agent_finding_snapshot_file>{ };
			if (Window == nullptr || !SelectedDiffPublication.has_value()) {
				return files;
			}
			const auto* const workbench = Window->comparison_surface();
			if (workbench == nullptr || workbench->current_result() == nullptr
				|| workbench->current_result()->Alignment.Key
					!= SelectedDiffPublication->Key) {
				return files;
			}
			const auto& alignment = workbench->current_result()->Alignment;
			auto        file      = core::agent_finding_snapshot_file{
				.Path           = SelectedDiffPublication->Request.File.PresentationPath,
				.Comparison     = alignment.Digest,
				.LeftLineCount  = alignment.LeftLineCount,
				.RightLineCount = alignment.RightLineCount,
			};
			file.Segments.reserve(alignment.Segments.size());
			for (const auto& segment : alignment.Segments) {
				file.Segments.push_back(segment.Id);
			}
			file.Groups.reserve(alignment.Groups.size());
			for (const auto& group : alignment.Groups) {
				file.Groups.push_back(group.Id);
			}
			file.Moves.reserve(alignment.Moves.size());
			for (const auto& move : alignment.Moves) {
				file.Moves.push_back(move.Id);
			}
			files.push_back(std::move(file));
			return files;
		}

		void present_agent_refusal(
			const std::string_view aSubject, const std::string_view aReason)
		{
			if (Window == nullptr || Window->agent_panel() == nullptr) {
				return;
			}
			Window->agent_panel()->refuse(
				std::string{ aSubject } + " refused: " + std::string{ aReason });
		}

		/// \brief Starts one agent turn for one asked-for workshop operation.
		///
		/// \details The reviewer's half of the workshop: the operation is
		/// admitted under the one entry point it means anything for, held
		/// against the draft it was asked about, and handed to the same
		/// `dispatch_agent_invocation` a start gesture and a chat send use.
		/// There is no second launch path, no workshop agent, and no host of
		/// this method's own — the turn it starts is the one this application
		/// already knows how to start, with `workshop_comment` named as the
		/// gesture and the operation in the supplement.
		///
		/// Nothing here writes a revision. The answer arrives as this turn's
		/// outcome and is proposed by the composed workshop host; inventing
		/// text here would be this composition answering a question it
		/// forwarded.
		///
		/// One press, one turn. A second press while the first turn is still
		/// unanswered is refused and named rather than queued: a queued press
		/// would either start a second turn about a comment the first is
		/// already revising, or sit silently until the reviewer concluded the
		/// control did nothing.
		void ask_comment_workshop(const qt::comment_workshop_request& aRequest)
		{
			const auto admitted = core::workshop_operation_for(
				core::agent_entry_point::workshop_comment, aRequest.Operation);
			if (!admitted.has_value()) {
				present_agent_refusal("comment workshop", "unknown_operation");
				return;
			}
			if (!ReviewDraft.has_value() || ReviewDraft->Id != aRequest.Draft) {
				present_agent_refusal(
					"comment workshop",
					application::name(application::
										  agent_comment_workshop_refusal::
											  different_draft));
				return;
			}
			// One press, one *running* turn. A record left over from a turn
			// that has already ended is not a turn in flight: it is only the
			// name of the operation the next outcome may still be labelled
			// with, and asking again replaces it below.
			if (PendingWorkshopOperation.has_value()
				&& PendingWorkshopOperation->Running) {
				present_agent_refusal(
					"comment workshop", "workshop_turn_in_flight");
				return;
			}
			PendingWorkshopOperation = pending_comment_workshop{
				.Draft     = aRequest.Draft,
				.Operation = *admitted,
			};
			// The draft's own text and anchor are this turn's context, so the
			// question the agent is answering is about the comment on screen
			// and not about whatever the launch panel happened to have
			// checked. No private notes cross: the reviewer asked how to say
			// this better, not what else is known.
			if (!dispatch_agent_invocation(
					core::codex_jsonl_profile_id,
					std::vector<application::local_note_id>{ },
					current_agent_draft_choice(),
					false,
					std::string_view{ },
					std::nullopt,
					core::agent_entry_point::workshop_comment,
					admitted)) {
				// The launch panel has already named why. The operation is
				// cleared rather than left recorded, because a question no
				// turn was started for is not waiting for an answer, and a
				// later unrelated revision must not be labelled with it.
				PendingWorkshopOperation.reset();
				// BPR-0538. And said on the surface that asked. The panel
				// names the refusal for the operator; the reviewer is looking
				// at the six controls that just shut, and they reopen here
				// with the reason where the answer would have been.
				present_comment_workshop_settlement(
					aRequest.Draft, "the turn was refused before it started");
			}
		}

		/// \brief Ends the press whose turn has just ended.
		///
		/// \details BPR-0538. The third sink's whole decision, in one place.
		/// Three questions in order, and all three are about *this* turn:
		///
		/// - Is this the turn the press is waiting on? A press bound to
		///   another session is untouched, so a second, unrelated turn
		///   finishing — a launch-panel start, a chat send — neither reopens
		///   the six nor consumes the operation the press is holding.
		/// - Whatever it produced, it is over, so the six reopen. The tooltip
		///   they carried said a turn was running; it is not.
		/// - Did it produce anything the reviewer could still convert into the
		///   revision that was asked for? If it did, the record stays so that
		///   conversion is still labelled with the operation that produced it.
		///   If it did not — no finding delivered, a failed run, a session the
		///   review moved out from under — nothing is coming, so the record
		///   goes and the surface says why.
		///
		/// Nothing here retries and nothing here proposes: a turn that
		/// answered nothing is reported as having answered nothing.
		void settle_comment_workshop(
			const core::agent_session_id         aSession,
			const application::agent_run_outcome aOutcome)
		{
			if (!PendingWorkshopOperation.has_value()
				|| !PendingWorkshopOperation->Session.has_value()
				|| *PendingWorkshopOperation->Session != aSession) {
				return;
			}
			PendingWorkshopOperation->Running = false;
			const auto draft                  = PendingWorkshopOperation->Draft;
			const auto answered               = PendingWorkshopOperation->Delivered
				&& aOutcome == application::agent_run_outcome::success;
			auto reason = std::string{ };
			if (!answered) {
				reason = aOutcome == application::agent_run_outcome::success
					? std::string{ "the turn proposed no revision" }
					: "the turn ended "
						+ std::string{ application::name(aOutcome) };
				PendingWorkshopOperation.reset();
			}
			present_comment_workshop_settlement(draft, reason);
		}

		/// \brief Reopens the six controls, and says why if nothing is coming.
		///
		/// \details Presented on the workshop surface rather than on the
		/// launch panel: the reviewer pressed a control beside their own text
		/// and is watching the space the before/after would fill. A reason
		/// delivered to the invocation panel is a reason they are not reading.
		void present_comment_workshop_settlement(
			const core::review_draft_id aDraft, const std::string& aReason)
		{
			if (Window == nullptr || Window->review_editor() == nullptr) {
				return;
			}
			Window->review_editor()->present(qt::comment_workshop_settlement{
				.Draft  = aDraft,
				.Reason = aReason,
			});
		}

		/// \brief Presents what one asked-for finding action produced.
		///
		/// \details A promoted comment becomes a *proposal* rather than a
		/// draft: the reviewer reads the before and the after on the shipped
		/// editor and accepts it there, which is the one gesture that emits a
		/// local draft command. Nothing here writes, and nothing here reaches
		/// the review-mutation host.
		void present_finding_outcome(
			const application::agent_finding_outcome& aOutcome)
		{
			if (aOutcome.Draft.has_value() && WorkshopHost != nullptr
				&& ReviewDraft.has_value()) {
				// The operation the reviewer asked for, spent by the turn it
				// was asked in and by no other. The pairing is the session
				// identity carried from the press: a finding promoted from a
				// different turn — or from this one before the workshop turn
				// was admitted — is a revision nobody workshopped, and
				// labelling it with a pending operation would name a question
				// that produced none of this text.
				const auto matched = PendingWorkshopOperation.has_value()
					&& PendingWorkshopOperation->Session.has_value()
					&& *PendingWorkshopOperation->Session == aOutcome.Session
					&& PendingWorkshopOperation->Draft == ReviewDraft->Id;
				const auto asked = matched
					? std::optional<core::workshop_operation>{
						  PendingWorkshopOperation->Operation
					  }
					: std::optional<core::workshop_operation>{ };
				if (matched) {
					PendingWorkshopOperation.reset();
				}
				static_cast<void>(WorkshopHost->propose(
					*ReviewDraft, *aOutcome.Draft, asked));
				return;
			}
			if (aOutcome.Defect.has_value()) {
				dispatch_defect(*aOutcome.Defect);
				return;
			}
			if (aOutcome.Note.has_value()) {
				refresh_agent_private_context_choices();
				return;
			}
			// A refused conversion is presented, not dropped. A reviewer who
			// is shown nothing cannot tell a refusal from a swallowed gesture.
			++FindingRefusalsPresented;
			present_agent_refusal(
				application::name(aOutcome.Action),
				aOutcome.Refusal
						!= application::agent_finding_action_refusal::none
					? application::name(aOutcome.Refusal)
					: promotion_refusal_name(aOutcome.Promotion));
		}

		[[nodiscard]] auto next_private_note_id() const
			-> application::local_note_id
		{
			auto next = std::uint64_t{ 1U };
			if (ReviewLedgerHost != nullptr) {
				if (const auto observed = ReviewLedgerHost->private_observation();
					observed.has_value()) {
					for (const auto& note : observed->Notes) {
						next = std::max(next, note.Id.value() + 1U);
					}
				}
			}
			return application::local_note_id{ next };
		}

		[[nodiscard]] auto current_note_attachment() const
			-> std::optional<application::local_note_attachment>
		{
			if (!SelectedDiffPublication.has_value()) {
				return std::nullopt;
			}
			return application::local_note_attachment{
				.File       = SelectedDiffPublication->Request.File.File,
				.Comparison = core::comparison_digest(SelectedDiffPublication->Key),
			};
		}

		/// \brief The durable defect anchored exactly where the finding is.
		///
		/// \details Nothing is moved onto a nearby defect. `promote_agent_finding`
		/// refuses `anchor_elsewhere` when the two disagree, and supplying the
		/// nearest one here would be that refusal arriving from the other
		/// direction.
		[[nodiscard]] auto anchored_durable_defect(
			const core::agent_finding& aFinding) const
			-> std::optional<core::defect_record>
		{
			if (!aFinding.Anchor.Line.has_value()) {
				return std::nullopt;
			}
			for (const auto& durable : DurableDefects) {
				if (durable.History.Defect.Anchor == *aFinding.Anchor.Line) {
					return durable.History.Defect;
				}
			}
			return std::nullopt;
		}

		void dispatch_finding_intent(qt::finding_action_intent aIntent)
		{
			if (Closing || FindingHost == nullptr) {
				return;
			}
			auto request = application::agent_finding_action_request{
				.Session = aIntent.Subject.Session,
				.Ordinal = aIntent.Subject.Ordinal,
				.Action  = composed_finding_action(aIntent.Action),
			};
			switch (request.Action) {
			case application::agent_finding_action::convert_to_draft_comment:
				request.Draft = ReviewDraft;
				break;
			case application::agent_finding_action::convert_to_draft_defect:
				request.Defect = anchored_durable_defect(aIntent.Subject.Finding);
				break;
			case application::agent_finding_action::add_to_private_notes:
				request.Note       = next_private_note_id();
				request.Attachment = current_note_attachment();
				break;
			case application::agent_finding_action::open_evidence:
			case application::agent_finding_action::ask_follow_up:
			case application::agent_finding_action::dismiss:
				break;
			}
			static_cast<void>(FindingHost->act(request));
		}

		/// \brief Carries one side-chat gesture to the composed chat host.
		///
		/// \details Translation and nothing else. The Qt intents are restated
		/// as the application's own request values because this host may not
		/// depend on the adapter that emits them, and every decision below the
		/// translation belongs to `agent_chat_session_host` and, through it, to
		/// the core. With the chat host cut, a send is refused by name rather
		/// than quietly doing nothing.
		void dispatch_side_chat_intent(qt::side_chat_intent aIntent)
		{
			++SideChatIntents;
			if (ChatHost == nullptr) {
				if (std::holds_alternative<qt::send_chat_message_intent>(
						aIntent)) {
					present_agent_refusal(
						"side chat", "no composed chat session host");
				}
				return;
			}
			if (const auto* const send
				= std::get_if<qt::send_chat_message_intent>(&aIntent);
				send != nullptr) {
				// Read at the gesture, from the alignment on screen. A
				// selection observed earlier would answer for where the
				// reviewer was, not where they are.
				ChatHost->observe_selection(current_agent_chat_selection());
				static_cast<void>(
					ChatHost->send(application::agent_chat_send{
						.Session = send->Session,
						.Action  = send->Action,
						.Scope   = send->Scope,
						.Text    = send->Text,
					}));
				return;
			}
			if (const auto* const widen
				= std::get_if<qt::widen_chat_scope_intent>(&aIntent);
				widen != nullptr) {
				static_cast<void>(
					ChatHost->widen(application::agent_chat_widening{
						.Session  = widen->Session,
						.Widening = widen->Widening,
					}));
				return;
			}
			if (const auto* const optIn
				= std::get_if<qt::chat_private_context_intent>(&aIntent);
				optIn != nullptr) {
				static_cast<void>(ChatHost->set_private_context(optIn->OptIn));
				return;
			}
			const auto& choice
				= std::get<qt::choose_chat_continuation_intent>(aIntent);
			static_cast<void>(
				ChatHost->choose(application::agent_chat_continuation_choice{
					.Session        = choice.Session,
					.Choice         = choice.Choice,
					.OfferedBinding = choice.OfferedBinding,
				}));
		}

		/// \brief Says on the panel what one chat gesture produced.
		///
		/// \details A refusal is presented, never dropped. The reason is the
		/// host's own name for it, which carries the core's verdict where there
		/// is one, so a reviewer whose send was refused is told which of the
		/// core's answers refused it rather than watching a chat do nothing.
		void present_chat_outcome(const application::agent_chat_outcome& aOutcome)
		{
			if (aOutcome.Refusal == application::agent_chat_refusal::none) {
				if (aOutcome.Turn.has_value()) {
					++SideChatTurns;
				}
				return;
			}
			++SideChatRefusals;
			auto reason = std::string{ application::name(aOutcome.Refusal) };
			switch (aOutcome.Refusal) {
			case application::agent_chat_refusal::session_stale:
				reason += " (";
				reason += core::stable_name(aOutcome.Staleness);
				reason += ')';
				break;
			case application::agent_chat_refusal::action_refused:
				reason += " (";
				reason += core::stable_name(aOutcome.Action);
				reason += ')';
				break;
			case application::agent_chat_refusal::scope_refused:
				reason += " (";
				reason += core::stable_name(aOutcome.Scope);
				reason += ')';
				break;
			case application::agent_chat_refusal::none:
			case application::agent_chat_refusal::no_session:
			case application::agent_chat_refusal::other_session:
			case application::agent_chat_refusal::private_context_refused:
			case application::agent_chat_refusal::empty_message:
			case application::agent_chat_refusal::unknown_continuation:
			case application::agent_chat_refusal::binding_mismatch:
			case application::agent_chat_refusal::launch_refused:
				break;
			}
			present_agent_refusal("side chat", reason);
		}

		/// \brief Every private note the review holds, by identity.
		[[nodiscard]] auto all_private_note_ids() const
			-> std::vector<application::local_note_id>
		{
			auto notes = std::vector<application::local_note_id>{ };
			if (ReviewLedgerHost == nullptr) {
				return notes;
			}
			const auto observed = ReviewLedgerHost->private_observation();
			if (!observed.has_value()) {
				return notes;
			}
			notes.reserve(observed->Notes.size());
			for (const auto& note : observed->Notes) {
				notes.push_back(note.Id);
			}
			return notes;
		}

		/// \brief The draft a chat turn may carry, when there is one to carry.
		[[nodiscard]] auto current_agent_draft_choice() const
			-> std::optional<application::agent_draft_choice>
		{
			const auto draft = current_agent_draft_observation();
			if (
				!draft.has_value()
				|| draft->Draft.State != core::review_draft_state::drafting) {
				return std::nullopt;
			}
			return draft->Choice;
		}

		/// \brief The reviewer's own prose, as the core admission takes it.
		///
		/// \details Read at call time and never held. This is the *material*
		/// the core decides about; whether any of it crosses is
		/// `core::admit_private_context`'s answer, keyed by session.
		[[nodiscard]] auto current_chat_private_prose() const
			-> application::agent_chat_private_prose
		{
			auto prose = application::agent_chat_private_prose{ };
			if (ReviewLedgerHost != nullptr) {
				if (const auto observed = ReviewLedgerHost->private_observation();
					observed.has_value()) {
					prose.Notes.reserve(observed->Notes.size());
					for (const auto& note : observed->Notes) {
						prose.Notes.push_back(note.Note);
					}
				}
			}
			if (const auto draft = current_agent_draft_observation();
				draft.has_value()
				&& draft->Draft.State == core::review_draft_state::drafting) {
				prose.Drafts.push_back(
					core::supplement_draft{ .Text = draft->Draft.Body });
			}
			return prose;
		}

		/// \brief Shows the chat the composed host is holding, and nothing else.
		///
		/// \details Every revision-bearing field comes from the host's own
		/// projection, which carries the *session's* binding. The composition
		/// adds only the four fields the core has no opinion about — provider
		/// name, Git host, workspace, and chips — so there is still no path by
		/// which the head now under review reaches this widget.
		void present_side_chat(const application::agent_chat_projection& aChat)
		{
			if (SideChat == nullptr || !AgentReview.has_value()) {
				return;
			}
			++SideChatPresentations;
			SideChat->present(qt::side_chat_projection{
				.Session        = aChat.Session,
				.Continuations  = aChat.Continuations,
				.Chat           = aChat.Chat,
				.PrivateContext = aChat.PrivateContext,
				.EntryPoint     = aChat.EntryPoint,
				.Provider       = agent_provider_name(aChat.Session.Profile),
				.GitHost        = AgentReview->Hostname,
				.Workspace      = LastAgentWorkspacePath,
			});
		}

		/// \brief Shows the chat when its host was cut out of the composition.
		///
		/// \details The panel stays on screen and stays truthful: it is handed
		/// the session it would have, presented as current, so the send gesture
		/// is still reachable and still observable. What is gone is the turn —
		/// `dispatch_side_chat_intent` refuses it by name — which is exactly
		/// what a construction cut is supposed to leave behind.
		void present_side_chat(const core::agent_revision_binding& aBinding)
		{
			if (SideChat == nullptr || !AgentReview.has_value()) {
				return;
			}
			const auto session = core::agent_session{
				.Id      = core::agent_session_id{ 1U },
				.Profile = core::codex_jsonl_profile_id,
				.Binding = aBinding,
			};
			present_side_chat(application::agent_chat_projection{
				.Session       = session,
				.Continuations = core::offer_agent_continuations(session, aBinding),
				.Chat          = core::agent_chat_state{
					.Scope = core::agent_chat_scope{
						.Kind = core::agent_chat_scope_kind::pull_request,
					},
				},
				.PrivateContext = core::agent_private_context_optin{
					.Session = session.Id,
				},
				.EntryPoint = core::agent_entry_point::ask_about_selection,
			});
		}

		[[nodiscard]] static auto agent_provider_name(
			const core::agent_profile_id aProfile) -> std::string
		{
			const auto profiles = core::built_in_agent_profiles();
			for (const auto& profile : profiles) {
				if (profile.Id == aProfile) {
					return profile.DisplayName;
				}
			}
			return { };
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
			if (Window != nullptr) {
				Window->set_conversation_endpoint_placement(false, false);
			}
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
				aPublication.Request.Diff.Comparison,
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
			core::file_comparison_key    aKey,
			const core::comparison_pair& aComparison,
			core::diff_options           aOptions,
			const std::string_view       aPath)
		{
			if (Closing || DiffHost == nullptr) {
				return;
			}
			// GitHub's current PR diff uses a merge-base coordinate on LEFT,
			// while BPR's retained Branch base is explicitly the accepted base
			// tip. The provider snapshot carries no exact merge-base identity, so
			// LEFT remains unplaced instead of applying a numeric line to a
			// potentially unrelated blob.
			constexpr auto leftHasCurrentDiffRevision = false;
			const auto     rightIsCurrentHead         = conversation_endpoint_is_current(
				aComparison, core::diff_side::right);
			if (Window != nullptr) {
				Window->set_conversation_endpoint_placement(
					leftHasCurrentDiffRevision, rightIsCurrentHead);
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
			// The one outstanding re-alignment is told to stop here rather
			// than waited on: its ports are closing, and a request that kept
			// resolving through them would be the hang the poll below reports
			// as a timeout.
			if (ContinuationRealignmentStop) {
				static_cast<void>(ContinuationRealignmentStop->request_stop());
			}
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
#if defined(BPR_BUILD_TESTING)
			if (ReviewSubmissionDecisionGate) {
				ReviewSubmissionDecisionGate->release();
			}
#endif
			if (ReviewSubmission) {
				ReviewSubmission->shutdown();
			}
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
			if (DeviceHost) {
				DeviceHost->shutdown();
			}
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
			if (AgentWorkspaceRecoveryPending.load(std::memory_order_acquire)) {
				schedule_shutdown_poll();
				return;
			}
			if (AgentWorkspaceRecoveryPostFailed.load(std::memory_order_acquire)) {
				AgentWorkspaceCleanupFailed = true;
			}
			if (!AgentWorkspaceCleanupAttempted) {
				AgentWorkspaceCleanupAttempted = true;
				if (AgentWorkspaceParent.has_value()) {
					AgentWorkspaceCleanupFailed = !adapters::git::remove_installed_agent_workspace_parent(*AgentWorkspaceParent);
				}
			}
			begin_other_shutdown();
			if (
				!DiffHost->is_quiescent() || !EpochHost->is_quiescent()
				|| !Host->is_quiescent()
				|| ContinuationRealignmentsInFlight.load(
					   std::memory_order_acquire)
					!= 0U
				|| (DeviceHost && !DeviceHost->is_quiescent())
				|| (DevicePersistence && !DevicePersistence->is_quiescent())
				|| (WorkbenchHost && !WorkbenchHost->is_quiescent())
				|| (QueueHost && !QueueHost->is_quiescent())
				|| (AppearanceHost && !AppearanceHost->is_quiescent())
				|| (ReviewSubmission && !ReviewSubmission->is_quiescent())
				|| (ReviewMutationHost && !ReviewMutationHost->is_quiescent())
				|| std::ranges::any_of(
					RetiredReviewMutationHosts,
					[](const auto& aHost) { return !aHost->is_quiescent(); })) {
				schedule_shutdown_poll();
				return;
			}

			ShutdownCleanupFailed         = Host->shutdown_cleanup_failed();
			EpochProjectionDeliveryFailed = EpochHost->projection_delivery_failed();
			ReviewSubmission.reset();
			ReviewMutationHost.reset();
			RetiredReviewMutationHosts.clear();
			AppearanceEffects.close_and_drain();
			AgentEffects.close_and_drain();
			QueueEffects.close_and_drain();
			WorkbenchEffects.close_and_drain();
			DiffEffects.close_and_drain();
			ReviewMutationEffects.close_and_drain();
			ReviewSubmissionEffects.close_and_drain();
			EpochEffects.close_and_drain();
			Provider->shutdown();
			ProviderEffects.close_and_drain();
			VaultEffects.close_and_drain();
			DeviceTransportEffects.close_and_drain();
			DeviceBindingEffects.close_and_drain();
			if (DiagnosticLog) {
				static_cast<void>(DiagnosticLog->record({ .Operation = application::diagnostic_operation::application, .Outcome = application::diagnostic_outcome::shutdown }));
			}
			if (OwnedDiagnosticLog) {
				OwnedDiagnosticLog->close();
			}
			ShutdownComplete = true;
		}

		std::shared_ptr<adapters::logging::quill_log>     OwnedDiagnosticLog;
		std::shared_ptr<application::diagnostic_log_port> DiagnosticLog;

		bool RememberDeviceSignIn = true;
#if defined(BPR_BUILD_TESTING)
		std::int64_t DeviceWallOffsetSeconds = 0;
#endif
		worker_executor                                          DeviceBindingEffects;
		std::unique_ptr<application::device_binding_store_port>  DeviceBindingStore;
		std::unique_ptr<application::device_binding_persistence> DevicePersistence;
		core::github_identity_generation                         DeviceRefreshGeneration;
		unregistered_device_authorization_transport              UnregisteredDeviceTransport;
		// The registered platforms' real wire, and the executor its background
		// work runs on. Declared executor-first so the transport can never
		// outlive the lanes it posts to. Both stay empty on unregistered
		// platforms.
		worker_executor DeviceTransportEffects;
		std::unique_ptr<adapters::github::detail::github_device_authorization_transport>
			DeviceWireTransport;
		// What the composition actually selected, for the status report: the
		// shipped capability where the real wire composed, the refusal where
		// it did not, and the fixture course's override in a tests-enabled
		// build.
		bool DeviceCapabilityRegistered = false;
#if defined(BPR_BUILD_TESTING)
		bool RegisteredDeviceCourse                     = false;
		bool DeviceRefreshCourse                        = false;
		bool SuppressDeviceBrowser                      = false;
		bool DisableDeviceAuthorizationPanelComposition = false;
		// [[BPR-0515]]. Whether this run composes its device wire against the
		// closed loopback port rather than the discovered system proxy.
		bool NetworkDenied = false;
#endif
#if defined(BPR_BUILD_TESTING)
		// RFC-BPR-0017 courses. The first forces the shipped selection
		// available against the recorded fixture so the composed anonymous
		// authority can be driven end to end; the second does the same and
		// then activates an enterprise profile, where the refusal is the
		// core policy's rather than the composition's.
		bool PublicPreviewEnabledCourse = false;
		bool PublicPreviewGhesCourse    = false;
#endif
		// Whether a browser capability was bound at all, which is the fact the
		// handoff's `unavailable` answer reports and the construction cut
		// removes.
		bool                  DeviceBrowserComposed = false;
		system_browser_opener SystemBrowser;
#if defined(BPR_BUILD_TESTING)
		recording_device_authorization_browser RecordingBrowser;
#endif
		qt::event_loop_device_timer          DeviceTimer;
		device_authorization_projection_sink DeviceProjections;
		// The window's panel, adopted rather than owned: the window built it
		// and keeps it alive. Null under the construction cut, and read on
		// every publication, which is why the fan-out asks rather than binds.
		qt::device_authorization_panel* DevicePanel = nullptr;
		std::unique_ptr<composition_device_authorization_projection>
			DeviceProjectionFanOut;
		// The vault slot this run's renewable material belongs to, and what
		// composition did with it. Held so a republication of the same
		// acceptance cannot bind twice or renew on every update.
		std::optional<application::github_credential_authority_key>
			DeviceRefreshAuthority;
		// [[BPR-0550]]. Whether the device presentation last seen was
		// `authorized`, so the memo above is dropped on the transition into it.
		bool        DeviceStageAuthorized     = false;
		std::size_t DeviceRefreshBindings     = 0U;
		std::size_t DeviceRefreshResumes      = 0U;
		std::size_t DeviceRefreshPublications = 0U;
		application::device_authorization_refresh_observation
			DeviceRefreshObservation;
		std::unique_ptr<application::device_authorization_clock_port>
			DeviceClock;
		std::unique_ptr<application::github_host_device_authorization_enrollment>
			DeviceEnrollment;
#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<counting_device_authorization_enrollment>
			CountedDeviceEnrollment;
#endif
		std::unique_ptr<application::device_authorization_application_host>
														DeviceHost;
		std::deque<validated_github_host_profile>       Profiles;
		std::vector<application::github_profile_option> ProfileOptions;
		worker_executor                                 ProviderEffects;
		worker_executor                                 VaultEffects;
		worker_executor                                 ReviewMutationEffects;
		worker_executor                                 ReviewSubmissionEffects;
#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<detail::github_public_path_fixture_components> Fixture;
		std::shared_ptr<detail::review_submission_fixture_state>       ReviewSubmissionServer;
		std::unique_ptr<gated_review_mutation_decision_executor>       ReviewSubmissionDecisionGate;
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
			int                          ReverseLeftRow              = -1;
			int                          ReverseRightRow             = -1;
			bool                         RefuseCorrelatedPersistence = false;
			std::size_t                  RefusedPersistenceAttempts  = 0U;
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
		std::size_t                          FixtureDiffHelperInvocations           = 0U;
		bool                                 DisableWorkbenchSelectionWire          = false;
		bool                                 DisableReviewLedgerGestureWire         = false;
		bool                                 DisableAgentProcessLauncherComposition = false;
		bool                                 DisableAgentHostComposition            = false;
		bool                                 DisableAgentGestureWire                = false;
		bool                                 DisableAgentFindingHostComposition     = false;
		bool                                 DisableChatSessionHostComposition      = false;
		bool                                 DisableCommentWorkshopComposition      = false;
		bool                                 DisableMcpHelperComposition            = false;
		bool                                 DisableSideChatPanelComposition        = false;
		bool                                 DisableFindingQueueComposition         = false;
		std::optional<std::filesystem::path> ExternalAgentExecutable;
		std::optional<std::filesystem::path> CodexAgentExecutable;
		bool                                 ReviewLedgerWasPresentOnEntry     = false;
		bool                                 ReviewLedgerRoundTripPassed       = false;
		bool                                 FixtureReviewMutationHostComposed = false;
#endif
		std::unique_ptr<application::github_credential_vault_port> OwnedVault;
		std::shared_ptr<github_wire_transport_factory_port>        TransportFactory;
		// [[BPR-0316]]. What the anonymous provider was constructed with, kept
		// so the live-candidate evidence can compare it against the shipped
		// factory this composition built rather than assert a call site.
		std::shared_ptr<github_wire_transport_factory_port>
			AnonymousTransportFactory;
#if defined(BPR_BUILD_TESTING)
		std::shared_ptr<shipped_anonymous_wire_transport_factory>
			 ShippedAnonymousTransports;
		bool PublicPreviewSelectionAvailable = false;
#endif
		std::unique_ptr<github_provider> Provider;
		// RFC-BPR-0017's credential-free authorities. Declared beside the
		// authenticated provider they are constructed beside, and after it, so
		// they are destroyed first and the counting wrapper can never outlive
		// the provider it forwards to.
		std::unique_ptr<github_provider>                       AnonymousProvider;
		std::unique_ptr<counting_anonymous_pull_requests>      CountedAnonymousPullRequests;
		std::unique_ptr<anonymous_git_smart_transport_factory> AnonymousSmartGit;
		// Whether the selection handed those authorities to the host. False on
		// every shipped platform in this slice, which is what makes the mode
		// answer `public_preview_unavailable` with nothing sent.
		bool PublicPreviewComposed = false;
		// Sign-in gestures the host forwarded. Evidence that the refusal
		// leaves the reviewer somewhere to go rather than nowhere.
		std::size_t                    PublicPreviewSignInRequests = 0U;
		std::size_t                    PublicPreviewWriteRefusals  = 0U;
		public_preview_projection_sink PublicPreviewProjections;
		std::unique_ptr<composition_public_preview_projection>
																	 PublicPreviewProjectionFanOut;
		qt::event_loop_executor                                      DecisionLane;
		std::unique_ptr<qt::workbench_window>                        Window;
		std::unique_ptr<review_submission_workflow>                  ReviewSubmission;
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
		worker_executor                                                McpEffects;
		worker_executor                                                WorkbenchEffects;
		worker_executor                                                AppearanceEffects;
		worker_executor                                                QueueEffects;
		std::unique_ptr<application::epoch_application_host>           EpochHost;
		std::unique_ptr<QTemporaryDir>                                 AgentWorkspaceRoot;
		std::filesystem::path                                          AgentWorkspaceNeighbourhood;
		std::string                                                    LastAgentWorkspacePath;
		std::atomic<bool>                                              AgentWorkspaceRecoveryPending{ false };
		std::atomic<bool>                                              AgentWorkspaceRecoveryPostFailed{ false };
		std::optional<adapters::git::installed_agent_workspace_parent>
																 AgentWorkspaceParent;
		std::unique_ptr<adapters::agent::agent_process_launcher> AgentProcess;
#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<composition_agent_process_launcher_probe> AgentProcessProbe;
#endif
		std::unique_ptr<composition_agent_invocation_projection> AgentProjections;
		std::unique_ptr<application::agent_invocation_host>      AgentHost;
		// BPR-0532. Declared after the epoch host it resolves and materializes
		// through, so it is destroyed before that host is.
		std::unique_ptr<application::agent_realignment_port> Realigner;
		/// The stop handle of the one outstanding continuation re-alignment.
		/// Held so shutdown and the wall-clock bound can stop the same request
		/// the completion is waiting on, and reset by that completion so a
		/// late timer stops nothing.
		std::shared_ptr<std::stop_source> ContinuationRealignmentStop;
		/// Counted rather than flagged, and atomic because the worker that
		/// posts the completion decrements it when the lane refuses the post.
		/// Shutdown waits on this: a re-alignment in flight holds ports that
		/// close_and_drain would otherwise pull out from under it.
		std::atomic_size_t ContinuationRealignmentsInFlight{ 0U };
		bool               ContinuationRealignmentDeadlineFired = false;
		/// Why the last continued turn froze what it froze: `none` when the
		/// old revision was aligned again, `deadline` when the wait bound
		/// stopped it, and the refusal's own name otherwise.
		std::string LastContinuationRealignment;
		/// Whether that re-alignment asked about a different pair than the one
		/// on screen.
		bool LastContinuationRealignmentRetargeted = false;
		// RFC-BPR-0012. Composed beside the agent host and driven by the
		// shipped window's own gestures. BPR-0513 moved the two panels into
		// the window's conversations drawer, which owns them as it owns every
		// other widget it places, so these are observers of what is on screen
		// rather than a second lifetime for it. They hold no process,
		// provider, credential, or store of their own; the only way out of
		// either is an intent this composition answers.
		qt::side_chat_panel*                                        SideChat     = nullptr;
		qt::finding_queue_view*                                     FindingQueue = nullptr;
		std::unique_ptr<composition_agent_finding_projection>       FindingProjections;
		std::unique_ptr<application::agent_finding_host>            FindingHost;
		std::unique_ptr<composition_agent_chat_projection>          ChatProjections;
		std::unique_ptr<application::agent_chat_session_host>       ChatHost;
		std::unique_ptr<composition_comment_workshop_projection>    WorkshopProjections;
		std::unique_ptr<application::agent_comment_workshop_host>   WorkshopHost;
		std::unique_ptr<adapters::agent::agent_process_launcher>    McpProcess;
		std::unique_ptr<application::mcp_context_helper_supervisor> McpHelper;
		std::filesystem::path                                       McpSnapshotRoot;
		std::size_t                                                 CommentWorkshopProposals = 0U;
		/// The operation the reviewer last asked the workshop for, and the
		/// turn started to answer it, spent by that turn's outcome and by no
		/// other. Absent is the ordinary state, and it is what makes a
		/// promoted finding a proposal nobody workshopped.
		std::optional<pending_comment_workshop>                         PendingWorkshopOperation;
		std::size_t                                                     FindingRefusalsPresented = 0U;
		std::size_t                                                     SideChatIntents          = 0U;
		std::size_t                                                     SideChatPresentations    = 0U;
		std::size_t                                                     SideChatTurns            = 0U;
		std::size_t                                                     SideChatRefusals         = 0U;
		std::size_t                                                     FrozenSnapshotWrites     = 0U;
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
		std::unique_ptr<composition_github_projection>                  GithubProjectionFanOut;
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

		/// The last turn this composition handed to the launch path, kept so a
		/// course can read what was dispatched. It cannot be read back off the
		/// chat host afterwards: the launch path admits a session of its own,
		/// so by the time a turn has run the host is holding a different one.
		std::optional<application::agent_chat_turn_request> LastChatTurn;
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
		const auto displayedSpanCount
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
			.DisplayedLeftSpans = displayedSpanCount(
				core::diff_side::left),
			.DisplayedRightSpans = displayedSpanCount(
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
			.DeviceAuthorizationHostComposed
			= Implementation->DeviceHost != nullptr,
			.DeviceAuthorizationStage
			= Implementation->DeviceProjections.last().Stage,
			.DeviceAuthorizationCapabilityRegistered
			= Implementation->DeviceCapabilityRegistered,
			.DeviceAuthorizationTransportReached
			= Implementation->UnregisteredDeviceTransport.reached(),
			.DeviceAuthorizationPublications
			= Implementation->DeviceProjections.published(),
			.DeviceAuthorizationBrowserComposed
			= Implementation->DeviceBrowserComposed,
			.DeviceAuthorizationHandoff
			= Implementation->DeviceProjections.last_attempt(),
			.DeviceAuthorizationBrowserOpens
			= Implementation->device_browser_opens(),
			.DeviceAuthorizationVaultStores
			= Implementation->device_vault_stores(),
			.DeviceAuthorizationDurableCopies
			= Implementation->device_durable_copies(),
			.DeviceAuthorizationTransportSubmissions
			= Implementation->device_transport_submissions(),
			.DeviceAuthorizationVaultStoredDigest
			= Implementation->device_vault_stored_digest(),
			.DeviceAuthorizationVaultStoredIsGrant
			= Implementation->device_vault_stored_is_grant(),
			.DeviceAuthorizationEnrollments
			= Implementation->device_enrollments(),
			.DeviceAuthorizationPanelComposed
			= Implementation->DevicePanel != nullptr,
			.DeviceAuthorizationRefreshRotations
			= Implementation->DeviceRefreshObservation.Rotations,
			.DeviceAuthorizationRefreshState
			= Implementation->DeviceRefreshObservation.State,
			.DeviceAuthorizationRefreshBindings
			= Implementation->DeviceRefreshBindings,
			.DeviceAuthorizationRefreshResumes
			= Implementation->DeviceRefreshResumes,
			.PublicPreviewComposed = Implementation->PublicPreviewComposed,
			.PublicPreviewUnavailable
			= Implementation->public_preview_unavailable(),
			.PublicPreviewPolicyRefused
			= Implementation->public_preview_policy_refused(),
			.PublicPreviewSignedOut = Implementation->public_preview_signed_out(),
			.PublicPreviewPublications
			= Implementation->PublicPreviewProjections.published(),
			.PublicPreviewRequests = Implementation->public_preview_requests(),
			.PublicPreviewHeaderEffects
			= Implementation->public_preview_header_effects(),
			.PublicPreviewVaultEffects
			= Implementation->public_preview_vault_effects(),
			.PublicPreviewSmartAcquisitions
			= Implementation->AnonymousSmartGit == nullptr
				? 0U
				: Implementation->AnonymousSmartGit->acquisitions(),
			.PublicPreviewSignInRequests
			= Implementation->PublicPreviewSignInRequests,
			.PublicPreviewReviewCommentsObserved
			= Implementation->PublicPreviewProjections.last().ReviewComments
				== application::public_preview_observation::observed,
			.PublicPreviewReviewCommentGroups
			= Implementation->PublicPreviewProjections.last()
				  .ReviewCommentGroups.size(),
			.PublicPreviewWriteRefusals
			= Implementation->PublicPreviewWriteRefusals,
			.PublicPreviewLiveCandidateProductionFactories
			= Implementation->public_preview_production_factories(),
			.PublicPreviewWireRequests
			= Implementation->public_preview_wire_requests(),
			.PublicPreviewWireCapabilityProbes
			= Implementation->public_preview_wire_capability_probes(),
			.PublicPreviewWireCredentialedRefusals
			= Implementation->public_preview_wire_credentialed_refusals(),
			.PublicPreviewBudgetObserved
			= Implementation->public_preview_budget().has_value(),
			.PublicPreviewBudgetLimit
			= Implementation->public_preview_budget().has_value()
				? Implementation->public_preview_budget()->Limit
				: 0U,
			.PublicPreviewBudgetRemaining
			= Implementation->public_preview_budget().has_value()
				? Implementation->public_preview_budget()->Remaining
				: 0U,
			.PublicPreviewBudgetResetUnixSeconds
			= Implementation->public_preview_budget().has_value()
				? Implementation->public_preview_budget()->ResetUnixSeconds
				: 0U,
			.PublicPreviewSnapshotPublicPreview
			= Implementation->public_preview_snapshot_signed_out(),
			.PublicPreviewSnapshotHeadOid
			= Implementation->public_preview_snapshot_head_oid(),
		};
	}

	void github_application::select_device_authorization()
	{
		Implementation->select_device_authorization();
	}

	void github_application::cancel_device_authorization()
	{
		Implementation->cancel_device_authorization();
	}

	void github_application::open_device_verification_address()
	{
		Implementation->open_device_verification_address();
	}

	void github_application::activate_public_preview(const bool aEnterprise)
	{
		Implementation->activate_public_preview(aEnterprise);
	}

	void github_application::attempt_public_preview_writes()
	{
		Implementation->attempt_public_preview_writes();
	}

	auto github_application::public_preview_wire_operations() const
		-> std::vector<public_preview_wire_operation_record>
	{
		return Implementation->public_preview_wire_operations();
	}

	auto public_preview_single_attempt_self_check() noexcept -> bool
	{
#if defined(BPR_BUILD_TESTING)
		// [[BPR-0316]]. Exercise the real wrapper without a network: a typed
		// failure stops retries, and successful pages share one request cap.
		class stub_wire_transport final
			: public adapters::github::detail::github_wire_transport_port
		{
		public:
			explicit stub_wire_transport(const bool aFail) noexcept
				: Fail{ aFail }
			{
			}

			[[nodiscard]] auto perform(
				const adapters::github::detail::github_wire_request& aRequest,
				std::stop_token                                      aStop)
				-> adapters::github::detail::github_wire_completion override
			{
				static_cast<void>(aStop);
				++Forwarded;
				if (!Fail) {
					return adapters::github::detail::github_wire_success{
						.Correlation = aRequest.Correlation,
						.Response    = { .Status = 200 },
					};
				}
				return adapters::github::detail::github_wire_error{
					.Correlation = aRequest.Correlation,
					.Error       = application::github_provider_error{
						.Failure
						= application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			}

			[[nodiscard]] auto perform_capability_probe(
				const adapters::github::detail::
					github_capability_probe_wire_request& aRequest,
				std::stop_token                           aStop)
				-> adapters::github::detail::
					github_capability_probe_wire_completion override
			{
				static_cast<void>(aStop);
				return adapters::github::detail::
					github_capability_probe_wire_cancelled{
						.Correlation = aRequest.Correlation,
					};
			}

			[[nodiscard]] auto forwarded() const noexcept -> std::size_t
			{
				return Forwarded;
			}

		private:
			bool        Fail;
			std::size_t Forwarded = 0U;
		};

		try {
			auto inner    = std::make_shared<stub_wire_transport>(true);
			auto requests = std::make_shared<std::atomic<std::size_t>>(0U);
			auto probes   = std::make_shared<std::atomic<std::size_t>>(0U);
			auto log      = std::make_shared<anonymous_wire_operation_log>();
			auto counting = counting_anonymous_wire_transport{ inner,
															   requests,
															   probes,
															   log };
			auto request  = adapters::github::detail::github_wire_request{ };
			request.Correlation.Operation
				= adapters::github::detail::read_operation::get_pull_request;
			request.Correlation.Page = 1U;

			const auto first = counting.perform(request, std::stop_token{ });
			const auto forwardedFirst
				= std::holds_alternative<
					  adapters::github::detail::github_wire_error>(first)
				&& inner->forwarded() == 1U
				&& requests->load(std::memory_order_relaxed) == 1U
				&& log->records().size() == 1U
				&& log->records().front().Outcome == "error_network";

			const auto second = counting.perform(request, std::stop_token{ });
			const auto refusedSecond
				= std::holds_alternative<
					  adapters::github::detail::github_wire_cancelled>(second)
				&& inner->forwarded() == 1U
				&& requests->load(std::memory_order_relaxed) == 1U
				&& log->records().size() == 1U;

			// An all-successful page stream must stop after nine forwards,
			// even if the factory creates another wrapper over the same budget.
			auto successful   = std::make_shared<stub_wire_transport>(false);
			auto pageRequests = std::make_shared<std::atomic<std::size_t>>(0U);
			auto pageLog      = std::make_shared<anonymous_wire_operation_log>();
			auto pages        = counting_anonymous_wire_transport{
				successful, pageRequests, probes, pageLog
			};
			for (auto page = std::uint32_t{ 1 }; page <= 9U; ++page) {
				request.Correlation.Page = page;
				if (!std::holds_alternative<adapters::github::detail::github_wire_success>(
						pages.perform(request, std::stop_token{ }))) {
					return false;
				}
			}
			auto replacement = counting_anonymous_wire_transport{
				successful, pageRequests, probes, pageLog
			};
			request.Correlation.Page = 10U;
			const auto refusedPage   = replacement.perform(request, std::stop_token{ });
			const auto refusedRetry  = pages.perform(request, std::stop_token{ });
			return forwardedFirst && refusedSecond
				&& std::holds_alternative<adapters::github::detail::github_wire_cancelled>(refusedPage)
				&& std::holds_alternative<adapters::github::detail::github_wire_cancelled>(refusedRetry)
				&& successful->forwarded() == 9U
				&& pageRequests->load(std::memory_order_relaxed) == 9U
				&& pageLog->records().size() == 9U;
		} catch (...) {
			return false;
		}
#else
		return false;
#endif
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

	auto detail::github_application_fixture_access::
		make_public_preview_selection_available(
			github_application_config aConfig)
			-> std::unique_ptr<github_application>
	{
		return std::unique_ptr<github_application>{ new github_application{
			std::make_unique<github_application::implementation>(
				std::move(aConfig),
				std::nullopt,
				detail::github_public_path_fixture_options{
					.PublicPreviewSelectionAvailable = true,
				}) } };
	}

	void detail::github_application_fixture_access::hold_review_submission_terminals(github_application& aApplication, bool aHold)
	{
		auto& gate = aApplication.Implementation->ReviewSubmissionDecisionGate;
		if (gate) {
			if (aHold) {
				gate->hold();
			} else {
				gate->release();
			}
		}
	}
	auto detail::github_application_fixture_access::held_review_submission_terminals(const github_application& aApplication) -> std::size_t
	{
		const auto& gate = aApplication.Implementation->ReviewSubmissionDecisionGate;
		return gate ? gate->pending() : 0U;
	}

	auto detail::github_application_fixture_access::device_binding_store_idle(const github_application& aApplication) noexcept -> bool
	{
		return aApplication.Implementation->DevicePersistence && aApplication.Implementation->DevicePersistence->is_quiescent();
	}

	auto detail::github_application_fixture_access::device_binding_durable(const github_application& aApplication) noexcept -> bool
	{
		return aApplication.Implementation->Host->accepted_device_binding_durable();
	}

	auto detail::github_application_fixture_access::large_review_snapshot(
		const github_application& aApplication) -> std::optional<application::github_pull_request_snapshot>
	{
		return aApplication.Implementation->Host->accepted_snapshot();
	}

	auto detail::github_application_fixture_access::diff_work_evidence(
		const github_application& aApplication) noexcept -> diff_work_fixture_evidence
	{
		const auto& implementation = *aApplication.Implementation;
		const auto& host           = *implementation.DiffHost;
		return {
			.OutstandingJobs              = host.outstanding_jobs(),
			.WorkbenchOutstandingRequests = implementation.WorkbenchHost->outstanding_requests(),
			.CacheEntries                 = host.cache_entries(),
			.CacheBytes                   = host.cache_bytes(),
			.FixtureDiffHelperInvocations = implementation.FixtureDiffHelperInvocations,
			.Pending                      = host.state().Pending.has_value(),
			.NavigationReady              = implementation.WorkbenchHost->state().Navigation.has_value(),
			.Quiescent                    = host.is_quiescent(),
		};
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
				result.push_back(digits[(static_cast<unsigned>(byte) >> 4U) & 0x0fU]);
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
				&& !aThread.Outdated
				&& (aThread.DiffSide == application::github_diff_side::left
					|| aThread.DiffSide == application::github_diff_side::right)
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
			const auto identity = defect_identity(review_ledger_probe_thread_identity());
			const auto side     = aThread.DiffSide == application::github_diff_side::left
				? core::diff_side::left
				: core::diff_side::right;
			const auto anchor   = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = side,
				.Line       = *aThread.Line - 1U,
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
			const auto identity = defect_identity(review_ledger_probe_thread_identity());
			const auto side     = aThread.DiffSide == application::github_diff_side::left
				? core::diff_side::left
				: core::diff_side::right;
			const auto anchor   = core::source_line_ref{
				.Comparison = aComparison,
				.Side       = side,
				.Line       = *aThread.Line - 1U,
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
			auto wrongSide                        = *expected;
			wrongSide.History.Defect.Anchor.Side  = aThread.DiffSide
					== application::github_diff_side::left
				? core::diff_side::right
				: core::diff_side::left;
			wrongSide.History.OriginalAnchor.Side
				= wrongSide.History.Defect.Anchor.Side;
			return review_ledger_probe_defect_matches_thread(
					   *expected, aFile, aComparison, aThread)
				&& !review_ledger_probe_defect_matches_thread(
					   wrongLine, aFile, aComparison, aThread)
				&& !review_ledger_probe_defect_matches_thread(
					   wrongSide, aFile, aComparison, aThread);
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
					auto defect = review_ledger_probe_defect(
						probe.File, storedComparison, *defectThread);
					// The reuse course advances to head two. The conversation-backed
					// defect from head one must survive, but its old comparison can no
					// longer prove an exact placement.
					if (defect.has_value()) {
						defect->History.Availability
							= core::defect_anchor_availability::outdated;
					}
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
						&& ledger->statistics().PrivateWrites == 1U;
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
		const auto  secondIdentity       = std::string{ "review-thread:PRRT_fixture_3" };
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
		const auto anchorMatchesThread = [implementation](
											 const application::durable_defect_record* const aDefect,
											 const std::string_view                          aConversation) {
			const auto* const thread = implementation->accepted_thread(aConversation);
			if (aDefect == nullptr || thread == nullptr || thread->Outdated
				|| !thread->Line.has_value() || *thread->Line == 0U) {
				return false;
			}
			const auto side = thread->DiffSide == application::github_diff_side::left
				? std::optional{ core::diff_side::left }
				: thread->DiffSide == application::github_diff_side::right
				? std::optional{ core::diff_side::right }
				: std::nullopt;
			return side.has_value()
				&& aDefect->History.OriginalAnchor.Side == *side
				&& aDefect->History.OriginalAnchor.Line == *thread->Line - 1U;
		};

		if (aCourse == detail::defect_composition_probe::reopen) {
			const auto* const first  = findDefect(firstDefectIdentity);
			const auto* const second = findDefect(secondDefectIdentity);
			if (first == nullptr || second == nullptr) {
				return result::pending;
			}
			probe.Evidence.ExactThreads = anchorMatchesThread(first, firstIdentity)
				&& anchorMatchesThread(second, secondIdentity);
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

		auto* const model = window->conversation_model();
		auto* const view  = window->conversation_view();
		if (!model->index_for_id(firstIdentity).isValid()
			|| !model->index_for_id(secondIdentity).isValid()
			|| !model->index_for_id(fileIdentity).isValid()) {
			return result::pending;
		}
		const auto selectConversation = [window, model, view](
											const std::string_view aIdentity) {
			window->conversation_panel()->set_anchor_line(std::nullopt);
			const auto index = model->index_for_id(aIdentity);
			if (!index.isValid()) {
				return false;
			}
			view->setCurrentIndex(index);
			return true;
		};

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
			if (!implementation->SelectedDiffPublication.has_value()
				|| window->comparison_surface()->current_result() == nullptr) {
				return result::pending;
			}
			if (!selectConversation(firstIdentity)
				|| !window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			if (!selectConversation(secondIdentity)
				|| !window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			if (!selectConversation(fileIdentity)
				|| window->classify_defect_action()->isEnabled()) {
				return fail();
			}
			window->classify_defect_action()->trigger();
			const auto* const first     = findDefect(firstDefectIdentity);
			const auto* const second    = findDefect(secondDefectIdentity);
			probe.Evidence.ExactThreads = anchorMatchesThread(first, firstIdentity)
				&& anchorMatchesThread(second, secondIdentity);
			probe.Evidence.LocalClassification
				= implementation->Fixture->review_mutation_evidence().Submissions == 0U;
			probe.Evidence.CorrelatedEvidence = first != nullptr && second != nullptr
				&& first->History.Defect.State == core::defect_state::open
				&& second->History.Defect.State
					== core::defect_state::needs_verification;
			probe.Evidence.DurableHistory = second != nullptr
				&& second->History.Availability
					== core::defect_anchor_availability::available
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
			probe.ReverseLeftRow              = rightRow;
			probe.ReverseRightRow             = leftRow;
			probe.RefuseCorrelatedPersistence = true;
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
		const auto& reversedResult
			= window->comparison_surface()->current_result();
		if (reversedResult == nullptr
			|| reversedResult->Alignment.Digest
				!= core::comparison_digest(
					implementation->SelectedDiffPublication->Key)) {
			return result::pending;
		}
		if (!selectConversation(firstIdentity)) {
			return fail();
		}
		const auto defectCountBeforeRefusal = implementation->DurableDefects.size();
		if (window->comparison_surface()->conversation_anchor().has_value()
			|| window->classify_defect_action()->isEnabled()
			|| defectCountBeforeRefusal != 2U) {
			return fail();
		}
		window->classify_defect_action()->trigger();
		if (implementation->DurableDefects.size() != defectCountBeforeRefusal) {
			return fail();
		}
		const auto* first  = findDefect(firstDefectIdentity);
		const auto* second = findDefect(secondDefectIdentity);
		if (first == nullptr || second == nullptr
			|| first->History.Defect.State
				!= core::defect_state::open
			|| second->History.Defect.State
				!= core::defect_state::needs_verification
			|| first->History.Availability
				!= core::defect_anchor_availability::outdated
			|| second->History.Availability
				!= core::defect_anchor_availability::outdated) {
			return fail();
		}
		probe.Evidence.PersistFailureFailsClosed
			= probe.RefusedPersistenceAttempts >= 2U;
		probe.RefuseCorrelatedPersistence = false;
		if (!probe.Evidence.PersistFailureFailsClosed) {
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
		const auto file = core::file_identity_of("review.cpp");
		if (!file.valid()) {
			return false;
		}
		implementation->request_diff(
			core::file_comparison_key{
				.Authority   = *state.Authority,
				.Repository  = state.Authority->Repository,
				.PullRequest = state.Authority->PullRequest,
				.LeftAnchor  = state.AcceptedComparison->Left.Identity,
				.RightAnchor = state.AcceptedComparison->Right.Identity,
				.LeftBlob    = leftBlob,
				.RightBlob   = rightBlob,
				.File        = file,
				.Options     = *digest,
				.Algorithm   = core::supported_diff_algorithm_revision,
			},
			*state.AcceptedComparison,
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

	auto detail::github_application_fixture_access::finding_queue(
		const github_application& aApplication) noexcept -> qt::finding_queue_view*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation == nullptr ? nullptr
										 : implementation->FindingQueue;
	}

	auto detail::github_application_fixture_access::side_chat_panel(
		const github_application& aApplication) noexcept -> qt::side_chat_panel*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation == nullptr ? nullptr
										 : implementation->SideChat;
	}

	auto detail::github_application_fixture_access::start_mcp_context_helper(
		github_application& aApplication, std::string aRequest)
		-> application::mcp_helper_refusal
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->McpHelper == nullptr) {
			return application::mcp_helper_refusal::no_helper;
		}
		return implementation->McpHelper->start(std::move(aRequest));
	}

	auto detail::github_application_fixture_access::agent_chat_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_chat_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		auto evidence = detail::agent_chat_composition_evidence{
			.ChatHostComposed      = implementation->ChatHost != nullptr,
			.SideChatPanelComposed = implementation->SideChat != nullptr,
			.Intents               = implementation->SideChatIntents,
			.Presentations         = implementation->SideChatPresentations,
			.Turns                 = implementation->SideChatTurns,
			.RefusalsPresented     = implementation->SideChatRefusals,
			.Statistics            = implementation->ChatHost != nullptr
				? implementation->ChatHost->statistics()
				: application::agent_chat_session_statistics{ },
		};
		evidence.LastTurn = implementation->LastChatTurn;
		if (implementation->ChatHost != nullptr) {
			evidence.Session    = implementation->ChatHost->session();
			evidence.Observed   = implementation->ChatHost->observed();
			evidence.Projection = implementation->ChatHost->projection();
		}
		if (implementation->SideChat != nullptr) {
			evidence.SendEnabled
				= implementation->SideChat->send_action() != nullptr
					&& implementation->SideChat->send_action()->isEnabled()
				? "enabled"
				: "disabled";
			evidence.SessionStateText
				= implementation->SideChat->session_state_text().toStdString();
			evidence.HeadOidText
				= implementation->SideChat->head_oid_text().toStdString();
		}
		if (implementation->Window != nullptr) {
			if (auto* const panel = implementation->Window->agent_panel();
				panel != nullptr) {
				evidence.AgentPanelStatusText
					= panel->status_text().toStdString();
			}
		}
		return evidence;
	}

	auto detail::github_application_fixture_access::dispatch_side_chat_send(
		github_application& aApplication, std::string aText) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->SideChat == nullptr
			|| implementation->ChatHost == nullptr) {
			return false;
		}
		const auto shown = implementation->ChatHost->projection();
		if (!shown.has_value()) {
			return false;
		}
		implementation->dispatch_side_chat_intent(qt::send_chat_message_intent{
			.Session = shown->Session.Id,
			.Action  = core::agent_action_intent{ shown->EntryPoint },
			.Scope   = shown->Chat.Scope,
			.Text    = std::move(aText),
		});
		return true;
	}

	auto detail::github_application_fixture_access::
		dispatch_side_chat_private_context(
			github_application&          aApplication,
			const core::agent_session_id aSession,
			const bool                   aNotes,
			const bool                   aDrafts) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ChatHost == nullptr) {
			return false;
		}
		implementation->dispatch_side_chat_intent(qt::chat_private_context_intent{
			.OptIn = {
				.Session = aSession,
				.Notes   = aNotes,
				.Drafts  = aDrafts,
			},
		});
		return true;
	}

	auto detail::github_application_fixture_access::agent_finding_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_finding_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		auto evidence = detail::agent_finding_composition_evidence{
			.FindingHostComposed = implementation->FindingHost != nullptr,
			.CommentWorkshopComposed
			= implementation->WorkshopHost != nullptr,
			.McpHelperComposed     = implementation->McpHelper != nullptr,
			.SideChatPanelComposed = implementation->SideChat != nullptr,
			.FindingQueueComposed  = implementation->FindingQueue != nullptr,
			.QueuedFindings        = implementation->FindingQueue != nullptr
				? implementation->FindingQueue->finding_count()
				: 0U,
			.WorkshopProposals     = implementation->CommentWorkshopProposals,
			.RefusalsPresented     = implementation->FindingRefusalsPresented,
			.SideChatPresentations = implementation->SideChatPresentations,
			.SideChatIntents       = implementation->SideChatIntents,
			.FrozenSnapshotWrites  = implementation->FrozenSnapshotWrites,
			.FrozenFileCount       = implementation->frozen_snapshot_files().size(),
			.FindingStatistics     = implementation->FindingHost != nullptr
				? implementation->FindingHost->statistics()
				: application::agent_finding_host_statistics{ },
			.WorkshopStatistics    = implementation->WorkshopHost != nullptr
				? implementation->WorkshopHost->statistics()
				: application::agent_comment_workshop_statistics{ },
			.McpStatistics         = implementation->McpHelper != nullptr
				? implementation->McpHelper->statistics()
				: application::mcp_helper_supervisor_statistics{ },
			.McpLaunch             = implementation->McpHelper != nullptr
				? implementation->McpHelper->describe()
				: std::nullopt,
			.McpAnswer             = implementation->McpHelper != nullptr
				? implementation->McpHelper->answer()
				: std::string{ },
			.LastContinuationRealignment
			= implementation->LastContinuationRealignment,
			.LastContinuationRealignmentRetargeted
			= implementation->LastContinuationRealignmentRetargeted,
		};
		if (implementation->FindingQueue != nullptr) {
			evidence.QueuedClaim
				= implementation->FindingQueue->claim_text().toStdString();
			evidence.QueuedAnchorState
				= implementation->FindingQueue->anchor_state_text().toStdString();
		}
		if (implementation->ReviewDraft.has_value()) {
			evidence.LocalDraftBody = implementation->ReviewDraft->Body;
		}
		if (implementation->PendingWorkshopOperation.has_value()) {
			evidence.WorkshopPendingOperation = std::string{ core::stable_name(
				implementation->PendingWorkshopOperation->Operation) };
			evidence.WorkshopPendingSessionBound
				= implementation->PendingWorkshopOperation->Session.has_value();
		}
		if (implementation->Window != nullptr) {
			if (auto* const editor = implementation->Window->review_editor();
				editor != nullptr) {
				evidence.WorkshopOriginalText
					= editor->workshop_original_text().toStdString();
				evidence.WorkshopProposalText
					= editor->workshop_proposal_text().toStdString();
				evidence.WorkshopOperationText
					= editor->workshop_operation_text().toStdString();
			}
			if (auto* const panel = implementation->Window->agent_panel();
				panel != nullptr) {
				evidence.AgentPanelStatusText
					= panel->status_text().toStdString();
			}
		}
		if (implementation->SideChat != nullptr) {
			evidence.SideChatHeadOidText
				= implementation->SideChat->head_oid_text().toStdString();
		}
		return evidence;
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
			.AgentProcessLauncherComposed = implementation->AgentProcess != nullptr,
			.AgentProcessLauncherInvocations
			= implementation->AgentProcessProbe != nullptr
				? implementation->AgentProcessProbe->invocations()
				: 0U,
			.HostComposed = implementation->AgentHost != nullptr,
			.Quiescent    = implementation->AgentHost == nullptr
				|| implementation->AgentHost->is_quiescent(),
			.Statistics    = implementation->AgentHost != nullptr
				? implementation->AgentHost->statistics()
				: application::agent_invocation_host_statistics{ },
			.WorkspaceRoot = std::filesystem::path{ implementation->LastAgentWorkspacePath }.parent_path(),
		};
	}
#endif
}
