#include "bpr/qt/local_draft_view.hpp"

#include "bpr/qt/local_diff_view.hpp"
#include "bpr/qt/local_draft_publication_area.hpp"
#include "bpr/qt/suggestion_editor.hpp"

#include <QAction>
#include <QDateTime>
#include <QLocale>
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
#include <QStackedWidget>
#include <QTabBar>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
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
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);

		// The title is the window's; this editor is Overview's heading.
		Title = new QLineEdit{ this };
		Title->setObjectName(QStringLiteral("localDraftTitle"));
		Title->setAccessibleName(QStringLiteral("Local draft title"));
		Title->setFrame(false);
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() + 5.0);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		auto titlePalette = Title->palette();
		titlePalette.setColor(QPalette::Base, titlePalette.color(QPalette::Window));
		Title->setPalette(titlePalette);
		Title->setToolTip(QStringLiteral("The pull request's title; edit it here"));
		Title->setFocusPolicy(Qt::ClickFocus);
		Badge = new QLabel{ QStringLiteral("Local draft"), this };
		Badge->setObjectName(QStringLiteral("localDraftBadge"));
		Badge->setToolTip(QStringLiteral("Prepared on this computer. Nothing is on GitHub until a pull request is created."));
		Badge->hide();

		build_bar();
		column->addWidget(findChild<QWidget*>(QStringLiteral("localDraftBar")));

		// A specific recovery or choice, shown only when there is one.
		auto* const notices      = new QWidget{ this };
		auto* const noticeColumn = new QVBoxLayout{ notices };
		noticeColumn->setContentsMargins(12, 0, 12, 0);
		noticeColumn->setSpacing(4);
		Notice = new QLabel{ notices };
		Notice->setObjectName(QStringLiteral("localDraftNotice"));
		Notice->setWordWrap(true);
		Notice->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Notice->hide();
		noticeColumn->addWidget(Notice);
		TargetRow                = new QWidget{ notices };
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
		noticeColumn->addWidget(TargetRow);
		Rebind = new QPushButton{ QStringLiteral("Rebind to This Folder"), notices };
		Rebind->setObjectName(QStringLiteral("localDraftRebind"));
		Rebind->setToolTip(QStringLiteral("Continue this draft from the folder it was moved to"));
		Rebind->hide();
		noticeColumn->addWidget(Rebind, 0, Qt::AlignLeft);
		column->addWidget(notices);

		build_body();
		column->addWidget(findChild<QSplitter*>(QStringLiteral("localDraftBody")), 1);

		// The status line: where you are, and in author state what GitHub lacks.
		StatusLine = new QLabel{ this };
		StatusLine->setObjectName(QStringLiteral("localDraftStatusLine"));
		StatusLine->setContentsMargins(12, 4, 12, 4);
		StatusLine->setTextFormat(Qt::RichText);
		// The window's status bar shows this line; the label holds it.
		StatusLine->hide();

		connect(Title, &QLineEdit::editingFinished, this, [this] {
			if (!Presenting && Dispatch.Rename && Projection.Draft && !Title->text().trimmed().isEmpty() && Title->text().trimmed() != text_of(Projection.Draft->Title)) {
				Dispatch.Rename(utf8(Title->text().trimmed()));
			}
			Title->setModified(false);
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
		const auto openCandidateFile = [this](QListWidgetItem* const aItem) {
			if (!Presenting && aItem != nullptr && Dispatch.OpenFile) {
				ShowingLocal = false;
				Dispatch.OpenFile(utf8(aItem->data(path_role).toString()));
				set_overview(false);
			}
		};
		connect(Changes, &QListWidget::currentItemChanged, this, openCandidateFile);
		connect(OverviewFiles, &QListWidget::itemActivated, this, openCandidateFile);
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

	void local_draft_view::build_bar()
	{
		// The review bar's row: candidate (or comparison) at the left, the
		// navigation and view picker in the middle, publishing at the right.
		auto* const bar = new QWidget{ this };
		bar->setObjectName(QStringLiteral("localDraftBar"));
		bar->setFixedHeight(44);
		auto* const row = new QHBoxLayout{ bar };
		row->setContentsMargins(10, 6, 10, 6);
		row->setSpacing(8);
		Candidates = new QToolButton{ bar };
		Candidates->setObjectName(QStringLiteral("localDraftCandidates"));
		Candidates->setAccessibleName(QStringLiteral("Candidate shown"));
		Candidates->setToolTip(QStringLiteral("The captured versions of this draft; Refresh records what the worktree proposes now"));
		Candidates->setPopupMode(QToolButton::InstantPopup);
		Candidates->setToolButtonStyle(Qt::ToolButtonTextOnly);
		CandidateMenu = new QMenu{ Candidates };
		Candidates->setMenu(CandidateMenu);
		// Wide enough for "Candidate 32 of 32" from the start, so the words are
		// never cut and the pop-up never changes width.
		Candidates->setMinimumWidth(Candidates->fontMetrics().horizontalAdvance(QStringLiteral("Candidate %1 of %1").arg(core::maximum_local_draft_candidates)) + 40);
		row->addWidget(Candidates);
		// Refresh, Discard… and Close live in the candidate's menu and on Home;
		// the buttons are kept as the menu's actions' targets.
		Refresh = new QPushButton{ QStringLiteral("Refresh"), bar };
		Refresh->setObjectName(QStringLiteral("localDraftRefresh"));
		Refresh->hide();
		Discard = new QPushButton{ QStringLiteral("Discard…"), bar };
		Discard->setObjectName(QStringLiteral("localDraftDiscard"));
		Discard->hide();
		Close = new QPushButton{ QStringLiteral("Close"), bar };
		Close->setObjectName(QStringLiteral("localDraftClose"));
		Close->hide();
		Branch = new QToolButton{ bar };
		Branch->setObjectName(QStringLiteral("localDraftBranch"));
		Branch->setAccessibleName(QStringLiteral("Branch"));
		Branch->setAutoRaise(true);
		Branch->setPopupMode(QToolButton::InstantPopup);
		Branch->setToolButtonStyle(Qt::ToolButtonTextOnly);
		Branch->setMenu(new QMenu{ Branch });
		row->addWidget(Branch);
		Identity = new QLabel{ bar };
		Identity->setObjectName(QStringLiteral("localDraftIdentity"));
		Identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Identity->setFocusPolicy(Qt::TabFocus);
		row->addWidget(Identity);
		Comparing = new QToolButton{ bar };
		Comparing->setObjectName(QStringLiteral("localDraftComparing"));
		Comparing->setAccessibleName(QStringLiteral("Comparing"));
		Comparing->setPopupMode(QToolButton::InstantPopup);
		Comparing->setToolButtonStyle(Qt::ToolButtonTextOnly);
		Comparing->setMinimumWidth(Comparing->fontMetrics().horizontalAdvance(QStringLiteral("Comparing whole pull request")) + 40);
		auto* const comparing = new QMenu{ Comparing };
		const auto  names     = std::array{ QStringLiteral("Uncommitted"), QStringLiteral("Not Pushed"), QStringLiteral("Whole PR") };
		for (std::size_t index = 0U; index < names.size(); ++index) {
			auto* const action = comparing->addAction(names[index]);
			action->setCheckable(true);
			connect(action, &QAction::triggered, this, [this, index] {
				if (LocalChanges != nullptr) {
					LocalChanges->pick(static_cast<core::local_comparison>(index))->click();
				}
			});
		}
		Comparing->setMenu(comparing);
		Comparing->hide();
		row->addWidget(Comparing);
		row->addStretch(1);
		Picker = new view_picker{ bar };
		Picker->set_picked([this](const review_view aView) { show_view(aView); });
		row->addWidget(Picker);
		row->addStretch(1);
		PublishState = new QLabel{ bar };
		PublishState->setObjectName(QStringLiteral("localDraftPublishState"));
		row->addWidget(PublishState);
		Create = new QPushButton{ QStringLiteral("Create Draft PR…"), bar };
		Create->setObjectName(QStringLiteral("localDraftCreate"));
		Create->setToolTip(QStringLiteral("Push, create the draft pull request and open it on GitHub, in the Status tab's steps"));
		row->addWidget(Create);
		connect(Create, &QPushButton::clicked, this, [this] {
			Areas->setCurrentIndex(0);
			if (Dispatch.CreatePullRequest) {
				Dispatch.CreatePullRequest();
			}
		});
	}

	void local_draft_view::build_body()
	{
		auto* const body = new QSplitter{ Qt::Horizontal, this };
		body->setObjectName(QStringLiteral("localDraftBody"));
		body->setChildrenCollapsible(false);

		// The left drawer: Files and, in author state, Local Changes.
		auto* const left       = new QWidget{ body };
		auto* const leftColumn = new QVBoxLayout{ left };
		leftColumn->setContentsMargins(0, 0, 0, 0);
		leftColumn->setSpacing(0);
		Drawer = new QTabBar{ left };
		Drawer->setObjectName(QStringLiteral("localDraftDrawerTabs"));
		Drawer->setAccessibleName(QStringLiteral("Files drawer"));
		Drawer->setDocumentMode(true);
		Drawer->setExpanding(false);
		Drawer->setDrawBase(false);
		Drawer->addTab(QStringLiteral("Files"));
		Drawer->setTabToolTip(0, QStringLiteral("The candidate's changed files and the working edits you can include"));
		Drawer->addTab(QStringLiteral("Local Changes"));
		Drawer->setTabToolTip(1, QStringLiteral("What is uncommitted, not pushed, or in the whole pull request, with staging, commit and push"));
		leftColumn->addWidget(Drawer);
		DrawerStack            = new QStackedWidget{ left };
		auto* const files      = new QWidget{ DrawerStack };
		auto* const fileColumn = new QVBoxLayout{ files };
		fileColumn->setContentsMargins(10, 8, 10, 8);
		const auto heading = [](const QString& aText, QWidget* const aOwner) {
			auto* const label = new QLabel{ aText, aOwner };
			auto        font  = label->font();
			font.setWeight(QFont::DemiBold);
			label->setFont(font);
			return label;
		};
		Changes = new QListWidget{ files };
		Changes->setObjectName(QStringLiteral("localDraftChanges"));
		Changes->setAccessibleName(QStringLiteral("Changed files"));
		Changes->setFrameShape(QFrame::NoFrame);
		fileColumn->addWidget(Changes, 3);
		WorkingTitle = heading(QStringLiteral("Not committed"), files);
		fileColumn->addWidget(WorkingTitle);
		Working = new QListWidget{ files };
		Working->setObjectName(QStringLiteral("localDraftWorking"));
		Working->setAccessibleName(QStringLiteral("Working edits"));
		Working->setFrameShape(QFrame::NoFrame);
		fileColumn->addWidget(Working, 1);
		DrawerStack->addWidget(files);
		LocalChanges = new local_changes_panel{ DrawerStack };
		DrawerStack->addWidget(LocalChanges);
		leftColumn->addWidget(DrawerStack, 1);
		connect(Drawer, &QTabBar::currentChanged, DrawerStack, &QStackedWidget::setCurrentIndex);
		body->addWidget(left);

		// The centre: Overview or Review.
		Center       = new QStackedWidget{ body };
		OverviewPage = new QWidget{ Center };
		auto* const overview = new QVBoxLayout{ OverviewPage };
		overview->setContentsMargins(24, 16, 24, 16);
		overview->setSpacing(10);
		Title->setParent(OverviewPage);
		overview->addWidget(Title);
		DescriptionArea = new local_draft_description_area{ OverviewPage };
		overview->addWidget(DescriptionArea, 2);
		overview->addWidget(heading(QStringLiteral("Files"), OverviewPage));
		OverviewFiles = new QListWidget{ OverviewPage };
		OverviewFiles->setObjectName(QStringLiteral("localDraftOverviewFiles"));
		OverviewFiles->setAccessibleName(QStringLiteral("Files"));
		overview->addWidget(OverviewFiles, 1);
		Center->addWidget(OverviewPage);
		ReviewStack = new QStackedWidget{ Center };
		ReviewDiff  = new local_diff_view{ ReviewStack };
		ReviewStack->addWidget(ReviewDiff);
		// The one-pane text of the candidate's diff: the selection a tour stop uses.
		Diff = new QPlainTextEdit{ ReviewStack };
		Diff->setObjectName(QStringLiteral("localDraftDiff"));
		Diff->setAccessibleName(QStringLiteral("Proposed diff"));
		Diff->setReadOnly(true);
		Diff->setLineWrapMode(QPlainTextEdit::NoWrap);
		Diff->setTabStopDistance(QFontMetricsF{ Diff->font() }.horizontalAdvance(QLatin1Char{ ' ' }) * 4.0);
		ReviewStack->addWidget(Diff);
		EditPage             = new QWidget{ ReviewStack };
		auto* const edit     = new QVBoxLayout{ EditPage };
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
		Save = new QPushButton{ QStringLiteral("Save"), editHead };
		Save->setObjectName(QStringLiteral("localDraftSave"));
		Save->setFixedWidth(Save->fontMetrics().horizontalAdvance(QStringLiteral("Reload")) + 36);
		headRow->addWidget(Save);
		edit->addWidget(editHead);
		Editor = new suggestion_editor{ EditPage };
		Editor->setObjectName(QStringLiteral("localDraftEditor"));
		Editor->setAccessibleName(QStringLiteral("File editor"));
		edit->addWidget(Editor, 1);
		ReviewStack->addWidget(EditPage);
		Center->addWidget(ReviewStack);
		Center->setCurrentWidget(ReviewStack);
		body->addWidget(Center);

		// The right drawer: Status (publishing), Notes and Tour.
		auto* const right       = new QWidget{ body };
		auto* const rightColumn = new QVBoxLayout{ right };
		rightColumn->setContentsMargins(0, 0, 0, 0);
		rightColumn->setSpacing(0);
		Areas = new QTabBar{ right };
		Areas->setObjectName(QStringLiteral("localDraftAreas"));
		Areas->setAccessibleName(QStringLiteral("Draft drawer"));
		Areas->setDocumentMode(true);
		Areas->setExpanding(false);
		Areas->setDrawBase(false);
		Areas->addTab(QStringLiteral("Status"));
		Areas->setTabToolTip(0, QStringLiteral("Publishing in three steps: push, create the draft pull request, on GitHub"));
		Areas->addTab(QStringLiteral("Notes"));
		Areas->setTabToolTip(1, QStringLiteral("Your notes on this draft and the harness's"));
		Areas->addTab(QStringLiteral("Tour"));
		Areas->setTabToolTip(2, QStringLiteral("The route a reviewer takes through the change"));
		rightColumn->addWidget(Areas);
		AreaStack       = new QStackedWidget{ right };
		PublicationArea = new local_draft_publication_area{ AreaStack };
		AreaStack->addWidget(PublicationArea);
		auto* const notes      = new QWidget{ AreaStack };
		auto* const noteColumn = new QVBoxLayout{ notes };
		noteColumn->setContentsMargins(10, 8, 10, 8);
		Notes = new QListWidget{ notes };
		Notes->setObjectName(QStringLiteral("localDraftNotes"));
		Notes->setAccessibleName(QStringLiteral("Notes"));
		Notes->setWordWrap(true);
		Notes->setFrameShape(QFrame::NoFrame);
		Notes->setContextMenuPolicy(Qt::CustomContextMenu);
		noteColumn->addWidget(Notes, 1);
		NoteEditor = new QLineEdit{ notes };
		NoteEditor->setObjectName(QStringLiteral("localDraftNoteEditor"));
		NoteEditor->setAccessibleName(QStringLiteral("New note"));
		NoteEditor->setPlaceholderText(QStringLiteral("Add a note"));
		NoteEditor->setToolTip(QStringLiteral("Return saves the note on the selected file, or on the whole change when none is selected"));
		noteColumn->addWidget(NoteEditor);
		AreaStack->addWidget(notes);
		TourArea = new local_draft_tour_area{ AreaStack };
		AreaStack->addWidget(TourArea);
		rightColumn->addWidget(AreaStack, 1);
		connect(Areas, &QTabBar::currentChanged, AreaStack, &QStackedWidget::setCurrentIndex);
		body->addWidget(right);
		body->setStretchFactor(0, 0);
		body->setStretchFactor(1, 1);
		body->setStretchFactor(2, 0);
		body->setSizes({ 300, 820, 380 });

		Links.Selection = [this] { return selected_lines(); };
		Links.Reveal    = [this](const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast) { reveal(aPath, aFirst, aLast); };
		DescriptionArea->set_dispatch(&PreparationDispatch, &Links);
		TourArea->set_dispatch(&PreparationDispatch, &Links);

		LocalChanges->set_dispatch({ });
		ReviewDiff->set_action([this](const std::size_t aChange, const bool aStage) {
			if (Dispatch.StageChange && LocalProjection.File && aChange < ShownHunks.size()) {
				Dispatch.StageChange(LocalProjection.File->Path, ShownHunks[aChange], aStage);
			}
		});
		connect(Save, &QPushButton::clicked, this, [this] {
			if (!LocalProjection.ChangedOnDisk.empty() && LocalProjection.ChangedOnDisk == EditPath) {
				// Reload: the disk's text replaces the editor's; nothing is written.
				EditRead.clear();
				EditDirty = false;
				LocalProjection.ChangedOnDisk.clear();
				present_edit();
				return;
			}
			const auto text = Editor->text();
			if (Dispatch.Save && !EditPath.empty()) {
				Dispatch.Save(EditPath, EditRead, std::vector<std::uint8_t>(text.begin(), text.end()));
			}
		});
	}

	void local_draft_view::set_dispatch(local_draft_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void local_draft_view::set_preparation_dispatch(local_draft_preparation_dispatch aDispatch)
	{
		PreparationDispatch = std::move(aDispatch);
	}

	auto local_draft_view::selected_lines() const -> std::optional<local_draft_lines>
	{
		const auto& view   = Projection;
		const auto* latest = view.Draft ? core::latest_candidate(*view.Draft) : nullptr;
		if (latest == nullptr || view.Shown != latest->Ordinal || !view.File || view.File->Status != application::review_material_status::available) {
			return std::nullopt;
		}
		if (!ShowingLocal) {
			if (const auto lines = ReviewDiff->selected_lines()) {
				return local_draft_lines{ .Path = view.File->Path, .First = lines->first, .Last = lines->second };
			}
		}
		const auto cursor = Diff->textCursor();
		if (!cursor.hasSelection()) {
			return std::nullopt;
		}
		auto from = Diff->document()->findBlock(cursor.selectionStart()).blockNumber();
		auto to   = Diff->document()->findBlock(cursor.selectionEnd()).blockNumber();
		if (cursor.hasSelection() && cursor.selectionEnd() > cursor.selectionStart() && Diff->document()->findBlock(cursor.selectionEnd()).position() == cursor.selectionEnd() && to > from) {
			--to;
		}
		auto first = std::uint32_t{ 0U };
		auto last  = std::uint32_t{ 0U };
		for (auto row = std::max(from, 0); row <= to && static_cast<std::size_t>(row) < view.File->Rows.size(); ++row) {
			if (const auto line = view.File->Rows[static_cast<std::size_t>(row)].RightLine) {
				first = first == 0U ? *line : first;
				last  = *line;
			}
		}
		if (first == 0U) {
			return std::nullopt;
		}
		return local_draft_lines{ .Path = view.File->Path, .First = first, .Last = last };
	}

	void local_draft_view::reveal(const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast)
	{
		const auto* latest = Projection.Draft ? core::latest_candidate(*Projection.Draft) : nullptr;
		if (latest == nullptr) {
			return;
		}
		PendingReveal = local_draft_lines{ .Path = aPath, .First = aFirst, .Last = aLast };
		ShowingLocal  = false;
		set_overview(false);
		if (Projection.Shown != latest->Ordinal && Dispatch.ShowCandidate) {
			Dispatch.ShowCandidate(latest->Ordinal);
		}
		if ((!Projection.File || Projection.File->Path != aPath) && Dispatch.OpenFile) {
			Dispatch.OpenFile(aPath);
		}
		apply_reveal();
	}

	void local_draft_view::apply_reveal()
	{
		if (!PendingReveal || !Projection.File || Projection.File->Path != PendingReveal->Path || Projection.File->Status != application::review_material_status::available) {
			return;
		}
		const auto& rows  = Projection.File->Rows;
		auto        first = -1;
		auto        last  = -1;
		for (std::size_t row = 0U; row < rows.size(); ++row) {
			if (rows[row].RightLine && *rows[row].RightLine >= PendingReveal->First && *rows[row].RightLine <= PendingReveal->Last) {
				first = first < 0 ? static_cast<int>(row) : first;
				last  = static_cast<int>(row);
			}
		}
		ReviewDiff->reveal(PendingReveal->First, PendingReveal->Last);
		PendingReveal.reset();
		if (first < 0) {
			return;
		}
		auto cursor = QTextCursor{ Diff->document()->findBlockByNumber(first) };
		cursor.setPosition(Diff->document()->findBlockByNumber(last).position() + Diff->document()->findBlockByNumber(last).length() - 1, QTextCursor::KeepAnchor);
		Diff->setTextCursor(cursor);
		Diff->centerCursor();
	}

	void local_draft_view::set_code_font(const QFont& aFont)
	{
		ReviewDiff->set_code_font(aFont);
		Editor->set_code_font(aFont);
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
		// Another draft opens on Review with no chat; view state is not
		// resumed (X23 as amended 2026-11-12).
		if (auto id = aProjection.Draft ? aProjection.Draft->Id.Value : std::string{ }; id != PresentedDraft) {
			PresentedDraft = std::move(id);
			Areas->setCurrentIndex(0);
			Drawer->setCurrentIndex(0);
			ShowingLocal = false;
			set_overview(false);
		}
		DescriptionArea->present(aProjection);
		TourArea->present(aProjection);
		PublicationArea->present(aProjection);
		present_header();
		present_recovery();
		present_changes();
		present_working();
		present_notes();
		present_diff();
		present_bar();
		present_center();
		present_status_line();
		apply_reveal();
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
			refresh->setToolTip(QStringLiteral("Record what the worktree proposes now, with the working files you included, as a new candidate"));
			auto* const discard = CandidateMenu->addAction(QStringLiteral("Discard…"), Discard, &QPushButton::click);
			discard->setObjectName(QStringLiteral("localDraftDiscardAction"));
		} else {
			Candidates->setText(QStringLiteral("No candidate"));
		}
		Candidates->setEnabled(view.Draft.has_value());
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
		OverviewFiles->clear();
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
			auto* const listed = new QListWidgetItem{ item->text(), OverviewFiles };
			listed->setData(path_role, path);
			listed->setToolTip(item->toolTip());
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
	auto local_draft_view::candidates() const noexcept -> QToolButton* { return Candidates; }
	auto local_draft_view::candidate_actions() const -> QList<QAction*>
	{
		auto actions = QList<QAction*>{ };
		for (auto* const action : CandidateMenu->actions()) {
			if (action->data().isValid()) {
				actions.push_back(action);
			}
		}
		return actions;
	}
	auto local_draft_view::drawer() const noexcept -> QTabBar* { return Drawer; }
	auto local_draft_view::overview() const noexcept -> bool { return Center->currentWidget() == OverviewPage; }
	auto local_draft_view::review_diff() const noexcept -> local_diff_view* { return ReviewDiff; }
	auto local_draft_view::picker() const noexcept -> view_picker* { return Picker; }
	auto local_draft_view::local_changes() const noexcept -> local_changes_panel* { return LocalChanges; }
	auto local_draft_view::editor() const noexcept -> suggestion_editor* { return Editor; }
	auto local_draft_view::save_button() const noexcept -> QPushButton* { return Save; }
	auto local_draft_view::create_button() const noexcept -> QPushButton* { return Create; }
	auto local_draft_view::branch_button() const noexcept -> QToolButton* { return Branch; }
	auto local_draft_view::comparing_button() const noexcept -> QToolButton* { return Comparing; }
	auto local_draft_view::status_line() const noexcept -> QLabel* { return StatusLine; }
	auto local_draft_view::local_projection() const noexcept -> const application::local_changes_projection& { return LocalProjection; }
	auto local_draft_view::target_repository() const noexcept -> QComboBox* { return TargetRepository; }
	auto local_draft_view::target_branch() const noexcept -> QComboBox* { return TargetBranch; }
	auto local_draft_view::use_target_button() const noexcept -> QPushButton* { return UseTarget; }
	auto local_draft_view::rebind_button() const noexcept -> QPushButton* { return Rebind; }
	auto local_draft_view::projection() const noexcept -> const application::local_draft_workbench_projection& { return Projection; }

	auto local_draft_view::areas() const noexcept -> QTabBar*
	{
		return Areas;
	}

	auto local_draft_view::description() const noexcept -> local_draft_description_area*
	{
		return DescriptionArea;
	}

	auto local_draft_view::tour() const noexcept -> local_draft_tour_area*
	{
		return TourArea;
	}

	auto local_draft_view::publication() const noexcept -> local_draft_publication_area*
	{
		return PublicationArea;
	}

	void local_draft_view::set_overview(const bool aOverview)
	{
		Center->setCurrentWidget(aOverview ? OverviewPage : static_cast<QWidget*>(ReviewStack));
	}

	auto local_draft_view::window_title() const -> QString
	{
		const auto* target = Projection.Draft ? &Projection.Draft->Target : Projection.Target ? &*Projection.Target
																							  : nullptr;
		const auto  title  = title_text();
		if (target == nullptr) {
			return title;
		}
		if (Projection.Draft && Projection.Draft->Association) {
			return QStringLiteral("%1  #%2 %3").arg(repository_text(target->Target.Repository)).arg(Projection.Draft->Association->Number).arg(title);
		}
		return QStringLiteral("%1  %2").arg(repository_text(target->Target.Repository), title);
	}

	void local_draft_view::show_view(const review_view aView)
	{
		if (aView == review_view::edit && !ShowingLocal && Projection.File && Dispatch.OpenLocalFile) {
			// Edit is the worktree's file: read it through Local Changes.
			ShowingLocal = true;
			Dispatch.OpenLocalFile(Projection.File->Path);
		}
		set_overview(false);
		present_center();
	}

	void local_draft_view::present_local(const application::local_changes_projection& aProjection)
	{
		LocalProjection = aProjection;
		LocalChanges->present(aProjection);
		if (!aProjection.Active) {
			ShowingLocal = false;
		} else if (Drawer->currentIndex() == 1 && aProjection.File) {
			ShowingLocal = true;
		}
		if (!aProjection.Saved.empty() && aProjection.Saved == EditPath) {
			// The disk now holds what was saved: that is what the next Save checks against.
			const auto text = Editor->text();
			EditRead.assign(text.begin(), text.end());
			EditDirty = false;
		}
		present_bar();
		present_center();
		present_status_line();
	}

	void local_draft_view::present_bar()
	{
		const auto& local  = LocalProjection;
		const auto  author = local.Active;
		Drawer->setTabVisible(1, author);
		if (!author && Drawer->currentIndex() == 1) {
			Drawer->setCurrentIndex(0);
		}
		Picker->set_author(author);
		auto refusal = QString{ };
		if (author && ShowingLocal && local.File && local.File->EditRefusal) {
			switch (*local.File->EditRefusal) {
			case core::local_edit_refusal::deleted:    refusal = QStringLiteral("the file is deleted"); break;
			case core::local_edit_refusal::not_a_file: refusal = QStringLiteral("it is not a file"); break;
			case core::local_edit_refusal::link:       refusal = QStringLiteral("it is a symbolic link"); break;
			case core::local_edit_refusal::submodule:  refusal = QStringLiteral("it is a submodule"); break;
			case core::local_edit_refusal::outside:    refusal = QStringLiteral("it is outside the worktree"); break;
			case core::local_edit_refusal::too_large:  refusal = QStringLiteral("it is larger than 1 MiB"); break;
			case core::local_edit_refusal::not_utf8:   refusal = QStringLiteral("it is not UTF-8 text"); break;
			case core::local_edit_refusal::nul_byte:   refusal = QStringLiteral("it holds a NUL byte"); break;
			case core::local_edit_refusal::filtered:   refusal = QStringLiteral("a Git filter (such as LFS) stores other bytes than the disk's"); break;
			}
		}
		Picker->set_edit_refusal(refusal);
		// Author state: the branch with ahead and behind is the one thing that
		// says the window is on your files; the comparison takes the slot the
		// branch-into-target text had.
		Branch->setVisible(author);
		Identity->setVisible(!author);
		Comparing->setVisible(author);
		if (author) {
			auto branch = QStringLiteral("⎇ %1").arg(text_of(local.Subject.Branch));
			if (local.Hosted.Observed && local.Hosted.Ahead > 0U) {
				branch += QStringLiteral("  ↑%1").arg(local.Hosted.Ahead);
			}
			if (local.Hosted.Observed && local.Hosted.Behind > 0U) {
				branch += QStringLiteral("  ↓%1").arg(local.Hosted.Behind);
			}
			Branch->setText(branch);
			Branch->setToolTip(QStringLiteral("Your worktree at %1").arg(text_of(local.Subject.Worktree)));
			auto* const menu = Branch->menu();
			menu->clear();
			auto* const where = menu->addAction(text_of(local.Subject.Worktree));
			where->setEnabled(false);
			menu->addSeparator();
			menu->addAction(QStringLiteral("Show Local Changes"), this, [this] { Drawer->setCurrentIndex(1); });
			auto* const push = menu->addAction(QStringLiteral("Push"), this, [this] { LocalChanges->push_button()->click(); });
			push->setEnabled(local.Push.Offered);
			const auto names = std::array{ QStringLiteral("Comparing uncommitted"), QStringLiteral("Comparing not pushed"), QStringLiteral("Comparing whole pull request") };
			Comparing->setText(names[static_cast<std::size_t>(local.Comparison)]);
			for (std::size_t index = 0U; index < 3U; ++index) {
				auto* const action = Comparing->menu()->actions()[static_cast<qsizetype>(index)];
				action->setChecked(index == static_cast<std::size_t>(local.Comparison));
				action->setEnabled(index != 1U || local.NotPushedAvailable);
			}
		}
		// Publishing: one word of where it stands, and Create Draft PR… where Submit review sits.
		const auto& publishing = Projection.Publishing;
		auto        state      = QString{ };
		auto        ink        = palette().color(QPalette::WindowText);
		if (!publishing.Linked.empty()) {
			state = QStringLiteral("● On GitHub");
		} else if (author && local.Push.State == core::local_push_state::missing) {
			state = QStringLiteral("● Push needed");
			ink   = QColor{ 154, 103, 0 };
		} else if (publishing.Checked && publishing.Push) {
			state = QStringLiteral("● Push needed");
			ink   = QColor{ 154, 103, 0 };
		}
		if (palette().color(QPalette::Window).lightness() < 128 && state.contains(QStringLiteral("Push"))) {
			ink = QColor{ 210, 153, 34 };
		}
		auto statePalette = PublishState->palette();
		statePalette.setColor(QPalette::WindowText, ink);
		PublishState->setPalette(statePalette);
		PublishState->setText(state);
		Create->setEnabled(Projection.Draft.has_value() && publishing.Linked.empty() && !(Projection.Draft && Projection.Draft->Association));
		Create->setDefault(Create->isEnabled() && (!author || local.Push.State == core::local_push_state::up_to_date));
	}

	void local_draft_view::present_center()
	{
		const auto view = Picker->view();
		if (view == review_view::edit && LocalProjection.Active) {
			ReviewStack->setCurrentWidget(EditPage);
			present_edit();
			return;
		}
		ReviewStack->setCurrentWidget(ReviewDiff);
		ReviewDiff->set_one_pane(view == review_view::one_pane);
		const auto short_of = [](const std::optional<core::git_commit_oid>& aCommit) { return aCommit ? short_commit(*aCommit).left(7) : QString{ }; };
		const auto target   = Projection.Draft ? text_of(Projection.Draft->Target.Target.Branch) : QStringLiteral("target");
		if (ShowingLocal && LocalProjection.Active && LocalProjection.File) {
			const auto& file = *LocalProjection.File;
			ShownHunks.clear();
			if (!file.Diff) {
				ReviewDiff->clear(QStringLiteral("%1 could not be compared.").arg(text_of(file.Path)));
				return;
			}
			auto rows  = core::diff_rows(*file.Diff);
			auto bands = std::vector<local_diff_band>{ };
			for (std::size_t index = 0U; index < file.Bands.size(); ++index) {
				const auto& band  = file.Bands[index];
				const auto  first = std::ranges::find_if(rows, [&band](const core::cycle_diff_row& aRow) {
                    if (aRow.Kind == core::cycle_diff_row_kind::context) {
                        return false;
                    }
                    const auto left  = aRow.LeftLine && *aRow.LeftLine - 1U >= band.Shown.LeftBegin && *aRow.LeftLine - 1U < band.Shown.LeftEnd;
                    const auto right = aRow.RightLine && *aRow.RightLine - 1U >= band.Shown.RightBegin && *aRow.RightLine - 1U < band.Shown.RightEnd;
                    return left || right;
                });
				const auto stage = band.Unstaged && band.Stage.has_value();
				ShownHunks.push_back(stage ? *band.Stage : band.Unstage.value_or(band.Shown));
				bands.push_back({ .Row    = static_cast<std::size_t>(std::distance(rows.begin(), first)),
								  .Title  = QStringLiteral("Change %1 of %2").arg(index + 1U).arg(file.Bands.size()),
								  .State  = band.Staged && band.Unstaged ? QStringLiteral("Partly staged") : band.Staged ? QStringLiteral("Staged") : QString{ },
								  .Action = stage ? QStringLiteral("Stage Change") : band.Unstage ? QStringLiteral("Unstage Change") : QString{ },
								  .Change = index,
								  .Stage  = stage });
			}
			auto left = QString{ };
			switch (file.Comparison) {
			case core::local_comparison::uncommitted: left = QStringLiteral("Last commit %1").arg(short_of(file.LeftCommit)); break;
			case core::local_comparison::not_pushed:
				left = LocalProjection.Hosted.PullRequest ? QStringLiteral("#%1 head %2").arg(*LocalProjection.Hosted.PullRequest).arg(short_of(file.LeftCommit)) : QStringLiteral("GitHub %1").arg(short_of(file.LeftCommit));
				break;
			case core::local_comparison::whole_pull_request: left = QStringLiteral("%1 at merge base %2").arg(target, short_of(file.LeftCommit)); break;
			}
			ReviewDiff->present(std::move(rows), std::move(bands), left, text_of(file.Path), QStringLiteral("Worktree"));
			return;
		}
		if (!Projection.File || Projection.File->Status != application::review_material_status::available) {
			ReviewDiff->clear(Projection.File ? Diff->toPlainText() : QString{ });
			return;
		}
		const core::local_draft_candidate* shown = nullptr;
		if (Projection.Draft) {
			const auto found = std::ranges::find(Projection.Draft->Candidates, Projection.Shown, &core::local_draft_candidate::Ordinal);
			shown            = found == Projection.Draft->Candidates.end() ? nullptr : &*found;
		}
		ReviewDiff->present(Projection.File->Rows, { }, shown != nullptr ? QStringLiteral("%1 at merge base %2").arg(target, short_commit(shown->Base).left(7)) : target,
							text_of(Projection.File->Path), shown != nullptr ? QStringLiteral("Candidate %1").arg(shown->Ordinal) : QString{ });
	}

	void local_draft_view::present_edit()
	{
		const auto* const file = LocalProjection.File ? &*LocalProjection.File : nullptr;
		if (file == nullptr || file->EditRefusal) {
			EditName->setText(file == nullptr ? QString{ } : text_of(file->Path));
			EditState->setText({ });
			Save->setEnabled(false);
			Editor->set_text({ });
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
		EditState->setText(changedOnDisk ? QStringLiteral("Changed on disk") : EditDirty ? QStringLiteral("Not saved") : QString{ });
		Save->setText(changedOnDisk ? QStringLiteral("Reload") : QStringLiteral("Save"));
		Save->setToolTip(changedOnDisk ? QStringLiteral("The file changed on disk: nothing was written. Reload reads it again; your edits are dropped")
									   : QStringLiteral("Write this text to the file in your worktree; nothing is staged"));
		Save->setEnabled(changedOnDisk || (EditDirty && LocalProjection.Running == application::local_changes_operation::none));
		// The gutter keeps the comparison; a line edited since the last save is dashed.
		const auto marks   = file->Marks;
		const auto read    = QString::fromUtf8(reinterpret_cast<const char*>(EditRead.data()), static_cast<qsizetype>(EditRead.size())).split(QLatin1Char{ '\n' });
		const auto dark    = palette().color(QPalette::Window).lightness() < 128;
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

	void local_draft_view::present_status_line()
	{
		auto parts = QStringList{ };
		if (Projection.Draft) {
			const auto count = Changes->count();
			const auto row   = Changes->currentRow();
			if (count > 0) {
				parts << QStringLiteral("File %1 of %2.").arg(std::max(row, 0) + 1).arg(count);
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
			const auto ink = palette().color(QPalette::Window).lightness() < 128 ? QStringLiteral("#d29922") : QStringLiteral("#9a6700");
			text += counts.isEmpty() ? QStringLiteral("&nbsp;&nbsp;Up to date") : QStringLiteral("&nbsp;&nbsp;<span style=\"color:%1\">%2</span>").arg(ink, counts.join(QStringLiteral(", ")));
		} else if (Projection.Draft && !Projection.RefreshOffered) {
			text += QStringLiteral("&nbsp;&nbsp;<span style=\"color:palette(placeholder-text)\">Worktree unchanged</span>");
		}
		StatusLine->setText(text);
	}
}
