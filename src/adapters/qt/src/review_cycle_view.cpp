#include "bpr/qt/review_cycle_view.hpp"

#include "bpr/core/sha256.hpp"
#include "bpr/qt/appearance_profile.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <span>
#include <utility>

namespace bpr::qt {
	namespace {
		auto text_of(const std::string& aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		auto revision_text(const core::cycle_source_revision_id aRevision) -> QString
		{
			return QStringLiteral("R%1").arg(aRevision.value());
		}

		auto readable(const core::material_text_kind aKind) -> bool
		{
			return aKind == core::material_text_kind::utf8 || aKind == core::material_text_kind::utf8_bom || aKind == core::material_text_kind::empty;
		}

		auto provenance_text(const core::authored_provenance aProvenance) -> QString
		{
			switch (aProvenance) {
			case core::authored_provenance::human:            return { };
			case core::authored_provenance::adapter_observed: return QStringLiteral("From an agent (observed)");
			case core::authored_provenance::self_reported:    return QStringLiteral("Pasted (self-reported)");
			case core::authored_provenance::unknown:          return QStringLiteral("Author unknown");
			}
			return { };
		}

		auto correspondence_text(const core::cycle_target_correspondence& aCorrespondence) -> QString
		{
			const auto against = revision_text(aCorrespondence.Against);
			switch (aCorrespondence.Kind) {
			case core::cycle_correspondence::exact_material: return QStringLiteral("Unchanged in %1").arg(against);
			case core::cycle_correspondence::exact_mapped:   return QStringLiteral("Found unchanged elsewhere in %1").arg(against);
			case core::cycle_correspondence::ambiguous:      return QStringLiteral("Appears %1 times in %2; none chosen").arg(aCorrespondence.Candidates).arg(against);
			case core::cycle_correspondence::deleted:        return QStringLiteral("File deleted in %1").arg(against);
			case core::cycle_correspondence::unavailable:    return QStringLiteral("Not readable in %1").arg(against);
			case core::cycle_correspondence::unmapped:       return QStringLiteral("Changed in %1; original kept").arg(against);
			}
			return { };
		}

		auto bytes_text(const std::vector<std::uint8_t>& aBytes) -> std::string_view
		{
			return { reinterpret_cast<const char*>(aBytes.data()), aBytes.size() };
		}

		/// "line 3" or "lines 3–5".
		auto lines_text(const core::cycle_line_span& aSpan) -> QString
		{
			return aSpan.First == aSpan.Last ? QStringLiteral("line %1").arg(aSpan.First) : QStringLiteral("lines %1–%2").arg(aSpan.First).arg(aSpan.Last);
		}

		auto pair_text(const core::cycle_comparison_pair& aPair) -> QString
		{
			return aPair.Left ? QStringLiteral("%1 → %2").arg(revision_text(aPair.Left->Id), revision_text(aPair.Right.Id)) : revision_text(aPair.Right.Id);
		}

		/// What the reviewer asked for, in the words the view chooser uses.
		auto request_text(const core::cycle_view_request& aRequest) -> QString
		{
			if (const auto* round = std::get_if<core::this_round_request>(&aRequest)) {
				return QStringLiteral("This round · F%1").arg(round->Round.value());
			}
			if (std::holds_alternative<core::since_last_review_request>(aRequest)) {
				return QStringLiteral("Since my last review");
			}
			if (std::holds_alternative<core::all_cycle_request>(aRequest)) {
				return QStringLiteral("All cycle changes");
			}
			return QStringLiteral("Chosen revisions");
		}

		auto step_text(const core::cycle_history_entry& aEntry) -> QString
		{
			auto parts = QStringList{ };
			switch (aEntry.Step) {
			case core::cycle_revision_step::initial:
			case core::cycle_revision_step::same_head:
			case core::cycle_revision_step::no_repository:     break;
			case core::cycle_revision_step::head_moved:        parts.append(QStringLiteral("head moved; commits between not captured")); break;
			case core::cycle_revision_step::branch_changed:    parts.append(aEntry.HeadReference ? QStringLiteral("other branch %1").arg(text_of(*aEntry.HeadReference).section(QLatin1Char('/'), 2)) : QStringLiteral("detached head")); break;
			case core::cycle_revision_step::previous_unlisted: parts.append(QStringLiteral("capture before it not listed")); break;
			}
			if (aEntry.SameMaterial) {
				parts.append(QStringLiteral("same bytes"));
			}
			if (aEntry.ParentGap) {
				parts.append(QStringLiteral("recorded parent %1").arg(aEntry.Parent ? revision_text(*aEntry.Parent) : QStringLiteral("none")));
			}
			for (const auto& imported : aEntry.Imported) {
				parts.append(QStringLiteral("%1 of %2/%3").arg(text_of(imported.ForeignRevision), text_of(imported.ForeignCycle), text_of(imported.ForeignRound)));
			}
			if (aEntry.Coverage == core::cycle_capture_coverage::partial) {
				parts.append(QStringLiteral("partial"));
			}
			return parts.isEmpty() ? revision_text(aEntry.Revision) : QStringLiteral("%1 (%2)").arg(revision_text(aEntry.Revision), parts.join(QStringLiteral("; ")));
		}

		auto disposition_text(const core::cycle_response_disposition aDisposition) -> QString
		{
			switch (aDisposition) {
			case core::cycle_response_disposition::addressed:           return QStringLiteral("says addressed");
			case core::cycle_response_disposition::partially_addressed: return QStringLiteral("says partly addressed");
			case core::cycle_response_disposition::disputed:            return QStringLiteral("disputes it");
			case core::cycle_response_disposition::blocked:             return QStringLiteral("says blocked");
			case core::cycle_response_disposition::discussion_only:     return QStringLiteral("discusses it");
			}
			return { };
		}

		/// Who a statement is from, with any claim marked as a claim.
		auto sender_text(const std::string& aFrom, const std::optional<core::cycle_claimed_author>& aClaimed, const core::cycle_response_channel aChannel) -> QString
		{
			auto who = aFrom.empty() ? QStringLiteral("someone") : text_of(aFrom);
			if (aClaimed && aClaimed->Kind == core::cycle_claimed_author_kind::agent) {
				auto model = aClaimed->Model ? text_of(*aClaimed->Model) : QString{ };
				who += model.isEmpty() ? QStringLiteral(" (claims to be an agent)") : QStringLiteral(" (claims agent %1)").arg(model);
			}
			return who + (aChannel == core::cycle_response_channel::response_file ? QStringLiteral(", response file") : QStringLiteral(", entered by hand"));
		}

		/// Where a note stands, from the reviewer's own decision or else from
		/// what was said; never from code that moved or disappeared.
		auto status_text(const core::cycle_item_reconciliation& aItem) -> QString
		{
			const auto checked = aItem.Decision ? revision_text(aItem.Decision->Checked) : QString{ };
			switch (aItem.Status) {
			case core::cycle_item_status::open:               return { };
			case core::cycle_item_status::reported:           return QStringLiteral("A recipient replied; nothing is verified");
			case core::cycle_item_status::needs_verification: return aItem.Claim == core::cycle_response_disposition::partially_addressed ? QStringLiteral("Recipient says partly addressed; needs your verification") : QStringLiteral("Recipient says addressed; needs your verification");
			case core::cycle_item_status::verified:           return QStringLiteral("Verified by you in %1").arg(checked);
			case core::cycle_item_status::recheck_needed:
				return aItem.Currency == core::cycle_verification_currency::item_amended ? QStringLiteral("Verified by you in %1; the note changed since, check again").arg(checked)
																						 : QStringLiteral("Verified by you in %1; its code changed since, check again").arg(checked);
			case core::cycle_item_status::needs_work: return QStringLiteral("Needs work (your decision in %1)").arg(checked);
			case core::cycle_item_status::withdrawn:  return QStringLiteral("Withdrawn by you");
			case core::cycle_item_status::deferred:   return QStringLiteral("Deferred by you");
			}
			return { };
		}

		auto first_line(const std::string& aBody) -> QString
		{
			return text_of(aBody).section(QLatin1Char('\n'), 0, 0).trimmed().left(160);
		}
	}

