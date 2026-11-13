#include "bpr/qt/agent_configuration_dialogs.hpp"

#include "bpr/application/writing_hooks.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <ranges>

namespace bpr::qt {
	namespace {
		constexpr int ItemRole   = Qt::UserRole;
		constexpr int IndexRole  = Qt::UserRole + 1;
		constexpr int AgentKind  = 1;
		constexpr int PromptKind = 2;

		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto note(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setForegroundRole(QPalette::PlaceholderText);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			return label;
		}

		[[nodiscard]] auto heading(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        bold  = label->font();
			bold.setWeight(QFont::DemiBold);
			label->setFont(bold);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			return label;
		}

		[[nodiscard]] auto mono(QWidget* aParent, const QString& aName) -> QPlainTextEdit*
		{
			auto* const text = new QPlainTextEdit{ aParent };
			text->setReadOnly(true);
			text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
			text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
			text->setAccessibleName(aName);
			text->setTabChangesFocus(true);
			return text;
		}

		[[nodiscard]] auto agent_kind(const core::agent_protocol aProtocol, const std::optional<core::setup_agent> aPreset) -> QString
		{
			if (aPreset == core::setup_agent::claude_code) {
				return QObject::tr("Claude Code");
			}
			if (aPreset == core::setup_agent::codex) {
				return QObject::tr("Codex");
			}
			switch (aProtocol) {
			case core::agent_protocol::claude_stream_json: return QObject::tr("Custom agent that speaks Claude Code");
			case core::agent_protocol::codex_app_server:
			case core::agent_protocol::codex_jsonl_exec:   return QObject::tr("Custom agent that speaks Codex");
			case core::agent_protocol::external_terminal:  return QObject::tr("Plain-text harness");
			}
			return QObject::tr("Custom agent");
		}

		[[nodiscard]] auto field_words(const core::agent_field aField) -> QString
		{
			switch (aField) {
			case core::agent_field::name:              return QObject::tr("Name");
			case core::agent_field::protocol:          return QObject::tr("Kind of agent");
			case core::agent_field::program:           return QObject::tr("Program");
			case core::agent_field::arguments:         return QObject::tr("Arguments");
			case core::agent_field::check_arguments:   return QObject::tr("Check arguments");
			case core::agent_field::model:             return QObject::tr("Model");
			case core::agent_field::effort:            return QObject::tr("Effort");
			case core::agent_field::strict_run_checks: return QObject::tr("Strict run checks");
			case core::agent_field::scratch_starts:    return QObject::tr("Scratch folder starts");
			case core::agent_field::builds_and_tests:  return QObject::tr("Build and run tests");
			case core::agent_field::mcp_servers:       return QObject::tr("MCP servers");
			case core::agent_field::instructions:      return QObject::tr("Instructions");
			}
			return { };
		}

		[[nodiscard]] auto field_value(const core::agent_entry& aEntry, const core::agent_field aField) -> QString
		{
			const auto onOff = [](const bool aOn) { return aOn ? QObject::tr("on") : QObject::tr("off"); };
			const auto list  = [](const std::vector<std::string>& aValues) {
				auto words = QStringList{ };
				for (const auto& value : aValues) {
					words.push_back(text_of(value));
				}
				return words.isEmpty() ? QObject::tr("none") : words.join(QLatin1Char(' '));
			};
			const auto orDefault = [](const std::string& aValue) { return aValue.empty() ? QObject::tr("agent's default") : text_of(aValue); };
			switch (aField) {
			case core::agent_field::name:              return text_of(aEntry.Name);
			case core::agent_field::protocol:          return agent_kind(aEntry.Protocol, aEntry.Preset);
			case core::agent_field::program:           return aEntry.Program.empty() ? QObject::tr("looked for when it runs") : text_of(aEntry.Program);
			case core::agent_field::arguments:         return list(aEntry.Arguments);
			case core::agent_field::check_arguments:   return list(aEntry.CheckArguments);
			case core::agent_field::model:             return orDefault(aEntry.Model);
			case core::agent_field::effort:            return orDefault(aEntry.Effort);
			case core::agent_field::strict_run_checks: return onOff(aEntry.StrictRunChecks);
			case core::agent_field::scratch_starts:    return onOff(aEntry.ScratchStarts);
			case core::agent_field::builds_and_tests:  return onOff(aEntry.BuildsAndTests);
			case core::agent_field::mcp_servers:       return aEntry.McpServers == core::agent_mcp_servers::own ? QObject::tr("the agent's own configuration") : QObject::tr("off");
			case core::agent_field::instructions:      return aEntry.Instructions == core::agent_instruction_delivery::message ? QObject::tr("in the message") : QObject::tr("system prompt");
			}
			return { };
		}

		[[nodiscard]] auto counted(const std::size_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return aCount == 1U ? aOne : aMany.arg(aCount);
		}

		[[nodiscard]] auto program_required(const core::portable_agent& aAgent) -> bool
		{
			return !aAgent.Preset || *aAgent.Preset == core::setup_agent::plain_text_harness || aAgent.Protocol == core::agent_protocol::external_terminal;
		}
	} // namespace

