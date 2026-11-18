/// \file
/// \brief [[BPR-2017]], RFC-BPR-0107. Settings' Import configuration… and
/// Export configuration…: the share row on the Agents roster, the export's
/// choice and exact text, and the import preview's bindings, differences and
/// defaults, driven by keyboard where a person would. `--render DIR` saves
/// each surface (native and light/dark under `BPR_NATIVE_LOOK`).

#include "bpr/application/agent_configuration_exchange.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/prompt_pack.hpp"
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
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleHints>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <array>
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

	/// [[BPR-2204]] (owner, look profile-settings-dialogs-v001: "It might be
	/// nice if the tree branches on the left were collapsable"). Every section
	/// starts open; its triangle, or Left and Right, close and open it; a
	/// closed section still shows its tick, the partial dash included.
	void test_export_sections_collapse(qt::agent_configuration_export_dialog& aDialog)
	{
		using core::profile_settings_section;
		auto* const tree = aDialog.items();
		aDialog.show();
		settle();
		for (const auto section : { profile_settings_section::agents, profile_settings_section::prompts, profile_settings_section::identity, profile_settings_section::repositories }) {
			expect(aDialog.section(section)->isExpanded(), "every section starts open");
		}
		auto* const prompts = aDialog.section(profile_settings_section::prompts);
		const auto  index   = tree->indexFromItem(prompts);
		// Left closes the section the cursor is on, Right opens it again.
		tree->setFocus();
		tree->setCurrentItem(prompts);
		auto left = QKeyEvent{ QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier };
		QApplication::sendEvent(tree, &left);
		settle();
		expect(!prompts->isExpanded(), "Left closes a section");
		auto right = QKeyEvent{ QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier };
		QApplication::sendEvent(tree, &right);
		settle();
		expect(prompts->isExpanded(), "Right opens it");
		// The triangle, at the row's left, closes it with a click.
		const auto row     = tree->visualRect(index);
		const auto at      = QPointF{ static_cast<qreal>(row.left() + 14), static_cast<qreal>(row.top() + 14 + 8) };
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, at, tree->viewport()->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, at, tree->viewport()->mapToGlobal(at), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(tree->viewport(), &press);
		QApplication::sendEvent(tree->viewport(), &release);
		settle();
		expect(!prompts->isExpanded(), "its triangle closes it");
		// Closed, its tick still follows its items: all ticked, some (the
		// partial dash) and none each draw a different tick.
		const auto tick = [tree, row] {
			settle();
			return tree->viewport()->grab(QRect{ row.left() + 20, row.top(), 32, 40 }).toImage();
		};
		const auto all       = tick();
		auto*      checkable = static_cast<QTreeWidgetItem*>(nullptr);
		for (int child = 0; child < prompts->childCount(); ++child) {
			if (prompts->child(child)->flags().testFlag(Qt::ItemIsUserCheckable)) {
				checkable = prompts->child(child);
			}
		}
		checkable->setCheckState(0, Qt::Unchecked);
		const auto some = tick();
		for (int child = 0; child < prompts->childCount(); ++child) {
			if (prompts->child(child)->flags().testFlag(Qt::ItemIsUserCheckable)) {
				prompts->child(child)->setCheckState(0, Qt::Unchecked);
			}
		}
		const auto none = tick();
		expect(all != some && some != none && all != none, "a closed section's tick shows all, some (the dash) or none");
		expect(aDialog.prompts().empty(), "and what is chosen is unchanged by closing");
		aDialog.all_prompts_button()->click();
		settle();
		QApplication::sendEvent(tree->viewport(), &press);
		QApplication::sendEvent(tree->viewport(), &release);
		settle();
		expect(prompts->isExpanded(), "and opens it again");
	}

	[[nodiscard]] auto test_export(const std::string& aRoot) -> std::string
	{
		auto unsaved = std::array<bool, core::writing_hook_count + 1U>{ };
		unsaved[1]   = true;
		// [[BPR-2198]]. A profile signed in to github.com, remembering an
		// enterprise server, with a saved and a followed repository.
		auto facts         = core::profile_facts{ .ProfileName = "Work" };
		facts.SignedIn     = { { .Host = "github.com", .Login = "octo" } };
		facts.Remembered   = { { .Host = "ghe.example.com", .Login = "octo-corp" } };
		facts.Repositories = { { .Host = "github.com", .Repository = "octo/app", .Saved = true }, { .Repository = "octo/lib", .Followed = true } };
		auto dialog        = qt::agent_configuration_export_dialog{ sender(), unsaved, "BetterPullReview test", facts };
		expect(dialog.windowTitle() == QStringLiteral("Export Profile Settings"), "the dialog is named for Profile Settings");
		expect(dialog.agents() == std::vector<std::uint32_t>{ 1U, 2U }, "every agent starts chosen");
		using core::profile_settings_section;
		auto* const identity     = dialog.section(profile_settings_section::identity);
		auto* const repositories = dialog.section(profile_settings_section::repositories);
		auto* const agents       = dialog.section(profile_settings_section::agents);
		expect(identity != nullptr && repositories != nullptr && agents != nullptr && dialog.section(profile_settings_section::prompts) != nullptr
				   && dialog.section(profile_settings_section::workflows) != nullptr,
			   "the five sections are listed");
		expect(identity->checkState(0) == Qt::Unchecked && !dialog.identity_chosen() && dialog.bytes().find("\"identity\"") == std::string::npos, "Identity starts unticked and stays out of the file");
		expect(repositories->checkState(0) == Qt::Checked && dialog.repositories().size() == 2U && dialog.bytes().find("\"repository\": \"octo/app\"") != std::string::npos,
			   "Repositories starts ticked and is in the file");
		identity->setCheckState(0, Qt::Checked);
		expect(dialog.identity_chosen() && dialog.bytes().find("\"login\": \"octo-corp\"") != std::string::npos && dialog.bytes().find("\"profile_name\": \"Work\"") != std::string::npos,
			   "ticking Identity puts the name and the accounts in the file");
		identity->setCheckState(0, Qt::Unchecked);
		repositories->setCheckState(0, Qt::Unchecked);
		expect(dialog.repositories().empty() && dialog.bytes().find("\"repositories\"") == std::string::npos, "unticking a section unticks every item in it");
		repositories->setCheckState(0, Qt::Checked);
		expect(dialog.repositories().size() == 2U, "and ticking it ticks them all again");
		agents->setCheckState(0, Qt::Unchecked);
		expect(dialog.agents().empty(), "unticking Agents unticks each agent");
		agents->setCheckState(0, Qt::Checked);
		expect(dialog.agents() == std::vector<std::uint32_t>{ 1U, 2U }, "and ticking it ticks each again");
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
		test_export_sections_collapse(dialog);
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
			const auto text = aDialog.items()->item(row)->text();
			if (text.startsWith(aName + QStringLiteral("   ")) || text.startsWith(aName + QLatin1Char('\n'))) {
				return aDialog.items()->item(row);
			}
		}
		return nullptr;
	}

	/// [[BPR-2204]]. Every word an action's cell shows: its task, its tags,
	/// who runs it and what it says it does.
	[[nodiscard]] auto cell_text(const QWidget* aCell) -> QString
	{
		auto words = QString{ };
		for (const auto* const label : aCell->findChildren<QLabel*>()) {
			words += label->text() + QLatin1Char('|');
		}
		return words.remove(QChar{ 0x200B });
	}

	/// [[BPR-2204]]. A cell's line, by its object name, as placed in `aTable`.
	[[nodiscard]] auto line_top(const QWidget* aCell, const QString& aLine, const QWidget* aTable) -> int
	{
		const auto* const label = aCell->findChild<QLabel*>(aLine);
		return label == nullptr ? -1 : label->mapTo(aTable, QPoint{ 0, 0 }).y();
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

	/// [[BPR-2198]]. Import shows Identity and Repositories in the same list
	/// and detail; an account not signed in says Sign in to use.
	void test_identity_and_repositories(qt::agent_configuration_import_dialog& aDialog)
	{
		auto* const identity = row_named(aDialog, QStringLiteral("Work"));
		expect(identity != nullptr && identity->checkState() == Qt::Unchecked && identity->text().contains(QStringLiteral("Left out")), "Identity is listed and starts unticked");
		auto* const saved    = row_named(aDialog, QStringLiteral("octo/app"));
		auto* const followed = row_named(aDialog, QStringLiteral("octo/lib"));
		expect(saved != nullptr && saved->text().contains(QStringLiteral("Sign in to use")) && !saved->flags().testFlag(Qt::ItemIsUserCheckable),
			   "a saved repository with no account signed in on its host says Sign in to use");
		expect(followed != nullptr && followed->checkState() == Qt::Checked, "a followed repository starts ticked");
		expect(aDialog.repository_choices() == std::vector<std::size_t>{ 1U } && !aDialog.identity_choice(), "only the followed repository is chosen to begin with");
		aDialog.select_identity();
		auto* const state = aDialog.detail()->findChild<QLabel*>(QStringLiteral("agentConfigurationAccountState"));
		expect(state != nullptr && state->text() == QStringLiteral("Sign in to use"), "the account says Sign in to use");
		expect(aDialog.outcome()->text().contains(QStringLiteral("renames this profile from “Default” to “Work”")), "Identity says what it renames");
		part(aDialog, QStringLiteral("Import"))->click();
		settle();
		expect(aDialog.identity_choice() && row_named(aDialog, QStringLiteral("Work"))->checkState() == Qt::Checked, "one click chooses Identity");
		part(aDialog, QStringLiteral("Leave out"))->click();
		settle();
		expect(!aDialog.identity_choice(), "and one leaves it out");
		aDialog.select_repository(0U);
		expect(aDialog.outcome()->text().contains(QStringLiteral("Sign in to use")) && aDialog.choice_buttons().isEmpty(), "the waiting repository explains itself and offers no choice");
		expect_no_sideways_pan(aDialog, "the Identity and repository details fit");
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
		auto decoded            = core::decode_agent_configuration(aBytes, preview.Prompts);
		expect(decoded.has_value(), "the export reads back");
		preview.File = std::move(*decoded);
		// [[BPR-2198]]. The file's Identity and Repositories, on a computer
		// signed in to nothing, in a profile named Default.
		preview.File.Identity     = core::portable_identity{ .ProfileName = "Work", .Accounts = { { .Host = "github.com", .Login = "octo" } } };
		preview.File.Repositories = { { .Host = "github.com", .Repository = "octo/app", .Saved = true }, { .Repository = "octo/lib", .Followed = true } };
		preview.Here              = core::profile_facts{ .ProfileName = "Default" };
		auto dialog               = qt::agent_configuration_import_dialog{ preview, { .Skills = { }, .Found = { }, .Profile = "Work" } };
		dialog.set_program_chooser([](const QString& aHint) { return aHint == QStringLiteral("lint-bot") ? QStringLiteral("/usr/local/bin/lint-bot") : QString{ }; });
		auto* const intro = dialog.findChild<QLabel*>(QStringLiteral("agentConfigurationIntro"));
		expect(intro != nullptr && intro->text().contains(QStringLiteral("into the profile “Work”")), "the preview says which profile it lands in");
		// Defaults: Work Claude's update starts chosen, the new Lint bot waits
		// for its program, the edited prompt's update starts chosen.
		expect(dialog.apply_button()->isEnabled() && dialog.apply_button()->text() == QStringLiteral("Import"), "Import is enabled as the window opens");
		test_identity_and_repositories(dialog);
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
		// [[BPR-2198]]. Workflows is a section, ticked to begin with as every
		// section but Identity is (core::exported_by_default).
		expect(workflowRow != nullptr && workflowRow->checkState(0) == Qt::Checked, "a workflow is offered for export, ticked with its section");
		workflowRow->setCheckState(0, Qt::Checked);
		const auto bytes = exporter.bytes();
		expect(bytes.find("\"version\": 1") != std::string::npos && bytes.find("\"Work Claude\"") != std::string::npos && bytes.find("/opt/org") == std::string::npos,
			   "the workflow travels with the agent it asks, without its program path");
		render(exporter, aRoot, QStringLiteral("export-workflow"));

		auto mine     = application::agent_configuration_snapshot{ };
		auto preview  = application::agent_configuration_preview{ .Snapshot = mine, .Prompts = application::local_prompts(mine) };
		preview.File  = core::decode_agent_configuration(bytes, preview.Prompts).value();
		auto importer = qt::agent_configuration_import_dialog{ preview, { .Skills = { }, .Found = { } } };
		auto asked    = 0;
		importer.set_program_chooser([&asked](const QString& aHint) {
			++asked;
			return aHint == QStringLiteral("org-companion") ? QStringLiteral("/usr/local/bin/org-companion") : QString{ };
		});
		importer.select_workflow(0U);
		// [[BPR-2204]]. As the design draws it, the tick is a workflow's choice:
		// the detail has no Add / Leave out of its own.
		expect(importer.choice_buttons().isEmpty() && row_named(importer, QStringLiteral("Synthetic Org pull request"))->checkState() == Qt::Checked && importer.workflow_choices().size() == 1U
				   && importer.workflow_choices()[0].Action == core::workflow_import_action::add,
			   "a new workflow: its tick is its choice, ticked, and the detail has no control of its own");
		const auto templateCell = cell_text(importer.workflow_comparison()->findChild<QWidget*>(QStringLiteral("agentConfigurationAction-template")));
		const auto describe     = cell_text(importer.workflow_comparison()->findChild<QWidget*>(QStringLiteral("agentConfigurationAction-describe")));
		expect(templateCell.contains(QStringLiteral("Required")) && templateCell.contains(QStringLiteral("You run")) && templateCell.contains(QStringLiteral("runs checks in your workspace")),
			   "each action, who runs it and with what, and what it says it does");
		expect(describe.contains(QStringLiteral("Work Claude")) && describe.contains(QStringLiteral("read-only")), "an agent action names its agent");
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
		// And a new action, so the table shows one.
		widened.Workflows[0].Workflow.Actions.push_back({ .Id = "link", .Slot = core::workflow_slot::finish_publication, .Task = "Link the ticket and upload the screenshot", .Binding = core::workflow_binding::command, .ProgramName = "org-companion", .Arguments = { "{request}", "{response}" }, .Effects = { core::workflow_effect::change_ticket } });
		updatedPreview.File = widened;
		auto updater        = qt::agent_configuration_import_dialog{ updatedPreview, { .Skills = { }, .Found = { } } };
		updater.select_workflow(0U);
		expect(updater.choice_buttons().isEmpty() && updater.workflow_choices().size() == 1U && updater.workflow_choices()[0].Action == core::workflow_import_action::replace, "an update starts chosen, by its tick");
		const auto widenedCells = updater.workflow_comparison()->findChildren<QWidget*>(QStringLiteral("agentConfigurationAction-template"));
		expect(widenedCells.size() == 2, "the action on both sides");
		const auto* const widenedCell = widenedCells[1];
		const auto* const effects     = widenedCell->findChild<QLabel*>(QStringLiteral("agentConfigurationActionEffects"));
		// [[BPR-2204]]. The added effect alone, in the caution colour; no
		// "Does more" tag, and its words reach a screen reader.
		expect(effects != nullptr && effects->text().contains(QStringLiteral("changes a ticket")) && effects->text().contains(QStringLiteral("color:")) && !cell_text(widenedCell).contains(QStringLiteral("Does more"))
				   && effects->accessibleDescription().contains(QStringLiteral("changes a ticket")),
			   "a widened effect is marked before Import, in the caution colour");
		expect(updater.workflow_comparison()->findChildren<QLabel*>().size() >= 6, "yours beside the file's, row against row");
		const auto says = updater.detail()->findChild<QLabel*>(QStringLiteral("agentConfigurationWorkflowSays"));
		expect(says != nullptr && says->text().contains(QStringLiteral("font-weight:600")), "the revisions are set in bold");
		// [[BPR-2204]] (13). Each action's two sides share one row: their task,
		// who runs it and what it does start on the same lines.
		updater.show();
		settle();
		// Shown, the detail may have been drawn again: find its cells anew.
		const auto* const table = updater.workflow_comparison();
		const auto        sides = table->findChildren<QWidget*>(QStringLiteral("agentConfigurationAction-template"));
		expect(sides.size() == 2, "both sides, shown");
		for (const auto* const line : { "agentConfigurationActionTask", "agentConfigurationActionWho", "agentConfigurationActionEffects" }) {
			const auto left  = line_top(sides[0], QString::fromLatin1(line), table);
			const auto right = line_top(sides[1], QString::fromLatin1(line), table);
			if (left != right) {
				std::cerr << line << ": " << left << " beside " << right << '\n';
			}
			expect(left >= 0 && left == right, "the two sides of a row stay in line");
		}
		const auto* const added = table->findChild<QWidget*>(QStringLiteral("agentConfigurationAction-link"));
		expect(added != nullptr && added->property("bprComparisonTint").toBool() && cell_text(added).contains(QStringLiteral("New action")), "a new action's whole cell is tinted, with its tag");
		for (const auto* const label : added->findChildren<QLabel*>()) {
			if (label->text() == QStringLiteral("New action")) {
				expect(label->mapTo(table, QPoint{ 0, 0 }).y() == line_top(added, QStringLiteral("agentConfigurationActionTask"), table), "New action sits on the task's first line");
			}
		}
		render(updater, aRoot, QStringLiteral("import-workflow-update"));
		// [[BPR-2204]] (9). Another item opens at its top, not at the
		// previous item's scroll offset.
		updater.resize(980, 420);
		settle();
		auto* const scroll = updater.findChild<QScrollArea*>();
		scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
		settle();
		expect(scroll->verticalScrollBar()->value() > 0, "the workflow's detail scrolls in a short window");
		updater.items()->setCurrentItem(row_named(updater, QStringLiteral("Work Claude")));
		settle();
		expect(scroll->verticalScrollBar()->value() == 0, "the next item opens at its top");
		updater.resize(980, 680);
		row_named(updater, QStringLiteral("Synthetic Org pull request"))->setCheckState(Qt::Unchecked);
		settle();
		expect(updater.workflow_choices().empty(), "unticking leaves it out");

		// An unknown action is named and its workflow refused; the rest still imports.
		auto unknown                     = preview.File;
		unknown.Workflows[0].Problem     = core::workflow_problem::unsupported;
		unknown.Workflows[0].Unsupported = "deploy_preview";
		auto refusedPreview              = preview;
		refusedPreview.File              = unknown;
		auto        refuser              = qt::agent_configuration_import_dialog{ refusedPreview, { .Skills = { }, .Found = { } } };
		auto* const refusedRow           = row_named(refuser, QStringLiteral("Synthetic Org pull request"));
		auto        under                = QString{ };
		for (int row = 0; row < refuser.items()->count() && refuser.items()->item(row) != refusedRow; ++row) {
			if (refuser.items()->item(row)->flags() == Qt::NoItemFlags) {
				under = refuser.items()->item(row)->text();
			}
		}
		expect(refusedRow != nullptr && under == QStringLiteral("Cannot be imported") && refusedRow->text().contains(QStringLiteral("deploy_preview"))
				   && !refusedRow->flags().testFlag(Qt::ItemIsUserCheckable),
			   "an unknown action is named and its workflow cannot be ticked");
		refuser.select_workflow(0U);
		expect(refuser.choice_buttons().isEmpty() && refuser.workflow_choices().empty(), "it is refused");
		// [[BPR-2204]]. Drawn with the refused mark where its tick would be.
		// The row delegate's mark role (Qt::UserRole + 25), refused = 2.
		expect(refusedRow->data(Qt::UserRole + 25).toInt() == 2, "a refused row wears the refused mark");
		render(refuser, aRoot, QStringLiteral("import-refused"));
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
			// [[BPR-2204]]. The design's ground: the list sits on the Prompts
			// page's panel colour, the detail on the sheet's own window colour
			// with no frame of its own.
			auto* const list   = dialog->findChild<QWidget*>(QStringLiteral("agentConfigurationImportList"));
			auto* const pane   = dialog->findChild<QFrame*>(QStringLiteral("agentConfigurationDetail"));
			auto* const editor = prompts->findChild<QFrame*>(QStringLiteral("promptsEditor"));
			expect(list != nullptr && pane != nullptr && editor != nullptr, "the list, the detail and the Prompts page's panel");
			const auto panel = editor->palette().color(QPalette::Active, editor->backgroundRole());
			expect(dialog->palette().color(QPalette::Active, QPalette::Window) == owner.palette().color(QPalette::Active, QPalette::Window), "the dialog takes its opener's theme");
			expect(list->palette().color(QPalette::Active, QPalette::AlternateBase) == panel && pane->frameShape() == QFrame::NoFrame, "the list is on the Prompts page's panel, by role; the detail has no frame");
			const auto image = dialog->grab().toImage();
			const auto ratio = image.devicePixelRatio();
			const auto pixel = [&image, ratio, dialog](const QWidget* aWidget, const QPoint aAt) {
				const auto at = aWidget->mapTo(dialog, aAt);
				return image.pixelColor(static_cast<int>(at.x() * ratio), static_cast<int>(at.y() * ratio));
			};
			// The list's empty foot and the detail's left margin: nothing but the ground is drawn there.
			const auto foot   = pixel(list, QPoint{ list->width() / 2, list->height() - 8 });
			const auto margin = pixel(pane, QPoint{ 0, pane->height() - 4 });
			const auto window = dialog->palette().color(QPalette::Active, QPalette::Window);
			if (foot.rgb() != panel.rgb() || margin.rgb() != window.rgb()) {
				std::cerr << name.toStdString() << ": list " << foot.name().toStdString() << " panel " << panel.name().toStdString() << ", detail " << margin.name().toStdString() << " window " << window.name().toStdString()
						  << '\n';
			}
			expect(foot.rgb() == panel.rgb() && margin.rgb() == window.rgb(), "and by the pixels they paint");
			// A theme change while it is open follows.
			auto other = qt::appearance_profile{ };
			static_cast<void>(other.select_mode(mode == core::appearance_mode::dark ? core::appearance_mode::light : core::appearance_mode::dark));
			other.apply_to(owner);
			settle();
			expect(list->palette().color(QPalette::Active, QPalette::AlternateBase) == editor->palette().color(QPalette::Active, editor->backgroundRole()), "and follows a theme change");
			profile.apply_to(owner);
			settle();
			render(*dialog, aRoot, QStringLiteral("import-theme-") + name);
			dialog->select_agent(0U);
			render(*dialog, aRoot, QStringLiteral("import-agent-theme-") + name);
			dialog->hide();
			delete dialog;
			// [[BPR-2204]]. Export in the same language: its list and its
			// file each sit on the Prompts page's panel, in this theme.
			auto unsaved         = std::array<bool, core::writing_hook_count + 1U>{ };
			unsaved[1]           = true;
			auto facts           = core::profile_facts{ .ProfileName = "Work" };
			facts.SignedIn       = { { .Host = "github.com", .Login = "octo" } };
			facts.Repositories   = { { .Host = "github.com", .Repository = "octo/app", .Saved = true }, { .Repository = "octo/lib", .Followed = true } };
			auto snapshot        = sender();
			snapshot.Workflows   = { };
			auto* const exporter = new qt::agent_configuration_export_dialog{ snapshot, unsaved, "BetterPullReview test", facts, &owner };
			exporter->show();
			settle();
			auto* const exportList = exporter->findChild<QWidget*>(QStringLiteral("agentConfigurationExportList"));
			auto* const exportFile = exporter->findChild<QWidget*>(QStringLiteral("agentConfigurationExportFile"));
			expect(exportList != nullptr && exportFile != nullptr && exportList->palette().color(QPalette::Active, QPalette::AlternateBase) == panel
					   && exportFile->palette().color(QPalette::Active, QPalette::AlternateBase) == panel,
				   "export's list and file are on the Prompts page's panel");
			render(*exporter, aRoot, QStringLiteral("export-theme-") + name);
			exporter->hide();
			delete exporter;
		}
	}

	// ----- RFC-BPR-0125 AC24 ([[BPR-2149]]): prompt packs -----

	constexpr auto pack_uuid  = std::string_view{ "0f8e4c2a-1b3d-4e5f-8a9b-0c1d2e3f4a5b" };
	constexpr auto fresh_uuid = std::string_view{ "99999999-9999-4999-8999-999999999999" };
	constexpr auto a_uuid     = std::string_view{ "11111111-1111-4111-8111-111111111111" };
	constexpr auto b_uuid     = std::string_view{ "22222222-2222-4222-9222-222222222222" };
	constexpr auto c_uuid     = std::string_view{ "33333333-3333-4333-a333-333333333333" };
	constexpr auto d_uuid     = std::string_view{ "44444444-4444-4444-b444-444444444444" };
	constexpr auto e_uuid     = std::string_view{ "55555555-5555-4555-8555-555555555555" };

	[[nodiscard]] auto suggested(const std::string_view aId, std::string aName, std::string aTemplate) -> core::prompt_macro
	{
		auto macro          = core::prompt_macro{ .Id = std::string{ aId }, .Place = core::prompt_place::agent_tab };
		macro.Body.Name     = std::move(aName);
		macro.Body.Template = std::move(aTemplate);
		return macro;
	}

	[[nodiscard]] auto linked_to_pack(const std::string_view aId, std::string aName, std::string aTemplate, const std::uint32_t aVersion) -> core::prompt_macro
	{
		auto macro = suggested(aId, std::move(aName), std::move(aTemplate));
		macro.Pack = core::prompt_pack_link{ .Pack = std::string{ pack_uuid }, .Label = "Team prompts", .Version = aVersion, .Place = macro.Place, .Body = macro.Body };
		return macro;
	}

	/// Export: suggested prompts start unticked (version 1); ticked, they go
	/// out as a named pack, new or a later version of one held here.
	void test_export_prompt_pack(const std::string& aRoot)
	{
		auto snapshot                         = sender();
		snapshot.PromptMacros.Differences.Own = { suggested("own.0123456789abcdef0123456789abcdef", "Look again", "Look again at {pr.title}."),
												  linked_to_pack(a_uuid, "Risks", "What could this break?", 2U) };
		auto dialog                           = qt::agent_configuration_export_dialog{ snapshot, { }, "BetterPullReview test" };
		expect(dialog.suggested_prompts().empty() && dialog.bytes().find("\"version\": 1") != std::string::npos && dialog.bytes().find("prompt_pack") == std::string::npos,
			   "no suggested prompt starts ticked, so the file stays version 1");
		dialog.set_minted_identities(std::string{ pack_uuid }.replace(0U, 1U, "1"), { std::string{ fresh_uuid } });
		auto* section = static_cast<QTreeWidgetItem*>(nullptr);
		for (QTreeWidgetItemIterator it{ dialog.items() }; *it != nullptr; ++it) {
			if ((*it)->text(0) == QStringLiteral("Suggested prompts")) {
				section = *it;
			}
		}
		expect(section != nullptr && section->childCount() == 2, "the reviewer's own prompts are listed, each to tick");
		expect(section->child(1)->text(1) == QStringLiteral("From Team prompts, version 2"), "a prompt from a pack says which");
		section->setCheckState(0, Qt::Checked);
		expect(dialog.suggested_prompts().size() == 2U && !dialog.prompt_pack() && !dialog.export_button()->isEnabled(), "a new pack needs a name");
		expect(dialog.findChild<QLabel*>(QStringLiteral("agentConfigurationExportSummary"))->text() == QStringLiteral("Name the prompt pack."), "and says so");
		dialog.pack_name()->setText(QStringLiteral("Review kit"));
		expect(dialog.prompt_pack() && dialog.prompt_pack()->Pack.Version == 1U && dialog.prompt_pack()->Pack.Label == "Review kit", "a new pack is version 1");
		expect(dialog.bytes().find("\"version\": 2") != std::string::npos && dialog.bytes().find(std::string{ fresh_uuid }) != std::string::npos
				   && dialog.bytes().find("own.0123") == std::string::npos,
			   "version 2; the prompt made before UUIDs goes out under the minted one");
		expect(dialog.exact_text()->toPlainText().contains(QStringLiteral("Look again at {pr.title}.")), "the exact text shows each prompt's template");
		dialog.pack_choice()->setCurrentIndex(1);
		expect(dialog.pack_name()->text() == QStringLiteral("Team prompts") && dialog.prompt_pack() && dialog.prompt_pack()->Pack.Id == pack_uuid && dialog.prompt_pack()->Pack.Version == 3U,
			   "an existing pack goes out as its next version under its own ID");
		expect(std::ranges::all_of(dialog.prompt_pack()->After.Own, [](const core::prompt_macro& aMacro) { return aMacro.Pack && aMacro.Pack->Version == 3U; }),
			   "what the export records links every exported prompt at that version");
		render(dialog, aRoot, QStringLiteral("export-prompt-pack"));
	}

	[[nodiscard]] auto pack_preview(const std::uint32_t aVersion) -> application::agent_configuration_preview
	{
		auto preview                                  = application::agent_configuration_preview{ };
		auto edited                                   = linked_to_pack(b_uuid, "Explain", "Explain the change.", 2U);
		edited.Body.Template                          = "Explain the change to a new teammate.";
		auto off                                      = linked_to_pack(a_uuid, "Risks", "What could this break?", 2U);
		off.On                                        = false;
		preview.Snapshot.PromptMacros.Differences.Own = { off, edited, linked_to_pack(c_uuid, "Old one", "An old prompt.", 2U), suggested(d_uuid, "Mine", "My own prompt.") };
		auto       file                               = core::decoded_prompt_pack{ .Pack = { .Id = std::string{ pack_uuid }, .Label = "Team prompts", .Version = aVersion }, .Macros = { } };
		const auto item                               = [&file](const std::string_view aId, const std::string& aName, const std::string& aTemplate) {
			auto macro          = core::portable_prompt_macro{ .Id = std::string{ aId }, .Place = core::prompt_place::agent_tab };
			macro.Body.Name     = aName;
			macro.Body.Template = aTemplate;
			file.Macros.push_back({ .Macro = std::move(macro), .Problem = std::nullopt, .Detail = { } });
		};
		item(a_uuid, "Risks", "What could this break, and for whom?");
		item(b_uuid, "Explain", "Explain the change, briefly.");
		item(e_uuid, "Tests", "Which tests cover this?");
		item("66666666-6666-4666-9666-666666666666", "Commits", "About these commits.");
		file.Macros.back().Problem = core::prompt_pack_item_problem::unsupported;
		file.Macros.back().Detail  = "commit-list";
		preview.File.Version       = 2U;
		preview.File.PromptPack    = std::move(file);
		return preview;
	}

	/// Import: each suggested prompt with its standing, the choice the core
	/// started it at, Replace or Keep mine on an edited copy, Remove or Keep
	/// on one the pack dropped, and an older pack flagged.
	void test_import_prompt_pack(const std::string& aRoot)
	{
		auto  dialog = qt::agent_configuration_import_dialog{ pack_preview(3U), { } };
		auto* risks  = row_named(dialog, QStringLiteral("Risks"));
		auto* expl   = row_named(dialog, QStringLiteral("Explain"));
		auto* tests  = row_named(dialog, QStringLiteral("Tests"));
		auto* old    = row_named(dialog, QStringLiteral("Old one"));
		auto* bad    = row_named(dialog, QStringLiteral("Commits"));
		expect(risks && risks->text().contains(QStringLiteral("Updated")) && risks->text().contains(QStringLiteral("Updated in place")) && risks->checkState() == Qt::Checked,
			   "a prompt not edited since: Updated in place, ticked");
		expect(expl && expl->text().contains(QStringLiteral("Edited here")) && expl->checkState() == Qt::Unchecked && expl->text().contains(QStringLiteral("Your copy kept")),
			   "an edited copy: a conflict, starting at Keep mine");
		expect(tests && tests->text().contains(QStringLiteral("New")) && tests->checkState() == Qt::Checked, "a prompt of no ID here: New, ticked");
		expect(old && old->text().contains(QStringLiteral("Dropped from the pack")) && old->checkState() == Qt::Checked && old->text().contains(QStringLiteral("Removed")),
			   "a prompt the pack dropped starts at Remove, said in the list");
		expect(bad && bad->text().contains(QStringLiteral("commit-list")) && bad->data(Qt::CheckStateRole).isNull(), "an unsupported prompt is listed, named and cannot be ticked");
		expect(row_named(dialog, QStringLiteral("Mine")) == nullptr, "a prompt not from the pack is not in the preview at all");
		const auto start = dialog.prompt_pack_choices();
		expect(start.Take == std::vector<std::size_t>{ 0U, 2U } && start.Remove == std::vector<std::string>{ std::string{ c_uuid } }, "the starting choices are the core's");

		dialog.select_suggested(1U);
		// Shown, the list's own selection builds the detail again: the parts
		// are read once that has settled.
		render(dialog, aRoot, QStringLiteral("import-prompt-pack-conflict"));
		settle();
		auto* const replace = part(dialog, QStringLiteral("Replace"));
		auto* const keep    = part(dialog, QStringLiteral("Keep mine"));
		expect(replace != nullptr && keep != nullptr && keep->isChecked(), "an edited copy offers Replace or Keep mine");
		expect(dialog.yours() && dialog.theirs() && dialog.yours()->toPlainText() == QStringLiteral("Explain the change to a new teammate.")
				   && dialog.theirs()->toPlainText() == QStringLiteral("Explain the change, briefly."),
			   "yours beside the pack's");
		press_space(replace);
		settle();
		expect(dialog.prompt_pack_choices().Take == std::vector<std::size_t>{ 0U, 1U, 2U }, "Replace takes it");
		dialog.select_dropped(0U);
		settle();
		auto* const keepIt = part(dialog, QStringLiteral("Keep"));
		expect(part(dialog, QStringLiteral("Remove")) != nullptr && keepIt != nullptr, "a dropped prompt offers Remove or Keep");
		press_space(keepIt);
		settle();
		expect(dialog.prompt_pack_choices().Remove.empty(), "Keep keeps it");
		expect(dialog.summary()->text().contains(QStringLiteral("1 suggested prompt")) && dialog.apply_button()->isEnabled(), "the footer counts the pack's prompts");
		expect_no_sideways_pan(dialog, "the suggested prompt's detail fits without panning");
		dialog.select_suggested(1U);
		render(dialog, aRoot, QStringLiteral("import-prompt-pack"));

		auto older = qt::agent_configuration_import_dialog{ pack_preview(1U), { } };
		expect(older.pack_plan().Older && older.summary()->text().startsWith(QStringLiteral("This is version 1 of “Team prompts”; version 2 is already here")), "an older pack is flagged first");
		expect(older.prompt_pack_choices().Take == std::vector<std::size_t>{ 2U } && older.prompt_pack_choices().Remove.empty(), "and its updates and removals start left out");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   root  = argc > 2 && std::string_view{ argv[1] } == "--render" ? std::string{ argv[2] } : std::string{ };
	const auto   bytes = test_export(root);
	test_import(bytes, root);
	const auto update = test_workflow(root);
	test_theme_matches_prompts(update, root);
	test_export_prompt_pack(root);
	test_import_prompt_pack(root);
	std::cout << "agent-configuration-dialogs:pass\n";
	return EXIT_SUCCESS;
}
