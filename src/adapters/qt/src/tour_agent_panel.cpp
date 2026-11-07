#include "bpr/qt/tour_agent_panel.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QAbstractTextDocumentLayout>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBlock>
#include <QTextDocument>
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
				layout()->addWidget(aField);
				aField->installEventFilter(this);
			}

		protected:
			bool eventFilter(QObject* aWatched, QEvent* aEvent) override
			{
				if (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut || aEvent->type() == QEvent::EnabledChange) {
					update();
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
				if (auto* const widget = item->widget()) {
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

		auto* const row = new QHBoxLayout{ };
		row->setSpacing(8);
		Draft = new quiet_push_button{ tr("Draft the tour"), this };
		Draft->setObjectName(QStringLiteral("tourDraftTour"));
		Draft->setToolTip(tr("Asks the drafting agent, in the background, to read this pull request's code and propose the Problem, the Solution and the stops. You accept, edit or discard each one."));
		row->addWidget(Draft, 0, Qt::AlignVCenter);
		Status = small(paragraph({ }, this));
		Status->setObjectName(QStringLiteral("tourDraftStatus"));
		row->addWidget(Status, 1, Qt::AlignVCenter);
		TranscriptToggle = new quiet_push_button{ this };
		TranscriptToggle->setObjectName(QStringLiteral("tourSessionTranscriptOpen"));
		TranscriptToggle->setFlat(true);
		TranscriptToggle->setCheckable(true);
		TranscriptToggle->setToolTip(tr("What the tour session read and answered, run by run. It never opens by itself."));
		row->addWidget(TranscriptToggle, 0, Qt::AlignVCenter);
		column->addLayout(row);
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
		connect(TranscriptToggle, &QPushButton::toggled, this, [this](const bool aOpen) {
			TranscriptBox->setVisible(aOpen);
			if (aOpen) {
				rebuild_transcript();
			}
			refresh_toggle();
		});

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

		// The transcript: opened from here, never pushed in front.
		TranscriptBox = new QWidget{ this };
		TranscriptBox->setObjectName(QStringLiteral("tourSessionTranscript"));
		TranscriptBox->setAccessibleName(tr("Tour session transcript"));
		auto* const transcriptColumn = new QVBoxLayout{ TranscriptBox };
		transcriptColumn->setContentsMargins(0, 2, 0, 4);
		transcriptColumn->setSpacing(4);
		auto* const transcriptTitle = small(paragraph(tr("The tour session is separate from the Agent tab's conversation. Each request is a fresh run that is given the tour so far."), TranscriptBox));
		transcriptTitle->setForegroundRole(QPalette::PlaceholderText);
		transcriptColumn->addWidget(transcriptTitle);
		TranscriptScroll = new QScrollArea{ TranscriptBox };
		TranscriptScroll->setFocusPolicy(Qt::NoFocus);
		TranscriptScroll->setWidgetResizable(true);
		TranscriptScroll->setFrameShape(QFrame::NoFrame);
		TranscriptScroll->setMinimumHeight(220);
		TranscriptScroll->setMaximumHeight(320);
		TranscriptScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const transcriptBody = new QWidget{ TranscriptScroll };
		TranscriptItems            = new QVBoxLayout{ transcriptBody };
		TranscriptItems->setContentsMargins(0, 0, 4, 0);
		TranscriptItems->setSpacing(8);
		TranscriptScroll->setWidget(transcriptBody);
		transcriptColumn->addWidget(TranscriptScroll);
		TranscriptBox->hide();
		column->addWidget(TranscriptBox);
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
		}
		Session  = aSession;
		Proposal = aProposal;
		rebuild_proposal();
		if (TranscriptBox->isVisible()) {
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
		// What it reads and where the proposal lands is the tab's notice
		// line, under this panel; the control itself says it is running.
		Status->hide();
		refresh_toggle();
	}

	void tour_agent_panel::set_available(const bool aReady, const QString& aWhy)
	{
		Ready = aReady;
		Why   = aWhy;
		set_running(RunningDraft, RunningAny);
	}

	void tour_agent_panel::set_failure_notice(QWidget* aNotice)
	{
		aNotice->setParent(this);
		FailureSlot->addWidget(aNotice);
	}

	void tour_agent_panel::open_transcript(const std::optional<core::tour_agent_run_id> aRun)
	{
		FocusRun = aRun;
		TranscriptToggle->setChecked(true);
		rebuild_transcript();
		TranscriptBox->show();
	}

	void tour_agent_panel::restyle(const QPalette& aGround)
	{
		for (auto* const widget : { static_cast<QWidget*>(Draft), static_cast<QWidget*>(TranscriptToggle), static_cast<QWidget*>(Status), static_cast<QWidget*>(ProposalTitle) }) {
			apply_agent_accent(*widget, aGround);
		}
		Draft->setIcon(drawer_icon(drawer_glyph::agent, Draft->palette()));
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
				lines << verb(entry.Kind) + QStringLiteral(" ") + QString::fromStdString(entry.Text);
			}
		}
		return lines.join(QLatin1Char('\n'));
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

	auto tour_agent_panel::accept_button(const std::size_t aIndex) const -> QPushButton*
	{
		const auto found = Accepts.find(aIndex);
		return found == Accepts.end() ? nullptr : found->second;
	}

	void tour_agent_panel::refresh_toggle()
	{
		const auto count = Session.Runs.size();
		TranscriptToggle->setText(TranscriptToggle->isChecked() ? tr("Hide transcript") : count == 0U ? tr("Transcript")
																									  : tr("Transcript (%1)").arg(count));
		TranscriptToggle->setEnabled(count > 0U);
	}

	void tour_agent_panel::rebuild_proposal()
	{
		clear(ProposalItems);
		Editors.clear();
		Accepts.clear();
		Relocates.clear();
		Placements.clear();
		Accented.erase(std::remove_if(Accented.begin(), Accented.end(), [](const QPointer<QWidget>& aWidget) { return aWidget.isNull(); }), Accented.end());
		Fields.clear();
		if (!Proposal || Proposal->Items.empty()) {
			ProposalBox->hide();
			return;
		}
		const auto pending = std::ranges::count_if(Proposal->Items, [](const core::tour_proposal_item& aItem) { return aItem.Decision == core::tour_proposal_decision::pending; });
		if (Proposal->LeftOut.size() == 1U) {
			LeftOut->setText(tr("1 suggested stop pointed outside this pull request and was left out."));
		} else {
			LeftOut->setText(tr("%1 suggested stops pointed outside this pull request and were left out.").arg(Proposal->LeftOut.size()));
		}
		LeftOut->setVisible(!Proposal->LeftOut.empty());
		ProposalTitle->setText(tr("<b>Proposed tour</b>&nbsp;&nbsp;%1").arg(tr("Accept, edit or discard each part. Nothing changes until you accept it. %n left to decide.", nullptr, static_cast<int>(pending)).toHtmlEscaped()));
		auto* const parent     = ProposalItems->parentWidget();
		const auto  code       = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		int         stopNumber = 0;
		for (std::size_t index = 0U; index < Proposal->Items.size(); ++index) {
			const auto& item = Proposal->Items[index];
			auto* const box  = new QWidget{ parent };
			box->setObjectName(QStringLiteral("tourProposalItem"));
			auto* const itemColumn = new QVBoxLayout{ box };
			itemColumn->setContentsMargins(0, 0, 0, 0);
			itemColumn->setSpacing(4);
			auto heading = QString{ };
			if (item.Part == core::tour_proposal_part::problem) {
				heading = tr("Problem");
			} else if (item.Part == core::tour_proposal_part::solution) {
				heading = tr("Solution");
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
			editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			const auto fit = [editor, lineSpacing = metrics.lineSpacing()] {
				// The text's own laid-out height, block by block, in pixels.
				auto text = 0.0;
				for (auto block = editor->document()->begin(); block.isValid(); block = block.next()) {
					text += editor->document()->documentLayout()->blockBoundingRect(block).height();
				}
				const auto margins = 2.0 * editor->document()->documentMargin() + 2;
				const auto height  = static_cast<int>(std::ceil(std::clamp(text, 3.0 * lineSpacing, 12.0 * lineSpacing) + margins));
				editor->setVerticalScrollBarPolicy(text > 12.0 * lineSpacing ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
				if (editor->height() != height) {
					editor->setFixedHeight(height);
				}
			};
			editor->setFixedHeight(std::clamp(static_cast<int>(item.Text.size() / 38U + 2U), 3, 12) * metrics.lineSpacing() + 18);
			connect(editor->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, editor, fit);
			QTimer::singleShot(0, editor, fit);
			Fields.emplace_back(frame);
			frame->hold(editor);
			itemColumn->addWidget(frame);
			Editors[index]      = editor;
			auto* const buttons = new QHBoxLayout{ };
			buttons->setSpacing(6);
			auto* const accept = new quiet_push_button{ tr("Accept"), box };
			accept->setObjectName(QStringLiteral("tourProposalAccept"));
			accept->setToolTip(item.Part == core::tour_proposal_part::stop ? tr("Adds this stop at the end of the tour, with this explanation.") : tr("Puts this text in the tour's %1.").arg(heading));
			auto* const discard = new quiet_push_button{ tr("Discard"), box };
			discard->setObjectName(QStringLiteral("tourProposalDiscard"));
			discard->setFlat(true);
			// [[BPR-1900]] owner look v001: the first click acts. Neither
			// button takes the keyboard on a press, so a field losing focus
			// mid-click can't move the tab under the pointer; Tab still
			// reaches both.
			accept->setFocusPolicy(Qt::TabFocus);
			discard->setFocusPolicy(Qt::TabFocus);
			buttons->addWidget(accept);
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
		restyle(palette());
	}

	void tour_agent_panel::rebuild_transcript()
	{
		clear(TranscriptItems);
		const auto code  = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		auto*      focus = static_cast<QWidget*>(nullptr);
		for (auto run = Session.Runs.rbegin(); run != Session.Runs.rend(); ++run) {
			auto* const box = new agent_margin{ TranscriptItems->parentWidget() };
			box->setObjectName(QStringLiteral("tourSessionRun"));
			auto* const runColumn = new QVBoxLayout{ box };
			runColumn->setContentsMargins(12, 6, 8, 6);
			runColumn->setSpacing(3);
			const auto  asked = run->Entries.empty() ? QString{ } : QString::fromStdString(run->Entries.front().Text);
			auto* const title = new QLabel{ tr("<b>%1</b>&nbsp;&nbsp;Run %2, %3, %4").arg(asked.toHtmlEscaped()).arg(run->Id.value()).arg(tools_words(run->Tools).toHtmlEscaped(), state_words(run->State).toHtmlEscaped()), box };
			title->setWordWrap(true);
			runColumn->addWidget(small(title));
			for (std::size_t index = 1U; index < run->Entries.size(); ++index) {
				const auto& entry   = run->Entries[index];
				auto* const line    = new QWidget{ box };
				auto* const lineRow = new QHBoxLayout{ line };
				lineRow->setContentsMargins(0, 0, 0, 0);
				lineRow->setSpacing(8);
				auto* const word = small(new QLabel{ verb(entry.Kind), line });
				word->setForegroundRole(QPalette::PlaceholderText);
				word->setFixedWidth(word->fontMetrics().horizontalAdvance(tr("Searched")) + 6);
				word->setAlignment(Qt::AlignRight | Qt::AlignTop);
				lineRow->addWidget(word, 0, Qt::AlignTop);
				auto* const text = small(paragraph(QString::fromStdString(entry.Text), line));
				text->setTextInteractionFlags(Qt::TextSelectableByMouse);
				if (entry.Kind == core::tour_transcript_kind::searched || entry.Kind == core::tour_transcript_kind::read || entry.Kind == core::tour_transcript_kind::ran) {
					auto font = code;
					font.setPointSizeF(text->font().pointSizeF());
					text->setFont(font);
				}
				lineRow->addWidget(text, 1);
				runColumn->addWidget(line);
			}
			if (run->Dropped > 0U) {
				auto* const more = small(paragraph(tr("%n more steps were not kept.", nullptr, static_cast<int>(run->Dropped)), box));
				more->setForegroundRole(QPalette::PlaceholderText);
				runColumn->addWidget(more);
			}
			TranscriptItems->addWidget(box);
			if (FocusRun && run->Id == *FocusRun) {
				focus = box;
			}
		}
		TranscriptItems->addStretch(1);
		if (focus != nullptr) {
			TranscriptScroll->ensureWidgetVisible(focus);
		}
	}
}
