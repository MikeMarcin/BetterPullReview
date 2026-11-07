#include "bpr/qt/tour_agent_panel.hpp"

#include "markdown_github.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		auto mixed(const QColor& aFrom, const QColor& aTo, const double aAmount) -> QColor
		{
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aAmount), static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aAmount),
									static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aAmount));
		}

		/// The agent's words, not yet the author's: a faint accent wash with
		/// a solid accent rule down the left, as a margin note.
		class agent_margin final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto       painter = QPainter{ this };
				const auto ground  = palette().color(QPalette::Active, QPalette::Base);
				const auto accent  = agent_accent(palette());
				painter.fillRect(rect(), mixed(ground, accent, 0.05));
				painter.fillRect(QRect{ 0, 0, 2, height() }, accent);
			}
		};

		/// [[BPR-1900]] owner look v001. A proposed field's own edge: the base
		/// colour inside a quiet one-pixel rounded edge, which turns the agent
		/// accent while the field has the keyboard. The same width either way,
		/// so focusing never moves or shrinks the text; drawn from the
		/// palette, no style sheet.
		class proposal_field_frame final : public QWidget
		{
		public:
			explicit proposal_field_frame(QWidget* aParent)
				: QWidget{ aParent }
			{
				auto* const column = new QVBoxLayout{ this };
				column->setContentsMargins(2, 2, 2, 2);
			}
			void hold(QPlainTextEdit* aField)
			{
				Field = aField;
				layout()->addWidget(aField);
				aField->installEventFilter(this);
				aField->viewport()->installEventFilter(this);
				connect(aField->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, [this] { fit(); });
				QTimer::singleShot(0, this, [this] { fit(); });
			}
			/// As tall as its text (three to twelve lines), so no scroll bar
			/// eats into the words; past twelve it scrolls. Again whenever
			/// its width or text changes, since lines wrap with the width.
			void fit()
			{
				if (Field == nullptr) {
					return;
				}
				const auto lineSpacing = Field->fontMetrics().lineSpacing();
				auto       text        = 0.0;
				for (auto block = Field->document()->begin(); block.isValid(); block = block.next()) {
					text += Field->document()->documentLayout()->blockBoundingRect(block).height();
				}
				const auto margins = 2.0 * Field->document()->documentMargin() + 2;
				const auto height  = static_cast<int>(std::ceil(std::clamp(text, 3.0 * lineSpacing, 12.0 * lineSpacing) + margins));
				if (Field->height() != height) {
					Field->setFixedHeight(height);
				}
			}

		protected:
			bool eventFilter(QObject* aWatched, QEvent* aEvent) override
			{
				if (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut || aEvent->type() == QEvent::EnabledChange) {
					update();
				}
				if (aEvent->type() == QEvent::Resize && Field != nullptr && aWatched == Field->viewport()) {
					QTimer::singleShot(0, this, [this] { fit(); });
				}
				return QWidget::eventFilter(aWatched, aEvent);
			}
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto base    = palette().color(QPalette::Active, QPalette::Base);
				const auto focused = focusWidget() != nullptr && focusWidget()->hasFocus();
				const auto edge    = focused ? agent_accent(palette()) : mixed(base, palette().color(QPalette::Active, QPalette::WindowText), 0.22);
				painter.setPen(QPen{ edge, 1.0 });
				painter.setBrush(base);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
			}

		private:
			QPlainTextEdit* Field = nullptr;
		};

		auto verb(const core::tour_transcript_kind aKind) -> QString
		{
			switch (aKind) {
			case core::tour_transcript_kind::asked:    return QObject::tr("Asked");
			case core::tour_transcript_kind::searched: return QObject::tr("Searched");
			case core::tour_transcript_kind::read:     return QObject::tr("Read");
			case core::tour_transcript_kind::ran:      return QObject::tr("Ran");
			case core::tour_transcript_kind::answered: return QObject::tr("Answered");
			case core::tour_transcript_kind::ended:    return QObject::tr("Ended");
			case core::tour_transcript_kind::left_out: return QObject::tr("Left out");
			}
			return { };
		}
		auto state_words(const core::tour_agent_run_state aState) -> QString
		{
			switch (aState) {
			case core::tour_agent_run_state::running:   return QObject::tr("running in the background");
			case core::tour_agent_run_state::answered:  return QObject::tr("answered");
			case core::tour_agent_run_state::failed:    return QObject::tr("did not finish");
			case core::tour_agent_run_state::cancelled: return QObject::tr("stopped");
			}
			return { };
		}
		/// [[BPR-1912]]. What became of a command outside the read-only list.
		auto note_words(const core::tour_transcript_note aNote) -> QString
		{
			switch (aNote) {
			case core::tour_transcript_note::none:             return { };
			case core::tour_transcript_note::not_run:          return QObject::tr("not run, outside the read-only list");
			case core::tour_transcript_note::ran_outside_list: return QObject::tr("ran, outside the read-only list");
			case core::tour_transcript_note::unchecked:        return QObject::tr("outside the read-only list, not checked");
			case core::tour_transcript_note::not_stopped:      return QObject::tr("not stopped, strict run checks are off");
			}
			return { };
		}
		/// [[BPR-1914]]. Where a passage was selected, as a reader says it.
		auto passage_words(const core::tour_passage& aPassage) -> QString
		{
			switch (aPassage.Field) {
			case core::tour_passage_field::problem:  return QObject::tr("About the Problem");
			case core::tour_passage_field::solution: return QObject::tr("About the Solution");
			case core::tour_passage_field::stop:
				return aPassage.StopTitle.empty() ? QObject::tr("About stop %1").arg(aPassage.Stop + 1U) : QObject::tr("About stop %1, %2").arg(aPassage.Stop + 1U).arg(QString::fromStdString(aPassage.StopTitle));
			case core::tour_passage_field::stop_code:
				return QObject::tr("About the code of stop %1, %2, lines %3 to %4").arg(aPassage.Stop + 1U).arg(QString::fromStdString(aPassage.Path)).arg(aPassage.FirstLine).arg(aPassage.LastLine);
			}
			return { };
		}
		auto tools_words(const core::tour_agent_tools aTools) -> QString
		{
			return aTools == core::tour_agent_tools::read_only ? QObject::tr("read-only, in the review's workspace") : QObject::tr("no tools");
		}
		auto small(QLabel* aLabel) -> QLabel*
		{
			auto font = aLabel->font();
			font.setPointSizeF(font.pointSizeF() * 0.92);
			aLabel->setFont(font);
			return aLabel;
		}
		auto paragraph(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			return label;
		}
		void clear(QVBoxLayout* aLayout)
		{
			while (auto* const item = aLayout->takeAt(0)) {
				// Hidden at once, so a part that is going never shows under
				// the parts that replace it before it is deleted.
				if (auto* const widget = item->widget()) {
					widget->hide();
					widget->deleteLater();
				}
				delete item;
			}
		}
	}

	tour_agent_panel::tour_agent_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("tourAgentSession"));
		setAccessibleName(tr("Tour agent session"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(10, 8, 10, 0);
		column->setSpacing(6);

		// [[BPR-1913]], owner 2026-11-06: "the button at the top looks like a
		// tab, except they're not tabs". Draft the tour is the row's one
		// bordered action, at its own width on the left; Transcript is a bare
		// action with a page glyph at the far end, never checkable, and opens
		// a window. Nothing here is a pair of equal segments.
		Head = new QGridLayout{ };
		Head->setContentsMargins(0, 0, 0, 0);
		Head->setHorizontalSpacing(8);
		Head->setVerticalSpacing(4);
		Draft = new quiet_push_button{ tr("Draft Tour"), this };
		Draft->setObjectName(QStringLiteral("tourDraftTour"));
		Draft->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Draft->setToolTip(tr("Asks the drafting agent, in the background, to read this pull request's code and propose the Problem, the Solution and the stops. You accept, edit or discard each one."));
		Head->addWidget(Draft, 0, 0, Qt::AlignVCenter);
		Status = small(paragraph({ }, this));
		Status->setObjectName(QStringLiteral("tourDraftStatus"));
		Status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		Head->addWidget(Status, 0, 1, Qt::AlignVCenter);
		Head->setColumnStretch(1, 1);
		// [[BPR-1914]] v003. A side chat about the tour as a whole: one quiet
		// glyph beside Conversation. [[BPR-1925]]. The pair keeps together
		// and takes its own row under Draft the tour when the drawer is too
		// narrow for both at their own widths (`fit_head`).
		Trail                = new QWidget{ this };
		auto* const trailRow = new QHBoxLayout{ Trail };
		trailRow->setContentsMargins(0, 0, 0, 0);
		trailRow->setSpacing(8);
		// [[BPR-1931]], owner: "the ability to reset/clear the tour that I'm
		// authoring". A quiet action, never the row's emphasis; it asks first.
		// [[BPR-1931]] v002, owner on v001: "the reset tour button can get
		// smaller like it is on agents so that top row doesn't have to wrap so
		// early". An icon only, as the Agent tab's header actions are: the
		// drawer's clear-and-start-again arrow, named in its tooltip.
		Reset = make_drawer_tool(Trail, drawer_icon(drawer_glyph::clear, palette()), tr("Reset tour"), QStringLiteral("tourResetTour"));
		Reset->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		trailRow->addWidget(Reset, 0, Qt::AlignVCenter);
		// [[BPR-1925]] v002. The tour's own chat toggle, as each part's is.
		ChatTour = new part_chat_toggle{ tr("the whole tour"), Trail };
		ChatTour->setObjectName(QStringLiteral("tourChatAboutTour"));
		TranscriptToggle = new quiet_push_button{ Trail };
		TranscriptToggle->setObjectName(QStringLiteral("tourSessionTranscriptOpen"));
		TranscriptToggle->setFlat(true);
		TranscriptToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		TranscriptToggle->setToolTip(tr("Opens the tour conversation in its own window: ask about the tour, and read what each run read and answered. It never opens by itself."));
		trailRow->addWidget(TranscriptToggle, 0, Qt::AlignVCenter);
		// [[BPR-1930]]. The whole tour's chat toggle at the header's right,
		// as each part's sits at the right of its own header.
		trailRow->addWidget(ChatTour, 0, Qt::AlignVCenter);
		Trail->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Head->addWidget(Trail, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
		column->addLayout(Head);
		fit_head();
		connect(ChatTour, &QToolButton::clicked, this, [this] {
			if (ChatDispatch) {
				ChatDispatch();
			}
		});

		// [[BPR-1930]] AC2. The remembered focus: a small chip in the agent's
		// accent, its × clears it.
		FocusChip = new QWidget{ this };
		FocusChip->setObjectName(QStringLiteral("tourFocusChip"));
		FocusChip->setAccessibleName(tr("Tour focus"));
		auto* const chipRow = new QHBoxLayout{ FocusChip };
		chipRow->setContentsMargins(10, 2, 4, 2);
		chipRow->setSpacing(4);
		FocusText = small(new QLabel{ FocusChip });
		FocusText->setObjectName(QStringLiteral("tourFocusText"));
		FocusText->setTextFormat(Qt::PlainText);
		chipRow->addWidget(FocusText);
		FocusClear = new QToolButton{ FocusChip };
		FocusClear->setObjectName(QStringLiteral("tourFocusClear"));
		FocusClear->setAutoRaise(true);
		FocusClear->setText(QStringLiteral("×"));
		FocusClear->setAccessibleName(tr("Clear the focus"));
		FocusClear->setToolTip(tr("Clears the focus. Later drafts and chat turns cover the whole pull request again."));
		FocusClear->setCursor(Qt::PointingHandCursor);
		chipRow->addWidget(FocusClear);
		FocusChip->installEventFilter(this);
		FocusChip->hide();
		auto* const chipLine = new QHBoxLayout{ };
		chipLine->setContentsMargins(0, 0, 0, 0);
		chipLine->addWidget(FocusChip, 0, Qt::AlignLeft);
		chipLine->addStretch(1);
		column->addLayout(chipLine);
		connect(FocusClear, &QToolButton::clicked, this, [this] {
			if (ClearFocusDispatch) {
				ClearFocusDispatch();
			}
		});

		// [[BPR-1931]] AC1. Reset tour asks in a plain sentence naming what
		// is lost, then offers Undo reset.
		ResetConfirm = new QWidget{ this };
		ResetConfirm->setObjectName(QStringLiteral("tourResetConfirm"));
		ResetConfirm->setAccessibleName(tr("Reset the tour?"));
		auto* const confirmColumn = new QVBoxLayout{ ResetConfirm };
		confirmColumn->setContentsMargins(12, 8, 8, 8);
		confirmColumn->setSpacing(6);
		ResetSentence = paragraph({ }, ResetConfirm);
		ResetSentence->setObjectName(QStringLiteral("tourResetSentence"));
		confirmColumn->addWidget(ResetSentence);
		auto* const confirmButtons = new QHBoxLayout{ };
		confirmButtons->setSpacing(6);
		confirmButtons->addStretch(1);
		ResetNo = new quiet_push_button{ tr("Keep the tour"), ResetConfirm };
		ResetNo->setObjectName(QStringLiteral("tourResetCancel"));
		ResetNo->setFlat(true);
		ResetNo->setFocusPolicy(Qt::TabFocus);
		ResetYes = new quiet_push_button{ tr("Reset tour"), ResetConfirm };
		ResetYes->setObjectName(QStringLiteral("tourResetConfirmButton"));
		ResetYes->setFocusPolicy(Qt::TabFocus);
		confirmButtons->addWidget(ResetNo);
		confirmButtons->addWidget(ResetYes);
		confirmColumn->addLayout(confirmButtons);
		ResetConfirm->installEventFilter(this);
		ResetConfirm->hide();
		column->addWidget(ResetConfirm);
		ResetDone = new QWidget{ this };
		ResetDone->setObjectName(QStringLiteral("tourResetDone"));
		auto* const doneRow = new QHBoxLayout{ ResetDone };
		doneRow->setContentsMargins(12, 6, 8, 6);
		doneRow->setSpacing(8);
		auto* const doneWords = small(paragraph(tr("Tour reset. Undo reset brings it all back while you haven't changed the tour since."), ResetDone));
		doneWords->setObjectName(QStringLiteral("tourResetDoneText"));
		doneRow->addWidget(doneWords, 1);
		UndoReset = new quiet_push_button{ tr("Undo reset"), ResetDone };
		UndoReset->setObjectName(QStringLiteral("tourResetUndo"));
		UndoReset->setFocusPolicy(Qt::TabFocus);
		doneRow->addWidget(UndoReset, 0, Qt::AlignVCenter);
		Accented.emplace_back(UndoReset);
		ResetDone->installEventFilter(this);
		ResetDone->hide();
		column->addWidget(ResetDone);
		connect(Reset, &QToolButton::clicked, this, [this] {
			ResetSentence->setText(reset_words(ResetTally));
			ResetDone->hide();
			ResetConfirm->setVisible(!ResetConfirm->isVisible());
			if (ResetConfirm->isVisible()) {
				ResetNo->setFocus(Qt::OtherFocusReason);
			}
		});
		connect(ResetNo, &QPushButton::clicked, this, [this] {
			ResetConfirm->hide();
			Reset->setFocus(Qt::OtherFocusReason);
		});
		connect(ResetYes, &QPushButton::clicked, this, [this] {
			ResetConfirm->hide();
			if (ResetDispatch) {
				ResetDispatch();
			}
		});
		connect(UndoReset, &QPushButton::clicked, this, [this] {
			if (UndoResetDispatch) {
				UndoResetDispatch();
			}
		});
		FailureSlot = new QVBoxLayout{ };
		FailureSlot->setContentsMargins(0, 0, 0, 0);
		column->addLayout(FailureSlot);
		connect(Draft, &QPushButton::clicked, this, [this] {
			if (RunningDraft) {
				if (StopDispatch) {
					StopDispatch();
				}
			} else if (DraftDispatch) {
				DraftDispatch();
			}
		});
		connect(TranscriptToggle, &QPushButton::clicked, this, [this] { open_transcript(); });

		// The proposal: the agent's draft, until each part is decided.
		auto* const proposal = new agent_margin{ this };
		ProposalBox          = proposal;
		ProposalBox->setObjectName(QStringLiteral("tourProposal"));
		ProposalBox->setAccessibleName(tr("Proposed tour"));
		auto* const proposalColumn = new QVBoxLayout{ ProposalBox };
		proposalColumn->setContentsMargins(12, 8, 8, 8);
		proposalColumn->setSpacing(6);
		ProposalTitle = new QLabel{ ProposalBox };
		ProposalTitle->setObjectName(QStringLiteral("tourProposalTitle"));
		ProposalTitle->setWordWrap(true);
		ProposalTitle->setTextFormat(Qt::RichText);
		// [[BPR-1925]] v003. The proposal's parts wait in the tour itself; here
		// one line says how many, with Accept all.
		auto* const titleRow = new QHBoxLayout{ };
		titleRow->setSpacing(8);
		titleRow->addWidget(ProposalTitle, 1);
		AcceptAll = new quiet_push_button{ tr("Accept all"), ProposalBox };
		AcceptAll->setObjectName(QStringLiteral("tourProposalAcceptAll"));
		AcceptAll->setFocusPolicy(Qt::TabFocus);
		AcceptAll->setToolTip(tr("Accepts every proposed part still waiting in the tour, as it reads now."));
		titleRow->addWidget(AcceptAll, 0, Qt::AlignTop);
		Accented.emplace_back(AcceptAll);
		connect(AcceptAll, &QPushButton::clicked, this, [this] {
			if (!Proposal || !DecideDispatch) {
				return;
			}
			// Each part's text first: deciding one lays the proposal out again.
			auto decisions = std::vector<std::pair<std::size_t, QString>>{ };
			for (std::size_t index = 0U; index < Proposal->Items.size(); ++index) {
				if (Proposal->Items[index].Decision == core::tour_proposal_decision::pending && !core::proposed_in_thread(Session, Proposal->Items[index])) {
					const auto found = Editors.find(index);
					decisions.emplace_back(index, found != Editors.end() && found->second != nullptr ? found->second->toPlainText() : QString::fromStdString(Proposal->Items[index].Text));
				}
			}
			for (const auto& [index, text] : decisions) {
				DecideDispatch(index, core::tour_proposal_decision::accepted, text);
			}
		});
		proposalColumn->addLayout(titleRow);
		// [[BPR-1900]] owner look v002 ("What's up with this?"): stops that
		// pointed outside the pull request are not shown; one quiet line
		// says how many, and the transcript keeps them.
		LeftOut = small(paragraph({ }, ProposalBox));
		LeftOut->setObjectName(QStringLiteral("tourProposalLeftOut"));
		LeftOut->setForegroundRole(QPalette::PlaceholderText);
		LeftOut->hide();
		proposalColumn->addWidget(LeftOut);
		// [[BPR-1912]]. Commands Claude ran outside the read-only list: one
		// quiet line, not an error, with the way to them in the transcript.
		OutsideList = small(new QLabel{ ProposalBox });
		OutsideList->setObjectName(QStringLiteral("tourProposalOutsideList"));
		OutsideList->setWordWrap(true);
		OutsideList->setTextFormat(Qt::RichText);
		OutsideList->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		OutsideList->setForegroundRole(QPalette::PlaceholderText);
		OutsideList->hide();
		connect(OutsideList, &QLabel::linkActivated, this, [this] {
			if (!Proposal) {
				return;
			}
			auto entry = std::optional<std::size_t>{ };
			if (const auto* run = core::find_tour_run(Session, Proposal->Run); run != nullptr) {
				const auto found = std::ranges::find(run->Entries, core::tour_transcript_note::ran_outside_list, &core::tour_transcript_entry::Note);
				if (found != run->Entries.end()) {
					entry = static_cast<std::size_t>(found - run->Entries.begin());
				}
			}
			open_transcript(Proposal->Run, entry);
		});
		proposalColumn->addWidget(OutsideList);
		// [[BPR-1930]]. A proposed focus belongs to the tour as a whole: it
		// waits here, under the line, at its own height.
		FocusSlot = new QVBoxLayout{ };
		FocusSlot->setContentsMargins(0, 2, 0, 2);
		FocusSlot->setSpacing(6);
		proposalColumn->addLayout(FocusSlot);
		auto* const proposalScroll = new QScrollArea{ ProposalBox };
		// [[BPR-1900]] owner look v001: a click in the proposal never parks
		// the keyboard on the pane itself (which drew a box around it and
		// took the first click); the wheel and the scroll bar still scroll.
		proposalScroll->setFocusPolicy(Qt::NoFocus);
		proposalScroll->setWidgetResizable(true);
		proposalScroll->setFrameShape(QFrame::NoFrame);
		proposalScroll->setMinimumHeight(300);
		proposalScroll->setMaximumHeight(460);
		ProposalScroll = proposalScroll;
		proposalScroll->viewport()->installEventFilter(this);
		proposalScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const proposalBody = new QWidget{ proposalScroll };
		proposalBody->setAutoFillBackground(false);
		ProposalItems = new QVBoxLayout{ proposalBody };
		ProposalItems->setContentsMargins(0, 0, 4, 0);
		ProposalItems->setSpacing(10);
		proposalScroll->setWidget(proposalBody);
		proposalScroll->viewport()->setAutoFillBackground(false);
		proposalColumn->addWidget(proposalScroll);
		ProposalBox->hide();
		column->addWidget(ProposalBox);

		// [[BPR-1913]], RFC-BPR-0092 T2 amended. The transcript: a window of
		// its own, opened from here or from a failure notice, never pushed in
		// front. [[BPR-1914]]: it is the tour conversation. The turns on the
		// left; the whole conversation as one text on the right, so a
		// selection spans turns; the ask box under it; Copy turn and Copy
		// conversation above it.
		TranscriptWindow = new QWidget{ this, Qt::Window };
		TranscriptWindow->setObjectName(QStringLiteral("tourSessionTranscript"));
		TranscriptWindow->setWindowTitle(tr("Tour conversation"));
		TranscriptWindow->setAccessibleName(tr("Tour conversation"));
		TranscriptWindow->resize(820, 620);
		auto* const windowColumn = new QVBoxLayout{ TranscriptWindow };
		windowColumn->setContentsMargins(14, 12, 14, 12);
		windowColumn->setSpacing(8);
		auto* const top   = new QHBoxLayout{ };
		auto* const about = small(paragraph(tr("Every side chat and draft of this tour, in order, with what the agent read and answered. Open a side chat from the chat glyph beside any part of the tour."), TranscriptWindow));
		about->setForegroundRole(QPalette::PlaceholderText);
		top->addWidget(about, 1);
		CopyRun = new quiet_push_button{ tr("Copy turn"), TranscriptWindow };
		CopyRun->setObjectName(QStringLiteral("tourSessionCopyRun"));
		CopyRun->setFlat(true);
		CopyRun->setToolTip(tr("Copies this turn as plain text: what was asked, each step and its command or path, and the reply or how it ended."));
		CopyAll = new quiet_push_button{ tr("Copy conversation"), TranscriptWindow };
		CopyAll->setObjectName(QStringLiteral("tourSessionCopyAll"));
		CopyAll->setFlat(true);
		CopyAll->setToolTip(tr("Copies every turn of this tour conversation as plain text, oldest first."));
		top->addWidget(CopyRun, 0, Qt::AlignTop);
		top->addWidget(CopyAll, 0, Qt::AlignTop);
		windowColumn->addLayout(top);
		Copied = small(paragraph({ }, TranscriptWindow));
		Copied->setObjectName(QStringLiteral("tourSessionCopied"));
		Copied->setForegroundRole(QPalette::PlaceholderText);
		Copied->setAlignment(Qt::AlignRight);
		windowColumn->addWidget(Copied);
		auto* const split = new QSplitter{ Qt::Horizontal, TranscriptWindow };
		split->setChildrenCollapsible(false);
		RunList = new QListWidget{ split };
		RunList->setObjectName(QStringLiteral("tourSessionRuns"));
		RunList->setAccessibleName(tr("Turns"));
		RunList->setWordWrap(true);
		RunList->setTextElideMode(Qt::ElideNone);
		RunList->setUniformItemSizes(false);
		RunList->setMinimumWidth(170);
		RunList->setSpacing(3);
		RunView = new QTextBrowser{ split };
		RunView->setObjectName(QStringLiteral("tourSessionRun"));
		RunView->setAccessibleName(tr("Conversation"));
		RunView->setOpenLinks(false);
		RunView->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		RunView->document()->setDocumentMargin(14);
		split->addWidget(RunList);
		split->addWidget(RunView);
		split->setStretchFactor(0, 0);
		split->setStretchFactor(1, 1);
		split->setSizes({ 200, 600 });
		windowColumn->addWidget(split, 1);
		connect(RunList, &QListWidget::currentRowChanged, this, [this](const int aRow) {
			if (aRow < 0) {
				return;
			}
			FocusRun = core::tour_agent_run_id{ RunList->item(aRow)->data(Qt::UserRole).toULongLong() };
			Copied->clear();
			show_run(FocusRun, std::nullopt);
		});
		connect(CopyRun, &QPushButton::clicked, this, [this] {
			if (const auto* run = ShownRun ? core::find_tour_run(Session, *ShownRun) : nullptr; run != nullptr) {
				copy(run_text(*run), tr("Copied turn %1.").arg(run->Id.value()));
			}
		});
		connect(CopyAll, &QPushButton::clicked, this, [this] { copy(session_text(), tr("Copied the conversation.")); });
		TranscriptWindow->hide();
		refresh_toggle();
		set_running(false, false);
	}

	void tour_agent_panel::set_dispatch(std::function<void()> aDraft, std::function<void()> aStop, decide_dispatch aDecide)
	{
		DraftDispatch  = std::move(aDraft);
		StopDispatch   = std::move(aStop);
		DecideDispatch = std::move(aDecide);
	}

	void tour_agent_panel::present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal)
	{
		// What the author typed into a pending item survives a presentation
		// of the same proposal.
		const auto same = Proposal && aProposal && Proposal->Run == aProposal->Run;
		if (same) {
			for (const auto& [index, editor] : Editors) {
				Edited[index] = editor->toPlainText();
			}
		} else {
			Edited.clear();
			Editing.clear();
		}
		// [[BPR-1914]]. A new turn: the conversation follows it.
		if (aSession.LastRun != Session.LastRun) {
			FocusRun.reset();
		}
		Session  = aSession;
		Proposal = aProposal;
		rebuild_proposal();
		if (TranscriptWindow->isVisible()) {
			rebuild_transcript();
		}
		refresh_toggle();
		refresh_shape();
	}

	void tour_agent_panel::set_running(const bool aDraftTour, const bool aAny)
	{
		RunningDraft = aDraftTour;
		RunningAny   = aAny;
		Draft->setText(aDraftTour ? tr("Stop drafting") : tr("Draft Tour"));
		fit_head();
		Draft->setToolTip(aDraftTour ? tr("Drafting the tour in the background. Stop it; nothing in the tour changes.") : Ready ? tr("Asks the drafting agent, in the background, to read this pull request's code and propose the Problem, the Solution and the stops. You accept, edit or discard each one.")
																																: Why);
		Draft->setEnabled(Ready && (aDraftTour || !aAny));
		ChatTour->setEnabled(true);
		// What it reads and where the proposal lands is the tab's notice
		// line, under this panel; the control itself says it is running.
		Status->hide();
		refresh_toggle();
		refresh_shape();
	}

	void tour_agent_panel::set_available(const bool aReady, const QString& aWhy)
	{
		Ready = aReady;
		Why   = aWhy;
		if (!aReady) {
			ChatTour->setToolTip(aWhy);
		}
		set_running(RunningDraft, RunningAny);
	}

	void tour_agent_panel::set_failure_notice(QWidget* aNotice)
	{
		aNotice->setParent(this);
		FailureSlot->addWidget(aNotice);
	}

	void tour_agent_panel::open_transcript(const std::optional<core::tour_agent_run_id> aRun, const std::optional<std::size_t> aEntry)
	{
		FocusRun = aRun;
		Copied->clear();
		rebuild_transcript();
		TranscriptWindow->show();
		TranscriptWindow->raise();
		TranscriptWindow->activateWindow();
		if (aEntry) {
			show_run(ShownRun, aEntry);
		}
	}

	void tour_agent_panel::restyle(const QPalette& aGround)
	{
		for (auto* const widget : { static_cast<QWidget*>(Draft), static_cast<QWidget*>(TranscriptToggle), static_cast<QWidget*>(Status), static_cast<QWidget*>(ProposalTitle) }) {
			apply_agent_accent(*widget, aGround);
		}
		Draft->setIcon(drawer_icon(drawer_glyph::agent, Draft->palette()));
		TranscriptToggle->setIcon(drawer_icon(drawer_glyph::page, TranscriptToggle->palette()));
		// [[BPR-1913]]. A window of its own takes no palette from its parent:
		// it wears the main window's appearance, as the Settings window does,
		// and its run is drawn again in that appearance's colours.
		// Every role is set: an inherited ground resolves nothing of its own,
		// and a window would then fall back to the application's palette.
		auto ground = aGround;
		ground.setResolveMask(~QPalette::ResolveMask{ 0 });
		TranscriptWindow->setPalette(ground);
		if (TranscriptWindow->isVisible()) {
			show_run(ShownRun, std::nullopt);
		}
		CopyRun->setIcon(drawer_icon(drawer_glyph::copy, CopyRun->palette()));
		apply_agent_accent(*ChatTour, aGround);
		ChatTour->setIcon(side_chat_icon(ChatTour->palette()));
		// [[BPR-1931]] v002. Reset tour's glyph in this appearance's ink.
		Reset->setIcon(drawer_icon(drawer_glyph::clear, Reset->palette()));
		// [[BPR-1930]]. The focus chip reads in the agent's accent.
		apply_agent_accent(*FocusText, aGround);
		apply_agent_accent(*FocusClear, aGround);
		FocusChip->update();
		ResetConfirm->update();
		ResetDone->update();
		for (const auto& widget : Accented) {
			if (widget) {
				apply_agent_accent(*widget, aGround);
			}
		}
		// The proposal's fields paint their own edge from the palette.
		for (const auto& field : Fields) {
			if (field) {
				field->update();
			}
		}
		ProposalBox->update();
	}

	auto tour_agent_panel::transcript_text() const -> QString
	{
		auto lines = QStringList{ };
		for (auto run = Session.Runs.rbegin(); run != Session.Runs.rend(); ++run) {
			lines << tr("Run %1, %2: %3").arg(run->Id.value()).arg(QString::fromStdString(run->Entries.empty() ? std::string{ } : run->Entries.front().Text), state_words(run->State));
			for (const auto& entry : run->Entries) {
				const auto note = note_words(entry.Note);
				lines << verb(entry.Kind) + QStringLiteral(" ") + QString::fromStdString(entry.Text) + (note.isEmpty() ? QString{ } : QStringLiteral(" (") + note + QLatin1Char(')'));
			}
		}
		return lines.join(QLatin1Char('\n'));
	}

	auto tour_agent_panel::run_text(const core::tour_agent_run& aRun) -> QString
	{
		const auto asked = aRun.Entries.empty() ? QString{ } : QString::fromStdString(aRun.Entries.front().Text);
		auto       how   = tr("%1, %2.").arg(state_words(aRun.State), tools_words(aRun.Tools));
		how[0]           = how[0].toUpper();
		const auto ask   = aRun.Request == core::tour_agent_request::ask;
		auto       lines = QStringList{ tr("## Run %1: %2").arg(aRun.Id.value()).arg(asked.section(QLatin1Char('\n'), 0, 0)), QString{ }, how, QString{ } };
		// [[BPR-1914]]. An ask turn: the passage it was about, quoted.
		if (ask && aRun.Passage) {
			lines << QStringLiteral("> ") + passage_words(*aRun.Passage) + QLatin1Char(':');
			for (const auto& line : QString::fromStdString(aRun.Passage->Text).split(QLatin1Char('\n'))) {
				lines << QStringLiteral("> ") + line;
			}
			lines << QString{ };
		}
		for (const auto& entry : aRun.Entries) {
			// The reply follows the steps, as written.
			if (ask && entry.Kind == core::tour_transcript_kind::answered) {
				continue;
			}
			const auto note = note_words(entry.Note);
			lines << QStringLiteral("- ") + verb(entry.Kind) + QStringLiteral(": ") + QString::fromStdString(entry.Text) + (note.isEmpty() ? QString{ } : QStringLiteral(" (") + note + QLatin1Char(')'));
		}
		if (aRun.Dropped > 0U) {
			lines << QStringLiteral("- ") + (aRun.Dropped == 1U ? tr("1 more step was not kept.") : tr("%1 more steps were not kept.").arg(aRun.Dropped));
		}
		if (ask && aRun.State == core::tour_agent_run_state::answered) {
			lines << QString{ } << QString::fromStdString(aRun.Reply);
			if (const auto proposed = proposed_words(aRun.Proposed); !proposed.isEmpty()) {
				lines << QString{ } << QStringLiteral("_") + proposed + QStringLiteral("_");
			}
		}
		return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
	}

	auto tour_agent_panel::session_text() const -> QString
	{
		auto runs = QStringList{ };
		for (const auto& run : Session.Runs) {
			runs << run_text(run);
		}
		return runs.join(QLatin1Char('\n'));
	}

	void tour_agent_panel::copy(const QString& aText, const QString& aSaid)
	{
		if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
			clipboard->setText(aText);
			Copied->setText(aSaid);
		}
	}

	auto tour_agent_panel::proposal_items() const -> QWidget* { return ProposalScroll->widget(); }

	auto tour_agent_panel::proposal_editor(const std::size_t aIndex) const -> QPlainTextEdit*
	{
		const auto found = Editors.find(aIndex);
		return found == Editors.end() ? nullptr : found->second;
	}

	auto tour_agent_panel::relocate_button(const std::size_t aIndex) const -> QPushButton*
	{
		const auto found = Relocates.find(aIndex);
		return found == Relocates.end() ? nullptr : found->second;
	}

	auto tour_agent_panel::placement_text(const std::size_t aIndex) const -> QString
	{
		const auto found = Placements.find(aIndex);
		return found == Placements.end() ? QString{ } : found->second->text();
	}

	void tour_agent_panel::set_code_style(const QFont& aFont, const qreal aTabStopDistance, std::optional<markdown_code_theme> aTheme, std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		CodeFont     = aFont;
		TabStop      = aTabStopDistance;
		CodeTheme    = aTheme;
		CodeDispatch = aDispatch;
		for (const auto& [index, reader] : Readers) {
			reader->set_code_font(CodeFont, TabStop);
			if (CodeTheme) {
				reader->set_code_theme(*CodeTheme);
			}
			if (CodeDispatch) {
				reader->set_code_syntax_dispatch(CodeDispatch);
				reader->request_code_syntax();
			}
		}
		fit_readers();
	}

	void tour_agent_panel::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		for (const auto& [index, reader] : Readers) {
			reader->apply_code_syntax(aSpans);
		}
	}

	void tour_agent_panel::fit_readers()
	{
		// [[BPR-1915]]. Each rendered text as tall as it reads at the
		// proposal's width, so nothing scrolls inside it.
		const auto listed = std::max(1, ProposalScroll->viewport()->width() - ProposalItems->contentsMargins().left() - ProposalItems->contentsMargins().right());
		for (const auto& [index, reader] : Readers) {
			if (reader != nullptr) {
				// [[BPR-1925]] v003. A box placed in the tour is as wide as its place.
				const auto* const box   = reader->parentWidget();
				const auto        width = box != nullptr && box->width() > 60 && !ProposalItems->parentWidget()->isAncestorOf(box) ? box->contentsRect().width() : listed;
				reader->setFixedHeight(reader->isHidden() ? 0 : reader->heightForWidth(width));
			}
		}
	}

	auto tour_agent_panel::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		const auto placedBox = std::ranges::any_of(Boxes, [aWatched](const auto& aEntry) { return aEntry.second == aWatched; });
		if (aEvent->type() == QEvent::Resize && ProposalScroll != nullptr && (aWatched == ProposalScroll->viewport() || placedBox)) {
			QTimer::singleShot(0, this, [this] { fit_readers(); });
		}
		// [[BPR-1930]], [[BPR-1931]]. The focus chip, a pill in the agent's
		// accent; the reset's question, on a faint warning wash with its rule;
		// and its done line, quiet. Drawn from the palette, no style sheet.
		if (aEvent->type() == QEvent::Paint && (aWatched == FocusChip || aWatched == ResetConfirm || aWatched == ResetDone)) {
			auto* const widget  = static_cast<QWidget*>(aWatched);
			auto        painter = QPainter{ widget };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto base   = palette().color(QPalette::Active, QPalette::Base);
			const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
			const auto bounds = QRectF{ widget->rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
			if (aWatched == FocusChip) {
				const auto accent = agent_accent(palette());
				painter.setPen(QPen{ mixed(base, accent, 0.55), 1.0 });
				painter.setBrush(mixed(base, accent, 0.10));
				painter.drawRoundedRect(bounds, bounds.height() / 2.0, bounds.height() / 2.0);
			} else {
				const auto dark = palette().color(QPalette::Window).lightness() < 128;
				const auto warn = aWatched == ResetConfirm ? (dark ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E }) : mixed(base, ink, 0.45);
				painter.setPen(QPen{ mixed(base, warn, 0.35), 1.0 });
				painter.setBrush(mixed(base, warn, aWatched == ResetConfirm ? 0.06 : 0.04));
				painter.drawRoundedRect(bounds, 8.0, 8.0);
				painter.setPen(Qt::NoPen);
				painter.setBrush(warn);
				painter.drawRoundedRect(QRectF{ bounds.left(), bounds.top() + 6.0, 3.0, bounds.height() - 12.0 }, 1.5, 1.5);
			}
			return false;
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void tour_agent_panel::set_shape_dispatch(std::function<void()> aClearFocus, std::function<void()> aReset, std::function<void()> aUndoReset)
	{
		ClearFocusDispatch = std::move(aClearFocus);
		ResetDispatch      = std::move(aReset);
		UndoResetDispatch  = std::move(aUndoReset);
	}

	void tour_agent_panel::present_reset(const core::tour_reset_tally& aTally, const bool aAvailable, const bool aUndoable)
	{
		ResetTally     = aTally;
		ResetAvailable = aAvailable;
		ResetDone->setVisible(aUndoable);
		if (aUndoable) {
			ResetConfirm->hide();
		}
		ResetSentence->setText(reset_words(aTally));
		refresh_shape();
	}

	auto tour_agent_panel::reset_words(const core::tour_reset_tally& aTally) -> QString
	{
		auto parts = QStringList{ };
		if (aTally.Problem) {
			parts << tr("the Problem");
		}
		if (aTally.Solution) {
			parts << tr("the Solution");
		}
		if (aTally.Stops > 0U) {
			parts << (aTally.Stops == 1U ? tr("its 1 stop") : tr("all %1 stops").arg(aTally.Stops));
		}
		if (aTally.Proposals > 0U) {
			parts << (aTally.Proposals == 1U ? tr("1 waiting proposal") : tr("%1 waiting proposals").arg(aTally.Proposals));
		}
		if (aTally.Chats > 0U) {
			parts << (aTally.Chats == 1U ? tr("1 chat") : tr("%1 chats").arg(aTally.Chats));
		}
		if (aTally.Focus) {
			parts << tr("the focus");
		}
		if (parts.isEmpty()) {
			return tr("The tour is empty. There is nothing to reset.");
		}
		auto joined = parts.front();
		for (qsizetype index = 1; index < parts.size(); ++index) {
			joined += (index + 1 == parts.size() ? tr(" and ") : QStringLiteral(", ")) + parts[index];
		}
		auto words = tr("Reset clears %1. You can undo it until you change the tour again.").arg(joined);
		if (aTally.Published) {
			words += QLatin1Char(' ') + tr("The tour published on GitHub stays as it is: this resets only your local draft.");
		}
		return words;
	}

	void tour_agent_panel::refresh_shape()
	{
		const auto focus = QString::fromStdString(Session.Focus);
		FocusText->setText(tr("Focus: %1").arg(focus));
		FocusText->setToolTip(tr("Later drafts and chat turns favour this part of the pull request. Clear it to cover the whole change again."));
		FocusChip->setVisible(!focus.isEmpty());
		const auto can = ResetAvailable && core::resettable(ResetTally) && !RunningAny;
		Reset->setEnabled(can);
		// [[BPR-1931]] v002. An icon only: its tooltip starts with its name.
		const auto why = RunningAny         ? tr("Wait for the tour agent to finish, or stop it, before resetting the tour.")
			: !core::resettable(ResetTally) ? tr("The tour is empty. There is nothing to reset.")
											: tr("Clears the Problem, the Solution, the stops, the proposals, the chats and the focus of this tour, after asking. You can undo it.");
		Reset->setToolTip(tr("Reset tour") + QStringLiteral("\n") + why);
		Reset->setAccessibleName(tr("Reset tour"));
		Reset->setAccessibleDescription(why);
		if (!can) {
			ResetConfirm->hide();
		}
		fit_head();
	}

	auto tour_agent_panel::stop_number(const core::tour_stop_id aStop) const -> int
	{
		if (!Tour) {
			return 0;
		}
		const auto found = std::ranges::find(Tour->Stops, aStop, &core::tour_stop::Id);
		return found == Tour->Stops.end() ? 0 : static_cast<int>(found - Tour->Stops.begin()) + 1;
	}

	auto tour_agent_panel::stop_title(const core::tour_stop_id aStop) const -> QString
	{
		if (!Tour) {
			return { };
		}
		const auto found = std::ranges::find(Tour->Stops, aStop, &core::tour_stop::Id);
		return found == Tour->Stops.end() || found->Title.empty() ? tr("Untitled stop") : QString::fromStdString(found->Title);
	}

	/// [[BPR-1930]]. What a structural part does, in the tour's own numbers:
	/// the stop a removal takes out, the lines a merge keeps, a split's
	/// pieces, the new order with the stops that move.
	void tour_agent_panel::add_revision_lines(QVBoxLayout* aColumn, const core::tour_proposal_item& aItem, QWidget* aBox)
	{
		const auto quietLine = [aBox, aColumn](const QString& aText) {
			auto* const line = small(paragraph(aText, aBox));
			line->setForegroundRole(QPalette::PlaceholderText);
			aColumn->addWidget(line);
			return line;
		};
		switch (aItem.Part) {
		case core::tour_proposal_part::remove_stop:
			quietLine(tr("Takes this stop and its explanation out of the tour. Undo arrangement brings it back."));
			break;
		case core::tour_proposal_part::merge_stops: {
			auto names = QStringList{ };
			for (const auto stop : aItem.Stops) {
				names << tr("%1, %2").arg(stop_number(stop)).arg(stop_title(stop));
			}
			quietLine(tr("Makes one stop of %1. It keeps the lines of each, and reads:").arg(names.join(tr("; "))));
			break;
		}
		case core::tour_proposal_part::split_stop: {
			const auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
			for (std::size_t index = 0U; index < aItem.Pieces.size(); ++index) {
				const auto& piece = aItem.Pieces[index];
				auto* const title = small(paragraph(tr("%1. %2").arg(index + 1U).arg(QString::fromStdString(piece.Title)), aBox));
				auto        heavy = title->font();
				heavy.setWeight(QFont::DemiBold);
				title->setFont(heavy);
				aColumn->addWidget(title);
				const auto  lines = piece.FirstLine == piece.LastLine ? tr("line %1").arg(piece.FirstLine) : tr("lines %1 to %2").arg(piece.FirstLine).arg(piece.LastLine);
				auto* const where = quietLine(QString::fromStdString(piece.Path) + QStringLiteral(", ") + lines);
				auto        face  = code;
				face.setPointSizeF(where->font().pointSizeF());
				where->setFont(face);
				quietLine(QString::fromStdString(piece.Explanation).simplified());
			}
			quietLine(tr("Each new stop has its lines to confirm in the code."));
			break;
		}
		case core::tour_proposal_part::reorder: {
			auto at = 0;
			for (const auto stop : aItem.Stops) {
				const auto was = stop_number(stop);
				if (was == 0) {
					continue;
				}
				++at;
				const auto  moved = was != at;
				auto* const line  = small(paragraph(moved ? tr("%1. %2 (moved from %3)").arg(at).arg(stop_title(stop)).arg(was) : tr("%1. %2").arg(at).arg(stop_title(stop)), aBox));
				line->setObjectName(moved ? QStringLiteral("tourProposedOrderMoved") : QStringLiteral("tourProposedOrderKept"));
				if (moved) {
					auto heavy = line->font();
					heavy.setWeight(QFont::DemiBold);
					line->setFont(heavy);
				} else {
					line->setForegroundRole(QPalette::PlaceholderText);
				}
				aColumn->addWidget(line);
			}
			break;
		}
		case core::tour_proposal_part::focus:
			quietLine(tr("Remembered for this tour's later drafts and chat turns, until you clear it."));
			break;
		default: break;
		}
	}

	auto tour_agent_panel::proposal_reader(const std::size_t aIndex) const -> markdown_body_view*
	{
		const auto found = Readers.find(aIndex);
		return found == Readers.end() ? nullptr : found->second;
	}

	auto tour_agent_panel::edit_button(const std::size_t aIndex) const -> QPushButton*
	{
		const auto found = EditButtons.find(aIndex);
		return found == EditButtons.end() ? nullptr : found->second;
	}

	auto tour_agent_panel::proposed_words(const core::tour_proposal_tally& aTally) -> QString
	{
		auto parts = QStringList{ };
		if (aTally.Problem) {
			parts << tr("a new Problem");
		}
		if (aTally.Solution) {
			parts << tr("a new Solution");
		}
		if (aTally.Stops > 0U) {
			parts << (aTally.Stops == 1U ? tr("1 new stop") : tr("%1 new stops").arg(aTally.Stops));
		}
		if (aTally.StopTexts > 0U) {
			parts << (aTally.StopTexts == 1U ? tr("a new explanation for 1 stop") : tr("new explanations for %1 stops").arg(aTally.StopTexts));
		}
		// [[BPR-1930]]. A revision of the tour's shape.
		if (aTally.Merges > 0U) {
			parts << (aTally.Merges == 1U ? tr("a merge") : tr("%1 merges").arg(aTally.Merges));
		}
		if (aTally.Splits > 0U) {
			parts << (aTally.Splits == 1U ? tr("a split") : tr("%1 splits").arg(aTally.Splits));
		}
		if (aTally.Removals > 0U) {
			parts << (aTally.Removals == 1U ? tr("1 stop to remove") : tr("%1 stops to remove").arg(aTally.Removals));
		}
		if (aTally.Reorder) {
			parts << tr("a new order");
		}
		if (aTally.Focus) {
			parts << tr("a focus");
		}
		if (parts.isEmpty()) {
			return { };
		}
		auto joined = parts.front();
		for (qsizetype index = 1; index < parts.size(); ++index) {
			joined += (index + 1 == parts.size() ? tr(" and ") : QStringLiteral(", ")) + parts[index];
		}
		return tr("Proposed %1. Accept, edit or discard it in the Tour tab.").arg(joined);
	}

	auto tour_agent_panel::accept_button(const std::size_t aIndex) const -> QPushButton*
	{
		const auto found = Accepts.find(aIndex);
		return found == Accepts.end() ? nullptr : found->second;
	}

	void tour_agent_panel::refresh_toggle()
	{
		const auto count = Session.Runs.size();
		// [[BPR-1914]]. Always offered: the conversation is where a question
		// about the tour is asked, before any run.
		TranscriptToggle->setText(count == 0U ? tr("Conversation") : tr("Conversation (%1)").arg(count));
		TranscriptToggle->setEnabled(true);
		fit_head();
	}

	void tour_agent_panel::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		fit_head();
	}

	void tour_agent_panel::fit_head()
	{
		if (Head == nullptr || Trail == nullptr) {
			return;
		}
		// The row's own width inside the panel's margins, against what Draft
		// the tour and the pair need at their size hints with the gap.
		const auto margins = layout()->contentsMargins();
		// The narrowest the panel goes is the wrapped head's, so a narrow
		// drawer can reach the width at which the pair takes its own row.
		setMinimumWidth(margins.left() + margins.right() + std::max(Draft->sizeHint().width(), Trail->sizeHint().width()));
		const auto room  = width() - margins.left() - margins.right();
		const auto need  = Draft->sizeHint().width() + Head->horizontalSpacing() + Trail->sizeHint().width();
		const auto wrap  = room > 0 && need > room;
		const auto index = Head->indexOf(Trail);
		auto       row = 0, column = 0, rows = 0, columns = 0;
		Head->getItemPosition(index, &row, &column, &rows, &columns);
		if (wrap == (row == 1)) {
			return;
		}
		Head->removeWidget(Trail);
		if (wrap) {
			// [[BPR-1930]]. Wrapped, the row keeps the chat toggle at the right.
			Head->addWidget(Trail, 1, 0, 1, 3, Qt::AlignRight | Qt::AlignVCenter);
		} else {
			Head->addWidget(Trail, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
		}
	}

	void tour_agent_panel::rebuild_proposal()
	{
		// [[BPR-1925]] v003. The boxes placed in the tour go with the old
		// proposal; their places are cleared before the new ones are made.
		for (auto& [index, box] : Boxes) {
			if (box) {
				box->hide();
				box->deleteLater();
			}
		}
		Boxes.clear();
		if (PlaceBegin) {
			PlaceBegin();
		}
		clear(ProposalItems);
		clear(FocusSlot);
		Editors.clear();
		Accepts.clear();
		Relocates.clear();
		Placements.clear();
		Readers.clear();
		EditButtons.clear();
		Accented.erase(std::remove_if(Accented.begin(), Accented.end(), [](const QPointer<QWidget>& aWidget) { return aWidget.isNull(); }), Accented.end());
		Fields.clear();
		if (!Proposal || Proposal->Items.empty()) {
			ProposalBox->hide();
			OutsideList->hide();
			return;
		}
		// [[BPR-1914]] v002. A part an ask turn proposed is decided in that
		// turn's thread, beside what it changes; here only a draft's.
		const auto here    = [this](const core::tour_proposal_item& aItem) { return !core::proposed_in_thread(Session, aItem); };
		const auto pending = std::ranges::count_if(Proposal->Items, [&here](const core::tour_proposal_item& aItem) { return here(aItem) && aItem.Decision == core::tour_proposal_decision::pending; });
		if (std::ranges::none_of(Proposal->Items, here)) {
			ProposalBox->hide();
			OutsideList->hide();
			return;
		}
		if (Proposal->LeftOut.size() == 1U) {
			LeftOut->setText(tr("1 suggested stop pointed outside this pull request and was left out."));
		} else {
			LeftOut->setText(tr("%1 suggested stops pointed outside this pull request and were left out.").arg(Proposal->LeftOut.size()));
		}
		LeftOut->setVisible(!Proposal->LeftOut.empty());
		// [[BPR-1912]]. Only Claude's commands are checked against the list.
		const auto* const proposed = core::find_tour_run(Session, Proposal->Run);
		const auto        outside  = proposed == nullptr ? std::size_t{ 0U } : proposed->OutsideList;
		const auto        see      = QStringLiteral("<a href=\"#outside-list\">%1</a>").arg(tr("See the transcript").toHtmlEscaped());
		OutsideList->setText(outside == 1U ? tr("Claude ran 1 command outside the read-only list. %1.").arg(see) : tr("Claude ran %1 commands outside the read-only list. %2.").arg(outside).arg(see));
		OutsideList->setVisible(outside > 0U);
		// [[BPR-1925]] v003. No list: the parts wait in the tour, grey until
		// accepted. One line says how many.
		const auto waiting = pending == 1 ? tr("1 part waits in the tour, marked grey. Nothing changes until you accept it.") : tr("%1 parts wait in the tour, marked grey. Nothing changes until you accept them.").arg(pending);
		ProposalTitle->setText(tr("<b>Proposed</b>&nbsp;&nbsp;%1").arg(waiting.toHtmlEscaped()));
		AcceptAll->setVisible(pending > 1);
		auto        kept   = std::size_t{ 0U };
		auto* const parent = ProposalItems->parentWidget();
		const auto  code   = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		for (std::size_t index = 0U; index < Proposal->Items.size(); ++index) {
			const auto& item = Proposal->Items[index];
			// [[BPR-1925]] v003. A decided part is the tour's now (or gone):
			// nothing of it is shown again.
			if (!here(item) || item.Decision != core::tour_proposal_decision::pending) {
				continue;
			}
			auto* const box = new QWidget{ parent };
			box->setObjectName(QStringLiteral("tourProposalItem"));
			auto* const itemColumn = new QVBoxLayout{ box };
			itemColumn->setContentsMargins(0, 0, 0, 0);
			itemColumn->setSpacing(4);
			// [[BPR-1925]] v003. Each heading is read on the part it changes: a
			// proposed stop is its title, under its grey ring and number.
			auto heading = QString{ };
			if (item.Part == core::tour_proposal_part::problem) {
				heading = tr("Proposed Problem");
			} else if (item.Part == core::tour_proposal_part::solution) {
				heading = tr("Proposed Solution");
			} else if (item.Part == core::tour_proposal_part::stop_text) {
				heading = tr("Proposed explanation");
			} else if (item.Part == core::tour_proposal_part::remove_stop) {
				// [[BPR-1930]]. A revision of the tour's shape, by its numbers.
				heading = tr("Remove stop %1, %2").arg(stop_number(item.StopId)).arg(stop_title(item.StopId));
			} else if (item.Part == core::tour_proposal_part::merge_stops) {
				auto numbers = QStringList{ };
				for (const auto stop : item.Stops) {
					numbers << QString::number(stop_number(stop));
				}
				const auto last = numbers.takeLast();
				heading         = tr("Merge stops %1 and %2 into %3").arg(numbers.join(QStringLiteral(", ")), last, QString::fromStdString(item.Stop.Title));
			} else if (item.Part == core::tour_proposal_part::split_stop) {
				heading = tr("Split stop %1, %2, into %3").arg(stop_number(item.StopId)).arg(stop_title(item.StopId)).arg(item.Pieces.size());
			} else if (item.Part == core::tour_proposal_part::reorder) {
				heading = tr("Proposed order");
			} else if (item.Part == core::tour_proposal_part::focus) {
				heading = tr("Proposed focus");
			} else {
				heading = QString::fromStdString(item.Stop.Title);
			}
			auto* const title = new QLabel{ heading, box };
			title->setWordWrap(true);
			title->setTextFormat(Qt::PlainText);
			auto titleFont = title->font();
			titleFont.setWeight(QFont::DemiBold);
			title->setFont(titleFont);
			itemColumn->addWidget(title);
			if (item.Part == core::tour_proposal_part::stop) {
				const auto  lines = item.Stop.FirstLine == item.Stop.LastLine ? tr("line %1").arg(item.Stop.FirstLine) : tr("lines %1 to %2").arg(item.Stop.FirstLine).arg(item.Stop.LastLine);
				auto* const where = small(paragraph(QString::fromStdString(item.Stop.Path) + QStringLiteral(", ") + lines, box));
				where->setFont([&] { auto font = code; font.setPointSizeF(where->font().pointSizeF()); return font; }());
				where->setForegroundRole(QPalette::PlaceholderText);
				itemColumn->addWidget(where);
				// [[BPR-1900]], owner 2026-11-06: "strongly prefer on hunk".
				// Near the change: a small quiet label, it publishes as is. Off
				// it: say so plainly, and offer the move; no red, nothing
				// disabled. The author can still move it in the code.
				if (item.Placement != core::tour_stop_placement::on_change && item.Decision == core::tour_proposal_decision::pending) {
					const auto  off  = item.Placement == core::tour_stop_placement::off_change;
					auto* const said = small(paragraph(off ? tr("Outside the change. It won't appear when the tour is published until it's on changed lines.") : tr("Outside the change"), box));
					said->setObjectName(QStringLiteral("tourProposalPlacement"));
					said->setForegroundRole(QPalette::PlaceholderText);
					itemColumn->addWidget(said);
					Placements[index] = said;
					if (off) {
						auto* const move = new quiet_push_button{ tr("Move to nearest change"), box };
						move->setObjectName(QStringLiteral("tourProposalRelocate"));
						move->setFocusPolicy(Qt::TabFocus);
						move->setToolTip(tr("Moves this stop onto the closest changed lines in this file. You can also add it as it is and choose its lines in the code."));
						connect(move, &QPushButton::clicked, this, [this, index] {
							if (RelocateDispatch) {
								RelocateDispatch(index);
							}
						});
						itemColumn->addWidget(move, 0, Qt::AlignLeft);
						Relocates[index] = move;
						Accented.emplace_back(move);
					}
				}
			}
			const auto textual = core::takes_text(item.Part);
			add_revision_lines(itemColumn, item, box);
			if (item.ReplacesAuthorText) {
				auto* const note = small(paragraph(textual ? tr("Accepting replaces what you wrote here.") : tr("What you wrote for this stop goes with it."), box));
				note->setForegroundRole(QPalette::PlaceholderText);
				itemColumn->addWidget(note);
			}
			auto* const editor = new QPlainTextEdit{ box };
			editor->setObjectName(QStringLiteral("tourProposalText"));
			editor->setAccessibleName(heading);
			editor->setPlainText(Edited.contains(index) ? Edited[index] : QString::fromStdString(item.Text));
			editor->setTabChangesFocus(true);
			// [[BPR-1900]] owner look v001 ("that box makes the text feel
			// cramped"): no native focus ring drawn inside the field; room
			// around the words; focus shown by the field's own edge turning
			// the agent accent, the same width, so nothing moves or shrinks.
			editor->setAttribute(Qt::WA_MacShowFocusRect, false);
			editor->setFrameShape(QFrame::NoFrame);
			auto* const frame = new proposal_field_frame{ box };
			frame->setObjectName(QStringLiteral("tourProposalField"));
			editor->document()->setDocumentMargin(8);
			const auto metrics = editor->fontMetrics();
			// The field is as tall as its text (three to twelve lines), so no
			// scroll bar eats into the words; past twelve it scrolls.
			editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
			editor->setFixedHeight(std::clamp(static_cast<int>(item.Text.size() / 38U + 2U), 3, 12) * metrics.lineSpacing() + 18);
			Fields.emplace_back(frame);
			frame->hold(editor);
			itemColumn->addWidget(frame);
			Editors[index] = editor;
			// [[BPR-1915]]. Shown as it will read: rendered, as the preview and
			// GitHub render it. Edit text shows the Markdown source instead.
			auto* const reader = new markdown_body_view{ box };
			reader->setObjectName(QStringLiteral("tourProposalRendered"));
			reader->set_in_place(true);
			reader->setFocusPolicy(Qt::NoFocus);
			// [[BPR-1919]]. Its code as the diff's: font, colours, highlighter.
			if (CodeFont != QFont{ }) {
				reader->set_code_font(CodeFont, TabStop);
			}
			if (CodeTheme) {
				reader->set_code_theme(*CodeTheme);
			}
			if (CodeDispatch) {
				reader->set_code_syntax_dispatch(CodeDispatch);
			}
			reader->set_height_observer([this] { fit_readers(); });
			// [[BPR-1925]] v002. Among the readers before its body asks for
			// colours: a cached answer comes inside that request.
			Readers[index] = reader;
			reader->set_body(editor->toPlainText().toStdString(), "tour-proposal:" + std::to_string(Proposal->Run.value()) + ":" + std::to_string(index) + ":" + std::to_string(qHash(editor->toPlainText())));
			itemColumn->insertWidget(itemColumn->indexOf(frame), reader);
			const auto edit = Editing.contains(index) && textual;
			reader->setVisible(!edit && textual);
			frame->setVisible(edit);
			auto* const buttons = new QHBoxLayout{ };
			buttons->setSpacing(6);
			// [[BPR-1930]]. The primary action says what it does: Remove,
			// Merge, Split, Reorder, Set focus.
			const auto  acceptWords = item.Part == core::tour_proposal_part::remove_stop ? tr("Remove")
				: item.Part == core::tour_proposal_part::merge_stops                     ? tr("Merge")
				: item.Part == core::tour_proposal_part::split_stop                      ? tr("Split")
				: item.Part == core::tour_proposal_part::reorder                         ? tr("Reorder")
				: item.Part == core::tour_proposal_part::focus                           ? tr("Set focus")
																						 : tr("Accept");
			auto* const accept      = new quiet_push_button{ acceptWords, box };
			accept->setObjectName(QStringLiteral("tourProposalAccept"));
			accept->setToolTip(item.Part == core::tour_proposal_part::stop              ? tr("Adds this stop at the end of the tour, with this explanation.")
								   : item.Part == core::tour_proposal_part::stop_text   ? tr("Replaces this stop's explanation with this text.")
								   : item.Part == core::tour_proposal_part::remove_stop ? tr("Takes this stop out of the tour. Undo arrangement brings it back.")
								   : item.Part == core::tour_proposal_part::merge_stops ? tr("Makes these stops one, keeping all their lines, with this explanation. Undo arrangement takes it back.")
								   : item.Part == core::tour_proposal_part::split_stop  ? tr("Puts these stops in this stop's place. Undo arrangement takes it back.")
								   : item.Part == core::tour_proposal_part::reorder     ? tr("Puts the stops in this order. Undo arrangement takes it back.")
								   : item.Part == core::tour_proposal_part::focus       ? tr("Remembers this focus for the tour's later drafts and chat turns.")
																						: tr("Puts this text in the tour's %1.").arg(heading));
			auto* const discard = new quiet_push_button{ tr("Discard"), box };
			discard->setObjectName(QStringLiteral("tourProposalDiscard"));
			discard->setFlat(true);
			// [[BPR-1900]] owner look v001: the first click acts. Neither
			// button takes the keyboard on a press, so a field losing focus
			// mid-click can't move the tab under the pointer; Tab still
			// reaches both.
			accept->setFocusPolicy(Qt::TabFocus);
			discard->setFocusPolicy(Qt::TabFocus);
			auto* const editText = new quiet_push_button{ edit ? tr("Show formatted") : tr("Edit text"), box };
			editText->setObjectName(QStringLiteral("tourProposalEdit"));
			editText->setFlat(true);
			editText->setFocusPolicy(Qt::TabFocus);
			editText->setToolTip(tr("Switches between this text as it will read and its Markdown source, which you can change before you accept it."));
			editText->setVisible(textual);
			EditButtons[index] = editText;
			connect(editText, &QPushButton::clicked, this, [this, index, editor, reader, frame, editText] {
				if (Editing.contains(index)) {
					Editing.erase(index);
					reader->set_body(editor->toPlainText().toStdString(), "tour-proposal:" + std::to_string(Proposal ? Proposal->Run.value() : 0U) + ":" + std::to_string(index) + ":" + std::to_string(qHash(editor->toPlainText())));
					frame->hide();
					reader->show();
					fit_readers();
					editText->setText(tr("Edit text"));
				} else {
					Editing.insert(index);
					reader->hide();
					frame->show();
					editor->setFocus(Qt::OtherFocusReason);
					editText->setText(tr("Show formatted"));
				}
			});
			// [[BPR-1925]] v003, owner: "move the accept into the stops
			// directly. probably where it says ready, next". The part's one
			// primary action at the right, as Ready, next is on a stop.
			buttons->addWidget(editText);
			buttons->addWidget(discard);
			buttons->addStretch(1);
			buttons->addWidget(accept);
			itemColumn->addLayout(buttons);
			Accepts[index] = accept;
			connect(accept, &QPushButton::clicked, this, [this, index, editor] {
				if (DecideDispatch) {
					DecideDispatch(index, core::tour_proposal_decision::accepted, editor->toPlainText());
				}
			});
			connect(discard, &QPushButton::clicked, this, [this, index] {
				if (DecideDispatch) {
					DecideDispatch(index, core::tour_proposal_decision::discarded, { });
				}
			});
			Accented.emplace_back(accept);
			Boxes[index] = box;
			if (Place && Place(index, item, box)) {
				// [[BPR-1930]]. A wrapped line's minimum is its height at a narrow
				// width, which left gaps between the lines of a box on a stop's
				// card; its height for the card's width is what it needs.
				for (auto* const label : box->findChildren<QLabel*>()) {
					if (label->wordWrap()) {
						label->setMinimumSize(1, 1);
					}
				}
				box->installEventFilter(this);
			} else if (item.Part == core::tour_proposal_part::focus) {
				// [[BPR-1930]]. The tour's focus waits under the line.
				FocusSlot->addWidget(box);
				box->installEventFilter(this);
			} else {
				ProposalItems->addWidget(box);
				++kept;
			}
		}
		ProposalItems->addStretch(1);
		// The list is only for a part the tour has no place for.
		ProposalScroll->setVisible(kept > 0U);
		ProposalBox->setVisible(pending > 0);
		QTimer::singleShot(0, this, [this] { fit_readers(); });
		restyle(palette());
	}

	void tour_agent_panel::rebuild_transcript()
	{
		const auto blocked = RunList->blockSignals(true);
		RunList->clear();
		auto chosen = -1;
		for (auto run = Session.Runs.rbegin(); run != Session.Runs.rend(); ++run) {
			const auto asked = run->Entries.empty() ? QString{ } : QString::fromStdString(run->Entries.front().Text);
			// A long question is cut to a few lines' worth in the list; the
			// conversation has it whole.
			auto first = asked.section(QLatin1Char('\n'), 0, 0);
			if (first.size() > 90) {
				first = first.left(88).trimmed() + QStringLiteral("\u2026");
			}
			auto* const item = new QListWidgetItem{ tr("%1\nRun %2, %3").arg(first).arg(run->Id.value()).arg(state_words(run->State)), RunList };
			item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(run->Id.value()));
			item->setToolTip(asked);
			if (run->State == core::tour_agent_run_state::failed) {
				auto heavier = item->font();
				heavier.setWeight(QFont::DemiBold);
				item->setFont(heavier);
			}
			// The run asked for, else the newest.
			if (FocusRun && run->Id == *FocusRun) {
				chosen = RunList->count() - 1;
			}
		}
		if (chosen < 0 && RunList->count() > 0) {
			chosen = 0;
		}
		RunList->setCurrentRow(chosen);
		RunList->blockSignals(blocked);
		show_run(chosen < 0 ? std::nullopt : std::optional{ core::tour_agent_run_id{ RunList->item(chosen)->data(Qt::UserRole).toULongLong() } }, std::nullopt);
	}

	void tour_agent_panel::show_run(const std::optional<core::tour_agent_run_id> aRun, const std::optional<std::size_t> aEntry)
	{
		const auto* const run = aRun ? core::find_tour_run(Session, *aRun) : nullptr;
		ShownRun              = run == nullptr ? std::nullopt : aRun;
		CopyRun->setEnabled(run != nullptr);
		CopyAll->setEnabled(!Session.Runs.empty());
		const auto muted = TranscriptWindow->palette().color(QPalette::Active, QPalette::PlaceholderText).name();
		const auto code  = QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toHtmlEscaped();
		if (Session.Runs.empty()) {
			RunView->setHtml(QStringLiteral("<p style=\"color:%1\">%2</p>").arg(muted, tr("Nothing asked yet. Select a passage in the Problem, the Solution or a stop and press Ask about this, or ask below about the tour or the change.").toHtmlEscaped()));
			return;
		}
		// [[BPR-1914]]. The whole conversation, oldest first, as one
		// document: a selection runs across turns, and the platform's Copy
		// takes it as text.
		RunView->clear();
		auto cursor = QTextCursor{ RunView->document() };
		for (const auto& each : Session.Runs) {
			append_run(cursor, each, muted, code);
		}
		if (run == nullptr) {
			return;
		}
		const auto anchor = aEntry ? QStringLiteral("run-%1-entry-%2").arg(run->Id.value()).arg(*aEntry) : QStringLiteral("run-%1").arg(run->Id.value());
		RunView->scrollToAnchor(anchor);
	}

	void tour_agent_panel::append_run(QTextCursor& aCursor, const core::tour_agent_run& aRun, const QString& aMuted, const QString& aCode) const
	{
		const auto ask    = aRun.Request == core::tour_agent_request::ask;
		const auto accent = agent_accent(TranscriptWindow->palette()).name();
		const auto asked  = ask ? QString::fromStdString(aRun.Question) : aRun.Entries.empty() ? QString{ }
																							   : QString::fromStdString(aRun.Entries.front().Text);
		auto       html   = QStringLiteral("<a name=\"run-%1\"></a>").arg(aRun.Id.value());
		// What was asked: the passage, quoted, then the question.
		if (ask && aRun.Passage) {
			html += QStringLiteral("<p style=\"margin-top:16px; margin-bottom:0; color:%1\"><i>%2</i></p><blockquote style=\"margin-top:2px; margin-bottom:4px; margin-left:14px; color:%1\">\u201C%3\u201D</blockquote>")
						.arg(aMuted, passage_words(*aRun.Passage).toHtmlEscaped(), QString::fromStdString(aRun.Passage->Text).toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
		}
		html += QStringLiteral("<p style=\"margin-top:%1px; margin-bottom:2px\"><b>%2</b></p>").arg(ask && aRun.Passage ? 2 : 16).arg(asked.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
		html += QStringLiteral("<p style=\"color:%1; margin-top:0; margin-bottom:4px; font-size:small\">%2</p>").arg(aMuted, tr("Run %1, %2, %3.").arg(aRun.Id.value()).arg(state_words(aRun.State), tools_words(aRun.Tools)).toHtmlEscaped());
		// What it did: each step, quiet, as the transcript has always said it.
		auto steps = QString{ };
		auto said  = QString{ };
		for (std::size_t index = 1U; index < aRun.Entries.size(); ++index) {
			const auto& entry = aRun.Entries[index];
			const auto  text  = QString::fromStdString(entry.Text).toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
			if (entry.Kind == core::tour_transcript_kind::answered) {
				said = text;
				continue;
			}
			const auto tool = entry.Kind == core::tour_transcript_kind::searched || entry.Kind == core::tour_transcript_kind::read || entry.Kind == core::tour_transcript_kind::ran;
			auto       line = tool ? QStringLiteral("<span style=\"font-family:'%1'\">%2</span>").arg(aCode, text) : text;
			// How a run ended: its first line is the headline, the failure's
			// own log under it in the plain weight.
			if (entry.Kind == core::tour_transcript_kind::ended) {
				const auto split = text.indexOf(QStringLiteral("<br>"));
				line             = split < 0 ? QStringLiteral("<b>%1</b>").arg(text) : QStringLiteral("<b>%1</b>%2").arg(text.left(split), text.mid(split));
			}
			if (const auto note = note_words(entry.Note); !note.isEmpty()) {
				line += QStringLiteral("<br><span style=\"color:%1\"><i>%2</i></span>").arg(aMuted, note.toHtmlEscaped());
			}
			steps += QStringLiteral("<tr><td valign=\"top\" align=\"right\" style=\"color:%1; padding-right:10px\" width=\"72\"><a name=\"run-%2-entry-%3\"></a><a name=\"entry-%3\"></a>%4</td><td valign=\"top\">%5</td></tr>")
						 .arg(aMuted)
						 .arg(aRun.Id.value())
						 .arg(index)
						 .arg(verb(entry.Kind).toHtmlEscaped(), line);
		}
		if (!steps.isEmpty()) {
			html += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"2\" width=\"100%\" style=\"font-size:small\">") + steps + QStringLiteral("</table>");
		}
		if (aRun.Dropped > 0U) {
			html += QStringLiteral("<p style=\"color:%1; font-size:small\">%2</p>").arg(aMuted, (aRun.Dropped == 1U ? tr("1 more step was not kept.") : tr("%1 more steps were not kept.").arg(aRun.Dropped)).toHtmlEscaped());
		}
		const auto reply = ask && aRun.State == core::tour_agent_run_state::answered;
		if (reply || !said.isEmpty()) {
			html += QStringLiteral("<p style=\"margin-top:8px; margin-bottom:2px; color:%1\"><b>%2</b></p>").arg(accent, tr("Tour agent").toHtmlEscaped());
		}
		if (!reply && !said.isEmpty()) {
			html += QStringLiteral("<p style=\"margin-top:0\">%1</p>").arg(said);
		}
		// Each turn starts a block of its own, never inside the last reply's
		// list or paragraph.
		aCursor.movePosition(QTextCursor::End);
		if (!aCursor.atStart()) {
			auto gap = QTextBlockFormat{ };
			gap.setTopMargin(22);
			aCursor.insertBlock(gap, QTextCharFormat{ });
		}
		aCursor.insertHtml(html);
		if (!reply) {
			return;
		}
		// [[BPR-1914]], [[BPR-1915]]. The reply as the agent wrote it, read as
		// a body is read: paragraphs, lists, code.
		auto scratch = QTextDocument{ };
		scratch.setDefaultFont(RunView->document()->defaultFont());
		detail::import_github_markdown(scratch, QString::fromStdString(aRun.Reply));
		// Code in the code face the transcript's commands use.
		auto codeFormat = QTextCharFormat{ };
		codeFormat.setFontFamilies({ QFontDatabase::systemFont(QFontDatabase::FixedFont).family() });
		for (auto block = scratch.begin(); block.isValid(); block = block.next()) {
			for (auto part = block.begin(); !part.atEnd(); ++part) {
				if (part.fragment().charFormat().fontFixedPitch()) {
					auto span = QTextCursor{ &scratch };
					span.setPosition(part.fragment().position());
					span.setPosition(part.fragment().position() + part.fragment().length(), QTextCursor::KeepAnchor);
					span.mergeCharFormat(codeFormat);
				}
			}
		}
		aCursor.movePosition(QTextCursor::End);
		aCursor.insertBlock(QTextBlockFormat{ }, QTextCharFormat{ });
		aCursor.insertFragment(QTextDocumentFragment{ &scratch });
		if (const auto proposed = proposed_words(aRun.Proposed); !proposed.isEmpty()) {
			aCursor.movePosition(QTextCursor::End);
			aCursor.insertBlock(QTextBlockFormat{ }, QTextCharFormat{ });
			aCursor.insertHtml(QStringLiteral("<p style=\"color:%1; margin-top:6px\"><i>%2</i></p>").arg(accent, proposed.toHtmlEscaped()));
		}
	}

}
