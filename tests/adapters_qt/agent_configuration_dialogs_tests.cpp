/// \file
/// \brief [[BPR-2017]], RFC-BPR-0107. Settings' Import configuration… and
/// Export configuration…: the share row on the Agents roster, the export's
/// choice and exact text, and the import preview's bindings, differences and
/// defaults, driven by keyboard where a person would. `--render DIR` saves
/// each surface (native and light/dark under `BPR_NATIVE_LOOK`).

#include "bpr/application/agent_configuration_exchange.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/qt/agent_configuration_dialogs.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/prompts_page.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleHints>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// Space pressed and released on a focused control, as a keyboard does.
	void press_space(QWidget* aWidget)
	{
		aWidget->setFocus();
		auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto sender() -> application::agent_configuration_snapshot
	{
		auto snapshot               = application::agent_configuration_snapshot{ };
		auto work                   = core::agent_entry{ .Id = 1U, .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/Users/me/bin/work-claude" };
		work.Arguments              = { "--profile", "work" };
		work.Model                  = "opus";
		work.StrictRunChecks        = false;
		work.BuildsAndTests         = true;
		work.ScratchStarts          = false;
		work.Identity               = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
		auto lint                   = core::agent_entry{ .Id = 2U, .Name = "Lint bot", .Protocol = core::agent_protocol::external_terminal, .Program = "/opt/tools/lint-bot" };
		lint.Identity               = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
		snapshot.Agents.Agents      = { work, lint };
		snapshot.Hooks.Overrides[0] = "Say what is wrong today in two sentences. /team-style";
		snapshot.WrittenAgainst[0]  = std::string{ application::writing_hook_version };
		return snapshot;
	}

	void render(QWidget& aWidget, const std::string& aRoot, const QString& aName)
	{
		if (aRoot.empty()) {
			return;
		}
		const auto native = qEnvironmentVariable("BPR_NATIVE_LOOK") == QStringLiteral("1");
		const auto scheme = native ? qEnvironmentVariable("BPR_NATIVE_LOOK_SCHEME", QStringLiteral("light")) : QStringLiteral("offscreen");
		if (native) {
			QGuiApplication::styleHints()->setColorScheme(scheme == QStringLiteral("dark") ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
		}
		aWidget.show();
		settle();
		expect(aWidget.grab().save(QString::fromStdString(aRoot) + QLatin1Char('/') + aName + QLatin1Char('-') + scheme + QStringLiteral(".png")), "render saved");
	}

	void test_share_row(const std::string& aRoot)
	{
		auto panel   = qt::agent_programs_panel{ };
		auto imports = 0;
		auto exports = 0;
		panel.set_dispatch({ .ImportConfiguration = [&imports] { ++imports; }, .ExportConfiguration = [&exports] { ++exports; } });
		auto preferences   = application::agent_setup_preferences{ };
		preferences.Agents = sender().Agents.Agents;
		panel.present(preferences);
		auto* const reset = panel.findChild<QPushButton*>(QStringLiteral("settingsResetAgents"));
		expect(panel.import_button() != nullptr && panel.export_button() != nullptr && reset != nullptr, "the share row's buttons and Reset agents…");
		expect(panel.import_button()->text() == QStringLiteral("Import configuration…") && panel.export_button()->text() == QStringLiteral("Export configuration…"), "RFC-BPR-0107's names");
		panel.resize(640, 900);
		panel.show();
		settle();
		expect(panel.import_button()->mapTo(&panel, QPoint{ }).y() < reset->mapTo(&panel, QPoint{ }).y(), "the row sits above Remove every agent and set up again");
		expect(panel.import_button()->focusPolicy() & Qt::TabFocus, "Import is reached by Tab");
		press_space(panel.import_button());
		panel.export_button()->click();
		expect(imports == 1 && exports == 1, "each button opens its own dialog");
		panel.present_exchange(QStringLiteral("Imported 2 agents and 1 prompts."));
		expect(panel.exchange_note()->isVisible(), "what the last import did is said under the row");
		render(panel, aRoot, QStringLiteral("share-row"));
	}

	[[nodiscard]] auto test_export(const std::string& aRoot) -> std::string
	{
		auto unsaved = std::array<bool, core::writing_hook_count + 1U>{ };
		unsaved[1]   = true;
		auto dialog  = qt::agent_configuration_export_dialog{ sender(), unsaved, "BetterPullReview test" };
		expect(dialog.agents() == std::vector<std::uint32_t>{ 1U, 2U }, "every agent starts chosen");
		auto prompts = dialog.prompts();
		expect(prompts.size() == 1U && prompts[0].Purpose == "tour.problem" && !prompts[0].BuiltIn, "the edited prompt starts chosen; built-in ones do not");
		const auto& text = dialog.bytes();
		expect(text.find("/Users/me") == std::string::npos && text.find("aaaaaaaa") == std::string::npos, "no path or identity in the exact text");
		expect(dialog.exact_text()->toPlainText().toStdString() == text && dialog.exact_text()->isReadOnly(), "the exact text shown is what is written");
		expect(text.find("\"builds_and_tests\": true") != std::string::npos && text.find("\"scratch_starts\": false") != std::string::npos, "both RFC-BPR-0104 settings ride along");
		// A prompt with unsaved edits cannot be chosen.
		auto* unsavedRow = static_cast<QTreeWidgetItem*>(nullptr);
		for (QTreeWidgetItemIterator it{ dialog.items() }; *it != nullptr; ++it) {
			if ((*it)->text(0) == QStringLiteral("Solution summary")) {
				unsavedRow = *it;
			}
		}
		expect(unsavedRow != nullptr && !unsavedRow->flags().testFlag(Qt::ItemIsEnabled) && unsavedRow->text(1).contains(QStringLiteral("not saved")), "unsaved edits are named, not substituted");
		dialog.all_prompts_button()->click();
		prompts = dialog.prompts();
		expect(prompts.size() == core::writing_hook_count, "every prompt but the unsaved one: the seven others and the review skill");
		expect(std::ranges::count_if(prompts, &application::exported_prompt::BuiltIn) == static_cast<long>(core::writing_hook_count - 1U), "the shipped defaults go as built-in");
		expect(dialog.bytes().find("\"origin\": \"built-in\"") != std::string::npos, "marked built-in in the file");
		dialog.label_field()->setText(QStringLiteral("Team"));
		expect(dialog.bytes().find("\"label\": \"Team\"") != std::string::npos, "the label is in the text");
		render(dialog, aRoot, QStringLiteral("export"));
		dialog.set_file_chooser([] { return QStringLiteral("/tmp/agents.json"); });
		dialog.export_button()->click();
		expect(dialog.result() == QDialog::Accepted && dialog.chosen_path() == QStringLiteral("/tmp/agents.json"), "Export… asks where and closes");
		return dialog.bytes();
	}

	/// A label's words without the break opportunities a path is given.
	[[nodiscard]] auto plain(const QLabel* aLabel) -> QString { return aLabel->text().remove(QChar{ 0x200B }); }

	/// The list row of an item, by the name it starts with.
	[[nodiscard]] auto row_named(qt::agent_configuration_import_dialog& aDialog, const QString& aName) -> QListWidgetItem*
	{
		for (int row = 0; row < aDialog.items()->count(); ++row) {
			if (aDialog.items()->item(row)->text().startsWith(aName + QStringLiteral("   "))) {
				return aDialog.items()->item(row);
			}
		}
		return nullptr;
	}

	/// The shown item's choice part with these words.
	[[nodiscard]] auto part(qt::agent_configuration_import_dialog& aDialog, const QString& aText) -> QToolButton*
	{
		for (auto* const button : aDialog.choice_buttons()) {
			if (button->text() == aText) {
				return button;
			}
		}
		return nullptr;
	}

	/// Nothing in the detail pane pans sideways at the dialog's narrowest.
	void expect_no_sideways_pan(qt::agent_configuration_import_dialog& aDialog, const std::string_view aWhat)
	{
		aDialog.show();
		aDialog.resize(aDialog.minimumSizeHint().width(), 680);
		settle();
		auto* const scroll = aDialog.findChild<QScrollArea*>();
		const auto  fits   = scroll != nullptr && scroll->horizontalScrollBar()->maximum() == 0 && scroll->widget()->width() <= scroll->viewport()->width()
			&& scroll->widget()->minimumSizeHint().width() <= scroll->viewport()->width();
		if (!fits && scroll != nullptr) {
			std::cerr << "content " << scroll->widget()->minimumSizeHint().width() << " viewport " << scroll->viewport()->width() << '\n';
		}
		expect(fits, aWhat);
		aDialog.resize(980, 680);
		settle();
	}

	/// [[BPR-2032]] (design workflow-import-v1, B, the owner's choice). The
	/// Prompt library's shape: a ticked list and the selected item in full;
	/// everything the file can bring starts chosen; each choice is one click.
	void test_import(const std::string& aBytes, const std::string& aRoot)
	{
		// The recipient has its own Work Claude and an edited Problem statement.
		auto mine               = application::agent_configuration_snapshot{ };
		auto work               = core::agent_entry{ .Id = 3U, .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/me/work-claude" };
		work.Arguments          = { "--profile", "work" };
		work.Identity           = "cccccccccccccccccccccccccccccccc";
		mine.Agents.Agents      = { work };
		mine.Hooks.Overrides[0] = "My own problem prompt.";
		auto preview            = application::agent_configuration_preview{ .Snapshot = mine, .Prompts = application::local_prompts(mine) };
		auto decoded            = core::decode_agent_configuration(aBytes, preview.Prompts, application::prompt_aliases());
		expect(decoded.has_value(), "the export reads back");
		preview.File = std::move(*decoded);
		auto dialog  = qt::agent_configuration_import_dialog{ preview, { .Skills = { }, .Found = { }, .Profile = "Work" } };
		dialog.set_program_chooser([](const QString& aHint) { return aHint == QStringLiteral("lint-bot") ? QStringLiteral("/usr/local/bin/lint-bot") : QString{ }; });
		auto* const intro = dialog.findChild<QLabel*>(QStringLiteral("agentConfigurationIntro"));
		expect(intro != nullptr && intro->text().contains(QStringLiteral("into the profile “Work”")), "the preview says which profile it lands in");
		// Defaults: Work Claude's update starts chosen, the new Lint bot waits
		// for its program, the edited prompt's update starts chosen.
		expect(dialog.apply_button()->isEnabled() && dialog.apply_button()->text() == QStringLiteral("Import"), "Import is enabled as the window opens");
		auto* const workRow = row_named(dialog, QStringLiteral("Work Claude"));
		auto* const lintRow = row_named(dialog, QStringLiteral("Lint bot"));
		expect(workRow != nullptr && workRow->text().contains(QStringLiteral("Updated")) && workRow->checkState() == Qt::Checked && workRow->text().contains(QStringLiteral("Updates yours")),
			   "an update is ticked, with its tag and one line of what happens");
		expect(lintRow != nullptr && lintRow->text().contains(QStringLiteral("New")) && lintRow->checkState() == Qt::Checked && lintRow->text().contains(QStringLiteral("once you choose its program")),
			   "a new agent is ticked and says it waits for its program");
		auto choices = dialog.agent_choices();
		expect(choices.size() == 1U && choices[0].Item == 0U && choices[0].Action == core::agent_import_action::replace && choices[0].Target == 3U, "the update applies; the unbound agent waits");
		expect(!choices[0].AcceptRelaxedChecks && !choices[0].AcceptBuilds, "relaxed checks and builds start unaccepted");
		expect(dialog.summary()->text().contains(QStringLiteral("1 agent waits for its program")), "the waiting agent is said");
		auto problem = std::size_t{ 0U };
		for (std::size_t index = 0U; index < preview.File.Prompts.size(); ++index) {
			if (preview.File.Prompts[index].Prompt.Purpose == "tour.problem") {
				problem = index;
			}
		}
		expect(std::ranges::contains(dialog.prompt_choices(), problem), "an edited prompt's update starts chosen");
		// Lint bot: choose its program.
		dialog.select_agent(1U);
		expect(dialog.program_button() != nullptr && plain(dialog.program_line()).contains(QStringLiteral("lint-bot")), "the program to choose is named by its file name");
		dialog.program_button()->click();
		settle();
		expect(plain(dialog.program_line()).contains(QStringLiteral("/usr/local/bin/lint-bot")), "bound");
		choices = dialog.agent_choices();
		expect(choices.size() == 2U && choices[1].Item == 1U && choices[1].Program == "/usr/local/bin/lint-bot", "a bound agent is applied");
		// Work Claude: Update / Use Work Claude / Leave out, one click each.
		dialog.select_agent(0U);
		expect(dialog.choice_buttons().size() == 3 && part(dialog, QStringLiteral("Update")) != nullptr && part(dialog, QStringLiteral("Update"))->isChecked()
				   && part(dialog, QStringLiteral("Use Work Claude")) != nullptr && part(dialog, QStringLiteral("Leave out")) != nullptr,
			   "an agent you have: Update, Use one of yours, Leave out, the update chosen");
		expect(dialog.settings_table() != nullptr && dialog.settings_table()->findChildren<QLabel*>().size() > 3, "an update shows yours beside the file's");
		auto fields = QString{ };
		for (auto* const label : dialog.settings_table()->findChildren<QLabel*>()) {
			fields += label->text() + QLatin1Char('|');
		}
		expect(fields.contains(QStringLiteral("Model")) && fields.contains(QStringLiteral("opus")), "every changed field is shown");
		expect(dialog.outcome()->text().contains(QStringLiteral("can be continued")), "keeping its program, its conversations continue");
		expect(dialog.relaxed_checks() != nullptr && !dialog.relaxed_checks()->isChecked() && dialog.builds() != nullptr && !dialog.builds()->isChecked(), "relaxed checks and builds are offered, unticked");
		expect(dialog.builds()->text() == QStringLiteral("Let Work Claude build and run tests"), "the permission is said in plain words");
		dialog.relaxed_checks()->click();
		settle();
		expect(dialog.outcome()->text().contains(QStringLiteral("no longer be continued")), "relaxing checks changes what it runs: the consequence is stated");
		expect(dialog.agent_choices()[0].AcceptRelaxedChecks && !dialog.agent_choices()[0].AcceptBuilds, "accepted for this entry only");
		render(dialog, aRoot, QStringLiteral("import-agent"));
		expect_no_sideways_pan(dialog, "an agent's detail fits the pane");
		// One click: Leave out, then the tick brings the update back.
		part(dialog, QStringLiteral("Leave out"))->click();
		settle();
		expect(workRow->checkState() == Qt::Unchecked && workRow->text().contains(QStringLiteral("Left out")) && dialog.agent_choices().size() == 1U, "Leave out is one click, and unticks");
		workRow->setCheckState(Qt::Checked);
		settle();
		expect(dialog.agent_choices().size() == 2U && dialog.agent_choices()[0].Action == core::agent_import_action::replace, "the tick brings the update back");
		part(dialog, QStringLiteral("Use Work Claude"))->click();
		settle();
		expect(dialog.agent_choices()[0].Action == core::agent_import_action::use_existing && dialog.outcome()->text().contains(QStringLiteral("stands for it")), "Use one of yours is one click");
		// The edited prompt: compare and choose.
		dialog.select_prompt(problem);
		expect(part(dialog, QStringLiteral("Update")) != nullptr && part(dialog, QStringLiteral("Update"))->isChecked(), "a prompt: Update or Leave out, the update chosen");
		expect(dialog.outcome()->text().contains(QStringLiteral("written against review-preparation-v1")), "the version it was written against is named");
		expect(dialog.outcome()->text().contains(QStringLiteral("/team-style")), "a skill it names that is not here is a visible prerequisite");
		expect(dialog.yours()->toPlainText() == QStringLiteral("My own problem prompt.") && dialog.theirs()->toPlainText().contains(QStringLiteral("two sentences")), "yours beside the file's");
		press_space(part(dialog, QStringLiteral("Leave out")));
		settle();
		expect(!std::ranges::contains(dialog.prompt_choices(), problem), "left out by keyboard");
		render(dialog, aRoot, QStringLiteral("import-prompt"));
		// A prompt the same as yours has nothing to choose.
		const auto same = std::ranges::find_if(dialog.plan().Prompts, [](const core::prompt_import_plan& aPlan) { return aPlan.Standing == core::import_standing::unchanged; });
		expect(same != dialog.plan().Prompts.end(), "the shipped prompts are unchanged");
		dialog.select_prompt(static_cast<std::size_t>(same - dialog.plan().Prompts.begin()));
		expect(dialog.choice_buttons().isEmpty(), "Unchanged has no choice");
		expect(dialog.apply_button()->isEnabled(), "Import is offered");
	}
	/// [[BPR-2032]], RFC-BPR-0112. A workflow exported with the agent it
	/// asks, previewed with its origin, actions and declared effects, its
	/// program bound here, and an update compared before it is taken.
	[[nodiscard]] auto test_workflow(const std::string& aRoot) -> application::agent_configuration_preview
	{
		auto snapshot      = sender();
		auto workflow      = core::local_workflow{ };
		workflow.Workflow  = { .PortableId = "synthetic-org",
							   .Name       = "Synthetic Org pull request",
							   .Origin     = "Synthetic Org",
							   .Guidance   = "Say why.",
							   .Actions    = { { .Id = "describe", .Slot = core::workflow_slot::propose_description, .Task = "Draft the description", .Binding = core::workflow_binding::agent, .Agent = "x", .Prompt = "Draft it.", .Effects = { core::workflow_effect::read_context, core::workflow_effect::write_proposal } },
											   { .Id = "template", .Slot = core::workflow_slot::check_preparation, .Task = "Check the template", .Binding = core::workflow_binding::command, .ProgramName = "org-companion", .Arguments = { "{request}", "{response}" }, .Effects = { core::workflow_effect::run_check }, .Required = true } } };
		workflow.Programs  = { { .Action = "template", .Program = "/opt/org/org-companion" } };
		workflow.Agents    = { { .Action = "describe", .AgentName = "Work Claude" } };
		snapshot.Workflows = { workflow };
		auto  exporter     = qt::agent_configuration_export_dialog{ snapshot, { }, "BetterPullReview test" };
		auto* workflowRow  = static_cast<QTreeWidgetItem*>(nullptr);
		for (QTreeWidgetItemIterator it{ exporter.items() }; *it != nullptr; ++it) {
			if ((*it)->text(0) == QStringLiteral("Synthetic Org pull request")) {
				workflowRow = *it;
			}
			if ((*it)->text(0) == QStringLiteral("Work Claude") || (*it)->text(0) == QStringLiteral("Lint bot")) {
				(*it)->setCheckState(0, Qt::Unchecked);
			}
		}
		expect(workflowRow != nullptr && workflowRow->checkState(0) == Qt::Unchecked, "a workflow is offered for export, unchosen");
		workflowRow->setCheckState(0, Qt::Checked);
		const auto bytes = exporter.bytes();
		expect(bytes.find("\"version\": 2") != std::string::npos && bytes.find("\"Work Claude\"") != std::string::npos && bytes.find("/opt/org") == std::string::npos,
			   "the workflow travels with the agent it asks, without its program path");
		render(exporter, aRoot, QStringLiteral("export-workflow"));

		auto mine     = application::agent_configuration_snapshot{ };
		auto preview  = application::agent_configuration_preview{ .Snapshot = mine, .Prompts = application::local_prompts(mine) };
		preview.File  = core::decode_agent_configuration(bytes, preview.Prompts, application::prompt_aliases()).value();
		auto importer = qt::agent_configuration_import_dialog{ preview, { .Skills = { }, .Found = { } } };
		auto asked    = 0;
		importer.set_program_chooser([&asked](const QString& aHint) {
			++asked;
			return aHint == QStringLiteral("org-companion") ? QStringLiteral("/usr/local/bin/org-companion") : QString{ };
		});
		importer.select_workflow(0U);
		expect(part(importer, QStringLiteral("Add")) != nullptr && part(importer, QStringLiteral("Add"))->isChecked() && importer.choice_buttons().size() == 2, "a new workflow: Add or Leave out, added");
		auto* const template_ = importer.workflow_comparison()->findChild<QLabel*>(QStringLiteral("agentConfigurationAction-template"));
		auto* const describe  = importer.workflow_comparison()->findChild<QLabel*>(QStringLiteral("agentConfigurationAction-describe"));
		expect(template_ != nullptr && template_->text().contains(QStringLiteral("Required")) && plain(template_).contains(QStringLiteral("You run")) && template_->text().contains(QStringLiteral("runs checks in your workspace")),
			   "each action, who runs it and with what, and what it says it does");
		expect(describe != nullptr && describe->text().contains(QStringLiteral("Work Claude")) && describe->text().contains(QStringLiteral("read-only")), "an agent action names its agent");
		expect(importer.workflow_program_buttons().size() == 1, "one program to choose, named by the file and carrying no path");
		importer.workflow_program_buttons().front()->click();
		settle();
		const auto choices = importer.workflow_choices();
		expect(asked == 1 && choices.size() == 1U && choices[0].Programs.size() == 1U && choices[0].Programs[0].Program == "/usr/local/bin/org-companion", "the program is bound on this computer");
		expect(importer.summary()->text().contains(QStringLiteral("Nothing runs until you start an action in a draft")), "importing runs nothing, and says so");
		render(importer, aRoot, QStringLiteral("import-workflow"));
		expect_no_sideways_pan(importer, "a workflow's detail fits the pane");

		// The same workflow again, with a widened action: compared, and chosen
		// (the owner's accepted change, workflow-import-v001).
		mine.Workflows      = { workflow };
		auto updatedPreview = application::agent_configuration_preview{ .Snapshot = mine, .Prompts = application::local_prompts(mine) };
		auto widened        = preview.File;
		widened.Workflows[0].Workflow.Actions[1].Effects.push_back(core::workflow_effect::change_ticket);
		updatedPreview.File = widened;
		auto updater        = qt::agent_configuration_import_dialog{ updatedPreview, { .Skills = { }, .Found = { } } };
		updater.select_workflow(0U);
		expect(part(updater, QStringLiteral("Update")) != nullptr && part(updater, QStringLiteral("Update"))->isChecked() && updater.workflow_choices().size() == 1U
				   && updater.workflow_choices()[0].Action == core::workflow_import_action::replace,
			   "an update starts chosen");
		auto* const widenedCell = updater.workflow_comparison()->findChild<QLabel*>(QStringLiteral("agentConfigurationAction-template"));
		expect(widenedCell != nullptr && widenedCell->text().contains(QStringLiteral("Does more")) && widenedCell->text().contains(QStringLiteral("changes a ticket")), "a widened effect is marked before Import");
		expect(updater.workflow_comparison()->findChildren<QLabel*>().size() >= 6, "yours beside the file's, row against row");
		render(updater, aRoot, QStringLiteral("import-workflow-update"));
		part(updater, QStringLiteral("Leave out"))->click();
		settle();
		expect(updater.workflow_choices().empty() && row_named(updater, QStringLiteral("Synthetic Org pull request"))->checkState() == Qt::Unchecked, "Leave out is one click, and unticks");

		// An unknown action is named and its workflow refused; the rest still imports.
		auto unknown                     = preview.File;
		unknown.Workflows[0].Problem     = core::workflow_problem::unsupported;
		unknown.Workflows[0].Unsupported = "deploy_preview";
		auto refusedPreview              = preview;
		refusedPreview.File              = unknown;
		auto        refuser              = qt::agent_configuration_import_dialog{ refusedPreview, { .Skills = { }, .Found = { } } };
		auto* const refusedRow           = row_named(refuser, QStringLiteral("Synthetic Org pull request"));
		expect(refusedRow != nullptr && refusedRow->text().contains(QStringLiteral("Cannot be imported")) && refusedRow->text().contains(QStringLiteral("deploy_preview"))
				   && !refusedRow->flags().testFlag(Qt::ItemIsUserCheckable),
			   "an unknown action is named and its workflow cannot be ticked");
		refuser.select_workflow(0U);
		expect(refuser.choice_buttons().isEmpty() && refuser.workflow_choices().empty(), "it is refused");
		return updatedPreview;
	}
	/// Owner, look workflow-import-v002: "Super ugly. Something went way
	/// wrong with the colors." The dialog drew with the system palette, not
	/// the theme Settings carries. In each theme the detail pane is the
	/// Prompts page's detail panel, by role and by the pixels painted.
	void test_theme_matches_prompts(const application::agent_configuration_preview& aPreview, const std::string& aRoot)
	{
		for (const auto mode : { core::appearance_mode::dark, core::appearance_mode::light, core::appearance_mode::high_contrast_dark, core::appearance_mode::high_contrast_light }) {
			auto owner   = QWidget{ };
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(owner);
			const auto  name    = QString::fromUtf8(core::stable_name(profile.variant()).data());
			auto* const column  = new QVBoxLayout{ &owner };
			auto* const prompts = new qt::prompts_page{ &owner };
			column->addWidget(prompts);
			owner.resize(1100, 900);
			owner.show();
			auto* const dialog = new qt::agent_configuration_import_dialog{ aPreview, { .Skills = { }, .Found = { } }, &owner };
			dialog->select_workflow(0U);
			dialog->show();
			settle();
			auto* const pane   = dialog->findChild<QFrame*>(QStringLiteral("agentConfigurationDetail"));
			auto* const editor = prompts->findChild<QFrame*>(QStringLiteral("promptsEditor"));
			expect(pane != nullptr && editor != nullptr, "the two detail panels");
			const auto panel = editor->palette().color(QPalette::Active, editor->backgroundRole());
			expect(dialog->palette().color(QPalette::Active, QPalette::Window) == owner.palette().color(QPalette::Active, QPalette::Window), "the dialog takes its opener's theme");
			expect(pane->palette().color(QPalette::Active, pane->backgroundRole()) == panel, "the detail pane is the Prompts page's panel, by role");
			const auto image = pane->grab().toImage();
			const auto ratio = image.devicePixelRatio();
			// The pane's left margin: nothing but the panel is drawn there.
			const auto drawn = image.pixelColor(static_cast<int>(6 * ratio), image.height() / 2);
			if (drawn.rgb() != panel.rgb()) {
				std::cerr << name.toStdString() << ": drawn " << drawn.name().toStdString() << " panel " << panel.name().toStdString() << '\n';
			}
			expect(drawn.rgb() == panel.rgb(), "and by the pixels it paints");
			// A theme change while it is open follows.
			auto other = qt::appearance_profile{ };
			static_cast<void>(other.select_mode(mode == core::appearance_mode::dark ? core::appearance_mode::light : core::appearance_mode::dark));
			other.apply_to(owner);
			settle();
			expect(pane->palette().color(QPalette::Active, pane->backgroundRole()) == editor->palette().color(QPalette::Active, editor->backgroundRole()), "and follows a theme change");
			profile.apply_to(owner);
			settle();
			render(*dialog, aRoot, QStringLiteral("import-theme-") + name);
			dialog->hide();
			delete dialog;
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   root = argc > 2 && std::string_view{ argv[1] } == "--render" ? std::string{ argv[2] } : std::string{ };
	test_share_row(root);
	const auto bytes = test_export(root);
	test_import(bytes, root);
	const auto update = test_workflow(root);
	test_theme_matches_prompts(update, root);
	std::cout << "agent-configuration-dialogs:pass\n";
	return EXIT_SUCCESS;
}
