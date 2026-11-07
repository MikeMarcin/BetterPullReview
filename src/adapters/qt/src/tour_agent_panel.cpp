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
		auto* const row = new QHBoxLayout{ };
		row->setSpacing(8);
		Draft = new quiet_push_button{ tr("Draft the tour"), this };
		Draft->setObjectName(QStringLiteral("tourDraftTour"));
		Draft->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Draft->setToolTip(tr("Asks the drafting agent, in the background, to read this pull request's code and propose the Problem, the Solution and the stops. You accept, edit or discard each one."));
		row->addWidget(Draft, 0, Qt::AlignVCenter);
		Status = small(paragraph({ }, this));
		Status->setObjectName(QStringLiteral("tourDraftStatus"));
		row->addWidget(Status, 1, Qt::AlignVCenter);
		row->addStretch(1);
		TranscriptToggle = new quiet_push_button{ this };
		TranscriptToggle->setObjectName(QStringLiteral("tourSessionTranscriptOpen"));
		TranscriptToggle->setFlat(true);
		TranscriptToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		TranscriptToggle->setToolTip(tr("Opens the tour conversation in its own window: ask about the tour, and read what each run read and answered. It never opens by itself."));
		row->addWidget(TranscriptToggle, 0, Qt::AlignVCenter);
		column->addLayout(row);
		// [[BPR-1914]] v003. A side chat about the tour as a whole: one quiet
		// glyph beside Conversation; the chat docks at the window's edge.
		ChatTour = new QToolButton{ this };
		ChatTour->setObjectName(QStringLiteral("tourChatAboutTour"));
		ChatTour->setAutoRaise(true);
		ChatTour->setFocusPolicy(Qt::TabFocus);
		ChatTour->setAccessibleName(tr("Open side chat about the tour"));
		row->insertWidget(row->indexOf(TranscriptToggle), ChatTour, 0, Qt::AlignVCenter);
		connect(ChatTour, &QToolButton::clicked, this, [this] {
			if (ChatDispatch) {
				ChatDispatch();
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
		proposalColumn->addWidget(ProposalTitle);
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
	}

	void tour_agent_panel::set_running(const bool aDraftTour, const bool aAny)
	{
		RunningDraft = aDraftTour;
		RunningAny   = aAny;
		Draft->setText(aDraftTour ? tr("Stop drafting") : tr("Draft the tour"));
		Draft->setToolTip(aDraftTour ? tr("Drafting the tour in the background. Stop it; nothing in the tour changes.") : Ready ? tr("Asks the drafting agent, in the background, to read this pull request's code and propose the Problem, the Solution and the stops. You accept, edit or discard each one.")
																																: Why);
		Draft->setEnabled(Ready && (aDraftTour || !aAny));
		ChatTour->setEnabled(true);
		// What it reads and where the proposal lands is the tab's notice
		// line, under this panel; the control itself says it is running.
		Status->hide();
		refresh_toggle();
	}

	void tour_agent_panel::set_available(const bool aReady, const QString& aWhy)
	{
		Ready = aReady;
		Why   = aWhy;
		ChatTour->setToolTip(aReady ? tr("Open a side chat about the whole tour and the change. Each part of the tour has its own: use the chat glyph beside it.") : aWhy);
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
		const auto width = std::max(1, ProposalScroll->viewport()->width() - ProposalItems->contentsMargins().left() - ProposalItems->contentsMargins().right());
		for (const auto& [index, reader] : Readers) {
			if (reader != nullptr) {
				reader->setFixedHeight(reader->isHidden() ? 0 : reader->heightForWidth(width));
			}
		}
	}

	auto tour_agent_panel::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aEvent->type() == QEvent::Resize && ProposalScroll != nullptr && aWatched == ProposalScroll->viewport()) {
			QTimer::singleShot(0, this, [this] { fit_readers(); });
		}
		return QWidget::eventFilter(aWatched, aEvent);
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
	}

	void tour_agent_panel::rebuild_proposal()
	{
		clear(ProposalItems);
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
		ProposalTitle->setText(tr("<b>Proposed tour</b>&nbsp;&nbsp;%1").arg(tr("Accept, edit or discard each part. Nothing changes until you accept it. %n left to decide.", nullptr, static_cast<int>(pending)).toHtmlEscaped()));
		auto* const parent     = ProposalItems->parentWidget();
		const auto  code       = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		int         stopNumber = 0;
		for (std::size_t index = 0U; index < Proposal->Items.size(); ++index) {
			const auto& item = Proposal->Items[index];
			if (!here(item)) {
				continue;
			}
			auto* const box = new QWidget{ parent };
			box->setObjectName(QStringLiteral("tourProposalItem"));
			auto* const itemColumn = new QVBoxLayout{ box };
			itemColumn->setContentsMargins(0, 0, 0, 0);
			itemColumn->setSpacing(4);
			auto heading = QString{ };
			if (item.Part == core::tour_proposal_part::problem) {
				heading = tr("Problem");
			} else if (item.Part == core::tour_proposal_part::solution) {
				heading = tr("Solution");
			} else if (item.Part == core::tour_proposal_part::stop_text) {
				heading = tr("New explanation for %1").arg(QString::fromStdString(item.Stop.Title));
			} else {
				++stopNumber;
				heading = tr("Stop %1. %2").arg(stopNumber).arg(QString::fromStdString(item.Stop.Title));
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
			if (item.Decision != core::tour_proposal_decision::pending) {
				const auto  stopWords = item.Placement == core::tour_stop_placement::off_change
					? tr("Added to the tour. It won't appear when the tour is published until it's on changed lines: choose them in the code and use Use selected lines.")
					: tr("Added to the tour. Confirm its lines in the code before it is Ready.");
				auto* const decided   = small(paragraph(item.Decision == core::tour_proposal_decision::accepted ? (item.Part == core::tour_proposal_part::stop ? stopWords : tr("Accepted into the tour."))
																												: tr("Discarded."),
														box));
				decided->setForegroundRole(QPalette::PlaceholderText);
				itemColumn->addWidget(decided);
				ProposalItems->addWidget(box);
				continue;
			}
			if (item.ReplacesAuthorText) {
				auto* const note = small(paragraph(tr("Accepting replaces what you wrote here."), box));
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
			reader->set_body(editor->toPlainText().toStdString(), "tour-proposal:" + std::to_string(Proposal->Run.value()) + ":" + std::to_string(index) + ":" + std::to_string(qHash(editor->toPlainText())));
			itemColumn->insertWidget(itemColumn->indexOf(frame), reader);
			Readers[index]  = reader;
			const auto edit = Editing.contains(index);
			reader->setVisible(!edit);
			frame->setVisible(edit);
			auto* const buttons = new QHBoxLayout{ };
			buttons->setSpacing(6);
			auto* const accept = new quiet_push_button{ tr("Accept"), box };
			accept->setObjectName(QStringLiteral("tourProposalAccept"));
			accept->setToolTip(item.Part == core::tour_proposal_part::stop            ? tr("Adds this stop at the end of the tour, with this explanation.")
								   : item.Part == core::tour_proposal_part::stop_text ? tr("Replaces this stop's explanation with this text.")
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
			buttons->addWidget(accept);
			buttons->addWidget(editText);
			buttons->addWidget(discard);
			buttons->addStretch(1);
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
			ProposalItems->addWidget(box);
		}
		ProposalItems->addStretch(1);
		ProposalBox->show();
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
