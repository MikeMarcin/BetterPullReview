#include "github_application/application_startup.hpp"

#include "bpr/adapters/sqlite/profile_activation.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/remembered_connection_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/release_maintenance_controls.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/core/profile_data_reset.hpp"
#include "bpr/core/release_maintenance.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/motion_observer.hpp"
#include "github_application/profile_hosts.hpp"
#include "github_application/profile_runtime.hpp"
#include "github_application/review_mutation_selection.hpp"
#include "github_application/startup_failures.hpp"
#include "platform_credential_vault.hpp"
#include "review_mutation_execution_authority.hpp"
#include "terminal_diagnostics.hpp"

#include <exception>
#include <stdexcept>
#include <string_view>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/native_filesystem_path.hpp"
	#include <QDesktopServices>
	#include <QDir>
	#include <QFileInfo>
	#include <QString>
	#include <QTemporaryDir>
#endif
#if defined(BPR_LOCAL_AUTOMATION)
	#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#endif

namespace bpr::app {
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

	auto compatibility_window_context(core::bpr_profile_id aProfile) -> core::bpr_window_context
	{
		auto state = core::profile_window_state{ };
		if (aProfile != core::default_bpr_profile_id) {
			state.Profiles.push_back({ { aProfile, "Profile" }, { }, { } });
			state.LastProfileId = aProfile.value();
		}
		auto catalog = core::profile_window_catalog::restore(std::move(state));
		if (!catalog) {
			throw std::runtime_error{ "compatibility window admission unavailable" };
		}
		auto launch = catalog->launch(aProfile, core::profile_launch_intent::activate);
		if (!launch) {
			throw std::runtime_error{ "compatibility window admission unavailable" };
		}
		return launch->Activated;
	}

	github_application_startup::implementation::implementation(github_application_config aConfig, github_application_startup_completion aCompletion
#if defined(BPR_BUILD_TESTING)
															   ,
															   std::optional<detail::github_public_path_fixture> aFixture,
															   detail::github_public_path_fixture_options        aOptions
#endif
															   )
		: Config{ std::move(aConfig) }
		, Completion{ std::move(aCompletion) }
		, Host{ *this, *this, Effects, Decisions, *this, core::application_session_id{ 1U } }
	{
#if defined(BPR_BUILD_TESTING)
		Fixture = aFixture;
		Options = std::move(aOptions);
		if (Fixture) {
			Config.Profile             = Options.Profile;
			Config.ProfileLaunchIntent = Options.ProfileLaunchIntent;
			Config.ApplicationDataRoot = Options.ApplicationDataRoot;
			Config.DataReset           = Options.DataReset;
			Config.DifCatalog          = Options.DifCatalog;
			Config.StatusItem          = Options.StatusItem;
			if (!Config.ApplicationDataRoot) {
				auto temporary             = std::make_shared<QTemporaryDir>(QDir::temp().filePath(QStringLiteral("bpr-public-path-XXXXXX")));
				Config.ApplicationDataRoot = temporary->isValid() ? native_filesystem_path(QFileInfo{ temporary->path() }.canonicalFilePath()) / "data" : std::filesystem::path{ };
				Scratch                    = std::move(temporary);
			}
		}
#endif
		if (!Config.MotionObserver) {
			Config.MotionObserver = std::make_shared<qt::motion_observer>(qt::make_native_motion_source());
		}
		if (!Config.DifCatalog) {
			Config.DifCatalog = std::make_shared<qt::dif_catalog_provider>();
		}
		// [[BPR-1868]]. A root typed in another letter case than its folders
		// have on a case-insensitive volume names the same folders; the
		// stores admit only a root spelled as the file system spells it.
		RegistryRoot = adapters::sqlite::storage_root_on_disk_spelling(native_application_data_root(Config));
		// [[BPR-1870]]. A named folder that holds other programs' files is
		// never narrowed; the stores keep their data in a private folder
		// the application makes inside it instead.
		const auto placed  = adapters::sqlite::storage_root_placement(RegistryRoot);
		RegistryRootNested = placed != RegistryRoot;
		RegistryRoot       = placed;
	}

