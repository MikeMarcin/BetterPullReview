#include "bpr/qt/follow_up_window.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/stick_to_bottom.hpp"

#include <QAccessible>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>

#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto plain_label(const QString& aText, QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			return label;
		}

		[[nodiscard]] auto quiet_label(const QString& aText, QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		/// A bordered well on the view's Base, as the conversation cards are.
		[[nodiscard]] auto well(QWidget* aParent, const QString& aName) -> QFrame*
		{
			auto* const frame = new QFrame{ aParent };
			frame->setObjectName(aName);
			frame->setFrameShape(QFrame::StyledPanel);
			frame->setBackgroundRole(QPalette::Base);
			frame->setAutoFillBackground(true);
			return frame;
		}

		[[nodiscard]] auto place_words(const core::follow_up_subject& aSubject) -> QString
		{
			if (aSubject.Path.empty()) {
				return QObject::tr("the pull request");
			}
			auto file = QString::fromStdString(aSubject.Path).section(QLatin1Char{ '/' }, -1);
			if (aSubject.First == 0U) {
				return file;
			}
			if (aSubject.Last <= aSubject.First) {
				return QStringLiteral("%1:%2").arg(file).arg(aSubject.First);
			}
			return QStringLiteral("%1:%2–%3").arg(file).arg(aSubject.First).arg(aSubject.Last);
		}

		[[nodiscard]] auto heading_words(const core::follow_up_subject& aSubject) -> QString
		{
			switch (aSubject.Kind) {
			case core::follow_up_subject_kind::suggested_comment: return QObject::tr("Suggested comment at %1").arg(place_words(aSubject));
			case core::follow_up_subject_kind::draft:             return QObject::tr("Your draft at %1").arg(place_words(aSubject));
			case core::follow_up_subject_kind::thread:            return QObject::tr("Conversation at %1").arg(place_words(aSubject));
			}
			return { };
		}

		[[nodiscard]] auto placeholder_words(const core::follow_up_subject_kind aKind) -> QString
		{
			switch (aKind) {
			case core::follow_up_subject_kind::suggested_comment: return QObject::tr("Ask about this suggested comment");
			case core::follow_up_subject_kind::draft:             return QObject::tr("Ask about your draft");
			case core::follow_up_subject_kind::thread:            return QObject::tr("Ask about this conversation");
			}
			return { };
		}

		[[nodiscard]] auto proposal_words(const core::follow_up_apply aApply) -> QString
		{
			switch (aApply) {
			case core::follow_up_apply::update_suggestion: return QObject::tr("Proposed suggested comment");
			case core::follow_up_apply::update_draft:      return QObject::tr("Proposed draft");
			case core::follow_up_apply::draft_reply:       return QObject::tr("Proposed reply");
			}
			return { };
		}

		[[nodiscard]] auto item_words(const core::follow_up_subject& aSubject) -> QString
		{
			if (aSubject.Kind != core::follow_up_subject_kind::thread) {
				return QString::fromStdString(aSubject.Text);
			}
			auto lines = QStringList{ };
			for (const auto& post : aSubject.Thread) {
				lines << QStringLiteral("%1: %2").arg(post.Author.empty() ? QObject::tr("Someone") : QString::fromStdString(post.Author), QString::fromStdString(post.Body).trimmed());
			}
			return lines.join(QStringLiteral("\n\n"));
		}
	} // namespace

	auto follow_up_apply_words(const core::follow_up_apply aApply) -> QString
	{
		switch (aApply) {
		case core::follow_up_apply::update_suggestion: return QObject::tr("Update suggested comment");
		case core::follow_up_apply::update_draft:      return QObject::tr("Update draft");
		case core::follow_up_apply::draft_reply:       return QObject::tr("Draft reply");
		}
		return { };
	}

	auto follow_up_refusal_words(const core::follow_up_refusal aRefusal) -> QString
	{
		switch (aRefusal) {
		case core::follow_up_refusal::none:            return { };
		case core::follow_up_refusal::busy:            return QObject::tr("The agent is still answering. Wait for it, or press Stop.");
		case core::follow_up_refusal::empty_question:  return QObject::tr("Write a question first.");
		case core::follow_up_refusal::not_offered:     return QObject::tr("That cannot be applied to this item.");
		case core::follow_up_refusal::too_many_rounds: return QObject::tr("This conversation is as long as one follow-up keeps. Apply what it settled, or start again from the item.");
		case core::follow_up_refusal::stale:           return QObject::tr("That proposal is no longer the one shown. Nothing changed.");
		case core::follow_up_refusal::no_proposal:     return QObject::tr("There is no proposal to use.");
		case core::follow_up_refusal::empty_answer:    return QObject::tr("The agent answered nothing. Nothing changed.");
		case core::follow_up_refusal::unchanged:       return QObject::tr("The agent proposed the same text. Nothing changed.");
		case core::follow_up_refusal::too_long:        return QObject::tr("The agent's text is longer than GitHub keeps. Nothing changed.");
		case core::follow_up_refusal::target_changed:  return QObject::tr("The text changed since the agent proposed this. Nothing was replaced; ask for the update again.");
		case core::follow_up_refusal::failed:          return QObject::tr("The agent did not answer. Nothing changed.");
		}
		return { };
	}

	follow_up_window::follow_up_window(QWidget* const aParent)
		: QWidget{ aParent, Qt::Window }
	{
		setObjectName(QStringLiteral("followUpWindow"));
		setWindowTitle(tr("Follow-up"));
		setAccessibleName(tr("Follow-up with the agent"));
		setMinimumSize(380, 420);
		resize(520, 640);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(14, 12, 14, 12);
		column->setSpacing(8);

		// The item, as it is: what the conversation is about, quoted.
		Heading = new QLabel{ this };
		Heading->setObjectName(QStringLiteral("followUpHeading"));
		Heading->setTextFormat(Qt::PlainText);
		Heading->setWordWrap(true);
		auto headingFont = Heading->font();
		headingFont.setWeight(QFont::DemiBold);
		headingFont.setPointSizeF(headingFont.pointSizeF() * 1.08);
		Heading->setFont(headingFont);
		column->addWidget(Heading);
		Item               = well(this, QStringLiteral("followUpItem"));
		auto* const inItem = new QVBoxLayout{ Item };
		inItem->setContentsMargins(10, 8, 10, 8);
		inItem->setSpacing(4);
		ItemTitle = quiet_label({ }, Item, QStringLiteral("followUpItemTitle"));
		inItem->addWidget(ItemTitle);
		ItemText = plain_label({ }, Item, QStringLiteral("followUpItemText"));
		inItem->addWidget(ItemText);
		column->addWidget(Item);

		// The rounds, newest last, scrolling as one.
		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("followUpRounds"));
		Scroll->setWidgetResizable(true);
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		RoundsHolder = new QWidget{ Scroll };
		Rounds       = new QVBoxLayout{ RoundsHolder };
		Rounds->setContentsMargins(0, 4, 0, 4);
		Rounds->setSpacing(10);
		Rounds->addStretch(1);
		Scroll->setWidget(RoundsHolder);
		// [[BPR-2296]]. At its end, the conversation stays there as it grows.
		stick_to_bottom::attach(*Scroll);
		column->addWidget(Scroll, 1);
		Working = quiet_label({ }, this, QStringLiteral("followUpWorking"));
		Working->hide();
		column->addWidget(Working);

		// A proposal, beside what it replaces; nothing changes until Use.
		Proposal               = well(this, QStringLiteral("followUpProposal"));
		auto* const inProposal = new QVBoxLayout{ Proposal };
		inProposal->setContentsMargins(10, 8, 10, 8);
		inProposal->setSpacing(4);
		ProposalHeading = new QLabel{ Proposal };
		ProposalHeading->setObjectName(QStringLiteral("followUpProposalHeading"));
		ProposalHeading->setFont(headingFont);
		inProposal->addWidget(ProposalHeading);
		ProposalBefore = quiet_label({ }, Proposal, QStringLiteral("followUpProposalBefore"));
		ProposalBefore->setTextInteractionFlags(Qt::TextSelectableByMouse);
		inProposal->addWidget(ProposalBefore);
		ProposalAfter = plain_label({ }, Proposal, QStringLiteral("followUpProposalAfter"));
		inProposal->addWidget(ProposalAfter);
		auto* const proposalRow = new QHBoxLayout{ };
		proposalRow->setSpacing(6);
		proposalRow->addStretch(1);
		DiscardButton = new quiet_push_button{ tr("Discard"), Proposal };
		DiscardButton->setObjectName(QStringLiteral("followUpDiscard"));
		DiscardButton->setAccessibleDescription(tr("Drops the proposal. Nothing changes."));
		proposalRow->addWidget(DiscardButton);
		UseButton = new QPushButton{ tr("Use this"), Proposal };
		UseButton->setObjectName(QStringLiteral("followUpUse"));
		proposalRow->addWidget(UseButton);
		inProposal->addLayout(proposalRow);
		Proposal->hide();
		column->addWidget(Proposal);

		Status = quiet_label({ }, this, QStringLiteral("followUpStatus"));
		Status->hide();
		column->addWidget(Status);

		// The question, and under it the one apply action and Ask.
		Question = new QPlainTextEdit{ this };
		Question->setObjectName(QStringLiteral("followUpQuestion"));
		Question->setAccessibleName(tr("Question"));
		Question->setTabChangesFocus(true);
		Question->setFixedHeight(4 * Question->fontMetrics().lineSpacing() + 12);
		Question->installEventFilter(this);
		column->addWidget(Question);
		auto* const row = new QHBoxLayout{ };
		row->setSpacing(6);
		ApplyButton = new quiet_push_button{ this };
		ApplyButton->setObjectName(QStringLiteral("followUpApply"));
		row->addWidget(ApplyButton);
		row->addStretch(1);
		AskButton = new QPushButton{ tr("Ask"), this };
		AskButton->setObjectName(QStringLiteral("followUpAsk"));
		AskButton->setDefault(true);
		row->addWidget(AskButton);
		column->addLayout(row);

		connect(AskButton, &QPushButton::clicked, this, [this] {
			if (Running) {
				if (Intents.Stop) {
					Intents.Stop();
				}
				return;
			}
			ask();
		});
		connect(ApplyButton, &QPushButton::clicked, this, [this] {
			const auto offers = Shown ? core::follow_up_offers(Shown->Subject) : std::vector<core::follow_up_apply>{ };
			if (!Running && !offers.empty() && Intents.Propose) {
				Intents.Propose(offers.front());
			}
		});
		connect(UseButton, &QPushButton::clicked, this, [this] {
			if (Shown && Shown->Proposal && Intents.Use) {
				Intents.Use(Shown->Proposal->Id);
			}
		});
		connect(DiscardButton, &QPushButton::clicked, this, [this] {
			if (Shown && Shown->Proposal && Intents.Discard) {
				Intents.Discard(Shown->Proposal->Id);
			}
		});
		restyle();
	}

	void follow_up_window::set_intents(follow_up_intents aIntents) { Intents = std::move(aIntents); }

	void follow_up_window::present(const core::follow_up_state& aState, const QString& aAgent, const QString& aFailure)
	{
		const auto another = !Shown || Shown->Subject.Key != aState.Subject.Key;
		if (another) {
			Question->clear();
		}
		Heading->setText(heading_words(aState.Subject));
		setWindowTitle(tr("Follow-up: %1").arg(Heading->text()));
		ItemTitle->setText(QString::fromStdString(aState.Subject.Title));
		ItemTitle->setVisible(!aState.Subject.Title.empty());
		const auto item = item_words(aState.Subject);
		ItemText->setText(item.isEmpty() ? tr("(empty)") : item);
		Question->setPlaceholderText(placeholder_words(aState.Subject.Kind));
		if (another || aState.Rounds.size() != RoundsShown) {
			rebuild_rounds(aState);
		}
		// Another item's conversation opens at its end.
		if (another) {
			stick_to_bottom::attach(*Scroll).pin();
		}
		Running = aState.Phase != core::follow_up_phase::idle;
		stick_to_bottom::attach(*Scroll).set_turn_running(Running);
		if (Running) {
			const auto agent = aAgent.isEmpty() ? tr("The agent") : aAgent;
			Working->setText(aState.Phase == core::follow_up_phase::asking ? tr("%1 is answering…").arg(agent)
																		   : tr("%1 is writing: %2…").arg(agent, follow_up_apply_words(aState.PendingApply.value_or(core::follow_up_apply::update_suggestion))));
		}
		Working->setVisible(Running);
		const auto offers = core::follow_up_offers(aState.Subject);
		ApplyButton->setVisible(!offers.empty());
		if (!offers.empty()) {
			ApplyButton->setText(follow_up_apply_words(offers.front()));
			const auto tip = offers.front() == core::follow_up_apply::draft_reply ? tr("The agent writes a reply from this conversation. You see it first; it goes in the reply composer only if you use it. Nothing is posted.")
																				  : tr("The agent rewrites the text from this conversation. You see it first; it replaces the text only if you use it. Nothing is posted.");
			ApplyButton->setToolTip(tip);
			ApplyButton->setAccessibleDescription(tip);
		}
		ApplyButton->setEnabled(!Running);
		AskButton->setText(Running ? tr("Stop") : tr("Ask"));
		AskButton->setAccessibleDescription(Running ? tr("Stops the agent's answer") : tr("Sends the question to the agent. Nothing is posted."));
		if (aState.Proposal) {
			const auto& proposal = *aState.Proposal;
			ProposalHeading->setText(proposal_words(proposal.Apply));
			const auto before = QString::fromStdString(proposal.Base).trimmed();
			ProposalBefore->setText(proposal.Apply == core::follow_up_apply::draft_reply ? (before.isEmpty() ? QString{ } : tr("Goes after what the reply composer holds:\n%1").arg(before)) : tr("Replaces:\n%1").arg(before));
			ProposalBefore->setVisible(!ProposalBefore->text().isEmpty());
			ProposalAfter->setText(QString::fromStdString(proposal.Text));
			UseButton->setText(proposal.Apply == core::follow_up_apply::draft_reply ? tr("Put in reply") : tr("Use this"));
			UseButton->setAccessibleDescription(proposal.Apply == core::follow_up_apply::draft_reply ? tr("Puts the reply in the conversation's reply composer. Nothing is posted.") : tr("Replaces the text with the proposal. Nothing is posted."));
		}
		const auto proposalWas = Shown && Shown->Proposal.has_value() && !another;
		Proposal->setVisible(aState.Proposal.has_value());
		UseButton->setEnabled(!Running);
		Shown = aState;
		show_status(aFailure.isEmpty() ? follow_up_refusal_words(aState.Refused) : aFailure);
		if (aState.Proposal && !proposalWas) {
			UseButton->setFocus(Qt::OtherFocusReason);
		}
		restyle();
	}

	void follow_up_window::show_status(const QString& aText)
	{
		Status->setText(aText);
		Status->setVisible(!aText.isEmpty());
		if (!aText.isEmpty() && isVisible()) {
			auto event = QAccessibleAnnouncementEvent{ Status, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	auto follow_up_window::proposal_text() const -> QString { return Shown && Shown->Proposal ? ProposalAfter->text() : QString{ }; }

	auto follow_up_window::status_text() const -> QString { return Status->text(); }

	auto follow_up_window::heading_text() const -> QString { return Heading->text(); }

	auto follow_up_window::item_text() const -> QString { return ItemText->text(); }

	auto follow_up_window::answers() const -> QStringList
	{
		auto shown = QStringList{ };
		for (auto* const label : RoundsHolder->findChildren<QLabel*>(QStringLiteral("followUpAnswer"))) {
			shown << label->text();
		}
		return shown;
	}

	void follow_up_window::ask()
	{
		const auto text = Question->toPlainText().trimmed();
		if (text.isEmpty()) {
			show_status(follow_up_refusal_words(core::follow_up_refusal::empty_question));
			return;
		}
		if (Intents.Ask) {
			Question->clear();
			Intents.Ask(text);
		}
	}

	void follow_up_window::rebuild_rounds(const core::follow_up_state& aState)
	{
		while (Rounds->count() > 1) {
			auto* const item = Rounds->takeAt(0);
			// Gone at once, so what is listed is only what is shown.
			delete item->widget();
			delete item;
		}
		auto at = 0;
		for (const auto& round : aState.Rounds) {
			auto* const question = plain_label(QString::fromStdString(round.Question), RoundsHolder, QStringLiteral("followUpAsked"));
			auto        strong   = question->font();
			strong.setWeight(QFont::DemiBold);
			question->setFont(strong);
			question->setAccessibleName(tr("You asked"));
			Rounds->insertWidget(at++, question);
			auto* const answer = plain_label(QString::fromStdString(round.Answer), RoundsHolder, QStringLiteral("followUpAnswer"));
			answer->setAccessibleName(tr("The agent answered"));
			answer->setContentsMargins(10, 0, 0, 0);
			Rounds->insertWidget(at++, answer);
		}
		RoundsShown = aState.Rounds.size();
		// Laid out when the event loop next runs, the old rounds and the new
		// together, so a reader scrolled up keeps their place.
	}

	/// The two run-starting controls wear the Agent accent while they would
	/// start one; Stop wears the ordinary roles.
	void follow_up_window::restyle()
	{
		if (Running) {
			wear_ordinary_roles(*AskButton);
		} else {
			wear_agent_accent(*AskButton, palette());
		}
		wear_agent_accent(*ApplyButton, palette());
	}

	void follow_up_window::closeEvent(QCloseEvent* aEvent)
	{
		QWidget::closeEvent(aEvent);
		if (Intents.Closed) {
			Intents.Closed();
		}
	}

	void follow_up_window::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && AskButton != nullptr) {
			restyle();
		}
	}

	/// Return asks; Shift+Return is a new line.
	auto follow_up_window::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aWatched == Question && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !key->modifiers().testFlag(Qt::ShiftModifier)) {
				if (!Running) {
					ask();
				}
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
