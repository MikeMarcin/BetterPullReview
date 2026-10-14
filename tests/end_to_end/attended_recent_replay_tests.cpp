#include "../../src/app/src/github_public_path_fixture.hpp"
#include "../../src/app/src/system_browser_opener.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPushButton>
#include <QShortcut>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using namespace adapters::github;
	using namespace adapters::github::detail;
	using app::detail::github_application_fixture_access;

	using bpr::tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	class unused_effect_executor final : public application::effect_executor
	{
	public:
		void post(std::function<void()>) override
		{
			expect(false, "wire-only fixture must not post vault effects");
		}
	};

	auto profile() -> validated_github_host_profile
	{
		const auto result = validate_github_host_profile({ .Profile = core::host_profile_id{ 1U }, .Host = core::git_host_id{ 1U }, .Revision = application::github_profile_revision{ 1U }, .Authentication = application::github_authentication_transport::native_https, .Hostname = "github.com", .RestRoot = "https://api.github.com", .GraphqlRoot = "https://api.github.com/graphql" });
		expect(result.has_value(), "fixture host validates");
		return *result;
	}

	auto request(read_operation aOperation) -> github_wire_request
	{
		semantic_read_request semantic{ .Operation = aOperation };
		if (aOperation != read_operation::get_viewer_identity) {
			semantic.Owner      = "google";
			semantic.Repository = "UIforETW";
		}
		if (aOperation == read_operation::get_pull_request) {
			semantic.PullRequestNumber = 74U;
		}
		const auto wire = compile_read_request(semantic);
		expect(wire.has_value(), "fixture request compiles");
		return { .Correlation = { .Parent          = core::github_session_request_context{ .Request = core::request_id{ 1U }, .ApplicationSession = core::application_session_id{ 1U }, .Profile = core::host_profile_id{ 1U }, .IdentityGeneration = core::github_identity_generation{ 1U } },
								  .ProfileRevision = application::github_profile_revision{ 1U },
								  .Host            = core::git_host_id{ 1U },
								  .Operation       = aOperation },
				 .Wire        = *wire };
	}

	void strict_replay_course(const QTemporaryDir& aTemporary, bool aRepeat)
	{
		unused_effect_executor                          effects;
		const auto                                      selectedProfile = profile();
		app::detail::github_public_path_fixture_options options;
		options.ApplicationDataRoot = e2e::private_data_root(aTemporary);
		expect(options.ApplicationDataRoot.has_value(), "wire fixture root is owner-private");
		options.RepeatReadSessions = aRepeat;
		app::detail::github_public_path_fixture_components fixture{ app::detail::github_public_path_fixture::uiforetw_74, selectedProfile, effects, options };
		const auto                                         repository = request(read_operation::get_repository);
		const auto                                         context    = std::get<core::github_session_request_context>(repository.Correlation.Parent);
		const auto                                         anonymous  = fixture.transport_factory()->make_anonymous(selectedProfile, context);
		expect(anonymous.has_value(), "anonymous recording transport exists");
		const auto success = [&](const github_wire_request& aRequest) {
			return std::holds_alternative<github_wire_success>((*anonymous)->perform(aRequest, { }));
		};
		expect(success(repository), "first exact repository request succeeds");
		expect(success(repository) == aRepeat, "only explicit repeat sessions rewind the anonymous cursor");
		if (!aRepeat) {
			return;
		}
		// Every candidate reaches the actual fixture transport after its cursor
		// advanced. Rewind must never turn exact replay into a route-only match.
		const std::vector<std::function<void(github_wire_request&)>> mutations{
			[](auto& aRequest) { aRequest.Wire.Method = http_method::post; },
			[](auto& aRequest) { aRequest.Wire.Path += "/other"; },
			[](auto& aRequest) { aRequest.Wire.Query = "page=2"; },
			[](auto& aRequest) { aRequest.Wire.Headers.clear(); },
			[](auto& aRequest) { aRequest.Wire.Body      = "{}"; },
			[](auto& aRequest) { aRequest.Wire.Operation = read_operation::get_pull_request; },
			[](auto& aRequest) { auto semantic = aRequest.Wire.Semantic; semantic.Owner = "other"; aRequest.Wire = *compile_read_request(semantic); },
			[](auto& aRequest) { auto semantic = aRequest.Wire.Semantic; semantic.Repository = "other"; aRequest.Wire = *compile_read_request(semantic); },
		};
		for (const auto& mutate : mutations) {
			auto wrong = repository;
			mutate(wrong);
			const auto  result = (*anonymous)->perform(wrong, { });
			const auto* error  = std::get_if<github_wire_error>(&result);
			expect(error && error->Error.Failure == application::github_provider_failure::malformed_response,
				   "mismatch after rewind preserves fail-closed provider normalization");
			expect(success(repository), "exact repository boundary recovers after a refused request");
		}
		auto wrongNumber           = request(read_operation::get_pull_request);
		auto semantic              = wrongNumber.Wire.Semantic;
		semantic.PullRequestNumber = 75U;
		wrongNumber.Wire           = *compile_read_request(semantic);
		expect(!success(wrongNumber), "another pull request number is refused after rewind");
		expect(success(request(read_operation::get_pull_request)), "refused number does not consume the exact recorded pull request");

		const auto bootstrap = fixture.transport_factory()->make_bootstrap(selectedProfile, context, fixture.take_enrollment_credential());
		expect(bootstrap.has_value(), "synthetic bootstrap transport exists");
		const auto viewer = request(read_operation::get_viewer_identity);
		expect(std::holds_alternative<github_wire_success>((*bootstrap)->perform(viewer, { })), "bootstrap serves its viewer once");
		expect(std::holds_alternative<github_wire_error>((*bootstrap)->perform(repository, { })), "read rewind never admits repository on bootstrap");
		expect(std::holds_alternative<github_wire_error>((*bootstrap)->perform(viewer, { })), "read rewind never restarts bootstrap identity");
	}

	void strict_replay_contract(bool aRepeat)
	{
		QTemporaryDir temporary;
		expect(temporary.isValid(), "isolated wire fixture store exists");
		strict_replay_course(temporary, aRepeat);
		expect(e2e::remove_course_root(temporary), "the wire fixture's temporary root is removed");
	}

	void recent_resume_course(bool aAnonymous)
	{
		QTemporaryDir temporary;
		expect(temporary.isValid(), "isolated attended store exists");
		app::detail::github_public_path_fixture_options options;
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "attended store is owner-private");
		options.PublicPreviewEnabledCourse = aAnonymous;
		options.RegisteredDeviceCourse     = !aAnonymous;
		options.RepeatReadSessions         = true;
		// Deliberately leave IndependentWindowReads off: this is the strict
		// single-cursor recording used by the attended launcher.
		app::github_application* owner    = nullptr;
		bool                     terminal = false;
		auto                     desktop  = github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::uiforetw_74, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
		wait_for([&] { return terminal; }, "attended desktop starts");
		expect(owner != nullptr, "attended desktop admitted");
		owner->show();
		wait_for([&] { return owner->windows().size() == 1U; }, "one attended window exists");
		const auto context = owner->windows().front();
		auto&      window  = *owner->window(context);
		if (!aAnonymous) {
			window.home_landing()->primary_button()->click();
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "synthetic account signs in");
		}
		window.home_open_bar()->setText(QStringLiteral("https://github.com/google/UIforETW/pull/74"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "UIforETW opens through Home");
		wait_for([&] { return owner->status(context)->EpochStatus == core::epoch_status::ready; }, "UIforETW retained comparison ready");
		window.modality()->action(qt::review_modality::review)->trigger();
		// [[BPR-1295]]. In the assembled window no two window- or
		// application-wide commands share a key, and Ctrl+B is Hide Drawers.
		{
			auto seen = std::map<QString, QString>{ };
			for (auto* const action : window.findChildren<QAction*>()) {
				// A key is the window's when an object it is added to lives in
				// this window; a dialog's keys are the dialog's.
				const auto here = std::ranges::any_of(action->associatedObjects(), [&window](const QObject* aObject) {
					const auto* const widget = qobject_cast<const QWidget*>(aObject);
					return widget != nullptr && widget->window() == &window;
				});
				if (!here || (action->shortcutContext() != Qt::WindowShortcut && action->shortcutContext() != Qt::ApplicationShortcut)) {
					continue;
				}
				for (const auto& sequence : action->shortcuts()) {
					const auto text           = sequence.toString(QKeySequence::PortableText);
					const auto [where, fresh] = seen.emplace(text, action->objectName());
					expect(fresh || where->second == action->objectName(),
						   "the key " + text.toStdString() + " is held by " + where->second.toStdString() + " and " + action->objectName().toStdString());
				}
			}
			for (auto* const shortcut : window.findChildren<QShortcut*>()) {
				if (qobject_cast<const QWidget*>(shortcut->parent()) != nullptr && qobject_cast<const QWidget*>(shortcut->parent())->window() == &window
					&& (shortcut->context() == Qt::WindowShortcut || shortcut->context() == Qt::ApplicationShortcut)) {
					const auto text = shortcut->key().toString(QKeySequence::PortableText);
					expect(text.isEmpty() || seen.emplace(text, QStringLiteral("shortcut")).second,
						   "the key " + text.toStdString() + " is held twice in the assembled window");
				}
			}
			// [[BPR-1293]]. Threads takes Ctrl+4, and Ctrl+J goes to it.
			expect(seen[QStringLiteral("Ctrl+B")] == QStringLiteral("toggleWorkbenchDrawersAction")
					   && seen[QStringLiteral("Ctrl+3")] == QStringLiteral("reviewModalityDiscussAction")
					   && seen[QStringLiteral("Ctrl+4")] == QStringLiteral("reviewModalityThreadsAction")
					   && seen[QStringLiteral("Ctrl+J")] == QStringLiteral("showWorkbenchThreadsAction"),
				   "the assembled window gives Ctrl+B to Hide Drawers, Ctrl+3 to Discuss, and Ctrl+4 and Ctrl+J to Threads, each once");
		}
		auto*       files = window.files_drawer()->contents_view();
		QModelIndex selected;
		wait_for([&] {
			for (int row = 0; row < files->model()->rowCount(); ++row) {
				const auto index = files->model()->index(row, 0);
				if (index.data(Qt::DisplayRole).toString() == QStringLiteral("UIforETW/Resource.h")) {
					selected = index;
					return true;
				}
			}
			return false;
		},
				 "recorded resource file available");
		files->selectionModel()->setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		auto* surface = window.comparison_surface();
		wait_for([&] { return surface->current_result() && surface->current_context() && surface->selection() && !surface->geometry_pending(); }, "real drawer selection renders exact comparison");
		// [[BPR-1510]]. The assembled workbench keeps both remaining entries
		// to Threads after the redundant Comments-header button is removed.
		expect(window.findChild<QToolButton*>(QStringLiteral("workbenchThreadsFinderButton")) == nullptr,
			   "the composed Comments header has no redundant Threads finder button");
		surface->right_pane()->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		auto* const threads = window.show_threads_action();
		expect(threads != nullptr && threads->isEnabled() && threads->shortcut() == QKeySequence{ QStringLiteral("Ctrl+J") },
			   "the composed window retains its enabled Command-J action");
		threads->trigger();
		wait_for([&] { return window.current_modality() == qt::review_modality::threads && window.finder()->search_field()->hasFocus(); },
				 "Command-J's action enters Threads with search focused through the real host");
		threads->trigger();
		wait_for([&] { return window.current_modality() == qt::review_modality::review && surface->right_pane()->hasFocus(); },
				 "Command-J's action returns to Review with code focus restored");
		window.modality()->action(qt::review_modality::threads)->trigger();
		wait_for([&] { return window.current_modality() == qt::review_modality::threads && window.finder()->search_field()->hasFocus(); },
				 "the composed Threads tab still enters the finder with search focused");
		threads->trigger();
		wait_for([&] { return window.current_modality() == qt::review_modality::review; }, "Threads returns to the composed comparison");
		expect(surface->select_conversation_line(core::diff_side::right, 3U), "save an exact semantic reading line");
		const auto savedKey       = surface->current_context()->Key;
		const auto savedSelection = surface->selection();
		const auto snapshot       = github_application_fixture_access::large_review_snapshot(*owner, context);
		expect(snapshot.has_value() && snapshot->PullRequest.Number.value() == 74U, "accepted snapshot is exact UIforETW pull request");
		// The account baseline has no independent timeline recording; its
		// pre-existing unsupported operation is separate from the PR read.
		const auto expectedFailure = aAnonymous ? -1 : static_cast<int>(contract_failure_code::unexpected_operation);
		expect(github_application_fixture_access::evidence(*owner).RecordingFailure == expectedFailure,
			   "initial fixture failure state matches its recorded timeline availability");
		auto previousSnapshot = snapshot->Snapshot;
		for (int repeat = 0; repeat < 2; ++repeat) {
			window.home_action()->trigger();
			wait_for([&] {
				auto* recent = window.home_recent_list();
				if (!recent->isVisible() || recent->count() != 1 || !recent->item(0)->text().contains(QStringLiteral("google/UIforETW#74"))) {
					return false;
				}
				recent->setCurrentRow(0);
				return window.home_recent_resume_action()->isEnabled();
			},
					 "Recently viewed offers exact saved reading");
			window.home_recent_resume_action()->trigger();
			wait_for([&] { return !window.showing_home() && window.session_panel()->status() == core::github_status::pull_request_ready && window.home_open_notice()->text().isEmpty()
							   && surface->current_context() && surface->current_context()->Key == savedKey && surface->selection() == savedSelection && !surface->geometry_pending(); }, "Resume replays strict recording and restores exact reading");
			const auto resumed = github_application_fixture_access::large_review_snapshot(*owner, context);
			expect(resumed && resumed->Profile == snapshot->Profile && resumed->Host == snapshot->Host
					   && resumed->Account == snapshot->Account && resumed->Mode == snapshot->Mode
					   && resumed->Repository == snapshot->Repository && resumed->PullRequest.Number.value() == 74U,
				   "Resume preserves exact accepted authority and target");
			expect(resumed->Snapshot != previousSnapshot, "Resume accepts a new provider snapshot rather than keeping old content");
			previousSnapshot = resumed->Snapshot;
			expect(github_application_fixture_access::evidence(*owner).RecordingFailure == expectedFailure, "Resume adds no recording failure");
		}
		if (aAnonymous) {
			const auto evidence = github_application_fixture_access::evidence(*owner);
			expect(owner->status().PublicPreviewSignedOut && evidence.VaultStores == 0U && evidence.VaultRetrieveAttempts == 0U && !evidence.AccountTransportSelected,
				   "repeated public preview never borrows account authority or credentials");
		}
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "attended work drains before teardown");
		// [[BPR-1103]]. The stores close only once the desktop's runtimes are
		// deleted from the event loop; on Windows the directory cannot go before.
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "the attended store's temporary root is removed");
	}

	/// [[BPR-1338]]. The platform launch an attended look reaches, substituted:
	/// every address that got past the opener's own check, in order.
	auto platform_launches() -> std::vector<std::string>&
	{
		static auto launches = std::vector<std::string>{ };
		return launches;
	}

	auto record_platform_launch(const QUrl& aAddress) -> bool
	{
		platform_launches().push_back(aAddress.toString().toStdString());
		return true;
	}

	/// [[BPR-1338]]. The owner pressed Open on GitHub in the frozen attended
	/// build and no browser opened: every fixture run bound the recorder
	/// ([[BPR-0797]]), which answers "opened" and starts nothing. An attended
	/// look binds the real opener for the window's own opens. This drives the
	/// launcher's composition (uiforetw-74, the fixture account) with the
	/// platform launch substituted and reads exactly what reached it: the
	/// title row's button, a Following row and a Recently viewed row each hand
	/// over https://github.com/google/UIforETW/pull/74 once, and device
	/// sign-in still reaches only the recorder. Without the attended option
	/// the same gestures reach the recorder and never the platform.
	void attended_open_on_github_course(const bool aAttended)
	{
		QTemporaryDir temporary;
		expect(temporary.isValid(), "isolated attended store exists");
		app::detail::github_public_path_fixture_options options;
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "attended store is owner-private");
		options.RegisteredDeviceCourse = true;
		options.RepeatReadSessions     = true;
		options.AttendedWindowBrowser  = aAttended;
		const auto replaced            = app::detail::substitute_browser_launch(&record_platform_launch);
		platform_launches().clear();
		app::github_application* owner    = nullptr;
		bool                     terminal = false;
		auto                     desktop  = github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::uiforetw_74, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
		wait_for([&] { return terminal; }, "attended desktop starts");
		expect(owner != nullptr, "attended desktop admitted");
		owner->show();
		wait_for([&] { return owner->windows().size() == 1U; }, "one attended window exists");
		auto& window = *owner->window(owner->windows().front());
		window.home_landing()->primary_button()->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "synthetic account signs in");
		expect(platform_launches().empty(), "device sign-in in a fixture run never reaches the platform launch");
		const auto recorded = [&] { return github_application_fixture_access::browser_opens(*owner); };
		const auto before   = recorded();
		window.home_open_bar()->setText(QStringLiteral("https://github.com/google/UIforETW/pull/74"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "UIforETW opens through Home");
		// The launcher's own size; the title row lays the name out on a resize.
		window.resize(1'300, 820);
		QApplication::processEvents();
		wait_for([&] { return !window.showing_home() && window.open_on_github_action()->isEnabled() && window.open_on_github_button()->isVisible(); },
				 "the open pull request offers Open on GitHub beside its name");
		if (!window.open_on_github_button()->isVisible()) {
			std::cerr << "home=" << window.showing_home() << " enabled=" << window.open_on_github_action()->isEnabled() << " width=" << window.width()
					  << " identity=" << window.identity_label()->isVisible() << '\n';
		}

		constexpr auto page   = "https://github.com/google/UIforETW/pull/74";
		auto           opened = 0U;
		const auto     check  = [&](const char* aGesture) {
			++opened;
			const auto& launches = platform_launches();
			if (aAttended) {
				if (launches.size() != opened || launches.back() != page) {
					std::cerr << aGesture << ": launches=" << launches.size() << " last=" << (launches.empty() ? std::string{ } : launches.back())
							  << " recorded=" << recorded() - before << '\n';
				}
				expect(launches.size() == opened && launches.back() == page && recorded() == before,
					   std::string{ "an attended look hands exactly the pull request's page to the system browser: " } + aGesture);
			} else {
				expect(launches.empty() && recorded() == before + opened && github_application_fixture_access::browser_last_address(*owner) == page,
					   std::string{ "an ordinary fixture run records the page and launches nothing: " } + aGesture);
			}
		};
		window.open_on_github_button()->click();
		check("the title row's button");

		window.follow_pull_request_action()->trigger();
		wait_for([&] { return window.follow_pull_request_action()->isChecked(); }, "Follow keeps the pull request");
		window.home_action()->trigger();
		auto* const authored = window.home_list()->state_view(qt::home_group::my_pull_requests);
		auto        row      = QModelIndex{ };
		wait_for([&] {
			for (auto index = 0; authored != nullptr && index < authored->model()->rowCount(); ++index) {
				if (authored->model()->index(index, 0).data(qt::home_row_role::identity).toString() == QStringLiteral("google/UIforETW#74")) {
					row = authored->model()->index(index, 0);
					return window.showing_home();
				}
			}
			return false;
		},
				 "Home lists the viewer-authored pull request under My pull requests");
		window.activateWindow();
		QApplication::processEvents();
		authored->setFocus(Qt::OtherFocusReason);
		authored->setCurrentIndex(row);
		QApplication::processEvents();
		window.open_on_github_action()->trigger();
		check("an authored row");

		auto* const recent = window.home_recent_list();
		wait_for([&] { return recent->count() >= 1 && recent->item(0)->text().contains(QStringLiteral("google/UIforETW#74")); }, "Recently viewed lists the pull request");
		recent->setFocus(Qt::OtherFocusReason);
		recent->setCurrentRow(0);
		QApplication::processEvents();
		window.open_on_github_action()->trigger();
		check("a Recently viewed row");

		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "attended work drains before teardown");
		desktop.reset();
		expect(app::detail::substitute_browser_launch(replaced) == &record_platform_launch, "the platform launch is restored");
		expect(e2e::remove_course_root(temporary), "the attended store's temporary root is removed");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	strict_replay_contract(false);
	strict_replay_contract(true);
	recent_resume_course(true);
	recent_resume_course(false);
	attended_open_on_github_course(true);
	attended_open_on_github_course(false);
	std::cout << "ATTENDED_RECENT_REPLAY_OK\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