	review_cycle_view::review_cycle_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("reviewCycleView"));
		setAccessibleName(QStringLiteral("Local cycle"));
		setAccessibleDescription(QStringLiteral("Full source of a local review cycle, its files, tabs and notes"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);

		// The review bar: the same row, in the same order, as a pull request's.
		ReviewBar = new workbench_strip{ QStringLiteral("Review bar"), this, strip_role::review_bar };
		ReviewBar->setObjectName(QStringLiteral("cycleReviewBar"));
		UpdatesToggle = new updates_disclosure{ ReviewBar };
		UpdatesToggle->setObjectName(QStringLiteral("cycleUpdatesToggle"));
		UpdatesToggle->setAccessibleName(QStringLiteral("Updates"));
		ReviewBar->addWidget(UpdatesToggle);
		FilesToggle = new QAction{ strip_icon(strip_glyph::files_drawer, palette()), QStringLiteral("Files"), this };
		FilesToggle->setObjectName(QStringLiteral("cycleFilesToggle"));
		FilesToggle->setCheckable(true);
		FilesToggle->setChecked(true);
		ReviewBar->addAction(FilesToggle);
		RevisionLabel = new QLabel{ QStringLiteral("Full source"), ReviewBar };
		RevisionLabel->setObjectName(QStringLiteral("cycleRevisionLabel"));
		RevisionLabel->setAccessibleName(QStringLiteral("Shown revision"));
		RevisionLabel->setContentsMargins(6, 0, 6, 0);
		ReviewBar->addWidget(RevisionLabel);
		// BPR-1163: which material the active tab shows, chosen here.
		ViewButton = new QToolButton{ ReviewBar };
		ViewButton->setObjectName(QStringLiteral("cycleViewButton"));
		ViewButton->setText(QStringLiteral("View"));
		ViewButton->setAccessibleName(QStringLiteral("Choose what to compare"));
		ViewButton->setToolTip(QStringLiteral("Full source, this round, since my last review, all cycle changes or chosen revisions"));
		ViewButton->setPopupMode(QToolButton::InstantPopup);
		ViewButton->setAutoRaise(true);
		ViewMenu = new QMenu{ ViewButton };
		ViewMenu->setObjectName(QStringLiteral("cycleViewMenu"));
		ViewButton->setMenu(ViewMenu);
		ReviewBar->addWidget(ViewButton);
		RefreshAction = new QAction{ QStringLiteral("Show newer"), this };
		RefreshAction->setObjectName(QStringLiteral("cycleRefreshAction"));
		RefreshAction->setToolTip(QStringLiteral("A newer capture exists; move this view to it"));
		ReviewBar->addAction(RefreshAction);
		ReviewBar->addWidget(new strip_divider{ ReviewBar });
		PreviousFile = new QAction{ strip_icon(strip_glyph::previous_file, palette()), QStringLiteral("Previous file"), this };
		PreviousFile->setObjectName(QStringLiteral("cyclePreviousFile"));
		ReviewBar->addAction(PreviousFile);
		FilePosition = new QLabel{ QStringLiteral("File"), ReviewBar };
		FilePosition->setObjectName(QStringLiteral("cycleFilePosition"));
		FilePosition->setAccessibleName(QStringLiteral("File position"));
		ReviewBar->addWidget(FilePosition);
		NextFile = new QAction{ strip_icon(strip_glyph::next_file, palette()), QStringLiteral("Next file"), this };
		NextFile->setObjectName(QStringLiteral("cycleNextFile"));
		ReviewBar->addAction(NextFile);
		auto* const spacer = new QWidget{ ReviewBar };
		spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		ReviewBar->addWidget(spacer);
		// Capture revision: the independent way back after someone else worked.
		CaptureAction = new QAction{ QStringLiteral("Capture revision"), this };
		CaptureAction->setObjectName(QStringLiteral("cycleCaptureAction"));
		CaptureAction->setToolTip(QStringLiteral("Read the project folder now and keep its bytes as the next revision"));
		CaptureAgain = new QAction{ QStringLiteral("Record another observation of the same bytes"), this };
		CaptureAgain->setObjectName(QStringLiteral("cycleCaptureAgain"));
		auto* const captureMenu = new QMenu{ this };
		captureMenu->addAction(CaptureAgain);
		CaptureAction->setMenu(captureMenu);
		ReviewBar->addAction(CaptureAction);
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(CaptureAction))) {
			button->setPopupMode(QToolButton::MenuButtonPopup);
		}
		MarkReviewed = new QAction{ QStringLiteral("Mark reviewed"), this };
		MarkReviewed->setObjectName(QStringLiteral("cycleMarkReviewed"));
		MarkReviewed->setToolTip(QStringLiteral("Record that you reviewed through the revision shown; \"Since my last review\" starts here"));
		ReviewBar->addAction(MarkReviewed);
		CommentAction = new QAction{ strip_icon(strip_glyph::comment, palette()), QStringLiteral("Comment"), this };
		CommentAction->setObjectName(QStringLiteral("cycleCommentAction"));
		CommentAction->setToolTip(QStringLiteral("Comment on the selection, or on the file when nothing is selected"));
		ReviewBar->addAction(CommentAction);
		SuggestAction = new QAction{ QStringLiteral("Suggest change"), this };
		SuggestAction->setObjectName(QStringLiteral("cycleSuggestAction"));
		SuggestAction->setToolTip(QStringLiteral("Edit this file in place, starting at the cursor, and propose the changes as exact replacements"));
		ReviewBar->addAction(SuggestAction);
		ComposeAction = new QAction{ strip_icon(strip_glyph::compose_review, palette()), QStringLiteral("Compose feedback"), this };
		ComposeAction->setObjectName(QStringLiteral("cycleComposeAction"));
		ComposeAction->setEnabled(false);
		ComposeAction->setToolTip(QStringLiteral("Composing and sharing a feedback round is not available in this build yet"));
		ReviewBar->addAction(ComposeAction);
		NotesToggle = new QAction{ strip_icon(strip_glyph::comments_drawer, palette()), QStringLiteral("Notes"), this };
		NotesToggle->setObjectName(QStringLiteral("cycleNotesToggle"));
		NotesToggle->setCheckable(true);
		NotesToggle->setChecked(true);
		ReviewBar->addAction(NotesToggle);
		column->addWidget(ReviewBar);

		// The folding Updates line: captured revisions and their quality.
		UpdatesLine = new QLabel{ this };
		UpdatesLine->setObjectName(QStringLiteral("cycleUpdatesLine"));
		UpdatesLine->setAccessibleName(QStringLiteral("Captured revisions"));
		UpdatesLine->setTextFormat(Qt::PlainText);
		UpdatesLine->setContentsMargins(8, 2, 8, 2);
		UpdatesLine->hide();
		column->addWidget(UpdatesLine);
		connect(UpdatesToggle, &QToolButton::clicked, this, [this] {
			const auto open = !UpdatesToggle->open();
			UpdatesToggle->set_open(open);
			UpdatesLine->setVisible(open);
		});

		Tabs = new document_tab_strip{ this };
		Tabs->setObjectName(QStringLiteral("cycleDocumentTabs"));
		Tabs->set_dispatch([this](const core::document_tab_intent& aIntent) {
			if (Dispatch.Tab) {
				Dispatch.Tab(aIntent);
			}
		});
		column->addWidget(Tabs);

		Body = new QSplitter{ Qt::Horizontal, this };
		Body->setObjectName(QStringLiteral("cycleBody"));
		Body->setChildrenCollapsible(false);
		Files = new QListWidget{ Body };
		Files->setObjectName(QStringLiteral("cycleFiles"));
		Files->setAccessibleName(QStringLiteral("Files"));
		Files->setMinimumWidth(140);
		Files->setContextMenuPolicy(Qt::CustomContextMenu);
		Centre = new QStackedWidget{ Body };
		// One code view: the file is read here, and Suggest change edits it
		// here, so starting a suggestion changes nothing on screen but the
		// caret (owner rejection of composer-layout v004, 2026-10-02).
		Source = new suggestion_editor{ Centre };
		Source->setObjectName(QStringLiteral("cycleSource"));
		Source->set_read_only(true);
		Source->setContextMenuPolicy(Qt::CustomContextMenu);
		SourceNotice = new QLabel{ Centre };
		SourceNotice->setObjectName(QStringLiteral("cycleSourceNotice"));
		SourceNotice->setAlignment(Qt::AlignCenter);
		SourceNotice->setWordWrap(true);
		SourceNotice->setTextFormat(Qt::PlainText);
		Comparison = new QPlainTextEdit{ Centre };
		Comparison->setObjectName(QStringLiteral("cycleComparison"));
		Comparison->setAccessibleName(QStringLiteral("Comparison"));
		Comparison->setReadOnly(true);
		Comparison->setLineWrapMode(QPlainTextEdit::NoWrap);
		Comparison->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Centre->addWidget(Source);
		Centre->addWidget(SourceNotice);
		Centre->addWidget(Comparison);
		ReadingPage = Source;

		auto* const tray       = new QWidget{ Body };
		auto* const trayColumn = new QVBoxLayout{ tray };
		trayColumn->setContentsMargins(0, 0, 0, 0);
		tray->setObjectName(QStringLiteral("cycleNotesTray"));
		tray->setMinimumWidth(220);
		auto* const trayHeader = new QHBoxLayout{ };
		auto* const trayTitle  = new QLabel{ QStringLiteral("Notes"), tray };
		trayHeader->addWidget(trayTitle, 1);
		auto* const cycleNote = new QToolButton{ tray };
		cycleNote->setObjectName(QStringLiteral("cycleWholeNote"));
		cycleNote->setText(QStringLiteral("Note on the cycle"));
		cycleNote->setAutoRaise(true);
		trayHeader->addWidget(cycleNote);
		// BPR-1164: what a recipient said back, entered by hand or from a response file.
		ReplyButton = new QToolButton{ tray };
		ReplyButton->setObjectName(QStringLiteral("cycleAddReply"));
		ReplyButton->setText(QStringLiteral("Add reply"));
		ReplyButton->setToolTip(QStringLiteral("Record what someone said back about a round: paste their words, or open a response file"));
		ReplyButton->setAutoRaise(true);
		ReplyButton->setPopupMode(QToolButton::InstantPopup);
		ReplyMenu = new QMenu{ ReplyButton };
		ReplyMenu->setObjectName(QStringLiteral("cycleReplyMenu"));
		ReplyButton->setMenu(ReplyMenu);
		trayHeader->addWidget(ReplyButton);
		NextRound = new QToolButton{ tray };
		NextRound->setObjectName(QStringLiteral("cycleNextRound"));
		NextRound->setText(QStringLiteral("Next round"));
		NextRound->setToolTip(QStringLiteral("Prepare the next round's draft from the last round's assignment and the notes still open; nothing is sent"));
		NextRound->setAutoRaise(true);
		trayHeader->addWidget(NextRound);
		connect(NextRound, &QToolButton::clicked, this, [this] {
			if (Dispatch.PrepareNextRound) {
				Dispatch.PrepareNextRound();
			}
		});
		trayColumn->addLayout(trayHeader);
		Notes = new QListWidget{ tray };
		Notes->setObjectName(QStringLiteral("cycleNotes"));
		Notes->setAccessibleName(QStringLiteral("Notes"));
		Notes->setWordWrap(true);
		Notes->setTextElideMode(Qt::ElideNone);
		Notes->setSpacing(4);
		Notes->setAlternatingRowColors(true);
		Notes->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(Notes, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) {
			if (auto* const menu = decision_menu(Notes->indexAt(aAt).row())) {
				menu->popup(Notes->viewport()->mapToGlobal(aAt));
			}
		});
		trayColumn->addWidget(Notes, 1);
		// One discussion across files: the selection becomes a related location
		// of the chosen note, which keeps its own.
		RelateButton = new QPushButton{ QStringLiteral("Link selection to note"), tray };
		RelateButton->setObjectName(QStringLiteral("cycleRelateSelection"));
		RelateButton->setToolTip(QStringLiteral("Add the selected source as a related location of the selected note"));
		trayColumn->addWidget(RelateButton);
		TrayComposer     = new QStackedWidget{ tray };
		auto* const idle = new QWidget{ TrayComposer };
		TrayComposer->addWidget(idle);
		auto* const commentPage   = new QWidget{ TrayComposer };
		auto* const commentColumn = new QVBoxLayout{ commentPage };
		commentColumn->setContentsMargins(0, 0, 0, 0);
		CommentTarget = new QLabel{ commentPage };
		CommentTarget->setObjectName(QStringLiteral("cycleCommentTarget"));
		CommentTarget->setTextFormat(Qt::PlainText);
		CommentTarget->setWordWrap(true);
		commentColumn->addWidget(CommentTarget);
		CommentEditor = new QPlainTextEdit{ commentPage };
		CommentEditor->setObjectName(QStringLiteral("cycleCommentEditor"));
		CommentEditor->setAccessibleName(QStringLiteral("Note"));
		CommentEditor->setPlaceholderText(QStringLiteral("Write a note (Markdown)"));
		CommentEditor->setTabChangesFocus(true);
		commentColumn->addWidget(CommentEditor);
		auto* const commentButtons = new QHBoxLayout{ };
		commentButtons->addStretch(1);
		auto* const cancelComment = new QPushButton{ QStringLiteral("Cancel"), commentPage };
		cancelComment->setObjectName(QStringLiteral("cycleCommentCancel"));
		auto* const saveComment = new QPushButton{ QStringLiteral("Save note"), commentPage };
		saveComment->setObjectName(QStringLiteral("cycleCommentSave"));
		saveComment->setDefault(true);
		commentButtons->addWidget(cancelComment);
		commentButtons->addWidget(saveComment);
		commentColumn->addLayout(commentButtons);
		TrayComposer->addWidget(commentPage);
		ReplyPage               = new QWidget{ TrayComposer };
		auto* const replyColumn = new QVBoxLayout{ ReplyPage };
		replyColumn->setContentsMargins(0, 0, 0, 0);
		ReplyTarget = new QLabel{ ReplyPage };
		ReplyTarget->setObjectName(QStringLiteral("cycleReplyTarget"));
		ReplyTarget->setTextFormat(Qt::PlainText);
		ReplyTarget->setWordWrap(true);
		replyColumn->addWidget(ReplyTarget);
		ReplyFrom = new QLineEdit{ ReplyPage };
		ReplyFrom->setObjectName(QStringLiteral("cycleReplyFrom"));
		ReplyFrom->setAccessibleName(QStringLiteral("Reply from"));
		ReplyFrom->setPlaceholderText(QStringLiteral("From (as you know them)"));
		replyColumn->addWidget(ReplyFrom);
		ReplyAbout = new QCheckBox{ QStringLiteral("About the selected note"), ReplyPage };
		ReplyAbout->setObjectName(QStringLiteral("cycleReplyAboutNote"));
		replyColumn->addWidget(ReplyAbout);
		ReplyEditor = new QPlainTextEdit{ ReplyPage };
		ReplyEditor->setObjectName(QStringLiteral("cycleReplyEditor"));
		ReplyEditor->setAccessibleName(QStringLiteral("Reply"));
		ReplyEditor->setPlaceholderText(QStringLiteral("Paste or write what they said"));
		ReplyEditor->setTabChangesFocus(true);
		replyColumn->addWidget(ReplyEditor);
		auto* const replyButtons = new QHBoxLayout{ };
		replyButtons->addStretch(1);
		auto* const cancelReply = new QPushButton{ QStringLiteral("Cancel"), ReplyPage };
		cancelReply->setObjectName(QStringLiteral("cycleReplyCancel"));
		auto* const saveReply = new QPushButton{ QStringLiteral("Save reply"), ReplyPage };
		saveReply->setObjectName(QStringLiteral("cycleReplySave"));
		replyButtons->addWidget(cancelReply);
		replyButtons->addWidget(saveReply);
		replyColumn->addLayout(replyButtons);
		TrayComposer->addWidget(ReplyPage);
		connect(saveReply, &QPushButton::clicked, this, [this] { save_reply(); });
		connect(cancelReply, &QPushButton::clicked, this, [this] {
			ReplyEditor->clear();
			ReplyRound.reset();
			TrayComposer->setCurrentIndex(0);
		});
		connect(ReplyMenu, &QMenu::aboutToShow, this, [this] { rebuild_reply_menu(); });
		Composer = new suggestion_composer{ *Source, TrayComposer };
		TrayComposer->addWidget(Composer);
		TrayComposer->setCurrentIndex(0);
		trayColumn->addWidget(TrayComposer);
		// Leaving the composer, however it is left, gives the pane back to reading.
		connect(TrayComposer, &QStackedWidget::currentChanged, this, [this] {
			if (!suggesting()) {
				end_suggestion();
			}
		});

		Body->addWidget(Files);
		Body->addWidget(Centre);
		Body->addWidget(tray);
		Body->setStretchFactor(0, 0);
		Body->setStretchFactor(1, 1);
		Body->setStretchFactor(2, 0);
		column->addWidget(Body, 1);
		set_code_font(suggestion_code_font());

		connect(FilesToggle, &QAction::toggled, Files, &QWidget::setVisible);
		connect(NotesToggle, &QAction::toggled, tray, &QWidget::setVisible);
		connect(PreviousFile, &QAction::triggered, this, [this] { step_file(-1); });
		connect(NextFile, &QAction::triggered, this, [this] { step_file(1); });
		connect(CaptureAction, &QAction::triggered, this, [this] {
			if (Dispatch.Capture) {
				Dispatch.Capture(false);
			}
		});
		connect(CaptureAgain, &QAction::triggered, this, [this] {
			if (Dispatch.Capture) {
				Dispatch.Capture(true);
			}
		});
		connect(MarkReviewed, &QAction::triggered, this, [this] {
			if (Dispatch.MarkReviewed) {
				Dispatch.MarkReviewed();
			}
		});
		connect(RefreshAction, &QAction::triggered, this, [this] {
			if (Dispatch.Refresh) {
				Dispatch.Refresh();
			}
		});
		connect(ViewMenu, &QMenu::aboutToShow, this, [this] { rebuild_view_menu(); });
		connect(CommentAction, &QAction::triggered, this, [this] { begin_comment(); });
		connect(SuggestAction, &QAction::triggered, this, [this] { begin_suggestion(); });
		connect(cycleNote, &QToolButton::clicked, this, [this] {
			PendingTarget = core::cycle_whole_target{ };
			CommentTarget->setText(QStringLiteral("Whole cycle"));
			TrayComposer->setCurrentIndex(1);
			CommentEditor->setFocus(Qt::OtherFocusReason);
		});
		connect(saveComment, &QPushButton::clicked, this, [this] { save_comment(); });
		connect(cancelComment, &QPushButton::clicked, this, [this] {
			CommentEditor->clear();
			PendingTarget.reset();
			TrayComposer->setCurrentIndex(0);
		});
		Source->SelectionChanged = [this] { refresh_actions(); };
		connect(Source, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) {
			if (auto* const menu = source_menu(aAt)) {
				menu->popup(Source->viewport()->mapToGlobal(aAt));
			}
		});
		connect(Notes, &QListWidget::currentRowChanged, this, [this](int) { refresh_actions(); });
		connect(RelateButton, &QPushButton::clicked, this, [this] {
			const auto row    = Notes->currentRow();
			const auto anchor = selection_anchor();
			if (row < 0 || static_cast<std::size_t>(row) >= Projection.Notes.size() || !anchor || !Dispatch.Relate) {
				return;
			}
			// Copies: the dispatch can present a new projection before it returns.
			const auto item    = Projection.Notes[static_cast<std::size_t>(row)].Item;
			const auto version = Projection.Notes[static_cast<std::size_t>(row)].Version;
			Dispatch.Relate(item, version, core::cycle_related_reference{ .Target = *anchor });
		});
		connect(Source->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](const int aValue) {
			if (!Presenting && !suggesting() && Projection.Source && Dispatch.ObserveLine) {
				Dispatch.ObserveLine(Projection.Source->Tab, static_cast<std::uint64_t>(std::max(aValue, 0)));
			}
		});
		connect(Files, &QListWidget::itemActivated, this, [this](QListWidgetItem* aItem) {
			if (aItem != nullptr && Dispatch.OpenFile) {
				const auto newTab = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
				Dispatch.OpenFile(utf8(aItem->data(Qt::UserRole).toString()), newTab);
			}
		});
		connect(Files, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) {
			auto* const item = Files->itemAt(aAt);
			if (item == nullptr || Projection.Revisions.empty()) {
				return;
			}
			const auto path   = item->data(Qt::UserRole).toString();
			const auto folder = path.section(QLatin1Char('/'), 0, -2);
			auto*      menu   = new QMenu{ Files };
			menu->setAttribute(Qt::WA_DeleteOnClose);
			menu->addAction(QStringLiteral("Open in New Tab"), this, [this, path] {
				if (Dispatch.OpenFile) {
					Dispatch.OpenFile(utf8(path), true);
				}
			});
			if (!folder.isEmpty()) {
				menu->addAction(QStringLiteral("Note on %1/").arg(folder), this, [this, folder] {
					PendingTarget = core::cycle_module_target{ .Path = utf8(folder), .Revision = Projection.Source ? std::optional{ Projection.Source->Revision } : std::nullopt };
					CommentTarget->setText(QStringLiteral("Folder %1/").arg(folder));
					TrayComposer->setCurrentIndex(1);
					CommentEditor->setFocus(Qt::OtherFocusReason);
				});
			}
			menu->popup(Files->viewport()->mapToGlobal(aAt));
		});
		Composer->Saved = [this] {
			if (!SuggestionSource || !Dispatch.Suggest) {
				return;
			}
			// Each separate change is one exact suggestion of whole original lines.
			auto request = application::review_cycle_suggestion_request{
				.Intent    = Composer->illustrative() ? core::cycle_suggestion_intent::illustrative : core::cycle_suggestion_intent::exact_replacement,
				.Rationale = Composer->rationale(),
			};
			for (const auto& edit : Composer->edits()) {
				auto anchor = core::make_anchor(SuggestionSource->Revision, SuggestionSource->Path, SuggestionSource->Bytes, edit.Start, edit.End);
				if (!anchor) {
					return;
				}
				request.Edits.push_back({ .Selection = std::move(*anchor), .Replacement = edit.Replacement });
			}
			TrayComposer->setCurrentIndex(0);
			Dispatch.Suggest(std::move(request));
		};
		Composer->Cancelled = [this] { TrayComposer->setCurrentIndex(0); };
		refresh_actions();
	}

	void review_cycle_view::set_dispatch(review_cycle_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void review_cycle_view::set_code_font(const QFont& aFont)
	{
		const auto tab = QFontMetricsF{ aFont }.horizontalAdvance(QLatin1Char(' ')) * TabWidth;
		Comparison->setFont(aFont);
		Comparison->setTabStopDistance(tab);
		Composer->set_code_font(aFont);
	}

	void review_cycle_view::adopt_appearance(const appearance_profile& aProfile)
	{
		SyntaxColors = code_role_colors_of(aProfile.palette());
		// The review panes' code font and tab width, so the file reads as it does there.
		if (const auto width = aProfile.typography().Code.TabWidth; width != TabWidth && width > 0U) {
			TabWidth = width;
			show_source();
		}
		set_code_font(aProfile.code_font());
		apply_source_syntax();
		Composer->adopt_appearance(aProfile);
	}

	auto review_cycle_view::adopt_source_syntax(const std::string_view aDocument, application::shared_token_spans aSpans) -> bool
	{
		const auto& source = Projection.Source;
		if (!aSpans || !source || ReadingPage != Source || std::string_view{ reinterpret_cast<const char*>(source->Bytes.data()), source->Bytes.size() } != aDocument) {
			return false;
		}
		SourceSyntax = classify_text_lines(source->Bytes, std::move(aSpans));
		apply_source_syntax();
		return true;
	}

	void review_cycle_view::apply_source_syntax()
	{
		// While a suggestion is edited, the composer colours the text it holds.
		if (!SourceSyntax.Spans || !Projection.Source || suggesting()) {
			return;
		}
		Source->set_syntax(suggestion_syntax_lines(SourceSyntax, bytes_text(Projection.Source->Bytes), SyntaxColors));
	}

	void review_cycle_view::show_source()
	{
		if (suggesting() || !Projection.Source || ReadingPage != Source) {
			return;
		}
		// The same line stays at the top, and the caret where it was.
		const auto top                   = Source->first_visible_line();
		const auto [selected, selection] = Source->selected_bytes();
		const auto bytes                 = bytes_text(Projection.Source->Bytes);
		const auto same                  = Source->text() == bytes;
		Source->set_text(bytes, TabWidth);
		if (same) {
			Source->select_bytes(selected, selection);
		}
		Source->scroll_to_line(top);
	}

	void review_cycle_view::present(const application::review_cycle_workbench_projection& aProjection)
	{
		Presenting          = true;
		const auto previous = std::move(Projection);
		Projection          = aProjection;

		// The review bar and the Updates line.
		const auto latest = Projection.Revisions.empty() ? std::nullopt : std::optional{ Projection.Revisions.back() };
		if (Projection.Comparison) {
			const auto& shown = *Projection.Comparison;
			RevisionLabel->setText(QStringLiteral("%1 · %2%3").arg(request_text(shown.Request), pair_text(shown.Pair), shown.Pinned ? QStringLiteral(" (pinned)") : QString{ }));
		} else if (Projection.Source) {
			RevisionLabel->setText(QStringLiteral("Full source · %1").arg(revision_text(Projection.Source->Revision)));
		} else {
			RevisionLabel->setText(latest ? QStringLiteral("Full source · %1").arg(revision_text(*latest)) : QStringLiteral("Not captured yet"));
		}
		auto updates = QStringList{ };
		if (Projection.History && !Projection.History->Entries.empty()) {
			for (const auto& entry : Projection.History->Entries) {
				updates.append(step_text(entry));
			}
		} else {
			for (const auto revision : Projection.Revisions) {
				updates.append(revision_text(revision));
			}
		}
		auto line = updates.isEmpty() ? QStringLiteral("No revision captured") : QStringLiteral("Captured: %1").arg(updates.join(QStringLiteral(", ")));
		line += QStringLiteral(" · %1 files").arg(Projection.Files.size());
		if (Projection.FilesTruncated) {
			line += QStringLiteral(" (list shortened)");
		}
		if (Projection.Quality && Projection.Quality->Attention) {
			line += QStringLiteral(" · captured with %1 retries, %2 unavailable").arg(Projection.Quality->Retries).arg(Projection.Quality->Unavailable);
			UpdatesToggle->set_history_marked(true);
		} else {
			UpdatesToggle->set_history_marked(false);
		}
		if (Projection.Facts.LastReview) {
			line += QStringLiteral(" · reviewed through %1").arg(revision_text(Projection.Facts.LastReview->Revision));
		}
		if (!Projection.Notice.empty()) {
			line += QStringLiteral(" · ") + text_of(Projection.Notice);
		}
		if (Projection.NextRound) {
			const auto& next = *Projection.NextRound;
			line += QStringLiteral(" · next round draft saved from F%1: %2 carried, %3 new, %4 resolved left out").arg(next.From.value()).arg(next.Carried).arg(next.Added).arg(next.Resolved);
		}
		if (!Projection.Message.empty()) {
			line += QStringLiteral(" · ") + text_of(Projection.Message);
		}
		UpdatesLine->setText(line);
		RefreshAction->setVisible(Projection.NewerAvailable && !(Projection.Comparison && Projection.Comparison->Pinned));

		// The files drawer, rebuilt only when the list changed.
		if (previous.Files != Projection.Files) {
			Files->clear();
			for (const auto& file : Projection.Files) {
				auto* const item = new QListWidgetItem{ text_of(file.Path) + (readable(file.Text) ? QString{ } : QStringLiteral("  (binary or unsupported)")), Files };
				item->setData(Qt::UserRole, text_of(file.Path));
			}
		}

		// Tabs: a tab on an older revision than the latest names it in its pill.
		auto tabs = std::vector<document_tab_presentation>{ };
		for (const auto& tab : Projection.Tabs) {
			const auto path     = text_of(tab.View.Path);
			const auto compared = tab.View.Comparison.has_value();
			const auto own      = compared || (latest && tab.View.Revision != *latest);
			const auto pair     = compared ? QStringLiteral("%1→%2").arg(revision_text(tab.View.Comparison->Left), revision_text(tab.View.Revision)) : revision_text(tab.View.Revision);
			const auto ready    = (Projection.Source && Projection.Source->Tab == tab.Id) || (Projection.Comparison && Projection.Comparison->Tab == tab.Id);
			tabs.push_back({
				.Id            = tab.Id,
				.Name          = path.section(QLatin1Char('/'), -1),
				.Path          = path,
				.OwnPair       = own ? pair : QString{ },
				.Pair          = pair,
				.State         = Projection.ActiveTab == tab.Id ? (ready ? core::document_tab_view_state::ready : core::document_tab_view_state::loading) : core::document_tab_view_state::idle,
				.Active        = Projection.ActiveTab == tab.Id,
				.OwnComparison = own,
			});
		}
		Tabs->present(std::move(tabs));

		// The one source pane: exact retained bytes of the active tab.
		const auto& source  = Projection.Source;
		const auto  changed = !previous.Source || !source || previous.Source->Tab != source->Tab || previous.Source->Revision != source->Revision
			|| previous.Source->Path != source->Path || previous.Source->Bytes != source->Bytes;
		if (changed) {
			SourceSyntax = { };
		}
		if (Projection.Comparison) {
			present_comparison(*Projection.Comparison, !previous.Comparison || *previous.Comparison != *Projection.Comparison);
		} else if (!source) {
			// A refusal before any cycle opened is said where the page would show the source.
			if (Projection.Cycle) {
				SourceNotice->setText(QStringLiteral("Choose a file to read its captured source."));
			} else if (!Projection.Message.empty()) {
				SourceNotice->setText(text_of(Projection.Message));
			} else {
				SourceNotice->setText(QStringLiteral("Open a project to review it locally."));
			}
			ReadingPage = SourceNotice;
		} else if (source->Status != application::review_material_status::available) {
			SourceNotice->setText(QStringLiteral("The captured bytes of %1 in %2 are not available; nothing else is shown in their place.").arg(text_of(source->Path), revision_text(source->Revision)));
			ReadingPage = SourceNotice;
		} else if (!readable(source->Text)) {
			SourceNotice->setText(QStringLiteral("%1 is binary or not UTF-8 (%2 bytes). Notes can still be made on the whole file.").arg(text_of(source->Path)).arg(source->Bytes.size()));
			ReadingPage = SourceNotice;
		} else {
			ReadingPage = Source;
			// A file being edited stays as it is; leaving the suggestion shows these bytes.
			if (changed && !suggesting()) {
				Source->set_text(bytes_text(source->Bytes), TabWidth);
				Source->scroll_to_line(source->Line);
			}
			if (changed) {
				// Plain until the classification of exactly these bytes arrives.
				if (SourceSyntaxWanted) {
					SourceSyntaxWanted({ .Path = source->Path, .Document = std::string{ source->Bytes.begin(), source->Bytes.end() } });
				}
			}
		}
		const auto shownPath = source ? std::optional{ source->Path } : Projection.Comparison ? std::optional{ Projection.Comparison->Path }
																							  : std::nullopt;
		const auto active    = shownPath ? std::ranges::find(Projection.Files, *shownPath, &application::review_cycle_file_entry::Path) : Projection.Files.end();
		FilePosition->setText(active != Projection.Files.end() ? QStringLiteral("File %1 of %2").arg(active - Projection.Files.begin() + 1).arg(Projection.Files.size()) : QStringLiteral("File"));

		// Notes: current wording, where each attaches and what newer material says.
		Notes->clear();
		for (const auto& note : Projection.Notes) {
			auto lines = QStringList{ };
			if (const auto* anchor = std::get_if<core::cycle_source_anchor>(&note.Primary)) {
				auto where = QStringLiteral("%1 · %2").arg(text_of(anchor->Path), revision_text(anchor->Revision));
				if (source && source->Revision == anchor->Revision && source->Path == anchor->Path) {
					if (const auto span = core::project_lines(source->Bytes, anchor->Start, anchor->End)) {
						where = QStringLiteral("%1 · %2 · %3").arg(text_of(anchor->Path), lines_text(*span), revision_text(anchor->Revision));
					}
				}
				lines.append(where);
			} else if (const auto* file = std::get_if<core::cycle_file_target>(&note.Primary)) {
				lines.append(QStringLiteral("%1 (whole file) · %2").arg(text_of(file->Path), revision_text(file->Revision)));
			} else if (const auto* module = std::get_if<core::cycle_module_target>(&note.Primary)) {
				lines.append(QStringLiteral("Folder %1/").arg(text_of(module->Path)));
			} else {
				lines.append(QStringLiteral("Whole cycle"));
			}
			lines.append(!note.Body.empty() ? text_of(note.Body) : note.Suggestions > 0U ? QStringLiteral("(suggestion only)")
																						 : QStringLiteral("(no text)"));
			if (const auto* anchor = std::get_if<core::cycle_source_anchor>(&note.Primary); anchor != nullptr && !anchor->Original.empty()) {
				lines.append(QStringLiteral("Original: %1").arg(text_of(anchor->Original).section(QLatin1Char('\n'), 0, 0).left(120)));
			}
			if (note.Correspondence) {
				lines.append(correspondence_text(*note.Correspondence));
			}
			if (note.Suggestions > 0U) {
				lines.append(QStringLiteral("Suggested change · %1").arg(note.Suggestions));
			}
			if (const auto author = provenance_text(note.Provenance); !author.isEmpty()) {
				lines.append(author);
			}
			if (!note.Related.empty()) {
				lines.append(QStringLiteral("Related locations · %1").arg(note.Related.size()));
			}
			if (note.Reconciliation) {
				const auto& item = *note.Reconciliation;
				if (!note.Correspondence && item.Primary && item.Primary->Kind != core::cycle_correspondence::exact_material) {
					lines.append(QStringLiteral("Original code: %1").arg(correspondence_text(*item.Primary)));
				}
				for (const auto& suggestion : item.Suggestions) {
					if (suggestion.Observation == core::cycle_application_observation::already_applied) {
						lines.append(QStringLiteral("Suggested result present in %1 (not a verification)").arg(revision_text(item.Against)));
					}
				}
				if (const auto words = status_text(item); !words.isEmpty()) {
					lines.append(words);
				}
			}
			// What recipients said: claims, never a verification.
			for (const auto& statement : note.Statements) {
				const auto said = statement.Disposition ? disposition_text(*statement.Disposition) : QStringLiteral("replied");
				lines.append(QStringLiteral("F%1 reply · %2 %3: %4").arg(statement.Round.value()).arg(sender_text(statement.From, statement.ClaimedAuthor, statement.Channel), said, first_line(statement.Body)));
				if (!statement.Evidence.empty()) {
					lines.append(QStringLiteral("Evidence reported, not run by BPR · %1").arg(statement.Evidence.size()));
				}
			}
			auto* const item = new QListWidgetItem{ lines.join(QLatin1Char('\n')), Notes };
			item->setData(Qt::AccessibleTextRole, lines.join(QStringLiteral(". ")));
		}
		// Statements about a round as a whole, and structured responses, after the notes.
		for (const auto& response : Projection.Responses) {
			auto lines = QStringList{ };
			if (response.Channel == core::cycle_response_channel::response_file) {
				lines.append(QStringLiteral("Response %1 to F%2 · %3").arg(text_of(response.ForeignId)).arg(response.Round.value()).arg(sender_text(response.From, response.ClaimedAuthor, response.Channel)));
				lines.append(QStringLiteral("Speaks to %1 item(s); items it does not name have no response").arg(response.Items));
				if (response.ClaimedResult) {
					lines.append(response.CapturedResult ? QStringLiteral("Says the result is %1 (captured)").arg(revision_text(*response.CapturedResult))
														 : QStringLiteral("Says the result is %1 (not captured here)").arg(text_of(*response.ClaimedResult)));
				}
			} else if (response.Items == 0U) {
				lines.append(QStringLiteral("F%1 reply · %2").arg(response.Round.value()).arg(sender_text(response.From, std::nullopt, response.Channel)));
				lines.append(text_of(response.Body));
			} else {
				continue;
			}
			auto* const item = new QListWidgetItem{ lines.join(QLatin1Char('\n')), Notes };
			item->setData(Qt::AccessibleTextRole, lines.join(QStringLiteral(". ")));
			item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
		}
		// A later capture never moves an open composer: it states newer material.
		if (suggesting() && latest && SuggestionSource && SuggestionSource->Revision != *latest) {
			Composer->set_source_status(suggestion_source_status::newer_available);
		}
		show_reading_page();
		refresh_actions();
		Presenting = false;
	}

	void review_cycle_view::present_comparison(const application::review_cycle_comparison_view& aComparison, const bool aChanged)
	{
		const auto path = text_of(aComparison.Path);
		if (aComparison.Status != application::review_material_status::available && aComparison.Rows.empty()) {
			SourceNotice->setText(QStringLiteral("The captured bytes of %1 in %2 are not available; nothing else is shown in their place.").arg(path, pair_text(aComparison.Pair)));
			ReadingPage = SourceNotice;
			return;
		}
		if (aComparison.File && aComparison.File->Change == core::cycle_path_change::unknown) {
			SourceNotice->setText(QStringLiteral("%1 is not known on one side of %2 (outside the scope or not captured); no change is claimed.").arg(path, pair_text(aComparison.Pair)));
			ReadingPage = SourceNotice;
			return;
		}
		if (aComparison.Rows.empty()) {
			const auto unchanged = aComparison.File && !aComparison.File->changed();
			SourceNotice->setText(unchanged ? QStringLiteral("%1 is unchanged from %2.").arg(path, pair_text(aComparison.Pair))
											: QStringLiteral("%1 has no text comparison in %2 (binary, not UTF-8 or too large).").arg(path, pair_text(aComparison.Pair)));
			ReadingPage = SourceNotice;
			return;
		}
		if (aChanged) {
			auto lines = QStringList{ };
			lines.reserve(static_cast<qsizetype>(aComparison.Rows.size()));
			auto marks = QList<QTextEdit::ExtraSelection>{ };
			for (const auto& row : aComparison.Rows) {
				const auto sign  = row.Kind == core::cycle_diff_row_kind::added ? QLatin1Char('+') : row.Kind == core::cycle_diff_row_kind::removed ? QLatin1Char('-')
																																					: QLatin1Char(' ');
				const auto left  = row.LeftLine ? QString::number(*row.LeftLine) : QString{ };
				const auto right = row.RightLine ? QString::number(*row.RightLine) : QString{ };
				lines.append(QStringLiteral("%1 %2 %3 %4").arg(left, 5).arg(right, 5).arg(sign).arg(text_of(row.Text)));
			}
			Comparison->setPlainText(lines.join(QLatin1Char('\n')));
			auto block = Comparison->document()->firstBlock();
			for (const auto& row : aComparison.Rows) {
				if (row.Kind != core::cycle_diff_row_kind::context && block.isValid()) {
					auto mark = QTextEdit::ExtraSelection{ };
					mark.format.setBackground(row.Kind == core::cycle_diff_row_kind::added ? QColor{ 0x2E, 0xA0, 0x43, 0x40 } : QColor{ 0xCF, 0x22, 0x2E, 0x40 });
					mark.format.setProperty(QTextFormat::FullWidthSelection, true);
					mark.cursor = QTextCursor{ block };
					marks.append(mark);
				}
				block = block.next();
			}
			Comparison->setExtraSelections(marks);
		}
		ReadingPage = Comparison;
	}

	void review_cycle_view::rebuild_view_menu()
	{
		ViewMenu->clear();
		const auto captured = !Projection.Revisions.empty() && Projection.ActiveTab.has_value();
		auto*      full     = ViewMenu->addAction(QStringLiteral("Full source"), this, [this] {
			if (Dispatch.FullSource) {
				Dispatch.FullSource();
			}
		});
		full->setEnabled(captured && Projection.Comparison.has_value());
		auto* const rounds = ViewMenu->addMenu(QStringLiteral("This round"));
		rounds->setEnabled(captured && !Projection.Facts.Rounds.empty());
		for (const auto& round : Projection.Facts.Rounds) {
			const auto id = round.Round;
			rounds->addAction(QStringLiteral("F%1 · since %2").arg(id.value()).arg(revision_text(round.Start)), this, [this, id] {
				if (Dispatch.Compare) {
					Dispatch.Compare(core::this_round_request{ .Round = id });
				}
			});
		}
		auto* const since = ViewMenu->addAction(Projection.Facts.LastReview ? QStringLiteral("Since my last review (%1)").arg(revision_text(Projection.Facts.LastReview->Revision)) : QStringLiteral("Since my last review"), this, [this] {
			if (Dispatch.Compare) {
				Dispatch.Compare(core::since_last_review_request{ });
			}
		});
		since->setEnabled(captured && Projection.Facts.LastReview.has_value());
		if (!Projection.Facts.LastReview) {
			since->setToolTip(QStringLiteral("Mark reviewed first; opening files never counts as a review"));
		}
		ViewMenu->addAction(QStringLiteral("All cycle changes"), this, [this] {
					if (Dispatch.Compare) {
						Dispatch.Compare(core::all_cycle_request{ });
					}
				})
			->setEnabled(captured);
		// Choose revisions: an exact pair that never moves. The most recent
		// captures are offered; each left side lists the later ones.
		auto* const chosen = ViewMenu->addMenu(QStringLiteral("Choose revisions"));
		chosen->setEnabled(captured && Projection.Revisions.size() > 1U);
		constexpr auto offered = std::size_t{ 12 };
		const auto     first   = Projection.Revisions.size() > offered ? Projection.Revisions.size() - offered : std::size_t{ 0 };
		for (auto left = first; left + 1U < Projection.Revisions.size(); ++left) {
			auto* const from = chosen->addMenu(QStringLiteral("From %1").arg(revision_text(Projection.Revisions[left])));
			for (auto right = left + 1U; right < Projection.Revisions.size(); ++right) {
				const auto request = core::chosen_pair_request{ .Left = Projection.Revisions[left], .Right = Projection.Revisions[right] };
				from->addAction(QStringLiteral("to %1").arg(revision_text(request.Right)), this, [this, request] {
					if (Dispatch.Compare) {
						Dispatch.Compare(request);
					}
				});
			}
		}
	}

	auto review_cycle_view::decision_menu(const int aRow) -> QMenu*
	{
		if (aRow < 0 || static_cast<std::size_t>(aRow) >= Projection.Notes.size() || Projection.Revisions.empty()) {
			return nullptr;
		}
		const auto item    = Projection.Notes[static_cast<std::size_t>(aRow)].Item;
		const auto checked = Projection.Comparison ? Projection.Comparison->Pair.Right.Id : Projection.Source ? Projection.Source->Revision
																											  : Projection.Revisions.back();
		auto*      menu    = new QMenu{ Notes };
		menu->setObjectName(QStringLiteral("cycleDecisionMenu"));
		menu->setAttribute(Qt::WA_DeleteOnClose);
		const auto offer = [this, menu, item](const QString& aText, const std::optional<core::cycle_reviewer_decision> aDecision) {
			menu->addAction(aText, this, [this, item, aDecision] {
				if (Dispatch.Decide) {
					Dispatch.Decide(item, aDecision);
				}
			});
		};
		offer(QStringLiteral("Verified in %1").arg(revision_text(checked)), core::cycle_reviewer_decision::verified);
		offer(QStringLiteral("Needs work"), core::cycle_reviewer_decision::needs_work);
		offer(QStringLiteral("Withdraw"), core::cycle_reviewer_decision::withdrawn);
		offer(QStringLiteral("Defer"), core::cycle_reviewer_decision::deferred);
		offer(QStringLiteral("Clear my decision"), std::nullopt);
		return menu;
	}

	void review_cycle_view::rebuild_reply_menu()
	{
		ReplyMenu->clear();
		if (Projection.Facts.Rounds.empty()) {
			ReplyMenu->addAction(QStringLiteral("No round has been frozen yet"))->setEnabled(false);
			return;
		}
		for (const auto& round : Projection.Facts.Rounds) {
			const auto id = round.Round;
			ReplyMenu->addAction(QStringLiteral("Paste a reply to F%1…").arg(id.value()), this, [this, id] { begin_reply(id); });
			ReplyMenu->addAction(QStringLiteral("Open a response file for F%1…").arg(id.value()), this, [this, id] {
				if (Dispatch.ChooseResponse) {
					Dispatch.ChooseResponse(id);
				}
			});
		}
	}

	void review_cycle_view::begin_reply(const core::cycle_round_id aRound)
	{
		ReplyRound = aRound;
		ReplyItem.reset();
		ReplyTarget->setText(QStringLiteral("Reply to F%1").arg(aRound.value()));
		const auto row = Notes->currentRow();
		if (row >= 0 && static_cast<std::size_t>(row) < Projection.Notes.size()) {
			// Kept now: a projection arriving while the reply is written resets the list.
			ReplyItem = core::cycle_wire_name(Projection.Notes[static_cast<std::size_t>(row)].Item.Id);
		}
		ReplyAbout->setEnabled(ReplyItem.has_value());
		ReplyAbout->setChecked(ReplyItem.has_value());
		ReplyAbout->setText(ReplyItem ? QStringLiteral("About the selected note (%1)").arg(text_of(*ReplyItem)) : QStringLiteral("About the selected note"));
		TrayComposer->setCurrentWidget(ReplyPage);
		ReplyEditor->setFocus(Qt::OtherFocusReason);
	}

	void review_cycle_view::save_reply()
	{
		if (!ReplyRound) {
			return;
		}
		auto reply = core::cycle_manual_reply{ .From = utf8(ReplyFrom->text()), .Body = utf8(ReplyEditor->toPlainText()) };
		if (ReplyItem && ReplyAbout->isChecked()) {
			reply.Items.push_back(*ReplyItem);
		}
		const auto round = *ReplyRound;
		ReplyEditor->clear();
		ReplyRound.reset();
		TrayComposer->setCurrentIndex(0);
		if (Dispatch.Reply) {
			Dispatch.Reply(round, std::move(reply));
		}
	}

	auto review_cycle_view::selection_anchor() const -> std::optional<core::cycle_source_anchor>
	{
		if (!Projection.Source || !Source->has_selection() || Centre->currentWidget() != Source || suggesting()) {
			return std::nullopt;
		}
		const auto [start, end] = Source->selected_bytes();
		return core::make_anchor(Projection.Source->Revision, Projection.Source->Path, Projection.Source->Bytes, start, end);
	}

	void review_cycle_view::begin_comment()
	{
		if (!Projection.Source) {
			return;
		}
		if (const auto anchor = selection_anchor()) {
			const auto span = core::project_lines(Projection.Source->Bytes, anchor->Start, anchor->End);
			PendingTarget   = *anchor;
			CommentTarget->setText(span ? QStringLiteral("%1 · %2").arg(text_of(anchor->Path), lines_text(*span)) : text_of(anchor->Path));
		} else {
			// Nothing selected: a note on the whole file, with no invented line.
			PendingTarget = core::cycle_file_target{ Projection.Source->Revision, Projection.Source->Path, core::cycle_material_digest{ core::sha256(Projection.Source->Bytes) } };
			CommentTarget->setText(QStringLiteral("%1 (whole file)").arg(text_of(Projection.Source->Path)));
		}
		TrayComposer->setCurrentIndex(1);
		CommentEditor->setFocus(Qt::OtherFocusReason);
	}

	void review_cycle_view::save_comment()
	{
		if (!PendingTarget) {
			return;
		}
		if (Dispatch.Comment) {
			Dispatch.Comment({ .Primary = *PendingTarget, .Body = utf8(CommentEditor->toPlainText()) });
		}
		CommentEditor->clear();
		PendingTarget.reset();
		TrayComposer->setCurrentIndex(0);
	}

	void review_cycle_view::begin_suggestion()
	{
		if (!SuggestAction->isEnabled() || !Projection.Source) {
			return;
		}
		// The caret is enough; the file stays where it is and becomes editable.
		const auto [start, end] = Source->selected_bytes();
		const auto& shown       = *Projection.Source;
		if (!Composer->begin({
				.Location = utf8(QStringLiteral("%1 · %2 · editing the whole file").arg(text_of(shown.Path), revision_text(shown.Revision))),
				.Path     = shown.Path,
				.File     = std::string{ shown.Bytes.begin(), shown.Bytes.end() },
				.Start    = start,
				.End      = end,
			})) {
			return;
		}
		SuggestionSource  = shown;
		const auto latest = Projection.Revisions.empty() ? std::nullopt : std::optional{ Projection.Revisions.back() };
		if (latest && *latest != shown.Revision) {
			Composer->set_source_status(suggestion_source_status::newer_available);
		}
		TrayComposer->setCurrentWidget(Composer);
		Source->setAccessibleName(QStringLiteral("Suggested code"));
		Source->setAccessibleDescription(QStringLiteral("Edit the whole file. Alt+Shift+Arrow makes a box selection; Ctrl+Tab leaves the editor."));
		show_reading_page();
		Source->setFocus(Qt::OtherFocusReason);
		refresh_actions();
	}

	void review_cycle_view::end_suggestion()
	{
		if (!SuggestionSource) {
			return;
		}
		SuggestionSource.reset();
		read_source();
		show_source();
		apply_source_syntax();
		show_reading_page();
		refresh_actions();
		if (Centre->currentWidget() == Source) {
			Source->setFocus(Qt::OtherFocusReason);
		}
	}

	auto review_cycle_view::suggesting() const -> bool
	{
		return TrayComposer->currentWidget() == Composer && SuggestionSource.has_value();
	}

	void review_cycle_view::show_reading_page()
	{
		Centre->setCurrentWidget(suggesting() ? Source : ReadingPage);
	}

	void review_cycle_view::read_source()
	{
		Source->set_read_only(true);
		Source->setAccessibleName(QStringLiteral("Source"));
		Source->setAccessibleDescription(QStringLiteral("The captured file. Select text to comment on it, or Suggest change to edit it here."));
	}

	auto review_cycle_view::source_menu(const QPoint& aAt) -> QMenu*
	{
		if (Centre->currentWidget() != Source) {
			return nullptr;
		}
		// A right click outside the selection moves the caret, as a click would.
		const auto point      = Source->point_at(aAt);
		const auto clicked    = Source->model().byte_offset_of(point);
		const auto [from, to] = Source->selected_bytes();
		if (from == to || clicked < from || clicked > to) {
			Source->apply(core::place_caret{ .At = point });
		}
		refresh_actions();
		auto* const menu = Source->standard_menu(this);
		menu->setObjectName(QStringLiteral("cycleSourceMenu"));
		menu->setAttribute(Qt::WA_DeleteOnClose);
		if (!suggesting()) {
			const auto first = menu->actions().isEmpty() ? nullptr : menu->actions().constFirst();
			menu->insertAction(first, CommentAction);
			menu->insertAction(first, SuggestAction);
			menu->insertSeparator(first);
		}
		return menu;
	}

	void review_cycle_view::refresh_actions()
	{
		const auto open     = Projection.Cycle.has_value();
		const auto shown    = Projection.Source && Projection.Source->Status == application::review_material_status::available;
		const auto reading  = Centre->currentWidget() == Source && !suggesting();
		const auto selected = Source->has_selection() && reading;
		const auto editable = shown && Projection.Source->Bytes.size() <= core::maximum_suggestion_editor_bytes;
		CommentAction->setEnabled(open && shown && !suggesting());
		SuggestAction->setEnabled(open && reading && editable && !suggesting());
		SuggestAction->setToolTip(shown && !editable ? QStringLiteral("This file is too large to edit here; comment on it instead")
													 : QStringLiteral("Edit this file in place, starting at the cursor, and propose the changes as exact replacements"));
		const auto active = Projection.Source ? std::ranges::find(Projection.Files, Projection.Source->Path, &application::review_cycle_file_entry::Path) : Projection.Files.end();
		PreviousFile->setEnabled(active != Projection.Files.end() && active != Projection.Files.begin());
		NextFile->setEnabled(active != Projection.Files.end() && active + 1 != Projection.Files.end());
		const auto row = Notes->currentRow();
		RelateButton->setEnabled(open && shown && selected && row >= 0 && static_cast<std::size_t>(row) < Projection.Notes.size());
		const auto bound = open && Projection.Activity == application::review_cycle_activity::idle;
		CaptureAction->setEnabled(bound);
		CaptureAgain->setEnabled(bound && !Projection.Revisions.empty());
		MarkReviewed->setEnabled(open && !Projection.Revisions.empty());
		ViewButton->setEnabled(open && !Projection.Revisions.empty());
		ReplyButton->setEnabled(open);
		NextRound->setEnabled(open && !Projection.Facts.Rounds.empty() && Projection.Activity == application::review_cycle_activity::idle);
	}

	void review_cycle_view::step_file(const int aDirection)
	{
		if (!Projection.Source || !Dispatch.OpenFile) {
			return;
		}
		const auto active = std::ranges::find(Projection.Files, Projection.Source->Path, &application::review_cycle_file_entry::Path);
		if (active == Projection.Files.end()) {
			return;
		}
		const auto index = (active - Projection.Files.begin()) + aDirection;
		if (index >= 0 && index < static_cast<std::ptrdiff_t>(Projection.Files.size())) {
			// A copy: the dispatch can present a new projection before it returns.
			const auto path = Projection.Files[static_cast<std::size_t>(index)].Path;
			Dispatch.OpenFile(path, false);
		}
	}

	auto review_cycle_view::review_bar() const noexcept -> workbench_strip*
	{
		return ReviewBar;
	}
	auto review_cycle_view::tabs() const noexcept -> document_tab_strip*
	{
		return Tabs;
	}
	auto review_cycle_view::source() const noexcept -> suggestion_editor*
	{
		return Source;
	}
	auto review_cycle_view::files() const noexcept -> QListWidget*
	{
		return Files;
	}
	auto review_cycle_view::notes() const noexcept -> QListWidget*
	{
		return Notes;
	}
	auto review_cycle_view::composer() const noexcept -> suggestion_composer*
	{
		return Composer;
	}
	auto review_cycle_view::comment_editor() const noexcept -> QPlainTextEdit*
	{
		return CommentEditor;
	}
	auto review_cycle_view::updates_line() const noexcept -> QLabel*
	{
		return UpdatesLine;
	}
	auto review_cycle_view::comment_action() const noexcept -> QAction*
	{
		return CommentAction;
	}
	auto review_cycle_view::suggest_action() const noexcept -> QAction*
	{
		return SuggestAction;
	}
	auto review_cycle_view::compose_action() const noexcept -> QAction*
	{
		return ComposeAction;
	}
	auto review_cycle_view::updates_toggle() const noexcept -> updates_disclosure*
	{
		return UpdatesToggle;
	}
	auto review_cycle_view::relate_button() const noexcept -> QPushButton*
	{
		return RelateButton;
	}
	auto review_cycle_view::capture_action() const noexcept -> QAction*
	{
		return CaptureAction;
	}
	auto review_cycle_view::capture_again_action() const noexcept -> QAction*
	{
		return CaptureAgain;
	}
	auto review_cycle_view::view_button() const noexcept -> QToolButton*
	{
		return ViewButton;
	}
	auto review_cycle_view::mark_reviewed_action() const noexcept -> QAction*
	{
		return MarkReviewed;
	}
	auto review_cycle_view::refresh_action() const noexcept -> QAction*
	{
		return RefreshAction;
	}
	auto review_cycle_view::comparison() const noexcept -> QPlainTextEdit*
	{
		return Comparison;
	}
	auto review_cycle_view::next_round_button() const noexcept -> QToolButton*
	{
		return NextRound;
	}
	auto review_cycle_view::reply_button() const noexcept -> QToolButton*
	{
		return ReplyButton;
	}
	auto review_cycle_view::reply_editor() const noexcept -> QPlainTextEdit*
	{
		return ReplyEditor;
	}
	auto review_cycle_view::reply_from() const noexcept -> QLineEdit*
	{
		return ReplyFrom;
	}
	auto review_cycle_view::reply_about_note() const noexcept -> QCheckBox*
	{
		return ReplyAbout;
	}
	auto review_cycle_view::projection() const noexcept -> const application::review_cycle_workbench_projection&
	{
		return Projection;
	}
}
