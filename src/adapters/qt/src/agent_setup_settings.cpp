#include "bpr/qt/agent_setup_settings.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
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

	void agent_programs_panel::show_answer(const core::setup_agent /*aAgent*/, const std::optional<core::agent_wrapper_refusal>& aRefusal)
	{
		Status->setText(aRefusal ? refusal_words(*aRefusal) : tr("Saved."));
	}

	clone_folders_panel::clone_folders_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("cloneFoldersPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
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
		column->addWidget(note(tr("The clone, or the private copy, each repository's agent works with. Forget it to choose again."), this));
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
			const auto target = link.PrivateCopy ? tr("a private copy") : QDir::toNativeSeparators(text_of(link.Path));
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
		column->addWidget(heading(tr("Review skill"), this));
		column->addWidget(note(tr("How the agent reviews: sent with every review request, the same for every agent, and written to no harness's configuration. It grants nothing; the agent's own tools and permissions still govern."), this));
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
