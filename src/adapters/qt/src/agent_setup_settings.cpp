#include "bpr/qt/agent_setup_settings.hpp"

#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto heading(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        bold  = label->font();
			bold.setWeight(QFont::DemiBold);
			label->setFont(bold);
			return label;
		}

		[[nodiscard]] auto note(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		[[nodiscard]] auto refusal_words(const core::agent_wrapper_refusal aRefusal) -> QString { return text_of(core::describe(aRefusal)); }

		[[nodiscard]] auto skill_refusal_words(const core::review_skill_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::review_skill_refusal::too_large:       return QObject::tr("The skill is larger than 64 KiB. Shorten it, then save.");
			case core::review_skill_refusal::not_text:        return QObject::tr("The skill is not UTF-8 text.");
			case core::review_skill_refusal::no_front_matter: return QObject::tr("The skill needs its front matter: a --- block at the top with its name and description.");
			case core::review_skill_refusal::name_changed:    return QObject::tr("The skill's name must stay bpr-pr-review.");
			case core::review_skill_refusal::no_description:  return QObject::tr("The skill needs a description in its front matter.");
			}
			return { };
		}
	} // namespace

	agent_programs_panel::agent_programs_panel(QWidget* aParent)
		: QWidget{ aParent }
		, Editor{ default_agent_entry_editor() }
	{
		setObjectName(QStringLiteral("agentProgramsPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->addWidget(heading(tr("Agents"), this));
		column->addWidget(note(tr("The agents the Agent tab offers, in this order. An agent you have not added is never offered. Each runs its own program with the fixed arguments you give it, such as a wrapper given --codex for Codex models."), this));
		List = new QListWidget{ this };
		List->setObjectName(QStringLiteral("settingsAgentList"));
		List->setMaximumHeight(160);
		connect(List, &QListWidget::currentRowChanged, this, [this] { settle_buttons(); });
		connect(List, &QListWidget::itemDoubleClicked, this, [this] { EditButton->click(); });
		column->addWidget(List);
		auto* const row = new QHBoxLayout{ };
		AddButton       = new quiet_push_button{ tr("Add agent…"), this };
		AddButton->setObjectName(QStringLiteral("settingsAddAgent"));
		AddButton->setMenu(new QMenu{ AddButton });
		connect(AddButton->menu(), &QMenu::aboutToShow, this, [this] {
			auto* const menu = AddButton->menu();
			menu->clear();
			// Claude Code and Codex when found here and not added yet.
			for (const auto preset : { core::setup_agent::claude_code, core::setup_agent::codex }) {
				const auto found = std::ranges::find(Found, preset, &core::agent_detection::Agent);
				if (found == Found.end() || core::find_preset_entry(Agents, preset) != nullptr) {
					continue;
				}
				auto* const item = menu->addAction(tr("%1 (found at %2)").arg(text_of(core::plain_name(preset)), QDir::toNativeSeparators(text_of(found->Path))));
				item->setObjectName(preset == core::setup_agent::claude_code ? QStringLiteral("settingsAddClaude") : QStringLiteral("settingsAddCodex"));
				connect(item, &QAction::triggered, this, [this, preset] { add_agent(preset); });
			}
			if (!menu->actions().isEmpty()) {
				menu->addSeparator();
			}
			auto* const custom = menu->addAction(tr("Custom agent…"));
			custom->setObjectName(QStringLiteral("settingsAddCustom"));
			connect(custom, &QAction::triggered, this, [this] { add_agent(std::nullopt); });
		});
		row->addWidget(AddButton);
		EditButton = new quiet_push_button{ tr("Edit…"), this };
		EditButton->setObjectName(QStringLiteral("settingsEditAgent"));
		connect(EditButton, &QPushButton::clicked, this, [this] {
			const auto* const entry = selected();
			if (entry == nullptr || !Editor || !Dispatch.Update) {
				return;
			}
			if (auto edited = Editor(*entry, this)) {
				Dispatch.Update(std::move(*edited), [this](const std::optional<core::agent_entry_refusal> aRefusal) {
					Status->setText(aRefusal ? text_of(core::describe(*aRefusal)) : tr("Saved."));
				});
			}
		});
		row->addWidget(EditButton);
		RemoveButton = new quiet_push_button{ tr("Remove"), this };
		RemoveButton->setObjectName(QStringLiteral("settingsRemoveAgent"));
		connect(RemoveButton, &QPushButton::clicked, this, [this] {
			if (const auto* const entry = selected(); entry != nullptr && Dispatch.Remove) {
				Dispatch.Remove(entry->Id);
			}
		});
		row->addWidget(RemoveButton);
		UpButton = new quiet_push_button{ tr("Move up"), this };
		UpButton->setObjectName(QStringLiteral("settingsAgentUp"));
		connect(UpButton, &QPushButton::clicked, this, [this] {
			if (const auto* const entry = selected(); entry != nullptr && Dispatch.Move) {
				Dispatch.Move(entry->Id, -1);
			}
		});
		row->addWidget(UpButton);
		DownButton = new quiet_push_button{ tr("Move down"), this };
		DownButton->setObjectName(QStringLiteral("settingsAgentDown"));
		connect(DownButton, &QPushButton::clicked, this, [this] {
			if (const auto* const entry = selected(); entry != nullptr && Dispatch.Move) {
				Dispatch.Move(entry->Id, 1);
			}
		});
		row->addWidget(DownButton);
		row->addStretch(1);
		column->addLayout(row);
		Status = note({ }, this);
		Status->setObjectName(QStringLiteral("settingsAgentStatus"));
		column->addWidget(Status);
		build_check(*column);
		// Owner decision 2026-10-23 ("I guess we can keep a timeout as an
		// option, but i wouldnt want it on by default"): off unless chosen.
		auto* const limitRow = new QHBoxLayout{ };
		RunLimitOn           = new QCheckBox{ tr("Stop a run after"), this };
		RunLimitOn->setObjectName(QStringLiteral("settingsRunLimitOn"));
		limitRow->addWidget(RunLimitOn);
		RunLimit = new QSpinBox{ this };
		RunLimit->setObjectName(QStringLiteral("settingsRunLimitMinutes"));
		RunLimit->setRange(1, static_cast<int>(application::maximum_run_limit_minutes));
		RunLimit->setValue(10);
		RunLimit->setSuffix(tr(" min"));
		RunLimit->setEnabled(false);
		limitRow->addWidget(RunLimit);
		limitRow->addStretch(1);
		column->addLayout(limitRow);
		column->addWidget(note(tr("Off, a run goes on until the agent finishes or you press Stop."), this));
		connect(RunLimitOn, &QCheckBox::toggled, this, [this](const bool aOn) {
			RunLimit->setEnabled(aOn);
			send_run_limit();
		});
		connect(RunLimit, &QSpinBox::editingFinished, this, [this] { send_run_limit(); });
		column->addSpacing(8);
		// Owner look v006: the setup is not entered again except to start
		// afresh; Reset agents… asks first, then clears every agent setting.
		auto* const reset = new quiet_push_button{ tr("Reset agents…"), this };
		reset->setObjectName(QStringLiteral("settingsResetAgents"));
		reset->setToolTip(tr("Remove every agent and its settings, the clone folders and linked clones, and the edited review skill, then set up again"));
		connect(reset, &QPushButton::clicked, this, [this] {
			if (Dispatch.Reset) {
				Dispatch.Reset();
			}
		});
		column->addWidget(reset, 0, Qt::AlignLeft);
		settle_buttons();
	}

	void agent_programs_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void agent_programs_panel::send_run_limit()
	{
		if (Dispatch.SetRunLimit) {
			static_cast<void>(Dispatch.SetRunLimit(RunLimitOn->isChecked() ? static_cast<std::uint32_t>(RunLimit->value()) : 0U));
		}
	}

	void agent_programs_panel::set_editor(agent_entry_editor aEditor) { Editor = std::move(aEditor); }

	void agent_programs_panel::add_agent(const std::optional<core::setup_agent> aPreset)
	{
		if (!Editor || !Dispatch.Add) {
			return;
		}
		auto start = aPreset ? core::preset_agent_entry(*aPreset, core::codex_protocol_choice::app_server, { }) : core::agent_entry{ };
		if (auto chosen = Editor(start, this)) {
			Dispatch.Add(std::move(*chosen), [this](const std::optional<core::agent_entry_refusal> aRefusal) {
				Status->setText(aRefusal ? text_of(core::describe(*aRefusal)) : tr("Added."));
			});
		}
	}

	auto agent_programs_panel::selected() const -> const core::agent_entry*
	{
		const auto row = List->currentRow();
		return row < 0 || static_cast<std::size_t>(row) >= Agents.size() ? nullptr : &Agents[static_cast<std::size_t>(row)];
	}

	void agent_programs_panel::settle_buttons()
	{
		const auto row = List->currentRow();
		EditButton->setEnabled(row >= 0);
		RemoveButton->setEnabled(row >= 0);
		UpButton->setEnabled(row > 0);
		DownButton->setEnabled(row >= 0 && row + 1 < List->count());
	}

	void agent_programs_panel::present(const application::agent_setup_preferences& aPreferences, std::vector<core::agent_detection> aFound)
	{
		Found = std::move(aFound);
		{
			const auto blockToggle  = QSignalBlocker{ RunLimitOn };
			const auto blockMinutes = QSignalBlocker{ RunLimit };
			RunLimitOn->setChecked(aPreferences.RunLimitMinutes != 0U);
			RunLimit->setEnabled(aPreferences.RunLimitMinutes != 0U);
			if (aPreferences.RunLimitMinutes != 0U && !RunLimit->hasFocus()) {
				RunLimit->setValue(static_cast<int>(aPreferences.RunLimitMinutes));
			}
		}
		const auto keptId = selected() != nullptr ? selected()->Id : 0U;
		Agents            = aPreferences.Agents;
		const auto block  = QSignalBlocker{ List };
		List->clear();
		auto current = -1;
		for (const auto& entry : Agents) {
			auto line = text_of(entry.Name) + QStringLiteral(" — ") + QDir::toNativeSeparators(entry.Program.empty() ? tr("its own program") : text_of(entry.Program));
			for (const auto& argument : entry.Arguments) {
				line += QStringLiteral(" ") + text_of(argument);
			}
			if (!entry.Model.empty() || !entry.Effort.empty()) {
				line += QStringLiteral(" · ") + (entry.Model.empty() ? tr("default model") : text_of(entry.Model)) + (entry.Effort.empty() ? QString{ } : QStringLiteral(" · ") + text_of(entry.Effort));
			}
			auto* const item = new QListWidgetItem{ line, List };
			item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(entry.Id));
			if (entry.Id == keptId) {
				current = List->count() - 1;
			}
		}
		List->setCurrentRow(current);
		settle_buttons();
	}

	void agent_programs_panel::build_check(QVBoxLayout& aColumn)
	{
		aColumn.addSpacing(8);
		auto* const row = new QHBoxLayout{ };
		row->setSpacing(8);
		CheckButton = new quiet_push_button{ tr("Check agent"), this };
		CheckButton->setObjectName(QStringLiteral("settingsCheckAgent"));
		connect(CheckButton, &QPushButton::clicked, this, [this] {
			if (Dispatch.Check) {
				Dispatch.Check();
			}
		});
		row->addWidget(CheckButton, 0, Qt::AlignVCenter);
		StopCheckButton = new quiet_push_button{ tr("Stop"), this };
		StopCheckButton->setObjectName(QStringLiteral("settingsStopAgentCheck"));
		StopCheckButton->hide();
		connect(StopCheckButton, &QPushButton::clicked, this, [this] {
			if (Dispatch.StopCheck) {
				Dispatch.StopCheck();
			}
		});
		row->addWidget(StopCheckButton, 0, Qt::AlignVCenter);
		auto* const progress = new QProgressBar{ this };
		progress->setObjectName(QStringLiteral("settingsAgentCheckProgress"));
		progress->setRange(0, 0);
		progress->setTextVisible(false);
		progress->setMaximumWidth(64);
		progress->hide();
		CheckProgress = progress;
		row->addWidget(progress, 0, Qt::AlignVCenter);
		CheckNote = note({ }, this);
		CheckNote->setObjectName(QStringLiteral("settingsAgentCheckNote"));
		row->addWidget(CheckNote, 1, Qt::AlignVCenter);
		aColumn.addLayout(row);

		// The rows, as Check it works draws them: a mark and what it proves.
		CheckRows        = new QWidget{ this };
		auto* const rows = new QVBoxLayout{ CheckRows };
		rows->setContentsMargins(4, 2, 0, 0);
		rows->setSpacing(4);
		const auto line = [this, rows](QLabel*& aMark, QLabel*& aText, const QString& aName) {
			auto* const holder = new QWidget{ CheckRows };
			auto* const across = new QHBoxLayout{ holder };
			across->setContentsMargins(0, 0, 0, 0);
			across->setSpacing(8);
			aMark = new QLabel{ holder };
			aMark->setObjectName(aName + QStringLiteral("Mark"));
			aMark->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			aMark->setMinimumWidth(aMark->fontMetrics().horizontalAdvance(QChar{ 0x2713 }) + 2);
			across->addWidget(aMark, 0, Qt::AlignTop);
			aText = new QLabel{ holder };
			aText->setObjectName(aName);
			aText->setWordWrap(true);
			aText->setTextFormat(Qt::PlainText);
			aText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			across->addWidget(aText, 1);
			rows->addWidget(holder);
			return holder;
		};
		static_cast<void>(line(AgentMark, AgentRow, QStringLiteral("settingsAgentCheckRow")));
		WorkspaceLine = line(WorkspaceMark, WorkspaceRow, QStringLiteral("settingsWorkspaceCheckRow"));
		// What failed, what to do and the details sit under the rows' words.
		auto* const follow = new QWidget{ CheckRows };
		auto* const under  = new QVBoxLayout{ follow };
		under->setContentsMargins(AgentMark->minimumWidth() + 8, 0, 0, 0);
		under->setSpacing(4);
		rows->addWidget(follow);
		CheckFailure = new QLabel{ CheckRows };
		CheckFailure->setObjectName(QStringLiteral("settingsAgentCheckFailure"));
		CheckFailure->setWordWrap(true);
		CheckFailure->setTextFormat(Qt::PlainText);
		under->addWidget(CheckFailure);
		SignInLine             = new QWidget{ CheckRows };
		auto* const signInLine = new QHBoxLayout{ SignInLine };
		signInLine->setContentsMargins(0, 0, 0, 0);
		SignInCommand = new QLabel{ SignInLine };
		SignInCommand->setObjectName(QStringLiteral("settingsAgentSignInCommand"));
		SignInCommand->setTextInteractionFlags(Qt::TextSelectableByMouse);
		SignInCommand->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		signInLine->addWidget(SignInCommand);
		auto* const copy = new quiet_push_button{ tr("Copy"), SignInLine };
		copy->setObjectName(QStringLiteral("settingsAgentCopyCommand"));
		connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(SignInCommand->text()); });
		signInLine->addWidget(copy);
		signInLine->addStretch(1);
		under->addWidget(SignInLine);
		WorkspaceReason = new QLabel{ CheckRows };
		WorkspaceReason->setObjectName(QStringLiteral("settingsWorkspaceCheckFailure"));
		WorkspaceReason->setWordWrap(true);
		WorkspaceReason->setTextFormat(Qt::PlainText);
		under->addWidget(WorkspaceReason);
		auto* const toggle = new disclosure_button{ CheckRows };
		toggle->setObjectName(QStringLiteral("settingsAgentCheckDetails"));
		toggle->setText(tr("Show details"));
		toggle->setAccessibleName(tr("Show details"));
		DetailsToggle = toggle;
		under->addWidget(toggle, 0, Qt::AlignLeft);
		DetailsText = new QPlainTextEdit{ CheckRows };
		DetailsText->setObjectName(QStringLiteral("settingsAgentCheckDetailsText"));
		DetailsText->setAccessibleName(tr("Check details"));
		DetailsText->setReadOnly(true);
		DetailsText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		DetailsText->setMinimumHeight(96);
		DetailsText->setMaximumHeight(144);
		DetailsText->hide();
		under->addWidget(DetailsText);
		connect(toggle, &QToolButton::toggled, DetailsText, &QWidget::setVisible);
		CheckRows->hide();
		aColumn.addWidget(CheckRows);
	}

	void agent_programs_panel::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if ((aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) && ShownCheck) {
			const auto shown = *ShownCheck;
			present_check(shown);
		}
	}

	void agent_programs_panel::present_check(const agent_check_presentation& aCheck)
	{
		ShownCheck                   = aCheck;
		const auto& setup            = aCheck.Setup;
		const auto& state            = setup.State;
		const auto  saved            = state.Chosen ? state.Chosen : setup.Preferences.Chosen;
		const auto  name             = saved ? setup_agent_name(setup.Preferences, *saved) : tr("The agent");
		const auto  offer            = aCheck.Decision.Offer;
		const auto  running          = offer == core::agent_check_offer::running;
		const auto  workspaceRunning = state.WorkspaceCheck.State == core::setup_check_state::running;
		const auto  ran              = state.AgentCheck.State != core::setup_check_state::not_run || state.WorkspaceCheck.State != core::setup_check_state::not_run;
		CheckButton->setText(ran && !running ? tr("Check again") : tr("Check agent"));
		CheckButton->setEnabled(offer == core::agent_check_offer::available);
		StopCheckButton->setVisible(running);
		CheckProgress->setVisible(running);

		// What it does, what runs now, or why it cannot run.
		auto said = QString{ };
		switch (offer) {
		case core::agent_check_offer::available: {
			const auto agent = saved.value_or(core::setup_agent::claude_code);
			said             = !setup.Detected ? tr("Looks for your agents, then checks the chosen one as the setup does. No question or code is sent.")
				: !setup.ChecksSignIn          ? tr("Checks that %1's program can run, and runs its own check if it names one. No question or code is sent.").arg(name)
				: setup.ChecksWrapper          ? tr("Asks your Claude process wrapper for its sign-in status and version. No question or code is sent.")
											   : tr("Runs %1, which sends no question and no code.").arg(status_command(agent));
			said += QStringLiteral(" ")
				+ (aCheck.Decision.ChecksWorkspace ? tr("It also makes the open pull request's workspace once, then removes it.") : tr("Open a pull request to check its workspace too."));
			break;
		}
		case core::agent_check_offer::running:
			said = setup.Checking && workspaceRunning ? tr("Checking %1 and preparing the workspace…").arg(name)
				: setup.Checking                      ? (setup.Detected ? tr("Checking %1…").arg(name) : tr("Looking for your agents…"))
													  : tr("Preparing the workspace…");
			break;
		case core::agent_check_offer::no_agent:   said = tr("Add an agent, then check it here."); break;
		case core::agent_check_offer::not_chosen: said = tr("Several agents were found. Choose one in the Agent tab's setup, then check it here."); break;
		case core::agent_check_offer::forbidden:  said = tr("The open pull request's host doesn't allow %1, so it can't be checked now.").arg(name); break;
		}
		CheckNote->setText(said);
		CheckButton->setToolTip(offer == core::agent_check_offer::available ? tr("Run the setup's check of %1 again").arg(name)
									: running                               ? tr("A check is running. Stop it to start again.")
																			: said);
		CheckButton->setAccessibleDescription(offer == core::agent_check_offer::available ? QString{ } : CheckButton->toolTip());
		CheckProgress->setAccessibleName(said);

		// The rows once a check has run this session.
		const auto mark = [this](QLabel* aMark, const core::setup_check_state aState) {
			const auto [glyph, colour] = setup_check_mark(aState, palette());
			aMark->setText(glyph);
			auto shade = aMark->palette();
			shade.setColor(QPalette::WindowText, colour);
			aMark->setPalette(shade);
		};
		const auto agentRan     = state.AgentCheck.State != core::setup_check_state::not_run;
		const auto workspaceRan = state.WorkspaceCheck.State != core::setup_check_state::not_run;
		CheckRows->setVisible(agentRan || workspaceRan);
		mark(AgentMark, state.AgentCheck.State);
		auto proves = agent_check_words(setup, name);
		if (state.AgentCheck.State == core::setup_check_state::passed && !state.AgentCheck.Version.empty()) {
			proves = tr("%1, version %2").arg(proves, text_of(state.AgentCheck.Version));
		}
		AgentRow->setText(proves);
		AgentMark->parentWidget()->setVisible(agentRan);
		WorkspaceLine->setVisible(workspaceRan);
		mark(WorkspaceMark, state.WorkspaceCheck.State);
		WorkspaceRow->setText(tr("The open pull request's workspace can be made"));

		const auto agentFailed = state.AgentCheck.State == core::setup_check_state::failed && state.AgentCheck.Failure.has_value();
		CheckFailure->setVisible(agentFailed);
		CheckFailure->setText(agentFailed ? setup_failure_words(*state.AgentCheck.Failure, name, text_of(setup.CheckDetail)) : QString{ });
		const auto signIn = agentFailed && *state.AgentCheck.Failure == core::agent_setup_failure::not_signed_in && saved ? sign_in_command(*saved) : QString{ };
		SignInCommand->setText(signIn);
		SignInLine->setVisible(!signIn.isEmpty());
		const auto workspaceFailed = state.WorkspaceCheck.State == core::setup_check_state::failed;
		WorkspaceReason->setVisible(workspaceFailed);
		WorkspaceReason->setText(!workspaceFailed             ? QString{ }
									 : aCheck.WorkspaceReason ? workspace_unavailable_words(*aCheck.WorkspaceReason)
															  : setup_failure_words(core::agent_setup_failure::workspace_unavailable, name, { }));
		// A failed check's own output, and the workspace's log, behind one
		// disclosure; a new detail closes it, as in the setup.
		auto detail = agentFailed ? setup.CheckDetail : std::string{ };
		if (workspaceFailed && !setup.WorkspaceDetail.empty()) {
			detail += (detail.empty() ? std::string{ } : std::string{ "\n\n" }) + setup.WorkspaceDetail;
		}
		DetailsToggle->setVisible(agentFailed || workspaceFailed);
		if (detail != ShownDetail || !(agentFailed || workspaceFailed)) {
			DetailsToggle->setChecked(false);
		}
		ShownDetail = detail;
		DetailsText->setPlainText(detail.empty() ? tr("No process output was returned.") : text_of(detail));
	}

	auto agent_programs_panel::check_result_text() const -> QString
	{
		auto lines = QStringList{ };
		for (const auto* label : { AgentRow, WorkspaceRow, CheckFailure, SignInCommand, WorkspaceReason }) {
			if (label->isVisibleTo(this) && !label->text().isEmpty()) {
				const auto* const mark = label == AgentRow ? AgentMark : label == WorkspaceRow ? WorkspaceMark
																							   : nullptr;
				lines << (mark != nullptr ? mark->text() + QStringLiteral(" ") : QString{ }) + label->text();
			}
		}
		return lines.join(QLatin1Char{ '\n' });
	}

	void agent_programs_panel::show_answer(const core::setup_agent /*aAgent*/, const std::optional<core::agent_wrapper_refusal>& aRefusal)
	{
		Status->setText(aRefusal ? refusal_words(*aRefusal) : tr("Saved."));
	}

	clone_folders_panel::clone_folders_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("cloneFoldersPanel"));
		auto* const column = new QVBoxLayout{ this };
		// [[BPR-1856]]. The panel follows Add repository on Repositories; the
		// margin above its heading starts a new section rather than reading as
		// part of the button's row.
		column->setContentsMargins(0, 16, 0, 0);
		column->addWidget(heading(tr("Clone folders"), this));
		column->addWidget(note(tr("Where your clones live. The agent setup looks here, two levels down, for a clone of each pull request's repository. It reads only each clone's Git configuration and current branch, runs nothing and writes nothing."), this));
		// Owner look v002: Visual Studio's folder list, edited in place.
		Folders = new folder_list_editor{ this };
		Folders->setObjectName(QStringLiteral("settingsCloneFolders"));
		Folders->set_change([this](std::vector<std::string> aFolders) {
			if (Dispatch.SetFolders) {
				Dispatch.SetFolders(std::move(aFolders));
			}
		});
		column->addWidget(Folders);

		column->addSpacing(8);
		column->addWidget(heading(tr("Linked clones"), this));
		column->addWidget(note(tr("The clone each repository's agent works with, or none. Forget it to choose again."), this));
		Links = new QListWidget{ this };
		Links->setObjectName(QStringLiteral("settingsLinkedClones"));
		Links->setMaximumHeight(140);
		column->addWidget(Links);
		Forget = new QPushButton{ tr("Forget"), this };
		Forget->setObjectName(QStringLiteral("settingsForgetLink"));
		connect(Forget, &QPushButton::clicked, this, [this] {
			const auto at = Links->currentRow();
			if (at >= 0 && static_cast<std::size_t>(at) < Shown.Links.size() && Dispatch.Forget) {
				Dispatch.Forget(Shown.Links[static_cast<std::size_t>(at)]);
			}
		});
		column->addWidget(Forget, 0, Qt::AlignLeft);
		connect(Links, &QListWidget::currentRowChanged, this, [this](const int aRow) { Forget->setEnabled(aRow >= 0); });
		Forget->setEnabled(false);
	}

	void clone_folders_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void clone_folders_panel::present(const application::clone_link_projection& aProjection)
	{
		Shown = aProjection;
		Folders->present(Shown.Folders);
		Links->clear();
		for (const auto& link : Shown.Links) {
			// [[BPR-1873]]. No clone: a private copy, or for a review read
			// without its blobs only the pull request's files.
			// [[BPR-1879]]. The clone BetterPullReview keeps, and where.
			const auto target = link.PrivateCopy ? tr("no clone") : link.Managed ? tr("kept by BetterPullReview in %1").arg(QDir::toNativeSeparators(text_of(link.Path)))
																				 : QDir::toNativeSeparators(text_of(link.Path));
			Links->addItem(tr("%1/%2 on %3: %4").arg(text_of(link.Owner), text_of(link.Name), text_of(link.Host), target));
		}
		Forget->setEnabled(Links->currentRow() >= 0);
	}

	review_skill_panel::review_skill_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("reviewSkillPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		Heading = heading(tr("Review skill"), this);
		column->addWidget(Heading);
		Intro = note(tr("How the agent reviews: sent with every review request, the same for every agent, and written to no harness's configuration. It grants nothing; the agent's own tools and permissions still govern."), this);
		column->addWidget(Intro);
		Source = note({ }, this);
		Source->setObjectName(QStringLiteral("reviewSkillSource"));
		column->addWidget(Source);
		Offer                = new QWidget{ this };
		auto* const offerRow = new QHBoxLayout{ Offer };
		offerRow->setContentsMargins(0, 0, 0, 0);
		OfferText = new QLabel{ Offer };
		OfferText->setObjectName(QStringLiteral("reviewSkillOfferText"));
		offerRow->addWidget(OfferText, 1);
		auto* const compare = new quiet_push_button{ tr("Compare"), Offer };
		compare->setObjectName(QStringLiteral("reviewSkillCompare"));
		compare->setCheckable(true);
		offerRow->addWidget(compare);
		UseVersion = new QPushButton{ Offer };
		UseVersion->setObjectName(QStringLiteral("reviewSkillUseVersion"));
		connect(UseVersion, &QPushButton::clicked, this, [this] {
			if (Dispatch.UseBuiltIn) {
				Dispatch.UseBuiltIn(true);
			}
		});
		offerRow->addWidget(UseVersion);
		auto* const keep = new quiet_push_button{ tr("Keep mine"), Offer };
		keep->setObjectName(QStringLiteral("reviewSkillKeepMine"));
		connect(keep, &QPushButton::clicked, this, [this] {
			if (Dispatch.KeepMine) {
				Dispatch.KeepMine();
			}
		});
		offerRow->addWidget(keep);
		Offer->hide();
		column->addWidget(Offer);
		auto* const texts = new QHBoxLayout{ };
		Editor            = new QPlainTextEdit{ this };
		Editor->setObjectName(QStringLiteral("reviewSkillEditor"));
		Editor->setAccessibleName(tr("Review skill text"));
		Editor->setMinimumHeight(220);
		Editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		texts->addWidget(Editor, 1);
		Default = new QPlainTextEdit{ this };
		Default->setObjectName(QStringLiteral("reviewSkillDefault"));
		Default->setAccessibleName(tr("The newer default"));
		Default->setReadOnly(true);
		Default->hide();
		texts->addWidget(Default, 1);
		column->addLayout(texts);
		connect(compare, &QPushButton::toggled, Default, &QWidget::setVisible);
		auto* const row  = new QHBoxLayout{ };
		auto* const save = new QPushButton{ tr("Save"), this };
		save->setObjectName(QStringLiteral("reviewSkillSave"));
		connect(save, &QPushButton::clicked, this, [this] {
			if (Dispatch.Save) {
				Dispatch.Save(Editor->toPlainText().toStdString());
			}
		});
		row->addWidget(save);
		auto* const restore = new quiet_push_button{ tr("Restore default"), this };
		Restore             = restore;
		restore->setObjectName(QStringLiteral("reviewSkillRestore"));
		connect(restore, &QPushButton::clicked, this, [this] {
			if (Dispatch.UseBuiltIn) {
				Dispatch.UseBuiltIn(KeepCopy->isChecked());
			}
		});
		row->addWidget(restore);
		KeepCopy = new QCheckBox{ tr("Save my copy to Downloads first"), this };
		KeepCopy->setObjectName(QStringLiteral("reviewSkillKeepCopy"));
		KeepCopy->setChecked(true);
		row->addWidget(KeepCopy);
		row->addStretch(1);
		column->addLayout(row);
		Status = note({ }, this);
		Status->setObjectName(QStringLiteral("reviewSkillStatus"));
		column->addWidget(Status);
	}

	void review_skill_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void review_skill_panel::show_as_built_in_prompt(const QFont& aEditorFont)
	{
		Heading->hide();
		Intro->hide();
		Editor->setFont(aEditorFont);
		Default->setFont(aEditorFont);
		Editor->setMinimumHeight(300);
		// The other prompts' words for the same action.
		Restore->setText(tr("Reset to built-in"));
	}

	auto review_skill_panel::status_text() const -> QString { return Status->text(); }

	void review_skill_panel::show_answer(const application::review_skill_answer aAnswer)
	{
		switch (aAnswer) {
		case application::review_skill_answer::done:
		case application::review_skill_answer::refused:     return;
		case application::review_skill_answer::copy_failed: Status->setText(tr("Your copy could not be saved to Downloads, so nothing was removed.")); return;
		case application::review_skill_answer::not_saved:   Status->setText(tr("The skill could not be saved. Your text is still here.")); return;
		}
	}

	void review_skill_panel::present(const application::review_skill_projection& aProjection, const std::string& aBuiltIn)
	{
		if (aProjection.Generation == Generation) {
			return;
		}
		Generation = aProjection.Generation;
		Changed    = aProjection.Source == core::review_skill_source::profile_copy;
		// A refused save keeps what the reviewer wrote.
		if (!aProjection.Refusal) {
			Editor->setPlainText(text_of(aProjection.Text));
		}
		Source->setText(aProjection.Source == core::review_skill_source::built_in
							? tr("The built-in bpr-pr-review, version %1. Editing it makes your own copy, which updates never change.").arg(aProjection.BuiltInVersion)
							: tr("Your copy, made from version %1 of the built-in skill.").arg(aProjection.CopyBaseVersion.value_or(0U)));
		Offer->setVisible(aProjection.Offer.has_value());
		if (aProjection.Offer) {
			OfferText->setText(tr("Version %1 is available").arg(*aProjection.Offer));
			UseVersion->setText(tr("Use version %1").arg(*aProjection.Offer));
			Default->setPlainText(text_of(aBuiltIn));
		}
		auto status = QString{ };
		if (aProjection.Refusal) {
			status = skill_refusal_words(*aProjection.Refusal);
		} else if (aProjection.StoredProblem) {
			status = tr("Your saved copy can't be used: %1 Requests carry the built-in skill until it is fixed.").arg(skill_refusal_words(*aProjection.StoredProblem));
		} else if (aProjection.SavedCopy) {
			status = tr("Your copy was saved to %1.").arg(QDir::toNativeSeparators(text_of(*aProjection.SavedCopy)));
		}
		Status->setText(status);
	}
} // namespace bpr::qt
