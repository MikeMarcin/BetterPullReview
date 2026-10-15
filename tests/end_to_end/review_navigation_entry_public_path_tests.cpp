#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	void settle()
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 150) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	auto shown(const qt::workbench_window& aWindow, const core::file_identity aFile) -> bool
	{
		const auto* surface = aWindow.comparison_surface();
		return surface->current_result() && surface->current_context()
			&& surface->current_context()->Key.File == aFile && !surface->geometry_pending();
	}

	void open(app::github_application& aApplication, const core::file_identity aFile)
	{
		auto& window = aApplication.window();
		window.choose_modality(qt::review_modality::review);
		wait_for([&] { return !access::epoch_refresh_identity(aApplication).TabPending; }, "Review restoration settles before explicit file gesture");
		const auto index = window.files_drawer()->contents()->index_for_file(aFile);
		expect(index.isValid(), "requested file belongs to accepted actionable catalogue");
		window.files_drawer()->contents_view()->setCurrentIndex(index);
		wait_for([&] { return shown(window, aFile) && !access::epoch_refresh_identity(aApplication).TabPending; }, "ordinary Git/cache comparison displays selected file");
	}

	auto group_ordinal(const qt::workbench_window& aWindow) -> std::size_t
	{
		const auto* surface  = aWindow.comparison_surface();
		const auto& selected = surface->selection();
		if (!selected || !selected->Group || !surface->current_result()) {
			return 0U;
		}
		const auto& groups = surface->current_result()->Alignment.Groups;
		const auto  found  = std::ranges::find(groups, *selected->Group, &core::change_group::Id);
		return found == groups.end() ? 0U : static_cast<std::size_t>(std::ranges::distance(groups.begin(), found) + 1);
	}

	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_REVIEW_NAVIGATION_RENDER_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root) && aWindow.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "full composed window rendered");
		}
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	const auto    cancellationOnly = QCoreApplication::arguments().contains(QStringLiteral("--cancellation-only"));
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-review-navigation-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::inline_drawer_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composition starts");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);
	const auto actions = std::array{ window.previous_file_action(), window.next_file_action(), window.previous_change_action(), window.next_change_action() };
	expect(std::ranges::none_of(actions, [](const QAction* aAction) { return aAction->isEnabled(); }), "inactive workbench disables navigation");
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/inline-drawer#22"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value() && window.next_file_action()->isEnabled(); }, "PR opens on Overview with navigation");
	expect(window.document_tabs()->tabs().empty(), "Overview creates no source tab");
	for (const auto modality : { qt::review_modality::overview, qt::review_modality::discuss, qt::review_modality::threads }) {
		for (const auto command : { 2U, 3U }) {
			window.choose_modality(modality);
			settle();
			actions[command]->trigger();
			wait_for([&] { return window.current_modality() == qt::review_modality::review && window.statusBar()->currentMessage() == QStringLiteral("No file comparison is open."); }, "empty change command enters Review with exact no-comparison bound");
			expect(window.document_tabs()->tabs().empty() && !window.comparison_surface()->current_result(), "entry alone creates no tab or source");
			expect(QApplication::focusWidget() == window.review_bar()->widgetForAction(actions[command]), "empty Review focuses visible originating arrow");
		}
	}
	window.choose_modality(qt::review_modality::overview);
	settle();
	window.next_file_action()->trigger();
	wait_for([&] { return window.current_modality() == qt::review_modality::review && window.comparison_surface()->current_result(); }, "shortest public path: Next file enters Review and opens first file");
	const auto* files = window.files_drawer()->contents();
	auto        order = std::vector<core::file_identity>{ };
	for (int row = 0; row < files->rowCount(); ++row) {
		if (const auto file = files->file_at(files->index(row))) {
			order.push_back(*file);
		}
	}
	expect(order.size() >= 3, "accepted actionable file order available");
	wait_for([&] { return shown(window, order.front()) && !access::epoch_refresh_identity(*application).TabPending; }, "no-tab Next file opens first actionable file exactly once");
	const auto longFile = core::file_identity_of("src/ui/conversation_cards_view.cpp");
	open(*application, longFile);
	expect(window.comparison_surface()->current_result()->Alignment.Groups.size() >= 3, "real Git-backed course has separated changes");
	const auto commands = std::array{ core::workbench_command_id::previous_file, core::workbench_command_id::next_file, core::workbench_command_id::previous_change, core::workbench_command_id::next_change };
	const auto longTab  = access::epoch_refresh_identity(*application).ActiveTab;
	expect(longTab.has_value(), "active source tab");

	if (!cancellationOnly) {
		// Each entry point shares the action. Physical keys traverse Qt's shortcut
		// map, while menu activation and toolbar clicks use the same QAction.
		for (const auto modality : { qt::review_modality::overview, qt::review_modality::discuss, qt::review_modality::threads }) {
			for (std::size_t command = 0U; command < commands.size(); ++command) {
				for (int entry = 0; entry < 3; ++entry) {
					std::cout << "navigation modality=" << static_cast<int>(modality) << " command=" << command << " entry=" << entry << '\n'
							  << std::flush;

					open(*application, command < 2U ? order[1] : longFile);
					if (command >= 2U) {
						while (window.comparison_surface()->navigate_change(core::change_navigation_direction::previous, core::change_navigation_policy::stop)) { }
						settle();
					}
					if (command == 2U) {
						static_cast<void>(window.comparison_surface()->navigate_change(core::change_navigation_direction::next, core::change_navigation_policy::stop));
						wait_for([&] { return group_ordinal(window) == 2U; }, "previous-change case begins on second change");
					} else if (command == 3U) {
						while (group_ordinal(window) > 1U) {
							static_cast<void>(window.comparison_surface()->navigate_change(core::change_navigation_direction::previous, core::change_navigation_policy::stop));
						}
						settle();
					}
					const auto beforeGroup = group_ordinal(window);
					window.choose_modality(modality);
					settle();
					expect(window.current_modality() == modality, "originating modality visible");
					auto* action = actions[command];
					if (entry == 0) {
						auto* button = window.review_bar()->widgetForAction(action);
						expect(button && button->isVisible(), "real navigation button visible");
						qobject_cast<QToolButton*>(button)->click();
					} else if (entry == 1) {
						expect(window.view_menu()->actions().contains(action), "View menu owns navigation action");
						action->trigger();
					} else {
						window.modality()->setFocus();
						QApplication::processEvents();
						const auto key = action->shortcut()[0];
						tests::window_system_key(window, key.key(), key.keyboardModifiers());
					}
					wait_for([&] { return window.current_modality() == qt::review_modality::review && !access::epoch_refresh_identity(*application).TabPending
									   && (command < 2U ? shown(window, order[command == 0U ? 0U : 2U]) : shown(window, longFile) && group_ordinal(window) == (command == 2U ? beforeGroup - 1U : beforeGroup + 1U)); }, "each real action enters Review and performs exactly one ordinary step");
					expect(window.comparison_surface()->isAncestorOf(QApplication::focusWidget()), "navigation focuses visible Review content");
				}
			}
		}
		render(window, QStringLiteral("01-change-entry"));

		// Exact bounds still enter, retain reading and show the existing status.
		for (const auto modality : { qt::review_modality::overview, qt::review_modality::discuss, qt::review_modality::threads }) {
			for (std::size_t command = 0U; command < commands.size(); ++command) {
				open(*application, command < 2U ? (command == 0U ? order.front() : order.back()) : longFile);
				if (command == 3U) {
					while (window.comparison_surface()->navigate_change(core::change_navigation_direction::next, core::change_navigation_policy::stop)) { }
					settle();
				} else if (command == 2U) {
					while (window.comparison_surface()->navigate_change(core::change_navigation_direction::previous, core::change_navigation_policy::stop)) { }
					settle();
				}
				const auto before    = access::epoch_refresh_identity(*application).ActiveTab;
				const auto selection = window.comparison_surface()->selection();
				window.choose_modality(modality);
				settle();
				window.statusBar()->clearMessage();
				std::cout << "bound modality=" << static_cast<int>(modality) << " command=" << command << '\n'
						  << std::flush;
				actions[command]->trigger();
				const auto messages = std::array{ QStringLiteral("First file in this pull request."), QStringLiteral("Last file in this pull request."), QStringLiteral("First change in this file."), QStringLiteral("Last change in this file.") };
				wait_for([&] { return window.current_modality() == qt::review_modality::review && !access::epoch_refresh_identity(*application).TabPending && window.statusBar()->currentMessage() == messages[command]; }, "bound enters Review and displays exact existing explanation");
				const auto after = access::epoch_refresh_identity(*application).ActiveTab;
				expect(after && before && after->Id == before->Id && after->View.File == before->View.File && after->View.Comparison == before->View.Comparison && window.comparison_surface()->selection() == selection, "bound preserves file, pair and semantic reading position");
			}
		}
		render(window, QStringLiteral("02-bounded-entry"));
		auto boundTimer = QElapsedTimer{ };
		boundTimer.start();
		while (boundTimer.elapsed() < 2600) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(window.statusBar()->currentMessage() == QStringLiteral("Last change in this file."), "bound explanation remains for almost three seconds");
		wait_for([&] { return window.statusBar()->currentMessage().isEmpty(); }, "three-second bound message clears");
		expect(boundTimer.elapsed() < 3600, "bound message has existing three-second lifetime");

		// Hide Drawers is remembered on entry; subsequent presses do not reapply a preset.
		window.toggle_drawers_action()->trigger();
		settle();
		const auto drawersHidden = !window.files_drawer()->isVisible() && !window.conversations_drawer()->isVisible();
		expect(drawersHidden, "Review drawers hidden by ordinary command");
		window.choose_modality(qt::review_modality::discuss);
		settle();
		window.previous_change_action()->trigger();
		wait_for([&] { return window.current_modality() == qt::review_modality::review && !access::epoch_refresh_identity(*application).TabPending; }, "returns through navigation");
		expect(!window.files_drawer()->isVisible() && !window.conversations_drawer()->isVisible(), "entry restores hidden Review arrangement");
		window.next_change_action()->trigger();
		settle();
		expect(!window.files_drawer()->isVisible() && !window.conversations_drawer()->isVisible(), "navigation inside Review preserves hidden drawers");
		window.toggle_drawers_action()->trigger();
		settle();
	}

	// An older active tab and newer sibling/window are deliberately distinct.
	open(*application, longFile);
	settle();
	const auto siblingReading = window.comparison_surface()->selection();
	window.document_tabs()->new_tab_button()->click();
	wait_for([&] { return window.document_tabs()->tabs().size() == 2 && !access::epoch_refresh_identity(*application).TabPending; }, "sibling tab opens");
	const auto sibling  = window.document_tabs()->tabs().front();
	auto*      controls = window.epoch_selectors();
	int        baseRow  = -1;
	for (int row = 0; row < controls->history_model()->rowCount(); ++row) {
		const auto anchor = controls->history_model()->anchor_at(controls->history_model()->index(row, 0));
		if (anchor && anchor->Identity.Kind == core::comparison_anchor_kind::epoch_base) {
			baseRow = row;
			break;
		}
	}
	expect(baseRow >= 0, "retained base selectable");
	controls->right_selector()->setCurrentIndex(baseRow);
	wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.ActiveTab && identity.ActiveTab->View.Comparison.Left.Commit == identity.ActiveTab->View.Comparison.Right.Commit && shown(window, longFile) && !identity.TabPending; }, "active tab retains exact base/base material");
	settle();
	const auto older          = access::epoch_refresh_identity(*application).ActiveTab;
	const auto olderSelection = window.comparison_surface()->selection();
	auto       siblingMenu    = std::unique_ptr<QMenu>{ window.document_tabs()->make_tab_menu(sibling.Id, &window) };
	siblingMenu->findChild<QAction*>(QStringLiteral("documentTabMakeWindow"))->trigger();
	siblingMenu.reset();
	wait_for([&] { return access::tab_comparison_evidence(*application).Window != std::optional{ older->View.Comparison }; }, "window default uses newer sibling pair");
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison != std::optional{ older->View.Comparison }; }, "Overview loads newer window comparison");
	window.next_change_action()->trigger();
	expect(window.current_modality() == qt::review_modality::review, "Review visible before asynchronous older material completes");
	wait_for([&] { return !access::epoch_refresh_identity(*application).TabPending && access::epoch_refresh_identity(*application).AcceptedComparison == std::optional{ older->View.Comparison }
				   && window.statusBar()->currentMessage() == QStringLiteral("This comparison has no changes to navigate."); }, "change navigation evaluates older tab's no-changes bound after restoration");
	const auto restoredOlder = access::epoch_refresh_identity(*application).ActiveTab;

	expect(restoredOlder && restoredOlder->Id == older->Id && restoredOlder->View.File == older->View.File && restoredOlder->View.Comparison == older->View.Comparison && window.comparison_surface()->selection() == olderSelection && window.document_tabs()->tabs().front().Pair == sibling.Pair, "older reading and sibling pair preserved");
	render(window, QStringLiteral("03-older-no-changes"));

	// A reverse head/base tab has real changes and an older right side, while
	// the window remains base/head. Both delayed change directions step that tab.
	int headRow = -1;
	for (int row = 0; row < controls->history_model()->rowCount(); ++row) {
		const auto anchor = controls->history_model()->anchor_at(controls->history_model()->index(row, 0));
		if (anchor && anchor->Identity.Kind == core::comparison_anchor_kind::epoch_head) {
			headRow = row;
			break;
		}
	}
	expect(headRow >= 0, "retained head selectable");
	controls->left_selector()->setCurrentIndex(headRow);
	wait_for([&] { return shown(window, longFile) && !access::epoch_refresh_identity(*application).TabPending && group_ordinal(window) == 1U; }, "reverse head/base material opens");
	const auto reverse = access::epoch_refresh_identity(*application).AcceptedComparison;
	for (const auto command : { 3U, 2U }) {
		window.choose_modality(qt::review_modality::discuss);
		wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison != reverse; }, "Discuss uses distinct newer window pair");
		actions[command]->trigger();
		expect(window.current_modality() == qt::review_modality::review, "delayed change reveals Review immediately");
		wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison == reverse && !access::epoch_refresh_identity(*application).TabPending && group_ordinal(window) == (command == 3U ? 2U : 1U); }, "delayed change performs exactly one step on reverse active-tab material");
		expect(window.document_tabs()->tabs().front().Pair == sibling.Pair, "delayed change preserves sibling view");
	}
	render(window, QStringLiteral("04-delayed-change"));
	controls->left_selector()->setCurrentIndex(baseRow);
	wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison == std::optional{ older->View.Comparison } && shown(window, longFile) && !access::epoch_refresh_identity(*application).TabPending; }, "base/base comparison restored for file course");

	// A delayed file command steps once on the old pair; a later file gesture wins.
	open(*application, order[4]);
	const auto oldPair = access::epoch_refresh_identity(*application).AcceptedComparison;
	window.choose_modality(qt::review_modality::discuss);
	wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison != oldPair; }, "Discuss restores newer window context");
	window.next_file_action()->trigger();
	wait_for([&] { return shown(window, order[5]) && !access::epoch_refresh_identity(*application).TabPending; }, "delayed file navigation steps once after old pair restoration");
	expect(access::epoch_refresh_identity(*application).AcceptedComparison == oldPair && window.document_tabs()->tabs().front().Pair == sibling.Pair, "file step retains active older pair and sibling");
	open(*application, order[4]);
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison != oldPair; }, "newer window context returns");
	window.next_file_action()->trigger();
	window.previous_file_action()->trigger();
	wait_for([&] { return shown(window, order[3]) && !access::epoch_refresh_identity(*application).TabPending; }, "later retained command supersedes earlier delayed step");

	// This added file exists at head and is absent from both sides of the
	// active base/base tab. The newer hidden material cannot lend a step.
	const auto missingFile = core::file_identity_of("docs/api/v2/endpoints.md");
	window.files_drawer()->contents_view()->setCurrentIndex(window.files_drawer()->contents()->index_for_file(missingFile));
	wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.ActiveTab && identity.ActiveTab->View.File == missingFile && !identity.TabPending && !window.comparison_surface()->current_result(); }, "added file is unavailable at exact older pair");
	for (const auto command : { 1U, 3U }) {
		window.choose_modality(qt::review_modality::overview);
		wait_for([&] { return shown(window, missingFile) && access::epoch_refresh_identity(*application).AcceptedComparison != oldPair; }, "newer Overview can read added file");
		actions[command]->trigger();
		wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.ActiveTab && identity.ActiveTab->View.File == missingFile && identity.AcceptedComparison == oldPair && !identity.TabPending && !window.comparison_surface()->current_result() && window.comparison_surface()->status_text() == QStringLiteral("The selected file is absent from both revisions."); }, "unavailable exact Review material refuses retained file/change step without newer fallback");
		expect(QApplication::focusWidget() == window.review_bar()->widgetForAction(actions[command]), "unavailable Review focuses originating visible control");
	}
	render(window, QStringLiteral("05-unavailable-exact-material"));

	// Once Review has returned to its exact pair, a source failure must not
	// trap the file arrows: the tree can still choose either adjacent file.
	for (const auto command : { 0U, 1U }) {
		window.files_drawer()->contents_view()->setCurrentIndex(window.files_drawer()->contents()->index_for_file(missingFile));
		wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.SelectedFile == missingFile && identity.ActiveTab && identity.ActiveTab->View.File == missingFile && !identity.TabPending && !window.comparison_surface()->current_result(); }, "tree returns to unavailable file on exact older pair");
		expect(actions[command]->isEnabled() && actions[command]->statusTip() == (command == 0U ? QStringLiteral("Select the previous actionable file.") : QStringLiteral("Select the next actionable file.")), "adjacent file arrow is offered without a bound despite unavailable source");
		actions[command]->trigger();
		const auto adjacent = order[command == 0U ? 1U : 3U];
		wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.SelectedFile == adjacent && identity.ActiveTab && identity.ActiveTab->View.File == adjacent && identity.AcceptedComparison == oldPair && shown(window, adjacent) && !identity.TabPending; }, "ordinary Review file arrow escapes unavailable source on the exact pair");
	}
	for (const auto command : { 1U, 0U }) {
		window.files_drawer()->contents_view()->setCurrentIndex(window.files_drawer()->contents()->index_for_file(order[4]));
		expect(access::epoch_refresh_identity(*application).SelectedFile == order[4], "tree gesture chooses the file before queued tab reflection");
		actions[command]->trigger();
		const auto adjacent = order[command == 0U ? 3U : 5U];
		wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.SelectedFile == adjacent && identity.ActiveTab && identity.ActiveTab->View.File == adjacent && identity.AcceptedComparison == oldPair && shown(window, adjacent) && !identity.TabPending; }, "file arrow pressed during tree load steps from the newly chosen file exactly once");
	}
	window.files_filter()->setText(QStringLiteral("renderer"));
	window.next_file_action()->trigger();
	wait_for([&] { return access::epoch_refresh_identity(*application).SelectedFile == order[4] && shown(window, order[4]) && !access::epoch_refresh_identity(*application).TabPending; }, "file navigation follows accepted order after filtering the tree");
	window.previous_file_action()->trigger();
	wait_for([&] { return access::epoch_refresh_identity(*application).SelectedFile == order[3] && shown(window, order[3]) && !access::epoch_refresh_identity(*application).TabPending; }, "previous file can reach an accepted file hidden by the tree filter");
	window.files_filter()->clear();
	const auto epochUpdates = controls->statistics().PublishedUpdates;
	controls->refresh_action()->trigger();
	wait_for([&] { return controls->statistics().PublishedUpdates > epochUpdates && controls->status() == core::epoch_status::ready && shown(window, order[3]) && !access::epoch_refresh_identity(*application).TabPending; }, "same-head epoch refresh restores the chosen file");
	window.next_file_action()->trigger();
	wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.SelectedFile == order[4] && identity.AcceptedComparison == oldPair && shown(window, order[4]) && !identity.TabPending; }, "file navigation remains admitted after same-head refresh");
	render(window, QStringLiteral("06-file-navigation-recovery"));

	open(*application, order[4]);
	window.choose_modality(qt::review_modality::overview);
	wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison != oldPair; }, "replacement case begins in newer Overview");
	const auto originPath = window.document_tabs()->tabs().back().Path;
	window.next_file_action()->trigger();
	window.document_tabs()->tab_bar()->setCurrentIndex(0);
	wait_for([&] { return window.document_tabs()->tabs().front().Active && !access::epoch_refresh_identity(*application).TabPending; }, "tab replacement cancels old command");
	expect(window.document_tabs()->tabs().back().Path == originPath, "late command did not replace former active file");
	expect(window.comparison_surface()->selection() == siblingReading, "sibling tab restores its unchanged semantic reading position");

	// Other replacement gestures cancel before the old restoration completes.
	window.document_tabs()->tab_bar()->setCurrentIndex(1);
	wait_for([&] { return window.document_tabs()->tabs().back().Active && !access::epoch_refresh_identity(*application).TabPending; }, "older tab returns for cancellation courses");
	const auto beginDelayed = [&] {
		open(*application, order[4]);
		controls->right_selector()->setCurrentIndex(baseRow);
		wait_for([&] { return shown(window, order[4]) && !access::epoch_refresh_identity(*application).TabPending && access::epoch_refresh_identity(*application).AcceptedComparison == oldPair; }, "origin tab is reset to exact older pair before delayed command");
		auto menu = std::unique_ptr<QMenu>{ window.document_tabs()->make_tab_menu(sibling.Id, &window) };
		menu->findChild<QAction*>(QStringLiteral("documentTabMakeWindow"))->trigger();
		menu.reset();
		wait_for([&] { return access::tab_comparison_evidence(*application).Window != oldPair; }, "sibling restores newer window default without retargeting active tab");
		window.choose_modality(qt::review_modality::overview);
		wait_for([&] { return access::epoch_refresh_identity(*application).AcceptedComparison != oldPair; }, "cancellation begins on newer window pair");
		window.next_file_action()->trigger();
	};
	beginDelayed();
	window.files_drawer()->contents_view()->setCurrentIndex(window.files_drawer()->contents()->index_for_file(order[3]));
	wait_for([&] { return shown(window, order[3]) && !access::epoch_refresh_identity(*application).TabPending; }, "later explicit file gesture replaces retained step");
	settle();
	expect(shown(window, order[3]), "late material cannot overwrite later file choice");
	beginDelayed();
	std::cout << "comparison replacement offered=" << controls->whole_pr_action()->isEnabled() << " tabPending=" << access::epoch_refresh_identity(*application).TabPending << '\n'
			  << std::flush;
	controls->whole_pr_action()->trigger();
	wait_for([&] { return shown(window, order[4]) && !access::epoch_refresh_identity(*application).TabPending && access::epoch_refresh_identity(*application).AcceptedComparison != oldPair; }, "later comparison gesture rejects retained step");
	controls->right_selector()->setCurrentIndex(baseRow);
	wait_for([&] { return shown(window, order[4]) && !access::epoch_refresh_identity(*application).TabPending && access::epoch_refresh_identity(*application).AcceptedComparison == oldPair; }, "older pair reset after comparison cancellation");
	beginDelayed();
	window.choose_home(true);
	settle();
	expect(window.showing_home() && access::epoch_refresh_identity(*application).SelectedFile == order[4], "Home cancels pending navigation without changing retained file");
	window.choose_home(false);
	window.choose_modality(qt::review_modality::review);
	wait_for([&] { return shown(window, order[4]) && !access::epoch_refresh_identity(*application).TabPending; }, "return after Home retains origin file");

	// Real composer native arrow keys retain text/caret and do not enter Review.
	window.compose_review_action()->trigger();
	auto* editor = window.review_submission_editor_view()->body_editor();
	wait_for([&] { return editor->isVisible() && !editor->isReadOnly(); }, "real summary composer visible");
	editor->setPlainText(QStringLiteral("unsent first paragraph\n\nsecond paragraph"));
	editor->setFocus();
	QApplication::processEvents();
	const auto body = editor->toPlainText();
	for (const auto key : { Qt::Key_Up, Qt::Key_Down }) {
		tests::window_system_key(window, key, Qt::AltModifier);
		expect(window.current_modality() == qt::review_modality::discuss && editor->toPlainText() == body, "editable multiline keys preserve native input and modality");
	}
	window.next_file_action()->trigger();
	wait_for([&] { return window.current_modality() == qt::review_modality::review; }, "menu navigation can leave composer");
	window.compose_review_action()->trigger();
	wait_for([&] { return editor->isVisible(); }, "composer returns");
	expect(editor->toPlainText() == body, "unsent review summary survives navigation round trip");

	window.choose_home(true);
	settle();
	const auto beforeHome = access::epoch_refresh_identity(*application).ActiveTab;
	tests::window_system_key(window, Qt::Key_Down, Qt::AltModifier);
	expect(window.showing_home() && access::epoch_refresh_identity(*application).ActiveTab == beforeHome, "Home shortcut retains existing behavior");
	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "shutdown drains all commands");
	application.reset();
	expect(e2e::remove_course_root(temporary), "private root removed");
	std::cout << "REVIEW_NAVIGATION_ENTRY_OK qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " entry-matrix=" << (cancellationOnly ? "skipped" : "4x3x3") << " exact-tab=base/base\n";
}
