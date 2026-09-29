#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/tour_owner_service.hpp"
#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QPushButton>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), "bounded activation course condition");
	}
	class closing_pause final : public QObject
	{
	public:
		explicit closing_pause(std::filesystem::path aRelease)
			: Release{ std::move(aRelease) }
		{ }

	protected:
		auto eventFilter(QObject*, QEvent* aEvent) -> bool override
		{
			// The real Close command has committed and is delivering Hide.
			// Nested event processing tests pre-admission refusal during command
			// delivery, not the separate post-delivery synchronous-busy path.
			if (aEvent->type() == QEvent::Hide && !Observed) {
				Observed = true;
				std::cout << "CLOSING_READY\n"
						  << std::flush;
				wait_for([&] { return std::filesystem::exists(Release); });
			}
			return false;
		}

	private:
		std::filesystem::path Release;
		bool                  Observed = false;
	};
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	expect(aArgc == 5, "mode root profile signal arguments");
	const auto mode    = std::string{ aArgv[1] };
	const auto root    = std::filesystem::path{ aArgv[2] };
	const auto profile = core::bpr_profile_id{ std::stoull(aArgv[3]) };
	const auto signal  = std::filesystem::path{ aArgv[4] };
	if (mode == "seed") {
		auto store = adapters::sqlite::make_profile_registry_store(root);
		auto state = store->load({ });
		expect(state.has_value(), "seed registry");
		const auto revision  = state->Revision++;
		state->LastProfileId = 2;
		state->Profiles.push_back({ { core::bpr_profile_id{ 2 }, "Work" }, application::profile_root_mode::isolated, "profile-2" });
		expect(store->save(*state, revision, { }).has_value(), "seed second profile");
		return EXIT_SUCCESS;
	}
	auto options                                                       = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot                                        = root;
	options.Profile                                                    = profile;
	options.ProfileLaunchIntent                                        = mode == "new" ? core::profile_launch_intent::new_window : core::profile_launch_intent::activate;
	bool                                                   terminal    = false;
	unsigned                                               completions = 0;
	std::optional<app::github_application_startup_failure> failure;
	auto                                                   desktop = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
		terminal = true;
		++completions;
		if (!aResult) {
			failure = aResult.error();
		}
	});
	wait_for([&] { return terminal; });
	if (mode == "activate" || mode == "new" || mode == "unavailable" || mode == "refused") {
		const auto expectedFailure = mode == "unavailable" ? app::github_application_startup_failure::owner_busy : mode == "refused" ? app::github_application_startup_failure::activation_refused
																																	 : app::github_application_startup_failure::activation_queued;
		expect(failure == expectedFailure, "sender routes or reports absent endpoint");
		wait_for([&] { return desktop->is_quiescent(); });
		expect(completions == 1 && desktop->exit_code() == (mode == "unavailable" || mode == "refused" ? 1 : 0), "one classified sender terminal");
		expect(desktop->runtime(profile) == nullptr && desktop->windows(profile).empty(), "sender owns no runtime or window");
		std::cout << (mode == "unavailable" ? "ACTIVATION_UNAVAILABLE\n" : mode == "refused" ? "ACTIVATION_REFUSED\n"
																							 : "ACTIVATION_QUEUED\n");
		return EXIT_SUCCESS;
	}
	expect(!failure, "owner startup succeeds");
	wait_for([&] { return !desktop->windows(profile).empty(); });
	if (mode == "closing") {
		expect(desktop->windows(profile).size() == 1, "closing course has sole window");
		auto*         window = desktop->window(desktop->windows(profile).front());
		closing_pause pause{ signal };
		window->installEventFilter(&pause);
		window->close();
		wait_for([&] { return desktop->runtime(profile) == nullptr; });
		desktop->quit();
		wait_for([&] { return desktop->is_quiescent(); });
		std::cout << "OWNER_RELEASED\n";
		return EXIT_SUCCESS;
	}
	if (mode == "tour-owner") {
		// [[BPR-1061]]. The owner seeds one draft in its own store, so the course can drive the
		// shipped CLI against a real desktop without a second writer ever opening the database.
		auto registry = adapters::sqlite::make_profile_registry_store(root);
		auto loaded   = registry->load({ });
		expect(loaded.has_value(), "the running owner reads its registry");
		auto authority = application::profile_storage_authority::select(*loaded, profile);
		expect(authority.has_value(), "the running owner admits its profile");
		auto resolved = adapters::sqlite::resolve_profile_storage_root(root, *authority);
		expect(resolved.has_value(), "the running owner resolves its own profile root");
		auto draft          = tests::review_tour_fixture();
		draft.Owner.Profile = profile;
		const application::tour_saved_target target{ .Owner = draft.Owner, .Draft = draft.Id, .HostProfile = core::host_profile_id{ 1 }, .Hostname = "github.com", .Repository = { "o", "r" }, .Number = core::pull_request_number{ 7 }, .Base = draft.Base, .Head = draft.Head };
		auto                                 store = adapters::sqlite::make_review_tour_store(*resolved, profile);
		expect(store->save(draft, 0, { }).has_value() && store->remember_target(target, { }).has_value(), "the owner seeds a local draft");
		std::cout << "TOUR_DRAFT " << app::tour_draft_token(target) << " " << draft.Revision << "\n"
				  << std::flush;
	}
	std::cout << "OWNER_READY\n"
			  << std::flush;
	int focusStage = 0;
	wait_for([&] {
		if (focusStage == 0 && std::filesystem::exists(signal.string() + ".focus")) {
			const auto context = desktop->windows(profile).front();
			auto*      window  = desktop->window(context);
			QEvent     event{ QEvent::WindowActivate };
			QApplication::sendEvent(window, &event);
			desktop->management_window()->show();
			desktop->management_window()->activateWindow();
			focusStage = 1;
		}
		if (focusStage == 1 && QApplication::activeWindow() == desktop->management_window()) {
			focusStage = 2;
			std::cout << "MRU_FOCUSED\n"
					  << std::flush;
		}
		if (focusStage == 2 && std::filesystem::exists(signal.string() + ".check")) {
			const auto context = desktop->windows(profile).front();
			expect(QApplication::activeWindow() == desktop->window(context), "handoff activates exact MRU window through Qt");
			std::cout << "MRU_ACTIVATED\n"
					  << std::flush;
			focusStage = 3;
		}
		return std::filesystem::exists(signal);
	});
	const auto mutation = app::detail::github_application_fixture_access::review_mutation_evidence(*desktop->runtime(profile));
	expect(mutation.Submissions == 0 && mutation.Replies == 0 && mutation.Resolutions == 0, "activation performs no provider mutation");
	// [[BPR-0777]]. The desktop no longer appends a Reload Profiles button to the
	// chooser, because RFC-BPR-0033 moves recovery into the chooser's own failure
	// notice. The reload this course needs is the one Manage Profiles already performs,
	// and Create's enablement is the same registry-idle fact the button carried.
	auto* idle = desktop->management_window()->create_button();
	expect(idle != nullptr, "the chooser's registry-idle indicator");
	wait_for([&] { return idle->isEnabled(); });
	auto* manage = desktop->window(desktop->windows(profile).front())
					   ->findChild<QAction*>(QStringLiteral("manageProfilesAction"));
	expect(manage != nullptr, "registry reload reaches the host through Manage Profiles");
	manage->trigger();
	wait_for([&] { return idle->isEnabled(); });
	desktop->quit();
	wait_for([&] { return desktop->is_quiescent(); });
	expect(desktop->exit_code() == 0, "owner orderly drain");
	std::cout << "OWNER_RELEASED\n";
	return EXIT_SUCCESS;
}
