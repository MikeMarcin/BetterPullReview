#include "bpr/qt/item_side_chat.hpp"

#include "side_chat_parts.hpp"

#include "bpr/qt/resize_handle_cursor.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/chat_transcript_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QAccessible>
#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bpr::qt {
	namespace {
		using side_chat::quiet;
		using side_chat::small;
		using side_chat::words;

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

		/// \brief [[BPR-2333]]. The column's left-edge seam: dragging it left
		/// widens the column, a double-click returns it to the default width.
		class side_chat_width_handle final : public QWidget
		{
		public:
			explicit side_chat_width_handle(item_side_chat& aChat)
				: QWidget{ &aChat }
				, Chat{ aChat }
			{
				setObjectName(QStringLiteral("itemSideChatResizeHandle"));
				setAccessibleName(QObject::tr("Resize the side chat"));
				setToolTip(QObject::tr("Drag to resize the side chat; double-click for its default width"));
				setCursor(Qt::SizeHorCursor);
				setAttribute(Qt::WA_Hover, true);
				hold_resize_cursor(*this);
			}

			/// Wide enough to aim at, from the font so it scales with it.
			[[nodiscard]] auto sizeHint() const -> QSize override { return { std::max(6, fontMetrics().lineSpacing() / 3), 0 }; }

		protected:
			void mousePressEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() != Qt::LeftButton) {
					QWidget::mousePressEvent(aEvent);
					return;
				}
				Origin = aEvent->globalPosition().x();
				Start  = Chat.width();
				aEvent->accept();
			}
			void mouseMoveEvent(QMouseEvent* aEvent) override
			{
				if (!Origin || (aEvent->buttons() & Qt::LeftButton) == 0) {
					QWidget::mouseMoveEvent(aEvent);
					return;
				}
				// The seam is the column's left edge: left makes it wider.
				Chat.setFixedWidth(std::clamp(Start + static_cast<int>(std::lround(*Origin - aEvent->globalPosition().x())), item_side_chat::minimum_column_width, Chat.maximum_width()));
				aEvent->accept();
			}
			void mouseReleaseEvent(QMouseEvent* aEvent) override
			{
				if (!Origin || aEvent->button() != Qt::LeftButton) {
					QWidget::mouseReleaseEvent(aEvent);
					return;
				}
				Origin.reset();
				const auto chosen = Chat.width();
				Chat.setMinimumWidth(item_side_chat::minimum_column_width);
				Chat.setMaximumWidth(QWIDGETSIZE_MAX);
				Chat.resize_to(chosen);
				aEvent->accept();
			}
			void mouseDoubleClickEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton) {
					Chat.reset_width();
					aEvent->accept();
					return;
				}
				QWidget::mouseDoubleClickEvent(aEvent);
			}

		private:
			item_side_chat&       Chat;
			std::optional<double> Origin;
			int                   Start = 0;
		};

		/// What the item is, as the top of the chat names it before quoting it.
		[[nodiscard]] auto item_heading(const core::follow_up_subject_kind aKind) -> QString
		{
			switch (aKind) {
			case core::follow_up_subject_kind::suggested_comment: return QObject::tr("The suggested comment");
			case core::follow_up_subject_kind::draft:             return QObject::tr("Your draft");
			case core::follow_up_subject_kind::thread:            return QObject::tr("The conversation");
			}
			return { };
		}

		[[nodiscard]] auto invite_words(const core::follow_up_subject& aSubject) -> QString
		{
			switch (aSubject.Kind) {
			case core::follow_up_subject_kind::suggested_comment: return QObject::tr("Ask about this finding, or ask the agent to rewrite its comment.");
			case core::follow_up_subject_kind::draft:             return QObject::tr("Ask about your draft, or ask the agent to update it.");
			case core::follow_up_subject_kind::thread:
				return aSubject.Replies ? QObject::tr("Ask about this conversation, or ask the agent to draft your reply.") : QObject::tr("Ask about this conversation.");
			}
			return { };
		}

		[[nodiscard]] auto placeholder_words(const core::follow_up_subject_kind aKind) -> QString
		{
			switch (aKind) {
			case core::follow_up_subject_kind::suggested_comment: return QObject::tr("Ask about this finding");
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

	auto item_chat_apply_words(const core::follow_up_apply aApply) -> QString { return QString::fromStdString(core::follow_up_apply_words(aApply)); }

	auto item_chat_refusal_words(const core::follow_up_refusal aRefusal) -> QString
	{
		switch (aRefusal) {
		case core::follow_up_refusal::none:            return { };
		case core::follow_up_refusal::busy:            return QObject::tr("The agent is still answering. Wait for it, or press Stop.");
		case core::follow_up_refusal::empty_question:  return QObject::tr("Write a question first.");
		case core::follow_up_refusal::not_offered:     return QObject::tr("That cannot be applied to this item.");
		case core::follow_up_refusal::too_many_rounds: return QObject::tr("This chat is as long as one item's chat keeps. Apply what it settled, or start again from the item.");
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

	namespace {
		/// The header's first line: what kind of item the chat is about.
		[[nodiscard]] auto subject_kind_words(const core::follow_up_subject_kind aKind) -> QString
		{
			switch (aKind) {
			case core::follow_up_subject_kind::suggested_comment: return QObject::tr("Suggested comment");
			case core::follow_up_subject_kind::draft:             return QObject::tr("Your draft");
			case core::follow_up_subject_kind::thread:            return QObject::tr("Conversation");
			}
			return { };
		}

		/// The header's second line: the item's file and lines, as the
		/// tour's chat writes a stop's.
		[[nodiscard]] auto subject_place_words(const core::follow_up_subject& aSubject) -> QString
		{
			if (aSubject.Path.empty()) {
				return QObject::tr("The pull request");
			}
			const auto file = QString::fromStdString(aSubject.Path).section(QLatin1Char{ '/' }, -1);
			if (aSubject.First == 0U) {
				return QString{ file };
			}
			return aSubject.Last <= aSubject.First ? QObject::tr("%1, line %2").arg(file).arg(aSubject.First) : QObject::tr("%1, lines %2 to %3").arg(file).arg(aSubject.First).arg(aSubject.Last);
		}
	}

	auto item_chat_subject_words(const core::follow_up_subject& aSubject) -> QString
	{
		switch (aSubject.Kind) {
		case core::follow_up_subject_kind::suggested_comment: return QObject::tr("Suggested comment at %1").arg(place_words(aSubject));
		case core::follow_up_subject_kind::draft:             return QObject::tr("Your draft at %1").arg(place_words(aSubject));
		case core::follow_up_subject_kind::thread:            return QObject::tr("Conversation at %1").arg(place_words(aSubject));
		}
		return { };
	}

	item_side_chat::item_side_chat(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("itemSideChat"));
		setAccessibleName(tr("Side chat with the agent about one review item"));
		setMinimumWidth(minimum_column_width);
		// [[BPR-2333]]. The page beside it bounds its width; a page resize
		// asks the layout for the column's width again.
		Page = aParent;
		if (aParent != nullptr) {
			aParent->installEventFilter(this);
		}
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(10, 8, 10, 10);
		column->setSpacing(6);

		// The header (RFC-BPR-0130, header A): the one row every chat with
		// an agent wears. Here it is the kind of item and its place, Show,
		// Copy conversation and close. An item's chat keeps no sessions, so
		// core gives it Copy where the other chats have History; the agent's
		// glyph that stood before the subject is gone (the seam down the
		// column's edge says whose column it is).
		Header = new chat_header_view{ {
										   .Row     = QStringLiteral("itemSideChatHeader"),
										   .Subject = QStringLiteral("itemSideChatSubject"),
										   .Place   = QStringLiteral("itemSideChatQuote"),
										   .Show    = QStringLiteral("itemSideChatShow"),
										   .History = QStringLiteral("itemSideChatHistory"),
										   .Copy    = QStringLiteral("itemSideChatCopy"),
										   .Close   = QStringLiteral("itemSideChatClose"),
									   },
									   this };
		Header->set_tips({
			.Show  = tr("Show the item's place in the code"),
			.Close = tr("Close the chat. It is kept; asking about the item again opens it."),
		});
		Header->set_dispatch({
			.Show             = [this] {
				if (Shown && Intents.Reveal) {
					Intents.Reveal(Shown->Subject);
				} },
			.CopyConversation = [this] {
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(chat_text());
				} },
			.Close            = [this] { set_column_open(false); },
		});
		ShowItem  = Header->show_button();
		CopyChat  = Header->copy_tool();
		ClosePane = Header->close_tool();
		column->addWidget(Header);

		// The chat (RFC-BPR-0130 stage 3): the one transcript, the item it is
		// about quoted at its top, each round a turn, a proposal its own turn's
		// attachment.
		Transcript = new chat_transcript_view{ {
												   .Scroll       = QStringLiteral("itemSideChatScroll"),
												   .Bubble       = QStringLiteral("itemSideChatBubble"),
												   .Question     = QStringLiteral("itemSideChatQuestion"),
												   .Steered      = QStringLiteral("itemSideChatSteered"),
												   .Reply        = QStringLiteral("itemSideChatReply"),
												   .Failure      = QStringLiteral("itemSideChatFailure"),
												   .Ending       = QStringLiteral("itemSideChatEnding"),
												   .CopyReply    = QStringLiteral("itemSideChatCopyReply"),
												   .Resend       = QStringLiteral("itemSideChatResend"),
												   .Delete       = QStringLiteral("itemSideChatDelete"),
												   .QuestionNote = QStringLiteral("itemSideChatQuestionNote"),
												   .Activity     = QStringLiteral("itemSideChatActivity"),
											   },
											   this };
		Transcript->set_parts({
			.DressReader =
				[this](markdown_body_view& aReader) {
					if (CodeDispatch) {
						aReader.set_code_syntax_dispatch(CodeDispatch);
					}
				},
			.MakeAttachment =
				[this](const core::chat_turn& /*aTurn*/, const core::chat_attachment& aAttachment, QWidget* aHolder) -> QWidget* {
				const auto* const proposal = std::get_if<core::chat_follow_up_proposal>(&aAttachment.Payload);
				return proposal != nullptr ? make_proposal(*proposal, aHolder) : nullptr;
			},
			.Act = { },
		});
		auto* const item   = new QWidget{ Transcript->widget() };
		auto* const inItem = new QVBoxLayout{ item };
		item->setObjectName(QStringLiteral("itemSideChatItem"));
		inItem->setContentsMargins(0, 0, 0, 0);
		inItem->setSpacing(2);
		ItemHeading = quiet(small(words({ }, item)));
		inItem->addWidget(ItemHeading);
		ItemQuote = words({ }, item);
		ItemQuote->setObjectName(QStringLiteral("itemSideChatItemText"));
		auto italic = ItemQuote->font();
		italic.setItalic(true);
		ItemQuote->setFont(italic);
		inItem->addWidget(ItemQuote);
		Transcript->lead()->addWidget(item);
		Invite = quiet(small(words({ }, Transcript->widget())));
		Invite->setObjectName(QStringLiteral("itemSideChatEmpty"));
		Transcript->lead()->addWidget(Invite);
		Transcript->viewport()->installEventFilter(this);
		column->addWidget(Transcript, 1);

		// Under the transcript: a notice only when there is something to say,
		// the composer with the item as its chip, then the apply action.
		Status = quiet(small(words({ }, this)));
		Status->setObjectName(QStringLiteral("itemSideChatStatus"));
		Status->hide();
		column->addWidget(Status);
		// [[BPR-2337]] (owner, attended review b64bbc23: "The send button on
		// both the side chats (report and tour) should be positioned more
		// like the agent tab"): the Agent tab's composer, its insets, its
		// rule and Send at the right end of the row under it.
		// RFC-BPR-0130 stage 2: the one composer every chat places.
		Field               = new chat_composer{ { .Field = QStringLiteral("itemSideChatField"), .Text = QStringLiteral("itemSideChatComposer"), .Send = QStringLiteral("itemSideChatSend"), .Stop = QStringLiteral("itemSideChatStop") }, this };
		auto* const chip    = new passage_chip{ Field };
		auto* const chipRow = new QHBoxLayout{ chip };
		chipRow->setContentsMargins(8, 1, 8, 1);
		chip->setObjectName(QStringLiteral("itemSideChatChip"));
		Chip = small(new QLabel{ chip });
		Chip->setObjectName(QStringLiteral("itemSideChatChipText"));
		Chip->setTextFormat(Qt::PlainText);
		Chip->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		chipRow->addWidget(Chip, 1);
		Field->chips()->addWidget(chip);
		Composer = Field->text();
		Composer->setAccessibleName(tr("Message to the agent"));
		SkillCompletion = Field->skills();
		Send            = Field->send_button();
		// One round at a time, and no follow-ups: Send is Stop while it runs.
		Field->set_dispatch({
			.Send =
				[this](const bool /*aOpposite*/) {
					if (!Running) {
						send();
					}
				},
			.Stop =
				[this] {
					if (Intents.Stop) {
						Intents.Stop();
					}
				},
		});
		column->addWidget(Field);
		// The one thing the chat can apply, where the tour's chat keeps its
		// session line: it asks for the text, which shows in the chat first.
		auto* const footer = new QHBoxLayout{ };
		footer->setContentsMargins(0, 0, 0, 0);
		ApplyButton = new quiet_push_button{ this };
		ApplyButton->setObjectName(QStringLiteral("itemSideChatApply"));
		ApplyButton->setFlat(true);
		ApplyButton->setFocusPolicy(Qt::TabFocus);
		footer->addWidget(ApplyButton, 0, Qt::AlignVCenter);
		footer->addStretch(1);
		column->addLayout(footer);

		connect(ApplyButton, &QPushButton::clicked, this, [this] {
			const auto offers = Shown ? core::follow_up_offers(Shown->Subject) : std::vector<core::follow_up_apply>{ };
			if (!Running && !offers.empty() && Intents.Propose) {
				Intents.Propose(offers.front());
			}
		});
		ResizeHandle = new side_chat_width_handle{ *this };
		hide();
		present_lead();
		restyle();
	}

	auto item_side_chat::sizeHint() const -> QSize
	{
		const auto wanted = PreferredWidth ? static_cast<int>(*PreferredWidth) : default_width();
		return { std::clamp(wanted, minimum_column_width, maximum_width()), QWidget::sizeHint().height() };
	}

	auto item_side_chat::default_width() const -> int
	{
		const auto page = Page ? Page->width() : 0;
		return page <= 0 ? default_column_width : std::clamp(page * 3 / 10, minimum_column_width, default_column_width);
	}

	auto item_side_chat::maximum_width() const -> int
	{
		const auto page = Page ? Page->width() : 0;
		return page <= 0 ? QWIDGETSIZE_MAX : std::max(minimum_column_width, page * 6 / 10);
	}

	void item_side_chat::set_preferred_width(const std::optional<std::uint32_t> aWidth)
	{
		if (aWidth && *aWidth == 0U) {
			return;
		}
		PreferredWidth = aWidth;
		updateGeometry();
	}

	void item_side_chat::set_width_sink(std::function<void(std::optional<std::uint32_t>)> aSink) { WidthSink = std::move(aSink); }

	void item_side_chat::resize_to(const int aWidth)
	{
		const auto width = std::clamp(aWidth, minimum_column_width, std::max(minimum_column_width, maximum_width()));
		PreferredWidth   = static_cast<std::uint32_t>(width);
		updateGeometry();
		if (WidthSink) {
			WidthSink(PreferredWidth);
		}
	}

	void item_side_chat::reset_width()
	{
		PreferredWidth.reset();
		updateGeometry();
		if (WidthSink) {
			WidthSink(std::nullopt);
		}
	}

	void item_side_chat::set_intents(item_side_chat_intents aIntents) { Intents = std::move(aIntents); }

	void item_side_chat::present_skills(std::vector<core::skill_entry> aSkills) { SkillCompletion->present(std::move(aSkills)); }

	void item_side_chat::set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		CodeDispatch = std::move(aDispatch);
		for (auto* const reader : Transcript->readers()) {
			reader->set_code_syntax_dispatch(CodeDispatch);
			reader->request_code_syntax();
		}
	}

	void item_side_chat::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		for (auto* const reader : Transcript->readers()) {
			reader->apply_code_syntax(aSpans);
		}
	}

	void item_side_chat::set_column_open(const bool aOpen)
	{
		const auto was = ColumnOpen;
		ColumnOpen     = aOpen;
		if (aOpen) {
			TabShown = true;
		}
		setVisible(aOpen);
		if (aOpen) {
			Composer->setFocus(Qt::OtherFocusReason);
		}
		if (OpenObserver && (aOpen || was)) {
			OpenObserver(aOpen);
		}
		if (!aOpen && was && Intents.Closed) {
			Intents.Closed();
		}
	}

	void item_side_chat::set_tab_shown(const bool aShown)
	{
		TabShown = aShown;
		setVisible(ColumnOpen && TabShown);
	}

	void item_side_chat::present(const core::follow_up_state& aState, const QString& aAgent, const QString& aFailure)
	{
		const auto another = !Shown || Shown->Subject.Key != aState.Subject.Key;
		if (another) {
			Composer->clear();
			Seconds.clear();
		}
		const auto wasRunning = Running && !another;
		Running               = aState.Phase != core::follow_up_phase::idle;
		if (Running && !wasRunning) {
			Clock.start();
		}
		if (!Running && wasRunning && Clock.isValid()) {
			if (aState.Rounds.size() > (Shown ? Shown->Rounds.size() : 0U)) {
				Seconds.resize(aState.Rounds.size());
				Seconds.back() = Clock.elapsed() / 1000;
			}
			Clock.invalidate();
		}
		const auto proposalWas = !another && Shown && Shown->Proposal.has_value() && aState.Proposal && Shown->Proposal->Id == aState.Proposal->Id;
		Agent                  = aAgent;
		Shown                  = aState;
		// What the row says is this chat's; what it holds is core's.
		Header->present(core::plan_chat_header({
			.Kind         = core::chat_kind::review_item,
			.Subject      = subject_kind_words(aState.Subject.Kind).toStdString(),
			.Place        = subject_place_words(aState.Subject).toStdString(),
			.CanShow      = !aState.Subject.Path.empty() && aState.Subject.First != 0U,
			.About        = aState.Subject.Title,
			.SessionName  = { },
			.RevisionRead = { },
			.Cost         = std::nullopt,
		}));
		ChipWords = item_words(aState.Subject).simplified();
		Chip->setText(ChipWords);
		Chip->setToolTip(item_words(aState.Subject));
		Composer->setPlaceholderText(placeholder_words(aState.Subject.Kind));
		present_lead();
		// [[BPR-2296]]. Another item's chat opens at its end; at its end, the
		// chat follows what arrives, and one scrolled up stays put.
		if (another) {
			Transcript->pin();
		}
		// RFC-BPR-0130 stage 3. Only the turns that changed are drawn again.
		Transcript->present_live(agent_run_status{ .Agent = Agent.isEmpty() ? tr("The agent") : Agent }, Clock);
		Transcript->present(core::project_follow_up(aState, { .Agent = Agent.toStdString(), .Seconds = Seconds }));
		if (!aState.Proposal) {
			// The card goes with its turn, later; it is no longer the one shown.
			ProposalCard  = nullptr;
			ProposalText  = nullptr;
			UseButton     = nullptr;
			DiscardButton = nullptr;
		}
		const auto offers = core::follow_up_offers(aState.Subject);
		ApplyButton->setVisible(!offers.empty());
		if (!offers.empty()) {
			ApplyButton->setText(item_chat_apply_words(offers.front()));
			const auto tip = offers.front() == core::follow_up_apply::draft_reply ? tr("The agent writes a reply from this chat. You see it here first; it goes in the reply composer only if you use it. Nothing is posted.")
																				  : tr("The agent rewrites the text from this chat. You see it here first; it replaces the text only if you use it. Nothing is posted.");
			ApplyButton->setToolTip(tip);
			ApplyButton->setAccessibleDescription(tip);
		}
		ApplyButton->setEnabled(!Running);
		// The one rule (`core::decide_chat_send`): an item's chat takes no
		// follow-ups, so Send is Stop while its round runs.
		Field->present(core::decide_chat_send({ .Running = Running }));
		Send->setToolTip(Running ? tr("Stop the agent's answer") : tr("Send the question to the agent. Nothing is posted."));
		show_status(aFailure.isEmpty() ? item_chat_refusal_words(aState.Refused) : aFailure);
		restyle();
		QTimer::singleShot(0, this, [this] { fit_chip(); });
		if (UseButton && !proposalWas) {
			UseButton->setFocus(Qt::OtherFocusReason);
		}
	}

	void item_side_chat::present_lead()
	{
		const auto quoted = Shown ? item_words(Shown->Subject) : QString{ };
		ItemHeading->setText(Shown ? item_heading(Shown->Subject.Kind) : QString{ });
		ItemQuote->setText(quoted.isEmpty() ? tr("(empty)") : quoted);
		ItemQuote->parentWidget()->setVisible(Shown.has_value());
		Invite->setText(Shown ? invite_words(Shown->Subject) : QString{ });
		Invite->setVisible(Shown && Shown->Rounds.empty() && !Running);
	}

	void item_side_chat::show_status(const QString& aText)
	{
		Status->setText(aText);
		Status->setVisible(!aText.isEmpty());
		if (!aText.isEmpty() && isVisible()) {
			auto event = QAccessibleAnnouncementEvent{ Status, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	auto item_side_chat::proposal_text() const -> QString { return ProposalText != nullptr ? ProposalText->text() : QString{ }; }

	auto item_side_chat::status_text() const -> QString { return Status->text(); }

	auto item_side_chat::subject_text() const -> QString { return Shown ? item_chat_subject_words(Shown->Subject) : QString{ }; }

	auto item_side_chat::item_text() const -> QString { return Shown ? ItemQuote->text() : QString{ }; }

	auto item_side_chat::chip_text() const -> QString { return ChipWords; }

	auto item_side_chat::questions() const -> QStringList
	{
		auto shown = QStringList{ };
		for (std::size_t turn = 0U; turn < Transcript->turn_count(); ++turn) {
			if (const auto* const asked = Transcript->question(turn); asked != nullptr) {
				shown << asked->text();
			}
		}
		return shown;
	}

	auto item_side_chat::answers() const -> QStringList
	{
		auto shown = QStringList{ };
		for (std::size_t turn = 0U; turn < Transcript->turn_count(); ++turn) {
			if (const auto* const reply = Transcript->reply(turn); reply != nullptr) {
				shown << QString::fromStdString(reply->original_body());
			}
		}
		return shown;
	}

	// The rounds are the transcript's first turns.
	auto item_side_chat::status_line(const std::size_t aRound) const -> agent_run_status_line* { return Shown && aRound < Shown->Rounds.size() ? Transcript->ending(aRound) : nullptr; }

	auto item_side_chat::chat_text() const -> QString
	{
		if (!Shown) {
			return { };
		}
		auto lines = QStringList{ tr("**%1**").arg(item_chat_subject_words(Shown->Subject)), QString{ } };
		for (const auto& line : item_words(Shown->Subject).split(QLatin1Char('\n'))) {
			lines << QStringLiteral("> %1").arg(line);
		}
		lines << QString{ };
		const auto agent = Agent.isEmpty() ? tr("Agent") : agent_short_name(Agent);
		for (const auto& round : Shown->Rounds) {
			lines << tr("**You:** %1").arg(QString::fromStdString(round.Question)) << QString{ } << tr("**%1:**").arg(agent) << QString{ } << QString::fromStdString(round.Answer) << QString{ };
		}
		return lines.join(QLatin1Char('\n')).trimmed();
	}

	void item_side_chat::send()
	{
		const auto text = Composer->toPlainText().trimmed();
		if (text.isEmpty()) {
			show_status(item_chat_refusal_words(core::follow_up_refusal::empty_question));
			return;
		}
		if (Intents.Ask) {
			Composer->clear();
			// The reviewer's own question takes the chat to its end, as the
			// tour's chat does.
			Transcript->pin();
			Intents.Ask(text);
		}
	}

	auto item_side_chat::make_proposal(const core::chat_follow_up_proposal& aProposal, QWidget* aParent) -> QWidget*
	{
		// A proposal waits in the chat beside what it replaces, as the tour's
		// chat shows a part's proposal; nothing changes until Use.
		const auto& proposal = aProposal.Proposal;
		auto* const card     = new side_chat::proposal_frame{ aParent };
		card->setObjectName(QStringLiteral("itemSideChatProposal"));
		auto* const lines = new QVBoxLayout{ card };
		lines->setContentsMargins(12, 8, 8, 8);
		lines->setSpacing(4);
		auto* const title = small(words(proposal_words(proposal.Apply), card));
		auto        heavy = title->font();
		heavy.setWeight(QFont::DemiBold);
		title->setFont(heavy);
		apply_agent_accent(*title, palette());
		lines->addWidget(title);
		const auto before = QString::fromStdString(proposal.Base).trimmed();
		const auto said   = proposal.Apply == core::follow_up_apply::draft_reply ? (before.isEmpty() ? QString{ } : tr("Goes after what the reply composer holds:\n%1").arg(before)) : tr("Replaces:\n%1").arg(before);
		if (!said.isEmpty()) {
			auto* const replaced = quiet(small(words(said, card)));
			replaced->setObjectName(QStringLiteral("itemSideChatProposalBefore"));
			lines->addWidget(replaced);
		}
		ProposalText = words(QString::fromStdString(proposal.Text), card);
		ProposalText->setObjectName(QStringLiteral("itemSideChatProposalText"));
		lines->addWidget(ProposalText);
		auto* const buttons = new QHBoxLayout{ };
		buttons->setSpacing(6);
		const auto reply = proposal.Apply == core::follow_up_apply::draft_reply;
		UseButton        = make_tour_action(reply ? tr("Put in reply") : tr("Use this"), card);
		UseButton->setObjectName(QStringLiteral("itemSideChatUse"));
		UseButton->setToolTip(reply ? tr("Puts the reply in the conversation's reply composer. Nothing is posted.") : tr("Replaces the text with the proposal. Nothing is posted."));
		UseButton->setAccessibleDescription(UseButton->toolTip());
		UseButton->setEnabled(aProposal.Usable);
		DiscardButton = make_tour_action(tr("Discard"), card);
		DiscardButton->setObjectName(QStringLiteral("itemSideChatDiscard"));
		DiscardButton->setToolTip(tr("Drops the proposal. Nothing changes."));
		buttons->addWidget(UseButton);
		buttons->addWidget(DiscardButton);
		buttons->addStretch(1);
		lines->addLayout(buttons);
		connect(UseButton, &QPushButton::clicked, this, [this, id = proposal.Id] {
			if (Intents.Use) {
				Intents.Use(id);
			}
		});
		connect(DiscardButton, &QPushButton::clicked, this, [this, id = proposal.Id] {
			if (Intents.Discard) {
				Intents.Discard(id);
			}
		});
		ProposalCard = card;
		return card;
	}

	/// The apply action starts runs and wears the Agent accent; the composer
	/// draws Send and Stop itself.
	void item_side_chat::restyle()
	{
		Header->repaint_glyphs();
		wear_agent_accent(*ApplyButton, palette());
		update();
	}

	void item_side_chat::fit_chip() { Chip->setText(Chip->fontMetrics().elidedText(ChipWords, Qt::ElideRight, std::max(40, Chip->width()))); }

	void item_side_chat::paintEvent(QPaintEvent* /*aEvent*/) { side_chat::paint_seam(*this); }

	void item_side_chat::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && Send != nullptr) {
			restyle();
		}
	}

	void item_side_chat::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		ResizeHandle->setGeometry(0, 0, ResizeHandle->sizeHint().width(), height());
		ResizeHandle->raise();
	}

	bool item_side_chat::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		if (aEvent->type() == QEvent::Resize && aWatched == Page) {
			updateGeometry();
		}
		if (aEvent->type() == QEvent::Resize && aWatched == Transcript->viewport()) {
			QTimer::singleShot(0, this, [this] { fit_chip(); });
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
