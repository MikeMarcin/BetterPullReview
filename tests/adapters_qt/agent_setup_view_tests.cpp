/// \file
/// \brief [[BPR-1753]], RFC-BPR-0092. The Agent tab's setup and its
/// Settings rows, offscreen: each state's words and actions from the two
/// projections, the setup showing only once a step is known to be missing,
/// the ready line once both checks pass, every press an intent, and the
/// Settings rows for programs, clone folders and the review skill. With
/// `BPR_SETUP_RENDER_DIR` set, each state is also written there as a light and
/// a dark PNG for review.

#include "bpr/adapters/workspace/app_bundle_read.hpp"
#include "bpr/qt/agent_entry_dialog.hpp"
#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/argument_list_editor.hpp"
#include "bpr/qt/elapsed_words_label.hpp"
#include "bpr/qt/folder_list_editor.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
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

	/// One Agent step card: an entry when `aEntry` is not 0, else a found agent.
	[[nodiscard]] auto card(const std::uint32_t aEntry, const core::setup_agent aAgent, std::string aName, std::string aPath, const std::optional<core::agent_found_how> aHow,
							const bool aPermitted = true, const bool aChosen = false) -> core::agent_setup_choice
	{
		auto value              = core::agent_setup_choice{ .Entry = aEntry, .Agent = aAgent, .Name = std::move(aName), .Path = std::move(aPath), .How = aHow };
		value.Verdict.Permitted = aPermitted;
		value.Chosen            = aChosen;
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
				.ChooseEntry            = [this](const std::uint32_t aId) { Intents.push_back("entry:" + std::to_string(aId)); },
				.FindAgents             = [this] { Intents.emplace_back("find-agents"); },
				.FindClones             = [this] { Intents.emplace_back("find-clones"); },
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
				.KeepClone      = [this] { Intents.emplace_back("keep"); },
				.CancelKeeping  = [this] { Intents.emplace_back("cancel-keep"); },
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
		expect(test.button(QStringLiteral("agentSetupFindAgents")) != nullptr && test.button(QStringLiteral("agentSetupFindClones")) == nullptr && test.button(QStringLiteral("agentSetupLookAgain")) == nullptr, "the Agent step owns its specific rescan; no generic footer search remains");
		test.button(QStringLiteral("agentSetupFindAgents"))->click();
		expect(test.Intents.back() == "find-agents", "Find agents again only requests agent discovery");
		expect(test.text().contains(QStringLiteral("No agent found on this computer")) && test.text().contains(QStringLiteral("Claude Code and Codex")), "it says none was found and what it looked for");
		test.button(QStringLiteral("agentSetupInstallClaude"))->click();
		test.button(QStringLiteral("agentSetupInstallCodex"))->click();
		// Owner amendment 2026-10-23: Custom agent… in setup describes a
		// wrapper of the reviewer's own and adds it.
		test.View.set_entry_editor([](const core::agent_entry&, QWidget*) -> std::optional<core::agent_entry> {
			return core::agent_entry{ .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex" } };
		});
		test.button(QStringLiteral("agentSetupCustomAgent"))->click();
		expect(test.Intents == std::vector<std::string>{ "find-agents", "install:claude_code", "install:codex", "custom:Work Codex:/opt/work/bin/wrapper:--codex" }, "Install and Custom agent… are intents");
		render(test.View, QStringLiteral("none"));
	}

	void test_two_agents_and_a_refused_one()
	{
		auto test             = fixture{ };
		test.Setup.State.Rows = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path),
								  row(core::setup_agent::codex, "/Users/me/.local/bin/codex", core::agent_found_how::usual_folder, false) };
		test.Setup.Choices    = { card(0U, core::setup_agent::claude_code, "Claude Code", "/opt/homebrew/bin/claude", core::agent_found_how::on_path),
								  card(0U, core::setup_agent::codex, "Codex", "/Users/me/.local/bin/codex", core::agent_found_how::usual_folder, false) };
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

	/// \brief [[BPR-2256]], owner amendment 2026-11-18 ("If we can't get
	/// past step 1 then the whole drawer should just be in a loading
	/// state"). While no host policy is observed the setup is one loading
	/// state: no title, steps, cards or refusal. Once one is, the steps
	/// return and each refused card names its own reason.
	void test_host_policy_states()
	{
		auto test                = fixture{ };
		test.Setup.PolicyPending = true;
		test.Setup.State.Rows    = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path, false),
									 row(core::setup_agent::codex, "/opt/homebrew/bin/codex", core::agent_found_how::on_path, false) };
		test.Setup.Choices       = { card(7U, core::setup_agent::claude_code, "Claude stream JSON", "/opt/work/bin/claude-wrapper", core::agent_found_how::saved_program, false),
									 card(0U, core::setup_agent::claude_code, "Claude Code", "/opt/homebrew/bin/claude", core::agent_found_how::on_path, false),
									 card(0U, core::setup_agent::codex, "Codex", "/opt/homebrew/bin/codex", core::agent_found_how::on_path, false) };
		for (auto& choice : test.Setup.Choices) {
			choice.Verdict.Refusal = core::agent_invocation_refusal::host_not_mapped;
		}
		test.View.resize(420, 560);
		test.View.show();
		test.show();
		const auto shown = [&test](const QWidget* aWidget) { return aWidget != nullptr && aWidget->isVisibleTo(&test.View); };
		expect(test.View.needed() && shown(test.View.loading_state()), "while no policy is known the setup is its loading state");
		expect(test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow")).isEmpty() && test.View.findChildren<QWidget*>(QStringLiteral("agentSetupStep1")).isEmpty()
				   && !shown(test.View.findChild<QWidget*>(QStringLiteral("agentSetupTitle"))) && !shown(test.View.findChild<QWidget*>(QStringLiteral("agentSetupFooter"))),
			   "with no title, step, card or footer");
		const auto* const said = test.View.findChild<QLabel*>(QStringLiteral("agentSetupLoadingText"));
		expect(shown(said) && said->text() == QStringLiteral("Waiting for the pull request to load…") && !test.text().contains(QStringLiteral("Not allowed")) && !test.text().contains(QStringLiteral("aren't allowed")),
			   "it says the pull request is loading, and nothing is refused");
		render(test.View, QStringLiteral("policy-pending"));

		// The Enterprise policy arrives: Codex refused by the host, the others permitted.
		test.Setup.PolicyPending         = false;
		test.Setup.Choices[0].Verdict    = { .Permitted = true };
		test.Setup.Choices[1].Verdict    = { .Permitted = true };
		test.Setup.Choices[2].Verdict    = { .Refusal = core::agent_invocation_refusal::profile_forbidden_on_host, .RefusedBy = core::git_host_id{ 2U } };
		test.Setup.State.Rows[0].Verdict = { .Permitted = true };
		test.show();
		const auto cards = test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow"));
		expect(!shown(test.View.loading_state()) && cards.size() == 3 && shown(test.View.findChild<QWidget*>(QStringLiteral("agentSetupTitle"))), "once the policy is known the steps and cards return");
		expect(test.text().count(QStringLiteral("Not allowed for this pull request's host.")) == 1 && test.text().contains(QStringLiteral("Found on PATH.")) && test.text().contains(QStringLiteral("Your process wrapper.")),
			   "only Codex says the host does not allow it");
		render(test.View, QStringLiteral("policy-ghes"));

		// Each refusal reason has its own words, and waiting is not one.
		using refusal    = core::agent_invocation_refusal;
		const auto words = [](const refusal aRefusal, const bool aObserved, std::optional<core::agent_profile_id> aRequired = std::nullopt) {
			return qt::agent_verdict_words({ .Refusal = aRefusal, .RequiredInstead = aRequired }, aObserved);
		};
		const auto claude = core::built_in_agent_profiles().front();
		expect(qt::agent_verdict_words({ .Permitted = true }, true).isEmpty(), "a permitted agent says nothing");
		expect(words(refusal::host_not_mapped, false) == QStringLiteral("Waiting for the pull request to load…") && words(refusal::profile_forbidden_on_host, false) == words(refusal::host_not_mapped, false),
			   "before a policy, any refusal reads as waiting");
		expect(words(refusal::profile_forbidden_on_host, true) == QStringLiteral("Not allowed for this pull request's host."), "a host's refusal");
		expect(words(refusal::host_not_mapped, true) == qt::host_not_mapped_words() && qt::host_not_mapped_words() == QStringLiteral("Agents aren't allowed for this pull request's server."),
			   "a server with no policy, in the send refusal's own words");
		expect(words(refusal::profile_not_required_one, true, claude.Id) == QStringLiteral("This pull request's host requires %1.").arg(QString::fromStdString(claude.DisplayName)), "a required agent is named");
		expect(words(refusal::profile_not_required_one, true) == QStringLiteral("This pull request's host requires a different agent."), "and said when it cannot be");
		auto all = QStringList{ };
		for (const auto reason : { refusal::host_not_mapped, refusal::profile_forbidden_on_host, refusal::profile_not_required_one, refusal::profile_invalid, refusal::profile_unknown }) {
			all << words(reason, true);
		}
		all << words(refusal::host_not_mapped, false);
		all.removeDuplicates();
		expect(all.size() == 6, "every reason, and waiting, has its own sentence");
		test.View.hide();
	}

	void test_change_keeps_the_current_agent()
	{
		auto test                         = fixture{ };
		test.Setup.State.Rows             = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path),
											  row(core::setup_agent::codex, "/Users/me/.local/bin/codex", core::agent_found_how::on_path) };
		test.Setup.Choices                = { card(0U, core::setup_agent::claude_code, "Claude Code", "/opt/homebrew/bin/claude", core::agent_found_how::on_path, true, true),
											  card(0U, core::setup_agent::codex, "Codex", "/Users/me/.local/bin/codex", core::agent_found_how::on_path) };
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

	/// Owner report ("If I add an agent through agent settings, the agent
	/// setup wizard doesn't update to show it"): the Agent step's cards are the
	/// list as published, so a later projection with an agent added, edited or
	/// removed shows at once, the step staying open; an entry card chooses by
	/// its entry.
	void test_the_list_follows_settings()
	{
		auto test = fixture{ };
		test.show();
		expect(test.text().contains(QStringLiteral("No agent found on this computer")), "with nothing found and nothing added, none is found");
		// Settings adds a custom agent while the setup shows.
		test.Setup.Choices = { card(1U, core::setup_agent::claude_code, "Work Claude", "/opt/work/bin/wrapper", core::agent_found_how::saved_program) };
		test.show();
		expect(test.View.open_step() == core::setup_step::agent && test.text().contains(QStringLiteral("Work Claude")) && test.text().contains(QDir::toNativeSeparators(QStringLiteral("/opt/work/bin/wrapper")))
				   && !test.text().contains(QStringLiteral("No agent found on this computer")),
			   "an agent added in Settings is a card at once");
		// And a popular agent whose program is not found.
		test.Setup.Choices.push_back(card(2U, core::setup_agent::codex, "Codex", { }, std::nullopt));
		test.show();
		auto cards = test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow"));
		expect(cards.size() == 2 && test.text().contains(QStringLiteral("Not found on this computer.")) && cards[1]->focusPolicy() == Qt::NoFocus, "an added agent whose program is not found is shown, and cannot be chosen");
		click(*cards[1]);
		expect(test.Intents.empty(), "choosing it asks nothing");
		click(*cards[0]);
		expect(test.Intents == std::vector<std::string>{ "entry:1" }, "an added agent's card chooses its entry");
		// Edited, then removed, in Settings.
		test.Setup.Choices.front().Name = "Work Claude 2";
		test.show();
		expect(test.text().contains(QStringLiteral("Work Claude 2")), "an edit in Settings shows");
		test.Setup.Choices.erase(test.Setup.Choices.begin());
		test.show();
		expect(!test.text().contains(QStringLiteral("Work Claude")) && test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow")).size() == 1, "a removal in Settings shows");
		// The chosen card is the one the projection marks; Change keeps the step.
		test.Setup.State.Rows   = { row(core::setup_agent::claude_code, "/opt/work/bin/wrapper", core::agent_found_how::saved_program) };
		test.Setup.State.Chosen = core::setup_agent::claude_code;
		test.Setup.Choices      = { card(3U, core::setup_agent::claude_code, "Work Claude", "/opt/work/bin/wrapper", core::agent_found_how::saved_program, true, true),
									card(4U, core::setup_agent::claude_code, "Home Claude", "/opt/home/bin/wrapper", core::agent_found_how::saved_program) };
		test.show();
		test.button(QStringLiteral("agentSetupChange"))->click();
		auto radios = test.View.findChildren<QRadioButton*>(QStringLiteral("agentSetupAgentChoice"));
		expect(radios.size() == 2 && radios[0]->isChecked() && !radios[1]->isChecked(), "the chosen entry is marked, not every entry of its kind");
		test.Setup.Choices.push_back(card(5U, core::setup_agent::codex, "Work Codex", "/opt/work/bin/wrapper", core::agent_found_how::saved_program));
		test.show();
		expect(test.View.open_step() == core::setup_step::agent && test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow")).size() == 3,
			   "an agent added while Change is open keeps the step open and shows");
		test.Intents.clear();
		click(*test.View.findChildren<QWidget*>(QStringLiteral("agentSetupAgentRow"))[1]);
		expect(test.Intents == std::vector<std::string>{ "entry:4" }, "another entry of the chosen kind is chosen by its entry");
		render(test.View, QStringLiteral("list"));
	}

	void test_repository_states()
	{
		auto test               = fixture{ };
		test.Setup.State.Rows   = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path) };
		test.Setup.State.Chosen = core::setup_agent::claude_code;
		test.Clones.Suggestions = { "/Users/me/Documents/GitHub" };
		test.show();
		// [[BPR-1879]] owner look v006: linking the reviewer's own clone is the
		// first card, recommended; with no folder listed it holds the folder
		// line and the button adds the folder.
		expect(test.View.open_step() == core::setup_step::repository && test.text().contains(QStringLiteral("Link your clone")) && test.text().contains(QStringLiteral("Add the folder your clones are in"))
				   && test.View.findChild<QLabel*>(QStringLiteral("agentSetupLinkCardFigure"))->text() == QStringLiteral("Recommended"),
			   "with no clone folder the recommended card asks for the folder clones are in");
		// Owner look v003: one folder line in setup, typed or browsed, then
		// Add; the list editor is Settings' alone. Owner look v004: no "Or
		// add" row; the folder found is the line's text, and Add takes it.
		auto* const path = test.View.findChild<QLineEdit*>(QStringLiteral("agentSetupFolderPath"));
		auto* const add  = test.View.findChild<QPushButton*>(QStringLiteral("agentSetupFolderAdd"));
		expect(path != nullptr && add != nullptr && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupFolderBrowse")) != nullptr
				   && test.View.findChild<QWidget*>(QStringLiteral("agentSetupAddFolder")) == nullptr && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupSuggestedFolder")) == nullptr
				   && !test.text().contains(QStringLiteral("Or add")),
			   "no folder yet: one folder line with Browse and Add, and no separate suggestion row");
		if (path == nullptr || add == nullptr) {
			return;
		}
		expect(path->text() == QDir::toNativeSeparators(QStringLiteral("/Users/me/Documents/GitHub")) && add->isEnabled(), "the folder found is the line's text, and Add can be pressed at once");
		add->click();
		expect(test.Intents.back() == "folder:" + QDir::toNativeSeparators(QStringLiteral("/Users/me/Documents/GitHub")).toStdString(), "Add adds the prefilled folder in one press");
		const auto acceptedFolderIntent = test.Intents.back();
		const auto acceptedIntentCount  = test.Intents.size();
		path->clear();
		expect(!add->isEnabled(), "an empty line waits for a path");
		path->setText(QStringLiteral("work/clones"));
		add->click();
		expect(test.Intents.size() == acceptedIntentCount && test.Intents.back() == acceptedFolderIntent && test.View.findChild<QLabel*>(QStringLiteral("agentSetupFolderStatus"))->text().contains(QStringLiteral("full path")),
			   "a relative path is refused with what to write");
		path->setText(QStringLiteral("~/src"));
		add->click();
		const auto homeFolderIntent = "folder:" + QDir::toNativeSeparators(QDir{ QDir::homePath() }.filePath(QStringLiteral("src"))).toStdString();
		if (test.Intents.back() != homeFolderIntent) {
			std::cerr << "HOME_FOLDER_INTENT actual=" << test.Intents.back() << " expected=" << homeFolderIntent << '\n';
		}
		expect(test.Intents.size() == acceptedIntentCount + 1U && test.Intents.back() == homeFolderIntent, "~ is the home folder, and Add adds the folder");
		render(test.View, QStringLiteral("nofolders"));

		test.Clones.Folders    = { "/Users/me/Documents/GitHub" };
		test.Clones.Discovered = true;
		test.show();
		expect(test.text().contains(QStringLiteral("None in ")) && test.button(QStringLiteral("agentSetupChooseFolder"))->isDefault() && test.button(QStringLiteral("agentSetupEditFolders")) != nullptr
				   && test.button(QStringLiteral("agentSetupFindClones")) == nullptr,
			   "none found: the card says so, its button chooses the folder, Edit clone folders is small beside, and there is no separate search link");
		// [[BPR-1879]], owner look v006. A review that holds every file needs
		// no clone: only Link your clone is offered, as an improvement; there
		// is no third card, and nothing names a private copy.
		expect(test.View.findChild<QWidget*>(QStringLiteral("agentSetupPullRequestOnlyCard")) == nullptr && test.View.findChild<QWidget*>(QStringLiteral("agentSetupKeepCard")) == nullptr
				   && !test.text().contains(QStringLiteral("private copy"), Qt::CaseInsensitive),
			   "a review holding every file offers only Link your clone");
		render(test.View, QStringLiteral("noclone"));

		// [[BPR-1873]], [[BPR-1879]] owner look v005. A sparse review with no
		// clone found: one choice between cards in order of strength, the
		// reviewer's own clone first, chosen and recommended, and one button
		// acting on the card chosen.
		test.Facts.Sparse = true;
		test.show();
		// The step builds its widgets top to bottom, so creation order is the
		// order shown.
		const auto before = [&test](const QWidget* aFirst, const QWidget* aSecond) {
			const auto all = test.View.findChildren<QWidget*>();
			return aFirst != nullptr && aSecond != nullptr && all.indexOf(aFirst) < all.indexOf(aSecond);
		};
		auto* const linkCard   = test.View.findChild<QWidget*>(QStringLiteral("agentSetupLinkCard"));
		auto* const linkFigure = test.View.findChild<QLabel*>(QStringLiteral("agentSetupLinkCardFigure"));
		auto* const onlyCard   = test.View.findChild<QWidget*>(QStringLiteral("agentSetupPullRequestOnlyCard"));
		auto* const cost       = test.View.findChild<QLabel*>(QStringLiteral("agentSetupPullRequestOnlyFact"));
		auto* const choose     = test.View.findChild<QPushButton*>(QStringLiteral("agentSetupChooseFolder"));
		expect(linkCard != nullptr && linkFigure != nullptr && onlyCard != nullptr && cost != nullptr && choose != nullptr && linkFigure->text() == QStringLiteral("Recommended")
				   && before(linkCard, onlyCard) && choose->text() == QStringLiteral("Choose its folder…") && choose->isDefault()
				   && test.View.findChild<QWidget*>(QStringLiteral("agentSetupKeepCard")) == nullptr && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupPullRequestOnly")) == nullptr
				   && cost->text() == QStringLiteral("No callers, tests or history"),
			   "the reviewer's own clone is the first card, chosen, recommended and the default button; the pull request only is last, its cost in one line");
		click(*onlyCard);
		auto* const only = test.View.findChild<QPushButton*>(QStringLiteral("agentSetupPullRequestOnly"));
		expect(only != nullptr && only->text() == QStringLiteral("Use this pull request only") && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupChooseFolder")) == nullptr,
			   "choosing the pull request only turns the button to it");
		if (only != nullptr) {
			only->click();
			expect(test.Intents.back() == "private-copy", "the pull request only is remembered as this repository's no-clone choice");
		}
		click(*test.View.findChild<QWidget*>(QStringLiteral("agentSetupLinkCard")));
		render(test.View, QStringLiteral("noclone-sparse"));
		// Keeping a clone is the second card: its size is the card's figure,
		// where it goes its one line, the whole path in the tooltip.
		test.Clones.Repository->CanKeep          = true;
		test.Clones.Repository->ApproximateBytes = 126U * 1000U * 1000U;
		test.Clones.ManagedLocation              = "/Users/me/Library/Application Support/BetterPullReview/agent-clones-v1/github.com-42";
		test.show();
		auto* const keepCard  = test.View.findChild<QWidget*>(QStringLiteral("agentSetupKeepCard"));
		auto* const keepTitle = test.View.findChild<QLabel*>(QStringLiteral("agentSetupKeepCardTitle"));
		auto* const keepSize  = test.View.findChild<QLabel*>(QStringLiteral("agentSetupKeepSize"));
		auto* const keepCost  = test.View.findChild<QLabel*>(QStringLiteral("agentSetupKeepCost"));
		expect(keepCard != nullptr && keepTitle != nullptr && keepSize != nullptr && keepCost != nullptr && keepTitle->text() == QStringLiteral("Keep a clone for me")
				   && before(test.View.findChild<QWidget*>(QStringLiteral("agentSetupLinkCard")), keepCard)
				   && before(keepCard, test.View.findChild<QWidget*>(QStringLiteral("agentSetupPullRequestOnlyCard"))) && keepSize->text() == QStringLiteral("126 MB")
				   && keepCost->text().startsWith(QStringLiteral("Full history, kept in ")) && keepCost->toolTip().endsWith(QDir::toNativeSeparators(QStringLiteral("agent-clones-v1/github.com-42")))
				   && test.button(QStringLiteral("agentSetupChooseFolder"))->isDefault(),
			   "AC1: keeping a clone is the second card, with its size and where it goes; choosing a folder stays the default");
		render(test.View, QStringLiteral("keep-offer"));
		click(*keepCard);
		auto* const keep = test.View.findChild<QPushButton*>(QStringLiteral("agentSetupKeepClone"));
		expect(keep != nullptr && keep->text() == QStringLiteral("Keep a clone") && keep->isDefault(), "choosing it turns the button to keeping one");
		if (keep != nullptr) {
			keep->click();
			expect(test.Intents.back() == "keep", "the button asks the host to keep one");
		}
		render(test.View, QStringLiteral("keep-chosen"));
		test.Clones.Keeping = { .State = application::managed_clone_state::downloading, .Received = 42U * 1000U * 1000U, .Failure = std::nullopt };
		test.show();
		auto* const bar    = test.View.findChild<QProgressBar*>(QStringLiteral("agentSetupKeepBar"));
		auto* const status = test.View.findChild<QLabel*>(QStringLiteral("agentSetupKeepStatus"));
		auto* const stop   = test.View.findChild<QPushButton*>(QStringLiteral("agentSetupKeepCancel"));
		expect(bar != nullptr && status != nullptr && stop != nullptr && bar->value() == 333 && status->text() == QStringLiteral("42 MB of 126 MB")
				   && test.View.findChild<QLabel*>(QStringLiteral("agentSetupKeepCardTitle"))->text() == QStringLiteral("Keeping a clone")
				   && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupKeepClone")) == nullptr && test.View.findChild<QWidget*>(QStringLiteral("agentSetupPullRequestOnlyCard")) == nullptr
				   && test.View.findChild<QWidget*>(QStringLiteral("agentSetupLinkCard")) == nullptr && test.View.findChild<QLabel*>(QStringLiteral("agentSetupNoClone")) == nullptr
				   && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupEditFolders")) == nullptr && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupFindClones")) == nullptr,
			   "AC1, owner look v004: once it downloads, its progress and Cancel are the whole step");
		if (stop != nullptr) {
			stop->click();
			expect(test.Intents.back() == "cancel-keep", "Cancel asks the host to stop");
		}
		render(test.View, QStringLiteral("keep-downloading"));
		// Owner decision 2026-11-05: no limit, so a download may run past
		// GitHub's figure; the bar keeps moving and the bytes keep counting.
		test.Clones.Keeping.Received = 131U * 1000U * 1000U;
		test.show();
		auto* const pastBar    = test.View.findChild<QProgressBar*>(QStringLiteral("agentSetupKeepBar"));
		auto* const pastStatus = test.View.findChild<QLabel*>(QStringLiteral("agentSetupKeepStatus"));
		expect(pastBar != nullptr && pastStatus != nullptr && pastBar->maximum() == 0 && pastStatus->text() == QStringLiteral("131 MB")
				   && test.View.findChild<QPushButton*>(QStringLiteral("agentSetupKeepCancel")) != nullptr,
			   "past GitHub's size the bar keeps moving, the bytes keep counting, and Cancel stays");
		render(test.View, QStringLiteral("keep-downloading-past"));
		test.Clones.Keeping = { .State = application::managed_clone_state::failed, .Received = 0U, .Failure = application::managed_clone_failure::connection_failed };
		test.show();
		auto* const keepFailed = test.View.findChild<QLabel*>(QStringLiteral("agentSetupKeepFailed"));
		expect(keepFailed != nullptr && keepFailed->text() == QStringLiteral("No connection to GitHub.") && test.button(QStringLiteral("agentSetupKeepClone"))->text() == QStringLiteral("Retry"),
			   "a failed download says why on its card, and the button reads Retry");
		render(test.View, QStringLiteral("keep-failed"));
		test.Clones.Keeping = { };
		test.Clones.Linked  = application::repository_link{ .Host = "github.com", .Repository = 42U, .Owner = "bpr-fixtures", .Name = "engine", .Path = test.Clones.ManagedLocation, .Remote = "origin", .PrivateCopy = false, .Managed = true };
		test.show();
		expect(test.View.ready_text().contains(QStringLiteral("the clone BetterPullReview keeps")), "a kept clone is named as such, not by its folder");
		test.Clones.Linked.reset();
		test.Clones.Repository->CanKeep = false;
		test.Clones.ManagedLocation.clear();
		test.Clones.Folders.clear();
		test.show();
		expect(test.View.findChild<QWidget*>(QStringLiteral("agentSetupPullRequestOnlyCard")) != nullptr && test.button(QStringLiteral("agentSetupFolderAdd")) != nullptr,
			   "with no clone folder yet the same cards show, and the recommended one adds the folder");
		render(test.View, QStringLiteral("nofolders-sparse"));
		test.Clones.Folders = { "/Users/me/Documents/GitHub" };
		test.Facts.Sparse   = false;

		test.Clones.Matches = { core::clone_match{ .Path = "/Users/me/Documents/GitHub/engine", .Folder = "/Users/me/Documents/GitHub", .Remote = "origin", .Location = "github.com/bpr-fixtures/engine", .Branch = "main" },
								core::clone_match{ .Path = "/Users/me/src/engine-old", .Folder = "/Users/me/src", .Remote = "upstream", .Location = "github.com/bpr-fixtures/engine", .Branch = "spike/retry" } };
		test.show();
		auto clones = test.View.findChildren<QRadioButton*>(QStringLiteral("agentSetupCloneChoice"));
		expect(clones.size() == 2 && test.button(QStringLiteral("agentSetupUseClone"))->text() == QStringLiteral("Link this clone") && test.button(QStringLiteral("agentSetupUseClone"))->isDefault()
				   && test.button(QStringLiteral("agentSetupChooseOther")) != nullptr,
			   "clones found are on the recommended card, the first chosen, and one press links it");
		click(*test.View.findChildren<QWidget*>(QStringLiteral("agentSetupCloneRow"))[1]);
		QApplication::processEvents();
		test.button(QStringLiteral("agentSetupUseClone"))->click();
		expect(test.Intents.back() == "clone:/Users/me/src/engine-old", "choosing another clone found and Link this clone links that one");
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
		expect(test.button(QStringLiteral("agentSetupFindAgents")) == nullptr && test.button(QStringLiteral("agentSetupFindClones")) == nullptr, "Check it works offers no discovery action");
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

		test.Setup.CheckDetail     = "nonzero_exit (exit 7)\nstderr:\nSSO fixture expired <a href=\"https://example.invalid\">literal</a>";
		test.Setup.WorkspaceDetail = "fatal: fixture download failed from <redacted-url>";
		test.show();
		auto* details   = test.View.findChild<QToolButton*>(QStringLiteral("agentSetupShowDetails"));
		auto* log       = test.View.findChild<QToolButton*>(QStringLiteral("agentSetupShowWorkspaceLog"));
		auto* output    = test.View.findChild<QPlainTextEdit*>(QStringLiteral("agentSetupShowDetailsText"));
		auto* gitOutput = test.View.findChild<QPlainTextEdit*>(QStringLiteral("agentSetupShowWorkspaceLogText"));
		expect(details != nullptr && log != nullptr && output != nullptr && gitOutput != nullptr, "both promised diagnostics controls exist");
		expect(!output->isVisibleTo(&test.View) && !gitOutput->isVisibleTo(&test.View) && details->focusPolicy() != Qt::NoFocus && !details->accessibleName().isEmpty(), "diagnostics start collapsed and are keyboard-accessible");
		details->setFocus();
		auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
		auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
		QApplication::sendEvent(details, &press);
		QApplication::sendEvent(details, &release);
		log->click();
		QApplication::processEvents();
		expect(output->isVisibleTo(&test.View) && output->isReadOnly() && output->toPlainText().toStdString() == test.Setup.CheckDetail, "Space opens literal selectable read-only agent details");
		expect(gitOutput->isVisibleTo(&test.View) && gitOutput->toPlainText().toStdString() == test.Setup.WorkspaceDetail, "the workspace disclosure opens its own log");
		test.show();
		expect(test.View.findChild<QToolButton*>(QStringLiteral("agentSetupShowDetails"))->isChecked(), "an unchanged projection preserves the open disclosure");
		// [[BPR-2215]] (owner, 0.5.0 build 75: "␛[0;31m…␛[0m" in the details).
		test.Setup.CheckDetail     = "nonzero_exit (exit 1)\nstderr:\n\x1b[0;31merror:\x1b[0m \x1b[1msign-in\x1b[22m expired \xe2\x9c\x97\x1b]8;;https://example.invalid\x1b\\ link\x1b]8;;\x1b\\\x1b[0;3";
		test.Setup.WorkspaceDetail = "\x1b[33mwarning:\x1b[m fetch \x1b[38;2;255;0;0mfailed\x1b[39m";
		test.show();
		output    = test.View.findChild<QPlainTextEdit*>(QStringLiteral("agentSetupShowDetailsText"));
		gitOutput = test.View.findChild<QPlainTextEdit*>(QStringLiteral("agentSetupShowWorkspaceLogText"));
		expect(output != nullptr && gitOutput != nullptr && output->toPlainText() == QStringLiteral("nonzero_exit (exit 1)\nstderr:\nerror: sign-in expired \u2717 link")
				   && gitOutput->toPlainText() == QStringLiteral("warning: fetch failed") && !test.text().contains(QChar{ 0x1b }),
			   "coloured process output shows as plain text in both boxes");
		test.Setup.CheckDetail     = "nonzero_exit (exit 7)\nstderr:\nSSO fixture expired <a href=\"https://example.invalid\">literal</a>";
		test.Setup.WorkspaceDetail = "fatal: fixture download failed from <redacted-url>";
		test.show();
		render(test.View, QStringLiteral("check-details"));
		test.Setup.ChecksWrapper    = true;
		test.Setup.State.AgentCheck = core::fail_check(core::agent_setup_failure::status_unreadable);
		test.show();
		expect(test.text().contains(QStringLiteral("Claude process wrapper reports signed in")) && test.text().contains(QStringLiteral("Sign-in is reported by your wrapper for its configured provider"))
				   && !test.text().contains(QStringLiteral("Claude Code is signed in")),
			   "a wrapper check names the source of sign-in and its saved arguments");
		render(test.View, QStringLiteral("wrapper-check-details"));

		test.Setup.Checking = true;
		test.Setup.CheckDetail.clear();
		test.Setup.WorkspaceDetail.clear();
		test.Setup.State.AgentCheck     = { .State = core::setup_check_state::running };
		test.Setup.State.WorkspaceCheck = { .State = core::setup_check_state::running };
		test.show();
		expect(test.View.findChild<QPlainTextEdit*>(QStringLiteral("agentSetupShowDetailsText")) == nullptr
				   && test.View.findChild<QPlainTextEdit*>(QStringLiteral("agentSetupShowWorkspaceLogText")) == nullptr,
			   "a fresh check removes old diagnostic controls and text");
		test.button(QStringLiteral("agentSetupStop"))->click();
		expect(test.Intents.back() == "stop", "a running check can be stopped");
		test.Setup.Checking         = false;
		test.Setup.State.AgentCheck = { .State = core::setup_check_state::passed };
		test.show();
		expect(test.View.findChild<QProgressBar*>(QStringLiteral("agentSetupProgress")) != nullptr && test.text().contains(QStringLiteral("Preparing workspace"))
				   && test.button(QStringLiteral("agentSetupCheck")) == nullptr,
			   "finishing the agent check leaves visible busy feedback while the workspace is still running");
		render(test.View, QStringLiteral("workspace-busy"));
		test.Setup.Checking             = true;
		test.Setup.State.AgentCheck     = { .State = core::setup_check_state::running };
		test.Setup.State.WorkspaceCheck = { .State = core::setup_check_state::passed };
		test.show();
		expect(test.text().contains(QStringLiteral("Checking agent")) && test.button(QStringLiteral("agentSetupCheck")) == nullptr, "finishing the workspace first keeps the agent's busy feedback");
		test.Setup.Checking             = false;
		test.Setup.State.AgentCheck     = { .State = core::setup_check_state::passed, .Version = "2.1.296" };
		test.Setup.State.WorkspaceCheck = { .State = core::setup_check_state::passed };
		test.show();
		expect(!test.View.needed() && test.View.take_ready() && !test.View.take_ready(), "both checks passed: the setup gives way, once, to the ready line");
		expect(test.View.ready_title() == QStringLiteral("Claude Code is ready") && test.View.ready_text().contains(QStringLiteral("version 2.1.296")), "the ready line names the agent and version");
		test.Setup.ChecksSignIn = false;
		test.View.begin(core::setup_step::check);
		test.show();
		expect(!test.View.ready_text().contains(QStringLiteral("Signed in")) && test.View.ready_text().contains(QStringLiteral("Program check passed"))
				   && test.text().contains(QStringLiteral("is available")),
			   "a custom program check and ready line never assert authentication");
		test.Setup.ChecksSignIn = true;

		const auto gets = qt::agent_ready_gets({ .Repository = "bpr-fixtures/engine", .Number = 22U, .Title = "Clamp device scale", .BaseBranch = "main", .Base = "51d2dad", .Head = "7f3a9c1", .Files = 18U, .Threads = 6U, .Unresolved = 2U, .Clone = "/Users/me/Documents/GitHub/engine" });
		expect(gets.contains(QStringLiteral("bpr-fixtures/engine #22")) && gets.contains(QStringLiteral("git diff 51d2dad...7f3a9c1")) && gets.contains(QStringLiteral("6 review threads, 2 unresolved")),
			   "What the agent gets states the brief");

		// Owner look v006: the step searches the clone folders itself, once.
		test.Clones.Folders    = { "/Users/me/Documents/GitHub" };
		test.Clones.Discovered = false;
		test.show();
		test.View.begin(core::setup_step::repository);
		expect(test.View.needed() && test.View.open_step() == core::setup_step::repository, "Change repository folder… opens that step again");
		QApplication::processEvents();
		expect(test.button(QStringLiteral("agentSetupFindClones")) == nullptr && test.button(QStringLiteral("agentSetupFindAgents")) == nullptr && std::ranges::count(test.Intents, std::string{ "find-clones" }) >= 1,
			   "the Repository step searches its clone folders without a link to press");
		const auto searches = std::ranges::count(test.Intents, std::string{ "find-clones" });
		test.View.begin(core::setup_step::repository);
		QApplication::processEvents();
		expect(std::ranges::count(test.Intents, std::string{ "find-clones" }) == searches, "and only once for the repository, not on every refresh");
		render(test.View, QStringLiteral("repository-rescan"));
	}

	void test_settings_rows()
	{
		// [[BPR-1753]], owner amendment 2026-10-23: Settings › Agents
		// is a list with Add agent…, Edit…, Remove and Move up and down.
		auto programs = qt::agent_programs_panel{ };
		auto saved    = std::vector<std::string>{ };
		programs.set_dispatch({ .Reset = [&saved] { saved.emplace_back("reset"); },
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
								.Move   = [&saved](const std::uint32_t aId, const int aDelta) { saved.push_back("move:" + std::to_string(aId) + ":" + std::to_string(aDelta)); },
								.SetRunLimit =
									[&saved](const std::uint32_t aMinutes) {
										saved.push_back("limit:" + std::to_string(aMinutes));
										return true;
									},
								.SetScratchPolicy =
									[&saved](const std::uint32_t aLimit, const std::uint32_t aDays) {
										saved.push_back("scratch:" + std::to_string(aLimit) + ":" + std::to_string(aDays));
										return true;
									},
								.ClearScratch = [&saved] { saved.emplace_back("clear-scratch"); },
								.SetWorkspacePolicy =
									[&saved](const std::uint32_t aDays, const std::uint32_t aCap) {
										saved.push_back("workspaces:" + std::to_string(aDays) + ":" + std::to_string(aCap));
										return true;
									},
								.RemoveWorkspaces = [&saved] { saved.emplace_back("remove-workspaces"); } });
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
		// [[BPR-2318]]. Remove asks first.
		expect(programs.remove_question() != nullptr && saved.back() == "move:2:-1", "Remove asks before it removes an agent");
		programs.remove_question()->accept();
		programs.findChild<QPushButton*>(QStringLiteral("settingsResetAgents"))->click();
		expect(saved == std::vector<std::string>{ "add:Claude Code:preset", "add:Harness:custom", "update:2:Work Claude", "move:2:-1", "remove:2", "reset" },
			   "add, edit, move, remove and Set up agent… are intents");
		// Owner decision 2026-10-23: the run time limit is off unless chosen.
		expect(!programs.run_limit_toggle()->isChecked() && !programs.run_limit_minutes()->isEnabled(), "with none set, runs have no time limit");
		programs.run_limit_toggle()->setChecked(true);
		programs.run_limit_toggle()->setChecked(false);
		expect(saved.size() >= 2U && saved[saved.size() - 2U] == "limit:10" && saved.back() == "limit:0", "turning it on sends a limit, and off sends none");
		auto limited            = preferences;
		limited.RunLimitMinutes = 30U;
		programs.present(limited);
		expect(programs.run_limit_toggle()->isChecked() && programs.run_limit_minutes()->value() == 30, "a saved limit shows");
		// RFC-BPR-0104 as amended 2026-11-11 (owner look e97c6a3d): the
		// scratch folder limit, how long an unused folder is kept, what they
		// hold, and Clear now.
		expect(programs.scratch_limit()->value() == 256 && programs.scratch_keep_toggle()->isChecked() && programs.scratch_keep_days()->value() == 30, "the scratch policy starts at 256 MiB and 30 days");
		programs.scratch_limit()->setValue(1024);
		emit programs.scratch_limit()->editingFinished();
		programs.scratch_keep_toggle()->setChecked(false);
		expect(saved.size() >= 2U && saved[saved.size() - 2U] == "scratch:1024:30" && saved.back() == "scratch:1024:0", "a limit and keeping folders until cleared are sent");
		programs.present_scratch_usage(std::pair<std::uint64_t, std::size_t>{ 3U * 1024U * 1024U, 2U });
		expect(programs.scratch_usage()->text().contains(QStringLiteral("2 folders")) && programs.scratch_clear_button()->isEnabled(), "what the folders hold");
		programs.scratch_clear_button()->click();
		expect(saved.back() == "clear-scratch", "Clear scratch folders is an intent");
		programs.present_scratch_usage(std::pair<std::uint64_t, std::size_t>{ 0U, 0U }, QStringLiteral("Cleared 3 MB from 2 folders."));
		expect(programs.scratch_usage()->text().startsWith(QStringLiteral("No scratch folders.")) && !programs.scratch_clear_button()->isEnabled(), "nothing left to clear");
		// [[BPR-2293]]. The retained agent workspaces: the cap, the days, what
		// they hold and Remove…, which asks first.
		expect(programs.workspace_cap()->value() == 20 && programs.workspace_keep_toggle()->isChecked() && programs.workspace_keep_days()->value() == 7, "the workspace policy starts at 20 GB and 7 days");
		expect(programs.workspace_usage_label()->text().contains(QStringLiteral("measuring")) && !programs.workspace_remove_button()->isEnabled(), "unmeasured, the line says so and Remove waits");
		programs.workspace_cap()->setValue(40);
		emit programs.workspace_cap()->editingFinished();
		programs.workspace_keep_toggle()->setChecked(false);
		expect(saved.size() >= 2U && saved[saved.size() - 2U] == "workspaces:7:40" && saved.back() == "workspaces:0:40", "a cap and keeping workspaces until removed are sent");
		programs.present_workspace_usage(qt::agent_programs_panel::workspace_usage{ .Count = 3U, .Bytes = 17'200'000'000ULL, .InUse = 1U });
		expect(programs.workspace_usage_label()->text().startsWith(QStringLiteral("Retained agent workspaces: 3, 17.2 GB.")) && programs.workspace_usage_label()->text().contains(QStringLiteral("1 is in use")),
			   "the line says how many and their total size, and how many are in use");
		// [[BPR-2377]]. Remove… leads to Clear local data, which asks there.
		programs.workspace_remove_button()->click();
		expect(saved.back() == "remove-workspaces" && programs.workspace_remove_button()->toolTip().contains(QStringLiteral("Clear local data")),
			   "Remove… sends its intent, which opens Clear local data");
		programs.present_workspace_usage(qt::agent_programs_panel::workspace_usage{ .Count = 1U, .Bytes = 4'100'000'000ULL, .InUse = 1U }, QStringLiteral("Removed 2 workspaces, 13.1 GB."));
		expect(programs.workspace_usage_label()->text().contains(QStringLiteral("1, 4.1 GB")) && programs.workspace_usage_label()->text().endsWith(QStringLiteral("Removed 2 workspaces, 13.1 GB."))
				   && !programs.workspace_remove_button()->isEnabled(),
			   "the line says what is left, and Remove waits while only one in use is left");
		// The dialog: OK waits for an entry that may be kept, and the
		// arguments are taken one per line, as written.
		auto dialog = qt::agent_entry_dialog{ core::agent_entry{ } };
		expect(!dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled() && dialog.problem()->isVisibleTo(&dialog), "an empty agent cannot be kept");
		dialog.name()->setText(QStringLiteral("Work Codex"));
		dialog.protocol()->setCurrentIndex(dialog.protocol()->findData(static_cast<int>(core::agent_protocol::codex_jsonl_exec)));
		dialog.program()->setText(QStringLiteral("wrapper"));
		expect(!dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled() && dialog.problem()->text().contains(QStringLiteral("full path")), "a relative program is refused with why");
		dialog.program()->setText(QStringLiteral("/opt/work/bin/wrapper"));
		dialog.arguments()->set_arguments({ "--codex", " --profile work ", "" });
		expect(dialog.arguments()->list()->count() == 3 && dialog.entry().Arguments == std::vector<std::string>{ "--codex", "--profile work" }, "each row is one argument, trimmed, and an empty row is none");
		dialog.effort()->setCurrentIndex(dialog.effort()->findData(QStringLiteral("minimal")));
		dialog.check()->setPlainText(QStringLiteral("--codex\nlogin\nstatus"));
		const auto described = dialog.entry();
		expect(dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled() && described.Arguments == std::vector<std::string>{ "--codex", "--profile work" } && described.Effort == "minimal"
				   && described.CheckArguments == std::vector<std::string>{ "--codex", "login", "status" },
			   "a full entry may be kept, each line one argument, and Codex's efforts offered");
		// [[BPR-1918]] AC3. Strict run checks: on by default, a custom
		// agent's own to turn off, and not offered for a popular agent.
		expect(dialog.strict_checks() != nullptr && dialog.strict_checks()->isChecked() && described.StrictRunChecks, "a custom agent starts with strict run checks on");
		const auto* const meaning = dialog.findChild<QLabel*>(QStringLiteral("agentEntryStrictChecksMeaning"));
		expect(meaning != nullptr && meaning->text().contains(QStringLiteral("repositories you trust")) && meaning->text().contains(QStringLiteral("instead of stopping it")), "it says plainly what turning it off means");
		dialog.strict_checks()->setChecked(false);
		expect(!dialog.entry().StrictRunChecks && dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled(), "turned off, the entry keeps it off and may be kept");
		auto popular = qt::agent_entry_dialog{ core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { }) };
		expect(popular.strict_checks() == nullptr && popular.entry().StrictRunChecks, "a popular agent shows no switch and keeps strict checks");
		// [[BPR-2307]]. Rely on the launcher's sandbox: a custom Codex agent's
		// own, off by default, with what it means right under it.
		expect(dialog.launcher_sandbox() != nullptr && dialog.launcher_sandbox()->isVisibleTo(&dialog) && !dialog.launcher_sandbox()->isChecked() && !dialog.entry().RelyOnLauncherSandbox,
			   "a custom Codex agent shows the switch, off");
		const auto* const sandboxMeaning = dialog.findChild<QLabel*>(QStringLiteral("agentEntryLauncherSandboxMeaning"));
		expect(sandboxMeaning != nullptr && sandboxMeaning->text().contains(QStringLiteral("enforced only by the launcher")) && sandboxMeaning->text().contains(QStringLiteral("Read-only runs stay read-only")),
			   "it says the grant is then enforced only by the launcher");
		dialog.launcher_sandbox()->setChecked(true);
		expect(dialog.entry().RelyOnLauncherSandbox && dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled(), "turned on, the entry keeps it on and may be kept");
		dialog.protocol()->setCurrentIndex(dialog.protocol()->findData(static_cast<int>(core::agent_protocol::claude_stream_json)));
		expect(!dialog.launcher_sandbox()->isVisibleTo(&dialog) && !dialog.entry().RelyOnLauncherSandbox, "a Claude agent hides it and never keeps it");
		dialog.protocol()->setCurrentIndex(dialog.protocol()->findData(static_cast<int>(core::agent_protocol::codex_jsonl_exec)));
		dialog.launcher_sandbox()->setChecked(false);
		auto popularCodex = qt::agent_entry_dialog{ core::preset_agent_entry(core::setup_agent::codex, core::codex_protocol_choice::app_server, { }) };
		expect(popularCodex.launcher_sandbox() == nullptr && !popularCodex.entry().RelyOnLauncherSandbox, "the popular Codex shows no switch");
		// [[BPR-2007]], [[BPR-2008]]. Scratch folder starts On and Build and
		// run tests off, beside the model and effort, and both are kept.
		expect(popular.scratch_starts()->currentText() == QStringLiteral("On") && !popular.builds()->isChecked() && popular.entry().ScratchStarts && !popular.entry().BuildsAndTests,
			   "a new agent's scratch folder starts on; builds are off");
		popular.scratch_starts()->setCurrentIndex(1);
		popular.builds()->setChecked(true);
		expect(!popular.entry().ScratchStarts && popular.entry().BuildsAndTests, "both settings are the agent's own");
		const auto* const buildsMeaning = popular.findChild<QLabel*>(QStringLiteral("agentEntryBuildsMeaning"));
		expect(buildsMeaning != nullptr && buildsMeaning->text().contains(QStringLiteral("never in your own checkout")) && buildsMeaning->text().contains(QStringLiteral("asks before the first build")), "what turning builds on means");

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
		auto* const editor       = folders.folders();
		const auto  srcFolder    = QDir::toNativeSeparators(QStringLiteral("/Users/me/src")).toStdString();
		const auto  gitHubFolder = QDir::toNativeSeparators(QStringLiteral("/Users/me/Documents/GitHub")).toStdString();
		const auto  workFolder   = QDir::toNativeSeparators(QStringLiteral("/Volumes/work")).toStdString();
		expect(editor->list()->count() == 2 && folders.links()->count() == 1 && folders.links()->item(0)->text().contains(QStringLiteral(": no clone")), "folders and links are listed");
		editor->list()->setCurrentRow(1);
		editor->up_button()->click();
		expect(asked == std::vector<std::string>{ "set " + gitHubFolder + " " + srcFolder } && editor->folders() == std::vector<std::string>{ gitHubFolder, srcFolder }, "Move up reorders the list, which is the order clones are looked for in");
		editor->list()->setCurrentRow(1);
		editor->remove_button()->click();
		expect(asked.size() == 2U && asked.back() == "set " + gitHubFolder && editor->folders() == std::vector<std::string>{ gitHubFolder }, "Remove takes the selected folder out");
		expect(type_row(*editor, QStringLiteral("/Volumes/work")) && asked.size() == 3U && asked.back() == "set " + gitHubFolder + " " + workFolder && editor->folders() == std::vector<std::string>{ gitHubFolder, workFolder },
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
	/// [[BPR-2305]]. A custom Codex whose check found its developer
	/// instructions ignored: the failure says so, and one button switches the
	/// agent to the start of the prompt and checks it again. A launcher whose
	/// output is not Codex's is named with the version its check printed.
	void mouse_on(QWidget& aWidget, const QPoint aAt, const std::initializer_list<QEvent::Type> aEvents)
	{
		for (const auto type : aEvents) {
			const auto down  = type != QEvent::MouseButtonRelease;
			auto       event = QMouseEvent{ type, QPointF{ aAt }, aWidget.mapToGlobal(QPointF{ aAt }), Qt::LeftButton, down ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(&aWidget, &event);
		}
		QApplication::processEvents();
	}

	void key_on(QWidget& aWidget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::processEvents();
	}

	/// [[BPR-2318]] (owner, 2026-11-20: "a single click on an agent opens it's
	/// settings page but that should just select it. Double click and enter to
	/// open. There should be a visible trashcan on each row to remove the agent
	/// with a confirmation. Aagents should have drag handles to reorder how
	/// they appear.")
	void test_settings_rows_select_open_remove_reorder()
	{
		auto programs = qt::agent_programs_panel{ };
		auto saved    = std::vector<std::string>{ };
		auto opened   = std::vector<std::string>{ };
		programs.set_dispatch({ .Update = [](core::agent_entry, qt::agent_programs_panel::answer aAnswer) { aAnswer(std::nullopt); },
								.Remove = [&saved](const std::uint32_t aId) { saved.push_back("remove:" + std::to_string(aId)); },
								.Move   = [&saved](const std::uint32_t aId, const int aDelta) { saved.push_back("move:" + std::to_string(aId) + ":" + std::to_string(aDelta)); } });
		programs.set_editor([&opened](const core::agent_entry& aStart, QWidget*) -> std::optional<core::agent_entry> {
			opened.push_back(aStart.Name);
			return std::nullopt;
		});
		auto preferences   = application::agent_setup_preferences{ };
		preferences.Agents = *core::add_agent_entry({ }, { .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/claude" });
		preferences.Agents = *core::add_agent_entry(preferences.Agents, { .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/codex" });
		preferences.Agents = *core::add_agent_entry(preferences.Agents, { .Name = "Harness", .Protocol = core::agent_protocol::external_terminal, .Program = "/opt/work/bin/harness" });
		programs.present(preferences);
		programs.resize(480, 900);
		programs.show();
		QApplication::processEvents();
		auto* const list     = programs.agent_list();
		auto* const viewport = list->viewport();
		const auto  middle   = [list](const int aRow) {
			const auto rect = list->visualItemRect(list->item(aRow));
			return QPoint{ rect.center().x(), rect.center().y() };
		};

		mouse_on(*viewport, middle(1), { QEvent::MouseButtonPress, QEvent::MouseButtonRelease });
		expect(list->currentRow() == 1 && opened.empty(), "a single click selects the agent and opens nothing");
		mouse_on(*viewport, middle(1), { QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::MouseButtonDblClick, QEvent::MouseButtonRelease });
		expect(opened == std::vector<std::string>{ "Work Codex" }, "a double-click opens the agent's sheet");
		key_on(*list, Qt::Key_Return);
		key_on(*list, Qt::Key_Enter, Qt::KeypadModifier);
		expect(opened == std::vector<std::string>{ "Work Codex", "Work Codex", "Work Codex" }, "Return and Enter open the selected agent");

		// A grip and a trash on every row, named for the agent.
		for (auto row = 0; row < 3; ++row) {
			auto* const grip  = programs.agent_grip(row);
			auto* const trash = programs.agent_trash(row);
			const auto  rect  = list->visualItemRect(list->item(row));
			expect(grip != nullptr && grip->isVisible() && rect.contains(grip->geometry()) && grip->geometry().left() == rect.left(), "each row has a visible grip at its left");
			expect(trash != nullptr && trash->isVisible() && rect.contains(trash->geometry()) && trash->geometry().right() > rect.center().x() && !trash->icon().isNull(), "each row has a visible trash at its right");
		}
		expect(programs.agent_grip(0)->accessibleName() == QStringLiteral("Reorder Work Claude") && programs.agent_trash(2)->accessibleName() == QStringLiteral("Remove Harness")
				   && programs.agent_trash(2)->toolTip() == QStringLiteral("Remove Harness") && programs.agent_grip(0)->accessibleDescription().contains(QStringLiteral("Alt+Up")),
			   "the grip and the trash are named for their agent, and the grip says how to move it from the keyboard");
		expect(programs.agent_grip(0)->testAttribute(Qt::WA_TransparentForMouseEvents), "a press on the grip reaches the list, which starts the drag");

		// The trash asks; Cancel keeps the agent, Remove agent removes it.
		programs.agent_trash(2)->click();
		auto* question = programs.remove_question();
		expect(question != nullptr && question->isVisible() && list->currentRow() == 2 && question->windowTitle().contains(QStringLiteral("Remove Harness?")) && saved.empty(),
			   "a row's trash selects it and asks before anything is removed");
		question->reject();
		QApplication::processEvents();
		expect(saved.empty(), "Cancel removes nothing");
		programs.agent_trash(0)->click();
		programs.remove_question()->accept();
		QApplication::processEvents();
		expect(saved == std::vector<std::string>{ "remove:1" }, "confirming removes the agent of the row whose trash was pressed");
		list->setCurrentRow(1);
		key_on(*list, Qt::Key_Delete);
		expect(programs.remove_question() != nullptr && programs.remove_question()->isVisible() && saved.size() == 1U, "Delete asks too");
		programs.remove_question()->reject();
		QApplication::processEvents();

		// Reordered from the keyboard and by a drop.
		list->setCurrentRow(1);
		key_on(*list, Qt::Key_Down, Qt::AltModifier);
		key_on(*list, Qt::Key_Up, Qt::AltModifier);
		list->setCurrentRow(0);
		key_on(*list, Qt::Key_Up, Qt::AltModifier);
		expect(saved == std::vector<std::string>{ "remove:1", "move:2:1", "move:2:-1" }, "Alt+Down and Alt+Up move the selected agent; the first goes no higher");
		saved.clear();
		static_cast<void>(list->model()->moveRow(QModelIndex{ }, 0, QModelIndex{ }, 3));
		expect(saved == std::vector<std::string>{ "move:1:1", "move:1:1" }, "a row dropped two places down is moved there, a place at a time");
		programs.present(preferences);
		expect(list->item(0)->data(Qt::UserRole).toULongLong() == 1U && programs.agent_grip(2) != nullptr && programs.agent_trash(3) == nullptr && programs.agent_grip(0)->accessibleName() == QStringLiteral("Reorder Work Claude"),
			   "the list shows the order the host keeps, and the handles follow the rows");
		list->setCurrentRow(1);
		render(programs, QStringLiteral("settings-rows"));
		programs.hide();
	}

	/// [[BPR-2318]] (owner, 2026-11-20: "Arguments in the agent settings isn't
	/// good, needs to show more than one row, needs drag handles, needs scroll
	/// bar").
	void test_arguments_editor()
	{
		auto dialog = qt::agent_entry_dialog{ core::agent_entry{ .Name = "Work", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/claude", .Arguments = { "--one", "--two", "--three" } } };
		dialog.resize(560, 900);
		dialog.show();
		QApplication::processEvents();
		auto* const editor = dialog.arguments();
		auto* const list   = editor->list();
		const auto  row    = list->visualItemRect(list->item(0)).height();
		expect(list->count() == 3 && row > 0 && list->viewport()->height() >= 5 * row - 1, "the arguments show one per row, and five rows fit");
		expect(list->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded && !list->verticalScrollBar()->isVisible(), "three arguments need no scroll bar");
		expect(editor->grip(0) != nullptr && editor->grip(0)->isVisible() && editor->grip(0)->accessibleName() == QStringLiteral("Reorder --one")
				   && editor->trash(1) != nullptr && editor->trash(1)->accessibleName() == QStringLiteral("Remove --two"),
			   "every argument has a grip and a trash named for it");
		list->setCurrentRow(2);
		key_on(*list, Qt::Key_Up, Qt::AltModifier);
		expect(dialog.entry().Arguments == std::vector<std::string>{ "--one", "--three", "--two" } && list->currentRow() == 1, "Alt+Up moves the selected argument up and keeps it selected");
		static_cast<void>(list->model()->moveRow(QModelIndex{ }, 0, QModelIndex{ }, 3));
		expect(dialog.entry().Arguments == std::vector<std::string>{ "--three", "--two", "--one" }, "a dropped argument takes its new place");
		editor->trash(1)->click();
		expect(dialog.entry().Arguments == std::vector<std::string>{ "--three", "--one" } && editor->trash(2) == nullptr, "a row's trash removes that argument");
		editor->add_button()->click();
		QApplication::processEvents();
		auto* const field = list->viewport()->findChild<QLineEdit*>();
		expect(field != nullptr && field->isVisible() && list->count() == 3 && list->currentRow() == 2, "Add argument adds a row at the end and edits it");
		field->setText(QStringLiteral("--model sonnet"));
		key_on(*field, Qt::Key_Return);
		QApplication::processEvents();
		expect(dialog.entry().Arguments == std::vector<std::string>{ "--three", "--one", "--model sonnet" }, "the typed argument is kept as written, spaces and all");
		editor->add_button()->click();
		QApplication::processEvents();
		auto* const empty = list->viewport()->findChild<QLineEdit*>();
		expect(empty != nullptr && list->count() == 4, "a second row is being edited");
		key_on(*empty, Qt::Key_Escape);
		QApplication::processEvents();
		QApplication::processEvents();
		expect(list->count() == 3, "a row left empty is removed");
		editor->set_arguments(std::vector<std::string>(12U, "--long"));
		dialog.adjustSize();
		QApplication::processEvents();
		expect(list->count() == 12 && list->verticalScrollBar()->isVisible() && list->verticalScrollBar()->maximum() > 0, "twelve arguments scroll");
		list->verticalScrollBar()->setValue(list->verticalScrollBar()->maximum());
		QApplication::processEvents();
		const auto lastRect = list->visualItemRect(list->item(11));
		expect(editor->grip(11)->geometry().top() == lastRect.top(), "the handles follow the rows as they scroll");
		editor->set_arguments({ "--dangerously-skip-permissions", "--model", "claude-opus-4-1", "--append-system-prompt", "Review only the diff", "--verbose", "--output-format" });
		list->setCurrentRow(1);
		render(dialog, QStringLiteral("arguments"));
		dialog.hide();
	}

	/// Owner, 0.5.0 build 75: Add argument drew over the second argument row.
	/// The dialog opens at its own size, as `exec` shows it; Add argument and
	/// its hint sit below every row the list shows, for one to four
	/// arguments and after a reorder, a removal and an addition.
	void test_add_argument_stays_below_the_rows()
	{
		const auto clearOfRows = [](qt::agent_entry_dialog& aDialog, const char* aWhen) {
			QApplication::processEvents();
			auto* const editor = aDialog.arguments();
			auto* const list   = editor->list();
			const auto  add    = QRect{ editor->add_button()->mapTo(&aDialog, QPoint{ }), editor->add_button()->size() };
			const auto  hint   = editor->findChild<QLabel*>(QStringLiteral("agentEntryArgumentsHint"));
			const auto  below  = QRect{ hint->mapTo(&aDialog, QPoint{ }), hint->size() };
			const auto  frame  = QRect{ list->mapTo(&aDialog, QPoint{ }), list->size() };
			auto        clear  = !add.intersects(frame) && !below.intersects(frame) && add.height() >= editor->add_button()->sizeHint().height();
			for (auto row = 0; row < list->count(); ++row) {
				const auto rect = list->visualItemRect(list->item(row)).translated(list->viewport()->mapTo(&aDialog, QPoint{ }));
				clear           = clear && !add.intersects(rect) && !below.intersects(rect);
			}
			expect(clear, std::string{ "Add argument and its hint sit below the argument rows " } + aWhen + " (" + std::to_string(list->count()) + " rows, list " + std::to_string(frame.top()) + "+" + std::to_string(frame.height()) + ", add " + std::to_string(add.top()) + "+" + std::to_string(add.height()) + ", hint " + std::to_string(below.top()) + "+" + std::to_string(below.height()) + ")");
		};
		for (const auto count : { 0U, 1U, 2U, 3U, 4U }) {
			auto arguments = std::vector<std::string>{ };
			for (auto at = 0U; at < count; ++at) {
				arguments.push_back("--argument-" + std::to_string(at + 1U));
			}
			auto dialog = qt::agent_entry_dialog{ core::agent_entry{ .Name = "Work", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/claude", .Arguments = arguments } };
			dialog.show();
			clearOfRows(dialog, "when the dialog opens");
			if (count >= 2U) {
				dialog.arguments()->list()->setCurrentRow(1);
				key_on(*dialog.arguments()->list(), Qt::Key_Up, Qt::AltModifier);
				clearOfRows(dialog, "after a reorder");
				dialog.arguments()->trash(0)->click();
				clearOfRows(dialog, "after a removal");
			}
			dialog.arguments()->add_button()->click();
			clearOfRows(dialog, "while a new row is edited");
			dialog.hide();
		}
	}

	/// [[BPR-2215]] (owner, 0.5.0 build 76: "Preparing workspace… 0:12").
	/// While the workspace is prepared the setup says for how long, from the
	/// instant the host stamped, on a test clock: it ticks,
	/// keeps its start when the tab is left and shown again, and goes when
	/// the preparation ends. Its accessible name is the words alone.
	void test_preparing_workspace_shows_its_time()
	{
		const auto wait = [](const int aMilliseconds) {
			auto loop = QEventLoop{ };
			QTimer::singleShot(aMilliseconds, &loop, &QEventLoop::quit);
			loop.exec();
		};
		auto now  = std::int64_t{ 5'000'000 };
		auto test = fixture{ };
		test.View.set_clock([&now] { return now; });
		test.Setup.State.Rows              = { row(core::setup_agent::claude_code, "/opt/homebrew/bin/claude", core::agent_found_how::on_path) };
		test.Setup.State.Chosen            = core::setup_agent::claude_code;
		test.Clones.Linked                 = application::repository_link{ .Host = "github.com", .Repository = 7U, .Owner = "bpr-fixtures", .Name = "engine", .Path = "/Users/me/Documents/GitHub/engine", .Remote = "origin" };
		test.Setup.State.RepositoryChosen  = true;
		test.Setup.State.AgentCheck        = { .State = core::setup_check_state::passed };
		test.Setup.State.WorkspaceCheck    = { .State = core::setup_check_state::running };
		test.Setup.WorkspaceCheckStartedMs = now - 12'400;
		test.View.resize(520, 600);
		test.View.show();
		test.show();
		const auto label = [&test] { return dynamic_cast<qt::elapsed_words_label*>(test.View.findChild<QLabel*>(QStringLiteral("agentSetupRunning"))); };
		expect(label() != nullptr && label()->text() == QStringLiteral("Preparing workspace\u2026 0:12") && label()->accessibleName() == QStringLiteral("Preparing workspace\u2026") && label()->ticking(),
			   "preparing the workspace says how long it has taken, and its name is the words alone");
		now += 1'000;
		wait(800);
		expect(label()->text() == QStringLiteral("Preparing workspace\u2026 0:13") && label()->accessibleName() == QStringLiteral("Preparing workspace\u2026"), "it ticks once a second: " + label()->text().toStdString());
		test.View.hide();
		QApplication::processEvents();
		expect(!label()->ticking(), "it stops ticking while the tab is not shown");
		now += 60'000;
		test.View.show();
		test.show();
		expect(label()->text() == QStringLiteral("Preparing workspace\u2026 1:13") && label()->ticking(), "back on the tab, it keeps the host's start: " + label()->text().toStdString());
		test.Setup.WorkspaceCheckStartedMs = now - (3'600'000 + 61'000);
		test.show();
		expect(label()->text() == QStringLiteral("Preparing workspace\u2026 1:01:01"), "an hour reads as h:mm:ss: " + label()->text().toStdString());
		test.Setup.State.WorkspaceCheck = { .State = core::setup_check_state::passed };
		test.Setup.WorkspaceCheckStartedMs.reset();
		test.show();
		expect(label() == nullptr, "the time goes when the preparation ends");
		test.View.hide();
	}

	void test_settings_offers_the_prompt_form()
	{
		auto programs = qt::agent_programs_panel{ };
		programs.resize(560, 400);
		auto updated = std::vector<core::agent_entry>{ };
		auto checked = std::vector<std::uint32_t>{ };
		programs.set_dispatch({ .CheckEntry = [&checked](const std::uint32_t aId) { checked.push_back(aId); },
								.Update     = [&updated](core::agent_entry aEntry, const qt::agent_programs_panel::answer& aAnswer) {
                                    updated.push_back(std::move(aEntry));
                                    aAnswer(std::nullopt); } });
		auto preferences   = application::agent_setup_preferences{ };
		preferences.Agents = *core::add_agent_entry({ }, core::agent_entry{ .Name = "Launcher Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/launcher" });
		const auto id      = preferences.Agents.front().Id;
		programs.present(preferences, { });
		programs.show();
		QApplication::processEvents();
		auto check                     = qt::agent_check_presentation{ };
		check.Setup.Preferences        = preferences;
		check.Setup.Detected           = true;
		check.Setup.ChecksSignIn       = false;
		check.Setup.State.Chosen       = core::setup_agent::codex;
		check.Setup.State.AgentCheck   = core::fail_check(core::agent_setup_failure::instructions_dropped);
		check.Setup.InstructionsSwitch = id;
		check.Decision                 = { .Offer = core::agent_check_offer::available };
		programs.present_check(check);
		expect(programs.switch_instructions_button()->isVisibleTo(&programs) && programs.check_result_text().contains(QStringLiteral("At the start of the prompt")),
			   "the failure says the developer instructions were ignored and offers the prompt form");
		programs.switch_instructions_button()->click();
		expect(updated.size() == 1U && updated.front().Id == id && updated.front().Instructions == core::agent_instruction_delivery::message && checked == std::vector{ id },
			   "the button switches that agent to the start of the prompt and checks it again");
		check.Setup.State.AgentCheck = core::fail_check(core::agent_setup_failure::timed_out);
		check.Setup.InstructionsSwitch.reset();
		programs.present_check(check);
		expect(!programs.switch_instructions_button()->isVisibleTo(&programs), "any other failure offers no switch");

		const auto unreadable = qt::setup_failure_words(core::agent_setup_failure::output_unreadable, QStringLiteral("Launcher Codex"), QStringLiteral("version: launcher 0.6.2\nprotocol_failure"));
		expect(unreadable.contains(QStringLiteral("update the launcher")) && unreadable.contains(QStringLiteral("launcher 0.6.2")) && !unreadable.contains(QStringLiteral("protocol_failure")),
			   "an unreadable launcher is named with its version and the fix, not a decoder error");
		expect(!qt::setup_failure_words(core::agent_setup_failure::output_unreadable, QStringLiteral("Launcher Codex"), QStringLiteral("protocol_failure")).contains(QStringLiteral("says it is")),
			   "with no version printed none is claimed");
	}

	/// [[BPR-1859]]. Owner, 2026-11-04: "I want to be able to run agent
	/// check from agent settings page". Check agent on Settings: an intent,
	/// disabled with its reason, running with Stop and progress, and the
	/// result inline: a pass, or the failure, its remedy and its details.
	void test_settings_check_agent();

	/// \brief [[BPR-2348]]. A custom entry with its own Program never reads
	/// "Not found" (a result an earlier build kept reads as a failed check),
	/// while an entry that relies on the search still does; and a custom
	/// entry's failed check shows, in its details, the program it ran.
	void test_custom_entry_check_names_its_program()
	{
		auto programs = qt::agent_programs_panel{ };
		programs.resize(560, 420);
		programs.set_dispatch({ .Check = [] { }, .StopCheck = [] { } });
		auto preferences        = application::agent_setup_preferences{ };
		preferences.Agents      = *core::add_agent_entry({ }, { .Name = "Launcher Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/corp/bin/launcher", .Arguments = { "--codex" } });
		preferences.Agents      = *core::add_agent_entry(preferences.Agents, core::preset_agent_entry(core::setup_agent::codex, core::codex_protocol_choice::app_server, { }));
		preferences.EntryChecks = { { .Id = preferences.Agents[0].Id, .Check = core::fail_check(core::agent_setup_failure::agent_not_found) },
									{ .Id = preferences.Agents[1].Id, .Check = core::fail_check(core::agent_setup_failure::agent_not_found) } };
		programs.present(preferences, { });
		programs.show();
		QApplication::processEvents();
		expect(!programs.roster_row_text(0).contains(QStringLiteral("Not found")) && programs.roster_row_text(0).contains(QStringLiteral("Check failed")),
			   "a custom entry with its own Program never reads Not found: " + programs.roster_row_text(0).toStdString());
		expect(programs.roster_row_text(1).contains(QStringLiteral("Not found")), "a popular agent that relies on the search still can: " + programs.roster_row_text(1).toStdString());

		auto check                   = qt::agent_check_presentation{ };
		check.Setup.Preferences      = preferences;
		check.Setup.Detected         = true;
		check.Setup.State.Chosen     = core::setup_agent::codex;
		check.Setup.State.AgentCheck = core::fail_check(core::agent_setup_failure::status_unreadable);
		check.Setup.CheckedEntry     = preferences.Agents[0].Id;
		check.Setup.CheckDetail      = "Ran: /opt/corp/bin/launcher --codex …and the check's own\nnonzero_exit (exit 2)\nstderr:\nERROR: --no-extra-output must be the first argument";
		check.Decision               = { .Offer = core::agent_check_offer::available };
		programs.present_check(check);
		auto* const details = programs.findChild<QToolButton*>(QStringLiteral("settingsAgentCheckDetails"));
		auto* const text    = programs.findChild<QPlainTextEdit*>(QStringLiteral("settingsAgentCheckDetailsText"));
		expect(details != nullptr && text != nullptr, "the failed check has details");
		details->click();
		expect(text->toPlainText().startsWith(QStringLiteral("Ran: /opt/corp/bin/launcher --codex")), "its details name the program it ran first: " + text->toPlainText().toStdString());
		programs.hide();
	}

	void test_settings_check_agent()
	{
		auto programs = qt::agent_programs_panel{ };
		programs.resize(560, 400);
		auto pressed = std::vector<std::string>{ };
		programs.set_dispatch({ .Check = [&pressed] { pressed.emplace_back("check"); }, .StopCheck = [&pressed] { pressed.emplace_back("stop"); } });
		auto preferences   = application::agent_setup_preferences{ };
		preferences.Chosen = core::setup_agent::claude_code;
		preferences.Agents = *core::add_agent_entry({ }, core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { }));
		programs.present(preferences, { { .Agent = core::setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude" } });
		programs.show();
		QApplication::processEvents();
		const auto shown = [&programs](const QWidget* aWidget) { return aWidget != nullptr && aWidget->isVisibleTo(&programs); };

		// Nothing added: disabled, and the line says what to do.
		auto check = qt::agent_check_presentation{ };
		programs.present_check(check);
		expect(!programs.check_button()->isEnabled() && programs.check_note()->text() == QStringLiteral("Add an agent, then check it here.") && !shown(programs.stop_check_button()),
			   "with no agent added Check agent is disabled and says why");
		check.Decision.Offer = core::agent_check_offer::not_chosen;
		programs.present_check(check);
		expect(!programs.check_button()->isEnabled() && programs.check_note()->text().contains(QStringLiteral("Choose one in the Agent tab's setup")), "several found, none chosen: choose first");

		// Offered, with no review open: the agent alone.
		auto& setup        = check.Setup;
		setup.Preferences  = preferences;
		setup.Detected     = true;
		setup.State.Chosen = core::setup_agent::claude_code;
		setup.State.Rows   = { { .Agent = core::setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude", .Verdict = { .Permitted = true } } };
		check.Decision     = { .Offer = core::agent_check_offer::available, .ChecksWorkspace = false };
		programs.present_check(check);
		// [[BPR-2051]]: the roster's Check all, which checks each agent.
		expect(programs.check_button()->isEnabled() && programs.check_button()->text() == QStringLiteral("Check all"), "a chosen agent can be checked");
		expect(programs.check_note()->text() == QStringLiteral("Runs claude auth status, which sends no question and no code. Open a pull request to check its workspace too."),
			   "the line says what runs, and that the workspace needs an open pull request");
		expect(programs.check_result_text().isEmpty(), "no rows before a check has run");
		programs.check_button()->click();
		expect(pressed == std::vector<std::string>{ "check" }, "Check agent is an intent");
		programs.resize(560, programs.sizeHint().height());
		render(programs, QStringLiteral("settings-check-offered"));

		// Running: disabled with its reason, Stop and progress.
		setup.Checking                   = true;
		setup.State.AgentCheck.State     = core::setup_check_state::running;
		setup.State.WorkspaceCheck.State = core::setup_check_state::running;
		check.Decision                   = { .Offer = core::agent_check_offer::running };
		programs.present_check(check);
		auto* const progress = programs.findChild<QProgressBar*>(QStringLiteral("settingsAgentCheckProgress"));
		expect(!programs.check_button()->isEnabled() && programs.check_button()->toolTip() == QStringLiteral("A check is running. Stop it to start again.") && shown(programs.stop_check_button())
				   && shown(progress) && programs.check_note()->text() == QStringLiteral("Checking Claude Code and preparing the workspace…"),
			   "a running check is not started again, shows its progress and offers Stop");
		expect(progress->accessibleName() == programs.check_note()->text(), "the progress is named for what runs");
		programs.stop_check_button()->click();
		expect(pressed.back() == "stop", "Stop is an intent");
		programs.resize(560, programs.sizeHint().height());
		render(programs, QStringLiteral("settings-check-running"));

		// Failed: not signed in, its remedy and the command to copy, details.
		setup.Checking                   = false;
		setup.State.AgentCheck           = core::fail_check(core::agent_setup_failure::not_signed_in);
		setup.State.WorkspaceCheck.State = core::setup_check_state::passed;
		setup.CheckDetail                = "success (exit 0)\n{\"loggedIn\": false}";
		check.Decision                   = { .Offer = core::agent_check_offer::available, .ChecksWorkspace = true };
		programs.present_check(check);
		const auto failed = programs.check_result_text();
		expect(failed.contains(QStringLiteral("\u2715 Claude Code is signed in")) && failed.contains(QStringLiteral("\u2713 The open pull request's workspace can be made"))
				   && failed.contains(QStringLiteral("Claude Code isn't signed in. Sign in from a terminal, then check again.")) && failed.contains(QStringLiteral("claude auth login")),
			   "a failure says what failed, what to do and the command: " + failed.toStdString());
		expect(programs.check_button()->isEnabled() && programs.check_button()->text() == QStringLiteral("Check all"), "it can be run again");
		auto* const details = programs.findChild<QToolButton*>(QStringLiteral("settingsAgentCheckDetails"));
		auto* const text    = programs.findChild<QPlainTextEdit*>(QStringLiteral("settingsAgentCheckDetailsText"));
		expect(shown(details) && !shown(text), "its details wait behind Show details");
		details->click();
		expect(shown(text) && text->toPlainText().contains(QStringLiteral("loggedIn")), "Show details shows the check's own output");
		// [[BPR-2215]]. Coloured stderr shows as plain text here too.
		setup.CheckDetail = "nonzero_exit (exit 1)\nstderr:\n\x1b[0;31merror:\x1b[0m not signed in";
		programs.present_check(check);
		expect(text->toPlainText() == QStringLiteral("nonzero_exit (exit 1)\nstderr:\nerror: not signed in"), "Settings' details show coloured output as plain text: " + text->toPlainText().toStdString());
		setup.CheckDetail = "success (exit 0)\n{\"loggedIn\": false}";
		programs.present_check(check);
		programs.resize(560, programs.sizeHint().height());
		render(programs, QStringLiteral("settings-check-failed"));

		// Checked again after signing in: a pass with its version.
		setup.State.AgentCheck = { .State = core::setup_check_state::passed, .Version = "2.1.300" };
		setup.CheckDetail.clear();
		programs.present_check(check);
		const auto passed = programs.check_result_text();
		expect(passed == QStringLiteral("\u2713 Claude Code is signed in, version 2.1.300\n\u2713 The open pull request's workspace can be made"), "a pass says so: " + passed.toStdString());
		expect(!shown(details) && !shown(text), "a pass has no details to show");
		programs.resize(560, programs.sizeHint().height());
		render(programs, QStringLiteral("settings-check-passed"));

		// A workspace that could not be made says why.
		setup.State.WorkspaceCheck = core::fail_check(core::agent_setup_failure::workspace_unavailable);
		programs.present_check(check);
		expect(programs.check_result_text().contains(QStringLiteral("The agent's workspace could not be made.")) && shown(details), "a workspace failure says so, with its log");

		// Forbidden by the open review's host.
		check.Decision = { .Offer = core::agent_check_offer::forbidden };
		programs.present_check(check);
		expect(!programs.check_button()->isEnabled() && programs.check_note()->text().contains(QStringLiteral("doesn't allow Claude Code")), "a forbidden agent says so");
		programs.hide();
	}

	/// \brief [[BPR-2254]]. Saved agents that could not be read are never
	/// shown as an empty list: Settings › Agents says so in the list's place,
	/// naming the file and offering Troubleshooting; Add agent… is refused
	/// with that reason before anything is asked; and the footer says so in
	/// place of Set up an agent….
	void test_settings_unreadable()
	{
		const auto file     = QStringLiteral("/Users/me/Library/Application Support/BetterPullReview/state/v1/agent-setup.sqlite3");
		auto       programs = qt::agent_programs_panel{ };
		programs.resize(560, 420);
		auto pressed = std::vector<std::string>{ };
		programs.set_dispatch({ .Add =
									[&pressed](core::agent_entry, qt::agent_programs_panel::answer aAnswer) {
										pressed.emplace_back("add");
										aAnswer(core::agent_entry_refusal::settings_unreadable);
									},
								.OpenTroubleshooting = [&pressed] { pressed.emplace_back("troubleshooting"); } });
		auto asked = 0;
		programs.set_editor([&asked](const core::agent_entry& aStart, QWidget*) -> std::optional<core::agent_entry> {
			++asked;
			return aStart;
		});
		programs.present(application::agent_setup_preferences{ }, { { .Agent = core::setup_agent::claude_code, .Path = "/opt/homebrew/bin/claude" } });
		programs.show();
		QApplication::processEvents();
		expect(programs.agent_list()->isVisibleTo(&programs) && !programs.unreadable_notice()->isVisibleTo(&programs) && programs.add_button()->isEnabled(),
			   "read settings show the list and offer Add agent…");

		programs.present_settings_unreadable(file);
		QApplication::processEvents();
		expect(programs.unreadable_notice()->isVisibleTo(&programs) && !programs.agent_list()->isVisibleTo(&programs), "unreadable settings show the failure in the list's place, never an empty list");
		expect(programs.unreadable_text()->text().contains(QDir::toNativeSeparators(file)) && programs.unreadable_text()->text().contains(QStringLiteral("Nothing in the file was changed"))
				   && programs.unreadable_text()->text().contains(QStringLiteral("Clear local data")),
			   "the notice names the file, says nothing was changed and where to recover");
		expect(!programs.add_button()->isEnabled() && !programs.run_limit_toggle()->isEnabled() && !programs.scratch_limit()->isEnabled() && !programs.check_button()->isEnabled()
				   && !programs.findChild<QPushButton*>(QStringLiteral("settingsResetAgents"))->isEnabled(),
			   "no change is offered while nothing could be kept, Reset agents… included");
		programs.add_agent(std::nullopt);
		expect(asked == 0 && pressed.empty() && programs.status()->text() == QString::fromUtf8(core::describe(core::agent_entry_refusal::settings_unreadable).data()),
			   "Add agent… is refused with the reason before anything is asked");
		click(*programs.troubleshooting_button());
		expect(pressed == std::vector<std::string>{ "troubleshooting" }, "Open Troubleshooting… leads to the existing recovery");
		// A publication of the same defaults keeps the notice in place.
		programs.present(application::agent_setup_preferences{ }, { });
		expect(programs.unreadable_notice()->isVisibleTo(&programs) && !programs.agent_list()->isVisibleTo(&programs) && !programs.add_button()->isEnabled(), "and every later presentation keeps it");
		render(programs, QStringLiteral("settings-agents-unreadable"));
		programs.present_settings_unreadable({ });
		expect(programs.agent_list()->isVisibleTo(&programs) && !programs.unreadable_notice()->isVisibleTo(&programs) && programs.add_button()->isEnabled(), "settings that read again show the list again");

		const auto words = qt::setup_failure_words(core::agent_setup_failure::settings_unreadable, QStringLiteral("Claude Code"), file);
		expect(words.contains(QDir::toNativeSeparators(file)) && words.contains(QStringLiteral("couldn't be read")) && !words.contains(QStringLiteral("isn't where it was found")),
			   "a refused check names the file, never an agent that was not found");

		auto launch = qt::agent_launch_panel{ };
		auto footer = qt::agent_footer{ std::make_shared<qt::agent_run_settings_source>(launch) };
		footer.resize(640, 40);
		footer.show();
		QApplication::processEvents();
		footer.present_settings_unreadable(file);
		QApplication::processEvents();
		expect(footer.settings_button()->isVisibleTo(&footer) && footer.settings_button()->text() == QStringLiteral("Saved agents couldn't be read…")
				   && footer.settings_button()->toolTip().contains(QDir::toNativeSeparators(file)),
			   "the footer says the saved agents couldn't be read, not Set up an agent…");
		footer.present_settings_unreadable({ });
		QApplication::processEvents();
		expect(footer.settings_button()->text() == QStringLiteral("Set up an agent…"), "and goes back to Set up an agent… when they read");
		programs.hide();
	}

	/// [[BPR-2303]]. Writes a fixture app bundle (used on macOS only): `aInfo` as its Info.plist
	/// when not empty, and each of `aPrograms` in Contents/MacOS, executable
	/// unless its name starts with `-`.
	[[nodiscard, maybe_unused]] auto make_bundle(const QString& aBundle, const QByteArray& aInfo, const QStringList& aPrograms) -> QString
	{
		expect(QDir{ }.mkpath(aBundle + QStringLiteral("/Contents/MacOS")), "the fixture bundle's folders are made");
		if (!aInfo.isEmpty()) {
			auto info = QFile{ aBundle + QStringLiteral("/Contents/Info.plist") };
			expect(info.open(QIODevice::WriteOnly) && info.write(aInfo) == aInfo.size(), "the fixture Info.plist is written");
		}
		for (const auto& name : aPrograms) {
			const auto plain = name.startsWith(QLatin1Char{ '-' });
			auto       file  = QFile{ aBundle + QStringLiteral("/Contents/MacOS/") + (plain ? name.mid(1) : name) };
			expect(file.open(QIODevice::WriteOnly) && file.write("#!/bin/sh\n") > 0, "a fixture program is written");
			file.close();
			if (!plain) {
				expect(file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser), "a fixture program can run");
			}
		}
		return aBundle;
	}

	[[nodiscard, maybe_unused]] auto info_naming(const char* aProgram) -> QByteArray
	{
		return QByteArray{ R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict><key>CFBundleIdentifier</key><string>com.example.work</string><key>CFBundleExecutable</key><string>)" }
		+ aProgram + QByteArray{ "</string></dict></plist>\n" };
	}

	/// [[BPR-2303]] (owner, 2026-11-19: "The executable I need to point at is
	/// inside of an app bundle"). Choose… or a typed app gives the app's own
	/// program, read by the host; an app that names none it holds is refused.
	void test_program_from_an_app_bundle()
	{
		auto unread = qt::agent_entry_dialog{ core::agent_entry{ .Name = "Work", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/Applications/Work Agent.app" } };
		expect(unread.entry().Program == "/Applications/Work Agent.app" && unread.program()->text() == QStringLiteral("/Applications/Work Agent.app"),
			   "with no bundle reader, as on Windows and Linux, a .app path is kept as written");
#if defined(Q_OS_MACOS)
		const auto root = QTemporaryDir{ };
		expect(root.isValid(), "a scratch folder for the fixture bundles");
		const auto work     = make_bundle(root.path() + QStringLiteral("/Work Agent.app"), info_naming("work-agent"), { QStringLiteral("work-agent"), QStringLiteral("work-helper") });
		const auto bare     = make_bundle(root.path() + QStringLiteral("/Bare.app"), { }, { QStringLiteral("bare"), QStringLiteral("-README") });
		const auto crowded  = make_bundle(root.path() + QStringLiteral("/Crowded.app"), { }, { QStringLiteral("one"), QStringLiteral("two") });
		const auto misnamed = make_bundle(root.path() + QStringLiteral("/Misnamed.app"), info_naming("gone"), { QStringLiteral("here") });
		auto       reads    = 0;
		auto       chosen   = QString{ };
		auto       started  = QString{ };
		auto       host     = qt::agent_program_host{ .Choose =
														  [&chosen, &started](QWidget*, const QString&, const QString& aStart) {
													started = aStart;
													return chosen;
												},
											.ReadBundle =
												[&reads](const std::string& aBundle) {
													++reads;
													return adapters::workspace::read_app_bundle(aBundle);
												} };
		auto       dialog    = qt::agent_entry_dialog{ core::agent_entry{ .Name = "Work", .Protocol = core::agent_protocol::claude_stream_json }, nullptr, host };
		const auto ok        = [&dialog] { return dialog.buttons()->button(QDialogButtonBox::Ok)->isEnabled(); };
		const auto workAgent = work + QStringLiteral("/Contents/MacOS/work-agent");

		chosen = work;
		dialog.choose_button()->click();
		expect(dialog.program()->text() == workAgent && dialog.entry().Program == workAgent.toStdString() && ok() && !dialog.problem()->isVisibleTo(&dialog),
			   "choosing an app fills in the program its Info.plist names, not its other one");
		chosen = work + QStringLiteral("/Contents/MacOS/work-helper");
		dialog.choose_button()->click();
		expect(started == workAgent && dialog.program()->text() == chosen && dialog.entry().Program == chosen.toStdString(), "a program chosen inside an app is kept as chosen");
		chosen = bare;
		dialog.choose_button()->click();
		expect(dialog.program()->text() == bare + QStringLiteral("/Contents/MacOS/bare") && ok(), "with no Info.plist, the app's one program");
		chosen.clear();
		dialog.choose_button()->click();
		expect(dialog.program()->text() == bare + QStringLiteral("/Contents/MacOS/bare"), "a cancelled chooser changes nothing");

		dialog.program()->setText(work + QLatin1Char{ '/' });
		expect(dialog.entry().Program == workAgent.toStdString() && ok(), "a typed or pasted app is taken as its program even before the field is left");
		emit dialog.program()->editingFinished();
		expect(dialog.program()->text() == workAgent, "and the field shows that program once it is left");

		chosen = crowded;
		dialog.choose_button()->click();
		expect(dialog.program()->text() == crowded && !ok() && dialog.problem()->text().contains(QStringLiteral("Crowded.app has more than one program")),
			   "an app with two programs and no name is refused, saying to choose one inside it");
		dialog.program()->setText(misnamed);
		expect(!ok() && dialog.problem()->text().contains(QStringLiteral("Misnamed.app names a main program that isn't in its Contents/MacOS folder")), "a name the app does not hold is refused");
		expect(QDir{ crowded + QStringLiteral("/Contents/MacOS") }.removeRecursively(), "the fixture's programs folder is removed");
		dialog.program()->setText(QString{ });
		dialog.program()->setText(crowded);
		expect(!ok() && dialog.problem()->text().contains(QStringLiteral("Crowded.app has no Contents/MacOS folder")), "an app with no programs folder is refused");
		const auto before = reads;
		dialog.program()->setText(root.path() + QStringLiteral("/Missing.app"));
		expect(reads == before + 1 && dialog.entry().Program == (root.path() + QStringLiteral("/Missing.app")).toStdString(), "a .app that is not a folder is left for the program check, as before");
		dialog.program()->setText(QStringLiteral("/usr/local/bin/work-claude"));
		expect(reads == before + 1 && ok(), "a path that names no app is never read");
#endif
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
	test_setup_waits_until_a_step_is_known_missing();
	test_no_agent_found();
	test_two_agents_and_a_refused_one();
	test_host_policy_states();
	test_change_keeps_the_current_agent();
	test_the_list_follows_settings();
	test_repository_states();
	test_check_and_ready();
	test_settings_rows();
	test_settings_rows_select_open_remove_reorder();
	test_arguments_editor();
	test_add_argument_stays_below_the_rows();
	test_preparing_workspace_shows_its_time();
	test_settings_check_agent();
	test_custom_entry_check_names_its_program();
	test_settings_offers_the_prompt_form();
	test_settings_unreadable();
	test_program_from_an_app_bundle();
	std::cout << "agent-setup-view-tests:pass\n";
	return EXIT_SUCCESS;
}