	auto prompt_title(const std::string_view aPurpose) -> QString
	{
		if (aPurpose == application::review_skill_purpose) {
			return QObject::tr("Review skill");
		}
		const auto hook = application::prompt_hook(aPurpose);
		if (!hook) {
			return text_of(aPurpose);
		}
		switch (*hook) {
		case core::writing_hook::problem:              return QObject::tr("Problem statement");
		case core::writing_hook::solution:             return QObject::tr("Solution summary");
		case core::writing_hook::outline:              return QObject::tr("Tour outline");
		case core::writing_hook::comment:              return QObject::tr("Tour-stop comment");
		case core::writing_hook::feedback:             return QObject::tr("Review feedback");
		case core::writing_hook::enhance_stop:         return QObject::tr("Enhance a stop");
		case core::writing_hook::enhance_writing:      return QObject::tr("Enhance writing");
		case core::writing_hook::suggested_comment:    return QObject::tr("Suggested comments");
		case core::writing_hook::local_description:    return QObject::tr("Description writing");
		case core::writing_hook::local_discussion:     return QObject::tr("Change discussion");
		case core::writing_hook::local_commit_message: return QObject::tr("Commit message");
		case core::writing_hook::review_latest_upload: return QObject::tr("What changed");
		case core::writing_hook::review_proposed_fix:  return QObject::tr("Was this addressed");
		case core::writing_hook::review_explain_file:  return QObject::tr("Explain this file");
		}
		return text_of(aPurpose);
	}

	// ----- Export -----

