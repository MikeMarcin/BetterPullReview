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

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyleHints>
#include <QTreeWidget>

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
		auto dialog  = qt::agent_configuration_import_dialog{ preview, { .Skills = { }, .Found = { } } };
		dialog.set_program_chooser([](const QString& aHint) { return aHint == QStringLiteral("lint-bot") ? QStringLiteral("/usr/local/bin/lint-bot") : QString{ }; });
		// Defaults: the colliding Work Claude and the edited prompt are kept;
		// the new Lint bot waits for its program.
		expect(dialog.agent_choices().empty(), "nothing applies before a binding: Work Claude left, Lint bot unbound");
		expect(dialog.summary()->text().contains(QStringLiteral("1 agent waits for a program")), "the waiting agent is said");
		auto prompts = dialog.prompt_choices();
		expect(std::ranges::find_if(prompts, [&](const std::size_t aIndex) { return preview.File.Prompts[aIndex].Prompt.Purpose == "tour.problem"; }) == prompts.end(), "an edited prompt here is kept by default");
		// Lint bot: choose its program.
		dialog.select_agent(1U);
		expect(!dialog.program_button()->isHidden() && dialog.program_line()->text().contains(QStringLiteral("lint-bot")), "the program to choose is named by its file name");
		dialog.program_button()->click();
		expect(dialog.program_line()->text().contains(QStringLiteral("/usr/local/bin/lint-bot")), "bound");
		auto choices = dialog.agent_choices();
		expect(choices.size() == 1U && choices[0].Item == 1U && choices[0].Program == "/usr/local/bin/lint-bot", "a bound agent is applied");
		// Work Claude: Replace shows the differences and the consequence.
		dialog.select_agent(0U);
		expect(dialog.action()->currentText() == QStringLiteral("Leave out"), "a colliding agent starts left out");
		expect(dialog.relaxed_checks()->isHidden(), "no relaxation offered while left out");
		dialog.action()->setFocus();
		dialog.action()->setCurrentIndex(0);
		expect(dialog.action()->currentText() == QStringLiteral("Replace “Work Claude”"), "Replace the one it meets");
		expect(dialog.changes()->text().contains(QStringLiteral("Model: agent's default → opus")), "every changed field is shown");
		expect(dialog.changes()->text().contains(QStringLiteral("can be continued")), "keeping its program, its conversations continue");
		expect(!dialog.relaxed_checks()->isHidden() && !dialog.relaxed_checks()->isChecked() && !dialog.builds()->isHidden() && !dialog.builds()->isChecked(),
			   "relaxed checks and builds are offered, unticked");
		dialog.relaxed_checks()->click();
		expect(dialog.changes()->text().contains(QStringLiteral("no longer be continued")), "relaxing checks changes what it runs: the consequence is stated first");
		choices = dialog.agent_choices();
		expect(choices.size() == 2U && choices[0].AcceptRelaxedChecks && !choices[0].AcceptBuilds, "accepted for this entry only");
		render(dialog, aRoot, QStringLiteral("import-agent"));
		// The edited prompt: compare and choose.
		auto problem = std::size_t{ 0U };
		for (std::size_t index = 0U; index < preview.File.Prompts.size(); ++index) {
			if (preview.File.Prompts[index].Prompt.Purpose == "tour.problem") {
				problem = index;
			}
		}
		dialog.select_prompt(problem);
		expect(!dialog.use_prompt()->isChecked() && dialog.prompt_note()->text().contains(QStringLiteral("written against review-preparation-v1")), "the version it was written against is named");
		expect(dialog.prompt_note()->text().contains(QStringLiteral("/team-style")), "a skill it names that is not here is a visible prerequisite");
		press_space(dialog.use_prompt());
		expect(std::ranges::contains(dialog.prompt_choices(), problem), "chosen by keyboard");
		render(dialog, aRoot, QStringLiteral("import-prompt"));
		expect(dialog.apply_button()->isEnabled(), "Import selected is offered");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   root = argc > 2 && std::string_view{ argv[1] } == "--render" ? std::string{ argv[2] } : std::string{ };
	test_share_row(root);
	const auto bytes = test_export(root);
	test_import(bytes, root);
	std::cout << "agent-configuration-dialogs:pass\n";
	return EXIT_SUCCESS;
}
