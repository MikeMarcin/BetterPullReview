/// \file
/// \brief [[BPR-2248]]. The Commits tab's agent prompts through the window, on
/// commit-selection-v1 (four commits, one of them a merge of main) with the
/// fixture agent. No real agent, no network, no cost; nothing is published.
///
/// - The context menu over a selection lists Explain these commits and Review
///   these commits after the filter actions; over the merge of main alone
///   both are disabled and a note says why.
/// - Explain these commits on C1, X and C3 (not contiguous) sends one turn
///   under its name that names exactly those commits; the answer reads C1
///   and C3 against their messages, names the merge of main without
///   explaining it, and the request carried none of main's own change.
/// - Review these commits on C2 and C3 answers with a report whose findings
///   are on the course's files and offer Draft comment.
///
///     commit_asks_public_path
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/core/agent_report.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const std::string& aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	/// The tab's rows whose headlines start with `aHeadlines`, selected as a
	/// person selects them, Command-clicking each.
	void select(qt::commit_filter_tab& aTab, const std::vector<QString>& aHeadlines)
	{
		auto* const list  = aTab.list();
		auto* const model = aTab.model();
		list->selectionModel()->clearSelection();
		for (const auto& headline : aHeadlines) {
			auto found = false;
			for (int row = 0; row < model->rowCount(); ++row) {
				const auto index = model->index(row);
				if (model->data(index, Qt::DisplayRole).toString().startsWith(headline)) {
					list->selectionModel()->select(index, QItemSelectionModel::Select);
					found = true;
				}
			}
			expect(found, "the Commits tab lists " + headline.toStdString());
		}
	}

	[[nodiscard]] auto action(QMenu& aMenu, const QString& aName) -> QAction* { return aMenu.findChild<QAction*>(aName); }

	/// With `BPR_COMMIT_ASKS_RENDER_ROOT` set, a widget is saved there as a PNG
	/// (the owner's looks).
	void render(QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_COMMIT_ASKS_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QApplication::processEvents();
		static_cast<void>(aWidget.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")));
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication  qtApplication{ argc, argv };
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "commit-asks: no private data root\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot };
		options.FixtureAgentSession = true;
		auto terminal               = false;
		auto startup                = access::start(app::detail::github_public_path_fixture::commit_selection_v1, std::move(options), [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/commit-selection#2248"));
		window.home_open_action()->trigger();
		window.show_commits();
		auto* const commits = window.overview_page()->status_drawer()->commits();
		wait([&] { return commits->model()->rowCount() == 4; }, "the Commits tab lists the four commits");
		window.open_agent_question();
		auto* const tab = window.agent_tab();
		wait([&] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; }, "the fixture agent is set up");

		// Over the merge of main alone: both offered, disabled, with the reason.
		select(*commits, { QStringLiteral("Merge branch 'main'") });
		{
			std::unique_ptr<QMenu> menu{ commits->make_menu(nullptr) };
			auto* const            explain = action(*menu, QStringLiteral("commitAsk_commits.explain"));
			auto* const            review  = action(*menu, QStringLiteral("commitAsk_commits.review"));
			auto* const            note    = action(*menu, QStringLiteral("commitAskNote"));
			expect(explain != nullptr && review != nullptr && !explain->isEnabled() && !review->isEnabled(), "over a merge of main alone both are listed and disabled");
			expect(note != nullptr && note->text().contains(QStringLiteral("only merge the base branch")), "and a note says why");
		}

		// Explain these commits on C1, X and C3: not contiguous, a merge among them.
		select(*commits, { QStringLiteral("Change the generator input"), QStringLiteral("Merge branch 'main'"), QStringLiteral("Follow-ups") });
		const auto before = tab->turn_count();
		{
			std::unique_ptr<QMenu> menu{ commits->make_menu(nullptr) };
			auto* const            explain = action(*menu, QStringLiteral("commitAsk_commits.explain"));
			auto* const            review  = action(*menu, QStringLiteral("commitAsk_commits.review"));
			expect(explain != nullptr && explain->isEnabled() && explain->text() == QStringLiteral("Explain these commits"), "Explain these commits is offered");
			expect(review != nullptr && review->isEnabled() && review->text() == QStringLiteral("Review these commits"), "Review these commits is offered");
			const auto actions = menu->actions();
			const auto at      = [&actions](const QAction* aAction) { return std::ranges::find(actions, aAction) - actions.begin(); };
			expect(at(explain) > at(action(*menu, QStringLiteral("commitFilterHideFiles"))) && at(review) < at(action(*menu, QStringLiteral("commitFilterClearAll"))),
				   "after the filter actions, before Clear all filters");
			menu->popup(commits->mapToGlobal(QPoint{ 40, 40 }));
			render(*menu, QStringLiteral("commit-asks-menu"));
			render(window, QStringLiteral("commit-asks-window"));
			menu->hide();
			explain->trigger();
		}
		wait([&] { return tab->turn_count() == before + 1U && tab->state() != qt::agent_tab_state::working && tab->turn_outcome(before).has_value() && access::agent_invocation_evidence(*application).Quiescent; },
			 "Explain these commits is answered");
		const auto question = tab->turn_question(before).toStdString();
		const auto answer   = tab->turn_answer(before).toStdString();
		expect(tab->turn_name(before) == QStringLiteral("Explain these commits"), "the turn is sent under its name");
		expect(question.starts_with("Explain commits 60ee4c6, 5cc6213, 993ba24:"), "the question names exactly the selected commits: " + question.substr(0U, 80U));
		expect(answer.find("Change the generator input") != std::string::npos && answer.find("Follow-ups") != std::string::npos && answer.find("Regenerate") == std::string::npos,
			   "the answer reads the selected commits and no other:\n" + answer);
		expect(answer.find("## Merges of the base branch") != std::string::npos && answer.find("`5cc6213` Merge branch 'main'") != std::string::npos, "the merge of main is named, not explained");
		expect(answer.find("Base branch changes in the request: none.") != std::string::npos, "main's own change did not ride in the request");
		render(window, QStringLiteral("commit-asks-explained"));

		// Review these commits on C2 and C3: a report with findings to draft.
		select(*commits, { QStringLiteral("Regenerate"), QStringLiteral("Follow-ups") });
		const auto reviewAt = tab->turn_count();
		{
			std::unique_ptr<QMenu> menu{ commits->make_menu(nullptr) };
			auto* const            review = action(*menu, QStringLiteral("commitAsk_commits.review"));
			expect(review != nullptr && review->isEnabled(), "Review these commits is offered");
			review->trigger();
		}
		wait([&] { return tab->turn_count() == reviewAt + 1U && tab->state() != qt::agent_tab_state::working && tab->turn_outcome(reviewAt).has_value() && access::agent_invocation_evidence(*application).Quiescent; },
			 "Review these commits is answered");
		expect(tab->turn_name(reviewAt) == QStringLiteral("Review these commits") && tab->turn_question(reviewAt).startsWith(QStringLiteral("Review commits 14d1907, 993ba24 as a careful colleague would")),
			   "the review is sent under its name and names its commits: " + tab->turn_question(reviewAt).left(80).toStdString());
		auto* const card = tab->turn_report_card(reviewAt);
		expect(card != nullptr && card->isVisibleTo(tab), "the review is a report card");
		auto* const open = card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"));
		expect(open != nullptr, "the card opens its report");
		open->click();
		auto* const report = window.report_view();
		wait([&] { return window.report_shown() && report->isVisible() && report->findings().size() >= 2U; }, "the report opens with its findings");
		const auto& findings = report->findings();
		expect(std::ranges::any_of(findings, [](const core::report_finding& aFinding) { return aFinding.Keep == core::report_keep::draft_comment && aFinding.Path == "src/app.cpp" && aFinding.First == 40U; }),
			   "a finding offers Draft comment at its line");
		expect(report->dressed_view()->toPlainText().contains(QStringLiteral("Draft comment")), "the page offers Draft comment");
		render(window, QStringLiteral("commit-asks-review"));

		application->begin_shutdown();
		wait([&] { return application->status().ShutdownComplete; }, "shutdown");
		std::cout << "commit-asks:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "commit-asks: " << aError.what() << '\n';
		if (application) {
			std::cerr << "agent-panel: " << application->window().agent_panel()->status_text().toStdString() << '\n';
			application->begin_shutdown();
			static_cast<void>(e2e::wait_until_plain([&] { return application->status().ShutdownComplete; }, e2e::extended_agent_budget));
		}
		return EXIT_FAILURE;
	}
}