	agent_configuration_export_dialog::agent_configuration_export_dialog(application::agent_configuration_snapshot aSnapshot, const std::array<bool, core::writing_hook_count + 1U> aUnsaved,
																		 std::string aExportedBy, QWidget* const aParent)
		: QDialog{ aParent }
		, Snapshot{ std::move(aSnapshot) }
		, Local{ application::local_prompts(Snapshot) }
		, ExportedBy{ std::move(aExportedBy) }
	{
		setWindowTitle(tr("Export configuration"));
		setObjectName(QStringLiteral("agentConfigurationExport"));
		auto* const column = new QVBoxLayout{ this };
		column->addWidget(note(tr("Choose the agents and prompts to share. The file holds what each one is set to, never where its program is on this computer, your accounts or your history. "
								  "Read the text below before you share it: arguments and prompts can hold anything you typed."),
							   this));
		Items = new QTreeWidget{ this };
		Items->setObjectName(QStringLiteral("agentConfigurationExportItems"));
		Items->setAccessibleName(tr("Agents and prompts to export"));
		Items->setColumnCount(2);
		Items->setHeaderLabels({ tr("Item"), tr("What is shared") });
		Items->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		Items->setRootIsDecorated(false);
		auto* const agents = new QTreeWidgetItem{ Items, { tr("Agents") } };
		agents->setFlags(Qt::ItemIsEnabled);
		for (const auto& entry : Snapshot.Agents.Agents) {
			auto* const row = new QTreeWidgetItem{ agents, { text_of(entry.Name), agent_kind(entry.Protocol, entry.Preset) } };
			row->setData(0, ItemRole, AgentKind);
			row->setData(0, IndexRole, entry.Id);
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
			row->setCheckState(0, Qt::Checked);
		}
		if (Snapshot.Agents.Agents.empty()) {
			auto* const none = new QTreeWidgetItem{ agents, { tr("No agents added") } };
			none->setFlags(Qt::ItemIsEnabled);
		}
		auto* const prompts = new QTreeWidgetItem{ Items, { tr("Prompts") } };
		prompts->setFlags(Qt::ItemIsEnabled);
		for (std::size_t index = 0U; index < Local.size(); ++index) {
			const auto& prompt = Local[index];
			const auto  edited = !prompt.Override.empty();
			const auto  says   = edited ? tr("Your edit") + (prompt.WrittenAgainst ? tr(", written against %1").arg(text_of(*prompt.WrittenAgainst)) : QString{ })
										: tr("Built-in text, %1").arg(text_of(prompt.BuiltInVersion));
			auto* const row    = new QTreeWidgetItem{ prompts, { prompt_title(prompt.Purpose), says } };
			row->setData(0, ItemRole, PromptKind);
			row->setData(0, IndexRole, static_cast<qulonglong>(index));
			if (index < aUnsaved.size() && aUnsaved[index]) {
				row->setFlags(Qt::ItemIsSelectable);
				row->setCheckState(0, Qt::Unchecked);
				row->setText(1, tr("Has edits not saved: save or discard them first"));
				row->setToolTip(0, row->text(1));
			} else {
				row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
				row->setCheckState(0, edited ? Qt::Checked : Qt::Unchecked);
			}
		}
		Items->expandAll();
		column->addWidget(Items, 3);
		auto* const tools = new QHBoxLayout{ };
		AllPrompts        = new quiet_push_button{ tr("Choose every prompt"), this };
		AllPrompts->setObjectName(QStringLiteral("agentConfigurationAllPrompts"));
		AllPrompts->setToolTip(tr("Every prompt you can export, your edits and the built-in text of the rest, so you can improve them elsewhere and import them back."));
		tools->addWidget(AllPrompts);
		tools->addStretch(1);
		column->addLayout(tools);
		auto* const form  = new QHBoxLayout{ };
		auto* const named = new QLabel{ tr("&Label"), this };
		Label             = new QLineEdit{ this };
		named->setBuddy(Label);
		Label->setObjectName(QStringLiteral("agentConfigurationLabel"));
		Label->setMaxLength(static_cast<int>(core::maximum_agent_configuration_label_bytes / 4U));
		Label->setPlaceholderText(tr("Optional, for example: Team agents"));
		form->addWidget(named);
		form->addWidget(Label, 1);
		column->addLayout(form);
		column->addWidget(heading(tr("File contents"), this));
		Exact = mono(this, tr("Exact file contents"));
		Exact->setObjectName(QStringLiteral("agentConfigurationExact"));
		column->addWidget(Exact, 2);
		Size = note({ }, this);
		column->addWidget(Size);
		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Cancel, this };
		ExportButton        = buttons->addButton(tr("Export…"), QDialogButtonBox::AcceptRole);
		ExportButton->setObjectName(QStringLiteral("agentConfigurationExportButton"));
		column->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(ExportButton, &QPushButton::clicked, this, [this] {
			if (Bytes.empty() || !Chooser) {
				return;
			}
			Path = Chooser();
			if (!Path.isEmpty()) {
				accept();
			}
		});
		connect(Items, &QTreeWidget::itemChanged, this, [this] { refresh(); });
		connect(Label, &QLineEdit::textChanged, this, [this] { refresh(); });
		connect(AllPrompts, &QPushButton::clicked, this, [prompts] {
			for (int index = 0; index < prompts->childCount(); ++index) {
				if (prompts->child(index)->flags().testFlag(Qt::ItemIsUserCheckable)) {
					prompts->child(index)->setCheckState(0, Qt::Checked);
				}
			}
		});
		resize(760, 820);
		refresh();
	}

	void agent_configuration_export_dialog::set_file_chooser(std::function<QString()> aChooser) { Chooser = std::move(aChooser); }

	auto agent_configuration_export_dialog::agents() const -> std::vector<std::uint32_t>
	{
		auto chosen = std::vector<std::uint32_t>{ };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == AgentKind && (*it)->checkState(0) == Qt::Checked) {
				chosen.push_back((*it)->data(0, IndexRole).toUInt());
			}
		}
		return chosen;
	}

	auto agent_configuration_export_dialog::prompts() const -> std::vector<application::exported_prompt>
	{
		auto chosen = std::vector<application::exported_prompt>{ };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == PromptKind && (*it)->checkState(0) == Qt::Checked && (*it)->flags().testFlag(Qt::ItemIsEnabled)) {
				const auto& prompt = Local[(*it)->data(0, IndexRole).toULongLong()];
				chosen.push_back({ .Purpose = prompt.Purpose, .BuiltIn = prompt.Override.empty() });
			}
		}
		return chosen;
	}

	void agent_configuration_export_dialog::refresh()
	{
		const auto chosenAgents  = agents();
		const auto chosenPrompts = prompts();
		const auto encoded       = application::export_agent_configuration(Snapshot, chosenAgents, chosenPrompts, Label->text().trimmed().toStdString(), ExportedBy);
		Bytes                    = encoded.value_or(std::string{ });
		Exact->setPlainText(text_of(Bytes));
		const auto nothing = chosenAgents.empty() && chosenPrompts.empty();
		Size->setText(!encoded      ? tr("Too large: a configuration file holds at most 1 MiB. Choose fewer prompts.")
						  : nothing ? tr("Choose at least one agent or prompt.")
									: tr("%1 and %2, %3.")
										  .arg(counted(chosenAgents.size(), tr("1 agent"), tr("%1 agents")), counted(chosenPrompts.size(), tr("1 prompt"), tr("%1 prompts")),
											   QLocale{ }.formattedDataSize(static_cast<qint64>(Bytes.size()))));
		ExportButton->setEnabled(encoded.has_value() && !nothing);
	}

	// ----- Import -----

	agent_configuration_import_dialog::agent_configuration_import_dialog(application::agent_configuration_preview aPreview, facts aFacts, QWidget* const aParent)
		: QDialog{ aParent }
		, Preview{ std::move(aPreview) }
		, Facts{ std::move(aFacts) }
		, Agents(Preview.File.Agents.size())
		, Prompts(Preview.File.Prompts.size(), false)
	{
		setWindowTitle(tr("Import configuration"));
		setObjectName(QStringLiteral("agentConfigurationImport"));
		const auto& local = Preview.Snapshot.Agents.Agents;
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			const auto& item = Preview.File.Agents[index];
			if (!item.Problem && core::colliding_agent(local, item.Agent) == nullptr) {
				Agents[index].Action = core::agent_import_action::add;
			}
		}
		for (std::size_t index = 0U; index < Prompts.size(); ++index) {
			const auto& item = Preview.File.Prompts[index];
			// A prompt you have not edited takes the file's; an edited one is
			// kept until you choose otherwise.
			Prompts[index] = !item.Problem && item.Local && Preview.Prompts[*item.Local].Override.empty() && item.Prompt.Text != Preview.Prompts[*item.Local].BuiltIn;
		}
		auto* const column = new QVBoxLayout{ this };
		const auto  from   = Preview.File.Label.empty() ? tr("This file") : tr("“%1”").arg(text_of(Preview.File.Label));
		column->addWidget(note(tr("%1 was exported by %2. Nothing is saved until you choose Import selected, and nothing is run: check an agent afterwards from Agents & context.")
								   .arg(from, Preview.File.ExportedBy.empty() ? tr("an unnamed application") : text_of(Preview.File.ExportedBy)),
							   this));
		auto* const split = new QSplitter{ this };
		Items             = new QTreeWidget{ split };
		Items->setObjectName(QStringLiteral("agentConfigurationImportItems"));
		Items->setAccessibleName(tr("Items in the file"));
		Items->setColumnCount(2);
		Items->setHeaderLabels({ tr("Item"), tr("When imported") });
		Items->setRootIsDecorated(false);
		Items->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		auto* const agentGroup  = new QTreeWidgetItem{ Items, { tr("Agents") } };
		auto* const promptGroup = new QTreeWidgetItem{ Items, { tr("Prompts") } };
		auto* const otherGroup  = new QTreeWidgetItem{ Items, { tr("Not supported by this version") } };
		for (auto* const group : { agentGroup, promptGroup, otherGroup }) {
			group->setFlags(Qt::ItemIsEnabled);
		}
		for (std::size_t index = 0U; index < Preview.File.Agents.size(); ++index) {
			const auto& item   = Preview.File.Agents[index];
			auto* const parent = item.Problem == core::configuration_item_problem::unsupported_protocol ? otherGroup : agentGroup;
			auto* const row    = new QTreeWidgetItem{ parent, { text_of(item.Agent.Name) } };
			row->setData(0, ItemRole, AgentKind);
			row->setData(0, IndexRole, static_cast<qulonglong>(index));
		}
		for (std::size_t index = 0U; index < Preview.File.Prompts.size(); ++index) {
			const auto& item   = Preview.File.Prompts[index];
			auto* const parent = item.Problem == core::configuration_item_problem::unsupported_purpose ? otherGroup : promptGroup;
			auto* const row    = new QTreeWidgetItem{ parent, { item.Local ? prompt_title(Preview.Prompts[*item.Local].Purpose) : text_of(item.Prompt.Purpose) } };
			row->setData(0, ItemRole, PromptKind);
			row->setData(0, IndexRole, static_cast<qulonglong>(index));
		}
		for (auto* const group : { agentGroup, promptGroup, otherGroup }) {
			group->setHidden(group->childCount() == 0);
		}
		Items->expandAll();
		Detail = new QStackedWidget{ split };
		// Page 0: nothing chosen.
		Detail->addWidget(note(tr("Choose an item to see what importing it changes."), Detail));
		// Page 1: an agent.
		auto* const agentPage = new QWidget{ Detail };
		auto* const agentCol  = new QVBoxLayout{ agentPage };
		Title                 = heading({ }, agentPage);
		Kind                  = note({ }, agentPage);
		agentCol->addWidget(Title);
		agentCol->addWidget(Kind);
		auto* const actionRow = new QFormLayout{ };
		Action                = new QComboBox{ agentPage };
		Action->setObjectName(QStringLiteral("agentConfigurationAction"));
		actionRow->addRow(tr("&When imported"), Action);
		agentCol->addLayout(actionRow);
		auto* const programRow = new QHBoxLayout{ };
		ProgramLine            = note({ }, agentPage);
		ProgramLine->setObjectName(QStringLiteral("agentConfigurationProgram"));
		programRow->addWidget(ProgramLine, 1);
		ProgramButton = new quiet_push_button{ tr("Choose program…"), agentPage };
		ProgramButton->setObjectName(QStringLiteral("agentConfigurationChooseProgram"));
		programRow->addWidget(ProgramButton);
		agentCol->addLayout(programRow);
		Relaxed = new QCheckBox{ tr("Allow its relaxed run checks"), agentPage };
		Relaxed->setObjectName(QStringLiteral("agentConfigurationRelaxed"));
		Relaxed->setToolTip(tr("The file asks for this agent's run checks to be relaxed. Left unticked, it is imported with strict checks."));
		agentCol->addWidget(Relaxed);
		Builds = new QCheckBox{ tr("Allow Build and run tests"), agentPage };
		Builds->setObjectName(QStringLiteral("agentConfigurationBuilds"));
		Builds->setToolTip(tr("The file asks for this agent to build and run tests in a disposable checkout. Left unticked, builds stay off."));
		agentCol->addWidget(Builds);
		Changes = note({ }, agentPage);
		Changes->setObjectName(QStringLiteral("agentConfigurationChanges"));
		Changes->setForegroundRole(QPalette::WindowText);
		agentCol->addWidget(Changes);
		agentCol->addStretch(1);
		Detail->addWidget(agentPage);
		// Page 2: a prompt.
		auto* const promptPage = new QWidget{ Detail };
		auto* const promptCol  = new QVBoxLayout{ promptPage };
		PromptTitle            = heading({ }, promptPage);
		promptCol->addWidget(PromptTitle);
		UsePrompt = new QCheckBox{ tr("Use the imported text"), promptPage };
		UsePrompt->setObjectName(QStringLiteral("agentConfigurationUsePrompt"));
		promptCol->addWidget(UsePrompt);
		PromptNote = note({ }, promptPage);
		PromptNote->setObjectName(QStringLiteral("agentConfigurationPromptNote"));
		promptCol->addWidget(PromptNote);
		auto* const compare = new QSplitter{ Qt::Horizontal, promptPage };
		auto* const left    = new QWidget{ compare };
		auto* const leftCol = new QVBoxLayout{ left };
		leftCol->setContentsMargins({ });
		leftCol->addWidget(heading(tr("Yours"), left));
		Yours = mono(left, tr("Your prompt"));
		leftCol->addWidget(Yours);
		auto* const right    = new QWidget{ compare };
		auto* const rightCol = new QVBoxLayout{ right };
		rightCol->setContentsMargins({ });
		rightCol->addWidget(heading(tr("In the file"), right));
		Theirs = mono(right, tr("Imported prompt"));
		rightCol->addWidget(Theirs);
		promptCol->addWidget(compare, 1);
		Detail->addWidget(promptPage);
		// Page 3: an item this version cannot apply.
		auto* const problemPage = new QWidget{ Detail };
		auto* const problemCol  = new QVBoxLayout{ problemPage };
		ProblemTitle            = heading({ }, problemPage);
		ProblemText             = note({ }, problemPage);
		ProblemText->setObjectName(QStringLiteral("agentConfigurationItemProblem"));
		problemCol->addWidget(ProblemTitle);
		problemCol->addWidget(ProblemText);
		problemCol->addStretch(1);
		Detail->addWidget(problemPage);
		Items->setMinimumWidth(330);
		Items->header()->setStretchLastSection(true);
		split->setStretchFactor(0, 2);
		split->setStretchFactor(1, 3);
		split->setSizes({ 380, 560 });
		column->addWidget(split, 1);
		Problem = new QLabel{ this };
		Problem->setObjectName(QStringLiteral("agentConfigurationProblem"));
		Problem->setWordWrap(true);
		Problem->setForegroundRole(QPalette::Highlight);
		Problem->hide();
		column->addWidget(Problem);
		Summary = note({ }, this);
		Summary->setObjectName(QStringLiteral("agentConfigurationSummary"));
		Summary->setForegroundRole(QPalette::WindowText);
		column->addWidget(Summary);
		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Cancel, this };
		ApplyButton         = buttons->addButton(tr("Import selected"), QDialogButtonBox::AcceptRole);
		ApplyButton->setObjectName(QStringLiteral("agentConfigurationApply"));
		column->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(ApplyButton, &QPushButton::clicked, this, &QDialog::accept);
		connect(Items, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* aItem) { show_item(aItem); });
		connect(Action, &QComboBox::currentIndexChanged, this, [this](const int aIndex) {
			if (Showing || !ShownAgent || aIndex < 0 || static_cast<std::size_t>(aIndex) >= Options.size()) {
				return;
			}
			Agents[*ShownAgent].Action = Options[static_cast<std::size_t>(aIndex)].Action;
			Agents[*ShownAgent].Target = Options[static_cast<std::size_t>(aIndex)].Target;
			show_agent(*ShownAgent);
			refresh_rows();
		});
		connect(ProgramButton, &QPushButton::clicked, this, [this] {
			if (!ShownAgent || !Chooser) {
				return;
			}
			const auto chosen = Chooser(text_of(Preview.File.Agents[*ShownAgent].Agent.ProgramName));
			if (!chosen.isEmpty()) {
				Agents[*ShownAgent].Program = chosen.toStdString();
				show_agent(*ShownAgent);
				refresh_rows();
			}
		});
		connect(Relaxed, &QCheckBox::toggled, this, [this](const bool aOn) {
			if (!Showing && ShownAgent) {
				Agents[*ShownAgent].AcceptRelaxed = aOn;
				show_agent(*ShownAgent);
			}
		});
		connect(Builds, &QCheckBox::toggled, this, [this](const bool aOn) {
			if (!Showing && ShownAgent) {
				Agents[*ShownAgent].AcceptBuilds = aOn;
				show_agent(*ShownAgent);
			}
		});
		connect(UsePrompt, &QCheckBox::toggled, this, [this](const bool aOn) {
			if (!Showing && ShownPrompt) {
				Prompts[*ShownPrompt] = aOn;
				refresh_rows();
			}
		});
		resize(980, 660);
		refresh_rows();
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).isValid()) {
				Items->setCurrentItem(*it);
				break;
			}
		}
	}

	void agent_configuration_import_dialog::set_program_chooser(std::function<QString(const QString&)> aChooser) { Chooser = std::move(aChooser); }

	void agent_configuration_import_dialog::show_problem(const QString& aText)
	{
		Problem->setText(aText);
		Problem->setVisible(!aText.isEmpty());
	}

	auto agent_configuration_import_dialog::needs_program(const std::size_t aItem) const -> bool
	{
		const auto& state = Agents[aItem];
		return state.Action == core::agent_import_action::add && program_required(Preview.File.Agents[aItem].Agent);
	}

	auto agent_configuration_import_dialog::bound(const std::size_t aItem) const -> bool { return !needs_program(aItem) || !Agents[aItem].Program.empty(); }

	auto agent_configuration_import_dialog::agent_choices() const -> std::vector<core::agent_import_choice>
	{
		auto choices = std::vector<core::agent_import_choice>{ };
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			const auto& state = Agents[index];
			if (Preview.File.Agents[index].Problem || state.Action == core::agent_import_action::leave || !bound(index)) {
				continue;
			}
			choices.push_back({ .Item = index, .Action = state.Action, .Target = state.Target, .Program = state.Program, .AcceptRelaxedChecks = state.AcceptRelaxed, .AcceptBuilds = state.AcceptBuilds });
		}
		return choices;
	}

	auto agent_configuration_import_dialog::prompt_choices() const -> std::vector<std::size_t>
	{
		auto chosen = std::vector<std::size_t>{ };
		for (std::size_t index = 0U; index < Prompts.size(); ++index) {
			if (Prompts[index] && !Preview.File.Prompts[index].Problem) {
				chosen.push_back(index);
			}
		}
		return chosen;
	}

	auto agent_configuration_import_dialog::row_state(const std::size_t aItem) const -> QString
	{
		const auto& item = Preview.File.Agents[aItem];
		if (item.Problem) {
			return tr("Cannot be imported");
		}
		const auto& state  = Agents[aItem];
		const auto& local  = Preview.Snapshot.Agents.Agents;
		const auto  target = std::ranges::find(local, state.Target, &core::agent_entry::Id);
		switch (state.Action) {
		case core::agent_import_action::leave:        return tr("Left out");
		case core::agent_import_action::add:          return bound(aItem) ? tr("Added") : tr("Needs a program");
		case core::agent_import_action::use_existing: return target == local.end() ? tr("Left out") : tr("Uses %1").arg(text_of(target->Name));
		case core::agent_import_action::replace:      return target == local.end() ? tr("Left out") : tr("Replaces %1").arg(text_of(target->Name));
		}
		return { };
	}

	void agent_configuration_import_dialog::refresh_rows()
	{
		auto agents      = std::size_t{ 0U };
		auto waiting     = std::size_t{ 0U };
		auto unsupported = std::size_t{ 0U };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			const auto kind  = (*it)->data(0, ItemRole).toInt();
			const auto index = static_cast<std::size_t>((*it)->data(0, IndexRole).toULongLong());
			if (kind == AgentKind) {
				(*it)->setText(1, row_state(index));
			} else if (kind == PromptKind) {
				const auto& item = Preview.File.Prompts[index];
				(*it)->setText(1, item.Problem ? tr("Cannot be imported") : item.Local && item.Prompt.Text == (Preview.Prompts[*item.Local].Override.empty() ? Preview.Prompts[*item.Local].BuiltIn : Preview.Prompts[*item.Local].Override) ? tr("Same as yours")
								   : Prompts[index]                                                                                                                                                                                          ? tr("Used")
																																																											 : tr("Yours kept"));
			}
		}
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			unsupported += Preview.File.Agents[index].Problem ? 1U : 0U;
			waiting += !Preview.File.Agents[index].Problem && !bound(index) ? 1U : 0U;
		}
		for (const auto& item : Preview.File.Prompts) {
			unsupported += item.Problem ? 1U : 0U;
		}
		agents             = agent_choices().size();
		const auto prompts = prompt_choices().size();
		auto       words   = tr("Will import %1 and %2.").arg(counted(agents, tr("1 agent"), tr("%1 agents")), counted(prompts, tr("1 prompt"), tr("%1 prompts")));
		if (waiting > 0U) {
			words += QLatin1Char(' ') + counted(waiting, tr("1 agent waits for a program and is left out until it has one."), tr("%1 agents wait for a program and are left out until they have one."));
		}
		if (unsupported > 0U) {
			words += QLatin1Char(' ') + counted(unsupported, tr("1 item cannot be imported by this version."), tr("%1 items cannot be imported by this version."));
		}
		Summary->setText(words);
		ApplyButton->setEnabled(agents + prompts > 0U);
	}

	void agent_configuration_import_dialog::select_agent(const std::size_t aItem)
	{
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == AgentKind && (*it)->data(0, IndexRole).toULongLong() == aItem) {
				Items->setCurrentItem(*it);
			}
		}
	}

	void agent_configuration_import_dialog::select_prompt(const std::size_t aItem)
	{
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == PromptKind && (*it)->data(0, IndexRole).toULongLong() == aItem) {
				Items->setCurrentItem(*it);
			}
		}
	}

	void agent_configuration_import_dialog::show_item(QTreeWidgetItem* const aItem)
	{
		ShownAgent.reset();
		ShownPrompt.reset();
		if (aItem == nullptr || !aItem->data(0, ItemRole).isValid()) {
			Detail->setCurrentIndex(0);
			return;
		}
		const auto index = static_cast<std::size_t>(aItem->data(0, IndexRole).toULongLong());
		if (aItem->data(0, ItemRole).toInt() == AgentKind) {
			show_agent(index);
		} else {
			show_prompt(index);
		}
	}

	void agent_configuration_import_dialog::show_agent(const std::size_t aItem)
	{
		const auto& item  = Preview.File.Agents[aItem];
		const auto& agent = item.Agent;
		if (item.Problem) {
			ProblemTitle->setText(text_of(agent.Name));
			auto why = text_of(core::describe(*item.Problem));
			if (item.Refusal) {
				why += QLatin1Char(' ') + text_of(core::describe(*item.Refusal));
			}
			if (!item.Unsupported.empty()) {
				why += QLatin1Char(' ') + tr("It names “%1”.").arg(text_of(item.Unsupported));
			}
			ProblemText->setText(why + QLatin1Char(' ') + tr("The rest of the file can still be imported."));
			Detail->setCurrentIndex(3);
			return;
		}
		Showing           = true;
		ShownAgent        = aItem;
		auto&       state = Agents[aItem];
		const auto& local = Preview.Snapshot.Agents.Agents;
		Title->setText(text_of(agent.Name));
		Kind->setText(agent_kind(agent.Protocol, agent.Preset));
		// What it can do: add when it meets no agent here; replace the one it
		// meets; use any agent of the same kind as it is; or leave it out.
		Options.clear();
		Action->clear();
		const auto* const meets = core::colliding_agent(local, agent);
		if (meets == nullptr) {
			Options.push_back({ core::agent_import_action::add, 0U });
			Action->addItem(tr("Add as a new agent"));
		} else {
			Options.push_back({ core::agent_import_action::replace, meets->Id });
			Action->addItem(tr("Replace “%1”").arg(text_of(meets->Name)));
		}
		for (const auto& entry : local) {
			if (core::compatible_agent(entry, agent)) {
				Options.push_back({ core::agent_import_action::use_existing, entry.Id });
				Action->addItem(tr("Use “%1” as it is").arg(text_of(entry.Name)));
			}
		}
		Options.push_back({ core::agent_import_action::leave, 0U });
		Action->addItem(tr("Leave out"));
		const auto current = std::ranges::find_if(Options, [&state](const action_option& aOption) {
			return aOption.Action == state.Action && (aOption.Action == core::agent_import_action::add || aOption.Action == core::agent_import_action::leave || aOption.Target == state.Target);
		});
		if (current == Options.end()) {
			state.Action = core::agent_import_action::leave;
		}
		Action->setCurrentIndex(current == Options.end() ? static_cast<int>(Options.size()) - 1 : static_cast<int>(current - Options.begin()));
		const auto        target    = std::ranges::find(local, state.Target, &core::agent_entry::Id);
		const auto* const targetPtr = state.Action == core::agent_import_action::replace && target != local.end() ? &*target : nullptr;
		// The program this computer runs it with.
		const auto adding    = state.Action == core::agent_import_action::add;
		const auto replacing = targetPtr != nullptr;
		ProgramButton->setVisible((adding && program_required(agent)) || (replacing && program_required(agent)));
		if (!program_required(agent)) {
			const auto found = std::ranges::contains(Facts.Found, *agent.Preset);
			ProgramLine->setText(found ? tr("Program: %1 found on this computer, looked for each time it runs.").arg(agent_kind(agent.Protocol, agent.Preset))
									   : tr("Program: not found on this computer yet; it is looked for each time it runs."));
		} else if (!state.Program.empty()) {
			ProgramLine->setText(tr("Program: %1").arg(text_of(state.Program)));
		} else if (replacing) {
			ProgramLine->setText(tr("Program: keeps %1").arg(target->Program.empty() ? tr("its own") : text_of(target->Program)));
		} else {
			ProgramLine->setText(agent.ProgramName.empty() ? tr("Program: choose the program that runs it on this computer.")
														   : tr("Program: choose this computer's copy of “%1”.").arg(text_of(agent.ProgramName)));
		}
		ProgramLine->setVisible(adding || replacing);
		Relaxed->setVisible(!agent.StrictRunChecks && !agent.Preset && (adding || replacing));
		Relaxed->setChecked(state.AcceptRelaxed);
		Builds->setVisible(agent.BuildsAndTests && (adding || replacing));
		Builds->setChecked(state.AcceptBuilds);
		// What it changes, and what happens to the conversations.
		auto words = QStringList{ };
		if (replacing) {
			const auto choice = core::agent_import_choice{ .Item = aItem, .Action = state.Action, .Target = state.Target, .Program = state.Program, .AcceptRelaxedChecks = state.AcceptRelaxed, .AcceptBuilds = state.AcceptBuilds };
			const auto after  = core::imported_entry(agent, choice, targetPtr);
			const auto fields = core::agent_differences(*targetPtr, after);
			if (fields.empty()) {
				words.push_back(tr("Nothing changes: “%1” already has these settings.").arg(text_of(targetPtr->Name)));
			} else {
				words.push_back(tr("Changes:"));
				for (const auto field : fields) {
					words.push_back(tr("  %1: %2 → %3").arg(field_words(field), field_value(*targetPtr, field), field_value(after, field)));
				}
			}
			words.push_back(core::keeps_agent_identity(*targetPtr, after) ? tr("Its conversations stay and can be continued.")
																		  : tr("Its conversations stay readable but can no longer be continued, because what it runs changes."));
		} else if (adding) {
			const auto choice = core::agent_import_choice{ .Item = aItem, .Action = state.Action, .Program = state.Program.empty() ? std::string{ } : state.Program, .AcceptRelaxedChecks = state.AcceptRelaxed, .AcceptBuilds = state.AcceptBuilds };
			const auto after  = core::imported_entry(agent, choice, nullptr);
			for (const auto field : { core::agent_field::arguments, core::agent_field::check_arguments, core::agent_field::model, core::agent_field::effort, core::agent_field::strict_run_checks,
									  core::agent_field::scratch_starts, core::agent_field::builds_and_tests, core::agent_field::mcp_servers, core::agent_field::instructions }) {
				words.push_back(tr("%1: %2").arg(field_words(field), field_value(after, field)));
			}
			words.push_back(tr("Added at the end of your agents, not yet checked on this computer."));
		} else if (state.Action == core::agent_import_action::use_existing && target != local.end()) {
			words.push_back(tr("“%1” stands for it and is not changed.").arg(text_of(target->Name)));
		} else {
			words.push_back(tr("Not imported. Import the file again to decide later."));
		}
		Changes->setText(words.join(QLatin1Char('\n')));
		Detail->setCurrentIndex(1);
		Showing = false;
	}

	void agent_configuration_import_dialog::show_prompt(const std::size_t aItem)
	{
		const auto& item = Preview.File.Prompts[aItem];
		if (item.Problem || !item.Local) {
			ProblemTitle->setText(text_of(item.Prompt.Purpose));
			ProblemText->setText(text_of(core::describe(item.Problem.value_or(core::configuration_item_problem::unsupported_purpose))) + QLatin1Char(' ')
								 + tr("It is not saved. The rest of the file can still be imported."));
			Detail->setCurrentIndex(3);
			return;
		}
		Showing           = true;
		ShownPrompt       = aItem;
		const auto& local = Preview.Prompts[*item.Local];
		const auto  mine  = local.Override.empty() ? local.BuiltIn : local.Override;
		const auto  same  = item.Prompt.Text == mine;
		PromptTitle->setText(prompt_title(local.Purpose));
		UsePrompt->setChecked(Prompts[aItem]);
		UsePrompt->setEnabled(!same);
		auto words = QStringList{ };
		words.push_back(local.Override.empty() ? tr("Yours is the built-in text, %1.").arg(text_of(local.BuiltInVersion)) : tr("Yours is edited."));
		if (item.Prompt.Origin == core::portable_prompt_origin::built_in) {
			words.push_back(tr("The file's is the built-in text of %1.").arg(text_of(item.Prompt.WrittenAgainst.value_or(std::string{ "an unnamed version" }))));
		} else if (item.Prompt.WrittenAgainst) {
			words.push_back(*item.Prompt.WrittenAgainst == local.BuiltInVersion ? tr("The file's was written against %1, the built-in this version has.").arg(text_of(local.BuiltInVersion))
																				: tr("The file's was written against %1; this version's built-in is %2.")
																					  .arg(text_of(*item.Prompt.WrittenAgainst), text_of(local.BuiltInVersion)));
		} else {
			words.push_back(tr("The file does not say which built-in version its text was written against."));
		}
		if (same) {
			words.push_back(tr("The text is the same as yours."));
		} else if (item.Prompt.Text == local.BuiltIn) {
			words.push_back(tr("Using it restores the built-in text."));
		} else {
			words.push_back(tr("Using it makes it your edited prompt; Reset to built-in still brings the built-in text back."));
		}
		const auto texts    = std::array{ std::string_view{ item.Prompt.Text } };
		const auto resolved = core::resolve_skill_references(texts, Facts.Skills);
		if (!resolved.Unknown.empty()) {
			auto names = QStringList{ };
			for (const auto& name : resolved.Unknown) {
				names.push_back(QLatin1Char('/') + text_of(name));
			}
			words.push_back(tr("Names skills not found on this computer: %1. They stay as written; nothing is installed.").arg(names.join(QStringLiteral(", "))));
		}
		PromptNote->setText(words.join(QLatin1Char(' ')));
		Yours->setPlainText(text_of(mine));
		Theirs->setPlainText(text_of(item.Prompt.Text));
		Detail->setCurrentIndex(2);
		Showing = false;
	}
} // namespace bpr::qt
