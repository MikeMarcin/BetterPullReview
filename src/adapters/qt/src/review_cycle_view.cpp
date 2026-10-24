#include "bpr/qt/review_cycle_view.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include "bpr/core/sha256.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/keyboard_focus_ring.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextLayout>
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

		/// [[BPR-1184]]. A drawer row that lists the previous (-1) or next (1) files.
		constexpr int file_window_role = Qt::UserRole + 1;

		auto readable(const core::material_text_kind aKind) -> bool
		{
			return aKind == core::material_text_kind::utf8 || aKind == core::material_text_kind::utf8_bom || aKind == core::material_text_kind::empty;
		}

		/// [[BPR-1184]]. A count as a reader reads it: 100,001.
		auto count_text(const std::uint64_t aCount) -> QString
		{
			auto text = QString::number(aCount);
			for (auto at = text.size() - 3; at > 0; at -= 3) {
				text.insert(at, QLatin1Char(','));
			}
			return text;
		}

		/// [[BPR-1184]]. How far the running capture has got.
		auto capturing_text(const application::cycle_capture_progress& aProgress) -> QString
		{
			const auto again = aProgress.Attempt > 1 ? QStringLiteral(" (attempt %1: files changed while they were read)").arg(aProgress.Attempt) : QString{ };
			switch (aProgress.Phase) {
			case application::cycle_capture_progress::phase::reading:   return QStringLiteral("Capturing: %1 files read%2").arg(count_text(aProgress.Files), again);
			case application::cycle_capture_progress::phase::checking:  return QStringLiteral("Capturing: %1 files checked again%2").arg(count_text(aProgress.Files), again);
			case application::cycle_capture_progress::phase::recording: return QStringLiteral("Capturing: recording %1 files%2").arg(count_text(aProgress.Files), again);
			}
			return QStringLiteral("Capturing");
		}

		/// [[BPR-1184]]. Bytes a capture did not keep, and why.
		auto not_kept_text(const QString& aPath, const QString& aWhere, const application::review_material_status aStatus) -> QString
		{
			static_assert(core::maximum_captured_file_bytes == 128U * 1024U * 1024U, "the words name 128 MiB");
			if (aStatus == application::review_material_status::too_large) {
				return QStringLiteral("%1 was not captured in %2: it is larger than the 128 MiB one capture keeps of a file, so there are no bytes to show. A note can still be made on the whole file.")
					.arg(aPath, aWhere);
			}
			return QStringLiteral("The captured bytes of %1 in %2 are not available; nothing else is shown in their place.").arg(aPath, aWhere);
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
			switch (aChannel) {
			case core::cycle_response_channel::response_file:   return who + QStringLiteral(", response file");
			case core::cycle_response_channel::local_interface: return who + QStringLiteral(", appended by a granted harness");
			case core::cycle_response_channel::agent_adapter:   return who + QStringLiteral(", returned to Send to Agent");
			case core::cycle_response_channel::manual_entry:    break;
			}
			return who + QStringLiteral(", entered by hand");
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

		/// \brief [[BPR-1182]]. The first line of anchored code as it is
		/// written, without its line ending: in a CRLF file the CR belongs to
		/// the ending, not to the code.
		auto first_code_line(const std::string& aCode) -> QString
		{
			auto line = text_of(aCode).section(QLatin1Char('\n'), 0, 0);
			if (line.endsWith(QLatin1Char('\r'))) {
				line.chop(1);
			}
			return line.left(120);
		}

		/// \brief [[BPR-1157]]. The key that notes the caret's line in the code
		/// view, with Shift the one that begins Suggest change there: Return as
		/// macOS names it, Enter as Windows and Linux do. The code view takes
		/// the key itself (`suggestion_editor::Activated`), so this sequence
		/// only labels menus.
		auto note_key(const bool aShift) -> QKeySequence
		{
			return shortcut_key(aShift ? shortcut_command::cycle_suggest : shortcut_command::cycle_note);
		}

		/// The same key in words, for a tooltip or an accessible description.
		auto note_key_words(const bool aShift) -> QString
		{
#if defined(Q_OS_MACOS)
			return aShift ? QStringLiteral("Shift+Return") : QStringLiteral("Return");
#else
			return aShift ? QStringLiteral("Shift+Enter") : QStringLiteral("Enter");
#endif
		}

		auto comment_tip() -> QString
		{
			return QStringLiteral("Comment on the selection, or on the file when nothing is selected. In the code, %1 comments on the selection or the caret's line").arg(note_key_words(false));
		}

		auto suggest_tip() -> QString
		{
			return QStringLiteral("Edit this file in place, starting at the cursor, and propose the changes as exact replacements (%1 in the code)").arg(note_key_words(true));
		}

		/// \brief [[BPR-1157]]. The keys that cancel an open note or suggestion
		/// (owner, native-keyboard v002, 2026-10-02): Escape everywhere, and on
		/// macOS Control-C as well. Qt names the Mac's Control key Meta, so
		/// Command-C, Qt's Control, stays Copy. On Windows and Linux Ctrl+C is
		/// Copy, so it never cancels there.
		auto composer_cancel_keys() -> QList<QKeySequence>
		{
			return shortcut_keys(shortcut_command::cycle_cancel);
		}

		/// The same keys in words, for tooltips and accessible descriptions.
		auto cancel_key_words() -> QString
		{
#if defined(Q_OS_MACOS)
			return QStringLiteral("Esc or Control-C");
#else
			return QStringLiteral("Esc");
#endif
		}

		/// \brief [[BPR-1280]]. The key that saves the open note, reply or
		/// suggestion (owner, 2026-10-04: "I want cmd+return to save the
		/// note"): Command-Return on macOS, Control-Enter on Windows and
		/// Linux. Qt names the main key Return and the keypad's Enter, so
		/// both are bound; Qt's Control is the Mac's Command. Return alone
		/// keeps its meaning: a new line while writing, a note on the
		/// caret's line in the read-only code.
		auto composer_save_keys() -> QList<QKeySequence>
		{
			return shortcut_keys(shortcut_command::cycle_save);
		}

		/// The save key as the platform writes it: ⌘↩ on macOS, Ctrl+Enter elsewhere.
		auto save_key_words() -> QString
		{
#if defined(Q_OS_MACOS)
			// The Mac's own Return glyph, as its menus write it; Qt writes ↵.
			return QString::fromUtf8("\xE2\x8C\x98\xE2\x86\xA9");
#else
			return QKeySequence{ Qt::CTRL | Qt::Key_Enter }.toString(QKeySequence::NativeText);
#endif
		}

		/// A save button names its key, so the key is found without a manual.
		void label_save_button(QPushButton& aButton, const QString& aWords, const QString& aTip)
		{
			aButton.setText(QStringLiteral("%1  %2").arg(aWords, save_key_words()));
			aButton.setToolTip(QStringLiteral("%1 (%2)").arg(aTip, save_key_words()));
			aButton.setAccessibleName(aWords);
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
		// BPR-1163: which material the active tab shows, chosen here.
		// [[BPR-1182]]. One button names the view shown and opens the chooser,
		// as the pull request's comparison does in the same slot (RFC-BPR-0052).
		ViewButton = new QToolButton{ ReviewBar };
		ViewButton->setObjectName(QStringLiteral("cycleViewButton"));
		ViewButton->setText(RevisionWhole);
		ViewButton->setAccessibleName(QStringLiteral("View"));
		ViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		ViewButton->setPopupMode(QToolButton::InstantPopup);
		ViewButton->setAutoRaise(true);
		ViewMenu = new QMenu{ ViewButton };
		ViewMenu->setObjectName(QStringLiteral("cycleViewMenu"));
		ViewButton->setMenu(ViewMenu);
		ReviewBar->addWidget(ViewButton);
		// [[BPR-1182]]. The pull request's refresh glyph, named in its tooltip.
		RefreshAction = new QAction{ strip_icon(strip_glyph::refresh, palette()), QStringLiteral("Show newer"), this };
		RefreshAction->setObjectName(QStringLiteral("cycleRefreshAction"));
		RefreshAction->setToolTip(QStringLiteral("Show newer: a newer capture exists; move this view to it"));
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
		// [[BPR-1279]]. Rebind Folder: in the menu always, and on the bar beside
		// Capture revision while a refused capture offers it.
		RebindMenu = new QAction{ QStringLiteral("Rebind Folder…"), this };
		RebindMenu->setObjectName(QStringLiteral("cycleRebindMenuAction"));
		captureMenu->addAction(RebindMenu);
		ReviewBar->addAction(CaptureAction);
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(CaptureAction))) {
			button->setPopupMode(QToolButton::MenuButtonPopup);
		}
		RebindAction = new QAction{ QStringLiteral("Rebind Folder…"), this };
		RebindAction->setObjectName(QStringLiteral("cycleRebindAction"));
		RebindAction->setVisible(false);
		ReviewBar->addAction(RebindAction);
		for (auto* const action : { RebindAction, RebindMenu }) {
			action->setToolTip(QStringLiteral("Choose where this cycle's folder is now, or a replacement; nothing is bound until you confirm what is there"));
		}
		RebindDialog = new QDialog{ this };
		RebindDialog->setObjectName(QStringLiteral("cycleRebindDialog"));
		RebindDialog->setWindowTitle(QStringLiteral("Rebind Folder"));
		RebindDialog->setWindowModality(Qt::WindowModal);
		RebindDialog->setModal(true);
		auto* const rebindLayout = new QVBoxLayout{ RebindDialog };
		RebindWords              = new QLabel{ RebindDialog };
		RebindWords->setObjectName(QStringLiteral("cycleRebindWords"));
		RebindWords->setTextFormat(Qt::PlainText);
		RebindWords->setWordWrap(true);
		RebindWords->setTextInteractionFlags(Qt::TextSelectableByMouse);
		RebindWords->setMinimumWidth(RebindWords->fontMetrics().averageCharWidth() * 56);
		rebindLayout->addWidget(RebindWords);
		auto* const rebindButtons = new QDialogButtonBox{ RebindDialog };
		RebindConfirm             = rebindButtons->addButton(QStringLiteral("Rebind"), QDialogButtonBox::AcceptRole);
		RebindConfirm->setObjectName(QStringLiteral("cycleRebindConfirm"));
		auto* const rebindCancel = rebindButtons->addButton(QStringLiteral("Cancel"), QDialogButtonBox::RejectRole);
		rebindCancel->setObjectName(QStringLiteral("cycleRebindCancel"));
		rebindLayout->addWidget(rebindButtons);
		// Rebind confirms the check shown and leaves the dialog to the host;
		// Cancel and Escape leave it, binding nothing.
		connect(RebindConfirm, &QPushButton::clicked, this, [this] {
			if (Dispatch.ConfirmRebind) {
				Dispatch.ConfirmRebind();
			}
		});
		connect(rebindButtons, &QDialogButtonBox::rejected, RebindDialog, &QDialog::reject);
		connect(RebindDialog, &QDialog::rejected, this, [this] {
			if (!Presenting && Dispatch.CancelRebind) {
				Dispatch.CancelRebind();
			}
		});
		// [[BPR-1182]]. The pull request's private-progress tick, named in its tooltip.
		MarkReviewed = new QAction{ strip_icon(strip_glyph::private_progress, palette()), QStringLiteral("Mark reviewed"), this };
		MarkReviewed->setObjectName(QStringLiteral("cycleMarkReviewed"));
		MarkReviewed->setToolTip(QStringLiteral("Mark reviewed: record that you reviewed through the revision shown; \"Since my last review\" starts here"));
		ReviewBar->addAction(MarkReviewed);
		CommentAction = new QAction{ strip_icon(strip_glyph::comment, palette()), QStringLiteral("Comment"), this };
		CommentAction->setObjectName(QStringLiteral("cycleCommentAction"));
		CommentAction->setToolTip(comment_tip());
		ReviewBar->addAction(CommentAction);
		SuggestAction = new QAction{ QStringLiteral("Suggest change"), this };
		SuggestAction->setObjectName(QStringLiteral("cycleSuggestAction"));
		SuggestAction->setToolTip(suggest_tip());
		ReviewBar->addAction(SuggestAction);
		// BPR-1161: labelled, with the number of open notes, where the pencil was.
		ComposeAction = new QAction{ QStringLiteral("Share notes"), this };
		ComposeAction->setObjectName(QStringLiteral("cycleComposeAction"));
		ComposeAction->setToolTip(QStringLiteral("Share your notes as one round: copy them for an agent or a colleague, or save them to a file"));
		// BPR-1177: the finished result goes out as a pull request from the same
		// place, in its menu, so the bar gains no row and no button.
		PublishAction = new QAction{ QStringLiteral("Publish as Pull Request…"), this };
		PublishAction->setObjectName(QStringLiteral("cyclePublishAction"));
		PublishAction->setToolTip(QStringLiteral("Publish the latest capture as one new commit on a new branch, and open a pull request"));
		auto* const shareMenu = new QMenu{ this };
		shareMenu->setObjectName(QStringLiteral("cycleShareMenu"));
		shareMenu->addAction(PublishAction);
		// BPR-1273: once the cycle has a pull request, its notes can join that pull request's tour.
		PromoteAction = new QAction{ QStringLiteral("Add Notes to Review Tour…"), this };
		PromoteAction->setObjectName(QStringLiteral("cyclePromoteAction"));
		PromoteAction->setToolTip(QStringLiteral("Add chosen notes, in their own words, as stops of the linked pull request's Review Tour; nothing is sent to GitHub"));
		shareMenu->addAction(PromoteAction);
		ComposeAction->setMenu(shareMenu);
		ReviewBar->addAction(ComposeAction);
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(ComposeAction))) {
			button->setPopupMode(QToolButton::MenuButtonPopup);
		}
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
		read_source();
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
		// [[BPR-1281]]. The rows scrolled into view take their colours.
		connect(Comparison, &QPlainTextEdit::updateRequest, this, [this] { paint_comparison_syntax(); });
		Centre->addWidget(Source);
		Centre->addWidget(SourceNotice);
		Centre->addWidget(Comparison);
		// BPR-1161: Share notes is a sheet over the code (direction A), not a page in its place.
		SharePage    = new review_cycle_share_sheet{ this };
		PublishSheet = new review_cycle_publish_sheet{ this };
		PromoteSheet = new review_cycle_promote_sheet{ this };
		ReadingPage  = Source;

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
		// [[BPR-1274]]. A long round's notes are laid out a batch a turn, so a
		// change to one never holds the window for all of them.
		Notes->setLayoutMode(QListView::Batched);
		Notes->setBatchSize(64);
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
		RelateButton = new quiet_push_button{ QStringLiteral("Link selection to note"), tray };
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
		auto* const cancelComment = new quiet_push_button{ QStringLiteral("Cancel"), commentPage };
		cancelComment->setObjectName(QStringLiteral("cycleCommentCancel"));
		cancelComment->setToolTip(QStringLiteral("Discard this note (%1)").arg(cancel_key_words()));
		auto* const saveComment = new QPushButton{ commentPage };
		saveComment->setObjectName(QStringLiteral("cycleCommentSave"));
		saveComment->setDefault(true);
		label_save_button(*saveComment, QStringLiteral("Save note"), QStringLiteral("Save this note"));
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
		auto* const cancelReply = new quiet_push_button{ QStringLiteral("Cancel"), ReplyPage };
		cancelReply->setObjectName(QStringLiteral("cycleReplyCancel"));
		auto* const saveReply = new QPushButton{ ReplyPage };
		saveReply->setObjectName(QStringLiteral("cycleReplySave"));
		label_save_button(*saveReply, QStringLiteral("Save reply"), QStringLiteral("Keep this reply with its round"));
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
		Composer->cancel_button()->setToolTip(QStringLiteral("Discard this suggestion (%1)").arg(cancel_key_words()));
		label_save_button(*Composer->save_button(), QStringLiteral("Save suggestion"), QStringLiteral("Save these changes as a suggestion"));
		// [[BPR-1157]]. Escape, and Control-C on macOS, cancel the open note or
		// suggestion as its Cancel does, while focus is inside it. The keys
		// are claimed only while one is open; the code view keeps Escape
		// while it has a selection or extra carets to collapse.
		CancelKeys = new QShortcut{ this };
		CancelKeys->setObjectName(QStringLiteral("cycleComposerCancelKeys"));
		CancelKeys->setKeys(composer_cancel_keys());
		CancelKeys->setContext(Qt::WidgetWithChildrenShortcut);
		CancelKeys->setEnabled(false);
		connect(CancelKeys, &QShortcut::activated, this, [this] { cancel_focused_composer(); });
		// [[BPR-1280]]. Command-Return (Control-Enter) saves the open note,
		// reply or suggestion as its Save does, while focus is inside it.
		SaveKeys = new QShortcut{ this };
		SaveKeys->setObjectName(QStringLiteral("cycleComposerSaveKeys"));
		SaveKeys->setKeys(composer_save_keys());
		SaveKeys->setContext(Qt::WidgetWithChildrenShortcut);
		SaveKeys->setEnabled(false);
		connect(SaveKeys, &QShortcut::activated, this, [this] { save_focused_composer(); });
		// Leaving the composer, however it is left, gives the pane back to reading.
		connect(TrayComposer, &QStackedWidget::currentChanged, this, [this] {
			if (!suggesting()) {
				end_suggestion();
			}
			refresh_cancel_keys();
		});
		connect(Centre, &QStackedWidget::currentChanged, this, [this] { refresh_cancel_keys(); });

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
			// [[BPR-1184]]. While a capture runs the same button cancels it.
			if (Projection.Capturing) {
				if (Dispatch.CancelCapture) {
					Dispatch.CancelCapture();
				}
			} else if (Dispatch.Capture) {
				Dispatch.Capture(false);
			}
		});
		connect(CaptureAgain, &QAction::triggered, this, [this] {
			if (Dispatch.Capture) {
				Dispatch.Capture(true);
			}
		});
		for (auto* const action : { RebindAction, RebindMenu }) {
			connect(action, &QAction::triggered, this, [this] {
				if (Dispatch.ChooseRebind) {
					Dispatch.ChooseRebind();
				}
			});
		}
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
		connect(PublishAction, &QAction::triggered, this, [this] {
			if (Dispatch.OpenPublication && !Projection.Publication) {
				Dispatch.OpenPublication();
			}
		});
		connect(PromoteAction, &QAction::triggered, this, [this] {
			if (Dispatch.OpenPromotion && !Promoting) {
				Dispatch.OpenPromotion();
			}
		});
		connect(ComposeAction, &QAction::triggered, this, [this] {
			if (Dispatch.Compose && !Projection.Share) {
				Dispatch.Compose();
			}
		});
		connect(SuggestAction, &QAction::triggered, this, [this] { begin_suggestion(); });
		connect(cycleNote, &QToolButton::clicked, this, [this] {
			PendingTarget = core::cycle_whole_target{ };
			CommentTarget->setText(QStringLiteral("Whole cycle"));
			TrayComposer->setCurrentIndex(1);
			CommentEditor->setFocus(Qt::OtherFocusReason);
		});
		connect(saveComment, &QPushButton::clicked, this, [this] { save_comment(); });
		connect(cancelComment, &QPushButton::clicked, this, [this] { cancel_comment(); });
		Source->SelectionChanged = [this] { refresh_actions(); };
		// [[BPR-1157]]. Reading, Return notes the selection or the caret's line
		// and Shift+Return begins Suggest change there (owner, native-keyboard
		// v001, 2026-10-02). Editing, the code view keeps Return for itself.
		Source->Activated = [this](const bool aShift) {
			if (aShift) {
				if (!SuggestAction->isEnabled()) {
					return false;
				}
				begin_suggestion();
				return true;
			}
			if (!CommentAction->isEnabled()) {
				return false;
			}
			comment_here();
			return true;
		};
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
			// [[BPR-1184]]. The drawer's rows for the previous and next files.
			if (aItem != nullptr && aItem->data(file_window_role).isValid()) {
				if (Dispatch.ListFiles) {
					Dispatch.ListFiles(aItem->data(file_window_role).toInt(), false);
				}
				return;
			}
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
		// [[BPR-1157]]. Every stop Tab reaches on the page shows the window's
		// focus ring; the review bar's controls draw it themselves.
		ring_keyboard_focus(*this, QStringLiteral("cycleKeyboardFocus"), ReviewBar);
		refresh_actions();
	}

	void review_cycle_view::set_dispatch(review_cycle_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
		SharePage->set_dispatch(Dispatch.Share);
		PublishSheet->set_dispatch(Dispatch.Publish);
		PromoteSheet->set_dispatch(Dispatch.Promote);
	}

	void review_cycle_view::set_code_font(const QFont& aFont)
	{
		const auto tab = QFontMetricsF{ aFont }.horizontalAdvance(QLatin1Char(' ')) * TabWidth;
		Comparison->setFont(aFont);
		Comparison->setTabStopDistance(tab);
		Composer->set_code_font(aFont);
		SharePage->set_code_font(aFont, TabWidth);
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
		Source->set_contrast_variant(aProfile.variant());
		apply_source_syntax();
		Composer->adopt_appearance(aProfile);
		SharePage->set_code_colours(SyntaxColors);
		paint_comparison();
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

	/// \brief The notes list: each note's current wording, where it attaches
	/// and what newer material and recipients say, then the replies about a
	/// round as a whole.
	void review_cycle_view::present_notes(const std::optional<application::review_cycle_source_view>& aSource)
	{
		// Each item's lines, and whether it can be chosen.
		auto shown = std::vector<std::pair<QStringList, bool>>{ };
		for (const auto& note : Projection.Notes) {
			auto lines = QStringList{ };
			if (const auto* anchor = std::get_if<core::cycle_source_anchor>(&note.Primary)) {
				auto where = QStringLiteral("%1 · %2").arg(text_of(anchor->Path), revision_text(anchor->Revision));
				if (aSource && aSource->Revision == anchor->Revision && aSource->Path == anchor->Path) {
					if (const auto span = core::project_lines(aSource->Bytes, anchor->Start, anchor->End)) {
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
				lines.append(QStringLiteral("Original: %1").arg(first_code_line(anchor->Original)));
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
			shown.emplace_back(std::move(lines), true);
		}
		// Statements about a round as a whole, and structured responses, after the notes.
		for (const auto& response : Projection.Responses) {
			auto lines = QStringList{ };
			if (core::structured_response(response.Channel)) {
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
			shown.emplace_back(std::move(lines), false);
		}
		// [[BPR-1274]]. The same words leave the list as it is, with its
		// selection and scroll: rebuilding it lays every item out again.
		auto same = static_cast<qsizetype>(shown.size()) == Notes->count();
		for (auto row = 0; same && row < Notes->count(); ++row) {
			const auto* const item  = Notes->item(row);
			const auto&       entry = shown[static_cast<std::size_t>(row)];
			same                    = item->text() == entry.first.join(QLatin1Char('\n')) && item->flags().testFlag(Qt::ItemIsSelectable) == entry.second;
		}
		if (same) {
			return;
		}
		Notes->clear();
		for (const auto& [lines, selectable] : shown) {
			auto* const item = new QListWidgetItem{ lines.join(QLatin1Char('\n')), Notes };
			item->setData(Qt::AccessibleTextRole, lines.join(QStringLiteral(". ")));
			if (!selectable) {
				item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
			}
		}
	}

	void review_cycle_view::present(const application::review_cycle_workbench_projection& aProjection)
	{
		Presenting          = true;
		const auto previous = std::move(Projection);
		Projection          = aProjection;

		// The review bar and the Updates line.
		const auto latest = Projection.Revisions.empty() ? std::nullopt : std::optional{ Projection.Revisions.back() };
		// [[BPR-1182]]. Named whole; the revisions yield first at a narrow width.
		if (Projection.Comparison) {
			const auto& shown = *Projection.Comparison;
			const auto  state = shown.Pinned ? QStringLiteral(" (pinned)") : QString{ };
			RevisionWhole     = QStringLiteral("%1 · %2%3").arg(request_text(shown.Request), pair_text(shown.Pair), state);
			RevisionName      = request_text(shown.Request) + state;
		} else if (Projection.Source || latest) {
			RevisionWhole = QStringLiteral("Full source · %1").arg(revision_text(Projection.Source ? Projection.Source->Revision : *latest));
			RevisionName  = QStringLiteral("Full source");
		} else {
			RevisionWhole = QStringLiteral("Not captured yet");
			RevisionName  = RevisionWhole;
		}
		ViewButton->setAccessibleDescription(RevisionWhole);
		ViewButton->setToolTip(QStringLiteral("%1\nChoose full source, this round, since my last review, all cycle changes or chosen revisions").arg(RevisionWhole));
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
		if (Projection.FilesTruncated || Projection.FilesEarlier) {
			// [[BPR-1184]]. A large capture is listed a window at a time, and says so.
			const auto last = Projection.FilesFirst + Projection.Files.size() - 1U;
			line += QStringLiteral(" · files %1–%2 listed of %3 captured entries").arg(count_text(Projection.FilesFirst), count_text(last), count_text(Projection.CapturedEntries));
		} else {
			line += QStringLiteral(" · %1 files").arg(Projection.Files.size());
		}
		if (Projection.Capturing) {
			line += QStringLiteral(" · ") + capturing_text(*Projection.Capturing);
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
		// BPR-1177: the pull request the cycle became, as GitHub last said; the history stays here.
		for (const auto& link : Projection.Links) {
			line += QStringLiteral(" · %1 pull request #%2 (%3/%4)")
						.arg(link.Origin == core::cycle_pull_origin::published ? QStringLiteral("published as") : QStringLiteral("linked to externally published"))
						.arg(link.Address.Number)
						.arg(text_of(link.Address.Owner), text_of(link.Address.Repository));
			if (link.Drifted) {
				line += QStringLiteral(", changed on GitHub since");
			}
		}
		if (!Projection.Message.empty()) {
			line += QStringLiteral(" · ") + text_of(Projection.Message);
		}
		UpdatesLine->setText(line);
		RefreshAction->setVisible(Projection.NewerAvailable && !(Projection.Comparison && Projection.Comparison->Pinned));

		// The files drawer, rebuilt only when the list changed.
		if (previous.Files != Projection.Files || previous.FilesFirst != Projection.FilesFirst || previous.FilesTruncated != Projection.FilesTruncated
			|| previous.FilesEarlier != Projection.FilesEarlier) {
			Files->clear();
			if (Projection.FilesEarlier) {
				auto* const earlier = new QListWidgetItem{ QStringLiteral("Previous files (before %1)").arg(count_text(Projection.FilesFirst)), Files };
				earlier->setData(file_window_role, -1);
			}
			for (const auto& file : Projection.Files) {
				const auto  kept = file.State != core::cycle_path_state::unavailable;
				auto* const item = new QListWidgetItem{ text_of(file.Path) + (!kept ? QStringLiteral("  (not captured)") : readable(file.Text) ? QString{ }
																																			   : QStringLiteral("  (binary or unsupported)")),
														Files };
				item->setData(Qt::UserRole, text_of(file.Path));
			}
			if (Projection.FilesTruncated) {
				auto* const later = new QListWidgetItem{ QStringLiteral("Next files (from %1 of %2 captured entries)").arg(count_text(Projection.FilesFirst + Projection.Files.size()), count_text(Projection.CapturedEntries)), Files };
				later->setData(file_window_role, 1);
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
			SourceNotice->setText(not_kept_text(text_of(source->Path), revision_text(source->Revision), source->Status));
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
		const auto ordinal   = active - Projection.Files.begin() + 1;
		FileWords            = active != Projection.Files.end() ? QStringLiteral("File %1 of %2").arg(ordinal).arg(Projection.Files.size()) : QStringLiteral("File");
		FileCompact          = active != Projection.Files.end() ? QStringLiteral("File %1/%2").arg(ordinal).arg(Projection.Files.size()) : QStringLiteral("File");
		FilePosition->setToolTip(FileWords);

		// Notes: current wording, where each attaches and what newer material says.
		// [[BPR-1274]]. Rebuilt only when what it lists changed: each item's
		// words depend on the notes, the replies and the source shown, and
		// rebuilding lays every item out again.
		if (changed || previous.Notes != Projection.Notes || previous.Responses != Projection.Responses) {
			present_notes(source);
		}
		// A later capture never moves an open composer: it states newer material.
		if (suggesting() && latest && SuggestionSource && SuggestionSource->Revision != *latest) {
			Composer->set_source_status(suggestion_source_status::newer_available);
		}
		if (Projection.Share) {
			SharePage->present(*Projection.Share, Projection.Notes, Projection.Activity == application::review_cycle_activity::idle);
		}
		if (Projection.Publication) {
			PublishSheet->present(*Projection.Publication, Projection);
		}
		const auto publishing = Projection.Publication.has_value() && !PublishSheet->isVisible();
		const auto published  = !Projection.Publication.has_value() && previous.Publication.has_value();
		const auto composing  = Projection.Share.has_value() && !SharePage->isVisible();
		const auto composed   = !Projection.Share.has_value() && previous.Share.has_value();
		show_reading_page();
		refresh_actions();
		// [[BPR-1182]]. What the bar holds changes only here and with the width.
		apply_review_bar_width();
		present_rebind();
		if (composing) {
			// The sheet hangs from the review bar, over the file the reviewer was reading.
			SharePage->set_anchor(ReviewBar->mapToGlobal(QPoint{ ReviewBar->width() / 2, ReviewBar->height() }));
			SharePage->show();
			SharePage->raise();
			SharePage->activateWindow();
			SharePage->focus_first();
		} else if (composed) {
			SharePage->hide();
			if (Centre->currentWidget() == Source) {
				Source->setFocus(Qt::OtherFocusReason);
			}
		}
		if (publishing) {
			// Over the file the reviewer was reading, as Share notes is.
			PublishSheet->set_anchor(ReviewBar->mapToGlobal(QPoint{ ReviewBar->width() / 2, ReviewBar->height() }));
			PublishSheet->show();
			PublishSheet->raise();
			PublishSheet->activateWindow();
			PublishSheet->focus_first();
		} else if (published) {
			PublishSheet->hide();
			if (Centre->currentWidget() == Source) {
				Source->setFocus(Qt::OtherFocusReason);
			}
		}
		Presenting = false;
	}

	void review_cycle_view::present_rebind()
	{
		if (!Projection.Rebind) {
			if (RebindDialog->isVisible()) {
				RebindDialog->hide();
			}
			return;
		}
		const auto& rebind = *Projection.Rebind;
		const auto  title  = Projection.Cycle ? text_of(Projection.Cycle->Title) : QStringLiteral("this cycle");
		auto        words  = QStringLiteral("Rebind \"%1\" to %2?").arg(title, QDir::toNativeSeparators(text_of(rebind.Location)));
		if (rebind.Checking && rebind.Words.empty()) {
			words += QStringLiteral("\n\nComparing it with the latest capture…");
		} else {
			words += QStringLiteral("\n\n") + text_of(rebind.Words);
		}
		if (rebind.Confirmable) {
			words += QStringLiteral("\n\nIts captures, notes and rounds stay as they are; the next capture reads this folder. Nothing is captured until you capture.");
		}
		RebindWords->setText(words);
		RebindWords->setAccessibleName(words);
		RebindConfirm->setEnabled(rebind.Confirmable && !rebind.Checking);
		RebindConfirm->setVisible(rebind.Confirmable || rebind.Checking);
		// The words change while it is shown: it grows to hold them whole.
		RebindDialog->adjustSize();
		if (!RebindDialog->isVisible()) {
			RebindDialog->open();
		}
	}

	void review_cycle_view::present_comparison(const application::review_cycle_comparison_view& aComparison, const bool aChanged)
	{
		const auto path = text_of(aComparison.Path);
		if (aComparison.Status != application::review_material_status::available && aComparison.Rows.empty()) {
			SourceNotice->setText(not_kept_text(path, pair_text(aComparison.Pair), aComparison.Status));
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
			// [[BPR-1281]]. Each side's text is its rows joined by LF, so a
			// row's line is the same line of the text that is classified,
			// whatever ending the file used.
			auto lines = QStringList{ };
			lines.reserve(static_cast<qsizetype>(aComparison.Rows.size()));
			ComparisonTexts  = { };
			ComparisonSyntax = { };
			ComparisonPlaces.clear();
			ComparisonPlaces.reserve(aComparison.Rows.size());
			const auto append = [this](const std::size_t aSide, const std::string& aText) -> std::uint64_t {
				auto&      text  = ComparisonTexts.at(aSide);
				const auto begin = text.size();
				text.append(aText);
				text.push_back('\n');
				return begin;
			};
			for (const auto& row : aComparison.Rows) {
				const auto sign   = row.Kind == core::cycle_diff_row_kind::added ? QLatin1Char('+') : row.Kind == core::cycle_diff_row_kind::removed ? QLatin1Char('-')
																																					 : QLatin1Char(' ');
				const auto left   = row.LeftLine ? QString::number(*row.LeftLine) : QString{ };
				const auto right  = row.RightLine ? QString::number(*row.RightLine) : QString{ };
				const auto prefix = QStringLiteral("%1 %2 %3 ").arg(left, 5).arg(right, 5).arg(sign);
				lines.append(prefix + text_of(row.Text));
				auto place = comparison_place{ .Length = row.Text.size(), .Code = static_cast<int>(prefix.size()), .Kind = row.Kind };
				if (row.Kind != core::cycle_diff_row_kind::added) {
					place.Left = append(0U, row.Text);
				}
				if (row.Kind != core::cycle_diff_row_kind::removed) {
					place.Right = append(1U, row.Text);
				}
				ComparisonPlaces.push_back(place);
			}
			Comparison->setPlainText(lines.join(QLatin1Char('\n')));
			// Plain until the classification of exactly these texts arrives.
			if (ComparisonSyntaxWanted) {
				for (std::size_t side = 0; side < ComparisonTexts.size(); ++side) {
					if (side == 0U || ComparisonTexts[1] != ComparisonTexts[0]) {
						ComparisonSyntaxWanted({ .Path = aComparison.Path, .Document = ComparisonTexts.at(side) });
					}
				}
			}
			paint_comparison();
		}
		ReadingPage = Comparison;
	}

	auto review_cycle_view::adopt_comparison_syntax(const std::string_view aDocument, const application::shared_token_spans& aSpans) -> bool
	{
		if (!aSpans || !Projection.Comparison || ComparisonPlaces.empty()) {
			return false;
		}
		auto adopted = false;
		for (std::size_t side = 0; side < ComparisonTexts.size(); ++side) {
			if (const auto& text = ComparisonTexts.at(side); text == aDocument) {
				ComparisonSyntax.at(side) = classify_text_lines(std::span{ reinterpret_cast<const std::uint8_t*>(text.data()), text.size() }, aSpans);
				adopted                   = true;
			}
		}
		if (adopted) {
			paint_comparison_syntax();
		}
		return adopted;
	}

	void review_cycle_view::paint_comparison()
	{
		// The review panes' diff fills; fixed washes before an appearance applies.
		auto marks = QList<QTextEdit::ExtraSelection>{ };
		for (std::size_t index = 0; index < ComparisonPlaces.size(); ++index) {
			const auto kind = ComparisonPlaces[index].Kind;
			if (kind == core::cycle_diff_row_kind::context) {
				continue;
			}
			const auto added = kind == core::cycle_diff_row_kind::added;
			auto       fill  = code_sample_row_fill(added ? code_sample_row_kind::added : code_sample_row_kind::removed, SyntaxColors);
			if (!fill.isValid()) {
				fill = added ? QColor{ 0x2E, 0xA0, 0x43, 0x40 } : QColor{ 0xCF, 0x22, 0x2E, 0x40 };
			}
			append_line_band(marks, *Comparison, core::line_span{ .Begin = static_cast<std::uint32_t>(index), .End = static_cast<std::uint32_t>(index + 1U) }, fill);
		}
		Comparison->setExtraSelections(marks);
		paint_comparison_syntax();
	}

	void review_cycle_view::paint_comparison_syntax()
	{
		// Only the rows in view: scrolling asks again, and a row already in
		// its colours is not laid out again.
		if (ComparisonPlaces.empty()) {
			return;
		}
		auto quiet = QTextCharFormat{ };
		quiet.setForeground(Comparison->palette().color(QPalette::PlaceholderText));
		const auto last = Comparison->cursorForPosition(QPoint{ 0, Comparison->viewport()->height() }).block();
		for (auto block = Comparison->cursorForPosition(QPoint{ 0, 0 }).block(); block.isValid(); block = block.next()) {
			const auto index = static_cast<std::size_t>(block.blockNumber());
			if (index >= ComparisonPlaces.size()) {
				break;
			}
			const auto& place   = ComparisonPlaces[index];
			auto        formats = QList<QTextLayout::FormatRange>{ QTextLayout::FormatRange{ .start = 0, .length = place.Code, .format = quiet } };
			// A removed row is the left side's, an added row the right's; an
			// unchanged row is on both and takes whichever is classified.
			const auto right = place.Right && (!place.Left || ComparisonSyntax[1].Spans);
			const auto side  = right ? std::size_t{ 1 } : std::size_t{ 0 };
			if (const auto begin = right ? place.Right : place.Left; begin && ComparisonSyntax.at(side).Spans) {
				const auto& text = ComparisonTexts.at(side);
				const auto  code = std::span{ reinterpret_cast<const std::uint8_t*>(text.data()) + *begin, static_cast<std::size_t>(place.Length) };
				for (auto range : code_syntax_format_ranges_at(ComparisonSyntax.at(side), *begin, code, SyntaxColors)) {
					range.start += place.Code;
					formats.append(std::move(range));
				}
			}
			apply_syntax_format_ranges(*Comparison->document(), block, formats);
			if (block == last) {
				break;
			}
		}
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

	void review_cycle_view::comment_here()
	{
		if (!Projection.Source || selection_anchor()) {
			begin_comment();
			return;
		}
		// A caret alone: a note on its line, never on the whole file.
		const auto& shown = *Projection.Source;
		const auto  line  = Centre->currentWidget() == Source ? core::line_text_at(shown.Bytes, Source->selected_bytes().second) : std::nullopt;
		auto        at    = line ? core::make_anchor(shown.Revision, shown.Path, shown.Bytes, line->Start, line->End) : std::nullopt;
		if (!at) {
			begin_comment();
			return;
		}
		const auto span = core::project_lines(shown.Bytes, at->Start, at->End);
		CommentTarget->setText(span ? QStringLiteral("%1 · %2").arg(text_of(at->Path), lines_text(*span)) : text_of(at->Path));
		PendingTarget = std::move(*at);
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

	void review_cycle_view::cancel_comment()
	{
		CommentEditor->clear();
		PendingTarget.reset();
		TrayComposer->setCurrentIndex(0);
		if (Centre->currentWidget() == Source) {
			Source->setFocus(Qt::OtherFocusReason);
		}
	}

	void review_cycle_view::cancel_focused_composer()
	{
		const auto* const focus  = QApplication::focusWidget();
		const auto        within = [focus](const QWidget* aRegion) { return focus != nullptr && (focus == aRegion || aRegion->isAncestorOf(focus)); };
		if (TrayComposer->currentWidget() == CommentEditor->parentWidget() && within(CommentEditor->parentWidget())) {
			cancel_comment();
		} else if (suggesting() && (within(Source) || within(Composer)) && Composer->Cancelled) {
			Composer->Cancelled();
		}
	}

	void review_cycle_view::save_focused_composer()
	{
		const auto* const focus  = QApplication::focusWidget();
		const auto        within = [focus](const QWidget* aRegion) { return focus != nullptr && (focus == aRegion || aRegion->isAncestorOf(focus)); };
		const auto* const open   = TrayComposer->currentWidget();
		if (open == CommentEditor->parentWidget() && within(CommentEditor->parentWidget())) {
			save_comment();
			if (Centre->currentWidget() == Source) {
				Source->setFocus(Qt::OtherFocusReason);
			}
		} else if (open == ReplyPage && within(ReplyPage)) {
			save_reply();
		} else if (suggesting() && (within(Source) || within(Composer)) && Composer->save_button()->isEnabled()) {
			// Exactly what clicking Save suggestion does, refusals included.
			Composer->save_button()->click();
		}
	}

	void review_cycle_view::refresh_cancel_keys()
	{
		const auto* const open = TrayComposer->currentWidget();
		CancelKeys->setEnabled(open == CommentEditor->parentWidget() || open == Composer);
		SaveKeys->setEnabled(open == CommentEditor->parentWidget() || open == ReplyPage || open == Composer);
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
		Source->setAccessibleDescription(QStringLiteral("Edit the whole file. Alt+Shift+Arrow makes a box selection; Ctrl+Tab leaves the editor; %1 cancels the suggestion.").arg(cancel_key_words()));
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
		Source->setAccessibleDescription(QStringLiteral("The captured file. %1 comments on the selection or the caret's line; %2 begins Suggest change there.").arg(note_key_words(false), note_key_words(true)));
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
			// [[BPR-1157]]. The menu's own Comment and Suggest change, so each
			// shows the key that does the same in the code. They act where the
			// caret is, as the keys do; the review bar's Comment notes the file
			// when nothing is selected.
			const auto first   = menu->actions().isEmpty() ? nullptr : menu->actions().constFirst();
			auto*      comment = new QAction{ QStringLiteral("Comment"), menu };
			comment->setObjectName(QStringLiteral("cycleSourceComment"));
			comment->setEnabled(CommentAction->isEnabled());
			connect(comment, &QAction::triggered, this, [this] { comment_here(); });
			auto* const suggest = new QAction{ SuggestAction->text(), menu };
			suggest->setObjectName(QStringLiteral("cycleSourceSuggest"));
			suggest->setEnabled(SuggestAction->isEnabled());
			connect(suggest, &QAction::triggered, this, [this] { begin_suggestion(); });
			for (auto* const action : { comment, suggest }) {
				// A label only: an action of a menu alone never matches a key.
				action->setShortcut(note_key(action == suggest));
				action->setShortcutContext(Qt::WidgetShortcut);
				action->setShortcutVisibleInContextMenu(true);
				menu->insertAction(first, action);
			}
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
		// The share sheet is window-modal; the code's own gestures wait for it to close.
		CommentAction->setEnabled(open && shown && !suggesting() && !Projection.Share);
		SuggestAction->setEnabled(open && reading && editable && !suggesting());
		SuggestAction->setToolTip(shown && !editable ? QStringLiteral("This file is too large to edit here; comment on it instead") : suggest_tip());
		const auto active = Projection.Source ? std::ranges::find(Projection.Files, Projection.Source->Path, &application::review_cycle_file_entry::Path) : Projection.Files.end();
		// [[BPR-1184]]. At a window's edge they go on into the next or previous window.
		PreviousFile->setEnabled(active != Projection.Files.end() && (active != Projection.Files.begin() || Projection.FilesEarlier));
		NextFile->setEnabled(active != Projection.Files.end() && (active + 1 != Projection.Files.end() || Projection.FilesTruncated));
		const auto row = Notes->currentRow();
		RelateButton->setEnabled(open && shown && selected && row >= 0 && static_cast<std::size_t>(row) < Projection.Notes.size());
		const auto bound = open && Projection.Activity == application::review_cycle_activity::idle;
		// [[BPR-1184]]. While a capture runs, Capture revision becomes Cancel capture.
		const auto capturing = open && Projection.Capturing.has_value();
		CaptureAction->setText(capturing ? QStringLiteral("Cancel capture") : QStringLiteral("Capture revision"));
		CaptureAction->setToolTip(capturing ? QStringLiteral("Stop reading the project folder; nothing is recorded and the latest revision stays as it is")
											: QStringLiteral("Read the project folder now and keep its bytes as the next revision"));
		CaptureAction->setEnabled(bound || capturing);
		CaptureAgain->setEnabled(bound && !capturing && !Projection.Revisions.empty());
		// [[BPR-1279]]. Offered on the bar from a refused capture; from the menu whenever the cycle is bound and settled.
		const auto rebindable = bound && !capturing && Projection.Cycle && Projection.Cycle->WorkspaceKind.has_value() && !Projection.Rebind;
		RebindAction->setVisible(open && Projection.RebindOffered);
		RebindAction->setEnabled(rebindable);
		RebindMenu->setEnabled(rebindable);
		MarkReviewed->setEnabled(open && !Projection.Revisions.empty());
		ViewButton->setEnabled(open && !Projection.Revisions.empty());
		ReplyButton->setEnabled(open);
		NextRound->setEnabled(open && !Projection.Facts.Rounds.empty() && Projection.Activity == application::review_cycle_activity::idle);
		ComposeAction->setEnabled(open && !Projection.Revisions.empty() && !Projection.Share && !suggesting());
		PublishAction->setEnabled(open && !Projection.Revisions.empty() && !Projection.Publication && !suggesting());
		PromoteAction->setEnabled(open && !Projection.Links.empty() && !Projection.Notes.empty() && !Projection.Publication && !Promoting && !suggesting());
		const auto waiting = std::ranges::count(Projection.Notes, core::private_thread_lifecycle::open, &application::review_cycle_note_entry::Lifecycle);
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(ComposeAction))) {
			set_strip_badge(*button, waiting == 0 ? QString{ } : QString::number(waiting));
			button->setAccessibleDescription(waiting == 0 ? QString{ } : QStringLiteral("%1 open %2").arg(waiting).arg(waiting == 1 ? QStringLiteral("note") : QStringLiteral("notes")));
		}
	}

	void review_cycle_view::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		apply_review_bar_width();
	}

	void review_cycle_view::apply_review_bar_width()
	{
		// [[BPR-1182]]. Every control stays on the bar down to the window's
		// floor (RFC-BPR-0052): as the view narrows the shown revisions go
		// first ("Full source" for "Full source · R2"), then the position's
		// words ("File 2/4"), then its numbers. The view's name and File
		// remain, and the tooltips keep the whole text. Each width starts
		// whole and measures, so nothing returns at a narrower width.
		const auto available = width();
		const auto remeasure = [this] {
			if (auto* const layout = ReviewBar->layout(); layout != nullptr) {
				layout->invalidate();
			}
		};
		const auto over = [&] { return ReviewBar->sizeHint().width() > available; };
		ViewButton->setText(RevisionWhole);
		FilePosition->setText(FileWords);
		remeasure();
		if (over()) {
			ViewButton->setText(RevisionName);
			remeasure();
		}
		if (over()) {
			FilePosition->setText(FileCompact);
			remeasure();
		}
		if (over()) {
			FilePosition->setText(QStringLiteral("File"));
			remeasure();
		}
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
		} else if (Dispatch.ListFiles && ((index < 0 && Projection.FilesEarlier) || (index >= 0 && Projection.FilesTruncated))) {
			// [[BPR-1184]]. Past the window's edge: the next window, and its first file.
			Dispatch.ListFiles(aDirection, true);
		}
	}

	auto review_cycle_view::rebind_action() const noexcept -> QAction*
	{
		return RebindAction;
	}

	auto review_cycle_view::rebind_menu_action() const noexcept -> QAction*
	{
		return RebindMenu;
	}

	auto review_cycle_view::rebind_dialog() const noexcept -> QDialog*
	{
		return RebindDialog;
	}

	auto review_cycle_view::publish_sheet() const noexcept -> review_cycle_publish_sheet*
	{
		return PublishSheet;
	}

	auto review_cycle_view::publish_action() const noexcept -> QAction*
	{
		return PublishAction;
	}

	auto review_cycle_view::promote_sheet() const noexcept -> review_cycle_promote_sheet*
	{
		return PromoteSheet;
	}

	auto review_cycle_view::promote_action() const noexcept -> QAction*
	{
		return PromoteAction;
	}

	void review_cycle_view::present_promotion(const std::optional<application::review_cycle_promotion_view>& aView)
	{
		const auto opening = aView.has_value() && !Promoting;
		Promoting          = aView.has_value();
		if (aView) {
			PromoteSheet->present(*aView);
		}
		refresh_actions();
		if (opening) {
			// Over the file the reviewer was reading, as Publish as Pull Request is.
			PromoteSheet->set_anchor(ReviewBar->mapToGlobal(QPoint{ ReviewBar->width() / 2, ReviewBar->height() }));
			PromoteSheet->show();
			PromoteSheet->raise();
			PromoteSheet->activateWindow();
			PromoteSheet->focus_first();
		} else if (!aView && PromoteSheet->isVisible()) {
			PromoteSheet->hide();
			if (Centre->currentWidget() == Source) {
				Source->setFocus(Qt::OtherFocusReason);
			}
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
	auto review_cycle_view::share_page() const noexcept -> review_cycle_share_sheet*
	{
		return SharePage;
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
