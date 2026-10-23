/// \file
/// \brief [[BPR-PENDING-AQ6]], RFC-BPR-0092. The Agent tab's setup and its
/// Settings rows, offscreen: each state's words and actions from the two
/// projections, the setup showing only once a step is known to be missing,
/// the ready line once both checks pass, every press an intent, and the
/// Settings rows for programs, clone folders and the review skill. With
/// `BPR_SETUP_RENDER_DIR` set, each state is also written there as a light and
/// a dark PNG for review.

#include "bpr/qt/agent_entry_dialog.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/folder_list_editor.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStyleFactory>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto dark_palette() -> QPalette
	{
		auto palette = QPalette{ };
		palette.setColor(QPalette::Window, QColor{ 0x1F, 0x23, 0x28 });
		palette.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Base, QColor{ 0x16, 0x1B, 0x22 });
		palette.setColor(QPalette::Text, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Button, QColor{ 0x2D, 0x33, 0x3B });
		palette.setColor(QPalette::ButtonText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Highlight, QColor{ 0x2F, 0x81, 0xF7 });
		palette.setColor(QPalette::HighlightedText, QColor{ 0xFF, 0xFF, 0xFF });
		palette.setColor(QPalette::PlaceholderText, QColor{ 0x8B, 0x94, 0x9E });
		palette.setColor(QPalette::Link, QColor{ 0x58, 0xA6, 0xFF });
		return palette;
	}

	void render(QWidget& aWidget, const QString& aName)
	{
		const auto folder = qEnvironmentVariable("BPR_SETUP_RENDER_DIR");
		if (folder.isEmpty()) {
			return;
		}
		QDir{ }.mkpath(folder);
		const auto light = aWidget.palette();
		for (const auto& [suffix, palette] : { std::pair{ QStringLiteral("light"), light }, std::pair{ QStringLiteral("dark"), dark_palette() } }) {
			aWidget.setPalette(palette);
			aWidget.resize(420, 760);
			aWidget.show();
			QApplication::processEvents();
			static_cast<void>(aWidget.grab().save(folder + QStringLiteral("/") + aName + QStringLiteral("-") + suffix + QStringLiteral(".png")));
		}
		aWidget.setPalette(light);
	}

	void click(QWidget& aWidget)
	{
		const auto centre  = QPointF{ aWidget.rect().center() };
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, centre, aWidget.mapToGlobal(centre), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, centre, aWidget.mapToGlobal(centre), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
		QApplication::processEvents();
	}

	/// New folder, then a path typed into the row's own field and Return.
	[[nodiscard]] auto type_row(qt::folder_list_editor& aEditor, const QString& aText) -> bool
	{
		aEditor.new_button()->click();
		QApplication::processEvents();
		auto* const field = aEditor.list()->findChild<QLineEdit*>(QStringLiteral("folderRowPath"));
		if (field == nullptr || aEditor.list()->findChild<QToolButton*>(QStringLiteral("folderRowBrowse")) == nullptr) {
			return false;
		}
		field->setText(aText);
		auto press = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(field, &press);
		QApplication::processEvents();
		QApplication::processEvents();
		return true;
	}

	[[nodiscard]] auto row(const core::setup_agent aAgent, std::string aPath, const core::agent_found_how aHow, const bool aPermitted = true) -> core::agent_setup_row
	{
		auto value              = core::agent_setup_row{ };
		value.Agent             = aAgent;
		value.Path              = std::move(aPath);
		value.How               = aHow;
		value.Verdict.Permitted = aPermitted;
		return value;
	}

	struct fixture
	{
		qt::agent_setup_view                View;
		application::agent_setup_projection Setup;
		application::clone_link_projection  Clones;
		qt::agent_setup_facts               Facts{ .Repository = "bpr-fixtures/engine", .Head = "7f3a9c1" };
		std::vector<std::string>            Intents;

		fixture()
		{
			Setup.Detected    = true;
			Setup.Searched    = { core::setup_agent::claude_code, core::setup_agent::codex };
			Clones.Loaded     = true;
			Clones.Repository = application::clone_repository{ .Host = "github.com", .Repository = 7U, .Owner = "bpr-fixtures", .Name = "engine" };
			View.set_intents(qt::agent_setup_intents{
				.Choose                 = [this](const core::setup_agent aAgent) { Intents.push_back("choose:" + std::string{ core::stable_name(aAgent) }); },
				.LookAgain              = [this] { Intents.emplace_back("look-again"); },
				.OpenSettings           = [this] { Intents.emplace_back("settings"); },
				.OpenRepositorySettings = [this] { Intents.emplace_back("repository-settings"); },
				.OpenInstallPage        = [this](const core::setup_agent aAgent) { Intents.push_back("install:" + std::string{ core::stable_name(aAgent) }); },
				.ChooseClone            = [this](std::string aPath) { Intents.push_back("clone:" + aPath); },
				.AddCloneFolder         = [this](std::string aFolder) { Intents.push_back("folder:" + aFolder); },
				.SetCloneFolders =
					[this](std::vector<std::string> aFolders) {
						auto joined = std::string{ "folders:" };
						for (const auto& folder : aFolders) {
							joined += (joined.size() > 8U ? "|" : "") + folder;
						}
						Intents.push_back(std::move(joined));
					},
				.UsePrivateCopy = [this] { Intents.emplace_back("private-copy"); },
				.Check          = [this] { Intents.emplace_back("check"); },
				.Stop           = [this] { Intents.emplace_back("stop"); },
				.AddCustomAgent = [this](core::agent_entry aEntry) { Intents.push_back("custom:" + aEntry.Name + ":" + aEntry.Program + (aEntry.Arguments.empty() ? std::string{ } : ":" + aEntry.Arguments.front())); },
			});
		}

		void show()
		{
			Setup.Step = core::current_setup_step(Setup.State);
			View.present(Setup, Clones, Facts);
			QApplication::processEvents();
		}

		[[nodiscard]] auto button(const QString& aName) const -> QPushButton* { return View.findChild<QPushButton*>(aName); }
		[[nodiscard]] auto text() const -> QString { return View.step_text(); }
	};

	void test_setup_waits_until_a_step_is_known_missing()
	{
		auto test           = fixture{ };
		test.Setup.Detected = false;
		test.show();
		expect(!test.View.needed(), "before the agents are looked for, the setup does not take the tab");
		test.Setup.Detected   = true;
		test.Clones.Verifying = true;
		test.show();
		expect(!test.View.needed(), "nor while the repository's link is being checked again");
		test.Clones.Verifying             = false;
		test.Setup.State.Rows             = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path) };
		test.Setup.State.Chosen           = core::setup_agent::claude_code;
		test.Clones.Linked                = application::repository_link{ .Host = "github.com", .Repository = 7U, .Owner = "bpr-fixtures", .Name = "engine", .Path = "/Users/me/src/engine", .Remote = "origin" };
		test.Setup.State.RepositoryChosen = true;
		test.show();
		expect(!test.View.needed(), "a saved agent and a serving link open on the conversation: no setup for a new window");
	}

	void test_no_agent_found()
	{
		auto test = fixture{ };
		test.show();
		expect(test.View.needed() && test.View.open_step() == core::setup_step::agent, "with no agent the setup shows its Agent step");
		expect(test.text().contains(QStringLiteral("No agent found on this computer")) && test.text().contains(QStringLiteral("Claude Code and Codex")), "it says none was found and what it looked for");
		test.button(QStringLiteral("agentSetupInstallClaude"))->click();
		test.button(QStringLiteral("agentSetupInstallCodex"))->click();
		// Owner amendment 2026-10-23: Custom agent… in setup describes a
		// wrapper of the reviewer's own and adds it.
		test.View.set_entry_editor([](const core::agent_entry&, QWidget*) -> std::optional<core::agent_entry> {
			return core::agent_entry{ .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } };
		});
		test.button(QStringLiteral("agentSetupCustomAgent"))->click();
		expect(test.Intents == std::vector<std::string>{ "install:claude_code", "install:codex", "custom:Work Codex:/opt/work/bin/wrapper:--codex" }, "Install and Custom agent… are intents");
		render(test.View, QStringLiteral("none"));
	}

	void test_two_agents_and_a_refused_one()
	{
		auto test             = fixture{ };
		test.Setup.State.Rows = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path),
								  row(core::setup_agent::codex, "/Users/me/.local/bin/codex", core::agent_found_how::usual_folder, false) };
		test.show();
		const auto choices = test.View.findChildren<QRadioButton*>(QStringLiteral("agentSetupAgentChoice"));
		expect(choices.size() == 2 && choices[0]->isEnabled() && !choices[1]->isEnabled() && !choices[0]->isChecked(), "both rows show, the refused one disabled, none chosen yet");
		expect(test.text().contains(QStringLiteral("Found on PATH.")) && test.text().contains(QStringLiteral("Not allowed for this pull request's host.")), "each row says how it was found or why it is refused");
		// Owner look v002: the whole card chooses, by mouse or keyboard; the
		// radio only shows it.
		const auto cards = test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow"));
		expect(cards.size() == 2 && cards[0]->focusPolicy() == Qt::StrongFocus && cards[1]->focusPolicy() == Qt::NoFocus, "a permitted card takes the keyboard focus, a refused one does not");
		click(*cards[1]);
		expect(test.Intents.empty(), "a refused card chooses nothing");
		click(*cards[0]);
		expect(test.Intents == std::vector<std::string>{ "choose:claude_code" }, "clicking anywhere on the card chooses it");
		auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
		QApplication::sendEvent(test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow"))[0], &key);
		expect(test.Intents.size() == 2U && test.Intents.back() == "choose:claude_code", "and Space on a focused card does too");
		render(test.View, QStringLiteral("two"));
	}

	void test_change_keeps_the_current_agent()
	{
		auto test                         = fixture{ };
		test.Setup.State.Rows             = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path),
											  row(core::setup_agent::codex, "/Users/me/.local/bin/codex", core::agent_found_how::on_path) };
		test.Setup.State.Chosen           = core::setup_agent::claude_code;
		test.Setup.State.RepositoryChosen = false;
		test.show();
		expect(test.View.open_step() == core::setup_step::repository, "with an agent chosen, the setup is at Repository");
		test.button(QStringLiteral("agentSetupChange"))->click();
		expect(test.View.open_step() == core::setup_step::agent && test.button(QStringLiteral("agentSetupContinue")) != nullptr
				   && test.button(QStringLiteral("agentSetupContinue"))->text() == QStringLiteral("Continue with Claude Code"),
			   "Change reopens Agent with Continue with the agent already chosen");
		test.button(QStringLiteral("agentSetupContinue"))->click();
		expect(test.View.open_step() == core::setup_step::repository && test.Intents.empty(), "Continue keeps it and goes back to Repository, changing nothing");
		test.button(QStringLiteral("agentSetupChange"))->click();
		click(*test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow"))[0]);
		expect(test.View.open_step() == core::setup_step::repository && test.Intents.empty(), "choosing the agent already chosen keeps it too");
		test.button(QStringLiteral("agentSetupChange"))->click();
		auto* const second = test.View.findChildren<QWidget*>(QStringLiteral("agentSetupStepHeader"))[1];
		click(*second);
		expect(test.View.open_step() == core::setup_step::repository, "and the Repository step's own header goes on to it");
		render(test.View, QStringLiteral("change"));
	}

	void test_repository_states()
	{
		auto test               = fixture{ };
		test.Setup.State.Rows   = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path) };
		test.Setup.State.Chosen = core::setup_agent::claude_code;
		test.Clones.Suggestions = { "/Users/me/Documents/GitHub" };
		test.show();
		expect(test.View.open_step() == core::setup_step::repository && test.text().contains(QStringLiteral("Where do your clones live?")), "with no clone folder it asks where clones live");
		test.button(QStringLiteral("agentSetupSuggestedFolder"))->click();
		expect(test.Intents.back() == "folder:/Users/me/Documents/GitHub", "a suggested folder is added in one press");
		// Owner look v002: Visual Studio's folder list, edited in place.
		auto* const list = dynamic_cast<qt::folder_list_editor*>(test.View.findChild<QWidget*>(QStringLiteral("agentSetupAddFolder")));
		expect(list != nullptr && list->new_button()->isEnabled() && !list->remove_button()->isEnabled() && !list->up_button()->isEnabled(),
			   "no folder yet: New folder is offered, Remove and the moves wait for a row");
		if (list == nullptr) {
			return;
		}
		const auto typed = type_row(*list, QStringLiteral("work/clones"));
		expect(typed && test.Intents.back() == "folder:/Users/me/Documents/GitHub" && list->status()->text().contains(QStringLiteral("full path"))
				   && list->list()->count() == 0,
			   "a relative path is refused with what to write, and its new row goes");
		expect(type_row(*list, QStringLiteral("~/src")) && test.Intents.back() == "folders:" + QDir::toNativeSeparators(QDir::homePath() + QStringLiteral("/src")).toStdString(),
			   "~ is the home folder, and the new list is the intent");
		render(test.View, QStringLiteral("nofolders"));

		test.Clones.Folders    = { "/Users/me/Documents/GitHub" };
		test.Clones.Discovered = true;
		test.show();
		expect(test.text().contains(QStringLiteral("No clone of bpr-fixtures/engine in your clone folders")), "none found says so");
		test.button(QStringLiteral("agentSetupPrivateCopy"))->click();
		expect(test.Intents.back() == "private-copy", "the private copy is one press away");
		render(test.View, QStringLiteral("noclone"));

		test.Clones.Matches = { core::clone_match{ .Path = "/Users/me/Documents/GitHub/engine", .Folder = "/Users/me/Documents/GitHub", .Remote = "origin", .Location = "github.com/bpr-fixtures/engine", .Branch = "main" },
								core::clone_match{ .Path = "/Users/me/src/engine-old", .Folder = "/Users/me/src", .Remote = "upstream", .Location = "github.com/bpr-fixtures/engine", .Branch = "spike/retry" } };
		test.show();
		auto clones = test.View.findChildren<QRadioButton*>(QStringLiteral("agentSetupCloneChoice"));
		expect(clones.size() == 2 && !test.button(QStringLiteral("agentSetupUseClone"))->isEnabled(), "two clones: none is chosen until the reviewer picks one");
		click(*test.View.findChildren<QWidget*>(QStringLiteral("agentSetupCloneRow"))[0]);
		QApplication::processEvents();
		test.button(QStringLiteral("agentSetupUseClone"))->click();
		expect(test.Intents.back() == "clone:/Users/me/Documents/GitHub/engine", "Use this clone links the chosen one");
		render(test.View, QStringLiteral("repo"));
	}

	void test_check_and_ready()
	{
		auto test               = fixture{ };
		test.Setup.State.Rows   = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path) };
		test.Setup.State.Chosen = core::setup_agent::claude_code;
		test.show();
		expect(test.View.needed(), "the setup took the tab at the Repository step");
		test.Clones.Linked                = application::repository_link{ .Host = "github.com", .Repository = 7U, .Owner = "bpr-fixtures", .Name = "engine", .Path = "/Users/me/Documents/GitHub/engine", .Remote = "origin" };
		test.Setup.State.RepositoryChosen = true;
		test.show();
		expect(test.View.needed() && test.View.open_step() == core::setup_step::check, "once begun, the setup stays until it is checked");
		expect(test.text().contains(QStringLiteral("claude auth status")) && test.text().contains(QStringLiteral("Claude Code is signed in")) && test.text().contains(QStringLiteral("at 7f3a9c1")),
			   "Check it works names the command it runs and its two rows");
		test.button(QStringLiteral("agentSetupCheck"))->click();
		expect(test.Intents.back() == "check", "Check is an intent");
		render(test.View, QStringLiteral("check"));

		test.Setup.State.AgentCheck     = core::fail_check(core::agent_setup_failure::not_signed_in);
		test.Setup.State.WorkspaceCheck = core::fail_check(core::agent_setup_failure::workspace_unavailable);
		test.Facts.WorkspaceReason      = application::agent_workspace_unavailable_reason::connection_failed;
		test.show();
		expect(test.text().contains(QStringLiteral("Claude Code isn't signed in")) && test.text().contains(QStringLiteral("claude auth login")) && test.text().contains(QStringLiteral("GitHub could not be reached"))
				   && test.button(QStringLiteral("agentSetupCheck"))->text() == QStringLiteral("Check again"),
			   "each failed row says what failed and what to do, with Check again");
		expect(!test.text().contains(QStringLiteral("not_signed_in")) && !test.text().contains(QStringLiteral("workspace_unavailable")), "no stable code is shown");
		render(test.View, QStringLiteral("signin"));

		test.Setup.Checking = true;
		test.show();
		test.button(QStringLiteral("agentSetupStop"))->click();
		expect(test.Intents.back() == "stop", "a running check can be stopped");
		test.Setup.Checking             = false;
		test.Setup.State.AgentCheck     = { .State = core::setup_check_state::passed, .Version = "2.1.296" };
		test.Setup.State.WorkspaceCheck = { .State = core::setup_check_state::passed };
		test.show();
		expect(!test.View.needed() && test.View.take_ready() && !test.View.take_ready(), "both checks passed: the setup gives way, once, to the ready line");
		expect(test.View.ready_title() == QStringLiteral("Claude Code is ready") && test.View.ready_text().contains(QStringLiteral("version 2.1.296")), "the ready line names the agent and version");
		const auto gets = qt::agent_ready_gets({ .Repository = "bpr-fixtures/engine", .Number = 22U, .Title = "Clamp device scale", .BaseBranch = "main", .Base = "51d2dad", .Head = "7f3a9c1", .Files = 18U, .Threads = 6U, .Unresolved = 2U, .Clone = "/Users/me/Documents/GitHub/engine" });
		expect(gets.contains(QStringLiteral("bpr-fixtures/engine #22")) && gets.contains(QStringLiteral("git diff 51d2dad...7f3a9c1")) && gets.contains(QStringLiteral("6 review threads, 2 unresolved")),
			   "What the agent gets states the brief");

		test.View.begin(core::setup_step::repository);
		expect(test.View.needed() && test.View.open_step() == core::setup_step::repository, "Change repository folder… opens that step again");
	}

	void test_settings_rows()
	{
		// [[BPR-PENDING-AQ6]], owner amendment 2026-10-23: Settings › Agents
		// is a list with Add agent…, Edit…, Remove and Move up and down.
		auto programs = qt::agent_programs_panel{ };
		auto saved    = std::vector<std::string>{ };
		programs.set_dispatch({ .SetUp = [&saved] { saved.emplace_back("set-up"); },
								.Add =
									[&saved](core::agent_entry aEntry, qt::agent_programs_panel::answer aAnswer) {
										saved.push_back("add:" + aEntry.Name + (aEntry.Preset ? ":preset" : ":custom"));
										aAnswer(std::nullopt);
									},
								.Update =
									[&saved](core::agent_entry aEntry, qt::agent_programs_panel::answer aAnswer) {
										saved.push_back("update:" + std::to_string(aEntry.Id) + ":" + aEntry.Name);
										aAnswer(core::agent_entry_refusal::duplicate_name);
									},
								.Remove = [&saved](const std::uint32_t aId) { saved.push_back("remove:" + std::to_string(aId)); },
								.Move   = [&saved](const std::uint32_t aId, const int aDelta) { saved.push_back("move:" + std::to_string(aId) + ":" + std::to_string(aDelta)); } });
		auto preferences   = application::agent_setup_preferences{ };
		preferences.Agents = *core::add_agent_entry({ }, { .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/wrapper" });
		preferences.Agents = *core::add_agent_entry(preferences.Agents, { .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } });
		programs.present(preferences, { { .Agent = core::setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude" } });
		expect(programs.agent_list()->count() == 2 && programs.agent_list()->item(1)->text().contains(QStringLiteral("Work Codex")) && programs.agent_list()->item(1)->text().endsWith(QStringLiteral("--codex"))
				   && saved.empty(),
			   "each agent shows with its program and fixed arguments, and showing sends nothing");
		programs.add_button()->menu()->aboutToShow();
		const auto offered = programs.add_button()->menu()->actions();
		expect(std::ranges::any_of(offered, [](const QAction* aItem) { return aItem->objectName() == QStringLiteral("settingsAddClaude"); })
				   && std::ranges::none_of(offered, [](const QAction* aItem) { return aItem->objectName() == QStringLiteral("settingsAddCodex"); })
				   && std::ranges::any_of(offered, [](const QAction* aItem) { return aItem->objectName() == QStringLiteral("settingsAddCustom"); }),
			   "Add agent… offers Claude Code, found and not added, and a custom agent; Codex was not found");
		programs.set_editor([](const core::agent_entry& aStart, QWidget*) -> std::optional<core::agent_entry> {
			auto entry = aStart;
			if (entry.Name.empty()) {
				entry = { .Name = "Harness", .Protocol = core::agent_protocol::external_terminal, .Program = "/usr/local/bin/harness" };
			} else if (entry.Id != 0U) {
				entry.Name = "Work Claude";
			}
			return entry;
		});
		programs.add_agent(core::setup_agent::claude_code);
		programs.add_agent(std::nullopt);
		programs.agent_list()->setCurrentRow(1);
		programs.edit_button()->click();
		expect(programs.status()->text().contains(QStringLiteral("already has that name")), "a refused edit says why");
		programs.up_button()->click();
		programs.remove_button()->click();
		programs.findChild<QPushButton*>(QStringLiteral("settingsSetUpAgent"))->click();
		expect(saved == std::vector<std::string>{ "add:Claude Code:preset", "add:Harness:custom", "update:2:Work Claude", "move:2:-1", "remove:2", "set-up" },
			   "add, edit, move, remove and Set up agent… are intents");
		// The dialog: OK waits for an entry that may be kept, and the
		// arguments are taken one per line, as written.
		auto dialog = qt::agent_entry_dialog{ core::agent_entry{ } };
		expect(!dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled() && dialog.problem()->isVisibleTo(&dialog), "an empty agent cannot be kept");
		dialog.name()->setText(QStringLiteral("Work Codex"));
		dialog.protocol()->setCurrentIndex(dialog.protocol()->findData(static_cast<int>(core::agent_protocol::codex_jsonl_exec)));
		dialog.program()->setText(QStringLiteral("wrapper"));
		expect(!dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled() && dialog.problem()->text().contains(QStringLiteral("full path")), "a relative program is refused with why");
		dialog.program()->setText(QStringLiteral("/opt/work/bin/wrapper"));
		dialog.arguments()->setPlainText(QStringLiteral("--codex\n--profile work\n"));
		dialog.effort()->setCurrentIndex(dialog.effort()->findData(QStringLiteral("minimal")));
		const auto described = dialog.entry();
		expect(dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled() && described.Arguments == std::vector<std::string>{ "--codex", "--profile work" } && described.Effort == "minimal",
			   "a full entry may be kept, each line one argument, and Codex's efforts offered");

		auto folders = qt::clone_folders_panel{ };
		auto asked   = std::vector<std::string>{ };
		folders.set_dispatch({ .SetFolders =
								   [&asked](std::vector<std::string> aFolders) {
									   auto joined = std::string{ "set" };
									   for (const auto& folder : aFolders) {
										   joined += " " + folder;
									   }
									   asked.push_back(std::move(joined));
								   },
							   .Forget = [&asked](const application::repository_link& aLink) { asked.push_back("forget " + aLink.Name); } });
		auto clones    = application::clone_link_projection{ };
		clones.Folders = { "/Users/me/src", "/Users/me/Documents/GitHub" };
		clones.Links   = { application::repository_link{ .Host = "github.com", .Repository = 7U, .Owner = "bpr-fixtures", .Name = "engine", .PrivateCopy = true } };
		folders.present(clones);
		auto* const editor = folders.folders();
		expect(editor->list()->count() == 2 && folders.links()->count() == 1 && folders.links()->item(0)->text().contains(QStringLiteral("a private copy")), "folders and links are listed");
		editor->list()->setCurrentRow(1);
		editor->up_button()->click();
		expect(asked.back() == "set /Users/me/Documents/GitHub /Users/me/src", "Move up reorders the list, which is the order clones are looked for in");
		editor->list()->setCurrentRow(1);
		editor->remove_button()->click();
		expect(asked.back() == "set /Users/me/Documents/GitHub", "Remove takes the selected folder out");
		expect(type_row(*editor, QStringLiteral("/Volumes/work")) && asked.back() == "set /Users/me/Documents/GitHub " + QDir::toNativeSeparators(QStringLiteral("/Volumes/work")).toStdString(),
			   "New folder adds a typed row at the end");
		folders.links()->setCurrentRow(0);
		folders.findChild<QPushButton*>(QStringLiteral("settingsForgetLink"))->click();
		expect(asked.back() == "forget engine", "Forget names the link it acts on");

		auto skill   = qt::review_skill_panel{ };
		auto pressed = std::vector<std::string>{ };
		skill.set_dispatch({ .Save       = [&pressed](std::string aText) { pressed.push_back("save " + aText); },
							 .UseBuiltIn = [&pressed](const bool aCopy) { pressed.emplace_back(aCopy ? "built-in with copy" : "built-in"); },
							 .KeepMine   = [&pressed] { pressed.emplace_back("keep"); } });
		auto view            = application::review_skill_projection{ };
		view.Source          = core::review_skill_source::profile_copy;
		view.Text            = "---\nname: bpr-pr-review\n---\nmine";
		view.BuiltInVersion  = 2U;
		view.CopyBaseVersion = 1U;
		view.Offer           = 2U;
		view.Generation      = 1U;
		skill.present(view, "built-in text");
		expect(skill.editor()->toPlainText() == QString::fromStdString(view.Text) && !skill.offer()->isHidden() && skill.findChild<QPushButton*>(QStringLiteral("reviewSkillUseVersion"))->text() == QStringLiteral("Use version 2"),
			   "the copy shows with the newer version's offer");
		skill.findChild<QPushButton*>(QStringLiteral("reviewSkillKeepMine"))->click();
		skill.findChild<QPushButton*>(QStringLiteral("reviewSkillUseVersion"))->click();
		skill.findChild<QCheckBox*>(QStringLiteral("reviewSkillKeepCopy"))->setChecked(false);
		skill.findChild<QPushButton*>(QStringLiteral("reviewSkillRestore"))->click();
		skill.editor()->setPlainText(QStringLiteral("edited"));
		skill.findChild<QPushButton*>(QStringLiteral("reviewSkillSave"))->click();
		expect(pressed == std::vector<std::string>{ "keep", "built-in with copy", "built-in", "save edited" }, "Keep mine, Use version N, Restore default and Save are intents");
		view.Refusal    = core::review_skill_refusal::name_changed;
		view.Generation = 2U;
		skill.present(view, "built-in text");
		expect(skill.editor()->toPlainText() == QStringLiteral("edited") && skill.status_text().contains(QStringLiteral("must stay bpr-pr-review")), "a refused save keeps the text and says why");
		skill.show_answer(application::review_skill_answer::copy_failed);
		expect(skill.status_text().contains(QStringLiteral("nothing was removed")), "a failed Downloads copy says nothing was removed");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
	test_setup_waits_until_a_step_is_known_missing();
	test_no_agent_found();
	test_two_agents_and_a_refused_one();
	test_change_keeps_the_current_agent();
	test_repository_states();
	test_check_and_ready();
	test_settings_rows();
	std::cout << "agent-setup-view-tests:pass\n";
	return EXIT_SUCCESS;
}
