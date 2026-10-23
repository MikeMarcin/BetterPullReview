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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

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

	folder_entry::folder_entry(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("folderEntry"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(4);
		auto* const row = new QHBoxLayout{ };
		Field           = new QLineEdit{ this };
		Field->setObjectName(QStringLiteral("folderEntryPath"));
		Field->setPlaceholderText(QDir::toNativeSeparators(QStringLiteral("~/Documents/GitHub")));
		Field->setAccessibleName(tr("Clone folder"));
		Field->setClearButtonEnabled(true);
		row->addWidget(Field, 1);
		auto* const choose = new quiet_push_button{ tr("Browse…"), this };
		choose->setObjectName(QStringLiteral("folderEntryBrowse"));
		choose->setToolTip(tr("Choose the folder in a file browser"));
		const auto pick = [this] {
			const auto start  = Field->text().trimmed().isEmpty() ? QDir::homePath() : Field->text().trimmed();
			const auto folder = QFileDialog::getExistingDirectory(this, tr("Choose a clone folder"), start);
			if (!folder.isEmpty()) {
				Field->setText(QDir::toNativeSeparators(folder));
				Field->setFocus(Qt::OtherFocusReason);
			}
		};
		connect(choose, &QPushButton::clicked, this, pick);
		row->addWidget(choose);
		Add = new QPushButton{ tr("Add"), this };
		Add->setObjectName(QStringLiteral("folderEntryAdd"));
		Add->setDefault(true);
		Add->setEnabled(false);
		connect(Add, &QPushButton::clicked, this, [this] { add(); });
		connect(Field, &QLineEdit::returnPressed, this, [this] { add(); });
		connect(Field, &QLineEdit::textChanged, this, [this](const QString& aText) {
			Add->setEnabled(!aText.trimmed().isEmpty());
			Status->clear();
		});
		row->addWidget(Add);
		column->addLayout(row);
		Status = note({ }, this);
		Status->setObjectName(QStringLiteral("folderEntryStatus"));
		column->addWidget(Status);
	}

	void folder_entry::set_add(std::function<void(std::string)> aAdd) { Adder = std::move(aAdd); }

	void folder_entry::add()
	{
		auto text = Field->text().trimmed();
		if (text.isEmpty()) {
			return;
		}
		if (text == QStringLiteral("~") || text.startsWith(QStringLiteral("~/"))) {
			text = QDir::homePath() + text.mid(1);
		}
		if (!QDir::isAbsolutePath(text)) {
			Status->setText(tr("Write the full path of the folder, starting at %1, or use Browse….").arg(QDir::toNativeSeparators(QDir::rootPath())));
			return;
		}
		const auto folder = QDir::toNativeSeparators(QDir::cleanPath(text));
		Field->clear();
		Status->setText(tr("Added %1. Looking in it for clones…").arg(folder));
		if (Adder) {
			Adder(folder.toStdString());
		}
	}

	agent_programs_panel::agent_programs_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentProgramsPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->addWidget(heading(tr("Agent setup"), this));
		auto* const setUp = new quiet_push_button{ tr("Set up agent…"), this };
		setUp->setObjectName(QStringLiteral("settingsSetUpAgent"));
		connect(setUp, &QPushButton::clicked, this, [this] {
			if (Dispatch.SetUp) {
				Dispatch.SetUp();
			}
		});
		column->addWidget(setUp, 0, Qt::AlignLeft);

		const auto pathRow = [this, column](const core::setup_agent aAgent, const QString& aName, QLineEdit*& aField, QLabel*& aStatus) {
			auto* const row = new QHBoxLayout{ };
			aField          = new QLineEdit{ this };
			aField->setObjectName(aName);
			aField->setPlaceholderText(tr("An absolute path to the program"));
			row->addWidget(aField, 1);
			auto* const choose = new quiet_push_button{ tr("Choose…"), this };
			connect(choose, &QPushButton::clicked, this, [this, field = aField] {
				const auto chosen = QFileDialog::getOpenFileName(this, tr("Choose the program"), field->text());
				if (!chosen.isEmpty()) {
					field->setText(QDir::toNativeSeparators(chosen));
				}
			});
			row->addWidget(choose);
			auto* const save = new QPushButton{ tr("Save"), this };
			save->setObjectName(aName + QStringLiteral("Save"));
			connect(save, &QPushButton::clicked, this, [this, aAgent, field = aField] {
				if (Dispatch.SetProgram) {
					Dispatch.SetProgram(aAgent, field->text().trimmed().toStdString());
				}
			});
			row->addWidget(save);
			column->addLayout(row);
			aStatus = note({ }, this);
			aStatus->setObjectName(aName + QStringLiteral("Status"));
			column->addWidget(aStatus);
		};

		column->addSpacing(8);
		column->addWidget(heading(tr("Codex"), this));
		column->addWidget(note(tr("Codex is found on PATH and in its usual install folders. A program saved here is used instead; empty uses the one found."), this));
		pathRow(core::setup_agent::codex, QStringLiteral("settingsCodexProgram"), CodexPath, CodexStatus);
		auto* const form = new QFormLayout{ };
		Protocol         = new QComboBox{ this };
		Protocol->setObjectName(QStringLiteral("settingsCodexProtocol"));
		Protocol->addItem(tr("App Server"), static_cast<int>(core::codex_protocol_choice::app_server));
		Protocol->addItem(tr("JSONL"), static_cast<int>(core::codex_protocol_choice::jsonl));
		connect(Protocol, &QComboBox::currentIndexChanged, this, [this](const int aIndex) {
			if (aIndex >= 0 && Dispatch.SetProtocol) {
				Dispatch.SetProtocol(static_cast<core::codex_protocol_choice>(Protocol->itemData(aIndex).toInt()));
			}
		});
		form->addRow(tr("Protocol"), Protocol);
		column->addLayout(form);

		column->addSpacing(8);
		column->addWidget(heading(tr("A harness that reads plain text"), this));
		column->addWidget(note(tr("For a tool that has no Claude or Codex interface: it reads the request on standard input and writes plain text. It runs only the program chosen here. A wrapper around Claude belongs under Claude › Process wrapper."), this));
		pathRow(core::setup_agent::plain_text_harness, QStringLiteral("settingsHarnessProgram"), HarnessPath, HarnessStatus);
	}

	void agent_programs_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void agent_programs_panel::present(const application::agent_setup_preferences& aPreferences)
	{
		const auto block = QSignalBlocker{ Protocol };
		if (!CodexPath->hasFocus()) {
			CodexPath->setText(text_of(aPreferences.CodexProgram));
		}
		if (!HarnessPath->hasFocus()) {
			HarnessPath->setText(text_of(aPreferences.HarnessProgram));
		}
		Protocol->setCurrentIndex(Protocol->findData(static_cast<int>(aPreferences.CodexProtocol)));
	}

	void agent_programs_panel::show_answer(const core::setup_agent aAgent, const std::optional<core::agent_wrapper_refusal>& aRefusal)
	{
		auto* const status = aAgent == core::setup_agent::codex ? CodexStatus : HarnessStatus;
		status->setText(aRefusal ? refusal_words(*aRefusal) : tr("Saved."));
	}

	clone_folders_panel::clone_folders_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("cloneFoldersPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->addWidget(heading(tr("Clone folders"), this));
		column->addWidget(note(tr("Where your clones live. The agent setup looks here, two levels down, for a clone of each pull request's repository. It reads only each clone's Git configuration and current branch, runs nothing and writes nothing."), this));
		Folders = new QListWidget{ this };
		Folders->setObjectName(QStringLiteral("settingsCloneFolders"));
		Folders->setMaximumHeight(120);
		column->addWidget(Folders);
		Entry = new folder_entry{ this };
		Entry->setObjectName(QStringLiteral("settingsAddCloneFolder"));
		Entry->set_add([this](std::string aFolder) {
			if (Dispatch.AddFolder) {
				Dispatch.AddFolder(std::move(aFolder));
			}
		});
		column->addWidget(Entry);
		auto* const row = new QHBoxLayout{ };
		Remove          = new quiet_push_button{ tr("Remove the selected folder"), this };
		Remove->setObjectName(QStringLiteral("settingsRemoveCloneFolder"));
		connect(Remove, &QPushButton::clicked, this, [this] {
			const auto at = Folders->currentRow();
			if (at >= 0 && static_cast<std::size_t>(at) < Shown.Folders.size() && Dispatch.RemoveFolder) {
				Dispatch.RemoveFolder(Shown.Folders[static_cast<std::size_t>(at)]);
			}
		});
		row->addWidget(Remove);
		row->addStretch(1);
		column->addLayout(row);

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
		connect(Folders, &QListWidget::currentRowChanged, this, [this](const int aRow) { Remove->setEnabled(aRow >= 0); });
		connect(Links, &QListWidget::currentRowChanged, this, [this](const int aRow) { Forget->setEnabled(aRow >= 0); });
		Remove->setEnabled(false);
		Forget->setEnabled(false);
	}

	void clone_folders_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void clone_folders_panel::present(const application::clone_link_projection& aProjection)
	{
		Shown = aProjection;
		Folders->clear();
		for (const auto& folder : Shown.Folders) {
			Folders->addItem(QDir::toNativeSeparators(text_of(folder)));
		}
		Links->clear();
		for (const auto& link : Shown.Links) {
			const auto target = link.PrivateCopy ? tr("a private copy") : QDir::toNativeSeparators(text_of(link.Path));
			Links->addItem(tr("%1/%2 on %3: %4").arg(text_of(link.Owner), text_of(link.Name), text_of(link.Host), target));
		}
		Remove->setEnabled(Folders->currentRow() >= 0);
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
