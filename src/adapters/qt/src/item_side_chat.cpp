#include "bpr/qt/item_side_chat.hpp"

#include "composer_parts.hpp"
#include "side_chat_parts.hpp"

#include "bpr/qt/resize_handle_cursor.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/stick_to_bottom.hpp"
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
#include <QScrollArea>
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

	auto item_chat_apply_words(const core::follow_up_apply aApply) -> QString
	{
		switch (aApply) {
		case core::follow_up_apply::update_suggestion: return QObject::tr("Update suggested comment");
		case core::follow_up_apply::update_draft:      return QObject::tr("Update draft");
		case core::follow_up_apply::draft_reply:       return QObject::tr("Draft reply");
		}
		return { };
	}

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

		// The title row, as the tour's chat has it: the agent's glyph, what
		// the chat is about and where, Show, Copy chat and close.
		auto* const top = new QHBoxLayout{ };
		top->setSpacing(4);
		Glyph = new QLabel{ this };
		Glyph->setObjectName(QStringLiteral("itemSideChatGlyph"));
		top->addWidget(Glyph, 0, Qt::AlignTop);
		auto* const aboutText = new QVBoxLayout{ };
		aboutText->setSpacing(1);
		Subject = new QLabel{ this };
		Subject->setObjectName(QStringLiteral("itemSideChatSubject"));
		Subject->setTextFormat(Qt::PlainText);
		Subject->setWordWrap(true);
		auto heavy = Subject->font();
		heavy.setWeight(QFont::DemiBold);
		Subject->setFont(heavy);
		Quote = quiet(small(words({ }, this)));
		Quote->setObjectName(QStringLiteral("itemSideChatQuote"));
		aboutText->addWidget(Subject);
		aboutText->addWidget(Quote);
		top->addLayout(aboutText, 1);
		ShowItem = new quiet_push_button{ tr("Show"), this };
		ShowItem->setObjectName(QStringLiteral("itemSideChatShow"));
		ShowItem->setFlat(true);
		ShowItem->setFocusPolicy(Qt::TabFocus);
		ShowItem->setToolTip(tr("Show the item's place in the code"));
		top->addWidget(ShowItem, 0, Qt::AlignTop);
		auto* const copyChat = new quiet_tool_button{ this };
		copyChat->set_tinted(true);
		CopyChat = copyChat;
		CopyChat->setObjectName(QStringLiteral("itemSideChatCopy"));
		CopyChat->setAutoRaise(true);
		CopyChat->setToolTip(tr("Copy this chat as text"));
		CopyChat->setAccessibleName(tr("Copy chat"));
		auto* const closePane = new quiet_tool_button{ this };
		closePane->set_tinted(true);
		ClosePane = closePane;
		ClosePane->setObjectName(QStringLiteral("itemSideChatClose"));
		ClosePane->setAutoRaise(true);
		ClosePane->setToolTip(tr("Close the chat. It is kept; asking about the item again opens it."));
		ClosePane->setAccessibleName(tr("Close the chat"));
		top->addWidget(CopyChat, 0, Qt::AlignTop);
		top->addWidget(ClosePane, 0, Qt::AlignTop);
		column->addLayout(top);

		// The chat.
		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("itemSideChatScroll"));
		Scroll->setWidgetResizable(true);
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->setFocusPolicy(Qt::NoFocus);
		auto* const body = new QWidget{ Scroll };
		Turns            = new QVBoxLayout{ body };
		Turns->setContentsMargins(0, 6, 6, 6);
		Turns->setSpacing(12);
		Scroll->setWidget(body);
		Scroll->viewport()->installEventFilter(this);
		// [[BPR-2296]]. At its end, the chat stays there as it grows.
		stick_to_bottom::attach(*Scroll);
		column->addWidget(Scroll, 1);

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
		Field = new composer_field{ this };
		Field->setObjectName(QStringLiteral("itemSideChatField"));
		auto* const fieldColumn = new QVBoxLayout{ Field };
		composer::set_field_margins(*fieldColumn);
		fieldColumn->setSpacing(4);
		auto* const chip    = new passage_chip{ Field };
		auto* const chipRow = new QHBoxLayout{ chip };
		chipRow->setContentsMargins(8, 1, 8, 1);
		chip->setObjectName(QStringLiteral("itemSideChatChip"));
		Chip = small(new QLabel{ chip });
		Chip->setObjectName(QStringLiteral("itemSideChatChipText"));
		Chip->setTextFormat(Qt::PlainText);
		Chip->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		chipRow->addWidget(Chip, 1);
		fieldColumn->addWidget(chip);
		Composer = new QPlainTextEdit{ Field };
		Composer->setObjectName(QStringLiteral("itemSideChatComposer"));
		Composer->setAccessibleName(tr("Message to the agent"));
		Composer->setTabChangesFocus(true);
		Composer->setFrameShape(QFrame::NoFrame);
		Composer->viewport()->setAutoFillBackground(false);
		Composer->setAttribute(Qt::WA_MacShowFocusRect, false);
		Composer->document()->setDocumentMargin(4);
		Composer->setFixedHeight(3 * Composer->fontMetrics().lineSpacing() + 12);
		Composer->installEventFilter(new side_chat::send_keys{ Composer, [this](const bool /*aOpposite*/) {
																  if (!Running) {
																	  send();
																  }
															  } });
		Composer->installEventFilter(Field);
		// Install after the send filter so Return chooses a skill while the list is open.
		SkillCompletion = new skill_completer{ Composer };
		fieldColumn->addWidget(Composer);
		auto* const sendRow = composer::add_send_row(*Field, *fieldColumn);
		sendRow->addStretch(1);
		Send = composer::make_send_button(*Field, QStringLiteral("itemSideChatSend"));
		Send->setFocusPolicy(Qt::TabFocus);
		Send->setToolTip(tr("Send the question to the agent. Nothing is posted."));
		sendRow->addWidget(Send);
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

		connect(CopyChat, &QToolButton::clicked, this, [this] {
			if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
				clipboard->setText(chat_text());
			}
		});
		connect(ClosePane, &QToolButton::clicked, this, [this] { set_column_open(false); });
		connect(ShowItem, &QPushButton::clicked, this, [this] {
			if (Shown && Intents.Reveal) {
				Intents.Reveal(Shown->Subject);
			}
		});
		connect(Send, &QPushButton::clicked, this, [this] {
			if (Running) {
				if (Intents.Stop) {
					Intents.Stop();
				}
				return;
			}
			send();
		});
		connect(ApplyButton, &QPushButton::clicked, this, [this] {
			const auto offers = Shown ? core::follow_up_offers(Shown->Subject) : std::vector<core::follow_up_apply>{ };
			if (!Running && !offers.empty() && Intents.Propose) {
				Intents.Propose(offers.front());
			}
		});
		ResizeHandle = new side_chat_width_handle{ *this };
		hide();
		rebuild();
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
				Seconds[aState.Rounds.size() - 1U] = Clock.elapsed() / 1000;
			}
			Clock.invalidate();
		}
		const auto proposalWas = !another && Shown && Shown->Proposal.has_value() && aState.Proposal && Shown->Proposal->Id == aState.Proposal->Id;
		Agent                  = aAgent;
		Shown                  = aState;
		Subject->setText(item_chat_subject_words(aState.Subject));
		Quote->setText(QString::fromStdString(aState.Subject.Title));
		Quote->setVisible(!aState.Subject.Title.empty());
		ShowItem->setVisible(!aState.Subject.Path.empty() && aState.Subject.First != 0U);
		ChipWords = item_words(aState.Subject).simplified();
		Chip->setText(ChipWords);
		Chip->setToolTip(item_words(aState.Subject));
		Composer->setPlaceholderText(placeholder_words(aState.Subject.Kind));
		// [[BPR-2296]]. Drawn again, the chat holds its height until the new
		// turns are laid out, so a reader scrolled up keeps their place.
		auto* const body = Scroll->widget();
		body->setMinimumHeight(body->height());
		rebuild();
		QTimer::singleShot(0, this, [this] {
			fit();
			Scroll->widget()->setMinimumHeight(0);
		});
		// [[BPR-2296]]. Another item's chat opens at its end; at its end, the
		// chat follows what arrives, and one scrolled up stays put.
		auto& follow = stick_to_bottom::attach(*Scroll);
		if (another) {
			follow.pin();
		}
		follow.set_turn_running(Running);
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
		// As the Agent tab's: the arrow, or Stop in words while it answers.
		Send->setText(Running ? tr("Stop") : QString{ });
		Send->setAccessibleName(Running ? tr("Stop") : tr("Send"));
		Send->setToolTip(Running ? tr("Stop the agent's answer") : tr("Send the question to the agent. Nothing is posted."));
		show_status(aFailure.isEmpty() ? item_chat_refusal_words(aState.Refused) : aFailure);
		restyle();
		if (UseButton != nullptr && !proposalWas) {
			UseButton->setFocus(Qt::OtherFocusReason);
		}
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

	auto item_side_chat::subject_text() const -> QString { return Subject->text(); }

	auto item_side_chat::item_text() const -> QString { return ItemQuote != nullptr ? ItemQuote->text() : QString{ }; }

	auto item_side_chat::chip_text() const -> QString { return ChipWords; }

	auto item_side_chat::questions() const -> QStringList
	{
		auto shown = QStringList{ };
		for (auto* const label : Asked) {
			shown << label->text();
		}
		return shown;
	}

	auto item_side_chat::answers() const -> QStringList
	{
		auto shown = QStringList{ };
		for (auto* const reply : Replies) {
			shown << QString::fromStdString(reply->original_body());
		}
		return shown;
	}

	auto item_side_chat::status_line(const std::size_t aRound) const -> agent_run_status_line* { return aRound < Endings.size() ? Endings[aRound] : nullptr; }

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
			stick_to_bottom::attach(*Scroll).pin();
			Intents.Ask(text);
		}
	}

	auto item_side_chat::reader(QWidget* aParent, const std::string& aBody, const std::string& aIdentity) -> markdown_body_view*
	{
		auto* const view = new markdown_body_view{ aParent };
		view->set_in_place(true);
		// [[BPR-1944]], as the tour's chat: a reply's code is a box as wide
		// as the chat, with Wrap and Copy.
		view->set_code_block_tools(true);
		view->setFocusPolicy(Qt::NoFocus);
		view->set_height_observer([this] { QTimer::singleShot(0, this, [this] { fit(); }); });
		Readers.emplace_back(view);
		view->set_body(aBody, aIdentity);
		// As tall as it reads now, so a chat drawn again keeps its height
		// and a reader scrolled up keeps their place.
		const auto available = std::max(160, Scroll->viewport()->width() - Turns->contentsMargins().left() - Turns->contentsMargins().right());
		view->setFixedHeight(view->heightForWidth(std::max(120, available)));
		return view;
	}

	void item_side_chat::rebuild()
	{
		side_chat::clear(Turns);
		Bubbles.clear();
		Asked.clear();
		Replies.clear();
		Endings.clear();
		ItemQuote     = nullptr;
		ProposalCard  = nullptr;
		ProposalText  = nullptr;
		UseButton     = nullptr;
		DiscardButton = nullptr;
		Readers.erase(std::remove_if(Readers.begin(), Readers.end(), [](const QPointer<markdown_body_view>& aView) { return aView.isNull(); }), Readers.end());
		if (!Shown) {
			return;
		}
		auto* const body  = Scroll->widget();
		const auto& state = *Shown;
		const auto  agent = Agent.isEmpty() ? tr("The agent") : agent_short_name(Agent);

		// The item the chat is about, quoted at its top.
		auto* const item   = new QWidget{ body };
		auto* const inItem = new QVBoxLayout{ item };
		item->setObjectName(QStringLiteral("itemSideChatItem"));
		inItem->setContentsMargins(0, 0, 0, 0);
		inItem->setSpacing(2);
		inItem->addWidget(quiet(small(words(item_heading(state.Subject.Kind), item))));
		const auto quoted = item_words(state.Subject);
		ItemQuote         = words(quoted.isEmpty() ? tr("(empty)") : quoted, item);
		ItemQuote->setObjectName(QStringLiteral("itemSideChatItemText"));
		auto italic = ItemQuote->font();
		italic.setItalic(true);
		ItemQuote->setFont(italic);
		inItem->addWidget(ItemQuote);
		Turns->addWidget(item);
		if (state.Rounds.empty() && !Running) {
			auto* const empty = quiet(small(words(invite_words(state.Subject), body)));
			empty->setObjectName(QStringLiteral("itemSideChatEmpty"));
			Turns->addWidget(empty);
		}

		// The reviewer's words on the right.
		const auto mine = [&](const QString& aText) {
			auto* const row    = new QHBoxLayout{ };
			auto* const bubble = new side_chat::bubble_frame{ body };
			bubble->setObjectName(QStringLiteral("itemSideChatBubble"));
			auto* const inside = new QVBoxLayout{ bubble };
			inside->setContentsMargins(12, 8, 12, 8);
			auto* const asked = words(aText, bubble);
			asked->setObjectName(QStringLiteral("itemSideChatQuestion"));
			inside->addWidget(asked);
			row->addStretch(1);
			row->addWidget(bubble);
			Turns->addLayout(row);
			Bubbles.push_back(bubble);
			return asked;
		};
		// A running request's live line, as the Agent tab's turns say it.
		const auto live = [&] {
			auto* const ending = new agent_run_status_line{ body };
			ending->setObjectName(QStringLiteral("itemSideChatEnding"));
			small(ending);
			ending->present(agent_run_status{ .Agent = Agent.isEmpty() ? tr("The agent") : Agent }, Clock);
			Turns->addWidget(ending);
		};

		for (std::size_t index = 0U; index < state.Rounds.size(); ++index) {
			const auto& round = state.Rounds[index];
			Asked.push_back(mine(QString::fromStdString(round.Question)));
			// The agent's answer on the left: the reply, then its ending
			// line with Copy.
			auto* const answer = new QWidget{ body };
			auto* const lines  = new QVBoxLayout{ answer };
			lines->setContentsMargins(0, 0, 0, 0);
			lines->setSpacing(4);
			auto* const said = reader(answer, round.Answer, "item-side-chat:" + state.Subject.Key + ":" + std::to_string(index));
			said->setObjectName(QStringLiteral("itemSideChatReply"));
			lines->addWidget(said);
			Replies.push_back(said);
			auto* const row = new QHBoxLayout{ };
			row->setContentsMargins(0, 0, 0, 0);
			row->setSpacing(2);
			auto* const ending = new agent_run_status_line{ answer };
			ending->setObjectName(QStringLiteral("itemSideChatEnding"));
			small(ending);
			const auto seconds = Seconds.find(index);
			ending->settle(seconds != Seconds.end() ? agent_run_answered_words(agent, seconds->second) + QStringLiteral(".") : tr("%1 answered.").arg(agent), true);
			Endings.push_back(ending);
			row->addWidget(ending, 1, Qt::AlignVCenter);
			auto* const copy = new quiet_tool_button{ answer };
			copy->set_tinted(true);
			copy->setObjectName(QStringLiteral("itemSideChatCopyReply"));
			copy->setAutoRaise(true);
			copy->setToolTip(tr("Copy this reply"));
			copy->setAccessibleName(tr("Copy reply"));
			copy->setIcon(drawer_icon(drawer_glyph::copy, palette()));
			connect(copy, &QToolButton::clicked, this, [text = QString::fromStdString(round.Answer)] {
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(text);
				}
			});
			row->addWidget(copy);
			lines->addLayout(row);
			Turns->addWidget(answer);
		}
		if (state.Phase == core::follow_up_phase::asking) {
			Asked.push_back(mine(QString::fromStdString(state.PendingQuestion)));
			live();
		} else if (state.Phase == core::follow_up_phase::proposing) {
			Asked.push_back(mine(item_chat_apply_words(state.PendingApply.value_or(core::follow_up_apply::update_suggestion))));
			live();
		}

		// A proposal waits in the chat beside what it replaces, as the tour's
		// chat shows a part's proposal; nothing changes until Use.
		if (state.Proposal) {
			const auto& proposal = *state.Proposal;
			auto* const card     = new side_chat::proposal_frame{ body };
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
			UseButton->setEnabled(!Running);
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
			Turns->addWidget(card);
			ProposalCard = card;
		}
		Turns->addStretch(1);
		QTimer::singleShot(0, this, [this] { fit(); });
	}

	/// Send and the apply action start runs and wear the Agent accent; while
	/// a run goes Send is Stop, in the ordinary roles.
	void item_side_chat::restyle()
	{
		auto accented = palette();
		accented.setColor(QPalette::Active, QPalette::ButtonText, agent_accent(palette()));
		const auto side = Glyph->fontMetrics().height();
		Glyph->setPixmap(side_chat_icon(accented).pixmap(QSize{ side, side }, devicePixelRatioF()));
		CopyChat->setIcon(drawer_icon(drawer_glyph::copy, palette()));
		ClosePane->setIcon(drawer_icon(drawer_glyph::close, palette()));
		if (Running) {
			wear_ordinary_roles(*Send);
		} else {
			wear_agent_accent(*Send, palette());
		}
		Send->setIcon(Running ? QIcon{ } : drawer_icon(drawer_glyph::send, Send->palette()));
		wear_agent_accent(*ApplyButton, palette());
		update();
	}

	void item_side_chat::fit()
	{
		const auto available = std::max(160, Scroll->viewport()->width() - Turns->contentsMargins().left() - Turns->contentsMargins().right());
		for (auto* const bubble : Bubbles) {
			side_chat::fit_bubble(*bubble, available);
		}
		for (const auto& each : Readers) {
			if (each && !each->isHidden()) {
				const auto height = each->heightForWidth(std::max(120, available));
				if (each->height() != height || each->minimumHeight() != height) {
					each->setFixedHeight(height);
				}
			}
		}
		Chip->setText(Chip->fontMetrics().elidedText(ChipWords, Qt::ElideRight, std::max(40, Chip->width())));
	}

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
		if (aEvent->type() == QEvent::Resize && aWatched == Scroll->viewport()) {
			QTimer::singleShot(0, this, [this] { fit(); });
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
