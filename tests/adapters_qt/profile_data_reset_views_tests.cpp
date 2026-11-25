/// \file
/// \brief [[BPR-1602]], [[BPR-NNNN]]. Settings › Storage & privacy › Clear
/// local data: a checkbox per category, what a checked one implies, one
/// Clear… and one confirmation for the selection; its Removes and Keeps lists; the
/// acknowledgement that gates a destructive reset; Cancel as the default; the
/// request that reaches the dispatch; Settings search; the recovery dialog's
/// three answers; and the status notice. Saves Light and Dark renders when
/// `BPR_PROFILE_RESET_RENDER_DIR` names a folder.
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/profile_data_reset_views.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using reset = core::profile_data_reset;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 6; ++round) {
			QApplication::processEvents();
		}
	}

	void save_render(QWidget& aWidget, const QString& aName)
	{
		const auto folder = qEnvironmentVariable("BPR_PROFILE_RESET_RENDER_DIR");
		if (!folder.isEmpty()) {
			static_cast<void>(aWidget.grab().save(folder + QLatin1Char{ '/' } + aName + QStringLiteral(".png")));
		}
	}

	/// The confirmations open under `aOwner`; the class has no Q_OBJECT, so by
	/// object name.
	auto confirmations(QWidget& aOwner) -> std::vector<qt::profile_reset_confirmation*>
	{
		auto found = std::vector<qt::profile_reset_confirmation*>{ };
		for (auto* dialog : aOwner.findChildren<QDialog*>(QStringLiteral("profileResetConfirmation"))) {
			if (auto* question = dynamic_cast<qt::profile_reset_confirmation*>(dialog)) {
				found.push_back(question);
			}
		}
		return found;
	}

	/// Checks exactly `aReset` in the list and presses Clear….
	auto open_question(qt::profile_reset_panel& aPanel, const core::profile_data_reset_selection aReset) -> qt::profile_reset_confirmation*
	{
		aPanel.set_checked({ });
		for (const auto each : core::profile_data_reset_categories) {
			if (aReset.contains(each) && aPanel.check_box(each)->isEnabled()) {
				aPanel.check_box(each)->click();
			}
		}
		expect(aPanel.clear_button()->isEnabled(), "something checked enables Clear");
		aPanel.clear_button()->click();
		settle();
		qt::profile_reset_confirmation* question = nullptr;
		for (auto* candidate : confirmations(*aPanel.window())) {
			if (candidate->isVisible() && candidate->reset() == aReset) {
				question = candidate;
			}
		}
		expect(question != nullptr && question->isVisible(), "Clear… asks one confirmation for the checked categories");
		return question;
	}

	void test_the_group_on_storage_and_privacy()
	{
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		auto* const panel = window.profile_reset_panel_widget();
		expect(panel != nullptr && panel->title() == QStringLiteral("Clear local data"), "Storage & privacy has one Clear local data group");
		auto* const page = window.settings()->page_widget(qt::settings_page::storage_and_privacy);
		expect(page != nullptr && page->isAncestorOf(panel), "the group is on the Storage & privacy page");
		// [[BPR-NNNN]]. A checkbox per category with one line saying exactly
		// what it clears; nothing checked; Clear waits for a check.
		for (const auto& [each, text] : { std::pair{ reset::caches, "Caches" }, std::pair{ reset::settings, "Settings" }, std::pair{ reset::agent_sessions, "Agent sessions and workspaces" },
										  std::pair{ reset::review_data, "Review data" }, std::pair{ reset::saved_sources, "Saved repositories and servers" },
										  std::pair{ reset::statistics, "Insights statistics" }, std::pair{ reset::connections, "Connections" } }) {
			auto* const box = panel->check_box(each);
			expect(box != nullptr && box->text() == QString::fromUtf8(text) && !box->isChecked() && box->isEnabled(), "each category is an unchecked box with its name");
			auto* const line = panel->findChild<QLabel*>(box->objectName() + QStringLiteral("Line"));
			expect(line != nullptr && !line->text().isEmpty() && box->accessibleDescription() == line->text(), "each category says what it clears");
		}
		expect(panel->findChild<QLabel*>(QStringLiteral("profileResetSettingsLine"))->text().contains(QStringLiteral("agent setup")), "Settings says it clears agent setup");
		const auto agentLine = panel->findChild<QLabel*>(QStringLiteral("profileResetAgentSessionsLine"))->text();
		expect(agentLine.contains(QStringLiteral("agent conversations")) && agentLine.contains(QStringLiteral("workspaces")) && agentLine.contains(QStringLiteral("Claude Code’s and Codex’s own session files stay")),
			   "Agent sessions says it clears conversations and workspaces, and leaves the agents' own files");
		expect(!panel->clear_button()->isEnabled() && panel->checked().empty(), "nothing is checked and Clear waits");
		// A measured size is shown beside its category.
		panel->present_size(reset::agent_sessions, QStringLiteral("3 workspaces, 17.2 GB"));
		expect(panel->size_label(reset::agent_sessions)->text() == QStringLiteral("3 workspaces, 17.2 GB") && panel->size_label(reset::agent_sessions)->isVisibleTo(panel)
				   && panel->check_box(reset::agent_sessions)->accessibleName().contains(QStringLiteral("17.2 GB")),
			   "the agent workspaces' size is shown beside them");
		// Settings brings Agent sessions with it, checked and locked, and gives
		// the box back as it was when unchecked.
		panel->check_box(reset::settings)->click();
		expect(panel->check_box(reset::agent_sessions)->isChecked() && !panel->check_box(reset::agent_sessions)->isEnabled()
				   && panel->findChild<QLabel*>(QStringLiteral("profileResetAgentSessionsLine"))->text().startsWith(QStringLiteral("Included with Settings")),
			   "Settings includes agent sessions, and says why");
		panel->check_box(reset::settings)->click();
		expect(!panel->check_box(reset::agent_sessions)->isChecked() && panel->check_box(reset::agent_sessions)->isEnabled(), "unchecking Settings frees it again");
		panel->select_all_button()->click();
		expect(panel->checked() == core::profile_data_reset_selection::all() && panel->select_all_button()->text() == QStringLiteral("Uncheck all"), "Check all checks every category");
		panel->select_all_button()->click();
		expect(panel->checked().empty() && !panel->clear_button()->isEnabled(), "Uncheck all clears the list");
		// Settings search finds a category by its name on this page.
		window.settings()->present(qt::settings_page::storage_and_privacy);
		settle();
		window.settings()->search_field()->setText(QStringLiteral("Agent sessions"));
		settle();
		auto* const results = window.settings()->search_results();
		expect(results->count() > 0 && results->item(0)->text().contains(QStringLiteral("Agent sessions")), "search finds Agent sessions and workspaces");
		window.settings()->search_field()->clear();
		settle();
	}

	void test_confirmations()
	{
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		window.show();
		auto* const panel    = window.profile_reset_panel_widget();
		auto        requests = std::vector<core::profile_data_reset_request>{ };
		window.set_profile_data_reset_dispatch([&requests](const core::profile_data_reset_request& aRequest) { requests.push_back(aRequest); });

		// Caches alone: one confirmation, no acknowledgement and no copy.
		auto* caches = open_question(*panel, reset::caches);
		expect(caches->wording().Title == QStringLiteral("Clear caches for “Default”?"), "the title names the category and the profile");
		expect(caches->copy_box() == nullptr && caches->acknowledge_box() == nullptr, "caches asks no acknowledgement and offers no copy");
		expect(caches->confirm_button()->isEnabled(), "caches can be confirmed at once");
		expect(caches->cancel_button()->isDefault() && QApplication::focusWidget() == caches->cancel_button(), "Cancel is the default and has focus");
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(caches, &escape);
		settle();
		expect(requests.empty(), "Escape cancels without a request");

		caches = open_question(*panel, reset::caches);
		caches->confirm_button()->click();
		settle();
		expect(requests.size() == 1U && requests.back() == core::profile_data_reset_request{ reset::caches }, "confirming caches dispatches it");

		// Settings lists the agent setup and, with it, the agent conversations.
		auto*      settings        = open_question(*panel, reset::settings);
		const auto settingsRemoves = settings->wording().Removes.join(QLatin1Char{ '\n' });
		expect(settingsRemoves.contains(QStringLiteral("Settings: agents you added")) && settingsRemoves.contains(QStringLiteral("Agent sessions and workspaces: agent conversations")),
			   "Settings says it removes agent setup and, through it, agent conversations and workspaces");
		expect(settings->wording().Lead.contains(QStringLiteral("Settings includes Agent sessions and workspaces")), "the lead says why agent sessions are included");
		expect(settings->wording().Keeps.contains(QStringLiteral("Saved repositories and servers")) && settings->wording().Keeps.contains(QStringLiteral("Claude Code’s and Codex’s own session files")),
			   "Settings keeps saved repositories and the agents' own session files");
		expect(settings->acknowledge_box() != nullptr && settings->acknowledge_box()->text().contains(QStringLiteral("agent conversations")), "it asks to acknowledge the agent conversations");
		settings->cancel_button()->click();
		settle();

		// Two categories at once: one confirmation, one request.
		const auto two    = core::profile_data_reset_selection{ reset::caches }.with(reset::review_data);
		auto*      review = open_question(*panel, two);
		expect(review->wording().Title == QStringLiteral("Clear the checked data for “Default”?"), "several categories are asked about together");
		expect(review->wording().Removes.contains(QStringLiteral("Caches: repository data downloaded from GitHub")) && review->wording().Removes.contains(QStringLiteral("Review data: private notes and their threads")),
			   "the ledger names each checked category's contents");
		expect(review->wording().Keeps.contains(QStringLiteral("Settings")) && review->wording().Keeps.contains(QStringLiteral("Agent sessions and workspaces")), "and keeps the unchecked ones");
		expect(review->copy_box() != nullptr && review->copy_box()->isChecked(), "a copy is offered and on by default");
		expect(review->acknowledge_box() != nullptr && !review->acknowledge_box()->isChecked(), "the acknowledgement starts unticked");
		expect(!review->confirm_button()->isEnabled(), "a destructive selection waits for the acknowledgement");
		review->confirm_button()->click();
		settle();
		expect(requests.size() == 1U, "a disabled clear dispatches nothing");
		review->acknowledge_box()->setChecked(true);
		expect(review->confirm_button()->isEnabled(), "the acknowledgement enables the clear");
		review->copy_box()->setChecked(false);
		review->confirm_button()->click();
		settle();
		expect(requests.size() == 2U && requests.back() == core::profile_data_reset_request{ two, true, false }, "the request carries both categories, the acknowledgement and the copy choice");

		// Agent sessions alone: acknowledged, no copy of store files.
		auto* agents = open_question(*panel, reset::agent_sessions);
		expect(agents->copy_box() == nullptr && agents->acknowledge_box() != nullptr, "agent sessions offers no copy and asks the acknowledgement");
		expect(agents->wording().Removes.contains(QStringLiteral("Retained agent workspaces, except one another app holds")), "it says it removes the retained workspaces");
		agents->acknowledge_box()->setChecked(true);
		agents->confirm_button()->click();
		settle();
		expect(requests.size() == 3U && requests.back() == core::profile_data_reset_request{ reset::agent_sessions, true, false }, "agent sessions dispatches");

		// Everything names every category and signs out.
		auto* everything = panel->ask(reset::everything);
		settle();
		expect(panel->checked() == core::profile_data_reset_selection::all(), "an entry point naming everything checks every box");
		expect(everything->wording().Title == QStringLiteral("Reset everything for “Default”?"), "every category is Reset everything");
		expect(everything->wording().Removes.contains(QStringLiteral("Connections: sign-ins made in this app, and their saved tokens"))
				   && everything->wording().Removes.contains(QStringLiteral("Saved repositories and servers: observer rules")),
			   "Reset everything says it removes sign-ins and the saved repositories' rules");
		expect(everything->wording().Lead.contains(QStringLiteral("signs you out")) && everything->wording().Lead.contains(QStringLiteral("doesn’t sign you out of GitHub CLI itself")),
			   "Reset everything says it signs out, and that gh itself stays signed in");
		expect(everything->wording().Keeps.contains(QStringLiteral("GitHub CLI’s own sign-in, which other tools use")), "Reset everything keeps gh's own sign-in");
		everything->acknowledge_box()->setChecked(true);
		everything->confirm_button()->click();
		settle();
		expect(requests.size() == 4U && requests.back() == core::profile_data_reset_request{ reset::everything, true, true }, "Reset everything dispatches with its copy");

		// Connections signs out and loses nothing that exists only here.
		auto* connections = open_question(*panel, reset::connections);
		expect(connections->wording().Title == QStringLiteral("Clear connections for “Default”?"), "Connections names the profile");
		expect(connections->wording().Lead.contains(QStringLiteral("doesn’t sign you out of GitHub CLI itself")), "Connections says gh itself stays signed in");
		expect(connections->copy_box() == nullptr && connections->acknowledge_box() == nullptr && connections->confirm_button()->isEnabled(), "Connections asks no acknowledgement and offers no copy");
		connections->confirm_button()->click();
		settle();
		expect(requests.size() == 5U && requests.back() == core::profile_data_reset_request{ reset::connections }, "confirming Connections dispatches it");

		// A refusal is said on the group; a completed clear on the status bar.
		window.refuse_profile_data_reset(QStringLiteral("BetterPullReview is busy with this profile."));
		expect(panel->status_label()->isVisibleTo(panel) && panel->status_label()->text().contains(QStringLiteral("busy")), "a refusal is said on the group");
		window.present_profile_data_reset({ reset::caches, core::release_maintenance_reason::none, 12U }, { });
		expect(window.statusBar()->currentMessage().startsWith(QStringLiteral("Caches cleared.")), "the reopened window says what was cleared");
		expect(!panel->status_label()->isVisibleTo(panel), "and clears the group's refusal");
	}

	void test_notices()
	{
		using reason = core::release_maintenance_reason;
		expect(qt::profile_data_reset_notice({ reset::everything, reason::none, 40U, true }, QStringLiteral("BetterPullReview reset 2026-10-18 14.02.33")).contains(QStringLiteral("BetterPullReview reset 2026-10-18 14.02.33")),
			   "the notice names the copy folder");
		expect(qt::profile_data_reset_notice({ reset::settings, reason::none, 4U, false, false }, { }).contains(QStringLiteral("could not be saved")), "a queue display that could not be restored is said");
		expect(qt::profile_data_reset_notice({ reset::review_data, reason::incomplete_inventory }, { }).contains(QStringLiteral("Nothing was removed")), "a refusal says nothing was removed");
		for (const auto& [each, lead] : { std::pair{ reset::everything, "This profile was reset" }, std::pair{ reset::connections, "Cleared: Connections" } }) {
			auto signedOut      = core::profile_data_reset_outcome{ each, reason::none, 40U };
			signedOut.SignedOut = true;
			expect(qt::profile_data_reset_notice(signedOut, { }) == QString::fromUtf8(lead) + QStringLiteral(" and signed out. GitHub CLI itself is still signed in; choose Sign in or Use GitHub CLI to connect again."),
				   "a clear that signed out says so, and that gh itself is still signed in");
			auto retained                = core::profile_data_reset_outcome{ each, reason::none, 40U };
			retained.ConnectionsRetained = true;
			expect(qt::profile_data_reset_notice(retained, { }).contains(QStringLiteral("could not be forgotten. Sign out in Accounts & hosts")), "connections that could not be forgotten are said");
			expect(qt::profile_data_reset_notice({ each, reason::none, 40U }, { }) == QString::fromUtf8(lead) + QStringLiteral("."), "with nothing signed in the notice promises nothing about sign-in");
		}
		// [[BPR-NNNN]]. Several categories, and one that did not clear.
		auto several = core::profile_data_reset_outcome{ core::profile_data_reset_selection{ reset::caches }.with(reset::review_data), reason::none, 9U };
		expect(qt::profile_data_reset_notice(several, { }) == QStringLiteral("Cleared: Caches, Review data."), "a clear of several names each");
		auto partial              = core::profile_data_reset_outcome{ core::profile_data_reset_selection{ reset::settings }.with(reset::agent_sessions).with(reset::caches), reason::none, 3U };
		partial.Failed            = core::profile_data_reset_selection{ reset::agent_sessions }.with(reset::settings);
		partial.WorkspacesRemoved = 2U;
		partial.WorkspacesLeft    = 1U;
		const auto said           = qt::profile_data_reset_notice(partial, { });
		expect(said.startsWith(QStringLiteral("Caches cleared.")) && said.contains(QStringLiteral("Not cleared: Settings, Agent sessions and workspaces."))
				   && said.contains(QStringLiteral("1 agent workspace(s) are in use")),
			   "a partial clear says which categories did not clear, and why");
		auto agents              = core::profile_data_reset_outcome{ reset::agent_sessions, reason::none, 0U };
		agents.WorkspacesRemoved = 2U;
		expect(qt::profile_data_reset_notice(agents, { }) == QStringLiteral("Cleared: Agent sessions and workspaces. 2 agent workspaces were removed."), "agent sessions says how many workspaces went");
	}

	void test_recovery_dialog()
	{
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("A local database in this profile could not be opened.") };
			expect(dialog.windowTitle() == QStringLiteral("Profile “Default” couldn’t open"), "the dialog names the profile that could not open");
			expect(dialog.clear_button()->isDefault() && dialog.clear_button()->text() == QStringLiteral("Clear caches and try again"), "clearing caches is the default");
			expect(dialog.details_label()->text().contains(QStringLiteral("--reset-profile-data")), "Details name the start-up flag");
			dialog.clear_button()->click();
			expect(dialog.result() == QDialog::Accepted && dialog.request() == core::profile_data_reset_request{ reset::caches }, "Clear caches and try again asks for the caches");
		}
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("cause") };
			dialog.quit_button()->click();
			expect(dialog.result() == QDialog::Rejected && !dialog.request(), "Quit asks for nothing");
		}
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("cause") };
			dialog.show();
			dialog.everything_button()->click();
			settle();
			const auto  open     = confirmations(dialog);
			auto* const question = open.empty() ? nullptr : open.back();
			expect(question != nullptr && question->isVisible() && question->reset() == reset::everything, "Reset everything… asks its confirmation first");
			expect(!dialog.request(), "nothing is chosen before it is confirmed");
			question->acknowledge_box()->setChecked(true);
			question->confirm_button()->click();
			settle();
			expect(dialog.request() == core::profile_data_reset_request{ reset::everything, true, true }, "the confirmed reset is the dialog's answer");
		}
		{
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), { } };
			expect(dialog.windowTitle() == QStringLiteral("Reset local data for “Default”?"), "asked for with the flag, the dialog says so");
			dialog.choose(qt::profile_recovery_choice::reset_everything, core::profile_data_reset_request{ reset::everything, false, false });
			expect(!dialog.request(), "an unacknowledged answer is refused");
		}
	}

	void test_renders()
	{
		if (qEnvironmentVariable("BPR_PROFILE_RESET_RENDER_DIR").isEmpty()) {
			return;
		}
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		window.set_storage_location(QStringLiteral("~/Library/Application Support/BetterPullReview/BetterPullReview"));
		const auto original = window.palette();
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto palette = profile.derive(original);
			window.setPalette(palette);
			window.settings()->setPalette(palette);
			window.settings()->resize(1100, 820);
			window.settings()->present(qt::settings_page::storage_and_privacy);
			settle();
			if (auto* const scroll = window.settings()->findChild<QScrollArea*>(QStringLiteral("settingsPageScroll-%1").arg(static_cast<int>(qt::settings_page::storage_and_privacy)))) {
				scroll->ensureWidgetVisible(window.profile_reset_panel_widget(), 0, 40);
			}
			settle();
			save_render(*window.settings(), QStringLiteral("page-storage-") + name);
			// [[BPR-NNNN]]. Settings checked brings Agent sessions with it.
			window.profile_reset_panel_widget()->present_size(reset::agent_sessions, QStringLiteral("3 workspaces, 17.2 GB"));
			window.profile_reset_panel_widget()->set_checked(core::profile_data_reset_selection{ reset::caches }.with(reset::settings));
			settle();
			save_render(*window.settings(), QStringLiteral("page-storage-checked-") + name);
			window.profile_reset_panel_widget()->set_checked({ });
			for (const auto& [each, slug] : { std::pair{ core::profile_data_reset_selection{ reset::caches }, "caches" }, std::pair{ core::profile_data_reset_selection{ reset::settings }, "settings" },
											  std::pair{ core::profile_data_reset_selection{ reset::caches }.with(reset::review_data), "several" },
											  std::pair{ core::profile_data_reset_selection{ reset::agent_sessions }, "agent-sessions" }, std::pair{ core::profile_data_reset_selection{ reset::everything }, "everything" } }) {
				auto* const question = new qt::profile_reset_confirmation{ nullptr, each, QStringLiteral("Default") };
				question->setPalette(palette);
				question->show();
				settle();
				save_render(*question, QStringLiteral("confirm-%1-%2").arg(QString::fromLatin1(slug), name));
				question->close();
				settle();
			}
			qt::profile_recovery_dialog dialog{ QStringLiteral("Default"), QStringLiteral("A local database in this profile could not be opened.") };
			dialog.setPalette(palette);
			dialog.show();
			settle();
			save_render(dialog, QStringLiteral("recovery-") + name);
		}
		window.setPalette(original);
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_group_on_storage_and_privacy();
	test_confirmations();
	test_notices();
	test_recovery_dialog();
	test_renders();
	std::cout << "profile-data-reset-views-tests:pass\n";
	return EXIT_SUCCESS;
}
