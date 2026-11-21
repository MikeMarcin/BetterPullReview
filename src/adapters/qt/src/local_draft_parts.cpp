#include "bpr/qt/local_draft_parts.hpp"

#include "bpr/core/terminal_text.hpp"

#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/local_draft_publication_area.hpp"
#include "bpr/qt/local_draft_workflow_area.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "conversation_glyphs.hpp"

#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief [[BPR-2156]]. The platform's save (or, for Reload, refresh)
		/// icon, else the style's.
		[[nodiscard]] auto save_icon(const QWidget& aButton, const bool aReload) -> QIcon
		{
			const auto fallback = aButton.style()->standardIcon(aReload ? QStyle::SP_BrowserReload : QStyle::SP_DialogSaveButton, nullptr, &aButton);
			return QIcon::fromTheme(aReload ? QIcon::ThemeIcon::ViewRefresh : QIcon::ThemeIcon::DocumentSave, fallback);
		}

		constexpr int path_role = Qt::UserRole + 1;
		constexpr int note_role = Qt::UserRole + 2;

		auto text_of(const std::string_view aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		auto short_commit(const core::git_commit_oid& aCommit) -> QString
		{
			return text_of(core::canonical_hex(aCommit).view()).left(12);
		}

		auto repository_text(const core::remote_location& aRepository) -> QString
		{
			return text_of(aRepository.Owner) + QLatin1Char{ '/' } + text_of(aRepository.Name);
		}

		/// One short sentence: what the working file is, never the mechanism.
		auto edit_word(const core::working_file& aFile) -> QString
		{
			switch (aFile.Qualification) {
			case core::working_qualification::filtered:    return QStringLiteral("A Git filter stores other bytes; it cannot be included.");
			case core::working_qualification::too_large:   return QStringLiteral("Too large to include.");
			case core::working_qualification::unsupported: return QStringLiteral("Not a file; it cannot be included.");
			case core::working_qualification::none:        break;
			}
			switch (aFile.Edit) {
			case core::working_edit::modified:  return QStringLiteral("Changed, not committed.");
			case core::working_edit::deleted:   return QStringLiteral("Deleted, not committed.");
			case core::working_edit::untracked: return QStringLiteral("Not tracked by Git.");
			}
			return { };
		}

		auto qualification_text(const core::candidate_qualification aQualification) -> QString
		{
			switch (aQualification) {
			case core::candidate_qualification::includes_working_files:  return QStringLiteral("Includes uncommitted files.");
			case core::candidate_qualification::sparse_checkout:         return QStringLiteral("Sparse checkout: some paths were not compared.");
			case core::candidate_qualification::working_scan_incomplete: return QStringLiteral("The working tree was not fully scanned.");
			case core::candidate_qualification::line_ending_conversion:  return QStringLiteral("core.autocrlf is set: line endings may differ.");
			}
			return { };
		}

		auto refusal_text(const core::remote_chain_refused& aRefused) -> QString
		{
			const auto where = text_of(aRefused.Location);
			switch (aRefused.Reason) {
			case core::remote_chain_refusal::cycle:                 return QStringLiteral("The local remotes lead back to %1; no hosted repository is reached.").arg(where);
			case core::remote_chain_refusal::unreadable:            return QStringLiteral("The clone at %1 could not be read.").arg(where);
			case core::remote_chain_refusal::too_many_hops:         return QStringLiteral("More than four local clones lead from %1 without reaching a hosted repository.").arg(where);
			case core::remote_chain_refusal::no_hosted_remote:      return QStringLiteral("No remote of %1 names a hosted repository.").arg(where);
			case core::remote_chain_refusal::several_local_remotes: return QStringLiteral("%1 names more than one local clone; which to follow is not guessed.").arg(where);
			}
			return { };
		}

		auto target_text(const application::local_draft_workbench_projection& aView) -> QString
		{
			const auto* target = aView.Draft ? &aView.Draft->Target : aView.Target ? &*aView.Target
																				   : nullptr;
			if (target == nullptr) {
				return { };
			}
			return QStringLiteral("%1 %2").arg(repository_text(target->Target.Repository), text_of(target->Target.Branch));
		}
	}

	local_draft_parts::local_draft_parts(QWidget* const aOwner)
		: QObject{ aOwner }
		, Owner{ aOwner }
	{
		build();
		present({ });
	}

	local_draft_parts::~local_draft_parts() = default;

	void local_draft_parts::build()
	{
		// The review bar's leading controls: the candidate, then the branch.
		BarLeading = new QWidget{ Owner };
		BarLeading->setObjectName(QStringLiteral("localDraftBarLeading"));
		auto* const leading = new QHBoxLayout{ BarLeading };
		leading->setContentsMargins(0, 0, 0, 0);
		leading->setSpacing(6);
		Candidates = new QToolButton{ BarLeading };
		Candidates->setObjectName(QStringLiteral("localDraftCandidates"));
		Candidates->setAccessibleName(QStringLiteral("Candidate shown"));
		Candidates->setToolTip(QStringLiteral("The captured versions of this draft."));
		Candidates->setPopupMode(QToolButton::InstantPopup);
		Candidates->setToolButtonStyle(Qt::ToolButtonTextOnly);
		CandidateMenu = new QMenu{ Candidates };
		Candidates->setMenu(CandidateMenu);
		Candidates->setMinimumWidth(Candidates->fontMetrics().horizontalAdvance(QStringLiteral("Candidate %1 of %1").arg(core::maximum_local_draft_candidates)) + 28);
		leading->addWidget(Candidates);
		Branch = new QToolButton{ BarLeading };
		Branch->setObjectName(QStringLiteral("localDraftBranch"));
		Branch->setAccessibleName(QStringLiteral("Branch"));
		Branch->setAutoRaise(true);
		Branch->setPopupMode(QToolButton::InstantPopup);
		Branch->setToolButtonStyle(Qt::ToolButtonTextOnly);
		Branch->setMenu(new QMenu{ Branch });
		leading->addWidget(Branch);
		Identity = new QLabel{ BarLeading };
		Identity->setObjectName(QStringLiteral("localDraftIdentity"));
		Identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Identity->setFocusPolicy(Qt::TabFocus);
		leading->addWidget(Identity);
		// Refresh, Discard… and Close are the candidate menu's actions'
		// targets, held apart from the bar so they act whatever the bar shows.
		auto* const targets = new QWidget{ Owner };
		targets->setObjectName(QStringLiteral("localDraftActionTargets"));
		targets->hide();
		Refresh = new QPushButton{ QStringLiteral("Refresh"), targets };
		Refresh->setObjectName(QStringLiteral("localDraftRefresh"));
		Refresh->hide();
		Discard = new QPushButton{ QStringLiteral("Discard…"), targets };
		Discard->setObjectName(QStringLiteral("localDraftDiscard"));
		Discard->hide();
		Close = new QPushButton{ QStringLiteral("Close"), targets };
		Close->setObjectName(QStringLiteral("localDraftClose"));
		Close->hide();

		// The review bar's trailing controls, where Compose review sits in a review.
		BarTrailing = new QWidget{ Owner };
		BarTrailing->setObjectName(QStringLiteral("localDraftBarTrailing"));
		auto* const trailing = new QHBoxLayout{ BarTrailing };
		trailing->setContentsMargins(0, 0, 0, 0);
		trailing->setSpacing(8);
		PublishState = new QLabel{ BarTrailing };
		PublishState->setObjectName(QStringLiteral("localDraftPublishState"));
		trailing->addWidget(PublishState);
		Create = new QPushButton{ QStringLiteral("Create Draft PR…"), BarTrailing };
		Create->setObjectName(QStringLiteral("localDraftCreate"));
		Create->setToolTip(QStringLiteral("Push and create the draft pull request."));
		trailing->addWidget(Create);

		// A recovery or a choice, shown only when there is one.
		NoticeRow = new QWidget{ Owner };
		NoticeRow->setObjectName(QStringLiteral("localDraftNoticeRow"));
		auto* const noticeColumn = new QVBoxLayout{ NoticeRow };
		noticeColumn->setContentsMargins(12, 6, 12, 6);
		noticeColumn->setSpacing(4);
		Notice = new QLabel{ NoticeRow };
		Notice->setObjectName(QStringLiteral("localDraftNotice"));
		Notice->setWordWrap(true);
		Notice->setTextInteractionFlags(Qt::TextSelectableByMouse);
		noticeColumn->addWidget(Notice);
		TargetRow                = new QWidget{ NoticeRow };
		auto* const targetLayout = new QHBoxLayout{ TargetRow };
		targetLayout->setContentsMargins(0, 0, 0, 0);
		TargetRepository = new QComboBox{ TargetRow };
		TargetRepository->setObjectName(QStringLiteral("localDraftTargetRepository"));
		TargetRepository->setAccessibleName(QStringLiteral("Target repository"));
		TargetBranch = new QComboBox{ TargetRow };
		TargetBranch->setObjectName(QStringLiteral("localDraftTargetBranch"));
		TargetBranch->setAccessibleName(QStringLiteral("Target branch"));
		TargetBranch->setEditable(true);
		UseTarget = new QPushButton{ QStringLiteral("Use This Target"), TargetRow };
		UseTarget->setObjectName(QStringLiteral("localDraftUseTarget"));
		targetLayout->addWidget(TargetRepository);
		targetLayout->addWidget(TargetBranch);
		targetLayout->addWidget(UseTarget);
		targetLayout->addStretch(1);
		noticeColumn->addWidget(TargetRow);
		Rebind = new QPushButton{ QStringLiteral("Rebind to This Folder"), NoticeRow };
		Rebind->setObjectName(QStringLiteral("localDraftRebind"));
		Rebind->setToolTip(QStringLiteral("Continue this draft from the folder it moved to."));
		noticeColumn->addWidget(Rebind, 0, Qt::AlignLeft);
		NoticeRow->hide();

		// The Files tab's footer: working files a candidate can include. Most
		// authors never need them, so they wait behind a disclosure.
		FilesFooter = new QWidget{ Owner };
		FilesFooter->setObjectName(QStringLiteral("localDraftFilesFooter"));
		auto* const footer = new QVBoxLayout{ FilesFooter };
		footer->setContentsMargins(8, 4, 8, 6);
		footer->setSpacing(2);
		WorkingDisclosure = new disclosure_button{ FilesFooter };
		WorkingDisclosure->setObjectName(QStringLiteral("localDraftWorkingDisclosure"));
		WorkingDisclosure->setCheckable(true);
		WorkingDisclosure->setChecked(false);
		footer->addWidget(WorkingDisclosure);
		Working = new QListWidget{ FilesFooter };
		Working->setObjectName(QStringLiteral("localDraftWorking"));
		Working->setAccessibleName(QStringLiteral("Working files"));
		Working->setFrameShape(QFrame::NoFrame);
		Working->setMaximumHeight(140);
		Working->hide();
		footer->addWidget(Working);
		connect(WorkingDisclosure, &QAbstractButton::toggled, Working, &QWidget::setVisible);

		LocalChanges = new local_changes_panel{ Owner };
		LocalChanges->hide();

		// The Edit view's page: the file's name, Save, and the editor.
		EditPage = new QWidget{ Owner };
		EditPage->setObjectName(QStringLiteral("localDraftEditPage"));
		auto* const edit = new QVBoxLayout{ EditPage };
		edit->setContentsMargins(0, 0, 0, 0);
		edit->setSpacing(0);
		auto* const editHead = new QWidget{ EditPage };
		editHead->setFixedHeight(32);
		auto* const headRow = new QHBoxLayout{ editHead };
		headRow->setContentsMargins(12, 2, 10, 2);
		EditName = new QLabel{ editHead };
		EditName->setObjectName(QStringLiteral("localDraftEditName"));
		headRow->addWidget(EditName);
		headRow->addStretch(1);
		EditState = new QLabel{ editHead };
		EditState->setObjectName(QStringLiteral("localDraftEditState"));
		headRow->addWidget(EditState);
		Save = new QPushButton{ editHead };
		Save->setObjectName(QStringLiteral("localDraftSave"));
		// [[BPR-2156]]. The owner, on the Edit view: "save should use the
		// standard save icon", then (look of 2026-11-14): "Drop the word Save
		// just have the icon … isn't it just QIcon::ThemeIcon::DocumentSave".
		// The icon alone, `DocumentSave` (the style's save icon where the
		// platform has none); Reload takes the platform's refresh. Its name
		// and tooltip carry the word.
		Save->setIcon(save_icon(*Save, false));
		Save->setAccessibleName(QStringLiteral("Save"));
		Save->setToolTip(QStringLiteral("Save: write the file to your worktree."));
		Save->setFixedWidth(Save->iconSize().width() + 24);
		headRow->addWidget(Save);
		edit->addWidget(editHead);
		Editor = new suggestion_editor{ EditPage };
		Editor->setObjectName(QStringLiteral("localDraftEditor"));
		Editor->setAccessibleName(QStringLiteral("File editor"));
		edit->addWidget(Editor, 1);
		EditPage->hide();

		// The Notes tab's page. RFC-BPR-0127 ([[BPR-2161]]): its notes are
		// called Local comments; their record and storage are unchanged.
		NotesPage = new QWidget{ Owner };
		NotesPage->setObjectName(QStringLiteral("localDraftNotesPage"));
		auto* const noteColumn = new QVBoxLayout{ NotesPage };
		noteColumn->setContentsMargins(10, 8, 10, 8);
		Notes = new QListWidget{ NotesPage };
		Notes->setObjectName(QStringLiteral("localDraftNotes"));
		Notes->setAccessibleName(QStringLiteral("Local comments"));
		Notes->setWordWrap(true);
		Notes->setFrameShape(QFrame::NoFrame);
		Notes->setContextMenuPolicy(Qt::CustomContextMenu);
		noteColumn->addWidget(Notes, 1);
		NoteEditor = new QLineEdit{ NotesPage };
		NoteEditor->setObjectName(QStringLiteral("localDraftNoteEditor"));
		NoteEditor->setAccessibleName(QStringLiteral("New local comment"));
		NoteEditor->setPlaceholderText(QStringLiteral("Add a local comment"));
		NoteEditor->setToolTip(QStringLiteral("Return saves it on the open file."));
		noteColumn->addWidget(NoteEditor);
		NotesPage->hide();

		// [[BPR-2196]]. Author's Workflow tab: the author's checklist, then
		// the publish steps (the accepted design moved them out of Status).
		auto* const status = new QScrollArea{ Owner };
		status->setObjectName(QStringLiteral("localDraftWorkflowPage"));
		status->setFrameShape(QFrame::NoFrame);
		status->setWidgetResizable(true);
		status->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		// RFC-BPR-0112: the draft's optional workflow above the publish steps.
		auto* const statusColumn = new QWidget{ status };
		auto* const statusLayout = new QVBoxLayout{ statusColumn };
		statusLayout->setContentsMargins(0, 0, 0, 0);
		WorkflowArea    = new local_draft_workflow_area{ statusColumn };
		PublicationArea = new local_draft_publication_area{ statusColumn };
		statusLayout->addWidget(WorkflowArea);
		statusLayout->addWidget(PublicationArea);
		statusLayout->addStretch(1);
		status->setWidget(statusColumn);
		status->hide();
		StatusPage = status;

		Links.Selection = [] { return std::optional<local_draft_lines>{ }; };
		Links.Reveal    = [this](const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast) { reveal(aPath, aFirst, aLast); };

		connect(Refresh, &QPushButton::clicked, this, [this] {
			if (Dispatch.Refresh) {
				Dispatch.Refresh();
			}
		});
		connect(Discard, &QPushButton::clicked, this, [this] { ask_discard(); });
		connect(Close, &QPushButton::clicked, this, [this] {
			if (Dispatch.Close) {
				Dispatch.Close();
			}
		});
		connect(Create, &QPushButton::clicked, this, [this] {
			if (AuthorMode.Handoff == core::author_handoff::create_draft_pull_request || AuthorMode.Handoff == core::author_handoff::hand_off_plan) {
				if (Dispatch.CreatePullRequest) {
					Dispatch.CreatePullRequest();
				}
			} else if (HandedOff) {
				HandedOff(AuthorMode.Handoff);
			}
		});
		connect(Working, &QListWidget::itemChanged, this, [this](QListWidgetItem* const aItem) {
			if (!Presenting && aItem != nullptr && Dispatch.Include) {
				Dispatch.Include(utf8(aItem->data(path_role).toString()), aItem->checkState() == Qt::Checked);
			}
		});
		connect(NoteEditor, &QLineEdit::returnPressed, this, [this] {
			const auto text = NoteEditor->text().trimmed();
			if (text.isEmpty() || !Dispatch.AddNote) {
				return;
			}
			Dispatch.AddNote(utf8(text), Projection.File ? Projection.File->Path : std::string{ });
			NoteEditor->clear();
		});
		connect(Notes, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) {
			auto* const item = Notes->itemAt(aAt);
			if (item == nullptr) {
				return;
			}
			auto* const menu   = new QMenu{ Notes };
			auto* const remove = menu->addAction(QStringLiteral("Delete Local Comment"));
			menu->setAttribute(Qt::WA_DeleteOnClose);
			connect(remove, &QAction::triggered, this, [this, note = item->data(note_role).toULongLong()] {
				if (Dispatch.RemoveNote) {
					Dispatch.RemoveNote(note);
				}
			});
			menu->popup(Notes->viewport()->mapToGlobal(aAt));
		});
		connect(TargetRepository, &QComboBox::currentIndexChanged, this, [this](const int aIndex) {
			if (Presenting || aIndex < 0 || !Projection.Selection) {
				return;
			}
			const auto& choice  = Projection.Selection->Choices[static_cast<std::size_t>(aIndex)];
			const auto  blocker = QSignalBlocker{ TargetBranch };
			TargetBranch->clear();
			for (const auto& branch : choice.Branches) {
				TargetBranch->addItem(text_of(branch));
			}
			if (choice.DefaultBranch) {
				TargetBranch->setCurrentText(text_of(*choice.DefaultBranch));
			}
		});
		connect(UseTarget, &QPushButton::clicked, this, [this] {
			const auto index = TargetRepository->currentIndex();
			if (!Projection.Selection || index < 0 || !Dispatch.ChooseTarget || TargetBranch->currentText().trimmed().isEmpty()) {
				return;
			}
			const auto& choice = Projection.Selection->Choices[static_cast<std::size_t>(index)];
			Dispatch.ChooseTarget(core::local_draft_target{ .Remote = choice.Destination.Remote, .Repository = choice.Destination.Repository, .Branch = utf8(TargetBranch->currentText().trimmed()) });
		});
		connect(Rebind, &QPushButton::clicked, this, [this] {
			if (Dispatch.Rebind) {
				Dispatch.Rebind();
			}
		});
		connect(Save, &QPushButton::clicked, this, [this] {
			if (!LocalProjection.ChangedOnDisk.empty() && LocalProjection.ChangedOnDisk == EditPath) {
				// Reload: the disk's text replaces the editor's; nothing is written.
				EditRead.clear();
				EditDirty = false;
				LocalProjection.ChangedOnDisk.clear();
				present_edit();
				changed();
				return;
			}
			const auto text = Editor->text();
			if (Dispatch.Save && !EditPath.empty()) {
				Dispatch.Save(EditPath, EditRead, std::vector<std::uint8_t>(text.begin(), text.end()));
			}
		});
	}

	void local_draft_parts::set_dispatch(local_draft_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void local_draft_parts::set_comparison_links(std::function<std::optional<local_draft_lines>()> aSelection, std::function<void(const std::string&, std::uint32_t, std::uint32_t)> aReveal)
	{
		Links.Selection = [this, selection = std::move(aSelection)]() -> std::optional<local_draft_lines> {
			// A stop is on the latest candidate's lines.
			const auto* latest = Projection.Draft ? core::latest_candidate(*Projection.Draft) : nullptr;
			if (latest == nullptr || Projection.Shown != latest->Ordinal || !selection) {
				return std::nullopt;
			}
			return selection();
		};
		RevealInComparison = std::move(aReveal);
	}

	void local_draft_parts::reveal(const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, const std::uint64_t aCandidate)
	{
		const auto* latest = Projection.Draft ? core::latest_candidate(*Projection.Draft) : nullptr;
		if (latest == nullptr) {
			return;
		}
		const auto candidate = aCandidate == 0U ? latest->Ordinal : aCandidate;
		if (Projection.Shown != candidate && Dispatch.ShowCandidate) {
			Dispatch.ShowCandidate(candidate);
		}
		if ((!Projection.File || Projection.File->Path != aPath) && Dispatch.OpenFile) {
			Dispatch.OpenFile(aPath);
		}
		if (RevealInComparison) {
			RevealInComparison(aPath, aFirst, aLast);
		}
	}

	auto local_draft_parts::selected_lines() const -> std::optional<local_draft_lines>
	{
		return Links.Selection ? Links.Selection() : std::nullopt;
	}

	void local_draft_parts::open_file(const std::string& aPath, const bool aEditing)
	{
		if (Presenting) {
			return;
		}
		if (Dispatch.OpenFile && (!Projection.File || Projection.File->Path != aPath)) {
			Dispatch.OpenFile(aPath);
		}
		// Owner, 2026-11-13: the Edit view follows the file chosen anywhere.
		if (aEditing && LocalProjection.Active && Dispatch.OpenLocalFile && (!LocalProjection.File || LocalProjection.File->Path != aPath)) {
			Dispatch.OpenLocalFile(aPath);
		}
	}

	void local_draft_parts::set_code_font(const QFont& aFont)
	{
		Editor->set_code_font(aFont);
	}

	void local_draft_parts::adopt_edit_syntax(const std::string& aDocument, application::shared_token_spans aSpans)
	{
		if (!aSpans || EditDirty || edit_document() != aDocument) {
			return;
		}
		const auto classified = classify_text_lines(std::span{ EditRead }, std::move(aSpans));
		Editor->set_syntax(suggestion_syntax_lines(classified, aDocument, SyntaxColors));
	}

	void local_draft_parts::adopt_appearance(const appearance_profile& aProfile)
	{
		SyntaxColors = code_role_colors_of(aProfile.palette());
		Editor->set_contrast_variant(aProfile.variant());
		Editor->set_code_font(aProfile.code_font());
	}

	void local_draft_parts::set_discard_question(std::function<bool()> aQuestion)
	{
		DiscardQuestion = std::move(aQuestion);
	}

	void local_draft_parts::set_rename_question(std::function<std::optional<QString>(const QString&)> aQuestion)
	{
		RenameQuestion = std::move(aQuestion);
	}

	void local_draft_parts::ask_rename()
	{
		if (!Projection.Draft || !Dispatch.Rename) {
			return;
		}
		const auto current = text_of(Projection.Draft->Title);
		auto       chosen  = std::optional<QString>{ };
		if (RenameQuestion) {
			chosen = RenameQuestion(current);
		} else {
			auto accepted = false;
			auto text     = QInputDialog::getText(Owner, QStringLiteral("Rename Local Draft"), QStringLiteral("Pull request title:"), QLineEdit::Normal, current, &accepted);
			if (accepted) {
				chosen = std::move(text);
			}
		}
		if (chosen && !chosen->trimmed().isEmpty() && chosen->trimmed() != current) {
			Dispatch.Rename(utf8(chosen->trimmed()));
		}
	}

	void local_draft_parts::ask_discard()
	{
		if (!Projection.Draft || !Dispatch.Discard) {
			return;
		}
		auto discard = false;
		if (DiscardQuestion) {
			discard = DiscardQuestion();
		} else {
			auto box = QMessageBox{ QMessageBox::Warning, QStringLiteral("Discard Local Draft"),
									QStringLiteral("Discard “%1”?").arg(text_of(Projection.Draft->Title)), QMessageBox::NoButton, Owner };
			box.setInformativeText(QStringLiteral("Its local comments and candidates are removed from this computer. The worktree, its branches and any pull request are not changed."));
			auto* const confirm = box.addButton(QStringLiteral("Discard"), QMessageBox::DestructiveRole);
			box.addButton(QMessageBox::Cancel);
			box.setDefaultButton(QMessageBox::Cancel);
			box.exec();
			discard = box.clickedButton() == confirm;
		}
		if (discard) {
			Dispatch.Discard();
		}
	}

	void local_draft_parts::present(const application::local_draft_workbench_projection& aProjection)
	{
		Projection = aProjection;
		Presenting = true;
		if (auto id = aProjection.Draft ? aProjection.Draft->Id.Value : std::string{ }; id != PresentedDraft) {
			PresentedDraft = std::move(id);
			WorkingDisclosure->setChecked(false);
		}
		PublicationArea->present(aProjection);
		WorkflowArea->present(aProjection);
		present_header();
		present_recovery();
		present_working();
		present_notes();
		present_bar();
		Presenting = false;
		changed();
	}

	void local_draft_parts::present_local(const application::local_changes_projection& aProjection)
	{
		LocalProjection = aProjection;
		LocalChanges->present(aProjection);
		if (!aProjection.Saved.empty() && aProjection.Saved == EditPath) {
			// The disk now holds what was saved: that is what the next Save checks against.
			const auto text = Editor->text();
			EditRead.assign(text.begin(), text.end());
			EditDirty = false;
		}
		present_bar();
		present_edit();
		changed();
	}

	void local_draft_parts::changed()
	{
		if (Changed) {
			Changed();
		}
	}

	auto local_draft_parts::title_text() const -> QString
	{
		if (Projection.Draft) {
			return text_of(Projection.Draft->Title);
		}
		return Projection.Branch.empty() ? QStringLiteral("Prepare pull request") : text_of(Projection.Branch);
	}

	auto local_draft_parts::window_title() const -> QString
	{
		const auto* target = Projection.Draft ? &Projection.Draft->Target : Projection.Target ? &*Projection.Target
																							  : nullptr;
		auto        title  = title_text();
		if (target == nullptr) {
			return title;
		}
		if (Projection.Draft && Projection.Draft->Association) {
			return QStringLiteral("%1  #%2 %3").arg(repository_text(target->Target.Repository)).arg(Projection.Draft->Association->Number).arg(title);
		}
		return QStringLiteral("%1  %2").arg(repository_text(target->Target.Repository), title);
	}

	void local_draft_parts::present_header()
	{
		const auto& view   = Projection;
		auto        source = text_of(view.Draft ? view.Draft->Branch : view.Branch);
		auto        target = target_text(view);
		Identity->setText(target.isEmpty() ? source : QStringLiteral("%1 into %2").arg(source, target));
		Identity->setToolTip(QStringLiteral("Worktree: %1").arg(text_of(view.Draft ? view.Draft->Worktree.Location : view.Location)));
		Identity->setAccessibleName(Identity->text());

		const core::local_draft_candidate* shown = nullptr;
		if (view.Draft) {
			const auto found = std::ranges::find(view.Draft->Candidates, view.Shown, &core::local_draft_candidate::Ordinal);
			shown            = found == view.Draft->Candidates.end() ? nullptr : &*found;
		}
		CandidateMenu->clear();
		if (view.Draft) {
			for (const auto& candidate : view.Draft->Candidates) {
				auto* const action = CandidateMenu->addAction(QStringLiteral("Candidate %1 of %2").arg(candidate.Ordinal).arg(view.Draft->Candidates.size()));
				action->setCheckable(true);
				action->setChecked(candidate.Ordinal == view.Shown);
				action->setData(QVariant::fromValue<qulonglong>(candidate.Ordinal));
				connect(action, &QAction::triggered, this, [this, ordinal = candidate.Ordinal] {
					if (Dispatch.ShowCandidate) {
						Dispatch.ShowCandidate(ordinal);
					}
				});
				if (candidate.Ordinal == view.Shown) {
					Candidates->setText(action->text());
				}
			}
			CandidateMenu->addSeparator();
			auto* const refresh = CandidateMenu->addAction(QStringLiteral("Refresh"), Refresh, &QPushButton::click);
			refresh->setObjectName(QStringLiteral("localDraftRefreshAction"));
			refresh->setEnabled(view.RefreshOffered && !view.Busy);
			refresh->setToolTip(QStringLiteral("Capture the worktree as a new candidate."));
			// The title is the pull request's, and Overview shows it; it is renamed here.
			auto* const rename = CandidateMenu->addAction(QStringLiteral("Rename…"), this, [this] { ask_rename(); });
			rename->setObjectName(QStringLiteral("localDraftRenameAction"));
			rename->setToolTip(QStringLiteral("Change the pull request's title."));
			auto* const discard = CandidateMenu->addAction(QStringLiteral("Discard…"), Discard, &QPushButton::click);
			discard->setObjectName(QStringLiteral("localDraftDiscardAction"));
		} else {
			Candidates->setText(QStringLiteral("No candidate"));
		}
		// The candidate's exact identity, one short line, for whoever asks.
		Candidates->setToolTip(shown != nullptr ? QStringLiteral("Head %1, from merge base %2.").arg(short_commit(shown->Head).left(7), short_commit(shown->Base).left(7))
												: QStringLiteral("The captured versions of this draft."));
		if (shown != nullptr && !shown->Qualifications.empty()) {
			Candidates->setToolTip(Candidates->toolTip() + QLatin1Char{ ' ' } + qualification_text(shown->Qualifications.front()));
		}
		Candidates->setEnabled(view.Draft.has_value());
		Refresh->setEnabled(view.Draft.has_value() && view.RefreshOffered && !view.Busy);
		Discard->setEnabled(view.Draft.has_value());
	}

	void local_draft_parts::present_recovery()
	{
		const auto& view  = Projection;
		auto        lines = QStringList{ };
		const auto  where = text_of(view.Location);
		switch (view.Recovery) {
		case application::local_draft_recovery::none: break;
		case application::local_draft_recovery::not_git:
			lines << QStringLiteral("%1 is not a Git worktree; a local PR draft needs one.").arg(where);
			break;
		case application::local_draft_recovery::unreadable: lines << QStringLiteral("%1 could not be read.").arg(where); break;
		case application::local_draft_recovery::repository: lines << QStringLiteral("This repository uses a format BetterPullReview does not read."); break;
		case application::local_draft_recovery::detached_head:
			lines << QStringLiteral("HEAD is detached. Check out the branch to prepare, then open the folder again.");
			break;
		case application::local_draft_recovery::unborn_branch: lines << QStringLiteral("%1 has no commits yet.").arg(text_of(view.Branch)); break;
		case application::local_draft_recovery::remote_chain:
			if (view.ChainRefusal) {
				lines << refusal_text(*view.ChainRefusal);
			}
			break;
		case application::local_draft_recovery::target_unavailable:
			lines << QStringLiteral("%1 is not in this clone. Fetch it with your own Git, then open the folder again.").arg(target_text(view));
			break;
		case application::local_draft_recovery::no_common_base:
			lines << QStringLiteral("%1 and %2 share no history.").arg(text_of(view.Branch), target_text(view));
			break;
		case application::local_draft_recovery::several_bases:
			lines << QStringLiteral("%1 and %2 have %3 merge bases. The comparison is not chosen for you.")
						 .arg(text_of(view.Branch), target_text(view))
						 .arg(view.Comparison.Bases.size());
			break;
		case application::local_draft_recovery::git:
			lines << QStringLiteral("Git could not read this repository%1").arg(view.Git && !view.Git->Detail.empty() ? QStringLiteral(": %1").arg(text_of(core::plain_terminal_text(view.Git->Detail))) : QStringLiteral("."));
			break;
		case application::local_draft_recovery::store: break;
		}
		if (view.Store) {
			if (view.Store->Failure == application::local_draft_store_failure::full) {
				lines << QStringLiteral("Local drafts are full (%1 of %2). Discard one to make room; nothing was removed.").arg(view.Store->Used).arg(view.Store->Limit);
			} else if (view.Store->Failure != application::local_draft_store_failure::stale) {
				lines << QStringLiteral("Local drafts could not be saved on this computer.");
			}
		}
		if (view.Reread) {
			lines << QStringLiteral("This draft was changed elsewhere and was read again.");
		}
		if (view.Capture && view.Capture->Failure == application::local_draft_capture_failure::changed) {
			lines << QStringLiteral("%1 changed while it was read. Refresh again.").arg(view.Capture->Path.empty() ? QStringLiteral("A file") : text_of(view.Capture->Path));
		}
		if (view.Outcome == application::local_draft_open_outcome::moved && view.Draft) {
			lines << QStringLiteral("This draft was prepared at %1.").arg(text_of(view.Draft->Worktree.Location));
		} else if (view.Draft && view.WorktreeMissing) {
			lines << QStringLiteral("The worktree is not at %1. Its candidates and local comments are here.").arg(text_of(view.Draft->Worktree.Location));
		} else if (view.Draft && view.Match == core::worktree_match::branch_switched) {
			lines << QStringLiteral("The worktree is on %1 now; this draft stays with %2.").arg(text_of(view.Branch), text_of(view.Draft->Branch));
		}
		if (view.Selection) {
			switch (view.Selection->Question) {
			case core::target_question::several_repositories:      lines << QStringLiteral("Choose where this pull request would go."); break;
			case core::target_question::no_default_branch:         lines << QStringLiteral("Choose the branch it would merge into."); break;
			case core::target_question::chosen_repository_missing: lines << QStringLiteral("The chosen repository is no longer a remote here. Choose again."); break;
			}
		}
		if (view.Stage == application::local_draft_stage::opening) {
			lines << QStringLiteral("Reading %1…").arg(where);
		}
		Notice->setText(lines.join(QLatin1Char{ '\n' }));
		Notice->setVisible(!lines.isEmpty());

		const auto choosing = view.Selection.has_value() && !view.Selection->Choices.empty();
		TargetRow->setVisible(choosing);
		if (choosing) {
			const auto blocker = QSignalBlocker{ TargetRepository };
			TargetRepository->clear();
			for (const auto& choice : view.Selection->Choices) {
				TargetRepository->addItem(QStringLiteral("%1 (%2)").arg(repository_text(choice.Destination.Repository), text_of(choice.Destination.Remote)));
			}
			TargetRepository->setCurrentIndex(-1);
			Presenting = false;
			TargetRepository->setCurrentIndex(0);
			Presenting = true;
		}
		const auto rebind = view.Outcome == application::local_draft_open_outcome::moved && view.Draft.has_value();
		Rebind->setVisible(rebind);
		NoticeRow->setVisible(!lines.isEmpty() || choosing || rebind);
	}

	void local_draft_parts::present_working()
	{
		const auto& view = Projection;
		Working->clear();
		const auto blocker  = QSignalBlocker{ Working };
		auto       included = 0;
		for (const auto& file : view.Working) {
			auto* const item = new QListWidgetItem{ text_of(file.Path), Working };
			item->setData(path_role, text_of(file.Path));
			item->setToolTip(edit_word(file));
			const auto capturable = file.Qualification == core::working_qualification::none && view.Draft.has_value();
			item->setFlags(capturable ? Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable : Qt::ItemIsSelectable);
			const auto in = std::ranges::find(view.Included, file.Path) != view.Included.end();
			included += in ? 1 : 0;
			item->setCheckState(in ? Qt::Checked : Qt::Unchecked);
		}
		auto label = view.Working.empty() ? QStringLiteral("No uncommitted files") : QStringLiteral("Uncommitted files (%1 of %2 included)").arg(included).arg(view.Working.size());
		if (view.ScanIncomplete) {
			label += QStringLiteral(", partly scanned");
		}
		WorkingDisclosure->setText(label);
		WorkingDisclosure->setEnabled(!view.Working.empty());
		WorkingDisclosure->setToolTip(QStringLiteral("Tick a file to include it in the next candidate."));
		FilesFooter->setVisible(view.Draft.has_value());
	}

	void local_draft_parts::present_notes()
	{
		const auto& view = Projection;
		Notes->clear();
		NoteEditor->setEnabled(view.Draft.has_value());
		if (!view.Draft) {
			return;
		}
		for (const auto& note : view.Draft->Notes) {
			auto text = text_of(note.Text);
			if (!note.Path.empty()) {
				text = QStringLiteral("%1: %2").arg(text_of(note.Path), text);
			}
			auto* const item = new QListWidgetItem{ conversation_icon(conversation_glyph::computer, Notes->palette()), text, Notes };
			item->setData(note_role, QVariant::fromValue<qulonglong>(note.Id));
			item->setToolTip(note.Origin == core::note_origin::harness ? QStringLiteral("From the harness. Only on this computer.") : QStringLiteral("Your local comment. Only on this computer."));
			item->setData(Qt::AccessibleDescriptionRole, QStringLiteral("Local comment, only on this computer"));
		}
	}

	void local_draft_parts::present_bar()
	{
		const auto& local  = LocalProjection;
		const auto  author = local.Active;
		// Author state: the branch with ahead and behind says the window is on
		// your files; otherwise the branch and where it goes.
		Branch->setVisible(author);
		Identity->setVisible(!author && Projection.Draft.has_value());
		if (author) {
			auto branch = QStringLiteral("⎇ %1").arg(text_of(local.Subject.Branch));
			if (local.Hosted.Observed && local.Hosted.Ahead > 0U) {
				branch += QStringLiteral("  ↑%1").arg(local.Hosted.Ahead);
			}
			if (local.Hosted.Observed && local.Hosted.Behind > 0U) {
				branch += QStringLiteral("  ↓%1").arg(local.Hosted.Behind);
			}
			Branch->setText(branch);
			Branch->setToolTip(local.Hosted.Observed ? QStringLiteral("↑ commits to push, ↓ commits on GitHub you lack.") : QStringLiteral("Your worktree's branch."));
			auto* const menu = Branch->menu();
			menu->clear();
			auto* const where = menu->addAction(text_of(local.Subject.Worktree));
			where->setEnabled(false);
			menu->addSeparator();
			auto* const push = menu->addAction(QStringLiteral("Push"), this, [this] { LocalChanges->push_button()->click(); });
			push->setEnabled(local.Push.Offered);
		}
		// Publishing: one word of where it stands, and Create Draft PR… where Compose review sits.
		const auto& publishing = Projection.Publishing;
		auto        state      = QString{ };
		auto        ink        = Owner->palette().color(QPalette::WindowText);
		if (!publishing.Linked.empty()) {
			state = QStringLiteral("● On GitHub");
		} else if ((author && local.Push.State == core::local_push_state::missing) || (publishing.Checked && publishing.Push)) {
			state = QStringLiteral("● Push needed");
			ink   = Owner->palette().color(QPalette::Window).lightness() < 128 ? QColor{ 210, 153, 34 } : QColor{ 154, 103, 0 };
		}
		auto statePalette = PublishState->palette();
		statePalette.setColor(QPalette::WindowText, ink);
		PublishState->setPalette(statePalette);
		PublishState->setText(state);
		present_handoff();
	}

	void local_draft_parts::present_author_mode(const local_draft_author_mode& aAuthorMode)
	{
		AuthorMode = aAuthorMode;
		WorkflowArea->present_checklist(aAuthorMode.Checklist, aAuthorMode.Case);
		present_handoff();
	}

	void local_draft_parts::present_handoff()
	{
		// [[BPR-2196]]. The one handoff at the right end of Author's bar, as
		// the core names it for the case.
		const auto associated = Projection.Draft && Projection.Draft->Association;
		switch (AuthorMode.Handoff) {
		case core::author_handoff::create_draft_pull_request:
			Create->setText(QStringLiteral("Create Draft PR…"));
			Create->setToolTip(QStringLiteral("Push and create the draft pull request."));
			Create->setEnabled(Projection.Draft.has_value() && Projection.Publishing.Linked.empty() && !associated);
			break;
		case core::author_handoff::hand_off_plan:
			Create->setText(QStringLiteral("Hand Off Plan…"));
			Create->setToolTip(QStringLiteral("Hand the plan to the workflow's publisher."));
			Create->setEnabled(Projection.Draft.has_value() && Projection.Publishing.Linked.empty() && !associated);
			break;
		case core::author_handoff::ready_for_review:
			Create->setText(QStringLiteral("Ready for review"));
			Create->setToolTip(QStringLiteral("Mark the draft ready for review on GitHub."));
			Create->setEnabled(AuthorMode.HandoffEnabled);
			break;
		case core::author_handoff::publish_update:
			Create->setText(QStringLiteral("Publish update…"));
			Create->setToolTip(AuthorMode.HandoffEnabled ? QStringLiteral("Push this update to the pull request.") : QStringLiteral("Nothing to push from your worktree."));
			Create->setEnabled(AuthorMode.HandoffEnabled);
			break;
		}
		Create->show();
		// The headline's words beside it: what is left.
		if (AuthorMode.Checklist.RequiredUnmet > 0U) {
			auto statePalette = PublishState->palette();
			statePalette.setColor(QPalette::WindowText, Owner->palette().color(QPalette::Window).lightness() < 128 ? QColor{ 248, 81, 73 } : QColor{ 207, 34, 46 });
			PublishState->setPalette(statePalette);
			PublishState->setText(AuthorMode.Checklist.RequiredUnmet == 1U ? QStringLiteral("● 1 check not met") : QStringLiteral("● %1 checks not met").arg(AuthorMode.Checklist.RequiredUnmet));
		}
	}

	auto local_draft_parts::edit_refusal() const -> QString
	{
		const auto& local = LocalProjection;
		if (!local.Active || !local.File || !local.File->EditRefusal) {
			return { };
		}
		switch (*local.File->EditRefusal) {
		case core::local_edit_refusal::deleted:    return QStringLiteral("the file is deleted");
		case core::local_edit_refusal::not_a_file: return QStringLiteral("it is not a file");
		case core::local_edit_refusal::link:       return QStringLiteral("it is a symbolic link");
		case core::local_edit_refusal::submodule:  return QStringLiteral("it is a submodule");
		case core::local_edit_refusal::outside:    return QStringLiteral("it is outside the worktree");
		case core::local_edit_refusal::too_large:  return QStringLiteral("it is larger than 1 MiB");
		case core::local_edit_refusal::not_utf8:   return QStringLiteral("it is not UTF-8 text");
		case core::local_edit_refusal::nul_byte:   return QStringLiteral("it holds a NUL byte");
		case core::local_edit_refusal::filtered:   return QStringLiteral("a Git filter stores other bytes");
		}
		return { };
	}

	auto local_draft_parts::edit_path() const -> const std::string&
	{
		return EditPath;
	}

	auto local_draft_parts::edit_document() const -> std::string
	{
		return { EditRead.begin(), EditRead.end() };
	}

	void local_draft_parts::present_edit()
	{
		const auto* const file = LocalProjection.Active && LocalProjection.File ? &*LocalProjection.File : nullptr;
		if (file == nullptr || file->EditRefusal) {
			EditName->setText(file == nullptr ? QString{ } : text_of(file->Path));
			EditState->setText(file == nullptr ? QString{ } : QStringLiteral("Not editable: %1").arg(edit_refusal()));
			Save->setEnabled(false);
			if (EditPath != (file == nullptr ? std::string{ } : file->Path) || !EditRead.empty()) {
				EditPath = file == nullptr ? std::string{ } : file->Path;
				EditRead.clear();
				EditDirty = false;
				Editor->set_text({ });
			}
			Editor->set_read_only(true);
			return;
		}
		if (EditPath != file->Path || (!EditDirty && EditRead != file->Disk)) {
			// Another file, or this one changed and nothing is unsaved: read it again.
			EditPath  = file->Path;
			EditRead  = file->Disk;
			EditDirty = false;
			Editor->set_read_only(false);
			Editor->set_text(std::string_view{ reinterpret_cast<const char*>(EditRead.data()), EditRead.size() });
			Editor->Changed = [this] {
				const auto text = Editor->text();
				EditDirty       = std::string_view{ reinterpret_cast<const char*>(EditRead.data()), EditRead.size() } != text;
				present_edit();
			};
		}
		const auto changedOnDisk = LocalProjection.ChangedOnDisk == EditPath && !EditPath.empty();
		EditName->setText(QStringLiteral("%1%2").arg(EditDirty ? QStringLiteral("● ") : QString{ }, text_of(file->Path)));
		EditState->setText(changedOnDisk ? QStringLiteral("Changed on disk") : EditDirty ? QStringLiteral("Not saved")
																						 : QString{ });
		Save->setAccessibleName(changedOnDisk ? QStringLiteral("Reload") : QStringLiteral("Save"));
		Save->setIcon(save_icon(*Save, changedOnDisk));
		Save->setToolTip(changedOnDisk ? QStringLiteral("Reload: read the file again; your edits are dropped.") : QStringLiteral("Save: write the file to your worktree."));
		Save->setEnabled(changedOnDisk || (EditDirty && LocalProjection.Running == application::local_changes_operation::none));
		// The gutter keeps the comparison; a line edited since the last save is dashed.
		const auto marks   = file->Marks;
		const auto read    = QString::fromUtf8(reinterpret_cast<const char*>(EditRead.data()), static_cast<qsizetype>(EditRead.size())).split(QLatin1Char{ '\n' });
		const auto dark    = Owner->palette().color(QPalette::Window).lightness() < 128;
		const auto added   = dark ? QColor{ 63, 185, 80 } : QColor{ 26, 127, 55 };
		const auto changed = dark ? QColor{ 88, 166, 255 } : QColor{ 9, 105, 218 };
		const auto removed = dark ? QColor{ 248, 81, 73 } : QColor{ 207, 34, 46 };
		Editor->set_gutter_marks([this, marks, read, added, changed, removed](const std::uint64_t aLine) -> std::optional<suggestion_editor::gutter_mark> {
			const auto& lines = Editor->model().state().Lines;
			if (EditDirty && aLine < lines.size()) {
				const auto now = QString::fromUtf8(lines[aLine].Content.data(), static_cast<qsizetype>(lines[aLine].Content.size()));
				if (aLine >= static_cast<std::uint64_t>(read.size()) || now != read[static_cast<qsizetype>(aLine)].chopped(read[static_cast<qsizetype>(aLine)].endsWith(QLatin1Char{ '\r' }) ? 1 : 0)) {
					return suggestion_editor::gutter_mark{ .Kind = suggestion_editor::gutter_mark::kind::changed, .Colour = changed, .Dashed = true };
				}
			}
			for (const auto& mark : marks) {
				if (mark.Line == aLine) {
					switch (mark.Kind) {
					case core::local_line_mark_kind::added:          return suggestion_editor::gutter_mark{ .Kind = suggestion_editor::gutter_mark::kind::added, .Colour = added };
					case core::local_line_mark_kind::changed:        return suggestion_editor::gutter_mark{ .Kind = suggestion_editor::gutter_mark::kind::changed, .Colour = changed };
					case core::local_line_mark_kind::removed_before: return suggestion_editor::gutter_mark{ .Kind = suggestion_editor::gutter_mark::kind::removed_before, .Colour = removed };
					}
				}
			}
			return std::nullopt;
		});
	}

	auto local_draft_parts::status_text(const int aFile, const int aFiles) const -> QString
	{
		auto parts = QStringList{ };
		if (Projection.Draft) {
			if (aFiles > 0 && aFile > 0) {
				parts << QStringLiteral("File %1 of %2.").arg(aFile).arg(aFiles);
			}
			const auto found = std::ranges::find(Projection.Draft->Candidates, Projection.Shown, &core::local_draft_candidate::Ordinal);
			if (found != Projection.Draft->Candidates.end()) {
				parts << QStringLiteral("Candidate %1 of %2, captured %3.")
							 .arg(found->Ordinal)
							 .arg(Projection.Draft->Candidates.size())
							 .arg(QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(found->CapturedAt), QLocale::ShortFormat));
			}
		}
		auto text = parts.join(QLatin1Char{ ' ' }).toHtmlEscaped();
		if (LocalProjection.Active && LocalProjection.Observed) {
			const auto uncommitted = core::uncommitted_paths(LocalProjection.Status).size();
			const auto ahead       = LocalProjection.Hosted.Ahead;
			auto       counts      = QStringList{ };
			if (uncommitted > 0U) {
				counts << QStringLiteral("%1 uncommitted").arg(uncommitted);
			}
			if (ahead > 0U) {
				counts << QStringLiteral("%1 to push").arg(ahead);
			}
			const auto ink = Owner->palette().color(QPalette::Window).lightness() < 128 ? QStringLiteral("#d29922") : QStringLiteral("#9a6700");
			text += counts.isEmpty() ? QStringLiteral("&nbsp;&nbsp;<span>Up to date</span>") : QStringLiteral("&nbsp;&nbsp;<span style=\"color:%1\">%2</span>").arg(ink, counts.join(QStringLiteral(", ")));
		} else if (Projection.Draft && !Projection.RefreshOffered) {
			text += QStringLiteral("&nbsp;&nbsp;<span>Worktree unchanged</span>");
		}
		return text;
	}

	auto local_draft_parts::author() const noexcept -> bool { return LocalProjection.Active; }
	auto local_draft_parts::bar_leading() const noexcept -> QWidget* { return BarLeading; }
	auto local_draft_parts::bar_trailing() const noexcept -> QWidget* { return BarTrailing; }
	auto local_draft_parts::notice_row() const noexcept -> QWidget* { return NoticeRow; }
	auto local_draft_parts::files_footer() const noexcept -> QWidget* { return FilesFooter; }
	auto local_draft_parts::local_changes() const noexcept -> local_changes_panel* { return LocalChanges; }
	auto local_draft_parts::edit_page() const noexcept -> QWidget* { return EditPage; }
	auto local_draft_parts::notes_page() const noexcept -> QWidget* { return NotesPage; }
	auto local_draft_parts::workflow_page() const noexcept -> QWidget* { return StatusPage; }
	auto local_draft_parts::projection() const noexcept -> const application::local_draft_workbench_projection& { return Projection; }
	auto local_draft_parts::local_projection() const noexcept -> const application::local_changes_projection& { return LocalProjection; }
	auto local_draft_parts::identity() const noexcept -> QLabel* { return Identity; }
	auto local_draft_parts::notice() const noexcept -> QLabel* { return Notice; }
	auto local_draft_parts::working() const noexcept -> QListWidget* { return Working; }
	auto local_draft_parts::working_disclosure() const noexcept -> disclosure_button* { return WorkingDisclosure; }
	auto local_draft_parts::notes() const noexcept -> QListWidget* { return Notes; }
	auto local_draft_parts::note_editor() const noexcept -> QLineEdit* { return NoteEditor; }
	auto local_draft_parts::refresh_button() const noexcept -> QPushButton* { return Refresh; }
	auto local_draft_parts::discard_button() const noexcept -> QPushButton* { return Discard; }
	auto local_draft_parts::close_button() const noexcept -> QPushButton* { return Close; }
	auto local_draft_parts::candidates() const noexcept -> QToolButton* { return Candidates; }
	auto local_draft_parts::target_repository() const noexcept -> QComboBox* { return TargetRepository; }
	auto local_draft_parts::target_branch() const noexcept -> QComboBox* { return TargetBranch; }
	auto local_draft_parts::use_target_button() const noexcept -> QPushButton* { return UseTarget; }
	auto local_draft_parts::rebind_button() const noexcept -> QPushButton* { return Rebind; }
	auto local_draft_parts::editor() const noexcept -> suggestion_editor* { return Editor; }
	auto local_draft_parts::save_button() const noexcept -> QPushButton* { return Save; }
	auto local_draft_parts::create_button() const noexcept -> QPushButton* { return Create; }
	auto local_draft_parts::branch_button() const noexcept -> QToolButton* { return Branch; }
	auto local_draft_parts::publication() const noexcept -> local_draft_publication_area* { return PublicationArea; }
	auto local_draft_parts::workflow() const noexcept -> local_draft_workflow_area* { return WorkflowArea; }

	auto local_draft_parts::candidate_actions() const -> QList<QAction*>
	{
		auto actions = QList<QAction*>{ };
		for (auto* const action : CandidateMenu->actions()) {
			if (action->data().isValid()) {
				actions.push_back(action);
			}
		}
		return actions;
	}
}
