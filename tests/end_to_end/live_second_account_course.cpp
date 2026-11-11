// [[BPR-0169]], [[BPR-1222]]. The shipped composition, offscreen, on the
// owner's designated live target, reading pull requests a second GitHub
// account (MikeMarcinTest) authored or reviews.
//
// Run by hand on macOS, never as a registered row: it signs in through the
// GitHub CLI account `gh` has active and reads live GitHub. It asks for no
// write, and a tests-enabled composition could not make one: its review
// composer is read-only without a fixture. Between steps the course writes
// `at-STEP` into its root and waits for `go-STEP`, so an independent client
// can read GitHub's own state at exactly that point.
//
//   --course observer --root DIR --pull N --author LOGIN --path PREFIX
//       Opens MikeMarcin/BPR-Test#N, then adds and removes a repository, an
//       author and a path observer rule in turn through the queue controls.
//   --course reader --root DIR --pull N
//       Opens MikeMarcin/BPR-Test#N and stays a while, asking for nothing.
//   --course epoch --root DIR --pull N
//       Opens MikeMarcin/BPR-Test#N in Review and drags each handle of the
//       Updates line onto the oldest update, saying where the handles are and
//       which updates they can take, and rendering the line into DIR
//       ([[BPR-1553]]). Run it again on the same DIR after a push to see the
//       line of a review this install has seen before.
//   --course parity --root DIR --pull N
//       Opens MikeMarcin/BPR-Test#N in Review on its default comparison and
//       visits every file, writing one JSON line per file into
//       DIR/parity.jsonl: the path, both blob OIDs the comparison read and
//       every removed (left) and added (right) line with its number and text
//       ([[BPR-1560]]). `tests/end_to_end/bpr1560_live_parity.py` compares
//       those lines with GitHub's own Files changed for the same pull request.
//   --course generated --root DIR --pull N
//       Opens MikeMarcin/BPR-Test#N, waits for the head's .gitattributes and
//       writes what Whole PR, each commit filter and Clear all filters list
//       and set aside into DIR/generated.jsonl ([[BPR-1692]]).
//   --course force-push --root DIR --pull N --path FILE
//       RFC-BPR-0089 M3. Opens MikeMarcin/BPR-Test#N, marks FILE read, hides
//       the chain's last commit, then waits at `at-push` while the operator
//       force-pushes a rewrite of it. After `go-push` it refreshes, writes what
//       the chip and the Commits tab offer, chooses the offer and writes the
//       filter after it into DIR/force-push.jsonl.
//   --course filtered-comment --root DIR --pull N --path FILE
//       RFC-BPR-0089 M4. Opens MikeMarcin/BPR-Test#N, hides the chain's last
//       commit, opens FILE and selects lines on both sides, writing where the
//       composition's core places a comment on each and what the editor says
//       before Publish into DIR/filtered-comment.jsonl. It then shows only the
//       last commit and does the same on its left side, and waits at
//       `at-post` while the operator publishes exactly those placements.
//       After `go-post` it refreshes and writes the lines the threads are
//       marked at, on C1's diff with the last commit hidden and on Whole PR.
//   --course whole-pr-comment --root DIR --pull N --path FILE
//       [[BPR-1735]]. Opens MikeMarcin/BPR-Test#N on Whole PR, opens FILE and
//       selects right-side lines 60, 14 and 13 (one-based), writing where the
//       composition's core places a comment on each from GitHub's own diff,
//       and what the editor says before Publish, into
//       DIR/whole-pr-comment.jsonl. It then waits at `at-post` while the
//       operator publishes exactly those placements.
//   --course agent-helpers --root DIR --pull N --path PROGRAM [--author claude|codex]
//       [[BPR-2004]], RFC-BPR-0103 AC07. Opens MikeMarcin/BPR-Test#N, adds the
//       installed agent at PROGRAM (Claude Code unless `--author codex`) as a
//       reviewer adds it, and asks it from the Agent tab to review with three
//       background helpers and end its message without waiting. It writes
//       what the running block showed and how the one turn ended into
//       DIR/agent-helpers.jsonl, with renders. It spends the account of the
//       installed agent; nothing is written to GitHub.
//   --course agent-scratch --root DIR --pull N --path PROGRAM [--author claude|codex]
//       [[BPR-2007]], RFC-BPR-0104 AC01, AC02, AC09. Opens MikeMarcin/BPR-Test#N,
//       adds the installed agent at PROGRAM, and asks it from the Agent tab
//       to write a script in its scratch folder, run it against the workspace
//       and try to write into the workspace. It then turns the scratch folder
//       off from the footer picker's grant item and asks it to write there
//       again. It writes what each turn did, the folder's files and the
//       workspace's `git status` into DIR/agent-scratch.jsonl. It spends the
//       installed agent's account; nothing is written to GitHub.
//   --course live-session --root DIR --pull N --path FILE --second FILE
//            --first-stop L --second-stop L --reads A,B,C [--approve M] [--agent PROGRAM]
//       RFC-BPR-0099 AC08, RFC-BPR-0105 AC08. The one course that writes:
//       a session over the --reads pull requests (agent and tour use), a
//       two-stop tour written and published on #N, then Approve on #M. The
//       composition publishes only to MikeMarcin/BPR-Test and only through
//       the window's own Publish and Approve. Driven by
//       `tests/end_to_end/live_session_course.py`, which makes and closes
//       the disposable pull requests and checks GitHub's answer with `gh`.
#include "bpr/app/github_application.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/core/agent_entry.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"
#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_line.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_queue.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QTextBlock>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QWindow>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	[[noreturn]] void fail(const std::string& aMessage)
	{
		throw std::runtime_error{ aMessage };
	}

	void say(const std::string& aLine)
	{
		std::cout << aLine << '\n'
				  << std::flush;
	}

	auto wait_for(const std::function<bool()>& aDone, const std::string& aWhat, std::chrono::seconds aBudget = std::chrono::seconds{ 180 }) -> bool
	{
		const auto deadline = std::chrono::steady_clock::now() + aBudget;
		while (std::chrono::steady_clock::now() < deadline) {
			QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
			if (aDone()) {
				return true;
			}
			QThread::msleep(20);
		}
		say("timeout:" + aWhat);
		return false;
	}

	void require(const std::function<bool()>& aDone, const std::string& aWhat, std::chrono::seconds aBudget = std::chrono::seconds{ 180 })
	{
		if (!wait_for(aDone, aWhat, aBudget)) {
			fail("timed out waiting for " + aWhat);
		}
	}

	/// Writes `at-STEP` with its facts and waits, events still running, until
	/// the operator's second client has read GitHub and created `go-STEP`.
	void checkpoint(const std::filesystem::path& aRoot, const std::string& aStep, const std::string& aFacts)
	{
		{
			std::ofstream out{ aRoot / ("at-" + aStep) };
			out << aFacts << '\n';
		}
		say("checkpoint:" + aStep + " " + aFacts);
		require([&] { return std::filesystem::exists(aRoot / ("go-" + aStep)); }, "go-" + aStep, std::chrono::seconds{ 1800 });
	}

	auto text(const QString& aText) -> std::string { return aText.toStdString(); }

	struct options
	{
		std::string           Course;
		std::filesystem::path Root;
		int                   Pull = 0;
		std::string           Author;
		std::string           Path;
		// live-session only.
		std::string      Second;
		std::string      Agent;
		std::vector<int> Reads;
		int              Approve    = 0;
		std::uint32_t    FirstStop  = 0U;
		std::uint32_t    SecondStop = 0U;
	};

	auto parse(const QStringList& aArguments) -> options
	{
		options result;
		for (qsizetype i = 1; i + 1 < aArguments.size(); i += 2) {
			const auto& key   = aArguments[i];
			const auto  value = aArguments[i + 1].toStdString();
			if (key == "--course") {
				result.Course = value;
			} else if (key == "--root") {
				result.Root = value;
			} else if (key == "--pull") {
				result.Pull = std::stoi(value);
			} else if (key == "--author") {
				result.Author = value;
			} else if (key == "--path") {
				result.Path = value;
			} else if (key == "--second") {
				result.Second = value;
			} else if (key == "--agent") {
				result.Agent = value;
			} else if (key == "--approve") {
				result.Approve = std::stoi(value);
			} else if (key == "--first-stop") {
				result.FirstStop = static_cast<std::uint32_t>(std::stoul(value));
			} else if (key == "--second-stop") {
				result.SecondStop = static_cast<std::uint32_t>(std::stoul(value));
			} else if (key == "--reads") {
				for (const auto& number : aArguments[i + 1].split(QLatin1Char(','), Qt::SkipEmptyParts)) {
					result.Reads.push_back(number.toInt());
				}
			} else {
				fail("unknown argument " + key.toStdString());
			}
		}
		if ((result.Course != "observer" && result.Course != "reader" && result.Course != "epoch" && result.Course != "parity" && result.Course != "commit-filter"
			 && result.Course != "generated" && result.Course != "force-push" && result.Course != "filtered-comment" && result.Course != "whole-pr-comment" && result.Course != "agent-helpers" && result.Course != "agent-scratch" && result.Course != "live-session")
			|| result.Root.empty() || result.Pull <= 0
			|| (result.Course == "observer" && (result.Author.empty() || result.Path.empty()))
			|| ((result.Course == "force-push" || result.Course == "filtered-comment" || result.Course == "whole-pr-comment" || result.Course == "agent-helpers" || result.Course == "agent-scratch") && result.Path.empty())
			|| (result.Course == "live-session" && (result.Path.empty() || result.Second.empty() || result.Reads.empty()))) {
			fail("usage: --course observer|reader|epoch|parity|commit-filter|generated|force-push|filtered-comment|whole-pr-comment --root DIR --pull N [--author LOGIN --path PREFIX]");
		}
		return result;
	}

	/// Signs in the way a person does from Home: Use GitHub CLI, then the
	/// github.com card's GitHub CLI offer. Returns the login the card shows.
	auto sign_in(qt::workbench_window& aWindow) -> std::string
	{
		auto* const use = aWindow.use_github_cli_button();
		if (use == nullptr) {
			fail("no Use GitHub CLI control");
		}
		use->click();
		require([&] {
			return aWindow.github_status_text() == "GitHub account connected."
				|| std::ranges::any_of(aWindow.server_cards(), [](qt::server_card* aCard) {
					   return aCard->hostname() == "github.com"
						   && ((aCard->cli_option() != nullptr && aCard->cli_offer_button() != nullptr && aCard->cli_offer_button()->isVisible())
							   || (aCard->cli_accounts() != nullptr && aCard->cli_accounts()->isVisible()));
				   });
		},
				"GitHub CLI discovery");
		say("cli-discovery:" + text(aWindow.github_cli_discovery_status()->text()));
		if (aWindow.github_status_text() != "GitHub account connected.") {
			auto offered = 0;
			for (auto* card : aWindow.server_cards()) {
				if (card->hostname() == "github.com" && card->cli_option() != nullptr && card->cli_offer_button() != nullptr && card->cli_offer_button()->isVisible()) {
					++offered;
					if (offered == 1) {
						say("cli-offer:" + text(card->cli_offer_button()->text()));
						card->cli_offer_button()->click();
					}
				}
			}
			say("cli-offers:" + std::to_string(offered));
			// [[BPR-1373]]. With more than one gh account the card lists each
			// in its own row instead of one offer; the course uses the row of
			// the account gh has active, as the header says.
			for (auto* card : aWindow.server_cards()) {
				if (offered != 0 || card->hostname() != "github.com" || card->cli_accounts() == nullptr) {
					continue;
				}
				for (auto* const row : card->cli_accounts()->findChildren<QWidget*>(QStringLiteral("serverCardCliAccount"))) {
					auto* const sentence = row->findChild<QLabel*>(QStringLiteral("serverCardCliAccountSentence"));
					auto* const choose   = row->findChild<QPushButton*>(QStringLiteral("serverCardCliAccountUse"));
					if (sentence != nullptr && choose != nullptr && row->isVisibleTo(card) && sentence->text().contains(QStringLiteral("gh's active account"))) {
						say("cli-account:" + text(row->property("login").toString()));
						choose->click();
						offered = 1;
						break;
					}
				}
			}
		}
		require([&] { return aWindow.github_status_text() == "GitHub account connected."; }, "GitHub account connected");
		std::string login;
		for (auto* card : aWindow.server_cards()) {
			if (card->hostname() == "github.com" && card->signed_in()) {
				login = text(card->login_label()->text());
				say("signed-in-card:" + login + " kind=" + text(card->kind_label()->text()));
			}
		}
		return login;
	}

	void open_pull(qt::workbench_window& aWindow, int aPull)
	{
		aWindow.home_open_bar()->setText(QStringLiteral("MikeMarcin/BPR-Test#%1").arg(aPull));
		say("open-enabled:" + std::string{ aWindow.home_open_action()->isEnabled() ? "yes" : "no" });
		aWindow.home_open_action()->trigger();
	}

	void dump(qt::workbench_window& aWindow)
	{
		say("dump:status=" + aWindow.github_status_text());
		say("dump:statusbar=" + text(aWindow.statusBar()->currentMessage()));
		auto* const editor = aWindow.review_submission_editor_view();
		say("dump:editor-status=" + text(editor->status_label()->text()) + " draft=" + (editor->shown_draft() ? "yes" : "no"));
		say("dump:home=" + text(aWindow.home_open_bar()->text()));
	}

	/// Opens the pull request and its published tour as a reader would, and
	/// leaves them open for a while; nothing here asks for a write.
	void reader_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		aWindow.choose_modality(qt::review_modality::review);
		wait_for([] { return false; }, "a reader's dwell (expected)", std::chrono::seconds{ 20 });
		dump(aWindow);
		checkpoint(aOptions.Root, "read", "{\"status\":\"" + aWindow.github_status_text() + "\"}");
	}

	/// [[BPR-2004]] AC07. One request that makes the installed agent start
	/// helpers, from the Agent tab, on the live pull request.
	void agent_helpers_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		const auto codex = aOptions.Author == "codex";
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		aWindow.agent_programs()->set_editor([&](const core::agent_entry& aStart, QWidget*) -> std::optional<core::agent_entry> {
			auto entry    = aStart;
			entry.Program = aOptions.Path;
			if (!codex) {
				entry.Model = "haiku";
			}
			return entry;
		});
		const auto protocol = codex ? core::agent_protocol::codex_app_server : core::agent_protocol::claude_stream_json;
		auto*      panel    = aWindow.agent_panel();
		const auto id       = QVariant::fromValue<qulonglong>(core::entry_profile_id(protocol, 1U).value());
		require([&] {
			if (panel->profile_selector()->findData(id) < 0) {
				aWindow.agent_programs()->add_agent(codex ? core::setup_agent::codex : core::setup_agent::claude_code);
			}
			return panel->profile_selector()->findData(id) >= 0;
		},
				"the installed agent is offered", std::chrono::seconds{ 60 });
		panel->profile_selector()->setCurrentIndex(panel->profile_selector()->findData(id));
		aWindow.settings()->hide();
		aWindow.choose_modality(qt::review_modality::review);
		aWindow.tray_tabs()->setCurrentIndex(3);
		auto* const tab    = aWindow.agent_tab();
		const auto  before = tab->turn_count();
		// As a person sends: once the setup has given way to the conversation.
		wait_for([&] { return tab->state() == qt::agent_tab_state::idle; }, "the Agent tab is ready to ask", std::chrono::seconds{ 120 });
		say("agent-tab-state-before-send:" + std::to_string(static_cast<int>(tab->state())));
		tab->send_prompt(QStringLiteral("Review this pull request. Start three sub-agents in the background, one for each of three different areas of the change, and do not wait for them: "
										"tell me you started three reviewers and end your message. When their results arrive, combine them into a short review."));
		require([&] { return tab->turn_count() == before + 1U; }, "the turn starts", std::chrono::seconds{ 120 });
		auto       most     = 0;
		auto       rows     = QStringList{ };
		auto       heads    = QStringList{ };
		auto       rendered = false;
		auto       states   = std::string{ };
		const auto watch    = [&] {
			if (const auto state = std::to_string(static_cast<int>(tab->state())) + ":" + std::to_string(static_cast<int>(panel->current_phase())) + ":" + tab->turn_footer(before).section(QStringLiteral(" · "), 0, 0).right(12).toStdString(); !states.ends_with(state) && states.size() < 1500U) {
				states += (states.empty() ? "" : "|") + state;
			}
			auto* const block = tab->turn_activity(before);
			if (block != nullptr && block->live()) {
				if (block->helper_rows() > most) {
					most = block->helper_rows();
					rows.clear();
					for (int row = 0; row < block->helper_rows(); ++row) {
						rows << block->helper_row_text(row);
					}
				}
				if (const auto head = tab->turn_footer(before).section(QStringLiteral(", "), 0, 0); !heads.contains(head) && heads.size() < 40) {
					heads << head;
				}
				if (!rendered && block->helper_rows() > 0) {
					rendered = true;
					aWindow.grab().save(QString::fromStdString((aOptions.Root / "agent-helpers-running.png").string()));
				}
			}
			return tab->state() != qt::agent_tab_state::working && tab->turn_outcome(before).has_value();
		};
		require(watch, "the one turn ends", std::chrono::seconds{ 900 });
		aWindow.grab().save(QString::fromStdString((aOptions.Root / "agent-helpers-ended.png").string()));
		auto* const block = tab->turn_activity(before);
		const auto  quote = [](const QString& aText) {
			auto escaped = aText.toStdString();
			auto out     = std::string{ };
			for (const auto character : escaped) {
				if (character == '"' || character == '\\') {
					out += '\\';
				}
				out += character == '\n' || character == '\t' ? ' ' : character;
			}
			return "\"" + out + "\"";
		};
		auto facts = std::string{ "{\"agent\":" } + quote(codex ? QStringLiteral("codex") : QStringLiteral("claude")) + ",\"turns\":" + std::to_string(tab->turn_count() - before)
			+ ",\"outcome\":" + quote(QString::fromUtf8(core::stable_name(*tab->turn_outcome(before)).data())) + ",\"footer\":" + quote(tab->turn_footer(before))
			+ ",\"most_helper_rows\":" + std::to_string(most) + ",\"summary\":" + quote(block != nullptr ? block->summary_text() : QString{ }) + ",\"rows\":[";
		for (qsizetype row = 0; row < rows.size(); ++row) {
			facts += (row == 0 ? "" : ",") + quote(rows[row]);
		}
		facts += "],\"heads\":[";
		for (qsizetype head = 0; head < heads.size(); ++head) {
			facts += (head == 0 ? "" : ",") + quote(heads[head]);
		}
		facts += "],\"answer_head\":" + quote(tab->turn_answer(before).left(500)) + ",\"reported_cost\":" + quote(panel->last_reported_cost()) + "}";
		std::ofstream{ aOptions.Root / "agent-helpers.jsonl", std::ios::app } << facts << '\n';
		say("agent-helpers:" + facts);
		say("agent-helpers-states:" + states);
		if (tab->turn_count() != before + 1U) {
			fail("more than one turn");
		}
	}

	/// [[BPR-2007]], RFC-BPR-0104 AC01, AC02, AC09. The installed agent writes
	/// and runs a script in its scratch folder and nowhere else; turned off
	/// from the footer picker's grant item, the next turn has no allowance.
	void agent_scratch_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		const auto codex = aOptions.Author == "codex";
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		aWindow.agent_programs()->set_editor([&](const core::agent_entry& aStart, QWidget*) -> std::optional<core::agent_entry> {
			auto entry    = aStart;
			entry.Program = aOptions.Path;
			if (!codex) {
				entry.Model = "haiku";
			}
			return entry;
		});
		const auto protocol = codex ? core::agent_protocol::codex_app_server : core::agent_protocol::claude_stream_json;
		auto*      panel    = aWindow.agent_panel();
		const auto profile  = core::entry_profile_id(protocol, 1U).value();
		const auto id       = QVariant::fromValue<qulonglong>(profile);
		require([&] {
			if (panel->profile_selector()->findData(id) < 0) {
				aWindow.agent_programs()->add_agent(codex ? core::setup_agent::codex : core::setup_agent::claude_code);
			}
			return panel->profile_selector()->findData(id) >= 0;
		},
				"the installed agent is offered", std::chrono::seconds{ 60 });
		panel->profile_selector()->setCurrentIndex(panel->profile_selector()->findData(id));
		aWindow.settings()->hide();
		aWindow.choose_modality(qt::review_modality::review);
		aWindow.tray_tabs()->setCurrentIndex(3);
		auto* const tab = aWindow.agent_tab();
		wait_for([&] { return tab->state() == qt::agent_tab_state::idle; }, "the Agent tab is ready to ask", std::chrono::seconds{ 120 });
		const auto quote = [](const QString& aText) {
			auto out = std::string{ };
			for (const auto character : aText.toStdString()) {
				if (character == '"' || character == '\\') {
					out += '\\';
				}
				out += character == '\n' || character == '\t' ? ' ' : character;
			}
			return "\"" + out + "\"";
		};
		const auto grantItem = [&]() -> std::pair<QString, QAction*> {
			auto* const picker = aWindow.agent_footer_widget()->picker();
			emit        picker->menu()->aboutToShow();
			auto        line    = QString{ };
			auto        scratch = static_cast<QAction*>(nullptr);
			for (auto* const item : picker->menu()->actions()) {
				if (item->objectName() == QStringLiteral("agentRunSettingsGrantLine")) {
					line = item->text();
				} else if (item->objectName() == QStringLiteral("agentRunSettingsScratch")) {
					scratch = item;
				}
			}
			return { line, scratch };
		};
		const auto scratchRoot = aOptions.Root / "data-agent-scratch-v1";
		const auto listing     = [&] {
			auto files = std::string{ };
			auto error = std::error_code{ };
			for (auto entry = std::filesystem::recursive_directory_iterator{ scratchRoot, error }; !error && entry != std::filesystem::recursive_directory_iterator{ }; entry.increment(error)) {
				if (entry->is_regular_file()) {
					files += (files.empty() ? "" : ",") + entry->path().lexically_relative(scratchRoot).generic_string();
				}
			}
			return files;
		};
		const auto workspaces = [&] {
			// Every checkout BPR made for this profile, and what Git says of it.
			auto states = std::string{ };
			auto error  = std::error_code{ };
			for (auto entry = std::filesystem::recursive_directory_iterator{ aOptions.Root / "data-agent-workspaces-v1", error }; !error && entry != std::filesystem::recursive_directory_iterator{ }; entry.increment(error)) {
				if (entry->path().filename() == ".git" || entry.depth() > 4) {
					entry.disable_recursion_pending();
				}
				if (entry->path().filename() == ".git") {
					const auto checkout = entry->path().parent_path();
					auto       process  = QProcess{ };
					process.start(QStringLiteral("git"), { QStringLiteral("-C"), QString::fromStdString(checkout.string()), QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=all") });
					process.waitForFinished(30'000);
					const auto out = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
					states += (states.empty() ? "" : ";") + std::string{ out.isEmpty() ? "clean" : out.toStdString() };
				}
			}
			return states;
		};
		const auto turn = [&](const QString& aPrompt) {
			const auto before = tab->turn_count();
			tab->send_prompt(aPrompt);
			require([&] { return tab->turn_count() == before + 1U; }, "the turn starts", std::chrono::seconds{ 120 });
			require([&] { return tab->state() != qt::agent_tab_state::working && tab->turn_outcome(before).has_value(); }, "the turn ends", std::chrono::seconds{ 900 });
			auto* const block = tab->turn_activity(before);
			return std::string{ "{\"outcome\":" } + quote(QString::fromUtf8(core::stable_name(*tab->turn_outcome(before)).data())) + ",\"summary\":" + quote(block != nullptr ? block->summary_text() : QString{ })
				+ ",\"answer\":" + quote(tab->turn_answer(before).left(900)) + "}";
		};
		const auto [lineOn, itemOn] = grantItem();
		const auto first            = turn(QStringLiteral("Write a Python script named count.py in your scratch folder that prints the number of lines in each regular file in the current directory and below, skipping .git. "
														  "Run it with python3 and its absolute path, as one command with nothing else, and report what it printed. Then try to create a file named bpr-probe.txt in the "
														  "current directory, and say exactly whether you were allowed. Do not start helpers."));
		const auto afterFirst       = listing();
		const auto cleanFirst       = workspaces();
		aWindow.grab().save(QString::fromStdString((aOptions.Root / ("agent-scratch-" + std::string{ codex ? "codex" : "claude" } + "-on.png")).string()));
		// AC09: off from the footer picker's grant item, as a person turns it off.
		auto [lineBefore, item] = grantItem();
		if (item == nullptr || !item->isChecked()) {
			fail("the grant item is not there, or not on");
		}
		item->trigger();
		QApplication::processEvents();
		const auto [lineOff, itemOff] = grantItem();
		const auto second             = turn(QStringLiteral("Now try to create a file named second.py in the same scratch folder as before, containing print(2), and run it. Say exactly whether you were allowed. Do not start helpers."));
		const auto afterSecond        = listing();
		const auto facts              = std::string{ "{\"agent\":" } + quote(codex ? QStringLiteral("codex") : QStringLiteral("claude")) + ",\"line_on\":" + quote(lineOn) + ",\"first\":" + first + ",\"scratch_after_first\":" + quote(QString::fromStdString(afterFirst))
			+ ",\"workspaces_after_first\":" + quote(QString::fromStdString(cleanFirst)) + ",\"line_off\":" + quote(lineOff) + ",\"item_off_checked\":" + (itemOff != nullptr && itemOff->isChecked() ? "true" : "false")
			+ ",\"second\":" + second + ",\"scratch_after_second\":" + quote(QString::fromStdString(afterSecond)) + ",\"workspaces_after_second\":" + quote(QString::fromStdString(workspaces())) + "}";
		std::ofstream{ aOptions.Root / "agent-scratch.jsonl", std::ios::app } << facts << '\n';
		say("agent-scratch:" + facts);
		static_cast<void>(itemOn);
	}

	/// Drags through the window, as a pointer is delivered, so whatever sits
	/// over the widget gets the events first.
	void window_drag(QWidget& aWidget, const QPoint& aFrom, const QPoint& aTo)
	{
		auto* const top    = aWidget.window();
		auto* const target = top != nullptr ? top->windowHandle() : nullptr;
		if (target == nullptr) {
			fail("the window has no native handle to deliver a drag to");
		}
		const auto send = [&](const QEvent::Type aType, const QPoint& aAt, const Qt::MouseButton aButton, const Qt::MouseButtons aButtons) {
			const auto local = QPointF{ aWidget.mapTo(top, aAt) };
			auto       event = QMouseEvent{ aType, local, local, QPointF{ aWidget.mapToGlobal(aAt) }, aButton, aButtons, Qt::NoModifier };
			QCoreApplication::sendEvent(target, &event);
		};
		send(QEvent::MouseMove, aFrom, Qt::NoButton, Qt::NoButton);
		send(QEvent::MouseButtonPress, aFrom, Qt::LeftButton, Qt::LeftButton);
		for (auto step = 1; step <= 4; ++step) {
			send(QEvent::MouseMove, aFrom + ((aTo - aFrom) * step) / 4, Qt::NoButton, Qt::LeftButton);
		}
		send(QEvent::MouseButtonRelease, aTo, Qt::LeftButton, Qt::NoButton);
	}

	/// Says what the Updates line shows: its nodes, where each handle is, which
	/// nodes a handle can be put on (the node menu's Compare actions), and a
	/// render of the window and the line into the root.
	void epoch_dump(qt::workbench_window& aWindow, const std::filesystem::path& aRoot, const std::string& aTag)
	{
		auto* const control = aWindow.epoch_selectors();
		auto* const line    = control->line();
		const auto  stop    = [](const std::optional<qt::epoch_line_stop>& aStop) {
			if (!aStop) {
				return std::string{ "none" };
			}
			// [[BPR-1557]]. The pull request's base, at the line's start.
			if (aStop->Kind == core::update_line_place_kind::pull_request_base) {
				return std::string{ "prbase" };
			}
			return std::string{ aStop->Kind == core::update_line_place_kind::update_base ? "base" : "head" } + std::to_string(aStop->Update);
		};
		std::string labels;
		for (std::size_t i = 0; i < line->update_count(); ++i) {
			labels += text(line->label_text(i)) + " {" + text(line->node_tooltip(i)) + "} | ";
		}
		say("epoch:" + aTag + " visible=" + (line->isVisible() ? "1" : "0") + " controlVisible=" + (control->isVisible() ? "1" : "0") + " size=" + std::to_string(line->width()) + "x" + std::to_string(line->height())
			+ " updates=" + std::to_string(line->update_count()) + " offered=" + (line->selection_offered() ? "1" : "0") + " left=" + stop(line->left_stop()) + " right=" + stop(line->right_stop())
			+ " readiness=" + text(control->readiness_text()) + " statement=" + text(line->statement()) + " status=" + text(control->status_text()) + " labels=" + labels);
		say("epoch:" + aTag + " name=" + text(line->accessibleName()) + " base=" + text(line->base_tooltip()));
		for (std::size_t i = 0; i < line->update_count(); ++i) {
			const auto at        = line->node_centre(i);
			auto       menuEvent = QContextMenuEvent{ QContextMenuEvent::Mouse, at, line->mapToGlobal(at) };
			QCoreApplication::sendEvent(line, &menuEvent);
			for (auto* const menu : line->findChildren<QMenu*>(QStringLiteral("epochLineMenu"))) {
				std::string states;
				for (auto* const action : menu->actions()) {
					if (!action->text().isEmpty()) {
						states += text(action->text()) + "=" + (action->isEnabled() ? "1" : "0") + ";";
					}
				}
				auto said = "epoch:" + aTag;
				said.append(" node").append(std::to_string(i)).append(" menu ").append(states);
				say(said);
				menu->close();
				delete menu;
			}
		}
		say("epoch:" + aTag + " comparing=" + (control->comparing_button() ? text(control->comparing_button()->text()) : std::string{ "-" }));
		aWindow.grab().save(QString::fromStdString((aRoot / ("window-" + aTag + ".png")).string()));
		line->grab().save(QString::fromStdString((aRoot / ("line-" + aTag + ".png")).string()));
	}

	/// [[BPR-1553]]. Opens the pull request, opens the Updates line from the
	/// review bar if it is folded, moves to Review, and drags the right and
	/// then the left handle onto the oldest update, saying what the line shows
	/// before and after each drag. Nothing here writes to GitHub; a drag onto
	/// an update GitHub recorded imports its head into this run's own cache.
	void epoch_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 820);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 15 });
		epoch_dump(aWindow, aOptions.Root, "overview");
		if (!aWindow.epoch_selectors()->line()->isVisible()) {
			if (auto* const disclosure = aWindow.findChild<QToolButton*>(QStringLiteral("reviewBarUpdates")); disclosure != nullptr) {
				disclosure->click();
			}
		}
		wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 5 });
		epoch_dump(aWindow, aOptions.Root, "overview-open");
		aWindow.choose_modality(qt::review_modality::review);
		wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 20 });
		epoch_dump(aWindow, aOptions.Root, "review");
		auto* const line = aWindow.epoch_selectors()->line();
		if (line->right_stop()) {
			window_drag(*line, line->stop_position(*line->right_stop()), line->node_centre(0U));
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 15 });
			epoch_dump(aWindow, aOptions.Root, "review-right-dragged");
		}
		if (line->left_stop()) {
			window_drag(*line, line->stop_position(*line->left_stop()), line->node_centre(0U));
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 15 });
			epoch_dump(aWindow, aOptions.Root, "review-left-dragged");
		}
		// [[BPR-1557]]. Left steps the left handle back to the pull request's
		// base, the line's first stop.
		if (line->left_stop()) {
			line->setFocus(Qt::OtherFocusReason);
			auto left = QKeyEvent{ QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier };
			for (std::size_t step = 0U; step <= line->update_count(); ++step) {
				QCoreApplication::sendEvent(line, &left);
				wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 8 });
			}
			epoch_dump(aWindow, aOptions.Root, "review-left-base");
		}
		dump(aWindow);
	}

	/// JSON string escaping for the parity course's lines.
	auto json_text(const std::string_view aText) -> std::string
	{
		std::string result{ "\"" };
		for (const auto c : aText) {
			switch (c) {
			case '"':  result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20U) {
					constexpr auto digits = std::string_view{ "0123456789abcdef" };
					result += "\\u00";
					result += digits[(static_cast<unsigned char>(c) >> 4U) & 0xFU];
					result += digits[static_cast<unsigned char>(c) & 0xFU];
				} else {
					result += c;
				}
			}
		}
		return result + "\"";
	}

	/// The changed lines of one side: `[number, text]` for every line a
	/// non-equal segment covers, numbered from one as GitHub numbers them.
	auto changed_lines(const core::computed_file_diff& aDiff, const core::diff_side aSide) -> std::string
	{
		const auto& document = aSide == core::diff_side::left ? aDiff.LeftDocument : aDiff.RightDocument;
		std::string result{ "[" };
		auto        first = true;
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == core::alignment_kind::equal) {
				continue;
			}
			const auto& span = aSide == core::diff_side::left ? segment.Left : segment.Right;
			for (auto line = span.Begin; line < span.End; ++line) {
				auto content = std::string{ };
				if (document && line < document->Lines.size()) {
					const auto& bytes = document->Lines[line].Content;
					content.assign(reinterpret_cast<const char*>(document->Bytes.data()) + bytes.Begin, bytes.End - bytes.Begin);
				}
				result += (first ? "" : ",") + std::string{ "[" } + std::to_string(line + 1U) + "," + json_text(content) + "]";
				first = false;
			}
		}
		return result + "]";
	}

	/// [[BPR-1560]]. Opens the pull request in Review on the comparison it
	/// opens with and visits every file in the Files order, writing what each
	/// comparison read and showed. Nothing here writes to GitHub.
	void parity_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 820);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		require([&] { return aWindow.overview_page()->projection().has_value() && !aWindow.overview_page()->projection()->Files.empty(); }, "the overview lists the files");
		const auto files = aWindow.overview_page()->projection()->Files;
		aWindow.choose_modality(qt::review_modality::review);
		auto* const surface = aWindow.comparison_surface();
		const auto  pathOf  = [&]() -> std::optional<std::string> {
			const auto& context = surface->current_context();
			if (!context) {
				return std::nullopt;
			}
			for (const auto& row : files) {
				if (core::file_identity_of(row.Path) == context->Key.File) {
					return row.Path;
				}
			}
			return std::nullopt;
		};
		const auto settled = [&] {
			const auto& context = surface->current_context();
			const auto& result  = surface->current_result();
			return context && result && result->Alignment.Key == context->Key && pathOf().has_value();
		};
		std::ofstream            out{ aOptions.Root / "parity.jsonl" };
		std::vector<std::string> visited;
		for (std::size_t index = 0U; index < files.size(); ++index) {
			if (index == 0U) {
				if (!wait_for(settled, "the first file's comparison (opens it next)", std::chrono::seconds{ 15 })) {
					// [[BPR-1572]]. Says what the review shows while it waits.
					say("parity:waiting comparing=" + (aWindow.epoch_selectors()->comparing_button() ? text(aWindow.epoch_selectors()->comparing_button()->text()) : std::string{ "-" })
						+ " status=" + text(aWindow.epoch_selectors()->status_text()) + " surface=" + text(surface->status_text()));
					if (auto* const open = surface->idle_open_first_button(); open != nullptr && open->isVisible()) {
						open->click();
					}
					require(settled, "the first file's comparison", std::chrono::seconds{ 120 });
				}
			} else {
				const auto before = *pathOf();
				aWindow.next_file_action()->trigger();
				require([&] { return settled() && *pathOf() != before; }, "file " + std::to_string(index + 1U) + "'s comparison", std::chrono::seconds{ 120 });
			}
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 1 });
			require(settled, "the comparison stays settled");
			if (index == 0U) {
				// Read once the first comparison has settled, so it names the
				// comparison that was shown rather than a loading state.
				say("parity:comparing=" + (aWindow.epoch_selectors()->comparing_button() ? text(aWindow.epoch_selectors()->comparing_button()->text()) : std::string{ "-" })
					+ " status=" + text(aWindow.epoch_selectors()->status_text()));
			}
			const auto  path    = *pathOf();
			const auto& context = *surface->current_context();
			const auto& result  = *surface->current_result();
			const auto  blob    = [](const std::optional<core::git_blob_oid>& aBlob) {
				return aBlob ? json_text(core::canonical_hex(*aBlob).view()) : std::string{ "null" };
			};
			constexpr auto operations = std::array<std::string_view, 7U>{ "added", "modified", "removed", "renamed", "copied", "changed", "unchanged" };
			const auto     row        = std::ranges::find(files, path, &application::overview_file_row::Path);
			const auto     operation  = static_cast<std::size_t>(row->Operation);
			const auto     line       = "{\"path\":" + json_text(path) + ",\"status\":" + json_text(operation < operations.size() ? operations[operation] : "unknown")
				+ ",\"previous\":" + (row->PreviousPath ? json_text(*row->PreviousPath) : std::string{ "null" }) + ",\"left_blob\":" + blob(context.Key.LeftBlob) + ",\"right_blob\":" + blob(context.Key.RightBlob)
				+ ",\"content\":" + std::to_string(static_cast<int>(result.Alignment.Content)) + ",\"removed\":" + changed_lines(result, core::diff_side::left)
				+ ",\"added\":" + changed_lines(result, core::diff_side::right) + ",\"threads\":[";
			auto threads = std::string{ };
			for (const auto& tick : surface->thread_ticks()) {
				threads += (threads.empty() ? "" : ",") + std::string{ "[" } + json_text(tick.Side == core::diff_side::left ? "LEFT" : "RIGHT") + "," + std::to_string(tick.Line) + ","
					+ json_text(text(tick.Description)) + "]";
			}
			out << line << threads << "]}" << '\n';
			say("parity:file " + path);
			visited.push_back(path);
		}
		out.flush();
		say("parity:files=" + std::to_string(visited.size()) + " of " + std::to_string(files.size()));
		epoch_dump(aWindow, aOptions.Root, "parity");
		// The interdiff from the previous update, when there is one: every
		// file's two blobs, or the state the comparison says it is in.
		auto* const previous = aWindow.epoch_selectors()->previous_upload_action();
		say("parity:previous-upload enabled=" + std::string{ previous != nullptr && previous->isEnabled() ? "1" : "0" });
		if (previous != nullptr && previous->isEnabled()) {
			previous->trigger();
			const auto interdiff = [&] {
				return settled() && surface->current_context()->Key.LeftAnchor.Kind == core::comparison_anchor_kind::epoch_head;
			};
			const auto absent = [&] { return surface->status_text().contains(QStringLiteral("absent from both revisions")); };
			const auto ready  = wait_for([&] { return aWindow.epoch_selectors()->status_text() == QStringLiteral("Review history is ready.") && (interdiff() || absent()); },
										 "the interdiff (may be unavailable)", std::chrono::seconds{ 60 });
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 2 });
			say("parity:interdiff shown=" + std::string{ ready ? "1" : "0" } + " comparing="
				+ (aWindow.epoch_selectors()->comparing_button() ? text(aWindow.epoch_selectors()->comparing_button()->text()) : std::string{ "-" })
				+ " status=" + text(aWindow.epoch_selectors()->status_text()) + " surface=" + text(surface->status_text()));
			std::ofstream between{ aOptions.Root / "parity-interdiff.jsonl" };
			for (std::size_t index = 0U; ready && index < files.size(); ++index) {
				// Each file is chosen in Files, as a reviewer does; a file the
				// two updates both lack is its own state, not a comparison.
				const auto& path = files[index].Path;
				const auto  row  = aWindow.files_drawer()->contents()->index_for_file(core::file_identity_of(path));
				aWindow.files_drawer()->contents_view()->setCurrentIndex(row);
				const auto shownHere  = [&] { return interdiff() && pathOf() == path; };
				const auto absentHere = [&] {
					return absent() && aWindow.files_drawer()->contents_view()->currentIndex() == row;
				};
				if (!wait_for([&] { return shownHere() || absentHere(); }, "the file's interdiff", std::chrono::seconds{ 60 })) {
					between << "{\"path\":" << json_text(path) << ",\"state\":\"unavailable\"}\n";
					continue;
				}
				if (!shownHere()) {
					between << "{\"path\":" << json_text(path) << ",\"state\":\"absent-both\"}\n";
					continue;
				}
				const auto& context = *surface->current_context();
				const auto  blob    = [](const std::optional<core::git_blob_oid>& aBlob) {
					return aBlob ? json_text(core::canonical_hex(*aBlob).view()) : std::string{ "null" };
				};
				between << "{\"path\":" << json_text(path) << ",\"state\":\"shown\",\"left_blob\":" << blob(context.Key.LeftBlob) << ",\"right_blob\":" << blob(context.Key.RightBlob)
						<< ",\"removed\":" << changed_lines(*surface->current_result(), core::diff_side::left)
						<< ",\"added\":" << changed_lines(*surface->current_result(), core::diff_side::right) << "}\n";
			}
			epoch_dump(aWindow, aOptions.Root, "parity-interdiff");
		}
		dump(aWindow);
	}

	/// RFC-BPR-0089. Opens the pull request on Overview, shows the Commits tab
	/// (which reads the chain's trees, through gh for a sparse review), and
	/// for each commit chooses Show only selected and then Hide selected on
	/// the second, through the tab's own context menu. For each it writes the
	/// filtered file list and, for up to `aLimit` files, the diff the review
	/// shows: its two endpoint commits, its blobs and its changed lines, into
	/// DIR/commit-filter.jsonl, which `rfc0089_commit_filter_live_course.py`
	/// compares with `git diff C^ C`. Nothing here writes to GitHub.
	void commit_filter_course(const options& aOptions, qt::workbench_window& aWindow, const std::size_t aLimit = 12U)
	{
		aWindow.resize(1300, 860);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		require([&] { return aWindow.overview_page()->projection().has_value() && !aWindow.overview_page()->projection()->Files.empty(); }, "the overview lists the files");
		const auto whole  = aWindow.overview_page()->projection()->Files.size();
		auto*      drawer = aWindow.overview_page()->status_drawer();
		drawer->tabs()->setCurrentIndex(1);
		auto* const tab   = drawer->commits();
		const auto  ready = [&] {
			const auto& projection = tab->projection();
			return projection && !projection->Rows.empty()
				&& (projection->Trees == application::commit_filter_trees::ready || projection->Trees == application::commit_filter_trees::objects_unavailable
					|| projection->Trees == application::commit_filter_trees::limit_exceeded || projection->Trees == application::commit_filter_trees::failed);
		};
		require(ready, "the Commits tab reads the chain's trees", std::chrono::seconds{ 600 });
		say("commit-filter:status=" + text(tab->status()->text()));
		if (tab->projection()->Trees != application::commit_filter_trees::ready) {
			fail("the chain's trees are not ready: " + text(tab->status()->text()));
		}
		std::ofstream out{ aOptions.Root / "commit-filter.jsonl" };
		auto          rows = std::string{ };
		for (const auto& row : tab->projection()->Rows) {
			rows += (rows.empty() ? "" : ",") + std::string{ "{\"commit\":" } + json_text(row.Commit) + ",\"headline\":" + json_text(row.Headline) + ",\"author\":" + json_text(row.Author)
				+ ",\"files\":" + (row.Files ? std::to_string(*row.Files) : std::string{ "null" }) + ",\"role\":" + std::to_string(static_cast<int>(row.Role)) + "}";
		}
		out << "{\"kind\":\"commits\",\"whole\":" << whole << ",\"rows\":[" << rows << "]}\n";
		auto* const surface  = aWindow.comparison_surface();
		const auto  hex      = [](const core::git_commit_oid& aOid) { return json_text(core::canonical_hex(aOid).view()); };
		const auto  blob     = [](const std::optional<core::git_blob_oid>& aBlob) { return aBlob ? json_text(core::canonical_hex(*aBlob).view()) : std::string{ "null" }; };
		const auto  scenario = [&](const std::string& aName, const QString& aAction, const std::vector<std::string>& aCommits) {
			auto selection = QItemSelection{ };
			for (const auto& commit : aCommits) {
				const auto row = tab->model()->row_of(commit);
				selection.select(tab->model()->index(row), tab->model()->index(row));
			}
			tab->list()->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
			auto        menu   = std::unique_ptr<QMenu>{ tab->make_menu(nullptr) };
			auto* const action = menu->findChild<QAction*>(aAction);
			if (action == nullptr || !action->isEnabled()) {
				fail(aName + ": the context menu does not offer the action");
			}
			action->trigger();
			const auto filtered = [&] {
				const auto& projection = tab->projection();
				return projection && projection->FilteredFiles && aWindow.overview_page()->projection()
					&& aWindow.overview_page()->projection()->Files.size() == *projection->FilteredFiles;
			};
			require(filtered, aName + ": the filtered file set");
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 1 });
			auto paths = std::vector<std::string>{ };
			for (const auto& row : aWindow.overview_page()->projection()->Files) {
				paths.push_back(row.Path);
			}
			auto files = std::string{ };
			for (const auto& path : paths) {
				files += (files.empty() ? "" : ",") + json_text(path);
			}
			const auto chip = text(aWindow.overview_page()->commit_chip()->summary()->text()) + " / " + text(aWindow.overview_page()->commit_chip()->detail()->text());
			say("commit-filter:" + aName + " files=" + std::to_string(paths.size()) + " chip=" + chip);
			aWindow.choose_modality(qt::review_modality::review);
			auto diffs = std::string{ };
			for (std::size_t index = 0U; index < paths.size() && index < aLimit; ++index) {
				const auto& path = paths[index];
				aWindow.files_drawer()->contents_view()->setCurrentIndex(aWindow.files_drawer()->contents()->index_for_file(core::file_identity_of(path)));
				const auto settled = [&] {
					const auto& context = surface->current_context();
					const auto& result  = surface->current_result();
					return context && result && result->Alignment.Key == context->Key && context->Key.File == core::file_identity_of(path);
				};
				if (!wait_for(settled, std::string{ aName }.append(" ").append(path), std::chrono::seconds{ 120 })) {
					fail(std::string{ aName }.append(": ").append(path).append(" never showed"));
				}
				const auto shown = [&] {
					const auto& context = *surface->current_context();
					const auto& result  = *surface->current_result();
					return std::string{ "\"left\":" } + (context.Key.Filter ? hex(context.Key.Filter->Left) : std::string{ "null" })
						+ ",\"right\":" + (context.Key.Filter ? hex(context.Key.Filter->Right) : std::string{ "null" }) + ",\"left_blob\":" + blob(context.Key.LeftBlob)
						+ ",\"right_blob\":" + blob(context.Key.RightBlob) + ",\"removed\":" + changed_lines(result, core::diff_side::left)
						+ ",\"added\":" + changed_lines(result, core::diff_side::right);
				};
				// RFC-BPR-0089 M3. A split file reads each shown commit's own
				// diff in turn, through the strip over it.
				auto* const strip = aWindow.split_strip();
				wait_for([&] { return strip->isVisibleTo(&aWindow); }, "a strip, if the file is split (expected for most files)", std::chrono::seconds{ 1 });
				if (!strip->isVisibleTo(&aWindow)) {
					diffs += (diffs.empty() ? "" : ",") + std::string{ "{\"path\":" } + json_text(path) + "," + shown() + "}";
					continue;
				}
				auto segments = std::string{ };
				for (std::size_t part = 0U; part < strip->parts().size(); ++part) {
					strip->parts()[part]->click();
					require([&] { return settled() && strip->parts().size() > part && strip->parts()[part]->isChecked() && strip->projection() && strip->projection()->Parts[part].Selected; },
							std::string{ aName }.append(": ").append(path).append(" diff ").append(std::to_string(part + 1U)));
					segments += (segments.empty() ? "{" : ",{") + shown() + ",\"commit\":" + json_text(strip->projection()->Parts[part].Commit) + "}";
				}
				auto hidden = std::string{ };
				for (const auto* const label : strip->hidden()) {
					hidden += (hidden.empty() ? "" : ",") + json_text(text(label->accessibleName()));
				}
				auto entry = std::string{ "{\"path\":" };
				entry.append(json_text(path)).append(",\"split\":true,\"segments\":[").append(segments).append("],\"hidden\":[").append(hidden).append("]}");
				diffs.append(diffs.empty() ? "" : ",").append(entry);
			}
			out << "{\"kind\":\"scenario\",\"name\":" << json_text(aName) << ",\"commits\":[";
			for (std::size_t index = 0U; index < aCommits.size(); ++index) {
				out << (index == 0U ? "" : ",") << json_text(aCommits[index]);
			}
			out << "],\"chip\":" << json_text(chip) << ",\"files\":[" << files << "],\"diffs\":[" << diffs << "]}\n";
			out.flush();
			aWindow.show_commits();
			aWindow.overview_page()->commit_chip()->clear_button()->click();
			require([&] { return aWindow.overview_page()->projection() && aWindow.overview_page()->projection()->Files.size() == whole; }, aName + ": Clear all filters restores Whole PR");
		};
		const auto& listed = tab->projection()->Rows;
		auto        chain  = std::vector<std::string>{ };
		for (const auto& row : listed) {
			if (row.Role != core::filter_commit_role::side) {
				chain.push_back(row.Commit);
			}
		}
		for (std::size_t index = 0U; index < chain.size(); ++index) {
			scenario("only-" + std::to_string(index + 1U), QStringLiteral("commitFilterShowOnly"), { chain[index] });
		}
		if (chain.size() >= 2U) {
			scenario("hide-2", QStringLiteral("commitFilterHide"), { chain[1] });
		}
		say("commit-filter:scenarios=" + std::to_string(chain.size() + (chain.size() >= 2U ? 1U : 0U)));
	}

	/// RFC-BPR-0089 M4 (owner decision Q1). Where a comment on a filtered
	/// diff's line goes, as the window's composition decides it with its core
	/// and as the editor says before Publish, written into
	/// DIR/filtered-comment.jsonl. A tests-enabled composition holds no
	/// publication grant, so the operator publishes exactly these placements
	/// through gh while the course waits at `at-post`; after `go-post` the
	/// course refreshes and writes the lines the threads are marked at.
	void filtered_comment_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 860);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		require([&] { return aWindow.overview_page()->projection().has_value() && !aWindow.overview_page()->projection()->Files.empty(); }, "the overview lists the files");
		auto* const drawer = aWindow.overview_page()->status_drawer();
		drawer->tabs()->setCurrentIndex(1);
		auto* const tab = drawer->commits();
		require([&] { return tab->projection() && !tab->projection()->Rows.empty() && tab->projection()->Trees == application::commit_filter_trees::ready; },
				"the Commits tab reads the chain's trees", std::chrono::seconds{ 600 });
		auto chain = std::vector<std::string>{ };
		for (const auto& row : tab->projection()->Rows) {
			if (row.Role != core::filter_commit_role::side) {
				chain.push_back(row.Commit);
			}
		}
		if (chain.size() < 2U) {
			fail("the course needs at least two chain commits");
		}
		const auto  last        = chain.back();
		auto* const surface     = aWindow.comparison_surface();
		const auto  hex         = [](const core::git_commit_oid& aOid) { return json_text(core::canonical_hex(aOid).view()); };
		const auto  gestureOnce = [&](const QString& aAction) {
			aWindow.show_commits();
			const auto row = tab->model()->row_of(last);
			tab->list()->selectionModel()->select(QItemSelection{ tab->model()->index(row), tab->model()->index(row) }, QItemSelectionModel::ClearAndSelect);
			auto        menu   = std::unique_ptr<QMenu>{ tab->make_menu(nullptr) };
			auto* const action = menu->findChild<QAction*>(aAction);
			if (action == nullptr || !action->isEnabled()) {
				fail("the context menu does not offer " + text(aAction));
			}
			action->trigger();
		};
		// [[BPR-1707]]. Made once: a gesture made while a refresh resolves the
		// comparison again applies (before, it was lost and M4 repeated it).
		const auto gesture = [&](const QString& aAction, const std::string& aShown) {
			gestureOnce(aAction);
			require([&] { return text(aWindow.overview_page()->commit_chip()->summary()->text()) == aShown; }, "the chip reads " + aShown, std::chrono::seconds{ 60 });
		};
		const auto clear = [&] {
			aWindow.show_commits();
			if (aWindow.overview_page()->commit_chip()->isVisible()) {
				aWindow.overview_page()->commit_chip()->clear_button()->click();
			}
			require([&] { return !aWindow.overview_page()->commit_chip()->isVisible(); }, "Clear all filters");
		};
		const auto openFile = [&] {
			aWindow.choose_modality(qt::review_modality::review);
			const auto file = core::file_identity_of(aOptions.Path);
			require([&] { return aWindow.files_drawer()->contents()->index_for_file(file).isValid(); }, "the file is listed");
			aWindow.files_drawer()->contents_view()->setCurrentIndex(aWindow.files_drawer()->contents()->index_for_file(file));
			require([&] {
				const auto& context = surface->current_context();
				const auto& result  = surface->current_result();
				return context && result && result->Alignment.Key == context->Key && context->Key.File == file;
			},
					"the file's diff is shown", std::chrono::seconds{ 120 });
		};
		std::ofstream out{ aOptions.Root / "filtered-comment.jsonl" };
		const auto    probe = [&](const std::string& aScenario, const core::diff_side aSide, const std::uint32_t aLine) {
			if (!surface->select_conversation_line(aSide, aLine)) {
				fail(aScenario + ": the line cannot be selected");
			}
			const auto decided = [&] {
				const auto& placed    = aWindow.filtered_comment_placement();
				const auto  selection = surface->selection();
				return placed && selection && placed->Selection == selection->PreferredLine && placed->Target.Refusal != core::filtered_comment_refusal::lines_pending;
			};
			require(decided, aScenario + ": where a comment on the line goes", std::chrono::seconds{ 120 });
			const auto& placed            = *aWindow.filtered_comment_placement();
			const auto [notice, blocking] = aWindow.filtered_comment_notice();
			const auto& filter            = surface->current_context()->Key.Filter;
			out << "{\"kind\":\"placement\",\"scenario\":" << json_text(aScenario) << ",\"side\":" << json_text(aSide == core::diff_side::left ? "LEFT" : "RIGHT")
				<< ",\"line\":" << aLine << ",\"left\":" << (filter ? hex(filter->Left) : std::string{ "null" }) << ",\"right\":" << (filter ? hex(filter->Right) : std::string{ "null" })
				<< ",\"placement\":" << static_cast<int>(placed.Target.Placement) << ",\"refusal\":" << static_cast<int>(placed.Target.Refusal)
				<< ",\"commit\":" << (placed.Target.Commit.valid() ? hex(placed.Target.Commit) : std::string{ "null" })
				<< ",\"target_side\":" << json_text(placed.Target.Side == core::diff_side::left ? "LEFT" : "RIGHT") << ",\"target_line\":" << placed.Target.Line
				<< ",\"notice\":" << json_text(text(notice)) << ",\"blocks\":" << (blocking ? "true" : "false") << "}\n";
			out.flush();
			say("filtered-comment:" + aScenario + " " + (aSide == core::diff_side::left ? "LEFT " : "RIGHT ") + std::to_string(aLine + 1U) + " -> placement=" + std::to_string(static_cast<int>(placed.Target.Placement))
				+ " line=" + std::to_string(placed.Target.Line + 1U) + " notice=" + text(notice));
		};
		const auto marks = [&](const std::string& aScenario) {
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 3 });
			const auto lines = [&](const core::diff_side aSide) {
				auto result = std::string{ };
				for (const auto& mark : surface->line_marks(aSide)) {
					if (!mark.Draft) {
						result += (result.empty() ? "" : ",") + std::to_string(mark.Line);
					}
				}
				return result;
			};
			out << "{\"kind\":\"marks\",\"scenario\":" << json_text(aScenario) << ",\"right\":[" << lines(core::diff_side::right) << "],\"left\":[" << lines(core::diff_side::left) << "]}\n";
			out.flush();
			say("filtered-comment:marks " + aScenario + " right=[" + lines(core::diff_side::right) + "] left=[" + lines(core::diff_side::left) + "]");
		};

		// The last commit hidden: the file's diff ends at the commit before it.
		gesture(QStringLiteral("commitFilterHide"), std::to_string(chain.size() - 1U) + " of " + std::to_string(chain.size()) + " commits shown");
		openFile();
		probe("hide-last", core::diff_side::right, 14U);
		probe("hide-last", core::diff_side::right, 4U);
		probe("hide-last", core::diff_side::right, 9U);
		// Only the last commit: the diff's left side is the commit before it.
		clear();
		gesture(QStringLiteral("commitFilterShowOnly"), "1 of " + std::to_string(chain.size()) + " commits shown");
		openFile();
		probe("only-last", core::diff_side::left, 14U);
		probe("only-last", core::diff_side::left, 9U);
		probe("only-last", core::diff_side::left, 12U);
		clear();
		checkpoint(aOptions.Root, "post", "placements written");

		// The status already reads ready before the refresh lands, so the
		// course lets it settle before it waits for the refreshed state.
		aWindow.github_refresh_action()->trigger();
		wait_for([] { return false; }, "the refresh settles (expected)", std::chrono::seconds{ 15 });
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the refreshed pull request is ready");
		require([&] { return tab->projection() && tab->projection()->Trees == application::commit_filter_trees::ready; }, "the trees after the refresh", std::chrono::seconds{ 600 });
		gesture(QStringLiteral("commitFilterHide"), std::to_string(chain.size() - 1U) + " of " + std::to_string(chain.size()) + " commits shown");
		openFile();
		marks("hide-last");
		clear();
		openFile();
		marks("whole");
		dump(aWindow);
	}

	/// [[BPR-1735]]. Where a comment on a Whole PR line goes, as the window's
	/// composition places it with its core from GitHub's own diff (the merge
	/// base to the head, read from the review's objects), and what the editor
	/// says before Publish, written into DIR/whole-pr-comment.jsonl. A line
	/// the core places on itself shows no placement; one outside every hunk
	/// shows the file comment. A tests-enabled composition holds no
	/// publication grant, so the operator publishes exactly these placements
	/// through gh while the course waits at `at-post`.
	void whole_pr_comment_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 860);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		require([&] { return aWindow.overview_page()->projection().has_value() && !aWindow.overview_page()->projection()->Files.empty(); }, "the overview lists the files");
		aWindow.choose_modality(qt::review_modality::review);
		auto* const surface = aWindow.comparison_surface();
		const auto  file    = core::file_identity_of(aOptions.Path);
		require([&] { return aWindow.files_drawer()->contents()->index_for_file(file).isValid(); }, "the file is listed");
		aWindow.files_drawer()->contents_view()->setCurrentIndex(aWindow.files_drawer()->contents()->index_for_file(file));
		require([&] {
			const auto& context = surface->current_context();
			const auto& result  = surface->current_result();
			return context && result && result->Alignment.Key == context->Key && context->Key.File == file && !context->Key.Filter;
		},
				"the file's Whole PR diff is shown", std::chrono::seconds{ 120 });
		std::ofstream out{ aOptions.Root / "whole-pr-comment.jsonl" };
		const auto    probe = [&](const std::uint32_t aLine, const bool aOffDiff) {
			if (!surface->select_conversation_line(core::diff_side::right, aLine)) {
				fail("line " + std::to_string(aLine + 1U) + " cannot be selected");
			}
			const auto placed = [&] {
				const auto& shown     = aWindow.filtered_comment_placement();
				const auto  selection = surface->selection();
				return shown && selection && shown->Selection == selection->PreferredLine;
			};
			if (aOffDiff) {
				require(placed, "where a comment on line " + std::to_string(aLine + 1U) + " goes", std::chrono::seconds{ 120 });
			} else {
				// GitHub's diff is read by now: no placement is the line itself.
				wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 3 });
			}
			const auto [notice, blocking] = aWindow.filtered_comment_notice();
			const auto& shown             = aWindow.filtered_comment_placement();
			const auto  target            = placed() ? std::optional{ shown->Target } : std::nullopt;
			out << "{\"kind\":\"placement\",\"line\":" << aLine << ",\"placement\":" << (target ? std::to_string(static_cast<int>(target->Placement)) : std::string{ "null" })
				<< ",\"refusal\":" << (target ? std::to_string(static_cast<int>(target->Refusal)) : std::string{ "null" })
				<< ",\"commit\":" << (target && target->Commit.valid() ? json_text(core::canonical_hex(target->Commit).view()) : std::string{ "null" })
				<< ",\"notice\":" << json_text(text(notice)) << ",\"blocks\":" << (blocking ? "true" : "false") << "}\n";
			out.flush();
			say("whole-pr-comment:RIGHT " + std::to_string(aLine + 1U) + " -> " + (target ? "placement=" + std::to_string(static_cast<int>(target->Placement)) : std::string{ "line comment" })
				+ " notice=" + text(notice));
		};
		probe(59U, true);
		probe(13U, true);
		probe(12U, false);
		checkpoint(aOptions.Root, "post", "placements written");
		dump(aWindow);
	}

	/// RFC-BPR-0089 M3, AC06. Opens the pull request, marks `--path` read in
	/// Review (a local receipt only), hides the chain's last commit, and waits
	/// at `at-push` for the operator's force push. After `go-push` it
	/// refreshes and writes, into DIR/force-push.jsonl, the rows, the chip and
	/// the rematch offers, whether the receipt is still read, and after the
	/// reviewer's choice of the chip's offer the rows and the chip again.
	/// Nothing here writes to GitHub.
	void force_push_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 860);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		auto* const drawer = aWindow.overview_page()->status_drawer();
		auto* const tab    = drawer->commits();
		auto* const chip   = aWindow.overview_page()->commit_chip();
		const auto  trees  = [&] {
			drawer->tabs()->setCurrentIndex(1);
			return tab->projection() && !tab->projection()->Rows.empty() && tab->projection()->Trees == application::commit_filter_trees::ready;
		};
		require(trees, "the Commits tab reads the chain's trees", std::chrono::seconds{ 600 });
		std::ofstream out{ aOptions.Root / "force-push.jsonl" };
		const auto    rows = [&] {
			auto result = std::string{ };
			for (const auto& row : tab->projection()->Rows) {
				result += (result.empty() ? "" : ",") + std::string{ "{\"commit\":" } + json_text(row.Commit) + ",\"state\":" + std::to_string(static_cast<int>(row.State)) + "}";
			}
			return "[" + result + "]";
		};
		const auto chipText = [&] { return json_text(text(chip->summary()->text()) + " / " + text(chip->detail()->text())); };

		// A Whole PR receipt before anything moves.
		const auto path = core::file_identity_of(aOptions.Path);
		const auto read = [&] { return aWindow.read_action()->text() == QStringLiteral("Mark &unread"); };
		const auto open = [&] {
			aWindow.choose_modality(qt::review_modality::review);
			require([&] { return aWindow.files_drawer()->contents()->index_for_file(path).isValid(); }, "the receipt's file is listed");
			aWindow.files_drawer()->contents_view()->setCurrentIndex(aWindow.files_drawer()->contents()->index_for_file(path));
			require([&] {
				const auto& context = aWindow.comparison_surface()->current_context();
				return context && context->Key.File == path && aWindow.read_action()->isEnabled();
			},
					"the receipt's file is shown");
		};
		open();
		const auto readBefore = read();
		if (!readBefore) {
			aWindow.read_action()->trigger();
		}
		require(read, "the receipt's file is read");
		aWindow.show_commits();
		require(trees, "the Commits tab again");
		const auto last = tab->projection()->Rows.back().Commit;
		tab->list()->selectionModel()->select(tab->model()->index(tab->model()->row_of(last)), QItemSelectionModel::ClearAndSelect);
		{
			auto        menu   = std::unique_ptr<QMenu>{ tab->make_menu(nullptr) };
			auto* const action = menu->findChild<QAction*>(QStringLiteral("commitFilterHide"));
			if (action == nullptr || !action->isEnabled()) {
				fail("Hide selected is not offered for the last commit");
			}
			action->trigger();
		}
		require([&] { return tab->projection()->Status == core::commit_filter_status::active; }, "the last commit is hidden");
		out << "{\"kind\":\"before\",\"hidden\":" << json_text(last) << ",\"rows\":" << rows() << ",\"chip\":" << chipText() << "}\n";
		out.flush();
		say("force-push:hidden " + last + " chip=" + text(chip->summary()->text()));
		checkpoint(aOptions.Root, "push", "{\"hidden\":" + json_text(last) + "}");

		aWindow.github_refresh_action()->trigger();
		const auto offered = [&] {
			const auto& projection = tab->projection();
			return trees() && projection->Gone.size() == 1U && !projection->Rematches.empty() && projection->Rematches.front().Basis == core::filter_rematch_basis::same_change
				&& !chip->rematch_button()->isHidden();
		};
		const auto sameChange = wait_for(offered, "the same-change offer after the push", std::chrono::seconds{ 600 });
		auto       offers     = std::string{ };
		for (const auto& offer : tab->projection()->Rematches) {
			offers += (offers.empty() ? "" : ",") + std::string{ "{\"gone\":" } + json_text(offer.Gone) + ",\"candidate\":" + json_text(offer.Candidate) + ",\"basis\":" + std::to_string(static_cast<int>(offer.Basis)) + "}";
		}
		auto gone = std::string{ };
		for (const auto& entry : tab->projection()->Gone) {
			gone += (gone.empty() ? "" : ",") + json_text(entry.ShortCommit);
		}
		const auto shownBefore = tab->projection()->Status;
		open();
		const auto kept = wait_for(read, "the receipt after the push", std::chrono::seconds{ 60 });
		aWindow.show_commits();
		out << "{\"kind\":\"pushed\",\"same_change\":" << (sameChange ? "true" : "false") << ",\"rows\":" << rows() << ",\"gone\":[" << gone << "],\"rematches\":[" << offers
			<< "],\"status\":" << static_cast<int>(shownBefore) << ",\"chip\":" << chipText() << ",\"button\":" << json_text(text(chip->rematch_button()->text()))
			<< ",\"receipt_kept\":" << (kept ? "true" : "false") << "}\n";
		out.flush();
		say("force-push:offered " + text(chip->rematch_button()->text()) + " chip=" + text(chip->detail()->text()));
		if (!sameChange) {
			fail("no same-change offer after the push");
		}
		chip->rematch_button()->click();
		require([&] { return tab->projection()->Gone.empty() && tab->projection()->Rematches.empty() && tab->projection()->Status == core::commit_filter_status::active; },
				"the offer is applied");
		out << "{\"kind\":\"rematched\",\"rows\":" << rows() << ",\"chip\":" << chipText() << "}\n";
		out.flush();
		say("force-push:rematched chip=" + text(chip->summary()->text()));
	}

	/// [[BPR-1692]]. Opens the pull request on Overview and waits until the
	/// head's `.gitattributes` are read (or say why not). Writes Whole PR's
	/// listed files, the generated row and the heading, then for each commit
	/// Show only selected and Hide selected on the second, and Clear all
	/// filters, the same again, and the Files drawer's walk in Review, into
	/// DIR/generated.jsonl, which `rfc0089_commit_filter_live_course.py
	/// --generated` compares with `git check-attr --source <head>`. Nothing
	/// here writes to GitHub.
	void generated_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1300, 860);
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.github_status_text() == "Pull request ready."; }, "the pull request is ready");
		const auto* const page = aWindow.overview_page();
		require([&] { return page->projection().has_value() && page->projection()->GeneratedAttributes != application::generated_attributes_read::pending; },
				"the head's .gitattributes are read", std::chrono::seconds{ 600 });
		std::ofstream out{ aOptions.Root / "generated.jsonl" };
		const auto    record = [&](const std::string& aName) {
			wait_for([] { return false; }, "settle (expected)", std::chrono::seconds{ 1 });
			const auto& projection = *page->projection();
			auto        files      = std::string{ };
			for (const auto& row : projection.Files) {
				files += (files.empty() ? "" : ",") + json_text(row.Path);
			}
			auto generated = std::string{ };
			for (const auto& row : page->generated_row()->rows()) {
				generated += (generated.empty() ? "" : ",") + json_text(row.Path) + ":" + std::to_string(static_cast<int>(row.Reason));
			}
			out << "{\"kind\":\"view\",\"name\":" << json_text(aName) << ",\"attributes\":" << static_cast<int>(projection.GeneratedAttributes)
				<< ",\"heading\":" << json_text(text(page->files_heading()->text())) << ",\"row\":" << json_text(text(page->generated_row()->toggle()->text()))
				<< ",\"files\":[" << files << "],\"generated\":{" << generated << "}}\n";
			out.flush();
			say("generated:" + aName + " files=" + std::to_string(projection.Files.size()) + " set-aside=" + std::to_string(page->generated_row()->rows().size())
				+ " heading=" + text(page->files_heading()->text()));
		};
		record("whole");
		aWindow.choose_modality(qt::review_modality::review);
		require([&] { return aWindow.files_drawer()->contents()->rowCount() == static_cast<int>(page->projection()->Files.size()); }, "the Files drawer lists the same files");
		auto walk = std::string{ };
		for (auto row = 0; row < aWindow.files_drawer()->contents()->rowCount(); ++row) {
			walk += (walk.empty() ? "" : ",")
				+ json_text(aWindow.files_drawer()->contents()->data(aWindow.files_drawer()->contents()->index(row, 0), Qt::DisplayRole).toString().toStdString());
		}
		out << "{\"kind\":\"walk\",\"paths\":[" << walk << "]}\n";
		aWindow.show_commits();
		auto* drawer = aWindow.overview_page()->status_drawer();
		drawer->tabs()->setCurrentIndex(1);
		auto* const tab = drawer->commits();
		require([&] { return tab->projection() && !tab->projection()->Rows.empty() && tab->projection()->Trees == application::commit_filter_trees::ready; },
				"the Commits tab reads the chain's trees", std::chrono::seconds{ 600 });
		const auto whole    = page->projection()->CommitFilterComparisonFiles.value_or(page->projection()->Files.size());
		const auto scenario = [&](const std::string& aName, const QString& aAction, const std::vector<std::string>& aCommits) {
			auto selection = QItemSelection{ };
			for (const auto& commit : aCommits) {
				const auto row = tab->model()->row_of(commit);
				selection.select(tab->model()->index(row), tab->model()->index(row));
			}
			tab->list()->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
			auto        menu   = std::unique_ptr<QMenu>{ tab->make_menu(nullptr) };
			auto* const action = menu->findChild<QAction*>(aAction);
			if (action == nullptr || !action->isEnabled()) {
				fail(aName + ": the context menu does not offer the action");
			}
			action->trigger();
			require([&] { return tab->projection() && tab->projection()->FilteredFiles && page->projection() && page->projection()->Files.size() == *tab->projection()->FilteredFiles; },
					aName + ": the filtered file set");
			record(aName);
			aWindow.overview_page()->commit_chip()->clear_button()->click();
			require([&] { return !tab->projection()->CommitFiltered; }, aName + ": Clear all filters");
		};
		auto chain = std::vector<std::string>{ };
		for (const auto& row : tab->projection()->Rows) {
			if (row.Role != core::filter_commit_role::side) {
				chain.push_back(row.Commit);
			}
		}
		out << "{\"kind\":\"commits\",\"whole\":" << whole << ",\"chain\":[";
		for (std::size_t index = 0U; index < chain.size(); ++index) {
			out << (index == 0U ? "" : ",") << json_text(chain[index]);
		}
		out << "]}\n";
		for (std::size_t index = 0U; index < chain.size(); ++index) {
			scenario("only-" + std::to_string(index + 1U), QStringLiteral("commitFilterShowOnly"), { chain[index] });
		}
		if (chain.size() >= 2U) {
			scenario("hide-2", QStringLiteral("commitFilterHide"), { chain[1] });
		}
		record("cleared");
	}

	void observer_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		open_pull(aWindow, aOptions.Pull);
		require([&] { return aWindow.add_queue_rule_action() != nullptr && aWindow.queue_rule_kind_selector() != nullptr; }, "queue controls exist");
		const std::vector<core::observer_rule> rules{
			{ .Kind = core::observer_rule_kind::repository, .Pattern = "MikeMarcin/BPR-Test" },
			{ .Kind = core::observer_rule_kind::author, .Pattern = aOptions.Author },
			{ .Kind = core::observer_rule_kind::path, .Pattern = aOptions.Path },
		};
		const auto enrolled = [&](const core::observer_rule& aRule) -> std::optional<core::queue_item> {
			auto* const landing = aWindow.queue_landing();
			for (int row = 0; row < landing->rowCount(); ++row) {
				const auto item = landing->item_at(landing->index(row));
				if (item && item->Ref.Number == aOptions.Pull && item->Membership.MatchedRule && item->Membership.Rule == aRule) {
					return item;
				}
			}
			return std::nullopt;
		};
		for (const auto& rule : rules) {
			const auto kind     = std::string{ core::name(rule.Kind) };
			auto*      selector = aWindow.queue_rule_kind_selector();
			selector->setCurrentIndex(selector->findData(static_cast<int>(rule.Kind)));
			aWindow.queue_rule_pattern_editor()->setText(QString::fromStdString(rule.Pattern));
			require([&] { return aWindow.add_queue_rule_action()->isEnabled(); }, kind + " rule can be added");
			aWindow.add_queue_rule_action()->trigger();
			require([&] { return enrolled(rule).has_value(); }, "#" + std::to_string(aOptions.Pull) + " enrolled by the " + kind + " rule");
			const auto item  = *enrolled(rule);
			const auto facts = "{\"rule\":\"" + kind + "\",\"pattern\":\"" + rule.Pattern + "\",\"ref\":\"" + item.Ref.Host + "/" + item.Ref.Repository + "#" + std::to_string(item.Ref.Number)
				+ "\",\"account\":\"" + item.Ref.Account + "\",\"role\":\"" + std::string{ core::name(item.Membership.Role) } + "\",\"authored\":" + (item.Membership.Authored ? "true" : "false")
				+ ",\"requested\":" + (item.Membership.Requested ? "true" : "false") + ",\"rows\":" + std::to_string(aWindow.queue_landing()->rowCount()) + "}";
			checkpoint(aOptions.Root, "enrolled-" + kind, facts);
			aWindow.remove_queue_rule_action()->trigger();
			require([&] { return !enrolled(rule).has_value() && !aWindow.remove_queue_rule_action()->isEnabled(); }, kind + " rule removed");
			checkpoint(aOptions.Root, "removed-" + kind, "{\"rule\":\"" + kind + "\",\"rows\":" + std::to_string(aWindow.queue_landing()->rowCount()) + "}");
		}
	}
	/// RFC-BPR-0099 AC08, RFC-BPR-0105 AC08. One window, one session: the
	/// course opens each `--reads` pull request in turn (on the first it asks
	/// the installed agent at `--agent` one question from the Agent tab, and
	/// it shows each one's Tour tab), then on `--pull` writes a two-stop tour
	/// as an author does (Prepare review tour, the selected lines, Add stop,
	/// the stop's own Accept), waits at `at-push` while the operator pushes a
	/// commit that leaves stop 2 at an earlier version, refreshes, prepares
	/// the publication and presses Publish, then opens `--approve` and
	/// presses Approve. It writes what the panel, the head and the review
	/// page showed at every change into DIR/live-session.jsonl.
	///
	/// Publishing needs the author's authorization, and here the course is
	/// the author: it authorizes only through the window's own Publish and
	/// Approve, and answers a question those raise as a person would (logged
	/// word for word). The composition issues the one-use grant that gesture
	/// issues in a shipped build; only `DesignatedLivePublication` lets a
	/// tests-enabled build compose the publishing connection, and only for
	/// MikeMarcin/BPR-Test.
	void live_session_course(const options& aOptions, qt::workbench_window& aWindow)
	{
		aWindow.resize(1400, 900);
		std::ofstream out{ aOptions.Root / "live-session.jsonl", std::ios::app };
		const auto    record = [&](const std::string& aKind, const std::string& aFacts) {
			out << "{\"kind\":" << json_text(aKind) << "," << aFacts << "}\n";
			out.flush();
			say("live-session:" + aKind + " " + aFacts);
		};
		const auto ready = [&](const int aPull) {
			open_pull(aWindow, aPull);
			if (!wait_for([&] { return aWindow.github_status_text() == "Pull request ready." && aWindow.overview_page()->projection().has_value() && static_cast<int>(aWindow.overview_page()->projection()->Number.value()) == aPull; }, "#" + std::to_string(aPull) + " is ready", std::chrono::seconds{ 240 })) {
				dump(aWindow);
				say("dump:projection=" + std::string{ aWindow.overview_page()->projection().has_value() ? std::to_string(aWindow.overview_page()->projection()->Number.value()) : std::string{ "none" } });
				aWindow.grab().save(QString::fromStdString((aOptions.Root / ("not-ready-" + std::to_string(aPull) + ".png")).string()));
				fail("#" + std::to_string(aPull) + " never became ready");
			}
			aWindow.choose_modality(qt::review_modality::review);
		};
		const auto show_tray = [&](const QString& aName) {
			for (int index = 0; index < aWindow.tray_tabs()->count(); ++index) {
				if (aWindow.tray_tabs()->accessibleTabName(index) == aName) {
					aWindow.tray_tabs()->setCurrentIndex(index);
					return true;
				}
			}
			return false;
		};

		// 1. The reads: a session of several pull requests, agent and tour.
		auto opened = 0;
		for (const auto pull : aOptions.Reads) {
			ready(pull);
			++opened;
			const auto tour = show_tray(QStringLiteral("Tour"));
			wait_for([] { return false; }, "a reader's look (expected)", std::chrono::seconds{ 3 });
			record("read", "\"pull\":" + std::to_string(pull) + ",\"tour_tab\":" + (tour ? "true" : "false") + ",\"status\":" + json_text(aWindow.github_status_text()));
			if (opened == 1 && !aOptions.Agent.empty()) {
				aWindow.agent_programs()->set_editor([&](const core::agent_entry& aStart, QWidget*) -> std::optional<core::agent_entry> {
					auto entry    = aStart;
					entry.Program = aOptions.Agent;
					entry.Model   = "haiku";
					return entry;
				});
				auto*      panel = aWindow.agent_panel();
				const auto id    = QVariant::fromValue<qulonglong>(core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U).value());
				require([&] {
					if (panel->profile_selector()->findData(id) < 0) {
						aWindow.agent_programs()->add_agent(core::setup_agent::claude_code);
					}
					return panel->profile_selector()->findData(id) >= 0;
				},
						"the installed agent is offered", std::chrono::seconds{ 60 });
				panel->profile_selector()->setCurrentIndex(panel->profile_selector()->findData(id));
				aWindow.settings()->hide();
				aWindow.choose_modality(qt::review_modality::review);
				show_tray(QStringLiteral("Agent"));
				auto* const tab    = aWindow.agent_tab();
				const auto  before = tab->turn_count();
				wait_for([&] { return tab->state() == qt::agent_tab_state::idle; }, "the Agent tab is ready to ask", std::chrono::seconds{ 120 });
				tab->send_prompt(QStringLiteral("In one sentence, what does this pull request change? Do not use any tools."));
				require([&] { return tab->turn_count() == before + 1U; }, "the turn starts", std::chrono::seconds{ 120 });
				require([&] { return tab->state() != qt::agent_tab_state::working && tab->turn_outcome(before).has_value(); }, "the agent answers", std::chrono::seconds{ 300 });
				record("agent", "\"pull\":" + std::to_string(pull) + ",\"outcome\":" + json_text(core::stable_name(*tab->turn_outcome(before))) + ",\"footer\":" + json_text(text(tab->turn_footer(before))) + ",\"answer_head\":" + json_text(text(tab->turn_answer(before).left(300))));
			}
		}

		// 2. The author's tour on --pull.
		ready(aOptions.Pull);
		++opened;
		auto* const editor = aWindow.tour_editor();
		require([&] { return aWindow.prepare_tour_action()->isEnabled() && !editor->shown().Loading; }, "Prepare review tour is offered", std::chrono::seconds{ 120 });
		aWindow.prepare_tour_action()->trigger();
		require([&] { return editor->shown().Draft != nullptr; }, "the tour draft opens", std::chrono::seconds{ 60 });
		const auto saved = [&] {
			require([&] { const auto& p = editor->shown(); return p.Draft && !p.Saving && p.SavedRevision == p.Draft->Revision; }, "the tour is saved");
		};
		const auto select_lines = [&](const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast) {
			auto* const surface = aWindow.comparison_surface();
			const auto  file    = core::file_identity_of(aPath);
			require([&] { return aWindow.files_drawer()->contents()->index_for_file(file).isValid(); }, aPath + " is listed");
			aWindow.files_drawer()->contents_view()->setCurrentIndex(aWindow.files_drawer()->contents()->index_for_file(file));
			require([&] { return surface->current_result() && surface->current_context() && surface->current_context()->Key.File == file && surface->current_semantic_index() && !surface->geometry_pending(); }, aPath + " is shown", std::chrono::seconds{ 120 });
			if (!surface->select_conversation_line(core::diff_side::right, aFirst)) {
				fail("line " + std::to_string(aFirst + 1U) + " of " + aPath + " cannot be selected");
			}
			auto* const pane = surface->right_pane();
			pane->setFocus();
			auto cursor = pane->textCursor();
			cursor.setPosition(pane->document()->findBlockByNumber(static_cast<int>(aFirst)).position());
			cursor.setPosition(pane->document()->findBlockByNumber(static_cast<int>(aLast)).position() + 1, QTextCursor::KeepAnchor);
			pane->setTextCursor(cursor);
			const auto range = surface->selected_source_range();
			if (!range || range->first.Line != aFirst || range->second.Line != aLast) {
				fail("the selection on " + aPath + " is not lines " + std::to_string(aFirst + 1U) + "-" + std::to_string(aLast + 1U));
			}
		};
		editor->problem_editor()->setPlainText(QStringLiteral("The retry delay doubled without a bound on the shift, and the second helper repeated it."));
		editor->solution_editor()->setPlainText(QStringLiteral("Bound the doubling in one place and keep the second helper's arithmetic beside it."));
		select_lines(aOptions.Path, aOptions.FirstStop, aOptions.FirstStop + 1U);
		aWindow.add_tour_stop_action()->trigger();
		editor->stop_title()->setPlainText(QStringLiteral("Bound the doubling"));
		editor->stop_body()->setPlainText(QStringLiteral("These lines carry the change; the rest of the file follows from them."));
		saved();
		select_lines(aOptions.Second, aOptions.SecondStop, aOptions.SecondStop + 1U);
		aWindow.add_tour_stop_action()->trigger();
		editor->stop_title()->setPlainText(QStringLiteral("The helper beside it"));
		editor->stop_body()->setPlainText(QStringLiteral("Read these lines with stop 1: they use the same bound."));
		saved();
		const auto stop_states = [&] {
			auto        result = std::string{ };
			const auto& draft  = editor->shown().Draft;
			for (const auto& stop : draft->Stops) {
				result += (result.empty() ? "" : ",") + std::string{ "{\"title\":" } + json_text(stop.Title) + ",\"accepted\":" + (core::tour_stop_accepted(*draft, stop) ? "true" : "false")
					+ ",\"need\":" + std::to_string(static_cast<int>(core::tour_stop_accept_need(*draft, stop))) + ",\"reason\":" + json_text(core::stable_name(core::decide_tour_stop_acceptance(*draft, stop).Reason)) + ",\"commit\":" + json_text(stop.Original ? std::string{ core::canonical_hex(stop.Original->Commit).view() } : std::string{ }) + "}";
			}
			return "[" + result + "]";
		};
		// The stop's own Accept, as the author presses it.
		const auto accept_all = [&] {
			const auto count = static_cast<int>(editor->shown().Draft->Stops.size());
			for (int row = 0; row < count; ++row) {
				editor->outline()->setCurrentRow(row);
				wait_for([] { return false; }, "the stop shows (expected)", std::chrono::seconds{ 1 });
				const auto& draft = editor->shown().Draft;
				const auto& stop  = draft->Stops[static_cast<std::size_t>(row)];
				if (core::tour_stop_accepted(*draft, stop)) {
					continue;
				}
				auto* const accept = editor->accept_button();
				record("accept", "\"stop\":" + std::to_string(row + 1) + ",\"enabled\":" + (accept->isEnabled() ? "true" : "false") + ",\"visible\":" + (accept->isVisible() ? "true" : "false") + ",\"text\":" + json_text(text(accept->text())) + ",\"reason\":" + json_text(text(editor->accept_reason()->text())));
				if (accept->isEnabled()) {
					accept->click();
					saved();
				}
			}
			record("stops", "\"stops\":" + stop_states());
		};
		accept_all();
		const auto head   = [&] { return std::string{ core::canonical_hex(editor->shown().Draft->ObservedHead.value_or(editor->shown().Draft->Head)).view() }; };
		const auto before = head();
		checkpoint(aOptions.Root, "push", "{\"head\":" + json_text(before) + "}");

		// 3. After the push, as the author refreshes.
		aWindow.github_refresh_action()->trigger();
		require([&] { return editor->shown().Draft && head() != before && !editor->shown().Loading; }, "the tour sees the new head", std::chrono::seconds{ 300 });
		// The window compares each stop's lines at the new head on its own;
		// Accept waits for that, as an author would.
		const auto compared = [&] {
			const auto& draft = editor->shown().Draft;
			return draft && std::ranges::none_of(draft->Stops, [&](const core::tour_stop& aStop) { return core::decide_tour_stop_acceptance(*draft, aStop).Reason == core::tour_acceptance_reason::unchecked; });
		};
		const auto checked = wait_for(compared, "the stops are compared at the new head", std::chrono::seconds{ 240 });
		record("compared", std::string{ "\"compared\":" } + (checked ? "true" : "false"));
		record("pushed", "\"head\":" + json_text(head()) + ",\"stops\":" + stop_states());
		accept_all();

		// 4. Prepare and Publish.
		aWindow.open_tour_page(true);
		const auto find   = [&](const char* aName) { return editor->findChild<QPushButton*>(QString::fromUtf8(aName)); };
		auto*      status = editor->findChild<QLabel*>(QStringLiteral("tourPublicationStatus"));
		auto*      form   = editor->findChild<QLabel*>(QStringLiteral("tourPublicationForm"));
		const auto panel  = [&] {
			auto repairs = std::string{ };
			for (auto* const reason : editor->findChildren<QLabel*>()) {
				if (reason->objectName().startsWith(QStringLiteral("tourRepairReason")) && reason->isVisibleTo(editor)) {
					repairs += (repairs.empty() ? "" : ",") + json_text(text(reason->accessibleName()));
				}
			}
			return "\"status\":" + json_text(text(status->text())) + ",\"form\":" + json_text(text(form->text())) + ",\"publish_enabled\":" + (find("tourPublish") && find("tourPublish")->isEnabled() ? "true" : "false")
				+ ",\"description\":" + (editor->description_publication() ? "true" : "false") + ",\"repairs\":[" + repairs + "]";
		};
		find("tourPreparePublication")->click();
		if (!wait_for([&] { return find("tourPublish")->isEnabled(); }, "Publish is offered", std::chrono::seconds{ 180 })) {
			record("not-publishable", panel() + ",\"stops\":" + stop_states());
			aWindow.grab().save(QString::fromStdString((aOptions.Root / "tour-not-publishable.png").string()));
			fail("Publish was never offered");
		}
		record("prepared", panel());
		// Both stops were written before the push. Stop 1's lines are the
		// same at the new head and inside its diff, so the author keeps the
		// tour to one review for it: Keep it to one review instead, then
		// Re-pin to current head on stop 1, and prepares again. Stop 2's lines
		// are no longer in the diff and stay at the earlier version.
		if (!form->text().contains(QStringLiteral("one review"))) {
			auto* const keep = editor->findChild<QPushButton*>(QStringLiteral("tourKeepOneReview"));
			if (keep != nullptr && keep->isVisible() && !keep->isChecked()) {
				keep->click();
			}
			const auto stop1  = editor->shown().Draft->Stops.front().Id;
			auto*      repin  = static_cast<QPushButton*>(nullptr);
			const auto offers = [&] {
				repin = editor->findChild<QPushButton*>(QStringLiteral("tourRepin%1").arg(stop1.value()));
				return repin != nullptr && repin->isVisibleTo(editor) && repin->isEnabled();
			};
			if (!wait_for(offers, "Re-pin to current head on stop 1", std::chrono::seconds{ 60 })) {
				record("no-repin", panel() + ",\"keep\":" + (keep != nullptr && keep->isVisible() ? "true" : "false"));
				aWindow.grab().save(QString::fromStdString((aOptions.Root / "tour-no-repin.png").string()));
				fail("stop 1 offers no re-pin");
			}
			repin->click();
			saved();
			if (keep != nullptr && keep->isChecked()) {
				keep->click();
			}
			record("repinned", "\"stops\":" + stop_states());
			accept_all();
			find("tourPreparePublication")->click();
			if (!wait_for([&] { return find("tourPublish")->isEnabled(); }, "Publish is offered again", std::chrono::seconds{ 180 })) {
				record("not-publishable", panel() + ",\"stops\":" + stop_states());
				fail("Publish was never offered after the re-pin");
			}
			record("prepared", panel());
		}
		aWindow.grab().save(QString::fromStdString((aOptions.Root / "tour-prepared.png").string()));
		auto*      steps    = editor->publication_steps();
		auto*      count    = editor->findChild<QLabel*>(QStringLiteral("tourPublicationStepCount"));
		auto       seen     = std::vector<std::string>{ };
		auto       rendered = 0;
		const auto watch    = [&] {
			const auto rows  = steps->row_texts().join(QStringLiteral(" | "));
			const auto shown = text(rows) + " # " + text(count->text()) + " # head=" + text(editor->head_bar()->state_label()->text());
			if (seen.empty() || seen.back() != shown) {
				seen.push_back(shown);
				record("step", "\"rows\":" + json_text(text(rows)) + ",\"count\":" + json_text(text(count->text())) + ",\"head\":" + json_text(text(editor->head_bar()->state_label()->text())));
				if (rendered < 6) {
					aWindow.grab().save(QString::fromStdString((aOptions.Root / ("tour-step-" + std::to_string(++rendered) + ".png")).string()));
				}
			}
			return status->text().startsWith(QStringLiteral("Published")) || (editor->publication_banner() != nullptr && editor->publication_banner()->isVisible());
		};
		watch();
		find("tourPublish")->click();
		require(watch, "the publication ends", std::chrono::seconds{ 300 });
		record("published", panel() + ",\"link\":" + json_text(text(editor->findChild<QLabel*>(QStringLiteral("tourPublicationLink"))->text())));
		aWindow.grab().save(QString::fromStdString((aOptions.Root / "tour-published.png").string()));
		if (!status->text().startsWith(QStringLiteral("Published"))) {
			fail("the tour did not publish");
		}

		// 5. Approve, as the reviewer presses it. A window with a published
		// tour opens the next pull request in a window of its own.
		if (aOptions.Approve > 0) {
			// Back to the code, then Home, as a person opens the next one. A
			// window still reading GitHub's answer to the publication may not
			// take the open at once; the person tries again.
			auto*      approving = static_cast<qt::workbench_window*>(nullptr);
			const auto showing   = [&] {
				for (auto* const widget : QApplication::topLevelWidgets()) {
					auto* const candidate = dynamic_cast<qt::workbench_window*>(widget);
					if (candidate != nullptr && candidate->github_status_text() == "Pull request ready." && candidate->overview_page()->projection().has_value()
						&& static_cast<int>(candidate->overview_page()->projection()->Number.value()) == aOptions.Approve) {
						approving = candidate;
						return true;
					}
				}
				return false;
			};
			auto attempts = 0;
			while (!showing() && attempts < 8) {
				++attempts;
				if (aWindow.tour_page_open()) {
					aWindow.close_tour_page();
				}
				aWindow.choose_home(true);
				wait_for([&] { return aWindow.showing_home(); }, "Home shows", std::chrono::seconds{ 10 });
				auto* const modal = QApplication::activeModalWidget();
				say("approve-open:attempt=" + std::to_string(attempts) + " home=" + (aWindow.showing_home() ? "yes" : "no") + " modal=" + (modal != nullptr ? text(modal->windowTitle()) : std::string{ "none" }) + " status=" + aWindow.github_status_text());
				open_pull(aWindow, aOptions.Approve);
				wait_for(showing, "#" + std::to_string(aOptions.Approve) + " is ready", std::chrono::seconds{ 45 });
			}
			++opened;
			if (approving == nullptr) {
				aWindow.grab().save(QString::fromStdString((aOptions.Root / ("not-ready-" + std::to_string(aOptions.Approve) + ".png")).string()));
				fail("#" + std::to_string(aOptions.Approve) + " never became ready");
			}
			record("approve-window", std::string{ "\"same_window\":" } + (approving == &aWindow ? "true" : "false"));
			approving->resize(1400, 900);
			approving->choose_modality(qt::review_modality::review);
			approving->open_review_composition();
			auto* const review  = approving->review_submission_editor_view();
			auto* const approve = review->findChild<QToolButton*>(QStringLiteral("wholeReviewApprove"));
			require([&] { return approve != nullptr && approve->isEnabled(); }, "Approve is offered", std::chrono::seconds{ 180 });
			const auto page = [&] {
				const auto& draft = review->shown_draft();
				const auto  state = !draft                                      ? std::string{ "none" }
					: draft->State == core::review_submission_state::drafting   ? std::string{ "drafting" }
					: draft->State == core::review_submission_state::submitting ? std::string{ "submitting" }
					: draft->State == core::review_submission_state::submitted  ? std::string{ "submitted" }
																				: std::string{ "discarded" };
				return "\"draft\":" + json_text(state) + ",\"status\":" + json_text(review->status_label()->isVisible() ? text(review->status_label()->text()) : std::string{ }) + ",\"waiting\":"
					+ json_text(review->waiting_panel()->isVisible() ? text(review->waiting_title_label()->text()) : std::string{ }) + ",\"window\":" + json_text(text(approving->statusBar()->currentMessage()))
					+ ",\"confirmed\":" + std::to_string(review->confirmed_submissions());
			};
			const auto confirmedBefore = review->confirmed_submissions();
			auto       pages           = std::vector<std::string>{ };
			const auto settled         = [&] {
				const auto shown = page();
				if (pages.empty() || pages.back() != shown) {
					pages.push_back(shown);
					record("approve", shown);
				}
				// GitHub's confirmation closes the page and the window says
				// "Review submitted"; a refusal keeps it with "Not published".
				return review->confirmed_submissions() > confirmedBefore || review->status_label()->text().startsWith(QStringLiteral("Not published"));
			};
			settled();
			approve->click();
			require(settled, "the approval ends", std::chrono::seconds{ 180 });
			record("approved", page());
			approving->grab().save(QString::fromStdString((aOptions.Root / "approve-ended.png").string()));
		}
		record("session", "\"pull_requests\":" + std::to_string(opened));
		dump(aWindow);
	}

}

