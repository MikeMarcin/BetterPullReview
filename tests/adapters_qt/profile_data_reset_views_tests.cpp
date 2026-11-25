/// \file
/// \brief [[BPR-1602]], [[BPR-2377]]. Settings › Storage & privacy › Clear
/// local data: a table of one line per category under a header row whose
/// three-state box checks or unchecks them all, what a checked one implies, one
/// Clear… and one confirmation for the selection; its Removes and Keeps lists; the
/// acknowledgement that gates a destructive reset; Cancel as the default; the
/// request that reaches the dispatch; Settings search; the recovery dialog's
/// three answers; and the status notice. When `BPR_PROFILE_RESET_RENDER_DIR`
/// names a folder it saves Light and Dark renders there and checks nothing:
/// a native-look run keeps its windows off screen, where none takes focus.
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
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
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

	/// A left click on `aWidget`'s centre, as the window would deliver it.
	void click_on(QWidget& aWidget)
	{
		const auto  local  = QPointF{ aWidget.rect().center() };
		const auto  global = QPointF{ aWidget.mapToGlobal(aWidget.rect().center()) };
		QMouseEvent press{ QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QMouseEvent release{ QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &release);
	}

	void press_space(QWidget& aWidget)
	{
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QApplication::sendEvent(&aWidget, &press);
		QKeyEvent release{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QApplication::sendEvent(&aWidget, &release);
	}

	[[nodiscard]] auto row_label(const qt::profile_reset_panel& aPanel, const reset aCategory, const QString& aPart) -> QLabel*
	{
		return aPanel.findChild<QLabel*>(aPanel.check_box(aCategory)->objectName() + aPart);
	}

	void test_the_group_on_storage_and_privacy()
	{
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		auto* const panel = window.profile_reset_panel_widget();
		expect(panel != nullptr && panel->title() == QStringLiteral("Clear local data"), "Storage & privacy has one Clear local data group");
		auto* const page = window.settings()->page_widget(qt::settings_page::storage_and_privacy);
		expect(page != nullptr && page->isAncestorOf(panel), "the group is on the Storage & privacy page");
		// [[BPR-2377]]. A row per category: its box, its name and one short
		// line; the whole account is the row's tooltip and the box's
		// description. Nothing checked; Clear waits for a check.
		for (const auto& [each, text] : { std::pair{ reset::caches, "Caches" }, std::pair{ reset::settings, "Settings" }, std::pair{ reset::agent_setup, "Agent setup" },
										  std::pair{ reset::agent_sessions, "Agent sessions" }, std::pair{ reset::agent_workspaces, "Agent workspaces" },
										  std::pair{ reset::review_data, "Review data" }, std::pair{ reset::saved_sources, "Saved repositories and servers" },
										  std::pair{ reset::statistics, "Insights statistics" }, std::pair{ reset::connections, "Connections" } }) {
			const auto  words = qt::profile_data_reset_category_words_for(each);
			auto* const box   = panel->check_box(each);
			expect(box != nullptr && box->accessibleName() == QString::fromUtf8(text) && !box->isChecked() && box->isEnabled(), "each category is an unchecked box with its name");
			expect((box->focusPolicy() & Qt::TabFocus) != 0, "each box is reached with the keyboard");
			auto* const name = row_label(*panel, each, QStringLiteral("Name"));
			expect(name != nullptr && name->text() == QString::fromUtf8(text) && name->buddy() == box, "the name beside a box is its label");
			auto* const line = row_label(*panel, each, QStringLiteral("Line"));
			expect(line != nullptr && line->text() == words.Brief && !words.Brief.isEmpty() && !line->wordWrap() && !words.Brief.contains(QLatin1Char{ '\n' }), "each category says what it clears in one line");
			expect(box->accessibleDescription() == words.Line && words.Line.size() >= words.Brief.size(), "the box's description is the whole account");
			for (QWidget* const part : { static_cast<QWidget*>(box), static_cast<QWidget*>(name), static_cast<QWidget*>(line) }) {
				expect(part->toolTip().contains(words.Line.toHtmlEscaped()), "the row's tooltip is the whole account");
			}
		}
		expect(panel->check_box(reset::settings)->accessibleDescription().contains(QStringLiteral("Agent sessions are kept")), "Settings says it keeps agent conversations");
		const auto agentLine = panel->check_box(reset::agent_sessions)->accessibleDescription();
		expect(agentLine.contains(QStringLiteral("agent sessions with their transcripts")) && agentLine.contains(QStringLiteral("Claude Code’s and Codex’s own session files stay")),
			   "Agent sessions says it clears conversations, and leaves the agents' own files");
		expect(panel->check_box(reset::agent_setup)->accessibleDescription().contains(QStringLiteral("review skill")), "Agent setup says what Reset agents did");
		expect(!panel->clear_button()->isEnabled() && panel->checked().empty(), "nothing is checked and Clear waits");
		// A measured size is shown beside its category.
		panel->present_size(reset::agent_workspaces, QStringLiteral("3 workspaces, 17.2 GB"));
		expect(panel->size_label(reset::agent_workspaces)->text() == QStringLiteral("3 workspaces, 17.2 GB") && panel->size_label(reset::agent_workspaces)->isVisibleTo(panel)
				   && panel->check_box(reset::agent_workspaces)->accessibleName().contains(QStringLiteral("17.2 GB")),
			   "the agent workspaces' size is shown beside them");
		// [[BPR-2377]], owner: Settings and Agent sessions are independent.
		panel->check_box(reset::settings)->click();
		expect(!panel->check_box(reset::agent_sessions)->isChecked() && panel->check_box(reset::agent_sessions)->isEnabled(), "Settings leaves Agent sessions alone");
		auto* const setup = panel->check_box(reset::agent_setup);
		expect(setup->isChecked() && !setup->isEnabled(), "Settings includes Agent setup, whose store it removes: checked and locked");
		expect(setup->accessibleDescription().startsWith(QStringLiteral("Included with Settings")) && setup->toolTip().contains(QStringLiteral("Included with Settings"))
				   && row_label(*panel, reset::agent_setup, QStringLiteral("Line"))->toolTip() == setup->toolTip(),
			   "the locked row's tooltip and description say why first");
		expect(setup->toolTip().contains(qt::profile_data_reset_category_words_for(reset::agent_setup).Line.toHtmlEscaped())
				   && row_label(*panel, reset::agent_setup, QStringLiteral("Line"))->text() == qt::profile_data_reset_category_words_for(reset::agent_setup).Brief,
			   "and still what it clears; its one line is unchanged");
		expect(!row_label(*panel, reset::agent_setup, QStringLiteral("Name"))->isEnabled() && !row_label(*panel, reset::agent_setup, QStringLiteral("Line"))->isEnabled(), "a locked row is dimmed whole");
		panel->check_box(reset::settings)->click();
		expect(!setup->isChecked() && setup->isEnabled() && !setup->toolTip().contains(QStringLiteral("Included with"))
				   && row_label(*panel, reset::agent_setup, QStringLiteral("Name"))->isEnabled(),
			   "unchecking Settings frees it again");
		panel->check_box(reset::agent_sessions)->click();
		auto* const workspaces = panel->check_box(reset::agent_workspaces);
		expect(workspaces->isChecked() && !workspaces->isEnabled() && workspaces->accessibleDescription().startsWith(QStringLiteral("Included with Agent sessions"))
				   && workspaces->toolTip().contains(QStringLiteral("Included with Agent sessions")),
			   "Agent sessions takes their workspaces, and says why");
		panel->check_box(reset::agent_sessions)->click();

		// The header row's box replaces Check all: unchecked with nothing
		// checked, a dash with some, checked with all; a click checks all,
		// or unchecks all when all are checked.
		auto* const all = panel->select_all_box();
		expect(all != nullptr && all->isTristate() && all->text().isEmpty() && all->accessibleName() == QStringLiteral("Check all") && (all->focusPolicy() & Qt::TabFocus) != 0,
			   "the header row holds one three-state box, named for screen readers and reached with the keyboard");
		expect(panel->findChild<QPushButton*>(QStringLiteral("profileResetSelectAll")) == nullptr, "no Check all button is left");
		expect(all->checkState() == Qt::Unchecked, "nothing checked: the header box is unchecked");
		panel->check_box(reset::caches)->click();
		expect(all->checkState() == Qt::PartiallyChecked, "some checked: the header box shows a dash");
		all->click();
		expect(panel->checked() == core::profile_data_reset_selection::all() && all->checkState() == Qt::Checked, "a click on a dash checks every category");
		for (const auto each : core::profile_data_reset_categories) {
			expect(panel->check_box(each)->isChecked(), "every row shows checked");
		}
		expect(!setup->isEnabled() && !workspaces->isEnabled(), "and the implied rows stay locked");
		all->click();
		expect(panel->checked().empty() && all->checkState() == Qt::Unchecked && !panel->clear_button()->isEnabled(), "a click on a checked header unchecks every category");
		expect(setup->isEnabled() && workspaces->isEnabled(), "which frees the implied rows");
		all->click();
		expect(panel->checked() == core::profile_data_reset_selection::all() && all->checkState() == Qt::Checked, "a click on an unchecked header checks every category");
		panel->check_box(reset::connections)->click();
		expect(all->checkState() == Qt::PartiallyChecked && !panel->checked().contains(reset::connections), "unchecking one row turns the header to a dash");
		panel->set_checked({ });
		// Every row a person can check, checked: the implied ones make it all.
		for (const auto each : { reset::caches, reset::settings, reset::agent_sessions, reset::review_data, reset::saved_sources, reset::statistics, reset::connections }) {
			panel->check_box(each)->click();
		}
		expect(all->checkState() == Qt::Checked && panel->checked() == core::profile_data_reset_selection::all(), "the header is checked when every row shows checked, implied ones included");
		press_space(*all);
		expect(panel->checked().empty() && all->checkState() == Qt::Unchecked, "Space on the header box unchecks all");
		press_space(*panel->check_box(reset::review_data));
		expect(panel->checked() == core::profile_data_reset_selection{ reset::review_data } && all->checkState() == Qt::PartiallyChecked, "Space on a row's box checks that row");
		panel->set_checked({ });

		// Settings search finds a category by its name on this page.
		window.settings()->present(qt::settings_page::storage_and_privacy);
		settle();
		window.settings()->search_field()->setText(QStringLiteral("Agent sessions"));
		settle();
		auto* const results = window.settings()->search_results();
		expect(results->count() > 0 && results->item(0)->text().contains(QStringLiteral("Agent sessions")), "search finds Agent sessions");
		window.settings()->search_field()->clear();
		settle();
	}

	/// Whether `aLine` draws its whole text at its present width.
	[[nodiscard]] auto whole(const QLabel& aLine) -> bool
	{
		return aLine.fontMetrics().horizontalAdvance(aLine.text()) <= aLine.contentsRect().width();
	}

	/// Owner, 2026-11-24: "Checkmark, label, and description should all fit a
	/// single line per option." At the Settings window's own size every row is
	/// one line with nothing shortened; narrower, a line ends in "…" and the
	/// rows stay one line.
	void test_one_line_per_category()
	{
		qt::workbench_window window;
		window.set_profile_label(QStringLiteral("Default"));
		auto* const panel = window.profile_reset_panel_widget();
		panel->present_size(reset::agent_workspaces, QStringLiteral("3 workspaces, 17.2 GB"));
		const auto openedAt = window.settings()->size();
		window.settings()->present(qt::settings_page::storage_and_privacy);
		settle();
		expect(window.settings()->size() == openedAt, "Settings opens at its own size");
		auto* const table = panel->findChild<QWidget*>(QStringLiteral("profileResetTable"));
		expect(table != nullptr && table->isVisibleTo(window.settings()), "the categories are one table");
		auto* const scroll = window.settings()->findChild<QScrollArea*>(QStringLiteral("settingsPageScroll-%1").arg(static_cast<int>(qt::settings_page::storage_and_privacy)));
		expect(scroll != nullptr && panel->width() <= scroll->viewport()->width(), "the group fits its page");
		const auto oneLine = [&](const std::string_view aWhen) {
			const auto lineHeight = table->fontMetrics().lineSpacing();
			auto       previous   = panel->select_all_box()->geometry().center().y();
			for (const auto each : core::profile_data_reset_categories) {
				auto* const box  = panel->check_box(each);
				auto* const name = row_label(*panel, each, QStringLiteral("Name"));
				auto* const line = row_label(*panel, each, QStringLiteral("Line"));
				const auto  y    = box->geometry().center().y();
				expect(name->height() < 2 * lineHeight && line->height() < 2 * lineHeight, std::string{ "a row's name and line are one line each " }.append(aWhen));
				expect(std::abs(name->geometry().center().y() - y) <= 2 && std::abs(line->geometry().center().y() - y) <= 2, std::string{ "a row's box, name and line share one line " }.append(aWhen));
				expect(box->geometry().center().x() < name->geometry().left() && name->geometry().right() < line->geometry().left(), std::string{ "box, name, line, left to right " }.append(aWhen));
				expect(y - previous > 0 && y - previous < 2 * lineHeight + 4, std::string{ "rows are one line apart " }.append(aWhen));
				expect(whole(*name), std::string{ "a name is never shortened " }.append(aWhen));
				previous = y;
			}
			auto* const size = panel->size_label(reset::agent_workspaces);
			auto* const line = row_label(*panel, reset::agent_workspaces, QStringLiteral("Line"));
			expect(whole(*size) && std::abs(size->geometry().center().y() - line->geometry().center().y()) <= 2 && line->geometry().right() < size->geometry().left()
					   && size->geometry().right() <= table->contentsRect().right(),
				   std::string{ "a size sits whole at the end of its row's line " }.append(aWhen));
		};
		oneLine("at the window's own size");
		for (const auto each : core::profile_data_reset_categories) {
			expect(whole(*row_label(*panel, each, QStringLiteral("Line"))), "at the window's own size no line is shortened");
		}
		const auto tableHeight = table->height();
		// Checking, and the lock a check implies, change no row's height.
		panel->set_checked(core::profile_data_reset_selection::all());
		settle();
		oneLine("with everything checked");
		expect(table->height() == tableHeight, "checking changes no row's height");
		panel->set_checked({ });
		// The narrowest Settings: lines shorten, rows stay one line, and
		// what was cut is whole in the tooltip.
		window.settings()->resize(window.settings()->minimumSize());
		settle();
		oneLine("at the narrowest window");
		expect(table->height() == tableHeight && panel->width() <= scroll->viewport()->width() && scroll->horizontalScrollBar()->maximum() == 0, "narrowing wraps nothing and pans nothing");
		for (const auto each : core::profile_data_reset_categories) {
			auto* const line = row_label(*panel, each, QStringLiteral("Line"));
			expect(line->width() > 0 && line->toolTip().contains(qt::profile_data_reset_category_words_for(each).Line.toHtmlEscaped()), "a shortened line is whole in its tooltip");
		}
		// A click anywhere on a row is a click on its box; the header row
		// is not a row.
		click_on(*row_label(*panel, reset::review_data, QStringLiteral("Line")));
		expect(panel->checked() == core::profile_data_reset_selection{ reset::review_data }, "a click on a row's line checks its box");
		click_on(*row_label(*panel, reset::review_data, QStringLiteral("Name")));
		expect(panel->checked().empty(), "a click on a row's name unchecks it");
		click_on(*panel->findChild<QLabel*>(QStringLiteral("profileResetLineTitle")));
		expect(panel->checked().empty(), "a click on a column title checks nothing");
		panel->check_box(reset::settings)->click();
		click_on(*row_label(*panel, reset::agent_setup, QStringLiteral("Line")));
		expect(panel->check_box(reset::agent_setup)->isChecked() && panel->checked() == core::plan_profile_data_reset(reset::settings).Selection, "a click on a locked row changes nothing");
		panel->set_checked({ });
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

		// Settings lists the agent setup and keeps the conversations.
		auto*      settings        = open_question(*panel, reset::settings);
		const auto settingsRemoves = settings->wording().Removes.join(QLatin1Char{ '\n' });
		expect(settingsRemoves.contains(QStringLiteral("Settings: agents you added")) && !settingsRemoves.contains(QStringLiteral("Agent sessions:")),
			   "Settings says it removes the agent setup, and no conversation");
		expect(settings->wording().Lead.contains(QStringLiteral("Included with Settings, which removes the agent setup")), "the lead says why Agent setup is included");
		expect(settings->wording().Keeps.contains(QStringLiteral("Agent sessions")) && settings->wording().Keeps.contains(QStringLiteral("Claude Code’s and Codex’s own session files")),
			   "Settings keeps agent sessions and the agents' own session files");
		expect(settings->acknowledge_box() == nullptr, "Settings asks no acknowledgement");
		settings->cancel_button()->click();
		settle();

		// Two categories at once: one confirmation, one request.
		const auto two    = core::profile_data_reset_selection{ reset::caches }.with(reset::review_data);
		auto*      review = open_question(*panel, two);
		expect(review->wording().Title == QStringLiteral("Clear the checked data for “Default”?"), "several categories are asked about together");
		expect(review->wording().Removes.contains(QStringLiteral("Caches: repository data downloaded from GitHub")) && review->wording().Removes.contains(QStringLiteral("Review data: private notes and their threads")),
			   "the ledger names each checked category's contents");
		expect(review->wording().Keeps.contains(QStringLiteral("Settings")) && review->wording().Keeps.contains(QStringLiteral("Agent sessions")), "and keeps the unchecked ones");
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

		// Agent sessions: acknowledged; their workspaces go with them.
		auto* agents = open_question(*panel, reset::agent_sessions);
		expect(agents->copy_box() != nullptr && agents->acknowledge_box() != nullptr && agents->acknowledge_box()->text().contains(QStringLiteral("my agent sessions")),
			   "agent sessions may copy its store and asks the acknowledgement");
		expect(agents->wording().Removes.join(QLatin1Char{ '\n' }).contains(QStringLiteral("Agent workspaces: retained agent workspaces")), "it says it removes the retained workspaces");
		agents->acknowledge_box()->setChecked(true);
		agents->copy_box()->setChecked(false);
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
		// [[BPR-2377]]. Several categories, and one that did not clear.
		auto several = core::profile_data_reset_outcome{ core::profile_data_reset_selection{ reset::caches }.with(reset::review_data), reason::none, 9U };
		expect(qt::profile_data_reset_notice(several, { }) == QStringLiteral("Cleared: Caches, Review data."), "a clear of several names each");
		auto partial              = core::profile_data_reset_outcome{ core::profile_data_reset_selection{ reset::agent_sessions }.with(reset::agent_workspaces).with(reset::caches), reason::none, 3U };
		partial.Failed            = core::profile_data_reset_selection{ reset::agent_sessions }.with(reset::agent_workspaces);
		partial.WorkspacesRemoved = 2U;
		partial.WorkspacesLeft    = 1U;
		const auto said           = qt::profile_data_reset_notice(partial, { });
		expect(said.startsWith(QStringLiteral("Caches cleared.")) && said.contains(QStringLiteral("Not cleared: Agent sessions, Agent workspaces."))
				   && said.contains(QStringLiteral("1 agent workspace(s) are in use")),
			   "a partial clear says which categories did not clear, and why");
		auto agents              = core::profile_data_reset_outcome{ core::profile_data_reset_selection{ reset::agent_sessions }.with(reset::agent_workspaces), reason::none, 0U };
		agents.WorkspacesRemoved = 2U;
		expect(qt::profile_data_reset_notice(agents, { }) == QStringLiteral("Cleared: Agent sessions, Agent workspaces. 2 agent workspaces were removed."), "agent sessions says how many workspaces went");
		auto setup   = core::profile_data_reset_outcome{ reset::agent_setup, reason::none, 0U };
		setup.Failed = reset::agent_setup;
		expect(qt::profile_data_reset_notice(setup, { }).contains(QStringLiteral("Not cleared: Agent setup.")), "an agent setup that could not reset is said");
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
			// [[BPR-2377]]. Settings checked brings Agent setup with it.
			window.profile_reset_panel_widget()->present_size(reset::agent_workspaces, QStringLiteral("3 workspaces, 17.2 GB"));
			window.profile_reset_panel_widget()->set_checked(core::profile_data_reset_selection{ reset::caches }.with(reset::settings));
			settle();
			save_render(*window.settings(), QStringLiteral("page-storage-checked-") + name);
			save_render(*window.profile_reset_panel_widget(), QStringLiteral("group-checked-") + name);
			window.profile_reset_panel_widget()->set_checked(core::profile_data_reset_selection::all());
			settle();
			save_render(*window.profile_reset_panel_widget(), QStringLiteral("group-all-") + name);
			window.profile_reset_panel_widget()->set_checked({ });
			settle();
			save_render(*window.profile_reset_panel_widget(), QStringLiteral("group-none-") + name);
			// The window's own size, then its narrowest, where lines shorten.
			window.profile_reset_panel_widget()->set_checked(core::profile_data_reset_selection{ reset::caches }.with(reset::settings));
			for (const auto& [size, slug] : { std::pair{ QSize{ 960, 640 }, "default" }, std::pair{ window.settings()->minimumSize(), "narrowest" } }) {
				window.settings()->resize(size);
				settle();
				save_render(*window.profile_reset_panel_widget(), QStringLiteral("group-%1-%2").arg(QString::fromLatin1(slug), name));
			}
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
	if (!qEnvironmentVariable("BPR_PROFILE_RESET_RENDER_DIR").isEmpty()) {
		test_renders();
		return EXIT_SUCCESS;
	}
	test_the_group_on_storage_and_privacy();
	test_one_line_per_category();
	test_confirmations();
	test_notices();
	test_recovery_dialog();
	test_renders();
	std::cout << "profile-data-reset-views-tests:pass\n";
	return EXIT_SUCCESS;
}
