#include "bpr/qt/chat_transcript_view.hpp"

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
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	using side_chat::quiet;
	using side_chat::small;
	using side_chat::words;

	/// One turn: the reviewer's bubble, what was steered in, and the answer
	/// (reply, why it failed, the ending line with the turn's actions, its
	/// attachments). Each part is changed in place when the turn changes.
	class chat_transcript_view::turn_view final : public QWidget
	{
	public:
		turn_view(chat_transcript_view& aOwner, QWidget* aParent)
			: QWidget{ aParent }
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
			Question = words({ }, Bubble);
			Question->setObjectName(names.Question);
			inside->addWidget(Question);
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
			Attachments = new QWidget{ Answer };
			Held        = new QVBoxLayout{ Attachments };
			Held->setContentsMargins(0, 0, 0, 0);
			Held->setSpacing(4);
			Column->addWidget(Attachments);
			outer->addWidget(Answer);

			connect(Copy, &QToolButton::clicked, this, [this] {
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(QString::fromStdString(answered() ? Turn.Reply : Turn.Failure));
				}
			});
			connect(Resend, &QPushButton::clicked, this, [this] { act(chat_turn_gesture::resend); });
			connect(Delete, &QPushButton::clicked, this, [this] { act(chat_turn_gesture::remove); });
			restyle();
		}

		void present(const core::chat_turn& aTurn)
		{
			const auto first = !Presented;
			Presented        = true;
			if (first || aTurn.Question != Turn.Question) {
				Question->setText(QString::fromStdString(aTurn.Question));
			}
			BubbleRow->setVisible(!aTurn.Question.empty());
			if (first || aTurn.Steered != Turn.Steered) {
				present_steered(aTurn.Steered);
			}
			present_reply(aTurn);
			Failure->setText(QString::fromStdString(aTurn.Failure));
			Failure->setVisible(!aTurn.Failure.empty());
			const auto attachmentsChanged = first || aTurn.Attachments != Turn.Attachments;
			Turn                          = aTurn;
			present_ending();
			if (attachmentsChanged) {
				present_attachments();
			}
			Answer->setVisible(!Reply.isNull() || !Turn.Failure.empty() || !EndingRow->isHidden() || !Turn.Attachments.empty());
		}

		void present_live()
		{
			if (Turn.Running) {
				Ending->present(Owner.Live.value_or(agent_run_status{ .Agent = tr("The agent") }), Owner.LiveClock);
			}
		}

		void restyle()
		{
			Copy->setIcon(drawer_icon(drawer_glyph::copy, palette()));
			wear_agent_accent(*Resend, palette());
		}

		[[nodiscard]] auto turn() const noexcept -> const core::chat_turn& { return Turn; }
		[[nodiscard]] auto bubble() const noexcept -> QWidget* { return BubbleRow->isHidden() ? nullptr : Bubble; }
		[[nodiscard]] auto question() const noexcept -> QLabel* { return BubbleRow->isHidden() ? nullptr : Question; }
		[[nodiscard]] auto reply() const noexcept -> markdown_body_view* { return Reply.data(); }
		[[nodiscard]] auto ending() const noexcept -> agent_run_status_line* { return EndingRow->isHidden() ? nullptr : Ending; }
		[[nodiscard]] auto copy() const noexcept -> QToolButton* { return Copy->isHidden() || EndingRow->isHidden() ? nullptr : Copy; }
		[[nodiscard]] auto resend() const noexcept -> QPushButton* { return Resend->isHidden() || EndingRow->isHidden() ? nullptr : Resend; }
		[[nodiscard]] auto remove() const noexcept -> QPushButton* { return Delete->isHidden() || EndingRow->isHidden() ? nullptr : Delete; }
		[[nodiscard]] auto attachment(const std::string& aKey) const -> QWidget*
		{
			const auto found = std::ranges::find(Made, aKey, &made_attachment::Key);
			return found == Made.end() ? nullptr : found->Widget.data();
		}
		[[nodiscard]] auto bubbles() const -> std::vector<QWidget*>
		{
			auto all = std::vector<QWidget*>{ };
			if (!BubbleRow->isHidden()) {
				all.push_back(Bubble);
			}
			all.insert(all.end(), SteeredBubbles.begin(), SteeredBubbles.end());
			return all;
		}

	private:
		struct made_attachment
		{
			std::string           Key;
			core::chat_attachment Value;
			QPointer<QWidget>     Widget;
		};

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
					Reply->set_body(aTurn.Reply, identity(aTurn));
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
			view->set_body(aTurn.Reply, identity(aTurn));
			// As tall as it reads now, so the transcript keeps its height and
			// a reader scrolled up keeps their place.
			view->setFixedHeight(view->heightForWidth(std::max(120, Owner.available_width())));
			Column->insertWidget(0, view);
			Reply = view;
		}

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
				auto* const widget = Owner.Parts.MakeAttachment ? Owner.Parts.MakeAttachment(Turn, attachment, Attachments) : nullptr;
				kept.push_back({ .Key = attachment.Key, .Value = attachment, .Widget = widget });
			}
			// What is no longer held goes; the rest stands in the turn's order.
			for (auto& gone : Made) {
				if (gone.Widget) {
					Held->removeWidget(gone.Widget);
					gone.Widget->hide();
					gone.Widget->deleteLater();
				}
			}
			Made = std::move(kept);
			for (std::size_t index = 0U; index < Made.size(); ++index) {
				if (auto* const widget = Made[index].Widget.data(); widget != nullptr && Held->indexOf(widget) != static_cast<int>(index)) {
					Held->removeWidget(widget);
					Held->insertWidget(static_cast<int>(index), widget);
				}
			}
			Attachments->setVisible(!Made.empty());
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
		QLabel*                      Failure     = nullptr;
		QWidget*                     EndingRow   = nullptr;
		agent_run_status_line*       Ending      = nullptr;
		QToolButton*                 Copy        = nullptr;
		QPushButton*                 Resend      = nullptr;
		QPushButton*                 Delete      = nullptr;
		QWidget*                     Attachments = nullptr;
		QVBoxLayout*                 Held        = nullptr;
		std::vector<made_attachment> Made;
	};

	chat_transcript_view::chat_transcript_view(const names& aNames, QWidget* aParent)
		: QScrollArea{ aParent }
		, Names{ aNames }
	{
		setObjectName(Names.Scroll);
		setWidgetResizable(true);
		setFrameShape(QFrame::NoFrame);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setFocusPolicy(Qt::NoFocus);
		auto* const body   = new QWidget{ this };
		auto* const column = new QVBoxLayout{ body };
		column->setContentsMargins(0, 6, 6, 6);
		column->setSpacing(12);
		Lead  = new QVBoxLayout{ };
		Turns = new QVBoxLayout{ };
		Trail = new QVBoxLayout{ };
		for (auto* const part : { Lead, Turns, Trail }) {
			part->setContentsMargins(0, 0, 0, 0);
			part->setSpacing(12);
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
		auto byId = std::unordered_map<std::string, turn_view*>{ };
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
		auto views = std::vector<turn_view*>{ };
		views.reserve(aTranscript.Turns.size());
		auto* const body = widget();
		for (std::size_t index = 0U; index < aTranscript.Turns.size(); ++index) {
			const auto& turn = aTranscript.Turns[index];
			if (appended.contains(turn.Id)) {
				auto* const made = new turn_view{ *this, body };
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
		stick_to_bottom::attach(*this).set_turn_running(std::ranges::any_of(Shown.Turns, &core::chat_turn::Running));
		fit_soon();
	}

	void chat_transcript_view::present_live(const agent_run_status& aStatus, const QElapsedTimer& aStarted)
	{
		Live      = aStatus;
		LiveClock = aStarted;
		for (auto* const view : Views) {
			view->present_live();
		}
	}

	auto chat_transcript_view::view_of(const std::size_t aTurn) const -> turn_view* { return aTurn < Views.size() ? Views[aTurn] : nullptr; }
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
		return std::max(160, viewport()->width() - column->contentsMargins().left() - column->contentsMargins().right());
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
		// Each rendered text as tall as it reads at the chat's width; the
		// reviewer's bubbles at most four fifths of it.
		const auto available = available_width();
		for (auto* const view : Views) {
			for (auto* const each : view->bubbles()) {
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
			fit_soon();
		}
		return QScrollArea::eventFilter(aWatched, aEvent);
	}
}
