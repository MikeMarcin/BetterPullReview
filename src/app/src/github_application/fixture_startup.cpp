#include "github_application/application_startup.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/app/profile_desktop.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/profile_data_reset.hpp"
	#include "bpr/core/profile_window.hpp"
	#include "bpr/qt/dif_runtime.hpp"
	#include "github_application_fixture_access.hpp"
	#include <filesystem>
	#include <functional>
	#include <memory>
	#include <utility>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::start_desktop(detail::github_public_path_fixture aFixture, detail::github_public_path_fixture_options aOptions, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>
	{
		const auto profile  = aOptions.Profile;
		const auto intent   = aOptions.ProfileLaunchIntent;
		auto       policy   = std::move(aOptions.EmptyDesktopPolicy);
		aOptions.DifCatalog = std::make_shared<qt::dif_catalog_provider>();
		// The shipped desktop's status item: a course on the cocoa platform gets
		// the menu bar's Dif exactly as the application does, and an offscreen
		// course, which has no tray, gets none.
		aOptions.StatusItem = native_menu_bar_status_item();
		auto statusItem     = aOptions.StatusItem;
		auto initialReset   = std::make_shared<std::optional<core::profile_data_reset_request>>(std::exchange(aOptions.DataReset, std::nullopt));
		return profile_desktop::start_with_factory(profile, [aFixture, options = std::move(aOptions), initialReset](core::bpr_profile_id aProfile, const std::filesystem::path& aRoot, core::profile_launch_intent aIntent, std::optional<core::profile_data_reset_request> aReset, github_application_startup_completion aDone) {
            auto next = aProfile == options.Profile || options.DistinctProfileAccounts ? options : detail::github_public_path_fixture_options{};
            next.DataReset = aReset ? aReset : aProfile == options.Profile ? std::exchange(*initialReset, std::nullopt) : std::nullopt;
			next.DifCatalog = options.DifCatalog;
			next.StatusItem = options.StatusItem;
			if (aProfile != options.Profile && options.DistinctProfileAccounts) {
				next.BeforeFilesPage = {};
				next.SharedDeviceVault.reset();
			}
            if (!aRoot.empty()) { next.ApplicationDataRoot = aRoot; next.GitExecutable = options.GitExecutable; }
            next.Profile = aProfile;
            next.ProfileLaunchIntent = aIntent;
            return detail::github_application_fixture_access::start_profile(aFixture, std::move(next), std::move(aDone)); }, std::move(aCompletion), intent, std::move(policy), false, std::move(statusItem));
	}

	auto detail::github_application_fixture_access::start_desktop_public_preview_selection_available(github_application_config aConfig, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>
	{
		const auto profile = aConfig.Profile;
		const auto intent  = aConfig.ProfileLaunchIntent;
		return profile_desktop::start_with_factory(profile, [config = std::move(aConfig)](core::bpr_profile_id aProfile, const std::filesystem::path& aRoot, core::profile_launch_intent aIntent, std::optional<core::profile_data_reset_request> aReset, github_application_startup_completion aDone) {
            auto next = profile_desktop::configuration_for_profile(config, aProfile, aRoot);
            next.ProfileLaunchIntent = aIntent;
            next.DataReset = aReset;
            return detail::github_application_fixture_access::start_profile_public_preview_selection_available(std::move(next), std::move(aDone)); }, std::move(aCompletion), intent);
	}

	auto detail::github_application_fixture_access::start(detail::github_public_path_fixture aFixture, detail::github_public_path_fixture_options aOptions, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state  = std::make_shared<github_application_startup::implementation>(github_application_config{ }, std::move(aCompletion), aFixture, std::move(aOptions));
		auto handle = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->begin();
		return handle;
	}

	auto detail::github_application_fixture_access::start_profile(detail::github_public_path_fixture aFixture, detail::github_public_path_fixture_options aOptions, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state       = std::make_shared<github_application_startup::implementation>(github_application_config{ }, std::move(aCompletion), aFixture, std::move(aOptions));
		auto handle      = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->OwnerOnly = true;
		state->begin();
		return handle;
	}

	auto detail::github_application_fixture_access::start_public_preview_selection_available(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state  = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion), std::nullopt, detail::github_public_path_fixture_options{ .PublicPreviewSelectionAvailable = true });
		auto handle = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->begin();
		return handle;
	}

	auto detail::github_application_fixture_access::start_profile_public_preview_selection_available(github_application_config aConfig, github_application_startup_completion aCompletion) -> std::unique_ptr<github_application_startup>
	{
		auto state       = std::make_shared<github_application_startup::implementation>(std::move(aConfig), std::move(aCompletion), std::nullopt, detail::github_public_path_fixture_options{ .PublicPreviewSelectionAvailable = true });
		auto handle      = std::unique_ptr<github_application_startup>{ new github_application_startup{ state } };
		state->OwnerOnly = true;
		state->begin();
		return handle;
	}
#endif
}
