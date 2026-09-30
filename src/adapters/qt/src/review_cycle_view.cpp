#include "bpr/qt/review_cycle_view.hpp"

#include "bpr/core/sha256.hpp"

#include <QAction>
#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
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

		/// The display text of exact bytes: one block per line, whatever the ending.
		auto display_text(const std::vector<std::uint8_t>& aBytes) -> QString
		{
			const auto lines = core::split_text_lines({ reinterpret_cast<const char*>(aBytes.data()), aBytes.size() });
			auto       shown = QStringList{ };
			shown.reserve(static_cast<qsizetype>(lines.size()));
			for (const auto& line : lines) {
				shown.append(text_of(line.Content));
			}
			return shown.join(QLatin1Char('\n'));
		}

		/// "line 3" or "lines 3–5".
		auto lines_text(const core::cycle_line_span& aSpan) -> QString
		{
			return aSpan.First == aSpan.Last ? QStringLiteral("line %1").arg(aSpan.First) : QStringLiteral("lines %1–%2").arg(aSpan.First).arg(aSpan.Last);
		}

		auto ending_size(const core::text_line_ending aEnding) -> std::uint64_t
		{
			switch (aEnding) {
			case core::text_line_ending::crlf: return 2U;
			case core::text_line_ending::lf:
			case core::text_line_ending::cr:   return 1U;
			case core::text_line_ending::none: return 0U;
			}
			return 0U;
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
		CommentAction = new QAction{ strip_icon(strip_glyph::comment, palette()), QStringLiteral("Comment"), this };
		CommentAction->setObjectName(QStringLiteral("cycleCommentAction"));
		CommentAction->setToolTip(QStringLiteral("Comment on the selection, or on the file when nothing is selected"));
		ReviewBar->addAction(CommentAction);
		SuggestAction = new QAction{ QStringLiteral("Suggest change"), this };
		SuggestAction->setObjectName(QStringLiteral("cycleSuggestAction"));
		SuggestAction->setToolTip(QStringLiteral("Propose exact replacement code for the selection"));
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
		Source = new QPlainTextEdit{ Centre };
		Source->setObjectName(QStringLiteral("cycleSource"));
		Source->setAccessibleName(QStringLiteral("Source"));
		Source->setReadOnly(true);
		Source->setLineWrapMode(QPlainTextEdit::NoWrap);
		Source->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		SourceNotice = new QLabel{ Centre };
		SourceNotice->setObjectName(QStringLiteral("cycleSourceNotice"));
		SourceNotice->setAlignment(Qt::AlignCenter);
		SourceNotice->setWordWrap(true);
		SourceNotice->setTextFormat(Qt::PlainText);
		Centre->addWidget(Source);
		Centre->addWidget(SourceNotice);

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
		trayColumn->addLayout(trayHeader);
		Notes = new QListWidget{ tray };
		Notes->setObjectName(QStringLiteral("cycleNotes"));
		Notes->setAccessibleName(QStringLiteral("Notes"));
		Notes->setWordWrap(true);
		Notes->setTextElideMode(Qt::ElideNone);
		Notes->setSpacing(4);
		Notes->setAlternatingRowColors(true);
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
		Composer = new suggestion_composer{ TrayComposer };
		TrayComposer->addWidget(Composer);
		TrayComposer->setCurrentIndex(0);
		trayColumn->addWidget(TrayComposer);

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
		connect(Source, &QPlainTextEdit::selectionChanged, this, [this] { refresh_actions(); });
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
			if (!Presenting && Projection.Source && Dispatch.ObserveLine) {
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
			const auto anchor = selection_anchor();
			if (anchor && Dispatch.Suggest) {
				Dispatch.Suggest({
					.Selection   = *anchor,
					.Replacement = Composer->replacement(),
					.Intent      = Composer->illustrative() ? core::cycle_suggestion_intent::illustrative : core::cycle_suggestion_intent::exact_replacement,
					.Rationale   = Composer->rationale(),
				});
			}
			TrayComposer->setCurrentIndex(0);
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
		Source->setFont(aFont);
		Source->setTabStopDistance(QFontMetricsF{ aFont }.horizontalAdvance(QLatin1Char(' ')) * core::default_suggestion_tab_width);
		Composer->set_code_font(aFont);
	}

	void review_cycle_view::present(const application::review_cycle_workbench_projection& aProjection)
	{
		Presenting          = true;
		const auto previous = std::move(Projection);
		Projection          = aProjection;

		// The review bar and the Updates line.
		const auto latest = Projection.Revisions.empty() ? std::nullopt : std::optional{ Projection.Revisions.back() };
		if (Projection.Source) {
			RevisionLabel->setText(QStringLiteral("Full source · %1").arg(revision_text(Projection.Source->Revision)));
		} else {
			RevisionLabel->setText(latest ? QStringLiteral("Full source · %1").arg(revision_text(*latest)) : QStringLiteral("Not captured yet"));
		}
		auto updates = QStringList{ };
		for (const auto revision : Projection.Revisions) {
			updates.append(revision_text(revision));
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
		if (!Projection.Message.empty()) {
			line += QStringLiteral(" · ") + text_of(Projection.Message);
		}
		UpdatesLine->setText(line);

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
			const auto path = text_of(tab.View.Path);
			const auto own  = latest && tab.View.Revision != *latest;
			tabs.push_back({
				.Id            = tab.Id,
				.Name          = path.section(QLatin1Char('/'), -1),
				.Path          = path,
				.OwnPair       = own ? revision_text(tab.View.Revision) : QString{ },
				.Pair          = revision_text(tab.View.Revision),
				.State         = Projection.ActiveTab == tab.Id ? (Projection.Source && Projection.Source->Tab == tab.Id ? core::document_tab_view_state::ready : core::document_tab_view_state::loading)
																: core::document_tab_view_state::idle,
				.Active        = Projection.ActiveTab == tab.Id,
				.OwnComparison = own,
			});
		}
		Tabs->present(std::move(tabs));

		// The one source pane: exact retained bytes of the active tab.
		const auto& source  = Projection.Source;
		const auto  changed = !previous.Source || !source || previous.Source->Tab != source->Tab || previous.Source->Revision != source->Revision
			|| previous.Source->Path != source->Path || previous.Source->Bytes != source->Bytes;
		if (!source) {
			SourceNotice->setText(Projection.Cycle ? QStringLiteral("Choose a file to read its captured source.") : QStringLiteral("Open a project to review it locally."));
			Centre->setCurrentWidget(SourceNotice);
		} else if (source->Status != application::review_material_status::available) {
			SourceNotice->setText(QStringLiteral("The captured bytes of %1 in %2 are not available; nothing else is shown in their place.").arg(text_of(source->Path), revision_text(source->Revision)));
			Centre->setCurrentWidget(SourceNotice);
		} else if (!readable(source->Text)) {
			SourceNotice->setText(QStringLiteral("%1 is binary or not UTF-8 (%2 bytes). Notes can still be made on the whole file.").arg(text_of(source->Path)).arg(source->Bytes.size()));
			Centre->setCurrentWidget(SourceNotice);
		} else {
			if (changed) {
				Source->setPlainText(display_text(source->Bytes));
				Source->verticalScrollBar()->setValue(static_cast<int>(source->Line));
			}
			Centre->setCurrentWidget(Source);
		}
		const auto active = source ? std::ranges::find(Projection.Files, source->Path, &application::review_cycle_file_entry::Path) : Projection.Files.end();
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
			auto* const item = new QListWidgetItem{ lines.join(QLatin1Char('\n')), Notes };
			item->setData(Qt::AccessibleTextRole, lines.join(QStringLiteral(". ")));
		}
		// A later capture never moves an open composer: it states newer material.
		if (TrayComposer->currentWidget() == Composer && latest && Projection.Source && Projection.Source->Revision != *latest) {
			Composer->set_source_status(suggestion_source_status::newer_available);
		}
		refresh_actions();
		Presenting = false;
	}

	auto review_cycle_view::source_offset(const int aPosition) const -> std::optional<std::uint64_t>
	{
		if (!Projection.Source) {
			return std::nullopt;
		}
		const auto& bytes = Projection.Source->Bytes;
		const auto  lines = core::split_text_lines({ reinterpret_cast<const char*>(bytes.data()), bytes.size() });
		const auto  block = Source->document()->findBlock(aPosition);
		if (!block.isValid() || static_cast<std::size_t>(block.blockNumber()) >= lines.size()) {
			return std::nullopt;
		}
		auto offset = std::uint64_t{ 0 };
		for (auto line = 0; line < block.blockNumber(); ++line) {
			const auto& entry = lines[static_cast<std::size_t>(line)];
			offset += entry.Content.size() + ending_size(entry.Ending);
		}
		const auto prefix = block.text().left(aPosition - block.position()).toUtf8().size();
		return offset + static_cast<std::uint64_t>(prefix);
	}

	auto review_cycle_view::selection_anchor() const -> std::optional<core::cycle_source_anchor>
	{
		const auto cursor = Source->textCursor();
		if (!Projection.Source || !cursor.hasSelection() || Centre->currentWidget() != Source) {
			return std::nullopt;
		}
		const auto start = source_offset(cursor.selectionStart());
		const auto end   = source_offset(cursor.selectionEnd());
		if (!start || !end) {
			return std::nullopt;
		}
		return core::make_anchor(Projection.Source->Revision, Projection.Source->Path, Projection.Source->Bytes, *start, *end);
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
		const auto anchor = selection_anchor();
		if (!anchor) {
			return;
		}
		const auto span = core::project_lines(Projection.Source->Bytes, anchor->Start, anchor->End);
		Composer->begin({
			.Location = utf8(span ? QStringLiteral("%1 · %2 · %3").arg(text_of(anchor->Path), lines_text(*span), revision_text(anchor->Revision)) : text_of(anchor->Path)),
			.Before   = anchor->Before,
			.Original = anchor->Original,
			.After    = anchor->After,
		});
		const auto latest = Projection.Revisions.empty() ? std::nullopt : std::optional{ Projection.Revisions.back() };
		if (latest && *latest != anchor->Revision) {
			Composer->set_source_status(suggestion_source_status::newer_available);
		}
		TrayComposer->setCurrentWidget(Composer);
		Composer->editor()->setFocus(Qt::OtherFocusReason);
	}

	void review_cycle_view::refresh_actions()
	{
		const auto open     = Projection.Cycle.has_value();
		const auto shown    = Projection.Source && Projection.Source->Status == application::review_material_status::available;
		const auto selected = Source->textCursor().hasSelection() && Centre->currentWidget() == Source;
		CommentAction->setEnabled(open && shown);
		SuggestAction->setEnabled(open && shown && selected);
		const auto active = Projection.Source ? std::ranges::find(Projection.Files, Projection.Source->Path, &application::review_cycle_file_entry::Path) : Projection.Files.end();
		PreviousFile->setEnabled(active != Projection.Files.end() && active != Projection.Files.begin());
		NextFile->setEnabled(active != Projection.Files.end() && active + 1 != Projection.Files.end());
		const auto row = Notes->currentRow();
		RelateButton->setEnabled(open && shown && selected && row >= 0 && static_cast<std::size_t>(row) < Projection.Notes.size());
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
	auto review_cycle_view::source() const noexcept -> QPlainTextEdit*
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
	auto review_cycle_view::projection() const noexcept -> const application::review_cycle_workbench_projection&
	{
		return Projection;
	}
}
