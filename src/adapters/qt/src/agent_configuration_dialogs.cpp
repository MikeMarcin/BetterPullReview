#include "bpr/qt/agent_configuration_dialogs.hpp"

#include "bpr/application/writing_hooks.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "pull_request_overview_support.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
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
		/// [[BPR-2032]].
		constexpr int WorkflowKind = 3;

		/// A dialog is a window, and Qt does not pass a window's palette to
		/// the windows it opens unless asked (owner, look workflow-import-v002:
		/// "Something went way wrong with the colors"). Asked, and attached
		/// to its opener again so the asking takes effect, it inherits the
		/// opener's palette and font and follows every later change; set
		/// explicitly, it would keep the first one.
		void adopt_opener_look(QWidget& aDialog, QWidget* const aOpener)
		{
			aDialog.setAttribute(Qt::WA_WindowPropagation);
			if (aOpener != nullptr) {
				aDialog.setParent(aOpener, aDialog.windowFlags());
			}
		}

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
		case core::writing_hook::problem:               return QObject::tr("Problem statement");
		case core::writing_hook::solution:              return QObject::tr("Solution summary");
		case core::writing_hook::outline:               return QObject::tr("Tour outline");
		case core::writing_hook::comment:               return QObject::tr("Tour-stop comment");
		case core::writing_hook::feedback:              return QObject::tr("Review feedback");
		case core::writing_hook::enhance_stop:          return QObject::tr("Enhance a stop");
		case core::writing_hook::enhance_writing:       return QObject::tr("Enhance writing");
		case core::writing_hook::suggested_comment:     return QObject::tr("Suggested comments");
		case core::writing_hook::local_description:     return QObject::tr("Description writing");
		case core::writing_hook::local_discussion:      return QObject::tr("Change discussion");
		case core::writing_hook::local_commit_message:  return QObject::tr("Commit message");
		case core::writing_hook::review_latest_upload:  return QObject::tr("What changed");
		case core::writing_hook::review_proposed_fix:   return QObject::tr("Was this addressed");
		case core::writing_hook::review_explain_file:   return QObject::tr("Explain this file");
		case core::writing_hook::review_suggest_change: return QObject::tr("Suggest the change");
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
		adopt_opener_look(*this, aParent);
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
		// [[BPR-2032]]. Workflows start unchosen; a chosen one brings its agents.
		if (!Snapshot.Workflows.empty()) {
			auto* const workflows = new QTreeWidgetItem{ Items, { tr("Workflows") } };
			workflows->setFlags(Qt::ItemIsEnabled);
			for (std::size_t index = 0U; index < Snapshot.Workflows.size(); ++index) {
				const auto& workflow = Snapshot.Workflows[index].Workflow;
				auto* const row      = new QTreeWidgetItem{ workflows, { text_of(workflow.Name), tr("Its actions and guidance, with the agents it asks; no program paths") } };
				row->setData(0, ItemRole, WorkflowKind);
				row->setData(0, IndexRole, static_cast<qulonglong>(index));
				row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
				row->setCheckState(0, Qt::Unchecked);
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

	auto agent_configuration_export_dialog::workflows() const -> std::vector<std::string>
	{
		auto chosen = std::vector<std::string>{ };
		for (QTreeWidgetItemIterator it{ Items }; *it != nullptr; ++it) {
			if ((*it)->data(0, ItemRole).toInt() == WorkflowKind && (*it)->checkState(0) == Qt::Checked) {
				chosen.push_back(Snapshot.Workflows[(*it)->data(0, IndexRole).toULongLong()].Workflow.PortableId);
			}
		}
		return chosen;
	}

	void agent_configuration_export_dialog::refresh()
	{
		const auto chosenAgents    = agents();
		const auto chosenPrompts   = prompts();
		const auto chosenWorkflows = workflows();
		const auto encoded         = application::export_agent_configuration(Snapshot, chosenAgents, chosenPrompts, Label->text().trimmed().toStdString(), ExportedBy, chosenWorkflows);
		Bytes                      = encoded.value_or(std::string{ });
		Exact->setPlainText(text_of(Bytes));
		const auto nothing = chosenAgents.empty() && chosenPrompts.empty() && chosenWorkflows.empty();
		Size->setText(!encoded      ? tr("Too large, or a chosen workflow's agent is no longer here: a configuration file holds at most 1 MiB.")
						  : nothing ? tr("Choose at least one agent or prompt.")
									: tr("%1 and %2, %3.")
										  .arg(counted(chosenAgents.size(), tr("1 agent"), tr("%1 agents")), counted(chosenPrompts.size(), tr("1 prompt"), tr("%1 prompts")),
											   QLocale{ }.formattedDataSize(static_cast<qint64>(Bytes.size()))));
		ExportButton->setEnabled(encoded.has_value() && !nothing);
	}

	// ----- Import -----

	namespace {
		constexpr int KindRole   = Qt::UserRole + 2;
		constexpr int HeaderRole = Qt::UserRole + 3;
		/// A choice's parts: the file's (Add or Update), one of yours, none.
		constexpr int TakePart  = 0;
		constexpr int UsePart   = 1;
		constexpr int LeavePart = 2;

		[[nodiscard]] auto standing_tag(const core::import_standing aStanding) -> QString
		{
			switch (aStanding) {
			case core::import_standing::added:     return QObject::tr("New");
			case core::import_standing::updated:   return QObject::tr("Updated");
			case core::import_standing::unchanged: return QObject::tr("Unchanged");
			case core::import_standing::refused:   return QObject::tr("Cannot be imported");
			}
			return { };
		}

		/// A path or file name wraps at its separators rather than widen the pane.
		[[nodiscard]] auto breakable(const QString& aText) -> QString
		{
			auto out = aText;
			out.replace(QLatin1Char('/'), QStringLiteral("/​"));
			return out;
		}

		[[nodiscard]] auto escaped(const std::string_view aText) -> QString { return text_of(aText).toHtmlEscaped(); }

		/// A section of a detail: a quiet heading over grouped rows, as Settings draws.
		[[nodiscard]] auto group_box(QWidget* aParent) -> std::pair<QFrame*, QVBoxLayout*>
		{
			// The Prompts page's bordered group: the panel shows through.
			auto* const frame = new QFrame{ aParent };
			frame->setFrameShape(QFrame::StyledPanel);
			auto* const layout = new QVBoxLayout{ frame };
			layout->setContentsMargins(10, 8, 10, 8);
			layout->setSpacing(6);
			return { frame, layout };
		}

		/// A part of one choice, drawn as the Prompts page draws its own (the
		/// Overview's Open and All): the chosen part a quiet chip.
		[[nodiscard]] auto segmented(const QString& aText, QWidget* const aParent) -> QToolButton*
		{
			auto* const button = new counts_mode_button{ aParent };
			button->setText(aText);
			button->setCheckable(true);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			return button;
		}

		[[nodiscard]] auto rich(const QString& aHtml, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aHtml, aParent };
			label->setTextFormat(Qt::RichText);
			label->setWordWrap(true);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			return label;
		}
	} // namespace

	agent_configuration_import_dialog::agent_configuration_import_dialog(application::agent_configuration_preview aPreview, facts aFacts, QWidget* const aParent)
		: QDialog{ aParent }
		, Preview{ std::move(aPreview) }
		, Facts{ std::move(aFacts) }
		, Plan{ core::plan_configuration_import(Preview.File, Preview.Snapshot.Agents.Agents, Preview.Prompts, Preview.Snapshot.Workflows) }
		, Agents(Preview.File.Agents.size())
		, Prompts(Preview.File.Prompts.size(), false)
		, Workflows(Preview.File.Workflows.size())
	{
		adopt_opener_look(*this, aParent);
		setWindowTitle(tr("Import Profile Settings"));
		setObjectName(QStringLiteral("agentConfigurationImport"));
		// The core's starting choices: everything the file can bring.
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			const auto& plan     = Plan.Agents[index];
			Agents[index].Action = plan.Default.Action;
			Agents[index].Target = plan.Default.Target;
			Agents[index].Ticked = plan.Default.Action != core::agent_import_action::leave ? plan.Default.Action
				: plan.StandIns.empty()                                                    ? core::agent_import_action::leave
																						   : core::agent_import_action::use_existing;
		}
		for (std::size_t index = 0U; index < Prompts.size(); ++index) {
			Prompts[index] = Plan.Prompts[index].Default;
		}
		for (std::size_t index = 0U; index < Workflows.size(); ++index) {
			Workflows[index].Action = Plan.Workflows[index].Default;
		}

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(20, 18, 20, 16);
		column->setSpacing(10);
		const auto  label = Preview.File.Label.empty() ? tr("Import profile settings") : tr("Import “%1”").arg(text_of(Preview.File.Label));
		auto* const title = heading(label, this);
		auto        big   = title->font();
		big.setPointSizeF(big.pointSizeF() * 1.25);
		title->setFont(big);
		column->addWidget(title);
		const auto  by    = Preview.File.ExportedBy.empty() ? tr("Exported by an unnamed application.") : tr("Exported by %1.").arg(text_of(Preview.File.ExportedBy));
		const auto  where = Facts.Profile.empty() ? tr("Importing saves what is ticked here into this profile and runs nothing.")
												  : tr("Importing saves what is ticked here into the profile “%1” and runs nothing.").arg(text_of(Facts.Profile));
		auto* const intro = note(by + QLatin1Char(' ') + where + QLatin1Char(' ') + tr("No program paths, accounts, sign-ins or history came with the file."), this);
		intro->setObjectName(QStringLiteral("agentConfigurationIntro"));
		column->addWidget(intro);

		// The Prompt library's shape: the list at a fixed width, the detail beside it.
		auto* const body = new QHBoxLayout{ };
		body->setSpacing(12);
		Items = new QListWidget{ this };
		Items->setObjectName(QStringLiteral("agentConfigurationImportItems"));
		Items->setAccessibleName(tr("Items in the file"));
		Items->setFixedWidth(272);
		Items->setMinimumHeight(360);
		Items->setSelectionMode(QAbstractItemView::SingleSelection);
		Items->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Items->setWordWrap(true);
		Items->setTextElideMode(Qt::ElideNone);
		body->addWidget(Items, 0);
		auto* const pane = new QFrame{ this };
		pane->setObjectName(QStringLiteral("agentConfigurationDetail"));
		pane->setFrameShape(QFrame::StyledPanel);
		pane->setAutoFillBackground(true);
		pane->setBackgroundRole(QPalette::AlternateBase);
		// The narrowest the detail goes: its widest unbreakable part, a
		// choice of three or a permission's tick, fits inside it.
		pane->setMinimumWidth(460);
		auto* const paneColumn = new QVBoxLayout{ pane };
		paneColumn->setContentsMargins(0, 0, 0, 0);
		Scroll = new QScrollArea{ pane };
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setWidgetResizable(true);
		// Every line wraps, so there is never anything to pan to.
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->viewport()->setAutoFillBackground(false);
		paneColumn->addWidget(Scroll);
		body->addWidget(pane, 1);
		column->addLayout(body, 1);

		Problem = new QLabel{ this };
		Problem->setObjectName(QStringLiteral("agentConfigurationProblem"));
		Problem->setWordWrap(true);
		Problem->setForegroundRole(QPalette::Highlight);
		Problem->hide();
		column->addWidget(Problem);
		auto* const footer = new QHBoxLayout{ };
		footer->setSpacing(12);
		Summary = note({ }, this);
		Summary->setObjectName(QStringLiteral("agentConfigurationSummary"));
		footer->addWidget(Summary, 1);
		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Cancel, this };
		ApplyButton         = buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
		ApplyButton->setObjectName(QStringLiteral("agentConfigurationApply"));
		ApplyButton->setDefault(true);
		footer->addWidget(buttons, 0, Qt::AlignBottom);
		column->addLayout(footer);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(ApplyButton, &QPushButton::clicked, this, &QDialog::accept);

		fill_list();
		connect(Items, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* const aItem) {
			if (Filling || aItem == nullptr || aItem->data(HeaderRole).toBool()) {
				return;
			}
			Shown = std::pair{ static_cast<item_kind>(aItem->data(KindRole).toInt()), static_cast<std::size_t>(aItem->data(IndexRole).toULongLong()) };
			// Rebuilt once the list's own signal returns.
			QTimer::singleShot(0, this, [this] { show_selected(); });
		});
		connect(Items, &QListWidget::itemChanged, this, [this](QListWidgetItem* const aItem) {
			if (Filling || aItem == nullptr || !aItem->flags().testFlag(Qt::ItemIsUserCheckable)) {
				return;
			}
			const auto kind  = static_cast<item_kind>(aItem->data(KindRole).toInt());
			const auto index = static_cast<std::size_t>(aItem->data(IndexRole).toULongLong());
			const auto on    = aItem->checkState() == Qt::Checked;
			auto       part  = on ? TakePart : LeavePart;
			auto       whom  = std::uint32_t{ 0U };
			if (on && kind == item_kind::agent && Agents[index].Ticked == core::agent_import_action::use_existing) {
				part = UsePart;
				whom = Agents[index].Target != 0U ? Agents[index].Target : Plan.Agents[index].StandIns.front();
			}
			QTimer::singleShot(0, this, [this, kind, index, part, whom] { choose(kind, index, part, whom); });
		});
		resize(980, 680);
		refresh_rows();
		for (int row = 0; row < Items->count(); ++row) {
			if (!Items->item(row)->data(HeaderRole).toBool()) {
				Items->setCurrentRow(row);
				break;
			}
		}
		show_selected();
	}

	void agent_configuration_import_dialog::changeEvent(QEvent* const aEvent)
	{
		QDialog::changeEvent(aEvent);
		// The detail's rich text carries colours from the palette: a theme
		// change draws it again.
		if ((aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) && Scroll != nullptr) {
			QTimer::singleShot(0, this, [this] { show_selected(); });
		}
	}

	void agent_configuration_import_dialog::set_program_chooser(std::function<QString(const QString&)> aChooser) { Chooser = std::move(aChooser); }

	void agent_configuration_import_dialog::show_problem(const QString& aText)
	{
		Problem->setText(aText);
		Problem->setVisible(!aText.isEmpty());
	}

	auto agent_configuration_import_dialog::bound(const std::size_t aItem) const -> bool
	{
		return Agents[aItem].Action != core::agent_import_action::add || !Plan.Agents[aItem].NeedsProgram || !Agents[aItem].Program.empty();
	}

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
			if (Prompts[index] && Plan.Prompts[index].Standing == core::import_standing::updated) {
				chosen.push_back(index);
			}
		}
		return chosen;
	}

	auto agent_configuration_import_dialog::workflow_choices() const -> std::vector<core::workflow_import_choice>
	{
		auto choices = std::vector<core::workflow_import_choice>{ };
		for (std::size_t index = 0U; index < Workflows.size(); ++index) {
			if (!Preview.File.Workflows[index].Problem && Workflows[index].Action != core::workflow_import_action::leave) {
				choices.push_back({ .Item = index, .Action = Workflows[index].Action, .Programs = Workflows[index].Programs, .Agents = { } });
			}
		}
		return choices;
	}

	auto agent_configuration_import_dialog::standing(const item_kind aKind, const std::size_t aItem) const -> core::import_standing
	{
		switch (aKind) {
		case item_kind::agent:    return Plan.Agents[aItem].Standing;
		case item_kind::prompt:   return Plan.Prompts[aItem].Standing;
		case item_kind::workflow: return Plan.Workflows[aItem].Standing;
		}
		return core::import_standing::refused;
	}

	auto agent_configuration_import_dialog::chosen(const item_kind aKind, const std::size_t aItem) const -> bool
	{
		switch (aKind) {
		case item_kind::agent:    return Agents[aItem].Action != core::agent_import_action::leave;
		case item_kind::prompt:   return Prompts[aItem];
		case item_kind::workflow: return Workflows[aItem].Action != core::workflow_import_action::leave;
		}
		return false;
	}

	auto agent_configuration_import_dialog::local_name(const std::uint32_t aId) const -> QString
	{
		const auto& local = Preview.Snapshot.Agents.Agents;
		const auto  entry = std::ranges::find(local, aId, &core::agent_entry::Id);
		return entry == local.end() ? QString{ } : text_of(entry->Name);
	}

	auto agent_configuration_import_dialog::asking_actions(const std::size_t aAgent) const -> QStringList
	{
		auto tasks = QStringList{ };
		for (const auto& workflow : Preview.File.Workflows) {
			for (const auto& action : workflow.Workflow.Actions) {
				if (action.Binding == core::workflow_binding::agent && action.Agent == Preview.File.Agents[aAgent].Agent.PortableId) {
					tasks << tr("“%1”").arg(text_of(action.Task));
				}
			}
		}
		return tasks;
	}

	auto agent_configuration_import_dialog::row_line(const item_kind aKind, const std::size_t aItem) const -> QString
	{
		const auto stood = standing(aKind, aItem);
		if (stood == core::import_standing::unchanged) {
			return tr("Same as yours");
		}
		if (aKind == item_kind::agent) {
			const auto& item = Preview.File.Agents[aItem];
			if (item.Problem) {
				return item.Unsupported.empty() ? text_of(core::describe(*item.Problem)) : tr("Names “%1”").arg(text_of(item.Unsupported));
			}
			const auto& state = Agents[aItem];
			switch (state.Action) {
			case core::agent_import_action::add:          return bound(aItem) ? tr("Added as a new agent") : tr("Added once you choose its program");
			case core::agent_import_action::replace:      return tr("Updates yours");
			case core::agent_import_action::use_existing: return tr("%1 stands for it").arg(local_name(state.Target));
			case core::agent_import_action::leave:        return tr("Left out");
			}
		}
		if (aKind == item_kind::prompt) {
			if (stood == core::import_standing::refused) {
				return tr("Not known to this version");
			}
			return Prompts[aItem] ? tr("Replaces yours") : tr("Yours kept");
		}
		const auto& item = Preview.File.Workflows[aItem];
		if (item.Problem) {
			return item.Unsupported.empty() ? text_of(core::describe(*item.Problem)) : tr("Names “%1”").arg(text_of(item.Unsupported));
		}
		if (Workflows[aItem].Action == core::workflow_import_action::leave) {
			return tr("Left out");
		}
		if (const auto local = Plan.Workflows[aItem].Local) {
			return tr("%1 replaces yours, %2")
				.arg(text_of(core::workflow_revision(item.Workflow).substr(0U, 6U)), text_of(core::workflow_revision(Preview.Snapshot.Workflows[*local].Workflow).substr(0U, 6U)));
		}
		return tr("Added as a new workflow");
	}

	void agent_configuration_import_dialog::fill_list()
	{
		Filling = true;
		Items->clear();
		const auto header = [this](const QString& aText) {
			auto* const item = new QListWidgetItem{ aText, Items };
			item->setData(HeaderRole, true);
			item->setFlags(Qt::NoItemFlags);
			auto font = item->font();
			font.setWeight(QFont::DemiBold);
			font.setPointSizeF(font.pointSizeF() * 0.92);
			item->setFont(font);
			// Every section opens with the same room above its name.
			item->setTextAlignment(Qt::AlignLeft | Qt::AlignBottom);
			item->setSizeHint(QSize{ 0, QFontMetrics{ font }.height() + 12 });
		};
		const auto add = [this](const item_kind aKind, const std::size_t aIndex, const QString& aName) {
			auto* const item = new QListWidgetItem{ aName, Items };
			item->setData(KindRole, static_cast<int>(aKind));
			item->setData(IndexRole, static_cast<qulonglong>(aIndex));
			item->setData(ItemRole, aName);
		};
		// Each section as Export lists it; what cannot be imported last.
		const auto section = [&](const QString& aTitle, const item_kind aKind, const std::size_t aCount, const auto& aName) {
			auto titled = false;
			for (std::size_t index = 0U; index < aCount; ++index) {
				if (standing(aKind, index) == core::import_standing::refused) {
					continue;
				}
				if (!titled) {
					header(aTitle);
					titled = true;
				}
				add(aKind, index, aName(index));
			}
		};
		const auto agentName  = [this](const std::size_t aIndex) { return text_of(Preview.File.Agents[aIndex].Agent.Name); };
		const auto promptName = [this](const std::size_t aIndex) {
			const auto& item = Preview.File.Prompts[aIndex];
			return item.Local && *item.Local < Preview.Prompts.size() ? prompt_title(Preview.Prompts[*item.Local].Purpose) : text_of(item.Prompt.Purpose);
		};
		const auto workflowName = [this](const std::size_t aIndex) { return text_of(Preview.File.Workflows[aIndex].Workflow.Name); };
		section(tr("Agents"), item_kind::agent, Agents.size(), agentName);
		section(tr("Workflows"), item_kind::workflow, Workflows.size(), workflowName);
		section(tr("Prompts"), item_kind::prompt, Prompts.size(), promptName);
		auto       titled  = false;
		const auto refused = [&](const item_kind aKind, const std::size_t aCount, const auto& aName) {
			for (std::size_t index = 0U; index < aCount; ++index) {
				if (standing(aKind, index) != core::import_standing::refused) {
					continue;
				}
				if (!titled) {
					header(tr("Cannot be imported"));
					titled = true;
				}
				add(aKind, index, aName(index));
			}
		};
		refused(item_kind::agent, Agents.size(), agentName);
		refused(item_kind::workflow, Workflows.size(), workflowName);
		refused(item_kind::prompt, Prompts.size(), promptName);
		Filling = false;
	}

	void agent_configuration_import_dialog::refresh_rows()
	{
		Filling = true;
		for (int row = 0; row < Items->count(); ++row) {
			auto* const item = Items->item(row);
			if (item->data(HeaderRole).toBool()) {
				continue;
			}
			const auto kind  = static_cast<item_kind>(item->data(KindRole).toInt());
			const auto index = static_cast<std::size_t>(item->data(IndexRole).toULongLong());
			const auto stood = standing(kind, index);
			// The library's row: the name, its tag, and one line of what
			// happens; under Cannot be imported the section says the tag.
			const auto name = item->data(ItemRole).toString();
			item->setText((stood == core::import_standing::refused ? name : name + QStringLiteral("   ") + standing_tag(stood)) + QLatin1Char('\n') + row_line(kind, index));
			auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			// Something to choose is ticked; an agent of another kind under
			// its name, with none of yours to stand for it, has nothing.
			const auto choosable = (stood == core::import_standing::added || stood == core::import_standing::updated)
				&& (kind != item_kind::agent || Plan.Agents[index].Default.Action != core::agent_import_action::leave || !Plan.Agents[index].StandIns.empty());
			if (choosable) {
				flags |= Qt::ItemIsUserCheckable;
				item->setFlags(flags);
				item->setCheckState(chosen(kind, index) ? Qt::Checked : Qt::Unchecked);
			} else {
				item->setFlags(flags);
				item->setData(Qt::CheckStateRole, QVariant{ });
			}
			item->setData(Qt::AccessibleDescriptionRole, standing_tag(stood) + QStringLiteral(", ") + row_line(kind, index));
			item->setToolTip(item->text());
		}
		Filling = false;

		// The footer: what Import does, in a sentence.
		auto agentsAdded = std::size_t{ 0U }, agentsUpdated = std::size_t{ 0U }, agentsUsed = std::size_t{ 0U }, waiting = std::size_t{ 0U }, refused = std::size_t{ 0U };
		for (std::size_t index = 0U; index < Agents.size(); ++index) {
			refused += Plan.Agents[index].Standing == core::import_standing::refused ? 1U : 0U;
			waiting += Plan.Agents[index].Standing != core::import_standing::refused && !bound(index) ? 1U : 0U;
		}
		for (const auto& choice : agent_choices()) {
			agentsAdded += choice.Action == core::agent_import_action::add ? 1U : 0U;
			agentsUpdated += choice.Action == core::agent_import_action::replace ? 1U : 0U;
			agentsUsed += choice.Action == core::agent_import_action::use_existing ? 1U : 0U;
		}
		auto workflowsAdded = std::size_t{ 0U }, workflowsUpdated = std::size_t{ 0U };
		for (const auto& choice : workflow_choices()) {
			(choice.Action == core::workflow_import_action::add ? workflowsAdded : workflowsUpdated) += 1U;
		}
		for (const auto& plan : Plan.Prompts) {
			refused += plan.Standing == core::import_standing::refused ? 1U : 0U;
		}
		for (const auto& plan : Plan.Workflows) {
			refused += plan.Standing == core::import_standing::refused ? 1U : 0U;
		}
		const auto prompts = prompt_choices().size();
		auto       adds    = QStringList{ };
		auto       updates = QStringList{ };
		if (agentsAdded > 0U) {
			adds << counted(agentsAdded, tr("1 agent"), tr("%1 agents"));
		}
		if (workflowsAdded > 0U) {
			adds << counted(workflowsAdded, tr("1 workflow"), tr("%1 workflows"));
		}
		if (agentsUpdated > 0U) {
			updates << counted(agentsUpdated, tr("1 agent"), tr("%1 agents"));
		}
		if (workflowsUpdated > 0U) {
			updates << counted(workflowsUpdated, tr("1 workflow"), tr("%1 workflows"));
		}
		if (prompts > 0U) {
			updates << counted(prompts, tr("1 prompt"), tr("%1 prompts"));
		}
		auto parts = QStringList{ };
		if (!adds.isEmpty()) {
			parts << tr("adds %1").arg(adds.join(tr(" and ")));
		}
		if (!updates.isEmpty()) {
			parts << tr("updates %1").arg(updates.join(tr(" and ")));
		}
		if (agentsUsed > 0U) {
			parts << counted(agentsUsed, tr("lets 1 of your agents stand in"), tr("lets %1 of your agents stand in"));
		}
		auto words = parts.isEmpty() ? tr("Nothing is ticked to import.") : tr("Import %1.").arg(parts.join(tr(", ")));
		if (waiting > 0U) {
			words += QLatin1Char(' ') + counted(waiting, tr("1 agent waits for its program."), tr("%1 agents wait for their programs."));
		}
		if (refused > 0U) {
			words += QLatin1Char(' ') + counted(refused, tr("1 item cannot be imported."), tr("%1 items cannot be imported."));
		}
		if (!parts.isEmpty()) {
			words += QLatin1Char(' ') + tr("Nothing runs until you start an action in a draft.");
		}
		Summary->setText(words);
		ApplyButton->setEnabled(!agent_choices().empty() || prompts > 0U || !workflow_choices().empty());
	}

	void agent_configuration_import_dialog::select(const item_kind aKind, const std::size_t aItem)
	{
		for (int row = 0; row < Items->count(); ++row) {
			auto* const item = Items->item(row);
			if (!item->data(HeaderRole).toBool() && item->data(KindRole).toInt() == static_cast<int>(aKind) && item->data(IndexRole).toULongLong() == aItem) {
				Items->setCurrentItem(item);
			}
		}
		Shown = std::pair{ aKind, aItem };
		show_selected();
	}

	void agent_configuration_import_dialog::select_agent(const std::size_t aItem) { select(item_kind::agent, aItem); }
	void agent_configuration_import_dialog::select_prompt(const std::size_t aItem) { select(item_kind::prompt, aItem); }
	void agent_configuration_import_dialog::select_workflow(const std::size_t aItem) { select(item_kind::workflow, aItem); }

	auto agent_configuration_import_dialog::choice_buttons() const -> QList<QToolButton*>
	{
		auto buttons = QList<QToolButton*>{ };
		if (Choice != nullptr) {
			for (auto* const button : Choice->buttons()) {
				buttons << static_cast<QToolButton*>(button);
			}
		}
		return buttons;
	}

	auto agent_configuration_import_dialog::workflow_program_buttons() const -> QList<QPushButton*>
	{
		return Detail == nullptr ? QList<QPushButton*>{ } : Detail->findChildren<QPushButton*>(QStringLiteral("agentConfigurationWorkflowProgram"));
	}

	void agent_configuration_import_dialog::choose(const item_kind aKind, const std::size_t aItem, const int aPart, const std::uint32_t aTarget)
	{
		switch (aKind) {
		case item_kind::agent: {
			auto&       state = Agents[aItem];
			const auto& plan  = Plan.Agents[aItem];
			if (aPart == TakePart) {
				state.Action = plan.Meets && plan.Standing == core::import_standing::updated ? core::agent_import_action::replace : core::agent_import_action::add;
				state.Target = state.Action == core::agent_import_action::replace ? *plan.Meets : 0U;
			} else if (aPart == UsePart && !plan.StandIns.empty()) {
				state.Action = core::agent_import_action::use_existing;
				state.Target = std::ranges::contains(plan.StandIns, aTarget) ? aTarget : plan.StandIns.front();
			} else {
				state.Action = core::agent_import_action::leave;
			}
			if (state.Action != core::agent_import_action::leave) {
				state.Ticked = state.Action;
			}
			break;
		}
		case item_kind::prompt: Prompts[aItem] = aPart == TakePart; break;
		case item_kind::workflow:
			Workflows[aItem].Action = aPart != TakePart ? core::workflow_import_action::leave
				: Plan.Workflows[aItem].Local           ? core::workflow_import_action::replace
														: core::workflow_import_action::add;
			break;
		}
		refresh_rows();
		if (Shown && Shown->first == aKind && Shown->second == aItem) {
			show_selected();
		}
	}

	void agent_configuration_import_dialog::show_selected()
	{
		Choice              = nullptr;
		ProgramButton       = nullptr;
		ProgramLine         = nullptr;
		Relaxed             = nullptr;
		Builds              = nullptr;
		Outcome             = nullptr;
		Settings            = nullptr;
		Comparison          = nullptr;
		Yours               = nullptr;
		Theirs              = nullptr;
		const auto scrolled = Scroll->verticalScrollBar()->value();
		if (auto* const old = Scroll->takeWidget(); old != nullptr) {
			old->hide();
			old->deleteLater();
		}
		// Parented from the start, so what it builds reads the dialog's palette.
		Detail = new QWidget{ Scroll->viewport() };
		Detail->setObjectName(QStringLiteral("agentConfigurationDetailPage"));
		Detail->setAutoFillBackground(false);
		auto* const column = new QVBoxLayout{ Detail };
		column->setContentsMargins(16, 14, 16, 14);
		column->setSpacing(10);
		if (!Shown) {
			column->addWidget(note(tr("Choose an item to see what importing it changes."), Detail));
		} else if (Shown->first == item_kind::agent) {
			show_agent(Shown->second, *Detail);
		} else if (Shown->first == item_kind::prompt) {
			show_prompt(Shown->second, *Detail);
		} else {
			show_workflow(Shown->second, *Detail);
		}
		column->addStretch(1);
		Scroll->setWidget(Detail);
		Scroll->verticalScrollBar()->setValue(scrolled);
	}

	namespace {
		/// The detail's title row: the name, larger, and its tag.
		void title_row(QVBoxLayout& aColumn, const QString& aName, const QString& aTag, QWidget& aPage)
		{
			// One label, so a long name wraps with its tag after it, in the
			// panel's secondary text colour.
			auto* const title = rich(QStringLiteral("<b>%1</b>&nbsp;&nbsp;<span style=\"font-weight:normal;color:%2\">%3</span>")
										 .arg(aName.toHtmlEscaped(), aPage.palette().color(QPalette::PlaceholderText).name(), aTag.toHtmlEscaped()),
									 &aPage);
			title->setObjectName(QStringLiteral("agentConfigurationTitle"));
			auto font = title->font();
			font.setPointSizeF(font.pointSizeF() * 1.1);
			title->setFont(font);
			aColumn.addWidget(title);
		}
	} // namespace

	void agent_configuration_import_dialog::show_refused(const QString& aName, const QString& aWhy, QWidget& aPage)
	{
		auto& column = *static_cast<QVBoxLayout*>(aPage.layout());
		title_row(column, aName, standing_tag(core::import_standing::refused), aPage);
		Outcome = note(aWhy + QLatin1Char(' ') + tr("It is not saved. The rest of the file can still be imported."), &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationItemProblem"));
		Outcome->setForegroundRole(QPalette::WindowText);
		column.addWidget(Outcome);
	}

	void agent_configuration_import_dialog::show_agent(const std::size_t aItem, QWidget& aPage)
	{
		auto&       column = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& item   = Preview.File.Agents[aItem];
		const auto& agent  = item.Agent;
		const auto& plan   = Plan.Agents[aItem];
		if (item.Problem) {
			auto why = text_of(core::describe(*item.Problem));
			if (item.Refusal) {
				why += QLatin1Char(' ') + text_of(core::describe(*item.Refusal));
			}
			if (!item.Unsupported.empty()) {
				why += QLatin1Char(' ') + tr("It names “%1”.").arg(text_of(item.Unsupported));
			}
			show_refused(text_of(agent.Name), why, aPage);
			return;
		}
		auto&       state = Agents[aItem];
		const auto& local = Preview.Snapshot.Agents.Agents;
		title_row(column, text_of(agent.Name), standing_tag(plan.Standing), aPage);
		const auto asks = asking_actions(aItem);
		auto       says = agent_kind(agent.Protocol, agent.Preset) + QStringLiteral(".");
		if (!asks.isEmpty()) {
			says += QLatin1Char(' ') + counted(static_cast<std::size_t>(asks.size()), tr("The workflow's action %1 asks for this agent, read-only.").arg(asks.join(tr(", "))), tr("The workflow's actions %1 ask for this agent, read-only.").arg(asks.join(tr(", "))));
		}
		column.addWidget(note(says, &aPage));
		const auto* const meets = plan.Meets ? &*std::ranges::find(local, *plan.Meets, &core::agent_entry::Id) : nullptr;
		if (plan.Standing == core::import_standing::unchanged) {
			Outcome = note(tr("“%1” already has these settings. Nothing to import.").arg(local_name(*plan.Meets)), &aPage);
			column.addWidget(Outcome);
			return;
		}

		// One control, every part showing: the file's, one of yours, none.
		auto* const parts = new QHBoxLayout{ };
		parts->setSpacing(0);
		Choice = new QButtonGroup{ &aPage };
		Choice->setExclusive(true);
		const auto updating = plan.Standing == core::import_standing::updated;
		if (!updating || (meets != nullptr && core::compatible_agent(*meets, agent))) {
			auto* const take = segmented(updating ? tr("Update") : tr("Add"), &aPage);
			take->setToolTip(updating ? tr("“%1” takes the file's settings.").arg(local_name(*plan.Meets)) : tr("Added as a new agent."));
			Choice->addButton(take, TakePart);
			parts->addWidget(take);
		}
		if (!plan.StandIns.empty()) {
			const auto shownTarget = state.Action == core::agent_import_action::use_existing ? state.Target : plan.StandIns.front();
			auto*      use         = segmented(tr("Use %1").arg(local_name(shownTarget)), &aPage);
			use->setToolTip(tr("One of your agents stands for it as it is, and is not changed."));
			if (plan.StandIns.size() > 1U) {
				// More than one could stand for it: the part names one and opens a menu of them.
				auto* const menu = new QMenu{ use };
				for (const auto id : plan.StandIns) {
					menu->addAction(local_name(id), this, [this, aItem, id] { QTimer::singleShot(0, this, [this, aItem, id] { choose(item_kind::agent, aItem, UsePart, id); }); });
				}
				use->setMenu(menu);
				use->setPopupMode(QToolButton::InstantPopup);
			}
			Choice->addButton(use, UsePart);
			parts->addWidget(use);
		}
		auto* const leave = segmented(tr("Leave out"), &aPage);
		Choice->addButton(leave, LeavePart);
		parts->addWidget(leave);
		parts->addStretch(1);
		const auto part = state.Action == core::agent_import_action::leave ? LeavePart
			: state.Action == core::agent_import_action::use_existing      ? UsePart
																		   : TakePart;
		if (auto* const on = Choice->button(part); on != nullptr) {
			on->setChecked(true);
		}
		connect(Choice, &QButtonGroup::idClicked, this, [this, aItem](const int aPart) {
			if (aPart == UsePart && Plan.Agents[aItem].StandIns.size() > 1U) {
				return;
			}
			QTimer::singleShot(0, this, [this, aItem, aPart] { choose(item_kind::agent, aItem, aPart, Agents[aItem].Target); });
		});
		column.addLayout(parts);

		const auto adding    = state.Action == core::agent_import_action::add;
		const auto replacing = state.Action == core::agent_import_action::replace && meets != nullptr;
		const auto choice    = core::agent_import_choice{ .Item = aItem, .Action = state.Action, .Target = state.Target, .Program = state.Program, .AcceptRelaxedChecks = state.AcceptRelaxed, .AcceptBuilds = state.AcceptBuilds };
		auto       outcome   = QString{ };
		if (adding) {
			outcome = tr("Added at the end of your agents, not yet checked on this computer.");
		} else if (replacing) {
			const auto after = core::imported_entry(agent, choice, meets);
			outcome          = core::keeps_agent_identity(*meets, after) ? tr("“%1” takes the file's settings. Its conversations stay and can be continued.").arg(local_name(meets->Id))
																		 : tr("“%1” takes the file's settings. Its conversations stay readable but can no longer be continued, because what it runs changes.").arg(local_name(meets->Id));
		} else if (state.Action == core::agent_import_action::use_existing) {
			outcome = tr("“%1” stands for it in the workflow and is not changed.").arg(local_name(state.Target));
		} else {
			outcome = asks.isEmpty() ? tr("Not imported.") : tr("Not imported. The actions that ask for it wait until an agent stands for it.");
		}
		Outcome = note(outcome, &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationChanges"));
		Outcome->setForegroundRole(QPalette::WindowText);
		column.addWidget(Outcome);
		if (!adding && !replacing) {
			return;
		}

		// What it is set to; an update shows yours beside the file's.
		const auto after = core::imported_entry(agent, choice, replacing ? meets : nullptr);
		column.addSpacing(4);
		column.addWidget(heading(replacing ? tr("What changes") : tr("What it is set to"), &aPage));
		auto [box, rows] = group_box(&aPage);
		box->setObjectName(QStringLiteral("agentConfigurationSettings"));
		Settings         = box;
		auto* const grid = new QGridLayout{ };
		grid->setHorizontalSpacing(14);
		grid->setVerticalSpacing(6);
		grid->setColumnStretch(1, 1);
		auto line = 0;
		if (replacing) {
			grid->setColumnStretch(2, 1);
			grid->addWidget(note(tr("Yours"), box), line, 1);
			grid->addWidget(note(tr("In the file"), box), line, 2);
			++line;
			const auto fields = core::agent_differences(*meets, after);
			if (fields.empty()) {
				grid->addWidget(note(tr("Nothing changes."), box), line++, 0, 1, 3);
			}
			for (const auto field : fields) {
				grid->addWidget(new QLabel{ field_words(field), box }, line, 0, Qt::AlignTop);
				auto* const before = rich(breakable(field_value(*meets, field)).toHtmlEscaped(), box);
				auto* const now    = rich(QStringLiteral("<b>%1</b>").arg(breakable(field_value(after, field)).toHtmlEscaped()), box);
				grid->addWidget(before, line, 1);
				grid->addWidget(now, line, 2);
				++line;
			}
		} else {
			for (const auto field : { core::agent_field::protocol, core::agent_field::model, core::agent_field::effort, core::agent_field::strict_run_checks, core::agent_field::scratch_starts,
									  core::agent_field::mcp_servers, core::agent_field::instructions }) {
				grid->addWidget(new QLabel{ field_words(field), box }, line, 0, Qt::AlignTop);
				grid->addWidget(rich(field_value(after, field).toHtmlEscaped(), box), line, 1);
				++line;
			}
		}
		rows->addLayout(grid);
		column.addWidget(box);

		// The program this computer runs it with: never a path from the file.
		auto [programBox, programRows] = group_box(&aPage);
		auto* const programRow         = new QHBoxLayout{ };
		ProgramLine                    = note({ }, programBox);
		ProgramLine->setObjectName(QStringLiteral("agentConfigurationProgram"));
		ProgramLine->setForegroundRole(QPalette::WindowText);
		programRow->addWidget(ProgramLine, 1);
		if (plan.NeedsProgram || (replacing && program_required(agent))) {
			ProgramButton = new quiet_push_button{ tr("Choose program…"), programBox };
			ProgramButton->setObjectName(QStringLiteral("agentConfigurationChooseProgram"));
			programRow->addWidget(ProgramButton, 0, Qt::AlignTop);
			connect(ProgramButton, &QPushButton::clicked, this, [this, aItem] {
				if (!Chooser) {
					return;
				}
				const auto picked = Chooser(text_of(Preview.File.Agents[aItem].Agent.ProgramName));
				if (!picked.isEmpty()) {
					Agents[aItem].Program = picked.toStdString();
					QTimer::singleShot(0, this, [this] {
						refresh_rows();
						show_selected();
					});
				}
			});
		}
		if (!program_required(agent)) {
			const auto found = std::ranges::contains(Facts.Found, *agent.Preset);
			ProgramLine->setText(found ? tr("Program: %1, found on this computer; looked for each time it runs.").arg(agent_kind(agent.Protocol, agent.Preset))
									   : tr("Program: not found on this computer yet; looked for each time it runs."));
		} else if (!state.Program.empty()) {
			ProgramLine->setText(tr("Program: %1").arg(breakable(text_of(state.Program))));
		} else if (replacing) {
			ProgramLine->setText(tr("Program: keeps %1").arg(meets->Program.empty() ? tr("its own") : breakable(text_of(meets->Program))));
		} else {
			ProgramLine->setText(agent.ProgramName.empty() ? tr("Program: not chosen yet. The file carries no path, so you say which program runs it on this computer. It is left out until you do.")
														   : tr("Program: %1 is not chosen yet. The file names it and carries no path, so you say where it is on this computer. It is left out until you do.")
																 .arg(breakable(text_of(agent.ProgramName))));
		}
		programRows->addLayout(programRow);
		column.addWidget(programBox);

		// What the file asks for that only you can allow: unticked until you do.
		const auto permission = [&column, &aPage](const QString& aName, const QString& aWhy, const bool aOn, const QString& aObject) {
			auto [frame, lines] = group_box(&aPage);
			auto* const check   = new QCheckBox{ aName, frame };
			check->setObjectName(aObject);
			check->setChecked(aOn);
			lines->addWidget(check);
			lines->addWidget(note(aWhy, frame));
			column.addWidget(frame);
			return check;
		};
		if (!agent.StrictRunChecks && !agent.Preset) {
			Relaxed = permission(tr("Let %1 relax its run checks").arg(text_of(agent.Name)), tr("The file asks for this. It stays strict unless you tick; you can change it later in the agent's settings."),
								 state.AcceptRelaxed, QStringLiteral("agentConfigurationRelaxed"));
			connect(Relaxed, &QCheckBox::toggled, this, [this, aItem](const bool aOn) {
				Agents[aItem].AcceptRelaxed = aOn;
				QTimer::singleShot(0, this, [this] { show_selected(); });
			});
		}
		if (agent.BuildsAndTests) {
			Builds = permission(tr("Let %1 build and run tests").arg(text_of(agent.Name)), tr("The file asks for this. It stays off unless you tick; you can turn it on later in the agent's settings."),
								state.AcceptBuilds, QStringLiteral("agentConfigurationBuilds"));
			connect(Builds, &QCheckBox::toggled, this, [this, aItem](const bool aOn) {
				Agents[aItem].AcceptBuilds = aOn;
				QTimer::singleShot(0, this, [this] { show_selected(); });
			});
		}
	}

	void agent_configuration_import_dialog::show_workflow(const std::size_t aItem, QWidget& aPage)
	{
		auto&       column   = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& item     = Preview.File.Workflows[aItem];
		const auto& workflow = item.Workflow;
		const auto& plan     = Plan.Workflows[aItem];
		if (item.Problem) {
			auto why = text_of(core::describe(*item.Problem));
			if (!item.Unsupported.empty()) {
				why += QLatin1Char(' ') + tr("It names “%1”, which this version does not know.").arg(text_of(item.Unsupported));
			}
			if (item.Action && *item.Action < workflow.Actions.size()) {
				why += QLatin1Char(' ') + tr("In the action “%1”.").arg(text_of(workflow.Actions[*item.Action].Task));
			}
			show_refused(text_of(workflow.Name), why, aPage);
			return;
		}
		auto&       state   = Workflows[aItem];
		const auto* yours   = plan.Local ? &Preview.Snapshot.Workflows[*plan.Local] : nullptr;
		const auto  theirs  = text_of(core::workflow_revision(workflow).substr(0U, 6U));
		const auto  mineRev = yours != nullptr ? text_of(core::workflow_revision(yours->Workflow).substr(0U, 6U)) : QString{ };
		title_row(column, text_of(workflow.Name), standing_tag(plan.Standing), aPage);
		auto says = tr("Says it comes from %1.").arg(workflow.Origin.empty() ? tr("no one in particular") : text_of(workflow.Origin));
		if (plan.Standing == core::import_standing::updated) {
			says += QLatin1Char(' ') + tr("Ticked, revision %1 replaces yours, %2.").arg(theirs, mineRev);
		} else if (plan.Standing == core::import_standing::unchanged) {
			says += QLatin1Char(' ') + tr("Yours is the same revision, %1. Nothing to import.").arg(theirs);
		}
		says += QLatin1Char(' ') + tr("No action runs on import; each is yours to start, in a draft.");
		column.addWidget(note(says, &aPage));
		if (plan.Standing != core::import_standing::unchanged) {
			auto* const parts = new QHBoxLayout{ };
			parts->setSpacing(0);
			Choice            = new QButtonGroup{ &aPage };
			auto* const take  = segmented(plan.Local ? tr("Update") : tr("Add"), &aPage);
			auto* const leave = segmented(tr("Leave out"), &aPage);
			Choice->addButton(take, TakePart);
			Choice->addButton(leave, LeavePart);
			parts->addWidget(take);
			parts->addWidget(leave);
			parts->addStretch(1);
			(state.Action == core::workflow_import_action::leave ? leave : take)->setChecked(true);
			connect(Choice, &QButtonGroup::idClicked, this, [this, aItem](const int aPart) { QTimer::singleShot(0, this, [this, aItem, aPart] { choose(item_kind::workflow, aItem, aPart); }); });
			column.addLayout(parts);
		}

		// Who runs each action, and with what; what it says it does.
		const auto accent    = agent_accent(palette()).name();
		const auto mark      = palette().color(QPalette::Highlight);
		const auto tint      = QColor{ mark.red(), mark.green(), mark.blue(), 60 }.name(QColor::HexArgb);
		const auto agentName = [this](const std::string& aPortable) {
			const auto found = std::ranges::find(Preview.File.Agents, aPortable, [](const core::decoded_agent& aAgent) { return aAgent.Agent.PortableId; });
			return found == Preview.File.Agents.end() ? text_of(aPortable) : text_of(found->Agent.Name);
		};
		const auto cell = [&](const core::workflow_action& aAction, const core::workflow_action* const aBefore) {
			auto html = QStringLiteral("<b>%1</b>").arg(escaped(aAction.Task));
			if (aAction.Required) {
				html += QStringLiteral(" &nbsp;<span style=\"color:%1\">%2</span>").arg(palette().color(QPalette::PlaceholderText).name(), tr("Required"));
			}
			if (aAction.Binding == core::workflow_binding::agent) {
				html += QStringLiteral("<br><span style=\"color:%1\">✦ %2</span>, %3").arg(accent, agentName(aAction.Agent).toHtmlEscaped(), tr("read-only"));
			} else {
				html += QStringLiteral("<br>") + tr("You run %1").arg(QStringLiteral("<code>%1</code>").arg(breakable(text_of(aAction.ProgramName)).toHtmlEscaped()));
			}
			auto effects = QStringList{ };
			for (const auto effect : aAction.Effects) {
				const auto words = escaped(core::describe(effect));
				const auto added = aBefore != nullptr && !std::ranges::contains(aBefore->Effects, effect);
				effects << (added ? QStringLiteral("<span style=\"background-color:%1\">%2</span>").arg(tint, words) : words);
			}
			html += QStringLiteral("<br>") + effects.join(QStringLiteral(", "));
			return html;
		};
		auto [box, rows] = group_box(&aPage);
		box->setObjectName(QStringLiteral("agentConfigurationWorkflowActions"));
		Comparison       = box;
		auto* const grid = new QGridLayout{ };
		grid->setHorizontalSpacing(16);
		grid->setVerticalSpacing(10);
		auto line = 0;
		if (yours != nullptr && plan.Standing == core::import_standing::updated) {
			grid->setColumnStretch(0, 1);
			grid->setColumnStretch(1, 1);
			grid->addWidget(heading(tr("Yours, revision %1").arg(mineRev), box), line, 0);
			grid->addWidget(heading(tr("In the file, revision %1").arg(theirs), box), line, 1);
			++line;
			for (const auto& action : workflow.Actions) {
				const auto before = std::ranges::find(yours->Workflow.Actions, action.Id, &core::workflow_action::Id);
				const auto had    = before != yours->Workflow.Actions.end();
				grid->addWidget(had ? rich(cell(*before, nullptr), box) : note(tr("No such action"), box), line, 0, Qt::AlignTop);
				auto html = cell(action, had ? &*before : nullptr);
				// The tag sits on the action's own first line, after its task.
				const auto tagged = [&html](const QString& aTag) {
					const auto end = html.indexOf(QStringLiteral("</b>")) + 4;
					html.insert(end, aTag);
				};
				if (!had) {
					tagged(QStringLiteral(" &nbsp;<span style=\"background-color:%1\">&nbsp;%2&nbsp;</span>").arg(tint, tr("New action")));
				} else if (std::ranges::any_of(action.Effects, [&](const core::workflow_effect aEffect) { return !std::ranges::contains(before->Effects, aEffect); })) {
					tagged(QStringLiteral(" &nbsp;<span style=\"background-color:%1\">&nbsp;%2&nbsp;</span>").arg(tint, tr("Does more")));
				} else if (*before != action) {
					tagged(QStringLiteral(" &nbsp;<i>%1</i>").arg(tr("Changed")));
				}
				auto* const label = rich(html, box);
				label->setObjectName(QStringLiteral("agentConfigurationAction-%1").arg(text_of(action.Id)));
				grid->addWidget(label, line, 1, Qt::AlignTop);
				++line;
			}
			for (const auto& action : yours->Workflow.Actions) {
				if (std::ranges::find(workflow.Actions, action.Id, &core::workflow_action::Id) == workflow.Actions.end()) {
					grid->addWidget(rich(cell(action, nullptr), box), line, 0, Qt::AlignTop);
					grid->addWidget(note(tr("Removed"), box), line, 1, Qt::AlignTop);
					++line;
				}
			}
		} else {
			grid->setColumnStretch(0, 1);
			for (const auto& action : workflow.Actions) {
				auto* const label = rich(cell(action, nullptr), box);
				label->setObjectName(QStringLiteral("agentConfigurationAction-%1").arg(text_of(action.Id)));
				grid->addWidget(label, line++, 0);
			}
		}
		rows->addLayout(grid);
		column.addWidget(box);

		// One program for every action that names it: chosen here, never from the file.
		if (plan.Standing != core::import_standing::unchanged) {
			auto names = QStringList{ };
			for (const auto& action : workflow.Actions) {
				if (action.Binding == core::workflow_binding::command && !names.contains(text_of(action.ProgramName))) {
					names << text_of(action.ProgramName);
				}
			}
			for (const auto& name : names) {
				auto asking = std::size_t{ 0U };
				auto bound  = QString{ };
				for (const auto& action : workflow.Actions) {
					if (action.Binding == core::workflow_binding::command && text_of(action.ProgramName) == name) {
						++asking;
						if (const auto program = std::ranges::find(state.Programs, action.Id, &core::workflow_program::Action); program != state.Programs.end()) {
							bound = text_of(program->Program);
						}
					}
				}
				auto [programBox, programRows] = group_box(&aPage);
				auto* const row                = new QHBoxLayout{ };
				auto* const words              = new QVBoxLayout{ };
				auto* const line1              = rich(bound.isEmpty() ? tr("%1 is not chosen yet").arg(QStringLiteral("<code>%1</code>").arg(breakable(name).toHtmlEscaped()))
																	  : tr("%1 is %2").arg(QStringLiteral("<code>%1</code>").arg(breakable(name).toHtmlEscaped()), breakable(bound).toHtmlEscaped()),
													  programBox);
				words->addWidget(line1);
				words->addWidget(note(counted(asking, tr("1 action asks for it."), tr("%1 actions ask for it.")) + QLatin1Char(' ')
										  + (bound.isEmpty() ? tr("The file names the program and carries no path, so you say where it is on this computer. Until then those actions cannot be handed off.")
															 : tr("Chosen on this computer for every action that names it.")),
									  programBox));
				row->addLayout(words, 1);
				auto* const choose = new quiet_push_button{ tr("Choose program…"), programBox };
				choose->setObjectName(QStringLiteral("agentConfigurationWorkflowProgram"));
				choose->setAccessibleName(tr("Choose program for %1").arg(name));
				row->addWidget(choose, 0, Qt::AlignTop);
				programRows->addLayout(row);
				column.addWidget(programBox);
				connect(choose, &QPushButton::clicked, this, [this, aItem, name] {
					if (!Chooser) {
						return;
					}
					const auto picked = Chooser(name);
					if (picked.isEmpty()) {
						return;
					}
					auto& programs = Workflows[aItem].Programs;
					for (const auto& action : Preview.File.Workflows[aItem].Workflow.Actions) {
						if (action.Binding == core::workflow_binding::command && text_of(action.ProgramName) == name) {
							std::erase_if(programs, [&action](const core::workflow_program& aProgram) { return aProgram.Action == action.Id; });
							programs.push_back({ .Action = action.Id, .Program = picked.toStdString() });
						}
					}
					QTimer::singleShot(0, this, [this] { show_selected(); });
				});
			}
		}
		if (plan.Standing == core::import_standing::updated) {
			Outcome = note(tr("Drafts that selected revision %1 keep it until you accept the update there.").arg(mineRev), &aPage);
			Outcome->setObjectName(QStringLiteral("agentConfigurationWorkflowChanges"));
			column.addWidget(Outcome);
		}
	}

	void agent_configuration_import_dialog::show_prompt(const std::size_t aItem, QWidget& aPage)
	{
		auto&       column = *static_cast<QVBoxLayout*>(aPage.layout());
		const auto& item   = Preview.File.Prompts[aItem];
		const auto& plan   = Plan.Prompts[aItem];
		if (plan.Standing == core::import_standing::refused) {
			show_refused(text_of(item.Prompt.Purpose), text_of(core::describe(item.Problem.value_or(core::configuration_item_problem::unsupported_purpose))), aPage);
			return;
		}
		const auto& local = Preview.Prompts[*item.Local];
		const auto  mine  = local.Override.empty() ? local.BuiltIn : local.Override;
		title_row(column, prompt_title(local.Purpose), standing_tag(plan.Standing), aPage);
		if (plan.Standing == core::import_standing::updated) {
			auto* const parts = new QHBoxLayout{ };
			parts->setSpacing(0);
			Choice            = new QButtonGroup{ &aPage };
			auto* const take  = segmented(tr("Update"), &aPage);
			auto* const leave = segmented(tr("Leave out"), &aPage);
			Choice->addButton(take, TakePart);
			Choice->addButton(leave, LeavePart);
			parts->addWidget(take);
			parts->addWidget(leave);
			parts->addStretch(1);
			(Prompts[aItem] ? take : leave)->setChecked(true);
			connect(Choice, &QButtonGroup::idClicked, this, [this, aItem](const int aPart) { QTimer::singleShot(0, this, [this, aItem, aPart] { choose(item_kind::prompt, aItem, aPart); }); });
			column.addLayout(parts);
		}
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
		if (plan.Standing == core::import_standing::unchanged) {
			words.push_back(tr("The text is the same as yours."));
		} else if (item.Prompt.Text == local.BuiltIn) {
			words.push_back(tr("Updating restores the built-in text."));
		} else {
			words.push_back(tr("Updating makes it your edited prompt; Reset to built-in still brings the built-in text back."));
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
		Outcome = note(words.join(QLatin1Char(' ')), &aPage);
		Outcome->setObjectName(QStringLiteral("agentConfigurationPromptNote"));
		column.addWidget(Outcome);
		auto* const compare = new QHBoxLayout{ };
		compare->setSpacing(12);
		for (const auto mineSide : { true, false }) {
			auto* const side = new QVBoxLayout{ };
			side->addWidget(heading(mineSide ? tr("Yours") : tr("In the file"), &aPage));
			auto* const text = mono(&aPage, mineSide ? tr("Your prompt") : tr("Imported prompt"));
			text->setPlainText(text_of(mineSide ? std::string_view{ mine } : std::string_view{ item.Prompt.Text }));
			text->setMinimumHeight(220);
			text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			side->addWidget(text);
			compare->addLayout(side, 1);
			(mineSide ? Yours : Theirs) = text;
		}
		column.addLayout(compare);
	}
} // namespace bpr::qt
