#include "bpr/qt/agent_entry_dialog.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#if defined(Q_OS_MACOS)
	#include "program_chooser_mac.hpp"
#endif

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto protocol_words(const core::agent_protocol aProtocol) -> QString
		{
			switch (aProtocol) {
			case core::agent_protocol::claude_stream_json: return QObject::tr("Claude Code (stream JSON)");
			case core::agent_protocol::codex_app_server:   return QObject::tr("Codex (App Server)");
			case core::agent_protocol::codex_jsonl_exec:   return QObject::tr("Codex (JSONL)");
			case core::agent_protocol::external_terminal:  return QObject::tr("Plain text");
			}
			return { };
		}

		/// [[BPR-2303]]. Why a chosen or typed app runs nothing, and what to do.
		[[nodiscard]] auto bundle_words(const core::app_bundle_refusal aRefusal, const QString& aBundle) -> QString
		{
			const auto bundle = aBundle.endsWith(QLatin1Char{ '/' }) ? aBundle.chopped(1) : aBundle;
			const auto name   = QFileInfo{ bundle }.fileName();
			switch (aRefusal) {
			case core::app_bundle_refusal::no_programs_folder:    return QObject::tr("%1 has no Contents/MacOS folder, so it has no program to run. Choose the program itself.").arg(name);
			case core::app_bundle_refusal::named_program_missing: return QObject::tr("%1 names a main program that isn't in its Contents/MacOS folder. Choose the program inside the app instead.").arg(name);
			case core::app_bundle_refusal::no_program:            return QObject::tr("%1 has no program in its Contents/MacOS folder. Choose the program inside the app instead.").arg(name);
			case core::app_bundle_refusal::several_programs:      return QObject::tr("%1 has more than one program in its Contents/MacOS folder and doesn't name its main one. Choose the one to run inside the app.").arg(name);
			}
			return { };
		}

		[[nodiscard]] auto platform_choose(QWidget* aParent, const QString& aTitle, const QString& aStart) -> QString
		{
#if defined(Q_OS_MACOS)
			return detail::choose_program_mac(aParent, aTitle, aStart);
#else
			return QFileDialog::getOpenFileName(aParent, aTitle, aStart);
#endif
		}
	} // namespace

	agent_entry_dialog::agent_entry_dialog(const core::agent_entry& aEntry, QWidget* aParent, agent_program_host aHost)
		: QDialog{ aParent }
		, Host{ std::move(aHost) }
		, Base{ aEntry }
	{
		setObjectName(QStringLiteral("agentEntryDialog"));
		setWindowTitle(aEntry.Id == 0U ? tr("Add agent") : tr("Edit agent"));
		auto* const column = new QVBoxLayout{ this };
		auto* const form   = new QFormLayout{ };
		Name               = new QLineEdit{ text_of(aEntry.Name), this };
		Name->setObjectName(QStringLiteral("agentEntryName"));
		Name->setPlaceholderText(tr("Work Claude"));
		form->addRow(tr("Name"), Name);
		Protocol = new QComboBox{ this };
		Protocol->setObjectName(QStringLiteral("agentEntryProtocol"));
		for (const auto protocol : { core::agent_protocol::claude_stream_json, core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec, core::agent_protocol::external_terminal }) {
			Protocol->addItem(protocol_words(protocol), static_cast<int>(protocol));
		}
		Protocol->setCurrentIndex(Protocol->findData(static_cast<int>(aEntry.Protocol)));
		Protocol->setToolTip(tr("What the program speaks: Claude Code's stream JSON, one of Codex's two protocols, or plain text in and out"));
		form->addRow(tr("Speaks"), Protocol);
		auto* const programRow = new QHBoxLayout{ };
		Program                = new QLineEdit{ text_of(aEntry.Program), this };
		Program->setObjectName(QStringLiteral("agentEntryProgram"));
		Program->setPlaceholderText(aEntry.Preset ? tr("Empty: the one found on this computer") : tr("/usr/local/bin/my-agent"));
		programRow->addWidget(Program, 1);
		auto* const browse = new quiet_push_button{ tr("Choose…"), this };
		browse->setObjectName(QStringLiteral("agentEntryChoose"));
		ChooseButton = browse;
		connect(browse, &QPushButton::clicked, this, [this] {
			const auto title  = tr("Choose the program");
			const auto chosen = Host.Choose ? Host.Choose(this, title, Program->text()) : platform_choose(this, title, Program->text());
			if (!chosen.isEmpty()) {
				Program->setText(QDir::toNativeSeparators(chosen));
				show_bundle_program();
			}
		});
		programRow->addWidget(browse);
		form->addRow(tr("Program"), programRow);
		// [[BPR-2318]] (owner, 2026-11-20). One argument per row: several
		// rows show, more scroll, and each has a grip and a trash.
		Arguments = new argument_list_editor{ this };
		Arguments->set_arguments(aEntry.Arguments);
		form->addRow(tr("Arguments"), Arguments);
		// Owner decision 2026-10-23: a custom agent's own check, optional.
		Check = new QPlainTextEdit{ this };
		Check->setObjectName(QStringLiteral("agentEntryCheck"));
		Check->setPlaceholderText(tr("Optional, one per line, such as --version"));
		Check->setToolTip(tr("Setup runs the program with these arguments, and it must exit 0. Empty: setup checks only that the program can start."));
		Check->setMaximumHeight(56);
		Check->setTabChangesFocus(true);
		auto check = QStringList{ };
		for (const auto& argument : aEntry.CheckArguments) {
			check << text_of(argument);
		}
		Check->setPlainText(check.join(QLatin1Char{ '\n' }));
		Check->setEnabled(!aEntry.Preset);
		form->addRow(tr("Check"), Check);
		Model = new QLineEdit{ text_of(aEntry.Model), this };
		Model->setObjectName(QStringLiteral("agentEntryModel"));
		Model->setPlaceholderText(tr("Empty: the agent's own default"));
		form->addRow(tr("Model"), Model);
		Effort = new QComboBox{ this };
		Effort->setObjectName(QStringLiteral("agentEntryEffort"));
		form->addRow(tr("Effort"), Effort);
		// [[BPR-2007]], RFC-BPR-0104 (owner, 2026-11-10: "Should start on, can
		// have that initial state be a setting in the agent settings").
		ScratchStarts = new QComboBox{ this };
		ScratchStarts->setObjectName(QStringLiteral("agentEntryScratchStarts"));
		ScratchStarts->addItem(tr("On"), true);
		ScratchStarts->addItem(tr("Off"), false);
		ScratchStarts->setCurrentIndex(aEntry.ScratchStarts ? 0 : 1);
		ScratchStarts->setToolTip(tr("Whether a pull request starts with the agent's scratch folder: a folder of its own, outside your repository, where it may write and run scripts. Each pull request remembers a change you make in the agent picker."));
		form->addRow(tr("Scratch folder starts"), ScratchStarts);
		// [[BPR-2139]], RFC-BPR-0104 (owner, 2026-11-12: "default MCPs to off").
		McpServers = new QComboBox{ this };
		McpServers->setObjectName(QStringLiteral("agentEntryMcpServers"));
		McpServers->addItem(tr("Off"), static_cast<int>(core::agent_mcp_servers::off));
		McpServers->addItem(tr("Use the agent's own configuration"), static_cast<int>(core::agent_mcp_servers::own));
		McpServers->setCurrentIndex(aEntry.McpServers == core::agent_mcp_servers::own ? 1 : 0);
		McpServers->setToolTip(tr("Off: the agent starts with no MCP servers. Own: the servers its own configuration names load in conversations; a pull request's servers never do, and one-off runs such as Enhance keep them off."));
		form->addRow(tr("MCP servers"), McpServers);
		// [[BPR-2140]], RFC-BPR-0107. The fallback for a wrapper that drops
		// the system prompt.
		Instructions = new QComboBox{ this };
		Instructions->setObjectName(QStringLiteral("agentEntryInstructions"));
		Instructions->addItem(tr("System prompt"), static_cast<int>(core::agent_instruction_delivery::system_prompt));
		Instructions->addItem(tr("In the message"), static_cast<int>(core::agent_instruction_delivery::message));
		Instructions->setCurrentIndex(aEntry.Instructions == core::agent_instruction_delivery::message ? 1 : 0);
		Instructions->setToolTip(tr("Where a built-in prompt's instructions go. Choose In the message if this agent's wrapper ignores the system prompt."));
		form->addRow(tr("Instructions"), Instructions);
		column->addLayout(form);
		// [[BPR-2008]]. Off until turned on; what it means right under it.
		Builds = new QCheckBox{ tr("Build and run tests"), this };
		Builds->setObjectName(QStringLiteral("agentEntryBuilds"));
		Builds->setChecked(aEntry.BuildsAndTests);
		column->addWidget(Builds);
		BuildsMeaning = new QLabel{ tr("In a disposable checkout of the pull request only, never in your own checkout. Each pull request asks before the first build, because building runs its code with your account's rights."), this };
		BuildsMeaning->setObjectName(QStringLiteral("agentEntryBuildsMeaning"));
		BuildsMeaning->setWordWrap(true);
		BuildsMeaning->setForegroundRole(QPalette::PlaceholderText);
		BuildsMeaning->setContentsMargins(style()->pixelMetric(QStyle::PM_IndicatorWidth) + style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing), 0, 0, 0);
		Builds->setAccessibleDescription(BuildsMeaning->text());
		column->addWidget(BuildsMeaning);
		// [[BPR-1918]]. A custom agent's opt-out, with what it means right
		// under it; a popular agent keeps strict checks and shows none.
		if (!aEntry.Preset) {
			StrictChecks = new QCheckBox{ tr("Strict run checks"), this };
			StrictChecks->setObjectName(QStringLiteral("agentEntryStrictChecks"));
			StrictChecks->setChecked(aEntry.StrictRunChecks);
			column->addWidget(StrictChecks);
			auto* const meaning = new QLabel{ tr("Stops a tour run when the agent loads or uses a tool it wasn't given. Turn off only for repositories you trust: the app then reports what the agent did instead of stopping it."), this };
			meaning->setObjectName(QStringLiteral("agentEntryStrictChecksMeaning"));
			meaning->setWordWrap(true);
			meaning->setForegroundRole(QPalette::PlaceholderText);
			meaning->setContentsMargins(style()->pixelMetric(QStyle::PM_IndicatorWidth) + style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing), 0, 0, 0);
			StrictChecks->setAccessibleDescription(meaning->text());
			column->addWidget(meaning);
		}
		// [[BPR-2307]] (owner, 2026-11-19). A custom Codex agent may turn
		// Codex's own sandbox off and rely on its launcher's. Off unless
		// turned on; shown only while the protocol is a Codex one.
		if (!aEntry.Preset) {
			LauncherSandbox = new QCheckBox{ tr("Rely on the launcher's sandbox"), this };
			LauncherSandbox->setObjectName(QStringLiteral("agentEntryLauncherSandbox"));
			LauncherSandbox->setChecked(aEntry.RelyOnLauncherSandbox);
			column->addWidget(LauncherSandbox);
			LauncherSandboxMeaning = new QLabel{ tr("For a launcher that runs Codex in its own sandbox, where Codex's sandbox makes every write fail. Codex's sandbox is then off whenever the agent may write, "
													"so the scratch folder and build limits are enforced only by the launcher. Read-only runs stay read-only."),
												 this };
			LauncherSandboxMeaning->setObjectName(QStringLiteral("agentEntryLauncherSandboxMeaning"));
			LauncherSandboxMeaning->setWordWrap(true);
			LauncherSandboxMeaning->setForegroundRole(QPalette::PlaceholderText);
			LauncherSandboxMeaning->setContentsMargins(style()->pixelMetric(QStyle::PM_IndicatorWidth) + style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing), 0, 0, 0);
			LauncherSandbox->setAccessibleDescription(LauncherSandboxMeaning->text());
			column->addWidget(LauncherSandboxMeaning);
		}
		auto* const words = new QLabel{ tr("The arguments are given to the program exactly as written, before the request's own, every time. Nothing is put into them and no shell runs them."), this };
		words->setWordWrap(true);
		words->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(words);
		Problem = new QLabel{ this };
		Problem->setObjectName(QStringLiteral("agentEntryProblem"));
		Problem->setWordWrap(true);
		column->addWidget(Problem);
		Buttons = new QDialogButtonBox{ QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this };
		connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		column->addWidget(Buttons);
		offer_efforts();
		Effort->setCurrentIndex(std::max(0, Effort->findData(text_of(aEntry.Effort))));
		connect(Name, &QLineEdit::textChanged, this, [this] { settle(); });
		connect(Program, &QLineEdit::textChanged, this, [this] {
			read_bundle();
			settle();
		});
		// A typed or pasted app shows its program once the field is left; OK
		// takes the program either way.
		connect(Program, &QLineEdit::editingFinished, this, [this] { show_bundle_program(); });
		Arguments->set_change([this] { settle(); });
		connect(Check, &QPlainTextEdit::textChanged, this, [this] { settle(); });
		connect(Model, &QLineEdit::textChanged, this, [this] { settle(); });
		if (StrictChecks != nullptr) {
			connect(StrictChecks, &QCheckBox::toggled, this, [this] { settle(); });
		}
		if (LauncherSandbox != nullptr) {
			connect(LauncherSandbox, &QCheckBox::toggled, this, [this] { settle(); });
		}
		connect(Effort, &QComboBox::currentIndexChanged, this, [this] { settle(); });
		connect(ScratchStarts, &QComboBox::currentIndexChanged, this, [this] { settle(); });
		connect(Builds, &QCheckBox::toggled, this, [this] { settle(); });
		connect(Protocol, &QComboBox::currentIndexChanged, this, [this] {
			offer_efforts();
			settle();
		});
		read_bundle();
		settle();
	}

	void agent_entry_dialog::read_bundle()
	{
		Bundle.reset();
		const auto path = Program->text().trimmed().toStdString();
		if (!Host.ReadBundle || !core::names_app_bundle(path)) {
			return;
		}
		if (const auto listing = Host.ReadBundle(path)) {
			Bundle = core::resolve_app_bundle_program(*listing);
		}
	}

	void agent_entry_dialog::show_bundle_program()
	{
		if (Bundle && Bundle->has_value()) {
			Program->setText(QString::fromStdString(**Bundle));
		}
	}

	void agent_entry_dialog::offer_efforts()
	{
		const auto kept = Effort->currentData().toString();
		Effort->clear();
		Effort->addItem(tr("The agent's own default"), QString{ });
		const auto protocol = static_cast<core::agent_protocol>(Protocol->currentData().toInt());
		if (const auto program = core::entry_settings_program(protocol)) {
			const auto levels = *program == core::agent_program::claude ? std::array<const char*, 5>{ "low", "medium", "high", "xhigh", "max" }
																		: std::array<const char*, 5>{ "minimal", "low", "medium", "high", "xhigh" };
			for (const auto* const level : levels) {
				Effort->addItem(QString::fromLatin1(level), QString::fromLatin1(level));
			}
		}
		Effort->setCurrentIndex(std::max(0, Effort->findData(kept)));
		Model->setEnabled(core::entry_settings_program(protocol).has_value());
		// A plain-text harness is given no allowance at all.
		const auto grants = core::entry_settings_program(protocol).has_value();
		ScratchStarts->setEnabled(grants);
		Builds->setEnabled(grants);
		McpServers->setEnabled(grants);
		Instructions->setEnabled(grants);
		// [[BPR-2305]]. Codex names the two places by its own words: its
		// developer instructions, or the start of the prompt it reads.
		const auto codex = protocol == core::agent_protocol::codex_jsonl_exec || protocol == core::agent_protocol::codex_app_server;
		if (LauncherSandbox != nullptr) {
			LauncherSandbox->setVisible(codex);
			LauncherSandboxMeaning->setVisible(codex);
		}
		Instructions->setItemText(0, codex ? tr("As developer instructions") : tr("System prompt"));
		Instructions->setItemText(1, codex ? tr("At the start of the prompt") : tr("In the message"));
		Instructions->setToolTip(codex ? tr("Where a built-in prompt's instructions go. Choose At the start of the prompt if Codex runs through a launcher that drops its developer instructions; Check tells you which one it follows.")
									   : tr("Where a built-in prompt's instructions go. Choose In the message if this agent's wrapper ignores the system prompt."));
	}

	auto agent_entry_dialog::entry() const -> core::agent_entry
	{
		auto result      = Base;
		result.Name      = Name->text().trimmed().toStdString();
		result.Protocol  = static_cast<core::agent_protocol>(Protocol->currentData().toInt());
		result.Program   = Bundle && Bundle->has_value() ? **Bundle : Program->text().trimmed().toStdString();
		result.Arguments = Arguments->arguments();
		result.CheckArguments.clear();
		for (const auto& line : Check->toPlainText().split(QLatin1Char{ '\n' })) {
			if (!line.trimmed().isEmpty()) {
				result.CheckArguments.push_back(line.trimmed().toStdString());
			}
		}
		result.Model  = Model->isEnabled() ? Model->text().trimmed().toStdString() : std::string{ };
		result.Effort = Effort->currentData().toString().toStdString();
		// A popular agent whose protocol was changed is a custom one now.
		const auto codex = result.Protocol == core::agent_protocol::codex_app_server || result.Protocol == core::agent_protocol::codex_jsonl_exec;
		if ((result.Preset == core::setup_agent::claude_code && result.Protocol != core::agent_protocol::claude_stream_json) || (result.Preset == core::setup_agent::codex && !codex)) {
			result.Preset.reset();
		}
		result.ScratchStarts  = ScratchStarts->currentData().toBool();
		result.BuildsAndTests = Builds->isChecked();
		result.McpServers     = static_cast<core::agent_mcp_servers>(McpServers->currentData().toInt());
		result.Instructions   = static_cast<core::agent_instruction_delivery>(Instructions->currentData().toInt());
		// A popular agent keeps its sign-in check and strict run checks.
		result.StrictRunChecks = StrictChecks == nullptr || StrictChecks->isChecked();
		// [[BPR-2307]]. Only a custom agent that speaks a Codex protocol.
		result.RelyOnLauncherSandbox = LauncherSandbox != nullptr && codex && LauncherSandbox->isChecked();
		if (result.Preset) {
			result.CheckArguments.clear();
			result.StrictRunChecks       = true;
			result.RelyOnLauncherSandbox = false;
		}
		return result;
	}

	void agent_entry_dialog::settle()
	{
		const auto refusal = core::inspect_agent_entry(entry());
		const auto bundle  = Bundle && !Bundle->has_value() ? bundle_words(Bundle->error(), Program->text().trimmed()) : QString{ };
		Problem->setText(!bundle.isEmpty() ? bundle : refusal ? text_of(core::describe(*refusal))
															  : QString{ });
		Problem->setVisible(!bundle.isEmpty() || refusal.has_value());
		Buttons->button(QDialogButtonBox::Ok)->setEnabled(bundle.isEmpty() && !refusal.has_value());
	}

	auto agent_entry_dialog::event(QEvent* aEvent) -> bool
	{
		const auto handled = QDialog::event(aEvent);
		if (aEvent->type() == QEvent::LayoutRequest) {
			fit_height();
		}
		return handled;
	}

	void agent_entry_dialog::resizeEvent(QResizeEvent* aEvent)
	{
		QDialog::resizeEvent(aEvent);
		fit_height();
	}

	void agent_entry_dialog::fit_height()
	{
		auto* const column = layout();
		if (column == nullptr || !column->hasHeightForWidth()) {
			return;
		}
		auto need = column->totalHeightForWidth(width());
		if (const auto* const shown = screen(); shown != nullptr) {
			need = std::min(need, shown->availableGeometry().height());
		}
		need = std::max(need, column->totalMinimumSize().height());
		if (need > 0 && need != minimumHeight()) {
			setMinimumHeight(need);
		}
	}

	auto default_agent_entry_editor(agent_program_host aHost) -> agent_entry_editor
	{
		return [host = std::move(aHost)](const core::agent_entry& aStart, QWidget* aParent) -> std::optional<core::agent_entry> {
			auto dialog = agent_entry_dialog{ aStart, aParent, host };
			if (dialog.exec() != QDialog::Accepted) {
				return std::nullopt;
			}
			return dialog.entry();
		};
	}
} // namespace bpr::qt