auto main(int argc, char** argv) -> int
try {
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto chosen = parse(QCoreApplication::arguments());
	std::filesystem::create_directories(chosen.Root / "data");
	std::filesystem::permissions(chosen.Root / "data", std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);

	// A question the application asks is a person's gesture. The one this
	// course meets is the GitHub CLI read consent; the course answers Yes and
	// logs the question word for word, so what was agreed to is on record.
	QTimer answer;
	answer.setInterval(100);
	QObject::connect(&answer, &QTimer::timeout, [] {
		auto* const box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (box == nullptr || !box->isVisible()) {
			return;
		}
		say("question:title=" + text(box->windowTitle()) + "\nquestion:text=" + text(box->text()) + "\nquestion:informative=" + text(box->informativeText()));
		auto* yes = box->button(QMessageBox::Yes);
		if (yes == nullptr) {
			// A write confirmation names its own action (Publish, Submit):
			// the person's answer is the box's accepting button.
			for (auto* const button : box->buttons()) {
				if (box->buttonRole(button) == QMessageBox::AcceptRole || box->buttonRole(button) == QMessageBox::YesRole) {
					yes = qobject_cast<QPushButton*>(button);
					break;
				}
			}
		}
		if (yes == nullptr) {
			say("question:no-accepting-button; left open");
			return;
		}
		say("question:answer=" + text(yes->text()));
		yes->click();
	});
	answer.start();

	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::profile_desktop::start(app::github_application_config{ .ApplicationDataRoot = chosen.Root / "data", .DesignatedLivePublication = chosen.Course == "live-session" }, [&](app::profile_desktop_startup_result aResult) {
		terminal = true;
		if (aResult) {
			owner = *aResult;
		} else {
			say("startup-failure:" + std::to_string(static_cast<int>(aResult.error())));
		}
	});
	require([&] { return terminal; }, "the profile owner starts", std::chrono::seconds{ 120 });
	if (owner == nullptr) {
		fail("the shipped composition did not start");
	}
	owner->show();
	require([&] { return !owner->windows().empty(); }, "the first window", std::chrono::seconds{ 60 });
	auto* const window = owner->window(owner->windows().front());
	const auto  login  = sign_in(*window);
	say("login:" + login);
	if (chosen.Course == "observer") {
		observer_course(chosen, *window);
	} else if (chosen.Course == "epoch") {
		epoch_course(chosen, *window);
	} else if (chosen.Course == "parity") {
		parity_course(chosen, *window);
	} else if (chosen.Course == "commit-filter") {
		commit_filter_course(chosen, *window);
	} else if (chosen.Course == "generated") {
		generated_course(chosen, *window);
	} else if (chosen.Course == "force-push") {
		force_push_course(chosen, *window);
	} else if (chosen.Course == "filtered-comment") {
		filtered_comment_course(chosen, *window);
	} else if (chosen.Course == "whole-pr-comment") {
		whole_pr_comment_course(chosen, *window);
	} else if (chosen.Course == "agent-helpers") {
		agent_helpers_course(chosen, *window);
	} else if (chosen.Course == "agent-scratch") {
		agent_scratch_course(chosen, *window);
	} else if (chosen.Course == "live-session") {
		live_session_course(chosen, *window);
	} else {
		reader_course(chosen, *window);
	}
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "shutdown", std::chrono::seconds{ 60 });
	say("live-second-account-course:" + chosen.Course + ":pass");
	return EXIT_SUCCESS;
} catch (const std::exception& error) {
	std::cout << "live-second-account-course:fail " << error.what() << '\n'
			  << std::flush;
	return EXIT_FAILURE;
}
