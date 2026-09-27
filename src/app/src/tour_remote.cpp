#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/review_tour_source.hpp"
#include "bpr/core/review_preparation.hpp"
#include "github_cli_runtime.hpp"
#include "github_device_authorization_transport.hpp"
#include "github_provider.hpp"
#include "live_review_submission_provider.hpp"
#include "native_https_transport.hpp"
#include "platform_credential_vault.hpp"
#include "tour_cancellation.hpp"
#include "tour_decisions.hpp"
#include "tour_runtime.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
namespace bpr::app {
	namespace {
		using namespace adapters::github;
		using namespace adapters::github::detail;
		class terminal_timer final : public application::device_authorization_timer_port
		{
		public:
			terminal_timer()
			{
				Timer.setSingleShot(true);
				QObject::connect(&Timer, &QTimer::timeout, [this] {auto callback=std::move(Expiry);if(callback){callback();} });
			}
			auto schedule_after(std::uint32_t aSeconds, std::function<void()> aExpiry) -> bool override
			{
				Expiry = std::move(aExpiry);
				Timer.start(static_cast<int>(aSeconds * 1000U));
				return true;
			}
			void cancel() noexcept override
			{
				Timer.stop();
				Expiry = { };
			}

		private:
			QTimer                Timer;
			std::function<void()> Expiry;
		};
		class terminal_device_projection final : public application::device_authorization_projection_port
		{
		public:
			void publish(const application::device_authorization_presentation& aValue) override
			{
				if (!aValue.UserCode.empty() && aValue.UserCode != Code) {
					Code = aValue.UserCode;
					std::cerr << "Open " << aValue.VerificationUri << " in your browser and enter code " << Code << ". Waiting for authorization.\n";
				}
			}
			void publish_refresh(const application::device_authorization_refresh_observation&) override { }

		private:
			std::string Code;
		};
		class remote_reader final : public tour_remote_reader, private application::github_projection_port, private application::github_epoch_handoff_port, private application::epoch_projection_port
		{
		public:
			remote_reader(std::filesystem::path aRoot, core::bpr_profile_id aProfile)
				: Root{ std::move(aRoot) }
				, Profile{ aProfile }
			{ }
			~remote_reader() override { stop(); }
			auto connect(std::string aHostname, bool aGh, bool aInteractive) -> tour_cli_result<std::string> override
			{
				if (!aInteractive && !aGh) {
					return refuse("interaction_required", "Run bpr tour connect --host " + aHostname + " --transport native in a terminal to sign in.", 3);
				}
				if (auto error = initialize(aHostname, aGh); !error) {
					return std::unexpected(error.error());
				}
				if (aGh) {
					if (aInteractive) {
						std::cerr << "Using the explicitly selected gh executable and its configured identity within the qualified process boundary.\n";
					}
					Host->dispatch(core::activate_host_profile_intent{ HostProfile->profile() });
					wait([&] { return Host->is_quiescent(); });
				} else {
					auto connected = device_connect();
					if (!connected) {
						return std::unexpected(connected.error());
					}
				}
				if (!Host->accepted_session()) {
					return refuse("authentication_unavailable", "The selected authentication service did not admit an account. Inspect its setup and reconnect.", 5);
				}
				return std::to_string(Host->state().Account->value());
			}
			auto open(std::string aHostname, core::repository_locator aRepository, core::pull_request_number aNumber, bool aGh) -> tour_cli_result<tour_remote_read> override
			{
				if (auto error = initialize(aHostname, aGh); !error) {
					return std::unexpected(error.error());
				}
				if (aGh) {
					Host->dispatch(core::activate_host_profile_intent{ HostProfile->profile() });
				} else {
					const auto&                                 p = *HostProfile;
					const application::github_profile_authority authority{ .Profile = p.profile(), .ProfileRevision = p.revision(), .Host = p.host(), .Hostname = application::github_canonical_hostname{ p.hostname() }, .ServerContract = p.server_contract(), .Authentication = p.authentication_transport(), .ProbeContractRevision = application::github_probe_contract_revision{ 1 } };
					bool                                        restored = false;
					if (!Host->restore_device_profile(authority, [&](auto) { restored = true; })) {
						return refuse("authentication_required", "Run bpr tour connect --host " + aHostname + " --transport native.", 5);
					}
					wait([&] { return restored && Host->is_quiescent(); });
				}
				wait([&] { return Host->is_quiescent(); });
				if (!Host->accepted_session()) {
					return refuse("authentication_required", "No current account was admitted. Run bpr tour connect --host " + aHostname + " --transport " + (aGh ? "gh" : "native") + " explicitly.", 5);
				}
				Host->dispatch(core::open_pull_request_intent{ aRepository, aNumber });
				wait([&] { return Host->is_quiescent(); });
				if (!Host->accepted_snapshot() || !Handoff) {
					return refuse("read_unavailable", "The bounded paged PR read was unavailable or incomplete. No empty tour was inferred.", 7);
				}
				const auto&                    h = *Handoff;
				application::tour_saved_target target{ .Owner = { Profile, h.Host, h.HostAuthority, h.Account, h.Repository, h.PullRequest }, .Draft = core::tour_draft_id{ 1 }, .HostProfile = h.Profile, .Hostname = std::string{ h.Hostname.value() }, .Repository = h.Locator, .Number = h.Number, .Base = h.Base, .Head = h.Head };
				return tour_remote_read{ std::move(target), *Host->accepted_snapshot() };
			}

