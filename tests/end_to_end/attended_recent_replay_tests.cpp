#include "../../src/app/src/github_public_path_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
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

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>

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
			// [[BPR-1293]]. Conversations takes Ctrl+4, and Ctrl+J goes to it.
			expect(seen[QStringLiteral("Ctrl+B")] == QStringLiteral("toggleWorkbenchDrawersAction")
					   && seen[QStringLiteral("Ctrl+3")] == QStringLiteral("reviewModalityDiscussAction")
					   && seen[QStringLiteral("Ctrl+4")] == QStringLiteral("reviewModalityConversationsAction")
					   && seen[QStringLiteral("Ctrl+J")] == QStringLiteral("showWorkbenchConversationsAction"),
				   "the assembled window gives Ctrl+B to Hide Drawers, Ctrl+3 to Discuss, and Ctrl+4 and Ctrl+J to Conversations, each once");
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
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	strict_replay_contract(false);
	strict_replay_contract(true);
	recent_resume_course(true);
	recent_resume_course(false);
	std::cout << "ATTENDED_RECENT_REPLAY_OK\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
