#include "bpr/qt/local_draft_view.hpp"

#include <QAction>
#include <QComboBox>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTextBlock>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
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

		auto change_word(const core::draft_change aChange) -> QString
		{
			switch (aChange) {
			case core::draft_change::added:    return QStringLiteral("Added");
			case core::draft_change::modified: return QStringLiteral("Modified");
			case core::draft_change::deleted:  return QStringLiteral("Deleted");
			case core::draft_change::mode:     return QStringLiteral("Mode changed");
			}
			return { };
		}

		auto change_letter(const core::draft_change aChange) -> QString
		{
			switch (aChange) {
			case core::draft_change::added:    return QStringLiteral("A");
			case core::draft_change::modified: return QStringLiteral("M");
			case core::draft_change::deleted:  return QStringLiteral("D");
			case core::draft_change::mode:     return QStringLiteral("T");
			}
			return { };
		}

		auto edit_word(const core::working_file& aFile) -> QString
		{
			switch (aFile.Qualification) {
			case core::working_qualification::filtered:    return QStringLiteral("Filtered (Git LFS or another filter): its bytes cannot be captured exactly");
			case core::working_qualification::too_large:   return QStringLiteral("Too large to capture");
			case core::working_qualification::unsupported: return QStringLiteral("Not a file or link: cannot be captured");
			case core::working_qualification::none:        break;
			}
			switch (aFile.Edit) {
			case core::working_edit::modified:  return QStringLiteral("Changed in the working tree, not committed");
			case core::working_edit::deleted:   return QStringLiteral("Deleted in the working tree, not committed");
			case core::working_edit::untracked: return QStringLiteral("Not tracked by Git");
			}
			return { };
		}

		auto qualification_text(const core::candidate_qualification aQualification) -> QString
		{
			switch (aQualification) {
			case core::candidate_qualification::includes_working_files:  return QStringLiteral("Includes uncommitted working files: this is not the branch head.");
			case core::candidate_qualification::sparse_checkout:         return QStringLiteral("Sparse checkout: paths outside it were not compared.");
			case core::candidate_qualification::working_scan_incomplete: return QStringLiteral("The working tree was not fully scanned.");
			case core::candidate_qualification::line_ending_conversion:  return QStringLiteral("core.autocrlf is set: working bytes may differ only in line endings.");
			}
			return { };
		}

		auto chain_text(const application::local_draft_workbench_projection& aView) -> QString
		{
			auto lines = QStringList{ };
			for (const auto& hop : aView.Chain) {
				lines << QStringLiteral("Through %1 (%2)").arg(text_of(hop.Location), text_of(hop.Remote));
			}
			return lines.join(QLatin1Char{ '\n' });
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

	local_draft_view::local_draft_view(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("localDraftPage"));
		setAccessibleName(QStringLiteral("Local draft"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(16, 12, 16, 12);
		column->setSpacing(6);

		// The title row: the draft's name, and that it is local.
		auto* const titleRow = new QHBoxLayout{ };
		Title                = new QLineEdit{ this };
		Title->setObjectName(QStringLiteral("localDraftTitle"));
		Title->setAccessibleName(QStringLiteral("Local draft title"));
		Title->setFrame(false);
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() + 3.0);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		// A heading that can be edited, not a field: it sits on the window's ground.
		auto titlePalette = Title->palette();
		titlePalette.setColor(QPalette::Base, titlePalette.color(QPalette::Window));
		Title->setPalette(titlePalette);
		Title->setToolTip(QStringLiteral("The draft's title; edit it here"));
		// The page opens on Changes: the title takes focus only when clicked.
		Title->setFocusPolicy(Qt::ClickFocus);
		titleRow->addWidget(Title, 1);
		Badge = new QLabel{ QStringLiteral("Local draft"), this };
		Badge->setObjectName(QStringLiteral("localDraftBadge"));
		Badge->setToolTip(QStringLiteral("Prepared on this computer. Nothing is on GitHub until a pull request is created."));
		titleRow->addWidget(Badge);
		column->addLayout(titleRow);

		// Source into target, and the page's commands at the same height.
		auto* const identityRow = new QHBoxLayout{ };
		Identity                = new QLabel{ this };
		Identity->setObjectName(QStringLiteral("localDraftIdentity"));
		Identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Identity->setFocusPolicy(Qt::TabFocus);
		identityRow->addWidget(Identity, 1);
		Candidates = new QComboBox{ this };
		Candidates->setObjectName(QStringLiteral("localDraftCandidates"));
		Candidates->setAccessibleName(QStringLiteral("Candidate shown"));
		Candidates->setToolTip(QStringLiteral("Each refresh records a candidate; earlier ones stay as they were captured"));
		identityRow->addWidget(Candidates);
		Refresh = new QPushButton{ QStringLiteral("Refresh"), this };
		Refresh->setObjectName(QStringLiteral("localDraftRefresh"));
		Refresh->setToolTip(QStringLiteral("Record what the worktree proposes now, with the working files you included, as a new candidate"));
		Discard = new QPushButton{ QStringLiteral("Discard…"), this };
		Discard->setObjectName(QStringLiteral("localDraftDiscard"));
		Discard->setToolTip(QStringLiteral("Remove this local draft's notes and candidates; the worktree, its branches and any pull request stay"));
		Close = new QPushButton{ QStringLiteral("Close"), this };
		Close->setObjectName(QStringLiteral("localDraftClose"));
		Close->setToolTip(QStringLiteral("Leave the draft; it stays in In preparation"));
		// Consistent sizes: the three commands share the widest width.
		const auto width = std::max({ Refresh->sizeHint().width(), Discard->sizeHint().width(), Close->sizeHint().width() });
		for (auto* const button : { Refresh, Discard, Close }) {
			button->setMinimumWidth(width);
			identityRow->addWidget(button);
		}
		column->addLayout(identityRow);

		// A specific recovery or choice, shown only when there is one.
		Notice = new QLabel{ this };
		Notice->setObjectName(QStringLiteral("localDraftNotice"));
		Notice->setWordWrap(true);
		Notice->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Notice->hide();
		column->addWidget(Notice);
		TargetRow                = new QWidget{ this };
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
		TargetRow->hide();
		column->addWidget(TargetRow);
		Rebind = new QPushButton{ QStringLiteral("Rebind to This Folder"), this };
		Rebind->setObjectName(QStringLiteral("localDraftRebind"));
		Rebind->setToolTip(QStringLiteral("Continue this draft from the folder it was moved to"));
		Rebind->hide();
		column->addWidget(Rebind, 0, Qt::AlignLeft);

		// Changes: the files, working edits and notes beside the exact diff.
		auto* const splitter = new QSplitter{ Qt::Horizontal, this };
		splitter->setObjectName(QStringLiteral("localDraftChangesArea"));
		splitter->setChildrenCollapsible(false);
		auto* const side       = new QWidget{ splitter };
		auto* const sideColumn = new QVBoxLayout{ side };
		sideColumn->setContentsMargins(0, 0, 0, 0);
		const auto heading = [side](const QString& aText) {
			auto* const label = new QLabel{ aText, side };
			auto        font  = label->font();
			font.setWeight(QFont::DemiBold);
			label->setFont(font);
			return label;
		};
		sideColumn->addWidget(heading(QStringLiteral("Changes")));
		Changes = new QListWidget{ side };
		Changes->setObjectName(QStringLiteral("localDraftChanges"));
		Changes->setAccessibleName(QStringLiteral("Changed files"));
		sideColumn->addWidget(Changes, 3);
		WorkingTitle = heading(QStringLiteral("Not committed"));
		WorkingTitle->setToolTip(QStringLiteral("Working-tree edits. Tick a file to include its exact bytes in the next candidate; nothing is staged or committed"));
		sideColumn->addWidget(WorkingTitle);
		Working = new QListWidget{ side };
		Working->setObjectName(QStringLiteral("localDraftWorking"));
		Working->setAccessibleName(QStringLiteral("Working edits"));
		sideColumn->addWidget(Working, 1);
		sideColumn->addWidget(heading(QStringLiteral("Notes")));
		Notes = new QListWidget{ side };
		Notes->setObjectName(QStringLiteral("localDraftNotes"));
		Notes->setAccessibleName(QStringLiteral("Notes"));
		Notes->setWordWrap(true);
		Notes->setContextMenuPolicy(Qt::CustomContextMenu);
		sideColumn->addWidget(Notes, 2);
		NoteEditor = new QLineEdit{ side };
		NoteEditor->setObjectName(QStringLiteral("localDraftNoteEditor"));
		NoteEditor->setAccessibleName(QStringLiteral("New note"));
		NoteEditor->setPlaceholderText(QStringLiteral("Add a note"));
		NoteEditor->setToolTip(QStringLiteral("Return saves the note on the selected file, or on the whole change when none is selected"));
		sideColumn->addWidget(NoteEditor);
		Diff = new QPlainTextEdit{ splitter };
		Diff->setObjectName(QStringLiteral("localDraftDiff"));
		Diff->setAccessibleName(QStringLiteral("Proposed diff"));
		Diff->setReadOnly(true);
		Diff->setLineWrapMode(QPlainTextEdit::NoWrap);
		Diff->setTabStopDistance(QFontMetricsF{ Diff->font() }.horizontalAdvance(QLatin1Char{ ' ' }) * 4.0);
		splitter->addWidget(side);
		splitter->addWidget(Diff);
		splitter->setStretchFactor(0, 0);
		splitter->setStretchFactor(1, 1);
		splitter->setSizes({ 300, 700 });
		column->addWidget(splitter, 1);

		connect(Title, &QLineEdit::editingFinished, this, [this] {
			if (!Presenting && Dispatch.Rename && Projection.Draft && !Title->text().trimmed().isEmpty() && Title->text().trimmed() != text_of(Projection.Draft->Title)) {
				Dispatch.Rename(utf8(Title->text().trimmed()));
			}
			Title->setModified(false);
		});
		connect(Candidates, &QComboBox::currentIndexChanged, this, [this](const int aIndex) {
			if (!Presenting && aIndex >= 0 && Dispatch.ShowCandidate) {
				Dispatch.ShowCandidate(Candidates->itemData(aIndex).toULongLong());
			}
		});
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
		connect(Changes, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* const aItem) {
			if (!Presenting && aItem != nullptr && Dispatch.OpenFile) {
				Dispatch.OpenFile(utf8(aItem->data(path_role).toString()));
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
			const auto* const current = Changes->currentItem();
			Dispatch.AddNote(utf8(text), current == nullptr ? std::string{ } : utf8(current->data(path_role).toString()));
			NoteEditor->clear();
		});
		connect(Notes, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) {
			auto* const item = Notes->itemAt(aAt);
			if (item == nullptr) {
				return;
			}
			auto* const menu   = new QMenu{ this };
			auto* const remove = menu->addAction(QStringLiteral("Remove Note"));
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
		present({ });
	}

	void local_draft_view::set_dispatch(local_draft_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void local_draft_view::set_code_font(const QFont& aFont)
	{
		Diff->setFont(aFont);
		Diff->setTabStopDistance(QFontMetricsF{ aFont }.horizontalAdvance(QLatin1Char{ ' ' }) * 4.0);
	}

	void local_draft_view::set_discard_question(std::function<bool()> aQuestion)
	{
		DiscardQuestion = std::move(aQuestion);
	}

	void local_draft_view::ask_discard()
	{
		if (!Projection.Draft || !Dispatch.Discard) {
			return;
		}
		auto discard = false;
		if (DiscardQuestion) {
			discard = DiscardQuestion();
		} else {
			auto box = QMessageBox{ QMessageBox::Warning, QStringLiteral("Discard Local Draft"),
									QStringLiteral("Discard “%1”?").arg(text_of(Projection.Draft->Title)), QMessageBox::NoButton, this };
			box.setInformativeText(QStringLiteral("Its notes and candidates are removed from this computer. The worktree, its branches and any pull request are not changed."));
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

	void local_draft_view::present(const application::local_draft_workbench_projection& aProjection)
	{
		Projection = aProjection;
		Presenting = true;
		present_header();
		present_recovery();
		present_changes();
		present_working();
		present_notes();
		present_diff();
		Presenting = false;
	}

	auto local_draft_view::title_text() const -> QString
	{
		if (Projection.Draft) {
			return text_of(Projection.Draft->Title);
		}
		return Projection.Branch.empty() ? QStringLiteral("Prepare pull request") : text_of(Projection.Branch);
	}

	void local_draft_view::present_header()
	{
		const auto& view = Projection;
		// The author's unsaved typing stays; anything else shows the draft's title.
		if (!Title->isModified()) {
			Title->setText(title_text());
		}
		Title->setReadOnly(!view.Draft);
		auto source = text_of(view.Draft ? view.Draft->Branch : view.Branch);
		auto target = target_text(view);
		Identity->setText(target.isEmpty() ? source : QStringLiteral("%1 into %2").arg(source, target));
		auto tip = QStringList{ QStringLiteral("Worktree: %1").arg(text_of(view.Draft ? view.Draft->Worktree.Location : view.Location)) };
		if (const auto chain = chain_text(view); !chain.isEmpty()) {
			tip << chain;
		}
		const auto* targetChoice = view.Draft ? &view.Draft->Target : view.Target ? &*view.Target
																				  : nullptr;
		if (targetChoice != nullptr) {
			tip << QStringLiteral("Target remote: %1 (%2)")
					   .arg(text_of(targetChoice->Target.Remote),
							targetChoice->Origin == core::target_origin::chosen ? QStringLiteral("chosen") : QStringLiteral("its default branch"));
		}
		const core::local_draft_candidate* shown = nullptr;
		if (view.Draft) {
			const auto found = std::ranges::find(view.Draft->Candidates, view.Shown, &core::local_draft_candidate::Ordinal);
			shown            = found == view.Draft->Candidates.end() ? nullptr : &*found;
		}
		if (shown != nullptr) {
			tip << QStringLiteral("Head %1").arg(short_commit(shown->Head)) << QStringLiteral("Target tip %1 when captured").arg(short_commit(shown->TargetTip))
				<< QStringLiteral("Compared from merge base %1").arg(short_commit(shown->Base));
			for (const auto qualification : shown->Qualifications) {
				tip << qualification_text(qualification);
			}
		}
		Identity->setToolTip(tip.join(QLatin1Char{ '\n' }));
		Identity->setAccessibleName(Identity->text());

		Candidates->clear();
		if (view.Draft) {
			for (const auto& candidate : view.Draft->Candidates) {
				Candidates->addItem(QStringLiteral("Candidate %1 of %2").arg(candidate.Ordinal).arg(view.Draft->Candidates.size()), QVariant::fromValue<qulonglong>(candidate.Ordinal));
				if (candidate.Ordinal == view.Shown) {
					Candidates->setCurrentIndex(Candidates->count() - 1);
				}
			}
		}
		Candidates->setEnabled(view.Draft && view.Draft->Candidates.size() > 1U);
		Refresh->setEnabled(view.Draft.has_value() && view.RefreshOffered && !view.Busy);
		Discard->setEnabled(view.Draft.has_value());
	}

	void local_draft_view::present_recovery()
	{
		const auto& view  = Projection;
		auto        lines = QStringList{ };
		const auto  where = text_of(view.Location);
		switch (view.Recovery) {
		case application::local_draft_recovery::none: break;
		case application::local_draft_recovery::not_git:
			lines << QStringLiteral("%1 is not a Git worktree. Review it as a Review Cycle with File › Open Project….").arg(where);
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
			lines << QStringLiteral("Git could not read this repository%1").arg(view.Git && !view.Git->Detail.empty() ? QStringLiteral(": %1").arg(text_of(view.Git->Detail)) : QStringLiteral("."));
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
			lines << QStringLiteral("The worktree is not at %1. Its candidates and notes are here.").arg(text_of(view.Draft->Worktree.Location));
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
		Rebind->setVisible(view.Outcome == application::local_draft_open_outcome::moved && view.Draft.has_value());
	}

	void local_draft_view::present_changes()
	{
		const auto& view = Projection;
		Changes->clear();
		if (!view.Draft) {
			return;
		}
		const auto shown = std::ranges::find(view.Draft->Candidates, view.Shown, &core::local_draft_candidate::Ordinal);
		if (shown == view.Draft->Candidates.end()) {
			return;
		}
		for (const auto& change : shown->Changes) {
			const auto  path    = text_of(change.Path);
			const auto  working = change.Right && change.Right->Origin == core::draft_material_origin::working;
			auto* const item    = new QListWidgetItem{ QStringLiteral("%1  %2").arg(change_letter(change.Change), path), Changes };
			item->setData(path_role, path);
			item->setToolTip(working ? QStringLiteral("%1, from the working tree").arg(change_word(change.Change)) : change_word(change.Change));
			auto font = item->font();
			font.setItalic(working);
			item->setFont(font);
			if (view.File && view.File->Path == change.Path) {
				Changes->setCurrentItem(item);
			}
		}
	}

	void local_draft_view::present_working()
	{
		const auto& view = Projection;
		Working->clear();
		const auto blocker = QSignalBlocker{ Working };
		for (const auto& file : view.Working) {
			auto* const item = new QListWidgetItem{ text_of(file.Path), Working };
			item->setData(path_role, text_of(file.Path));
			item->setToolTip(edit_word(file));
			const auto capturable = file.Qualification == core::working_qualification::none && view.Draft.has_value();
			item->setFlags(capturable ? Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable : Qt::ItemIsSelectable);
			const auto included = std::ranges::find(view.Included, file.Path) != view.Included.end();
			item->setCheckState(included ? Qt::Checked : Qt::Unchecked);
		}
		auto heading = QStringLiteral("Not committed");
		if (view.ScanIncomplete) {
			heading += QStringLiteral(" (partly scanned)");
		}
		WorkingTitle->setText(heading);
		auto tip = QStringLiteral("Working-tree edits. Tick a file to include its exact bytes in the next candidate; nothing is staged or committed");
		if (view.FilteredUncompared > 0U) {
			tip += QStringLiteral(". %1 filtered files were not compared").arg(view.FilteredUncompared);
		}
		WorkingTitle->setToolTip(tip);
	}

	void local_draft_view::present_notes()
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
			auto* const item = new QListWidgetItem{ text, Notes };
			item->setData(note_role, QVariant::fromValue<qulonglong>(note.Id));
			item->setToolTip(QStringLiteral("%1 on candidate %2").arg(note.Origin == core::note_origin::harness ? QStringLiteral("From the harness") : QStringLiteral("Your note")).arg(note.Candidate));
		}
	}

	void local_draft_view::present_diff()
	{
		const auto& view  = Projection;
		auto        text  = QString{ };
		auto        kinds = std::vector<core::cycle_diff_row_kind>{ };
		if (!view.File) {
			Diff->setPlainText({ });
			Diff->setExtraSelections({ });
			return;
		}
		const auto& file = *view.File;
		switch (file.Status) {
		case application::review_material_status::available:
			if (file.Reason == core::diff_reason_code::binary_file) {
				text = QStringLiteral("Binary file: no text to compare.");
				break;
			}
			for (const auto& row : file.Rows) {
				const auto prefix = row.Kind == core::cycle_diff_row_kind::added ? QLatin1Char{ '+' } : row.Kind == core::cycle_diff_row_kind::removed ? QLatin1Char{ '-' }
																																					   : QLatin1Char{ ' ' };
				text += prefix + text_of(row.Text) + QLatin1Char{ '\n' };
				kinds.push_back(row.Kind);
			}
			if (file.Rows.empty()) {
				text = file.Change == core::draft_change::mode ? QStringLiteral("Only the file's mode changed.") : QStringLiteral("No text differs.");
			}
			break;
		case application::review_material_status::missing:
		case application::review_material_status::too_large: text = QStringLiteral("The bytes of %1 were not kept with this candidate.").arg(text_of(file.Path)); break;
		case application::review_material_status::failure:   text = view.Busy ? QString{ } : QStringLiteral("%1 could not be compared.").arg(text_of(file.Path)); break;
		default:                                             break;
		}
		Diff->setPlainText(text);
		auto       selections = QList<QTextEdit::ExtraSelection>{ };
		const auto base       = palette().color(QPalette::Base);
		const auto blend      = [&base](const QColor& aInk) {
			return QColor::fromRgbF(base.redF() * 0.82F + aInk.redF() * 0.18F, base.greenF() * 0.82F + aInk.greenF() * 0.18F, base.blueF() * 0.82F + aInk.blueF() * 0.18F);
		};
		const auto added   = blend(QColor{ 0x2d, 0xa4, 0x4e });
		const auto removed = blend(QColor{ 0xcf, 0x22, 0x2e });
		auto       block   = Diff->document()->firstBlock();
		for (const auto kind : kinds) {
			if (!block.isValid()) {
				break;
			}
			if (kind != core::cycle_diff_row_kind::context) {
				auto selection = QTextEdit::ExtraSelection{ };
				selection.format.setBackground(kind == core::cycle_diff_row_kind::added ? added : removed);
				selection.format.setProperty(QTextFormat::FullWidthSelection, true);
				selection.cursor = QTextCursor{ block };
				selections.push_back(selection);
			}
			block = block.next();
		}
		Diff->setExtraSelections(selections);
	}

	auto local_draft_view::title_editor() const noexcept -> QLineEdit* { return Title; }
	auto local_draft_view::identity() const noexcept -> QLabel* { return Identity; }
	auto local_draft_view::notice() const noexcept -> QLabel* { return Notice; }
	auto local_draft_view::changes() const noexcept -> QListWidget* { return Changes; }
	auto local_draft_view::working() const noexcept -> QListWidget* { return Working; }
	auto local_draft_view::notes() const noexcept -> QListWidget* { return Notes; }
	auto local_draft_view::note_editor() const noexcept -> QLineEdit* { return NoteEditor; }
	auto local_draft_view::diff() const noexcept -> QPlainTextEdit* { return Diff; }
	auto local_draft_view::refresh_button() const noexcept -> QPushButton* { return Refresh; }
	auto local_draft_view::discard_button() const noexcept -> QPushButton* { return Discard; }
	auto local_draft_view::close_button() const noexcept -> QPushButton* { return Close; }
	auto local_draft_view::candidates() const noexcept -> QComboBox* { return Candidates; }
	auto local_draft_view::target_repository() const noexcept -> QComboBox* { return TargetRepository; }
	auto local_draft_view::target_branch() const noexcept -> QComboBox* { return TargetBranch; }
	auto local_draft_view::use_target_button() const noexcept -> QPushButton* { return UseTarget; }
	auto local_draft_view::rebind_button() const noexcept -> QPushButton* { return Rebind; }
	auto local_draft_view::projection() const noexcept -> const application::local_draft_workbench_projection& { return Projection; }
}