			auto publication() -> tour_cli_result<tour_publication_context> override
			{
				if (!Handoff || !Host || !Host->accepted_snapshot()) {
					return refuse("publication_unavailable", "Refresh this exact PR before publication.", 5);
				}
				const auto&                                 h = *Handoff;
				const auto&                                 s = *Host->accepted_snapshot();
				application::review_submission_live_binding binding{
					.Target         = { .Semantic = { h.Host, h.Repository, h.PullRequest, h.Head }, .Hostname = h.Hostname, .ServerContract = h.ServerContract, .Repository = h.Locator, .Number = h.Number, .RepositoryDatabaseId = s.Repository.Repository.value(), .RepositoryNodeId = std::string{ s.Repository.Node.value() }, .PullRequestDatabaseId = s.PullRequest.PullRequest.value(), .PullRequestNodeId = std::string{ s.PullRequest.Node.value() }, .BaseRepositoryDatabaseId = s.PullRequest.Repository.value(), .BaseOid = h.Base, .BaseRef = s.PullRequest.BaseRef, .HeadRef = s.PullRequest.HeadRef, .Open = s.PullRequest.State == application::github_pull_request_state::open },
					.ActingIdentity = { .Account = h.Account, .DatabaseId = h.Account.value(), .NodeId = std::string{ s.Viewer.Actor.value() }, .Login = s.Viewer.Login }
				};
				auto acceptedLease = Host->accepted_review_mutation_transport();
				if (!application::valid(binding) || !acceptedLease) {
					return refuse("publication_unavailable", "The current account, PR or mutation transport is unavailable.", 5);
				}
				const adapters::sqlite::review_mutation_recovery_configuration configuration{
					.StorageRoot = Root, .Host = std::string{ binding.Target.Hostname.value() }, .Account = std::to_string(binding.ActingIdentity.Account.value()), .Repository = binding.Target.Repository.Owner + "/" + binding.Target.Repository.Name, .PullRequest = static_cast<std::int64_t>(binding.Target.Number.value())
				};
				application::review_submission_provider_factory factory = [configuration, binding, lease = std::move(acceptedLease)]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> {
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
				// Tour operations still require the parent's exact host/transport qualification.
				return tour_publication_context{ std::move(binding), std::move(factory), Session, false, [configuration](std::string_view aKey) -> tour_cli_result<std::optional<application::staged_review_plan>> {
													auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
													if (!store) {
														return std::unexpected(tour_cli_error{ "recovery_unavailable", "The private publication journal is unavailable.", 7 });
													}
													auto record = (*store)->load_staged(std::string{ aKey });
													if (!record) {
														return std::unexpected(tour_cli_error{ "recovery_unavailable", "The saved publication record is unavailable.", 7 });
													}
													return *record ? std::optional{ (**record).Plan } : std::nullopt;
												} };
			}

			auto source(const core::tour_code_destination& aSource) -> tour_cli_result<tour_source_material> override
			{
				if (!Handoff || !core::valid(aSource)) {
					return refuse("source_unavailable", "No accepted read can authorize this source.", 5);
				}
				if (aSource.LastLine - aSource.FirstLine >= 200) {
					return refuse("source_too_large", "Select at most 200 source lines.", 2);
				}
				const auto&                                         h = *Handoff;
				const application::workbench_pull_request_authority authority{ h.ApplicationSession, h.Profile, h.ProfileRevision, h.Host, h.HostAuthority, h.Account, h.Viewer, h.Repository, h.PullRequest, h.Number, h.Snapshot, h.IdentityGeneration, h.PullRequestGeneration };
				if (!Epoch) {
					const auto git     = native_filesystem_path(QStandardPaths::findExecutable(QStringLiteral("git")));
					auto       cache   = source_effect([&] { return adapters::git::make_installed_git_cache({ .GitExecutable = git, .StorageRoot = Root }); });
					auto       catalog = source_effect([&] { return adapters::sqlite::make_epoch_catalog({ .StorageRoot = Root }); });
					if (!cache || !catalog) {
						return refuse("source_unavailable", "The existing private Git cache is unavailable.", 5);
					}
					Cache      = std::move(*cache);
					Catalog    = std::move(*catalog);
					Entropy    = application::make_system_epoch_entropy();
					EpochClock = application::make_system_epoch_clock();
					Epoch      = std::make_unique<application::epoch_application_host>(core::epoch_state{ .ApplicationSession = Session }, *Cache, *Catalog, *Entropy, *EpochClock, Effects, Decisions, static_cast<application::epoch_projection_port&>(*this));
					Epoch->accept(std::move(*Handoff));
					Handoff.reset();
					wait([&] { return Epoch->is_quiescent(); });
				}
				auto context = application::review_tour_source_context(*Epoch, authority, aSource);
				if (!context) {
					return refuse("source_unavailable", "Pinned source authority is unavailable; no working-tree text is substituted.", 5);
				}
				const application::workbench_selected_diff_request request{ .Context = { core::request_id{ 1 }, Session, core::request_generation{ 1 } }, .WorkbenchAuthority = authority, .Diff = *context, .File = { .File = core::file_identity_of(aSource.Path), .PresentationPath = aSource.Path, .CurrentPath = native_filesystem_path(QString::fromUtf8(aSource.Path)), .Change = application::workbench_file_change_kind::unchanged, .ContentKind = application::workbench_file_content_kind::text } };
				auto                                               material = source_effect([&] { return application::materialize_review_tour_source(*Epoch, *Epoch, request, aSource, { }); });
				if (!material) {
					if (material.error() == application::review_tour_source_failure::range_unavailable) {
						return refuse("source_range_unavailable", "The range is outside the pinned file.", 2);
					}
					if (material.error() == application::review_tour_source_failure::too_large) {
						return refuse("source_too_large", "Selected source exceeds its presentation bound.", 2);
					}
					return refuse("source_unavailable", "The exact pinned source is unavailable, changed, binary or not UTF-8; no working-tree text is substituted.", 5);
				}
				return std::move(*material);
			}

		private:
			auto refuse(std::string aCode, std::string aMessage, int aExit) -> std::unexpected<tour_cli_error> { return tour_cancelled() ? std::unexpected(tour_cli_error{ "cancelled", "The command was cancelled; no remote content writes were issued.", 130 }) : std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
			void publish(const application::epoch_presentation_update&) override { }
			template <typename F>
			auto source_effect(F aFunction) -> decltype(aFunction())
			{
				auto task   = std::make_shared<std::packaged_task<decltype(aFunction())()>>(std::move(aFunction));
				auto result = task->get_future();
				SourceEffects.post([task] { (*task)(); });
				wait([&] { return result.wait_for(std::chrono::milliseconds{ 0 }) == std::future_status::ready; });
				return result.get();
			}
			void publish(const application::github_presentation_update&) override { }
			void accept(application::accepted_pull_request_handoff aHandoff) override { Handoff = std::move(aHandoff); }
			void invalidate(application::pull_request_authority_invalidation) override { Handoff.reset(); }
			void wait(const std::function<bool()>& aDone)
			{
				// Underlying read/transport and device hosts own their finite time bounds.
				do {
					if (tour_cancelled() && !Cancelling) {
						Cancelling = true;
						if (Epoch) {
							Epoch->shutdown();
						}
						if (Device) {
							Device->shutdown();
						}
						if (Host) {
							Host->shutdown();
						}
						if (Provider) {
							Provider->shutdown();
						}
					}
					Decisions.drain();
					QCoreApplication::processEvents();
					if (aDone() || (Cancelling && (!Host || Host->is_quiescent()) && (!Device || Device->is_quiescent()) && (!Epoch || Epoch->is_quiescent()) && Effects.idle())) {
						break;
					}
					QThread::msleep(1);
				} while (true);
			}
			auto initialize(const std::string& aHostname, bool aGh) -> tour_cli_result<void>
			{
				if (Host) {
					return refuse("invalid_input", "A remote reader has one immutable host selection.", 2);
				}
#if !defined(__APPLE__)
				if (aGh) {
					return refuse("transport_unavailable", "The gh transport is not qualified on this platform.", 5);
				}
#endif
				const bool cloud     = aHostname == "github.com";
				auto       validated = validate_github_host_profile({ .Profile = core::host_profile_id{ cloud ? (aGh ? 2U : 1U) : (aGh ? 4U : 3U) }, .Host = core::git_host_id{ cloud ? 1U : 2U }, .Revision = application::github_profile_revision{ 1 }, .Server = cloud ? application::github_server_contract::github_com_2022_11_28 : application::github_server_contract::ghes_3_18_2022_11_28, .Authentication = aGh ? application::github_authentication_transport::github_cli : application::github_authentication_transport::native_https, .Provenance = github_profile_provenance::explicit_user, .Hostname = aHostname, .RestRoot = cloud ? "https://api.github.com" : "https://" + aHostname + "/api/v3", .GraphqlRoot = cloud ? "https://api.github.com/graphql" : "https://" + aHostname + "/api/graphql" });
				if (!validated) {
					return refuse("host_refused", "Use an explicitly approved canonical GitHub or GHES hostname.", 5);
				}
				HostProfile                                                 = std::make_unique<validated_github_host_profile>(*validated);
				std::shared_ptr<github_wire_transport_factory_port> factory = std::make_shared<native_github_wire_transport_factory>();
#if defined(__APPLE__)
				if (aGh) {
					factory = make_opted_in_cli_transport_factory();
				}
#endif
				const std::array<const validated_github_host_profile*, 1> profiles{ HostProfile.get() };
				auto                                                      provider = make_github_provider(profiles, Effects, factory);
				if (!provider) {
					return refuse("transport_unavailable", "Could not compose the admitted read provider.", 5);
				}
				Provider = std::move(*provider);
				Vault    = make_platform_vault(Effects, Profile);
				if (!Vault) {
					return refuse("vault_unavailable", "This platform has no available headless credential vault.", 5);
				}
				Session      = core::application_session_id{ QRandomGenerator::securelySeeded().generate64() | 1U };
				BindingStore = adapters::sqlite::make_device_binding_store(Root, Profile);
				Persistence  = std::make_unique<application::device_binding_persistence>(*BindingStore, Effects, Decisions);
				Host         = std::make_unique<application::github_application_host>(core::github_state{ .ApplicationSession = Session }, *Provider, *Provider, *Provider, *Provider, *Vault, Bindings, Decisions, static_cast<application::github_projection_port&>(*this), application::github_credential_shutdown_policy::retain, static_cast<application::github_epoch_handoff_port*>(this));
				Clock        = application::make_system_device_authorization_clock();
				Host->configure_device_persistence(*Persistence, [this] { return Clock->observe_wall_seconds(); });
				return { };
			}
			auto device_connect() -> tour_cli_result<void>
			{
				if (HostProfile->hostname() != "github.com") {
					return refuse("device_flow_unavailable", "The registered native device sign-in is available only on github.com. GHES requires its existing admitted credential setup.", 5);
				}
				auto wire = make_native_https_credential_free_transport(*HostProfile, { .Request = core::request_id{ 1 }, .ApplicationSession = Session, .Profile = HostProfile->profile(), .IdentityGeneration = core::github_identity_generation{ 1 } });
				if (!wire) {
					return refuse("authentication_unavailable", "Native device transport is unavailable.", 5);
				}
				auto transport = make_github_device_authorization_transport(std::shared_ptr<native_https_transport>{ std::move(*wire) }, "Iv23liitiYN1WSSbag50", Effects);
				if (!transport) {
					return refuse("authentication_unavailable", "The registered device transport could not start.", 5);
				}
				DeviceTransport = std::move(*transport);
				Enrollment      = std::make_unique<application::github_host_device_authorization_enrollment>(*Host);
				Device          = std::make_unique<application::device_authorization_application_host>(core::device_authorization_state{ .ApplicationSession = Session }, *DeviceTransport, *Vault, *Enrollment, *Clock, Timer, Decisions, DeviceProjection, application::device_authorization_host_limits{ }, application::device_authorization_browser_handoff{ }, application::device_authorization_capability{ .Registered = true, .Profile = HostProfile->profile() });
				Device->dispatch(core::begin_device_authorization_intent{ HostProfile->profile() });
				wait([&] { return !Device->state().Pending && Device->is_quiescent() && Host->is_quiescent() && Persistence->is_quiescent(); });
				return { };
			}
			void stop()
			{
				if (Epoch) {
					Epoch->shutdown();
				}
				if (Device) {
					Device->shutdown();
				}
				if (Host) {
					Host->shutdown();
				}
				if (Provider) {
					Provider->shutdown();
				}
				wait([&] { return (!Epoch || Epoch->is_quiescent()) && (!Device || Device->is_quiescent()) && (!Host || Host->is_quiescent()) && (!Persistence || Persistence->is_quiescent()) && Effects.idle(); });
				Effects.close_and_drain();
				SourceEffects.close_and_drain();
				Decisions.drain();
			}

			bool                                                 Cancelling = false;
			std::filesystem::path                                Root;
			core::bpr_profile_id                                 Profile;
			worker_executor                                      Effects;
			terminal_decisions                                   Decisions;
			worker_executor                                      SourceEffects;
			std::unique_ptr<application::git_cache_port>         Cache;
			std::unique_ptr<application::epoch_catalog_port>     Catalog;
			std::unique_ptr<application::epoch_entropy_port>     Entropy;
			std::unique_ptr<application::epoch_clock_port>       EpochClock;
			std::unique_ptr<application::epoch_application_host> Epoch;

			core::application_session_id                                              Session;
			std::unique_ptr<validated_github_host_profile>                            HostProfile;
			std::unique_ptr<github_provider>                                          Provider;
			std::unique_ptr<application::github_credential_vault_port>                Vault;
			application::github_credential_binding_catalog                            Bindings;
			std::unique_ptr<application::device_binding_store_port>                   BindingStore;
			std::unique_ptr<application::device_binding_persistence>                  Persistence;
			std::unique_ptr<application::github_application_host>                     Host;
			std::optional<application::accepted_pull_request_handoff>                 Handoff;
			std::unique_ptr<application::device_authorization_clock_port>             Clock;
			terminal_timer                                                            Timer;
			terminal_device_projection                                                DeviceProjection;
			std::unique_ptr<github_device_authorization_transport>                    DeviceTransport;
			std::unique_ptr<application::github_host_device_authorization_enrollment> Enrollment;
			std::unique_ptr<application::device_authorization_application_host>       Device;
		};
	}
	auto make_tour_remote_reader(std::filesystem::path aRoot, core::bpr_profile_id aProfile) -> std::unique_ptr<tour_remote_reader> { return std::make_unique<remote_reader>(std::move(aRoot), aProfile); }
}