	github_application_startup::implementation::~implementation()
	{
		Host.shutdown();
		Effects.close_and_drain();
	}

	void github_application_startup::implementation::begin()
	{
		const auto started = Host.start(Config.Profile);
		if (!started) {
			Decisions.post([self = shared_from_this()] { self->finish(std::unexpected(github_application_startup_failure::invalid_profile)); });
		}
	}

	void github_application_startup::implementation::cancel()
	{
		Cancelled = true;
		Host.cancel();
	}

	auto github_application_startup::implementation::is_quiescent() const noexcept -> bool { return Terminal && Host.is_quiescent() && Effects.idle(); }

	auto github_application_startup::implementation::load(std::stop_token aStop) -> application::profile_registry_result<application::profile_registry_snapshot>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(application::profile_registry_failure::cancelled);
		}
		// [[BPR-1868]]. Each refusal below names its own cause on the
		// startup-failure-source line: every one of them reaches a reader as
		// `registry_unavailable`, and a root that was only spelled in another
		// letter case, and one that already existed with a shared mode, were
		// told apart by nothing. The names are fixed text with no path.
		const auto refuse = [](const std::string_view aSource) {
			detail::note_startup_failure(aSource);
			return std::unexpected(application::profile_registry_failure::unavailable);
		};
		if (RegistryRoot.empty() || !RegistryRoot.is_absolute() || RegistryRoot.lexically_normal() != RegistryRoot) {
			return refuse("registry-root-path: the data root is not an absolute, normal path");
		}
		std::error_code error;
		const auto      status = std::filesystem::symlink_status(RegistryRoot, error);
		if (!std::filesystem::exists(status)) {
			auto ancestor = RegistryRoot.parent_path();
			while (!ancestor.empty() && !std::filesystem::exists(ancestor, error) && !error) {
				ancestor = ancestor.parent_path();
			}
			if (error || ancestor.empty() || std::filesystem::canonical(ancestor, error) != ancestor || error) {
				return refuse("registry-root-ancestor: the data root's nearest existing folder resolves through a link or another spelling");
			}
			if (!adapters::sqlite::create_storage_root_owner_private(RegistryRoot)) {
				return refuse("registry-root-create: the data root could not be created owner-private");
			}
		} else if (std::filesystem::is_symlink(status)) {
			return refuse("registry-root-link: the data root is a symbolic link");
		} else if (!std::filesystem::is_directory(status)) {
			return refuse("registry-root-not-directory: the data root exists and is not a folder");
		} else {
			// [[BPR-1870]]. Owner, 2026-11-04: "I want the app to just work".
			// A data root that is the application's own (empty, or holding
			// its `state/v1`) and this user's is narrowed in place rather
			// than refused, and says so on its own line.
			switch (adapters::sqlite::tighten_existing_storage_root(RegistryRoot)) {
			case adapters::sqlite::storage_root_tightening::already_private:
				break;
			case adapters::sqlite::storage_root_tightening::tightened:
				detail::record_terminal_diagnostic("STORAGE_PRIVATE_REPAIRED:registry-root: the data root folder was not owner-private and was made so\n");
				break;
			case adapters::sqlite::storage_root_tightening::not_owned:
				return refuse("registry-root-owner: the data root folder belongs to another account; name a folder you own, or one that does not exist yet");
			case adapters::sqlite::storage_root_tightening::foreign_content:
				return refuse("registry-root-foreign-content: other files appeared in the data root folder while it was being prepared; start again");
			case adapters::sqlite::storage_root_tightening::refused:
				break;
			}
		}
		if (RegistryRootNested) {
			detail::record_terminal_diagnostic("STORAGE_PRIVATE_NESTED:registry-root: the named data folder holds other files or is another account's, and is left unchanged; data is kept in its BetterPullReview/data folder\n");
		}
		// This only validates the registry base; it does not admit Default for runtime use.
		const auto baseAuthority = application::profile_storage_authority::select(application::profile_registry_snapshot{ }, core::default_bpr_profile_id);
		const auto base          = adapters::sqlite::resolve_profile_storage_root(RegistryRoot, *baseAuthority, aStop);
		if (!base) {
			if (aStop.stop_requested()) {
				return std::unexpected(application::profile_registry_failure::cancelled);
			}
			switch (base.error()) {
			case adapters::sqlite::profile_storage_root_failure::invalid_permissions:
				// [[BPR-1870]]. The root itself was narrowed above when it
				// could be; what is left could not be made private here.
				return refuse("registry-root-permissions: a data folder is not owner-private and could not be made so (it belongs to another account, or its mode cannot be changed); name a folder you own, or one that does not exist yet");
			case adapters::sqlite::profile_storage_root_failure::invalid_path:
				return refuse("registry-root-spelling: the data root resolves through a link or another spelling");
			case adapters::sqlite::profile_storage_root_failure::cancelled:
				return std::unexpected(application::profile_registry_failure::cancelled);
			case adapters::sqlite::profile_storage_root_failure::unavailable:
				break;
			}
			return refuse("registry-root-unavailable: the data root could not be opened, or is not on a supported local file system");
		}
		auto registryStore = adapters::sqlite::make_profile_registry_store(*base);
		return registryStore->load(aStop);
	}

	auto github_application_startup::implementation::save(const application::profile_registry_snapshot&, std::uint64_t, std::stop_token) -> application::profile_registry_result<application::profile_registry_snapshot>
	{
		return std::unexpected(application::profile_registry_failure::unavailable);
	}

	auto github_application_startup::implementation::resolve(const application::profile_storage_authority& aAuthority, std::stop_token aStop) -> application::profile_startup_result<std::filesystem::path>
	{
		const auto root = adapters::sqlite::resolve_profile_storage_root(RegistryRoot, aAuthority, aStop);
		if (!root) {
			return std::unexpected(aStop.stop_requested() ? application::profile_startup_failure::cancelled : application::profile_startup_failure::root_unavailable);
		}
		auto lease = adapters::sqlite::acquire_profile_runtime_lease(*root, aStop);
		if (!lease) {
			if (OwnerOnly && lease.error() == application::profile_runtime_lease_failure::busy) {
				const auto routed  = adapters::sqlite::send_profile_activation(*root, Config.Profile, Config.ProfileLaunchIntent, aStop);
				const auto outcome = routed ? *routed : routed.error();
				if (outcome == application::profile_activation_outcome::queued) {
					Handoff = github_application_startup_failure::activation_queued;
				} else if (outcome == application::profile_activation_outcome::refused) {
					Handoff = github_application_startup_failure::activation_refused;
				} else if (outcome == application::profile_activation_outcome::indeterminate) {
					Handoff = github_application_startup_failure::activation_indeterminate;
				}
			}
			return std::unexpected(lease.error() == application::profile_runtime_lease_failure::busy
									   ? application::profile_startup_failure::owner_busy
									   : aStop.stop_requested() ? application::profile_startup_failure::cancelled
																: application::profile_startup_failure::root_unavailable);
		}
		RuntimeLease = std::move(*lease);
		// Recovery precedes every profile store and producer. The registry has a
		// separate owner and is explicitly excluded from the purge inventory.
		if (!recover_release_profile(*root, RuntimeLease, Config.Profile)) {
			return std::unexpected(application::profile_startup_failure::root_unavailable);
		}
		// [[BPR-1602]]. A confirmed reset runs here, under the lease and before
		// any store opens, so a damaged store is removed without being read.
		// One that stopped part-way leaves its journal for the next start,
		// which finishes it; until then the profile does not open over it.
		if (Config.DataReset) {
			// [[BPR-1858]]. A reset that signs out reaches this profile's own
			// vault on a worker it owns, drained before the vault goes; a
			// course reaches its fixture vault instead, never the keychain.
			auto vaultEffects = std::optional<worker_executor>{ };
			auto vault        = std::unique_ptr<application::github_credential_vault_port>{ };
			if (core::profile_data_reset_signs_out(Config.DataReset->Reset)) {
				vaultEffects.emplace();
#if defined(BPR_BUILD_TESTING)
				if (Fixture) {
					vault = detail::make_github_fixture_reset_vault(*vaultEffects, Options);
				} else
#endif
				{
					vault = make_platform_vault(*vaultEffects, Config.Profile);
				}
			}
			DataResetReport = run_profile_data_reset(*root, RuntimeLease, Config.Profile, *Config.DataReset, vault.get(), aStop);
			if (vaultEffects) {
				vaultEffects->close_and_drain();
			}
			vault.reset();
			if (DataResetReport->Outcome.Reason == core::release_maintenance_reason::recovery_required) {
				detail::note_startup_failure("profile-data-reset-unfinished");
				return std::unexpected(application::profile_startup_failure::root_unavailable);
			}
		}
		if (OwnerOnly) {
			const auto endpoint = adapters::sqlite::make_profile_activation_server(*root, Config.Profile, RuntimeLease);
			if (endpoint) {
				ActivationServer = *endpoint;
			} else {
				// [[BPR-0881]]. A profile with no endpoint answers its own second launch with
				// owner-busy forever, and skipping the refusal silently is how that survived.
				const auto resolved = adapters::sqlite::profile_activation_endpoint(*root);
				bpr::app::detail::terminal_text_stream{ } << "profile-activation-endpoint-unavailable "
														  << (resolved ? "bind-refused" : adapters::sqlite::describe_profile_activation_endpoint_failure(resolved.error())) << '\n';
			}
			// The owner command endpoint (tour and, since BPR-1168, cycle commands) is separate:
			// without it the owner keeps working and a CLI caller is told the owner has no
			// channel, never given a second writer. Windows binds a named pipe (BPR-1168).
#if defined(BPR_LOCAL_AUTOMATION)
			const auto tour = adapters::sqlite::make_tour_owner_server(*root, Config.Profile, RuntimeLease);
			if (tour) {
				TourOwnerServer   = *tour;
				TourOwnerService  = std::make_shared<const tour_owner_service>(*root, Config.Profile);
				CycleOwnerService = std::make_shared<cycle_owner_service>(*root, Config.Profile);
			} else {
				bpr::app::detail::terminal_text_stream{ } << "tour-owner-endpoint-unavailable\n";
			}
#else
			// [[BPR-1179]]. Without local automation no endpoint is bound and
			// no cycle command is served. The tour service stays: the desktop
			// routes its own tour edits (a note promoted into a pull request's
			// tour) through it in process, with no channel involved.
			TourOwnerService = std::make_shared<const tour_owner_service>(*root, Config.Profile);
#endif
		}
		auto store  = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = *root });
		auto loaded = store ? store->load(aStop) : std::unexpected(application::queue_store_failure::unavailable);
		if (aStop.stop_requested()) {
			return std::unexpected(application::profile_startup_failure::cancelled);
		}
		if (loaded) {
			SavedQueue = std::move(*loaded);
		} else {
			SavedQueue.reset();
		}
		ConnectionStore  = adapters::sqlite::make_remembered_connection_store(*root, Config.Profile);
		SavedConnections = ConnectionStore->load(aStop);
		return *root;
	}

	void github_application_startup::implementation::publish(const application::profile_startup_terminal& aTerminal)
	{
		if (Cancelled || (!aTerminal.Result && aTerminal.Result.error() == application::profile_startup_failure::cancelled)) {
			finish(std::unexpected(github_application_startup_failure::cancelled));
			return;
		}
		if (!aTerminal.Result) {
			const auto failure = aTerminal.Result.error();
			// [[BPR-0877]]. `registry` below is the catch-all arm of this
			// mapping, so five different profile refusals arrive at a reader
			// wearing one name. Record the one that actually fired, first
			// refusal wins, so a run says which rather than needing a bisect.
			note_profile_startup_refusal(failure);
			if (failure == application::profile_startup_failure::owner_busy && Handoff) {
				finish(std::unexpected(*Handoff));
				return;
			}
			finish(std::unexpected(failure == application::profile_startup_failure::owner_busy             ? github_application_startup_failure::owner_busy
									   : failure == application::profile_startup_failure::unknown_profile  ? github_application_startup_failure::invalid_profile
									   : failure == application::profile_startup_failure::root_unavailable ? github_application_startup_failure::root
																										   : github_application_startup_failure::registry));
			return;
		}
		// [[BPR-0882]]. Three different steps fail into the one catch below,
		// so each names itself before it runs and the catch reports which.
		auto step = std::string_view{ "profile-runtime-construction" };
		try {
			auto profile = std::make_shared<github_profile_runtime>(std::move(Config), *aTerminal.Result, RegistryRoot, std::move(SavedQueue), std::move(ConnectionStore), std::move(SavedConnections), std::move(Scratch), OwnerOnly, std::move(RuntimeLease)
#if defined(BPR_BUILD_TESTING)
																																																										   ,
																	Fixture, std::move(Options)
#endif
			);
			profile->ActivationServer = std::move(ActivationServer);
			profile->DataResetReport  = std::move(DataResetReport);
			profile->TourOwnerServer  = std::move(TourOwnerServer);
			profile->TourOwnerService = std::move(TourOwnerService);
#if defined(BPR_LOCAL_AUTOMATION)
			profile->CycleOwnerService = std::move(CycleOwnerService);
			if (profile->CycleOwnerService) {
				// [[BPR-1830]]. Forwarded cycle commands record their rounds in the profile's log.
				profile->CycleOwnerService->set_diagnostics(profile->DiagnosticLog.get());
			}
#endif
			auto runtime = std::unique_ptr<github_application>{ new github_application{ std::move(profile) } };
			step         = "profile-services-start";
			runtime->Profile->start_services();
			if (!OwnerOnly) {
				step               = "compatibility-window-open";
				const auto context = compatibility_window_context(aTerminal.Result->Authority.profile());
				if (!runtime->open_window(context)) {
					throw std::runtime_error{ "initial window unavailable" };
				}
				runtime->Profile->Focused = context;
			}
			finish(std::move(runtime));
		} catch (...) {
			// A throw from the completion `finish` already ran is not this
			// startup refusing, so only an unfinished startup is named.
			if (!Terminal) {
				detail::note_startup_failure(step, std::current_exception());
			}
			finish(std::unexpected(github_application_startup_failure::unavailable));
		}
	}

	void github_application_startup::implementation::finish(github_application_startup_result aResult)
	{
		if (Terminal) {
			return;
		}
		auto keepAlive = shared_from_this();
		Terminal       = true;
		RuntimeLease.reset();
		auto completion = std::move(Completion);
		if (completion) {
			completion(std::move(aResult));
		}
	}

	github_application_startup::github_application_startup(std::shared_ptr<implementation> aImplementation)
		: Implementation{ std::move(aImplementation) }
	{ }

	github_application_startup::~github_application_startup() { Implementation->cancel(); }

	void github_application_startup::cancel() { Implementation->cancel(); }

	auto github_application_startup::is_quiescent() const noexcept -> bool { return Implementation->is_quiescent(); }

	auto github_application::start(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state  = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion));
		auto handle = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->begin();
		return handle;
	}

	auto github_application::start_profile(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state       = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion));
		auto handle      = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->OwnerOnly = true;
		state->begin();
		return handle;
	}
}
