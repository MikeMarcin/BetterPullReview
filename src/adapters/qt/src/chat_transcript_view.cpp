#include "bpr/qt/chat_transcript_view.hpp"

#include "agent_card_frame.hpp"
#include "side_chat_parts.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_steered_words.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/stick_to_bottom.hpp"
#include "bpr/qt/transcript_edge.hpp"

#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	using side_chat::quiet;
	using side_chat::small;
	using side_chat::words;

	namespace {
		/// An attachment a turn holds and the widget its chat's factory made
		/// for it.
		struct made_attachment
		{
			std::string           Key;
			core::chat_attachment Value;
			QPointer<QWidget>     Widget;
		};

		/// Puts \p aMoved, and what it holds, just before \p aBefore in their
		/// window's focus order. A widget joins the end of that order when it
		/// is made, so an attachment made after its turn would otherwise be
		/// reached after the turn's foot. Qt moves one widget at a time and
		/// orders only widgets that take focus, so each of them takes it for as
		/// long as this lasts.
		void focus_before(QWidget& aMoved, QWidget& aBefore)
		{
			auto* const previous = aBefore.previousInFocusChain();
			if (previous == nullptr || previous == &aMoved || aMoved.isAncestorOf(previous) || previous->window() != aMoved.window()) {
				return;
			}
			auto moved = std::vector<QWidget*>{ &aMoved };
			for (auto* next = aMoved.nextInFocusChain(); next != nullptr && next != &aMoved && aMoved.isAncestorOf(next); next = next->nextInFocusChain()) {
				moved.push_back(next);
			}
			auto policies = std::vector<Qt::FocusPolicy>{ };
			policies.reserve(moved.size());
			for (auto* const widget : moved) {
				policies.push_back(widget->focusPolicy());
				widget->setFocusPolicy(Qt::TabFocus);
			}
			const auto prior = previous->focusPolicy();
			previous->setFocusPolicy(Qt::TabFocus);
			auto* after = previous;
			for (auto* const widget : moved) {
				QWidget::setTabOrder(after, widget);
				after = widget;
			}
			previous->setFocusPolicy(prior);
			for (std::size_t index = 0U; index < moved.size(); ++index) {
				moved[index]->setFocusPolicy(policies[index]);
			}
		}
	}

	/// One turn, in either look: each part is changed in place when the turn
	/// changes.
	class chat_transcript_view::any_turn_view : public QWidget
	{
	public:
		using QWidget::QWidget;

		virtual void present(const core::chat_turn& aTurn) = 0;
		virtual void present_live()                        = 0;
		virtual void restyle()                             = 0;
		virtual void enable_resend(bool aEnabled)          = 0;
		virtual void remake(const std::string& aKey)       = 0;
		virtual void refresh_reply() { }
		/// Lays its readers out for `aWidth`, the column's.
		virtual void fit(int aWidth) { static_cast<void>(aWidth); }

		[[nodiscard]] virtual auto turn() const noexcept -> const core::chat_turn&       = 0;
		[[nodiscard]] virtual auto bubble() const noexcept -> QWidget*                   = 0;
		[[nodiscard]] virtual auto question() const noexcept -> QLabel*                  = 0;
		[[nodiscard]] virtual auto reply() const noexcept -> markdown_body_view*         = 0;
		[[nodiscard]] virtual auto ending() const noexcept -> agent_run_status_line*     = 0;
		[[nodiscard]] virtual auto copy() const noexcept -> QToolButton*                 = 0;
		[[nodiscard]] virtual auto resend() const noexcept -> QPushButton*               = 0;
		[[nodiscard]] virtual auto remove() const noexcept -> QPushButton*               = 0;
		[[nodiscard]] virtual auto activity() const noexcept -> agent_activity_view*     = 0;
		[[nodiscard]] virtual auto attachment(const std::string& aKey) const -> QWidget* = 0;
		[[nodiscard]] virtual auto card() const noexcept -> QWidget* { return nullptr; }
		[[nodiscard]] virtual auto recovery() const noexcept -> QPushButton* { return nullptr; }
	};

	/// One turn: the reviewer's bubble, what was steered in, and the answer
	/// (reply, why it failed, the ending line with the turn's actions, its
	/// attachments). Each part is changed in place when the turn changes.
	class chat_transcript_view::turn_view final : public any_turn_view
	{
	public:
		turn_view(chat_transcript_view& aOwner, QWidget* aParent)
			: any_turn_view{ aParent }
			, Owner{ aOwner }
		{
			const auto& names = Owner.Names;
			auto* const outer = new QVBoxLayout{ this };
			outer->setContentsMargins(0, 0, 0, 0);
			outer->setSpacing(12);

			// The reviewer's words, on the right.
			BubbleRow           = new QWidget{ this };
			auto* const mineRow = new QHBoxLayout{ BubbleRow };
			mineRow->setContentsMargins(0, 0, 0, 0);
			mineRow->addStretch(1);
			Bubble = new side_chat::bubble_frame{ BubbleRow };
			Bubble->setObjectName(names.Bubble);
			auto* const inside = new QVBoxLayout{ Bubble };
			inside->setContentsMargins(12, 8, 12, 8);
			inside->setSpacing(3);
			// What the chat puts above the words (a quoted passage, the prompt
			// the question was sent from).
			Above = new QVBoxLayout{ };
			Above->setContentsMargins(0, 0, 0, 0);
			Above->setSpacing(3);
			inside->addLayout(Above);
			Question = words({ }, Bubble);
			Question->setObjectName(names.Question);
			inside->addWidget(Question);
			Note = quiet(small(words({ }, Bubble)));
			Note->setObjectName(names.QuestionNote);
			Note->hide();
			inside->addWidget(Note);
			mineRow->addWidget(Bubble);
			outer->addWidget(BubbleRow);

			// What was steered into the turn, after the question.
			SteeredColumn       = new QWidget{ this };
			auto* const steered = new QVBoxLayout{ SteeredColumn };
			steered->setContentsMargins(0, 0, 0, 0);
			steered->setSpacing(12);
			outer->addWidget(SteeredColumn);

			// The agent's answer, on the left.
			Answer = new QWidget{ this };
			Column = new QVBoxLayout{ Answer };
			Column->setContentsMargins(0, 0, 0, 0);
			Column->setSpacing(4);
			// What the reply is (a local draft's check card), under it.
			ReplyAttachments = new QWidget{ Answer };
			ReplyHeld        = new QVBoxLayout{ ReplyAttachments };
			ReplyHeld->setContentsMargins(0, 0, 0, 0);
			ReplyHeld->setSpacing(4);
			ReplyAttachments->hide();
			Column->addWidget(ReplyAttachments);
			Failure = words({ }, Answer);
			Failure->setObjectName(names.Failure);
			Column->addWidget(Failure);
			EndingRow           = new QWidget{ Answer };
			auto* const endings = new QHBoxLayout{ EndingRow };
			endings->setContentsMargins(0, 0, 0, 0);
			endings->setSpacing(2);
			Ending = new agent_run_status_line{ EndingRow };
			Ending->setObjectName(names.Ending);
			small(Ending);
			endings->addWidget(Ending, 1, Qt::AlignVCenter);
			auto* const copy = new quiet_tool_button{ EndingRow };
			copy->set_tinted(true);
			copy->setObjectName(names.CopyReply);
			copy->setAutoRaise(true);
			Copy = copy;
			endings->addWidget(Copy);
			Resend = new quiet_push_button{ tr("Resend"), EndingRow };
			Resend->setObjectName(names.Resend);
			Resend->setFlat(true);
			Resend->setFocusPolicy(Qt::TabFocus);
			Resend->setToolTip(tr("Send this message to the agent again"));
			endings->addWidget(Resend);
			Delete = new quiet_push_button{ tr("Delete"), EndingRow };
			Delete->setObjectName(names.Delete);
			Delete->setFlat(true);
			Delete->setFocusPolicy(Qt::TabFocus);
			Delete->setToolTip(tr("Remove this message from the chat. Later questions are asked without it."));
			endings->addWidget(Delete);
			Column->addWidget(EndingRow);
			// [[BPR-2005]], RFC-BPR-0103. What the run is doing while it runs,
			// and its folded line once it ended; hidden while it says nothing.
			make_activity();
			Attachments = new QWidget{ Answer };
			Held        = new QVBoxLayout{ Attachments };
			Held->setContentsMargins(0, 0, 0, 0);
			Held->setSpacing(4);
			Column->addWidget(Attachments);
			outer->addWidget(Answer);

			connect(Copy, &QToolButton::clicked, this, [this] {
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(QString::fromStdString(answered() ? Turn.Reply : (Turn.FailureDetail.empty() ? Turn.Failure : Turn.FailureDetail)));
				}
			});
			Resend->setEnabled(Owner.ResendEnabled);
			connect(Resend, &QPushButton::clicked, this, [this] { act(chat_turn_gesture::resend); });
			connect(Delete, &QPushButton::clicked, this, [this] { act(chat_turn_gesture::remove); });
			restyle();
		}

		void present(const core::chat_turn& aTurn) override
		{
			const auto first   = !Presented;
			const auto settled = !first && Turn.Running && !aTurn.Running;
			Presented          = true;
			if (first || aTurn.Question != Turn.Question) {
				Question->setText(QString::fromStdString(aTurn.Question));
				side_chat::clear(Above);
				Question->show();
				if (Owner.Parts.DressBubble && !aTurn.Question.empty()) {
					Owner.Parts.DressBubble(aTurn, *Bubble, *Above, *Question);
				}
			}
			Note->setText(QString::fromStdString(aTurn.QuestionNote));
			Note->setVisible(!aTurn.QuestionNote.empty());
			BubbleRow->setVisible(!aTurn.Question.empty());
			if (first || aTurn.Steered != Turn.Steered) {
				present_steered(aTurn.Steered);
			}
			present_reply(aTurn);
			Failure->setText(QString::fromStdString(aTurn.Failure));
			Failure->setVisible(!aTurn.Failure.empty());
			const auto attachmentsChanged = first || aTurn.Attachments != Turn.Attachments;
			const auto activityChanged    = first || settled || aTurn.Activity != Turn.Activity;
			Turn                          = aTurn;
			present_ending();
			if (settled) {
				// The live block's rows and its clock end with the run.
				make_activity();
			}
			if (activityChanged && Turn.Activity && !Turn.Running) {
				Activity->fold(Turn.Activity->Activity, Turn.Activity->Seconds);
			}
			if (attachmentsChanged) {
				present_attachments();
			}
			Answer->setVisible(!Reply.isNull() || !Turn.Failure.empty() || !EndingRow->isHidden() || !Turn.Attachments.empty() || Turn.Activity.has_value() || Turn.Running);
		}

		void present_live() override
		{
			if (!Turn.Running) {
				return;
			}
			const auto status = Owner.Live.value_or(agent_run_status{ .Agent = tr("The agent") });
			Ending->present(status, Owner.LiveClock);
			if (status.Activity) {
				Activity->present(agent_short_name(status.Agent), *status.Activity, status.ActivityStartedMs);
			}
		}

		void restyle() override
		{
			Copy->setIcon(drawer_icon(drawer_glyph::copy, palette()));
			wear_agent_accent(*Resend, palette());
		}

		void enable_resend(const bool aEnabled) override { Resend->setEnabled(aEnabled); }

		void remake(const std::string& aKey) override
		{
			const auto found = std::ranges::find(Made, aKey, &made_attachment::Key);
			if (found == Made.end()) {
				return;
			}
			if (found->Widget) {
				ReplyHeld->removeWidget(found->Widget);
				Held->removeWidget(found->Widget);
				found->Widget->hide();
				found->Widget->deleteLater();
			}
			Made.erase(found);
			present_attachments();
		}

		[[nodiscard]] auto turn() const noexcept -> const core::chat_turn& override { return Turn; }
		[[nodiscard]] auto bubble() const noexcept -> QWidget* override { return BubbleRow->isHidden() ? nullptr : Bubble; }
		[[nodiscard]] auto question() const noexcept -> QLabel* override { return BubbleRow->isHidden() ? nullptr : Question; }
		[[nodiscard]] auto reply() const noexcept -> markdown_body_view* override { return Reply.data(); }
		[[nodiscard]] auto ending() const noexcept -> agent_run_status_line* override { return EndingRow->isHidden() ? nullptr : Ending; }
		[[nodiscard]] auto copy() const noexcept -> QToolButton* override { return Copy->isHidden() || EndingRow->isHidden() ? nullptr : Copy; }
		[[nodiscard]] auto resend() const noexcept -> QPushButton* override { return Resend->isHidden() || EndingRow->isHidden() ? nullptr : Resend; }
		[[nodiscard]] auto remove() const noexcept -> QPushButton* override { return Delete->isHidden() || EndingRow->isHidden() ? nullptr : Delete; }
		[[nodiscard]] auto activity() const noexcept -> agent_activity_view* override { return Activity; }
		[[nodiscard]] auto attachment(const std::string& aKey) const -> QWidget* override
		{
			const auto found = std::ranges::find(Made, aKey, &made_attachment::Key);
			return found == Made.end() ? nullptr : found->Widget.data();
		}

	private:
		/// A new activity block, in the old one's place.
		void make_activity()
		{
			auto* const made = new agent_activity_view{ Answer };
			made->setObjectName(Owner.Names.Activity);
			made->set_stop([owner = QPointer<chat_transcript_view>{ &Owner }] {
				if (owner && owner->Parts.Stop) {
					owner->Parts.Stop();
				}
			});
			if (Activity != nullptr) {
				Column->replaceWidget(Activity, made);
				Activity->hide();
				Activity->deleteLater();
			} else {
				Column->addWidget(made);
			}
			Activity = made;
		}

		[[nodiscard]] auto answered() const -> bool { return Turn.Ending && Turn.Ending->Outcome == core::agent_turn_outcome::answered; }

		void act(const chat_turn_gesture aGesture) const
		{
			if (Owner.Parts.Act) {
				Owner.Parts.Act(Turn.Id, aGesture);
			}
		}

		void present_steered(const std::vector<core::agent_steered_message>& aSteered)
		{
			auto* const column = static_cast<QVBoxLayout*>(SteeredColumn->layout());
			side_chat::clear(column);
			SteeredBubbles.clear();
			for (const auto& message : aSteered) {
				auto* const row     = new QWidget{ SteeredColumn };
				auto* const rowLine = new QHBoxLayout{ row };
				rowLine->setContentsMargins(0, 0, 0, 0);
				rowLine->addStretch(1);
				auto* const steered = new side_chat::bubble_frame{ row };
				steered->setObjectName(Owner.Names.Steered);
				auto* const lines = new QVBoxLayout{ steered };
				lines->setContentsMargins(12, 8, 12, 8);
				lines->setSpacing(3);
				auto* const state = quiet(small(words(agent_steered_state_words(message), steered)));
				state->setObjectName(Owner.Names.Steered + QStringLiteral("State"));
				lines->addWidget(state);
				auto* const text = words(QString::fromStdString(message.Text), steered);
				text->setObjectName(Owner.Names.Steered + QStringLiteral("Text"));
				lines->addWidget(text);
				steered->setAccessibleName(agent_steered_accessible_name(message));
				rowLine->addWidget(steered);
				column->addWidget(row);
				SteeredBubbles.push_back(steered);
			}
			SteeredColumn->setVisible(!aSteered.empty());
		}

		void present_reply(const core::chat_turn& aTurn)
		{
			if (aTurn.Reply.empty()) {
				if (Reply) {
					Reply->hide();
					Reply->deleteLater();
					Reply = nullptr;
				}
				return;
			}
			if (Reply) {
				if (aTurn.Reply != Turn.Reply) {
					Reply->set_body(body_of(aTurn), identity(aTurn));
					Owner.fit_soon();
				}
				return;
			}
			auto* const view = new markdown_body_view{ Answer };
			view->setObjectName(Owner.Names.Reply);
			view->set_in_place(true);
			// [[BPR-1944]]. A reply's code is a box as wide as the chat, with
			// Wrap and Copy.
			view->set_code_block_tools(true);
			view->setFocusPolicy(Qt::NoFocus);
			if (Owner.Parts.DressReader) {
				Owner.Parts.DressReader(*view);
			}
			view->set_height_observer([owner = QPointer<chat_transcript_view>{ &Owner }] {
				if (owner) {
					owner->fit_soon();
				}
			});
			view->set_body(body_of(aTurn), identity(aTurn));
			// As tall as it reads now, so the transcript keeps its height and
			// a reader scrolled up keeps their place.
			view->setFixedHeight(view->heightForWidth(std::max(120, Owner.available_width())));
			Column->insertWidget(0, view);
			Reply = view;
		}

		[[nodiscard]] auto body_of(const core::chat_turn& aTurn) const -> std::string { return Owner.Parts.ReplyBody ? Owner.Parts.ReplyBody(aTurn) : aTurn.Reply; }
		[[nodiscard]] auto identity(const core::chat_turn& aTurn) const -> std::string { return Owner.Names.Reply.toStdString() + ":" + aTurn.Id; }

		void present_ending()
		{
			if (Turn.Running) {
				EndingRow->show();
				Copy->hide();
				Resend->hide();
				Delete->hide();
				present_live();
				return;
			}
			if (!Turn.Ending) {
				EndingRow->hide();
				Ending->settle({ }, true);
				return;
			}
			EndingRow->show();
			const auto good = answered();
			Ending->settle(QString::fromStdString(core::chat_ending_words(*Turn.Ending)), good);
			Copy->setVisible(Turn.Actions.Copy);
			Copy->setToolTip(good ? tr("Copy this reply") : tr("Copy what went wrong"));
			Copy->setAccessibleName(good ? tr("Copy reply") : tr("Copy error"));
			Resend->setVisible(Turn.Actions.Recovery == core::agent_turn_recovery::resend);
			Delete->setVisible(Turn.Actions.Delete);
		}

		void present_attachments()
		{
			auto kept = std::vector<made_attachment>{ };
			for (const auto& attachment : Turn.Attachments) {
				const auto found = std::ranges::find(Made, attachment.Key, &made_attachment::Key);
				if (found != Made.end() && found->Value == attachment && found->Widget) {
					kept.push_back(std::move(*found));
					Made.erase(found);
					continue;
				}
				auto* const holder = attachment.Place != core::chat_attachment_place::after_ending ? ReplyAttachments : Attachments;
				auto* const widget = Owner.Parts.MakeAttachment ? Owner.Parts.MakeAttachment(Turn, attachment, holder) : nullptr;
				kept.push_back({ .Key = attachment.Key, .Value = attachment, .Widget = widget });
			}
			// What is no longer held goes; the rest stands in the turn's order.
			for (auto& gone : Made) {
				if (gone.Widget) {
					ReplyHeld->removeWidget(gone.Widget);
					Held->removeWidget(gone.Widget);
					gone.Widget->hide();
					gone.Widget->deleteLater();
				}
			}
			Made        = std::move(kept);
			auto placed = std::array<int, 2>{ 0, 0 };
			for (const auto& each : Made) {
				auto* const widget = each.Widget.data();
				if (widget == nullptr) {
					continue;
				}
				const auto  underReply = each.Value.Place != core::chat_attachment_place::after_ending;
				auto* const layout     = underReply ? ReplyHeld : Held;
				auto&       at         = placed[underReply ? 1U : 0U];
				if (layout->indexOf(widget) != at) {
					layout->removeWidget(widget);
					layout->insertWidget(at, widget);
				}
				++at;
			}
			Attachments->setVisible(placed[0] > 0);
			ReplyAttachments->setVisible(placed[1] > 0);
			Owner.fit_soon();
		}

		chat_transcript_view&        Owner;
		core::chat_turn              Turn;
		bool                         Presented     = false;
		QWidget*                     BubbleRow     = nullptr;
		QWidget*                     Bubble        = nullptr;
		QLabel*                      Question      = nullptr;
		QWidget*                     SteeredColumn = nullptr;
		std::vector<QWidget*>        SteeredBubbles;
		QWidget*                     Answer = nullptr;
		QVBoxLayout*                 Column = nullptr;
		QPointer<markdown_body_view> Reply;
		QLabel*                      Failure          = nullptr;
		QWidget*                     EndingRow        = nullptr;
		agent_run_status_line*       Ending           = nullptr;
		QToolButton*                 Copy             = nullptr;
		QPushButton*                 Resend           = nullptr;
		QPushButton*                 Delete           = nullptr;
		QWidget*                     Attachments      = nullptr;
		QVBoxLayout*                 Held             = nullptr;
		QWidget*                     ReplyAttachments = nullptr;
		QVBoxLayout*                 ReplyHeld        = nullptr;
		QVBoxLayout*                 Above            = nullptr;
		QLabel*                      Note             = nullptr;
		agent_activity_view*         Activity         = nullptr;
		std::vector<made_attachment> Made;
	};

	/// One turn in the Agent tab's look (RFC-BPR-0130 stage 5): the time it
	/// was asked over the question's outlined field, what was steered in, and
	/// the reply's card: the chat's parts over and under the reply, the
	/// ending line, what the run did, and the card's foot (Copy, the chat's
	/// own actions, the one recovery, Delete, the time it was answered).
	class chat_transcript_view::card_turn_view final : public any_turn_view
	{
	public:
		card_turn_view(chat_transcript_view& aOwner, QWidget* aParent)
			: any_turn_view{ aParent }
			, Owner{ aOwner }
		{
		}

		void present(const core::chat_turn& aTurn) override
		{
			const auto first   = !Presented;
			const auto settled = !first && Turn.Running && !aTurn.Running;
			const auto steered = first || aTurn.Steered != Turn.Steered;
			const auto changed = first || settled || aTurn.Activity != Turn.Activity;
			const auto held    = !first && aTurn.Attachments != Turn.Attachments;
			Turn               = aTurn;
			if (first) {
				make();
				Presented = true;
			}
			if (steered) {
				present_steered();
			}
			if (held) {
				present_attachments();
			}
			refresh_reply();
			present_ending();
			if (changed && Turn.Activity && !Turn.Running) {
				Activity->fold(Turn.Activity->Activity, Turn.Activity->Seconds);
			}
			present_actions();
		}

		void present_live() override
		{
			if (!Turn.Running) {
				return;
			}
			const auto status = Owner.Live.value_or(agent_run_status{ .Agent = tr("The agent") });
			Ending->present(status, Owner.LiveClock);
			if (status.Activity) {
				Activity->present(agent_short_name(status.Agent), *status.Activity, status.ActivityStartedMs);
			}
		}

		void restyle() override
		{
			if (Copy == nullptr) {
				return;
			}
			// The view hears a palette change before its turns take it: the
			// glyphs are drawn in the view's.
			Copy->setIcon(drawer_icon(drawer_glyph::copy, Owner.palette()));
			if (Turn.Actions.Recovery == core::agent_turn_recovery::resend) {
				wear_agent_accent(*Recovery, Owner.palette());
			}
		}

		void enable_resend(const bool aEnabled) override { static_cast<void>(aEnabled); }

		void remake(const std::string& aKey) override
		{
			const auto found = std::ranges::find(Made, aKey, &made_attachment::Key);
			if (found == Made.end()) {
				return;
			}
			forget(*found);
			Made.erase(found);
			present_attachments();
		}

		void refresh_reply() override
		{
			if (Reader == nullptr) {
				return;
			}
			const auto body     = Owner.Parts.ReplyBody ? Owner.Parts.ReplyBody(Turn) : Turn.Reply;
			const auto identity = Owner.Parts.ReplyIdentity ? Owner.Parts.ReplyIdentity(Turn) : Owner.Names.Reply.toStdString() + ":" + Turn.Id;
			if (!Shown || body != ShownBody || identity != ShownIdentity) {
				Reader->set_body(body, identity);
				Shown         = true;
				ShownBody     = body;
				ShownIdentity = identity;
			}
			Reader->setVisible(!body.empty());
			fit(Owner.available_width());
		}

		void fit(const int aWidth) override
		{
			// The reader is the column less its card's margins. Owner look
			// v004: measured without the card's, an answer was given the height
			// of a wider one and lost its last line.
			if (Reader == nullptr) {
				return;
			}
			const auto width = aWidth - agent_card::reply_left_margin - agent_card::reply_right_margin;
			Reader->setFixedHeight(Reader->isHidden() ? 0 : Reader->heightForWidth(std::max(1, width)));
		}

		[[nodiscard]] auto turn() const noexcept -> const core::chat_turn& override { return Turn; }
		[[nodiscard]] auto bubble() const noexcept -> QWidget* override { return Asked; }
		[[nodiscard]] auto question() const noexcept -> QLabel* override { return Question; }
		[[nodiscard]] auto reply() const noexcept -> markdown_body_view* override { return Reader; }
		[[nodiscard]] auto ending() const noexcept -> agent_run_status_line* override { return Ending; }
		[[nodiscard]] auto copy() const noexcept -> QToolButton* override { return Copy; }
		[[nodiscard]] auto resend() const noexcept -> QPushButton* override { return Turn.Actions.Recovery == core::agent_turn_recovery::resend ? Recovery : nullptr; }
		[[nodiscard]] auto remove() const noexcept -> QPushButton* override { return Delete; }
		[[nodiscard]] auto activity() const noexcept -> agent_activity_view* override { return Activity; }
		[[nodiscard]] auto attachment(const std::string& aKey) const -> QWidget* override
		{
			const auto found = std::ranges::find(Made, aKey, &made_attachment::Key);
			return found == Made.end() ? nullptr : found->Widget.data();
		}
		[[nodiscard]] auto card() const noexcept -> QWidget* override { return Card; }
		[[nodiscard]] auto recovery() const noexcept -> QPushButton* override { return Recovery; }

	private:
		void make()
		{
			const auto& names = Owner.Names;
			setObjectName(names.Turn);
			auto* const column = new QVBoxLayout{ this };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(4);
			// Owner look v003 and the agent drawers: the question in an
			// outlined field, its time right-aligned above it.
			auto* const askedTime = new QLabel{ this };
			askedTime->setObjectName(names.AskedTime);
			askedTime->setForegroundRole(QPalette::PlaceholderText);
			askedTime->setAlignment(Qt::AlignRight);
			column->addWidget(askedTime);
			Asked = new agent_card::rounded{ agent_card::rounded::look::field, this };
			Asked->setObjectName(names.Bubble);
			auto* const askedColumn = new QVBoxLayout{ Asked };
			askedColumn->setContentsMargins(10, 6, 10, 6);
			Question = new QLabel{ Asked };
			Question->setObjectName(names.Question);
			Question->setWordWrap(true);
			Question->setOpenExternalLinks(false);
			askedColumn->addWidget(Question);
			// What the chat says of the question: what it is about, the prompt
			// it was sent from, its words and links.
			if (Owner.Parts.DressBubble) {
				Owner.Parts.DressBubble(Turn, *Asked, *askedColumn, *Question);
			}
			column->addWidget(Asked);
			// [[BPR-2153]], look steer-v001. What the reviewer steered into
			// the turn, after the question and before the reply.
			SteeredColumn = new QWidget{ this };
			SteeredColumn->setObjectName(names.SteeredColumn);
			auto* const steeredColumn = new QVBoxLayout{ SteeredColumn };
			steeredColumn->setContentsMargins(0, 0, 0, 0);
			steeredColumn->setSpacing(4);
			SteeredColumn->hide();
			column->addWidget(SteeredColumn);
			// Owner look v003 ("the response contained in something we could
			// copy"): the reply in its own card.
			auto* const card = new agent_card::rounded{ agent_card::rounded::look::card, this };
			Card             = card;
			card->setObjectName(names.Card);
			auto* const replyColumn = new QVBoxLayout{ card };
			replyColumn->setContentsMargins(agent_card::reply_left_margin, 8, agent_card::reply_right_margin, 6);
			replyColumn->setSpacing(6);
			auto* const before = held_column(*replyColumn);
			// Owner decision (a): the answer reads as the agent wrote it, its
			// Markdown rendered in place.
			Reader = new markdown_body_view{ card };
			Reader->setObjectName(names.Reply);
			Reader->set_in_place(true);
			// [[BPR-1944]]. Its code blocks are boxes as wide as the chat,
			// with Wrap and Copy.
			Reader->set_code_block_tools(true);
			Reader->set_height_observer([this] { fit(Owner.available_width()); });
			if (Owner.Parts.DressReader) {
				Owner.Parts.DressReader(*Reader);
			}
			Reader->hide();
			replyColumn->addWidget(Reader);
			auto* const after        = held_column(*replyColumn);
			auto* const answeredTime = new QLabel{ card };
			answeredTime->setObjectName(names.AnsweredTime);
			answeredTime->setForegroundRole(QPalette::PlaceholderText);
			answeredTime->hide();
			if (Owner.Parts.DressCard) {
				Owner.Parts.DressCard(Turn, card_slots{ .Card = card, .BeforeReply = before, .AfterReply = after, .AskedTime = askedTime, .AnsweredTime = answeredTime });
			}
			// What the chat hangs on the turn stands after its own parts over
			// and under the reply; what it holds already is made here, where
			// it is read.
			BeforeHeld    = before;
			AfterHeld     = after;
			BeforeDressed = before->count();
			AfterDressed  = after->count();
			present_attachments();
			// While the agent works, and how it ended: the one line.
			Ending = new agent_run_status_line{ card };
			Ending->setObjectName(names.Ending);
			replyColumn->addWidget(Ending);
			// [[BPR-2005]], RFC-BPR-0103. Its helpers and the quiet notice
			// while it runs; the folded line after.
			Activity = new agent_activity_view{ card };
			Activity->setObjectName(names.Activity);
			Activity->set_stop([owner = QPointer<chat_transcript_view>{ &Owner }] {
				if (owner && owner->Parts.Stop) {
					owner->Parts.Stop();
				}
			});
			replyColumn->addWidget(Activity);
			EndingHeld       = held_column(*replyColumn);
			auto* const foot = new QHBoxLayout{ };
			foot->setSpacing(2);
			auto* const copy = make_drawer_tool(card, drawer_icon(drawer_glyph::copy, palette()), tr("Copy the response as Markdown"), names.CopyReply);
			copy->set_surface(QPalette::Base);
			copy->hide();
			Copy = copy;
			foot->addWidget(Copy);
			auto* const actions = new QHBoxLayout{ };
			actions->setContentsMargins(0, 0, 0, 0);
			actions->setSpacing(2);
			foot->addLayout(actions);
			if (Owner.Parts.DressFoot) {
				Owner.Parts.DressFoot(Turn, *card, *actions);
			}
			// [[BPR-1847]]. An error's one way back, which an answer never has.
			Recovery = new quiet_push_button{ card };
			Recovery->setObjectName(names.Recovery);
			Recovery->setFlat(true);
			Recovery->hide();
			foot->addWidget(Recovery);
			// [[BPR-1876]], [[BPR-1880]]. Delete, on every error.
			Delete = new quiet_push_button{ tr("Delete"), card };
			Delete->setObjectName(names.Delete);
			Delete->setFlat(true);
			Delete->hide();
			foot->addWidget(Delete);
			foot->addStretch(1);
			foot->addWidget(answeredTime);
			replyColumn->addLayout(foot);
			column->addWidget(card);
			connect(Copy, &QToolButton::clicked, this, [this] {
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(QString::fromStdString(Turn.Reply));
				}
				Copy->setToolTip(tr("Copied"));
			});
			connect(Recovery, &QPushButton::clicked, this, [this] {
				const auto gesture = Turn.Actions.Recovery == core::agent_turn_recovery::agent_settings ? chat_turn_gesture::agent_settings : chat_turn_gesture::resend;
				if (Turn.Actions.Recovery != core::agent_turn_recovery::none && Owner.Parts.Act) {
					Owner.Parts.Act(Turn.Id, gesture);
				}
			});
			connect(Delete, &QPushButton::clicked, this, [this] {
				if (Owner.Parts.Act) {
					Owner.Parts.Act(Turn.Id, chat_turn_gesture::remove);
				}
			});
			Finished = true;
		}

		/// A column in the card for the chat's own parts, spaced as the card.
		[[nodiscard]] static auto held_column(QVBoxLayout& aCard) -> QVBoxLayout*
		{
			auto* const held = new QVBoxLayout{ };
			held->setContentsMargins(0, 0, 0, 0);
			held->setSpacing(aCard.spacing());
			aCard.addLayout(held);
			return held;
		}

		/// The column an attachment stands in, and how many of the chat's own
		/// parts stand there before the attachments.
		[[nodiscard]] auto column_of(const core::chat_attachment_place aPlace) const -> std::pair<QVBoxLayout*, int>
		{
			switch (aPlace) {
			case core::chat_attachment_place::before_reply: return { BeforeHeld, BeforeDressed };
			case core::chat_attachment_place::after_reply:  return { AfterHeld, AfterDressed };
			case core::chat_attachment_place::after_ending: return { EndingHeld, 0 };
			}
			return { AfterHeld, AfterDressed };
		}

		void forget(made_attachment& aGone) const
		{
			if (!aGone.Widget) {
				return;
			}
			for (auto* const column : { BeforeHeld, AfterHeld, EndingHeld }) {
				if (column != nullptr) {
					column->removeWidget(aGone.Widget);
				}
			}
			aGone.Widget->hide();
			aGone.Widget->deleteLater();
		}

		/// Makes the widget of each attachment the turn holds that differs
		/// from the one made for it, lets go of those it no longer holds, and
		/// stands the rest in the turn's order in their places. The chat's
		/// factory makes the widgets; the turn's own parts are untouched.
		void present_attachments()
		{
			auto kept  = std::vector<made_attachment>{ };
			auto fresh = std::vector<std::pair<QWidget*, core::chat_attachment_place>>{ };
			for (const auto& attachment : Turn.Attachments) {
				const auto found = std::ranges::find(Made, attachment.Key, &made_attachment::Key);
				if (found != Made.end() && found->Value == attachment) {
					kept.push_back(std::move(*found));
					Made.erase(found);
					continue;
				}
				auto* const widget = Owner.Parts.MakeAttachment ? Owner.Parts.MakeAttachment(Turn, attachment, Card) : nullptr;
				kept.push_back({ .Key = attachment.Key, .Value = attachment, .Widget = widget });
				if (widget != nullptr) {
					fresh.emplace_back(widget, attachment.Place);
				}
			}
			for (auto& gone : Made) {
				forget(gone);
			}
			Made        = std::move(kept);
			auto placed = std::array<int, 3>{ 0, 0, 0 };
			for (const auto& each : Made) {
				auto* const widget = each.Widget.data();
				if (widget == nullptr) {
					continue;
				}
				const auto [column, dressed] = column_of(each.Value.Place);
				auto& at                     = placed[static_cast<std::size_t>(each.Value.Place)];
				if (column->indexOf(widget) != dressed + at) {
					column->removeWidget(widget);
					column->insertWidget(dressed + at, widget);
				}
				++at;
			}
			// One made after the turn is read where it stands: over the reply
			// before the reply, under it before the ending line, after what
			// the run did before the foot.
			if (Finished) {
				for (const auto& [widget, place] : fresh) {
					auto* const next = place == core::chat_attachment_place::before_reply ? static_cast<QWidget*>(Reader)
						: place == core::chat_attachment_place::after_reply               ? static_cast<QWidget*>(Ending)
																						  : static_cast<QWidget*>(Copy);
					focus_before(*widget, *next);
				}
			}
			Owner.fit_soon();
		}

		void present_steered()
		{
			auto* const column = static_cast<QVBoxLayout*>(SteeredColumn->layout());
			side_chat::clear(column);
			for (const auto& message : Turn.Steered) {
				auto* const bubble = new agent_card::rounded{ agent_card::rounded::look::field, SteeredColumn };
				bubble->setObjectName(Owner.Names.Steered);
				auto* const lines = new QVBoxLayout{ bubble };
				lines->setContentsMargins(10, 6, 10, 6);
				lines->setSpacing(2);
				auto* const state = new QLabel{ agent_steered_state_words(message), bubble };
				state->setObjectName(Owner.Names.Steered + QStringLiteral("State"));
				state->setForegroundRole(QPalette::PlaceholderText);
				state->setWordWrap(true);
				lines->addWidget(state);
				auto* const text = new QLabel{ QString::fromStdString(message.Text), bubble };
				text->setObjectName(Owner.Names.Steered + QStringLiteral("Text"));
				text->setTextFormat(Qt::PlainText);
				text->setWordWrap(true);
				text->setTextInteractionFlags(Qt::TextSelectableByMouse);
				lines->addWidget(text);
				bubble->setAccessibleName(agent_steered_accessible_name(message));
				column->addWidget(bubble);
			}
			SteeredColumn->setVisible(!Turn.Steered.empty());
		}

		void present_ending()
		{
			// A running turn's line is the chat's to present as the run goes.
			if (Turn.Running || !Turn.Ending) {
				return;
			}
			// Stopped reads as the agent drawers' Interrupted: in italics.
			Ending->settle(QString::fromStdString(core::chat_ending_words(*Turn.Ending)), Turn.Ending->Outcome == core::agent_turn_outcome::answered);
			if (Owner.Parts.DressEnding) {
				Owner.Parts.DressEnding(Turn, *Ending);
			}
		}

		/// What the ended turn offers, as its projector decided.
		void present_actions()
		{
			if (Turn.Running || !Turn.Ending) {
				return;
			}
			const auto& actions = Turn.Actions;
			// A fixed error is no longer drawn as one that holds now.
			const auto look        = actions.Error && !actions.FixedSince ? agent_card::rounded::look::danger : agent_card::rounded::look::card;
			const auto description = !actions.Error ? QString{ } : actions.FixedSince ? tr("Error, fixed since")
																					  : tr("Error");
			if (Card->Look != look || Card->accessibleDescription() != description) {
				Card->Look = look;
				Card->setAccessibleDescription(description);
				Card->update();
			}
			Copy->setVisible(actions.Copy);
			Copy->setToolTip(actions.Error ? tr("Copy the error and what the agent wrote") : tr("Copy the response as Markdown"));
			switch (actions.Recovery) {
			case core::agent_turn_recovery::none: break;
			case core::agent_turn_recovery::resend:
				Recovery->setText(tr("Resend"));
				Recovery->setToolTip(tr("Send this message to the agent again"));
				// [[BPR-2000]]. Resend starts a run.
				wear_agent_accent(*Recovery, palette());
				break;
			case core::agent_turn_recovery::agent_settings:
				Recovery->setText(tr("Agent settings…"));
				Recovery->setToolTip(tr("Check the agent's program and the form it answers in"));
				wear_ordinary_roles(*Recovery);
				break;
			}
			Recovery->setVisible(actions.Recovery != core::agent_turn_recovery::none);
			// [[BPR-1880]]. A message the agent received is forgotten once
			// deleted: no agent session is resumed, and the next message
			// carries the kept thread without it.
			Delete->setToolTip(core::agent_turn_never_sent(Turn.Ending->Outcome) ? tr("Remove this message from the conversation. It never reached the agent.")
																				 : tr("Remove this message from the conversation. The agent won't be sent it again, so it won't remember it."));
			Delete->setVisible(actions.Delete);
		}

		chat_transcript_view&  Owner;
		core::chat_turn        Turn;
		bool                   Presented     = false;
		agent_card::rounded*   Asked         = nullptr;
		QLabel*                Question      = nullptr;
		QWidget*               SteeredColumn = nullptr;
		agent_card::rounded*   Card          = nullptr;
		markdown_body_view*    Reader        = nullptr;
		agent_run_status_line* Ending        = nullptr;
		agent_activity_view*   Activity      = nullptr;
		quiet_tool_button*     Copy          = nullptr;
		QPushButton*           Recovery      = nullptr;
		QPushButton*           Delete        = nullptr;
		bool                   Shown         = false;
		std::string            ShownBody;
		std::string            ShownIdentity;
		/// The columns the chat's attachments stand in, and how many of its
		/// own parts come first in each.
		QVBoxLayout*                 BeforeHeld    = nullptr;
		QVBoxLayout*                 AfterHeld     = nullptr;
		QVBoxLayout*                 EndingHeld    = nullptr;
		int                          BeforeDressed = 0;
		int                          AfterDressed  = 0;
		std::vector<made_attachment> Made;
		/// The turn's own parts are all made.
		bool Finished = false;
	};

	chat_transcript_view::chat_transcript_view(const names& aNames, QWidget* aParent, const chat_transcript_look aLook)
		: QScrollArea{ aParent }
		, Look{ aLook }
		, Names{ aNames }
	{
		const auto cards = Look == chat_transcript_look::card;
		setObjectName(Names.Scroll);
		setWidgetResizable(true);
		setFrameShape(QFrame::NoFrame);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		if (!cards) {
			setFocusPolicy(Qt::NoFocus);
		}
		auto* const body   = new QWidget{ this };
		auto* const column = new QVBoxLayout{ body };
		// The Agent tab's cards stand in from both edges; a side chat's turns
		// run to its seam.
		const auto spacing = cards ? 10 : 12;
		if (cards) {
			column->setContentsMargins(12, 8, 12, 12);
		} else {
			column->setContentsMargins(0, 6, 6, 6);
		}
		column->setSpacing(spacing);
		Lead  = new QVBoxLayout{ };
		Turns = new QVBoxLayout{ };
		Trail = new QVBoxLayout{ };
		for (auto* const part : { Lead, Turns, Trail }) {
			part->setContentsMargins(0, 0, 0, 0);
			part->setSpacing(spacing);
			if (part == Turns && cards) {
				// The card look's turns stand in a column of their own, shown
				// once there is a turn.
				TurnsHolder = new QWidget{ body };
				TurnsHolder->setObjectName(Names.Turns);
				TurnsHolder->setLayout(Turns);
				TurnsHolder->hide();
				column->addWidget(TurnsHolder);
				continue;
			}
			column->addLayout(part);
		}
		column->addStretch(1);
		setWidget(body);
		viewport()->installEventFilter(this);
		// [[BPR-2296]]. At its end, the chat stays there as it grows.
		stick_to_bottom::attach(*this);
		// A turn cut at the top goes on under a drawn edge.
		transcript_edge::attach(*this);
	}

	chat_transcript_view::~chat_transcript_view() = default;

	void chat_transcript_view::set_parts(parts aParts) { Parts = std::move(aParts); }

	void chat_transcript_view::present(const core::chat_transcript& aTranscript)
	{
		LastDelta = core::chat_transcript_delta(Shown, aTranscript);
		if (LastDelta.empty()) {
			return;
		}
		auto byId = std::unordered_map<std::string, any_turn_view*>{ };
		for (std::size_t index = 0U; index < Views.size(); ++index) {
			byId.emplace(Shown.Turns[index].Id, Views[index]);
		}
		auto appended = std::unordered_map<std::string, bool>{ };
		auto changed  = std::unordered_map<std::string, bool>{ };
		for (const auto& change : LastDelta) {
			if (change.Kind == core::chat_change_kind::removed) {
				if (const auto found = byId.find(change.Turn); found != byId.end()) {
					Turns->removeWidget(found->second);
					found->second->hide();
					found->second->deleteLater();
					byId.erase(found);
				}
			} else if (change.Kind == core::chat_change_kind::appended) {
				appended[change.Turn] = true;
			} else {
				changed[change.Turn] = true;
			}
		}
		auto views = std::vector<any_turn_view*>{ };
		views.reserve(aTranscript.Turns.size());
		auto* const body = TurnsHolder != nullptr ? TurnsHolder : widget();
		for (std::size_t index = 0U; index < aTranscript.Turns.size(); ++index) {
			const auto& turn = aTranscript.Turns[index];
			if (appended.contains(turn.Id)) {
				auto* const made = Look == chat_transcript_look::card ? static_cast<any_turn_view*>(new card_turn_view{ *this, body }) : new turn_view{ *this, body };
				made->present(turn);
				// The turns before it already stand in their order.
				Turns->insertWidget(static_cast<int>(index), made);
				views.push_back(made);
				continue;
			}
			auto* const kept = byId.at(turn.Id);
			if (changed.contains(turn.Id)) {
				kept->present(turn);
			}
			views.push_back(kept);
		}
		Views = std::move(views);
		Shown = aTranscript;
		if (TurnsHolder != nullptr) {
			TurnsHolder->setVisible(!Views.empty());
		}
		stick_to_bottom::attach(*this).set_turn_running(std::ranges::any_of(Shown.Turns, &core::chat_turn::Running));
		fit_soon();
	}

	auto chat_transcript_view::index_of(const std::string& aId) const -> std::optional<std::size_t>
	{
		const auto found = std::ranges::find(Shown.Turns, aId, &core::chat_turn::Id);
		return found == Shown.Turns.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(std::distance(Shown.Turns.begin(), found)) };
	}

	void chat_transcript_view::refresh_reply(const std::size_t aTurn)
	{
		if (auto* const view = view_of(aTurn); view != nullptr) {
			view->refresh_reply();
		}
	}

	auto chat_transcript_view::card(const std::size_t aTurn) const -> QWidget*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->card() : nullptr;
	}

	auto chat_transcript_view::recovery(const std::size_t aTurn) const -> QPushButton*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->recovery() : nullptr;
	}

	void chat_transcript_view::present_live(const agent_run_status& aStatus, const QElapsedTimer& aStarted)
	{
		Live      = aStatus;
		LiveClock = aStarted;
		for (auto* const view : Views) {
			view->present_live();
		}
	}

	auto chat_transcript_view::view_of(const std::size_t aTurn) const -> any_turn_view* { return aTurn < Views.size() ? Views[aTurn] : nullptr; }
	auto chat_transcript_view::turn_widget(const std::size_t aTurn) const -> QWidget* { return view_of(aTurn); }

	auto chat_transcript_view::bubble(const std::size_t aTurn) const -> QWidget*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->bubble() : nullptr;
	}

	auto chat_transcript_view::question(const std::size_t aTurn) const -> QLabel*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->question() : nullptr;
	}

	auto chat_transcript_view::reply(const std::size_t aTurn) const -> markdown_body_view*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->reply() : nullptr;
	}

	auto chat_transcript_view::ending(const std::size_t aTurn) const -> agent_run_status_line*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->ending() : nullptr;
	}

	auto chat_transcript_view::copy_reply(const std::size_t aTurn) const -> QToolButton*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->copy() : nullptr;
	}

	auto chat_transcript_view::resend(const std::size_t aTurn) const -> QPushButton*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->resend() : nullptr;
	}

	auto chat_transcript_view::remove(const std::size_t aTurn) const -> QPushButton*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->remove() : nullptr;
	}

	auto chat_transcript_view::activity(const std::size_t aTurn) const -> agent_activity_view*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->activity() : nullptr;
	}

	void chat_transcript_view::remake_attachments(const std::string& aKey)
	{
		for (auto* const view : Views) {
			view->remake(aKey);
		}
	}

	void chat_transcript_view::set_resend_enabled(const bool aEnabled)
	{
		ResendEnabled = aEnabled;
		for (auto* const view : Views) {
			view->enable_resend(aEnabled);
		}
	}

	auto chat_transcript_view::attachment(const std::size_t aTurn, const std::string& aKey) const -> QWidget*
	{
		const auto* const view = view_of(aTurn);
		return view != nullptr ? view->attachment(aKey) : nullptr;
	}

	auto chat_transcript_view::readers() const -> std::vector<markdown_body_view*>
	{
		auto found = std::vector<markdown_body_view*>{ };
		for (auto* const each : widget()->findChildren<QWidget*>()) {
			if (auto* const reader = dynamic_cast<markdown_body_view*>(each); reader != nullptr) {
				found.push_back(reader);
			}
		}
		return found;
	}

	void chat_transcript_view::pin() { stick_to_bottom::attach(*this).pin(); }

	auto chat_transcript_view::available_width() const -> int
	{
		const auto* const column = widget()->layout();
		const auto        width  = viewport()->width() - column->contentsMargins().left() - column->contentsMargins().right();
		// The card look measures the column as it stands, however narrow.
		return Look == chat_transcript_look::card ? width : std::max(160, width);
	}

	void chat_transcript_view::fit_soon()
	{
		if (FitQueued) {
			return;
		}
		FitQueued = true;
		QTimer::singleShot(0, this, [this] {
			FitQueued = false;
			fit();
		});
	}

	void chat_transcript_view::fit()
	{
		// The card look fits its turns' readers alone: what a chat hangs in a
		// card is measured by the chat, at its own inset.
		if (Look == chat_transcript_look::card) {
			const auto width = available_width();
			for (auto* const view : Views) {
				view->fit(width);
			}
			return;
		}
		// Each rendered text as tall as it reads at the chat's width; the
		// reviewer's bubbles at most four fifths of it.
		const auto available = available_width();
		// Every bubble the column holds: the turns', and the chat's own.
		for (auto* const each : widget()->findChildren<QWidget*>()) {
			if (dynamic_cast<side_chat::bubble_frame*>(each) != nullptr && !each->isHidden()) {
				side_chat::fit_bubble(*each, available);
			}
		}
		for (auto* const each : readers()) {
			if (each->isHidden()) {
				continue;
			}
			// Inside a proposal's frame, less the frame's margins.
			const auto width  = dynamic_cast<side_chat::proposal_frame*>(each->parentWidget()) != nullptr ? available - 20 : available;
			const auto height = each->heightForWidth(std::max(120, width));
			if (each->height() != height || each->minimumHeight() != height) {
				each->setFixedHeight(height);
			}
		}
	}

	void chat_transcript_view::changeEvent(QEvent* aEvent)
	{
		QScrollArea::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange) {
			for (auto* const view : Views) {
				view->restyle();
			}
		}
	}

	auto chat_transcript_view::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aWatched == viewport() && aEvent->type() == QEvent::Resize) {
			// A card's reader follows the drawer's width at once, so the
			// column it stands in never lays out a stale height.
			if (Look == chat_transcript_look::card) {
				fit();
			} else {
				fit_soon();
			}
		}
		return QScrollArea::eventFilter(aWatched, aEvent);
	}
}
