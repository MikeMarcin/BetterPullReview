#include "bpr/qt/tour_side_chat.hpp"

#include "conversation_glyphs.hpp"
#include "pull_request_overview_support.hpp"
#include "side_chat_parts.hpp"
#include "wrapping_row.hpp"

#include "bpr/qt/agent_steered_words.hpp"

#include "bpr/core/agent_queue.hpp"
#include "bpr/core/agent_turn.hpp"
#include "bpr/core/chat_header.hpp"
#include "bpr/core/chat_transcript.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/local_check_card.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/tour_agent_panel.hpp"

#include <QAbstractButton>
#include <QClipboard>
#include <QDialog>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <map>

namespace bpr::qt {
	namespace {
		const auto open_quote  = QChar{ 0x201C };
		const auto close_quote = QChar{ 0x201D };
		const auto ellipsis    = QChar{ 0x2026 };

		using side_chat::clear;
		using side_chat::mixed;
		using side_chat::quiet;
		using side_chat::small;
		using side_chat::words;
		auto passage_heading(const core::tour_passage& aPassage) -> QString
		{
			switch (aPassage.Field) {
			case core::tour_passage_field::stop: return QObject::tr("From the stop's explanation");
			case core::tour_passage_field::stop_code:
				return aPassage.Path.empty() ? QObject::tr("The stop's code")
											 : QObject::tr("%1, lines %2 to %3").arg(QString::fromStdString(aPassage.Path)).arg(aPassage.FirstLine).arg(aPassage.LastLine);
			}
			return { };
		}
		using side_chat::bubble_frame;
		using side_chat::proposal_frame;
	}

	auto side_chat_icon(const QPalette& aPalette) -> QIcon
	{
		auto pixmap = QPixmap{ 32, 32 };
		pixmap.fill(Qt::transparent);
		auto painter = QPainter{ &pixmap };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ aPalette.color(QPalette::Active, QPalette::ButtonText), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
		painter.setBrush(Qt::NoBrush);
		auto bubble = QPainterPath{ };
		bubble.addRoundedRect(QRectF{ 4.0, 5.0, 24.0, 17.0 }, 6.0, 6.0);
		painter.drawPath(bubble);
		auto tail = QPainterPath{ };
		tail.moveTo(10.0, 22.0);
		tail.lineTo(9.0, 28.0);
		tail.lineTo(16.0, 22.0);
		painter.drawPath(tail);
		painter.end();
		return QIcon{ pixmap };
	}

	namespace {
		/// The bubble inside a toggle's box: its rounded body and its tail.
		void draw_bubble(QPainter& aPainter, const QRectF& aGlyph, const QColor& aInk)
		{
			aPainter.setPen(QPen{ aInk, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter.setBrush(Qt::NoBrush);
			const auto body = QRectF{ aGlyph.left() + 0.5, aGlyph.top() + 1.0, aGlyph.width() - 1.0, aGlyph.height() * 0.66 };
			aPainter.drawRoundedRect(body, 3.0, 3.0);
			auto tail = QPainterPath{ };
			tail.moveTo(body.left() + 3.0, body.bottom());
			tail.lineTo(body.left() + 2.5, aGlyph.bottom() - 0.5);
			tail.lineTo(body.left() + 6.5, body.bottom());
			aPainter.drawPath(tail);
		}
	}

	auto chat_toggle_size(const QFont& aFont, const std::size_t aTurns) -> QSize
	{
		const auto metrics = QFontMetrics{ aFont };
		const auto glyph   = 14;
		const auto figures = aTurns == 0U ? 0 : metrics.horizontalAdvance(QString::number(aTurns)) + 4;
		return { 8 + glyph + figures + 8, std::max(22, metrics.height() + 6) };
	}

	void paint_chat_toggle(QPainter& aPainter, const QRectF& aBox, const QPalette& aPalette, const std::size_t aTurns, const bool aOn, const bool aHovered)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto ground = aPalette.color(QPalette::Active, QPalette::Window);
		const auto accent = agent_accent(aPalette);
		const auto pill   = aBox.adjusted(0.5, 0.5, -0.5, -0.5);
		const auto radius = pill.height() / 2.0;
		if (aOn) {
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(accent);
			aPainter.drawRoundedRect(pill, radius, radius);
		} else {
			aPainter.setPen(QPen{ mixed(ground, accent, aHovered ? 0.85 : 0.55), 1.0 });
			aPainter.setBrush(mixed(ground, accent, aHovered ? 0.16 : 0.07));
			aPainter.drawRoundedRect(pill, radius, radius);
		}
		const auto ink   = aOn ? ground : accent;
		const auto glyph = QRectF{ pill.left() + 8.0, pill.center().y() - 7.0, 14.0, 14.0 };
		draw_bubble(aPainter, glyph, ink);
		if (aTurns != 0U) {
			auto font = aPainter.font();
			font.setWeight(QFont::DemiBold);
			aPainter.setFont(font);
			aPainter.setPen(ink);
			aPainter.drawText(QRectF{ glyph.right() + 4.0, pill.top(), pill.right() - glyph.right() - 4.0 - 6.0, pill.height() }, Qt::AlignLeft | Qt::AlignVCenter, QString::number(aTurns));
		}
		aPainter.restore();
	}

	part_chat_toggle::part_chat_toggle(const QString& aPart, QWidget* aParent)
		: QToolButton{ aParent }
		, Part{ aPart }
	{
		setCheckable(true);
		setAutoRaise(true);
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		present(0U, false);
	}

	void part_chat_toggle::present(const std::size_t aTurns, const bool aOn)
	{
		const auto resized = Turns != aTurns;
		Turns              = aTurns;
		setChecked(aOn);
		setText(aTurns == 0U ? QString{ } : QString::number(aTurns));
		const auto turns = aTurns == 0U ? tr("no turns yet") : aTurns == 1U ? tr("one turn")
																			: tr("%1 turns").arg(aTurns);
		setAccessibleName(aOn ? tr("Close the chat about %1").arg(Part) : tr("Open the chat about %1").arg(Part));
		setAccessibleDescription(turns);
		setToolTip(aOn ? tr("Close the chat about %1 (%2). Every chat is kept.").arg(Part, turns) : tr("Open the chat about %1 beside the tour (%2)").arg(Part, turns));
		if (resized) {
			updateGeometry();
		}
		update();
	}

	auto part_chat_toggle::sizeHint() const -> QSize { return chat_toggle_size(font(), Turns); }

	void part_chat_toggle::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setFont(font());
		const auto box = QRectF{ rect() }.adjusted(1.0, (height() - chat_toggle_size(font(), Turns).height()) / 2.0 + 1.0, -1.0, -(height() - chat_toggle_size(font(), Turns).height()) / 2.0 - 1.0);
		paint_chat_toggle(painter, box, palette(), Turns, isChecked(), underMouse() || isDown());
		if (hasFocus()) {
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), height() / 2.0 - 1.0, height() / 2.0 - 1.0);
		}
	}

	tour_side_chat::tour_side_chat(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("tourSideChat"));
		setAccessibleName(tr("Side chat with the tour agent"));
		setMinimumWidth(300);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(10, 8, 10, 10);
		column->setSpacing(6);

		// [[BPR-1925]], owner 2026-11-07: "it's kind of weird that the
		// sidechat is so decoupled from the tour when it's really about the
		// tour". The chat is the Tour tab's second column, beside the part it
		// is about, which is marked in the tour; the part's own glyph chooses
		// the chat, so there are no tabs. RFC-BPR-0130, header A: the one
		// row every chat with an agent wears (`chat_header_view`). Here it is
		// the part and its place, Show in code, History and close; the part
		// and its place each keep one line and end in an ellipsis where the
		// column is narrow (owner, attended review 2026-11-24: "Those buttons
		// are unreadable and thats not even the narrowest width").
		Header = new chat_header_view{ {
										   .Row     = QStringLiteral("tourSideChatHeader"),
										   .Subject = QStringLiteral("tourSideChatSubject"),
										   .Place   = QStringLiteral("tourSideChatQuote"),
										   .Show    = QStringLiteral("tourSideChatShow"),
										   .History = QStringLiteral("tourChatHistory"),
										   .Copy    = QStringLiteral("tourSideChatCopy"),
										   .Close   = QStringLiteral("tourSideChatClose"),
									   },
									   this };
		Header->set_tips({
			.Show  = tr("Open the stop in the Tour tab and show its lines in the code"),
			.Close = tr("Close the chat. Every part's chat is kept; its glyph in the tour opens it again."),
		});
		Header->set_dispatch({
			.Show             = [this] {
				if (const auto thread = current(); thread && RevealDispatch) {
					RevealDispatch(*thread);
				} },
			.CopyConversation = [this] { copy_chat(); },
			.Close            = [this] { set_column_open(false); },
		});
		Subject     = Header->subject();
		Quote       = Header->place();
		ShowPart    = Header->show_button();
		HistoryTool = Header->history_tool();
		CopyChat    = Header->copy_tool();
		ClosePane   = Header->close_tool();
		// The one History menu, the Agent tab's too (owner, the same review:
		// "It looks like tour built a different session selector from agent
		// too for no reason."): New session first, the sessions, Delete all,
		// the other parts' chats, Copy conversation, the revision read.
		HistoryMenu = new chat_history_menu{ core::chat_history_kind::tour_side_chat, *this, *HistoryTool, QStringLiteral("tourChat") };
		HistoryMenu->carry_over_action()->setObjectName(QStringLiteral("tourCarryOver"));
		const auto choose = [this](std::optional<std::string> aChoice) {
			if (auto thread = current(); thread && HistoryDispatch) {
				HistoryDispatch(*thread, std::move(aChoice));
			}
		};
		HistoryMenu->set_dispatch({
			.AboutToShow      = [choose] { choose(std::nullopt); },
			.NewSession       = [choose] { choose(std::string{ }); },
			.Open             = [choose](const std::string& aId) { choose(aId); },
			.CarryOver        = [choose](const std::string& aId) { choose("replay:" + aId); },
			.ViewOther        = [choose](const std::string& aId) { choose("view:" + aId); },
			.Edit             = [this](const std::string& aId, std::optional<std::string> aName) {
                if (auto thread = current(); thread && SessionEditDispatch) {
                    SessionEditDispatch(*thread, aId, std::move(aName));
                } },
			.CopyConversation = [this] { copy_chat(); },
			.ShowEveryRun     = { },
			.ResetAgents      = { },
		});
		// What History offers is core's, from what this chat is doing when asked.
		HistoryMenu->set_state_source([this] {
			const auto thread = current();
			return chat_history_menu::chat_state{
				.AgentReady = Ready,
				.Closed     = !(thread && (!Draft || core::active_tour_thread(*Draft, *thread))),
				.Running    = Running,
				.Busy       = !HostIdle,
				.ShownTurns = thread ? core::thread_runs(Session, *thread).size() : 0U,
				// Its session read an earlier head: never the new session,
				// asked in or not. Without this a chat nothing was asked in
				// kept New session greyed after the pull request moved, with
				// "Choose New session in History" written under it.
				.Pinned      = RevisionStale,
				.EveryRun    = false,
				.ResetAgents = false,
				.Agent       = AgentName,
			};
		});
		// A chat that keeps no sessions (a local draft's discussion) has no
		// History; core gives its header Copy conversation instead.
		column->addWidget(Header);

		// The chat (RFC-BPR-0130 stage 4): the one transcript, a turn each run
		// of the open part, what a turn proposed its attachments. At its end
		// it stays there as it grows; scrolled back, it stays where the author
		// put it; a turn cut at its top goes on under a drawn edge.
		Transcript = new chat_transcript_view{ {
												   .Scroll       = QStringLiteral("tourSideChatScroll"),
												   .Bubble       = QStringLiteral("tourSideChatBubble"),
												   .Question     = QStringLiteral("tourSideChatQuestion"),
												   .Steered      = QStringLiteral("tourSideChatSteered"),
												   .Reply        = QStringLiteral("tourSideChatReply"),
												   .Failure      = QStringLiteral("tourSideChatFailure"),
												   .Ending       = QStringLiteral("tourSideChatStatus"),
												   .CopyReply    = QStringLiteral("tourSideChatCopyReply"),
												   .Resend       = QStringLiteral("tourSideChatResend"),
												   .Delete       = QStringLiteral("tourSideChatDelete"),
												   .QuestionNote = QStringLiteral("tourSideChatWaitingNote"),
												   .Activity     = QStringLiteral("tourSideChatActivity"),
											   },
											   this };
		Transcript->set_parts({
			.DressReader    = [this](markdown_body_view& aReader) { dress_reply(aReader); },
			.MakeAttachment = [this](const core::chat_turn& aTurn, const core::chat_attachment& aAttachment, QWidget* aHolder) { return make_attachment(aTurn, aAttachment, aHolder); },
			.Act            = [this](const std::string& aTurn, const chat_turn_gesture aGesture) { act(aTurn, aGesture); },
			.DressBubble    = [this](const core::chat_turn& aTurn, QWidget& aBubble, QVBoxLayout& aAbove, QLabel& aQuestion) { dress_bubble(aTurn, aBubble, aAbove, aQuestion); },
			.ReplyBody      = [this](const core::chat_turn& aTurn) { return reply_body(aTurn); },
			.Stop =
				[this] {
					if (StopDispatch) {
						StopDispatch();
					}
				},
		});
		// [[BPR-1930]], [[BPR-1995]]. An empty chat invites a question.
		Empty = quiet(small(words({ }, Transcript->widget())));
		Empty->setObjectName(QStringLiteral("tourSideChatEmpty"));
		Empty->hide();
		Transcript->lead()->addWidget(Empty);
		// [[BPR-1941]]. A running Draft Tour, in the whole tour's chat, as the
		// Agent tab says a running turn.
		DraftBlock = new QWidget{ Transcript->widget() };
		DraftBlock->setObjectName(QStringLiteral("tourSideChatDrafting"));
		auto* const drafting = new QVBoxLayout{ DraftBlock };
		drafting->setContentsMargins(0, 0, 0, 0);
		drafting->setSpacing(2);
		auto* const draftHeading = small(words(tr("Draft Tour"), DraftBlock));
		draftHeading->setObjectName(QStringLiteral("tourSideChatDraftHeading"));
		auto heavyHeading = draftHeading->font();
		heavyHeading.setWeight(QFont::DemiBold);
		draftHeading->setFont(heavyHeading);
		Accented.emplace_back(draftHeading);
		drafting->addWidget(draftHeading);
		DraftLine = new agent_run_status_line{ DraftBlock };
		DraftLine->setObjectName(QStringLiteral("tourSideChatDraftStatus"));
		small(DraftLine);
		drafting->addWidget(DraftLine);
		DraftBlock->hide();
		Transcript->trail()->addWidget(DraftBlock);
		Transcript->viewport()->installEventFilter(this);
		column->addWidget(Transcript, 1);

		// [[BPR-1995]], RFC-BPR-0100 AC01, owner 2026-11-10: "The tour side chat
		// window UX got made ugly, especially at the bottom." Under the
		// transcript: a notice only when something is not as usual (a session
		// carried over, one the agent cannot pick up, a moved head), then the composer
		// with its passage as a chip inside the field, then what the session
		// cost. The sessions are the header's History (RFC-BPR-0130). The
		// update is offered only by the link under an answer that changed
		// nothing.
		Notice                = new QWidget{ this };
		auto* const noticeBox = new QVBoxLayout{ Notice };
		noticeBox->setContentsMargins(0, 0, 0, 0);
		noticeBox->setSpacing(2);
		Notice->setObjectName(QStringLiteral("tourSideChatNotice"));
		SessionNotice = quiet(small(new QLabel{ Notice }));
		SessionNotice->setObjectName(QStringLiteral("tourSessionNotice"));
		SessionNotice->setWordWrap(true);
		SessionNotice->setTextFormat(Qt::PlainText);
		SessionNotice->hide();
		noticeBox->addWidget(SessionNotice);
		RevisionStatus = quiet(small(new QLabel{ Notice }));
		RevisionStatus->setObjectName(QStringLiteral("tourRevisionStatus"));
		RevisionStatus->setTextFormat(Qt::PlainText);
		RevisionStatus->setWordWrap(true);
		RevisionStatus->hide();
		noticeBox->addWidget(RevisionStatus);
		ContinueOld = new quiet_tool_button{ Notice };
		ContinueOld->setObjectName(QStringLiteral("tourContinueOldRevision"));
		ContinueOld->setText(tr("Continue old revision"));
		ContinueOld->setToolTip(tr("Keep this chat's saved revision and workspace. No question is sent until you press Send."));
		ContinueOld->hide();
		noticeBox->addWidget(ContinueOld, 0, Qt::AlignLeft);
		// What the next question starts from besides the chat's own turns.
		CarriedAnswer = quiet(small(new QLabel{ Notice }));
		CarriedAnswer->setObjectName(QStringLiteral("tourCarriedAnswer"));
		CarriedAnswer->setTextFormat(Qt::PlainText);
		CarriedAnswer->setWordWrap(true);
		CarriedAnswer->hide();
		noticeBox->addWidget(CarriedAnswer);
		Notice->hide();
		column->addWidget(Notice);
		connect(ContinueOld, &QToolButton::clicked, this, [this] {
			if (!Running && !HistoryLoading && RevisionChoiceRequired && ContinuationDispatch) {
				if (const auto thread = current()) {
					ContinuationDispatch(*thread);
				}
			}
		});

		// RFC-BPR-0126 ([[BPR-2154]]). The part's waiting messages, as the
		// Agent tab shows its own, above the composer they were typed in.
		PendingList = new agent_pending_list{ this };
		PendingList->setObjectName(QStringLiteral("tourSideChatQueue"));
		column->addWidget(PendingList);
		// The composer: one field holding the passage it quotes, as a chip,
		// above the words. [[BPR-2337]] (owner, attended review b64bbc23:
		// "The send button on both the side chats (report and tour) should be
		// positioned more like the agent tab"): the Agent tab's composer, its
		// insets, its rule and Send at the right end of the row under it.
		// RFC-BPR-0130 stage 2: the one composer every chat places.
		Field                  = new chat_composer{ { .Field = QStringLiteral("tourSideChatField"), .Text = QStringLiteral("tourSideChatComposer"), .Send = QStringLiteral("tourSideChatSend"), .Stop = QStringLiteral("tourSideChatStop") }, this };
		PassageRow             = new passage_chip{ Field };
		auto* const passageRow = new QHBoxLayout{ PassageRow };
		passageRow->setContentsMargins(8, 1, 2, 1);
		passageRow->setSpacing(2);
		PassageRow->setObjectName(QStringLiteral("tourSideChatPassageChip"));
		PassageLine = small(new QLabel{ PassageRow });
		PassageLine->setObjectName(QStringLiteral("tourSideChatPassage"));
		PassageLine->setTextFormat(Qt::PlainText);
		PassageLine->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto* const clearPassage = new quiet_tool_button{ PassageRow };
		clearPassage->set_tinted(true);
		ClearPassage = clearPassage;
		ClearPassage->setObjectName(QStringLiteral("tourSideChatPassageClear"));
		ClearPassage->setAutoRaise(true);
		ClearPassage->setToolTip(tr("Ask without this passage"));
		ClearPassage->setAccessibleName(tr("Ask without this passage"));
		passageRow->addWidget(PassageLine, 1);
		passageRow->addWidget(ClearPassage, 0);
		Field->chips()->addWidget(PassageRow);
		Composer = Field->text();
		Composer->setAccessibleName(tr("Message to the tour agent"));
		// [[BPR-1916]]. `/name` completion, the composer's own.
		SkillCompletion = Field->skills();
		Send            = Field->send_button();
		// RFC-BPR-0126: Stop stays reachable while Send queues, beside it.
		StopRun = Field->stop_button();
		StopRun->setToolTip(tr("Stop the tour agent; nothing in the tour changes. Queued messages wait, paused."));
		Field->set_dispatch({
			.Send =
				[this](const bool aOpposite) {
					Opposite = aOpposite;
					send(Composer->toPlainText().trimmed(), false);
					Opposite = false;
				},
			.Stop =
				[this] {
					if (Mine && StopDispatch) {
						StopDispatch();
					}
				},
		});
		column->addWidget(Field);

		// Under the composer: Plan a check… for a local draft's whole-change
		// conversation. RFC-BPR-0130: what a session cost is not on the
		// chat's face; its row of History says it, and a chat that keeps no
		// sessions says it from its header's Copy tool.
		auto* const sessionRow = new detail::wrapping_row{ nullptr, 6, 4 };
		sessionRow->setContentsMargins(0, 0, 0, 0);
		// RFC-BPR-0120. A local draft's whole-change conversation plans a
		// check: the composer's words, or none for the agent to choose.
		PlanCheck = new quiet_push_button{ tr("Plan a check…"), this };
		PlanCheck->setObjectName(QStringLiteral("tourChatPlanCheck"));
		PlanCheck->setFlat(true);
		PlanCheck->setFocusPolicy(Qt::TabFocus);
		PlanCheck->hide();
		sessionRow->addWidget(PlanCheck);
		column->addLayout(sessionRow);

		connect(PlanCheck, &QPushButton::clicked, this, [this] {
			if (Checks.Plan && CheckAvailable && !Running) {
				const auto words = Composer->toPlainText().trimmed();
				Composer->clear();
				Checks.Plan(words);
			}
		});
		connect(ClearPassage, &QToolButton::clicked, this, [this] {
			if (const auto thread = current()) {
				Passages.erase(static_cast<std::size_t>(index_of(*thread)));
			}
			show_passage();
			Composer->setFocus(Qt::OtherFocusReason);
		});
		connect(Composer, &QPlainTextEdit::textChanged, this, [this] { refresh_actions(); });
		present_thread();
	}

	void tour_side_chat::set_dispatch(ask_dispatch aAsk, std::function<void()> aStop, decide_dispatch aDecide)
	{
		AskDispatch    = std::move(aAsk);
		StopDispatch   = std::move(aStop);
		DecideDispatch = std::move(aDecide);
	}

	auto tour_side_chat::index_of(const core::tour_thread& aThread) const -> int
	{
		const auto found = std::ranges::find(Open, aThread);
		return found == Open.end() ? -1 : static_cast<int>(found - Open.begin());
	}

	auto tour_side_chat::current() const -> std::optional<core::tour_thread>
	{
		const auto index = Current;
		return index < 0 || static_cast<std::size_t>(index) >= Open.size() ? std::nullopt : std::optional{ Open[static_cast<std::size_t>(index)] };
	}

	auto tour_side_chat::passage() const -> std::optional<core::tour_passage>
	{
		const auto index = Current;
		const auto found = index < 0 ? Passages.end() : Passages.find(static_cast<std::size_t>(index));
		return found == Passages.end() ? std::nullopt : std::optional{ found->second };
	}

	void tour_side_chat::open(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage, const bool aFocus)
	{
		auto index = index_of(aThread);
		if (index < 0) {
			Open.push_back(aThread);
			index = static_cast<int>(Open.size()) - 1;
		}
		if (aPassage) {
			Passages[static_cast<std::size_t>(index)] = std::move(*aPassage);
		}
		select(index);
		set_column_open(true);
		if (aFocus) {
			Composer->setFocus(Qt::OtherFocusReason);
		}
	}

	void tour_side_chat::select(const int aIndex)
	{
		if (aIndex < 0 || static_cast<std::size_t>(aIndex) >= Open.size()) {
			return;
		}
		if (Current != aIndex) {
			Current = aIndex;
			// What another part's chat starts from is the host's to say.
			Carried = tour_carried_answer::none;
			Edited.clear();
			Editing.clear();
			// Another chat opens at its end.
			Transcript->pin();
		}
		present_thread();
		changed();
		if (auto thread = current(); thread && HistoryDispatch) {
			HistoryDispatch(*thread, std::nullopt);
		}
	}

	void tour_side_chat::set_column_open(const bool aOpen)
	{
		const auto was = ColumnOpen;
		ColumnOpen     = aOpen && current().has_value();
		if (VisibilityDispatch && (was != ColumnOpen || ColumnOpen)) {
			VisibilityDispatch(ColumnOpen);
		}
		if (was != ColumnOpen) {
			changed();
		}
	}

	void tour_side_chat::changed()
	{
		if (ChangeObserver) {
			ChangeObserver();
		}
	}

	auto tour_side_chat::discussed() const -> std::optional<core::tour_thread>
	{
		return ColumnOpen ? current() : std::nullopt;
	}

	auto tour_side_chat::turns_about(const core::tour_thread& aThread) const -> std::size_t
	{
		return core::thread_runs(Session, aThread).size();
	}

	auto tour_side_chat::chat_parts() const -> std::size_t
	{
		return core::chat_threads(Session).size();
	}

	void tour_side_chat::close(const core::tour_thread& aThread)
	{
		const auto index = index_of(aThread);
		if (index < 0) {
			return;
		}
		// Passages are kept by tab place: those after this one move up.
		auto moved = std::map<std::size_t, core::tour_passage>{ };
		for (auto& [at, held] : Passages) {
			if (static_cast<int>(at) != index) {
				moved[static_cast<int>(at) > index ? at - 1U : at] = std::move(held);
			}
		}
		Passages = std::move(moved);
		Open.erase(Open.begin() + index);
		// The chat after it takes its place, or the one before at the end.
		// The chat in the column was this one: the column closes.
		const auto shown = Current == index;
		if (Current > index || Current >= static_cast<int>(Open.size())) {
			--Current;
		}
		Edited.clear();
		Editing.clear();
		if (shown || Open.empty()) {
			set_column_open(false);
		}
		present_thread();
		changed();
	}

	void tour_side_chat::present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal, std::shared_ptr<const core::review_tour> aDraft)
	{
		const auto same = Proposal && aProposal && Proposal->Run == aProposal->Run;
		if (!same) {
			Edited.clear();
			Editing.clear();
		}
		Session  = aSession;
		Proposal = aProposal;
		Draft    = std::move(aDraft);

		present_thread();
		changed();
	}

	void tour_side_chat::set_agent_name(const QString& aName)
	{
		if (AgentName == aName) {
			return;
		}
		AgentName = aName;
		present_session_notice();
		HistoryMenu->refresh();
	}

	void tour_side_chat::present_session_notice()
	{
		// RFC-BPR-0130's accepted words, the Agent tab's too; a session that
		// goes on in the ordinary way says nothing.
		const auto words = CarriedOver ? chat_history_menu::carried_over_words(AgentName) : CannotContinue ? chat_history_menu::cannot_continue_words(AgentName)
																										   : QString{ };
		SessionNotice->setText(words);
		SessionNotice->setVisible(!words.isEmpty());
	}

	void tour_side_chat::present_native_history(const application::native_agent_history_projection& aView)
	{
		if (aView.ReadOnly) {
			auto* dialog = new QDialog{ this };
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			dialog->setWindowTitle(tr("Retained tour chat"));
			auto* column = new QVBoxLayout{ dialog };
			auto* text   = new QPlainTextEdit{ dialog };
			text->setReadOnly(true);
			auto body = tr("Retained history. This chat is detached from the current tour part. Viewing it sends no question.\n\n");
			for (const auto& turn : aView.Turns) {
				body += QString::fromStdString(turn.Asked) + QStringLiteral("\n\n") + QString::fromStdString(turn.Answered) + QStringLiteral("\n\n");
			}
			text->setPlainText(body);
			column->addWidget(text);
			dialog->resize(600, 480);
			dialog->show();
			return;
		}
		HistoryLoading = aView.Loading;
		HistoryTool->setProperty("loading", aView.Loading);
		if (!aView.Loading) {
			// A notice only when the session does not go on in the ordinary
			// way. [[BPR-2059]]: a carried-over conversation's counts are a
			// diagnostic, kept on the line, and said in its tooltip only when
			// something was left out.
			CarriedOver    = aView.Replay;
			CannotContinue = !aView.Replay && !aView.NativeSupported;
			SessionNotice->setProperty("replayTurnsOmitted", QVariant::fromValue<qulonglong>(aView.ReplayTurnsOmitted));
			SessionNotice->setProperty("replayTextCutBytes", QVariant::fromValue<qulonglong>(aView.ReplayTextCutBytes));
			auto tip = aView.Replay ? chat_history_menu::carry_over_tip(aView.ReplayTurnsOmitted, aView.ReplayTextCutBytes > 0U) : QString{ };
			if (aView.Replay && !aView.ReplaySource.empty()) {
				tip += tr(" Source history: %1").arg(QString::fromStdString(aView.ReplaySource));
			}
			SessionNotice->setToolTip(tip);
			present_session_notice();
		}
		if (!aView.Loading) {
			const auto open     = std::ranges::find(aView.Entries, aView.Selected, &application::native_agent_history_entry::Id);
			const auto found    = !aView.Selected.empty() && open != aView.Entries.end();
			OpenSessionName     = found ? open->Name : std::string{ };
			OpenSessionRevision = found && open->Binding.HeadOid != decltype(open->Binding.HeadOid){ } ? std::string{ core::canonical_hex(open->Binding.HeadOid).view() } : std::string{ };
		}
		// [[BPR-2377]], RFC-BPR-0130. The sessions, in the one History menu.
		HistoryMenu->present(aView);
		refresh_actions();
	}

	void tour_side_chat::copy_chat() const
	{
		if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
			clipboard->setText(chat_text());
		}
	}

	void tour_side_chat::set_sessions_offered(const bool aOffered)
	{
		if (SessionsOffered != aOffered) {
			SessionsOffered = aOffered;
			refresh_header();
		}
	}

	auto tour_side_chat::new_session_action() const noexcept -> QAction* { return HistoryMenu->new_session_action(); }

	auto tour_side_chat::session_rows() -> agent_session_menu& { return *HistoryMenu->rows(); }

	void tour_side_chat::present_session_edit(const QString& aStatus)
	{
		HistoryMenu->present_status(aStatus);
	}

	void tour_side_chat::present_revision_continuation(std::optional<core::agent_continuation_offer> aOffer, const core::git_commit_oid aObservedHead, const bool aContinued)
	{
		const auto stale       = aOffer && aOffer->Required;
		RevisionStale          = stale;
		RevisionChoiceRequired = stale && !aContinued;
		ContinueOld->setVisible(RevisionChoiceRequired);
		RevisionStatus->setVisible(stale);
		if (stale) {
			RevisionStatus->setText((aContinued ? tr("Continuing saved revision %1. Current head: %2.") : tr("Head changed. This chat reads %1; current head: %2. Choose New session in History or Continue old revision."))
										.arg(QString::fromStdString(std::string{ core::canonical_hex(aOffer->ContinueAgainst.HeadOid).view() }).left(12), QString::fromStdString(std::string{ core::canonical_hex(aObservedHead).view() }).left(12)));
		}
		refresh_actions();
	}

	void tour_side_chat::set_draft(std::shared_ptr<const core::review_tour> aDraft)
	{
		if (Draft == aDraft) {
			return;
		}
		present(Session, Proposal, std::move(aDraft));
	}

	void tour_side_chat::set_running(const bool aAny, const bool aDrafting)
	{
		const auto wasDrafting = Drafting;
		Running                = aAny;
		Drafting               = aAny && aDrafting;
		// [[BPR-1941]]. The run's time counts from when it was asked for, as
		// the Agent tab's does; it ends with the run.
		if (aAny && !LiveClock.isValid()) {
			LiveClock.start();
		} else if (!aAny) {
			LiveClock.invalidate();
			Live.reset();
		}
		if (wasDrafting != Drafting) {
			present_thread();
			return;
		}
		refresh_actions();
	}

	void tour_side_chat::present_run_status(std::optional<agent_run_status> aStatus)
	{
		if (aStatus && !LiveClock.isValid()) {
			LiveClock.start();
		}
		Live = std::move(aStatus);
		if (!Live) {
			return;
		}
		if (const auto running = core::running_tour_run(Session)) {
			RunAgents[running->value()] = Live->Agent;
		}
		Transcript->present_live(*Live, LiveClock);
		if (DraftLine->live()) {
			DraftLine->present(*Live, LiveClock);
		}
	}

	void tour_side_chat::record_run_activity(const core::tour_agent_run_id aRun, core::agent_turn_activity aActivity, const qint64 aSeconds)
	{
		RunActivity[aRun.value()] = { std::move(aActivity), aSeconds };
	}

	auto tour_side_chat::activity_view(const std::size_t aTurn) const -> agent_activity_view*
	{
		return Transcript->activity(aTurn);
	}

	auto tour_side_chat::live_status() const -> agent_run_status
	{
		if (Live) {
			return *Live;
		}
		auto status  = agent_run_status{ };
		status.Agent = tr("The tour agent");
		status.Stage = agent_run_stage::thinking;
		return status;
	}

	auto tour_side_chat::agent_of(const core::tour_agent_run_id aRun) const -> QString
	{
		const auto found = RunAgents.find(aRun.value());
		return found == RunAgents.end() || found->second.isEmpty() ? tr("The tour agent") : found->second;
	}

	/// [[BPR-1930]], owner: "particularly during the initial agent draft".
	/// While Draft the tour runs, the tour's own chat still takes feedback.
	auto tour_side_chat::takes_feedback() const -> bool
	{
		const auto thread = current();
		return Drafting && !Mine && thread && thread->Kind == core::tour_thread_kind::tour;
	}

	void tour_side_chat::set_available(const bool aReady, const QString& aWhy)
	{
		Ready = aReady;
		Why   = aWhy;
		refresh_actions();
	}

	void tour_side_chat::set_host_idle(const bool aIdle)
	{
		if (HostIdle == aIdle) {
			return;
		}
		HostIdle = aIdle;
		refresh_actions();
	}

	void tour_side_chat::set_turn_name_source(turn_name_source aSource) { TurnNames = std::move(aSource); }

	void tour_side_chat::set_prompt_source(prompt_source aSource)
	{
		PromptSource = std::move(aSource);
		present_thread();
	}

	auto tour_side_chat::prompt_block() const -> core::agent_suggestion_block
	{
		// A prompt is a question sent by a press: while the chat asks which
		// revision to go on with, Send is refused and so is a prompt.
		return core::decide_agent_suggestion_use({
			.Sendable       = Ready && !HistoryLoading && current() && (!Draft || core::active_tour_thread(*Draft, *current())) && !Mine && (!Running || takes_feedback()),
			.RevisionChoice = RevisionChoiceRequired,
		});
	}

	auto tour_side_chat::prompts_usable() const -> bool { return prompt_block() == core::agent_suggestion_block::none; }

	void tour_side_chat::present_carried_answer(const tour_carried_answer aCarried)
	{
		if (Carried == aCarried) {
			return;
		}
		Carried = aCarried;
		refresh_actions();
	}

	void tour_side_chat::choose_prompt(const QString& aPrompt, const bool aEditFirst)
	{
		const auto thread = current();
		if (!thread || !PromptSource) {
			return;
		}
		// RFC-BPR-0125. A press expands against the facts at the press; one
		// that no longer applies does nothing.
		const auto offers = prompt_offers();
		auto       all    = offers.Chips;
		all.insert(all.end(), offers.More.begin(), offers.More.end());
		const auto key   = aPrompt.toStdString();
		auto       found = std::ranges::find(all, key, &core::prompt_offer::Id);
		if (found == all.end()) {
			found = std::ranges::find(all, key, &core::prompt_offer::Label);
		}
		if (found == all.end()) {
			return;
		}
		const auto offer = *found;
		const auto text  = QString::fromStdString(offer.Expansion.Text);
		// [[BPR-1934]] v002. Explain the selected block: asked in the
		// selected part's chat, opened if it is not the one shown, with
		// exactly the selected text quoted.
		if (offer.Subject == core::prompt_subject::selected_text && Selection) {
			const auto chosen = *Selection;
			if (chosen.Thread != *thread) {
				open(chosen.Thread, chosen.Passage, false);
			} else {
				Passages[static_cast<std::size_t>(Current)] = chosen.Passage;
				show_passage();
			}
		}
		const auto draft = Composer->toPlainText();
		// A prompt the reviewer finishes ("Focus on …") is always put in the
		// composer, with its words there to be completed.
		const auto press = core::decide_agent_suggestion_press({
			.EditFirst = aEditFirst || offer.finish(),
			.DraftHeld = !draft.trimmed().isEmpty(),
			.Usable    = prompts_usable(),
		});
		const auto toEnd = [this] {
			auto cursor = Composer->textCursor();
			cursor.movePosition(QTextCursor::End);
			Composer->setTextCursor(cursor);
		};
		const auto caret = static_cast<int>(QString::fromUtf8(offer.Expansion.Text.data(), static_cast<qsizetype>(offer.Expansion.Cursor.value_or(offer.Expansion.Text.size()))).size());
		switch (press) {
		case core::agent_suggestion_press::ignored: return;
		case core::agent_suggestion_press::send:
		case core::agent_suggestion_press::send_keeping_draft:
			// AC09. Sent as offered: skills only from the template's own text.
			SendingSkillText = core::prompt_skill_text(offer.Expansion);
			SendingName      = QString::fromStdString(offer.Label) != text.trimmed() ? QString::fromStdString(offer.Label) : QString{ };
			send(text, false);
			SendingSkillText.reset();
			SendingName.clear();
			if (press == core::agent_suggestion_press::send_keeping_draft) {
				// Sent alone, as if typed and sent; the reviewer's words stay.
				Composer->setPlainText(draft);
				toEnd();
			}
			return;
		case core::agent_suggestion_press::edit: {
			Composer->setPlainText(text);
			auto cursor = Composer->textCursor();
			cursor.setPosition(caret);
			Composer->setTextCursor(cursor);
			break;
		}
		case core::agent_suggestion_press::edit_after_draft: {
			toEnd();
			const auto lead = draft.endsWith(QLatin1Char('\n')) ? QString{ } : QStringLiteral("\n");
			const auto from = Composer->textCursor().position() + static_cast<int>(lead.size());
			Composer->textCursor().insertText(lead + text);
			auto cursor = Composer->textCursor();
			cursor.setPosition(from + caret);
			Composer->setTextCursor(cursor);
			break;
		}
		}
		Composer->setFocus(Qt::OtherFocusReason);
	}

	void tour_side_chat::set_code_style(const QFont& aFont, const qreal aTabStopDistance, std::optional<markdown_code_theme> aTheme, std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		CodeFont     = aFont;
		TabStop      = aTabStopDistance;
		CodeTheme    = std::move(aTheme);
		CodeDispatch = std::move(aDispatch);
		for (auto* const each : Transcript->readers()) {
			each->set_code_font(CodeFont, TabStop);
			if (CodeTheme) {
				each->set_code_theme(*CodeTheme);
			}
			if (CodeDispatch) {
				each->set_code_syntax_dispatch(CodeDispatch);
				each->request_code_syntax();
			}
		}
		fit();
	}

	void tour_side_chat::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		for (auto* const each : Transcript->readers()) {
			each->apply_code_syntax(aSpans);
		}
	}

	void tour_side_chat::restyle(const QPalette& aGround)
	{
		// [[BPR-2000]]. Send and Resend start runs and wear the
		// accent as run controls; titles and the suggestions' ground keep it.
		for (const auto& widget : Accented) {
			if (widget) {
				if (qobject_cast<QAbstractButton*>(widget.data()) != nullptr) {
					wear_agent_accent(*widget, aGround);
				} else {
					apply_agent_accent(*widget, aGround);
				}
			}
		}
		if (Mine) {
			wear_ordinary_roles(*Send);
		} else {
			wear_agent_accent(*Send, aGround);
		}
		Header->repaint_glyphs();
		ClearPassage->setIcon(drawer_icon(drawer_glyph::close, palette()));
		// [[BPR-2000]]. Send starts a run and wears the accent; while this
		// chat's run is going it is Stop, in the ordinary roles.
		if (Mine) {
			wear_ordinary_roles(*Send);
		} else {
			wear_agent_accent(*Send, palette());
		}
		Send->setIcon(drawer_icon(Mine ? drawer_glyph::close : drawer_glyph::send, Send->palette()));
		update();
	}

	void tour_side_chat::present_skills(std::vector<core::skill_entry> aSkills)
	{
		if (SkillCompletion != nullptr) {
			SkillCompletion->present(std::move(aSkills));
		}
	}

	// The runs are the transcript's first turns; the whole tour's waiting
	// feedback, when there is any, is its last.
	auto tour_side_chat::turn_count() const -> std::size_t { return Shown; }
	auto tour_side_chat::bubble(const std::size_t aTurn) const -> QWidget* { return aTurn < Shown ? Transcript->bubble(aTurn) : nullptr; }
	auto tour_side_chat::reply(const std::size_t aTurn) const -> markdown_body_view* { return aTurn < Shown ? Transcript->reply(aTurn) : nullptr; }
	auto tour_side_chat::status_line(const std::size_t aTurn) const -> agent_run_status_line* { return aTurn < Shown ? Transcript->ending(aTurn) : nullptr; }
	auto tour_side_chat::copy_reply_button(const std::size_t aTurn) const -> QToolButton* { return aTurn < Shown ? Transcript->copy_reply(aTurn) : nullptr; }
	auto tour_side_chat::resend_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Shown ? Transcript->resend(aTurn) : nullptr; }
	auto tour_side_chat::delete_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Shown ? Transcript->remove(aTurn) : nullptr; }
	auto tour_side_chat::waiting_feedback() const -> QWidget* { return Transcript->turn_count() > Shown ? Transcript->bubble(Shown) : nullptr; }

	auto tour_side_chat::accept_button(const std::size_t aIndex) const -> QPushButton*
	{
		const auto found = Accepts.find(aIndex);
		return found == Accepts.end() ? nullptr : found->second;
	}

	auto tour_side_chat::proposal_reader(const std::size_t aIndex) const -> markdown_body_view*
	{
		const auto found = ProposalReaders.find(aIndex);
		return found == ProposalReaders.end() ? nullptr : found->second;
	}

	auto tour_side_chat::reply_text(const core::tour_agent_run& aRun) -> QString { return QString::fromStdString(aRun.Reply); }

	auto tour_side_chat::chat_text() const -> QString
	{
		const auto thread = current();
		if (!thread) {
			return { };
		}
		auto lines = QStringList{ tr("## Side chat: %1").arg(name_of(*thread)), QString{ } };
		for (const auto* run : core::thread_runs(Session, *thread)) {
			if (run->Passage) {
				lines << QStringLiteral("> ") + passage_heading(*run->Passage) + QLatin1Char(':');
				for (const auto& line : QString::fromStdString(run->Passage->Text).split(QLatin1Char('\n'))) {
					lines << QStringLiteral("> ") + line;
				}
				lines << QString{ };
			}
			lines << tr("**You:** %1").arg(QString::fromStdString(run->Question)) << QString{ };
			for (const auto& message : run->Steered) {
				lines << tr("**You** (%1): %2").arg(agent_steered_state_words(message), QString::fromStdString(message.Text)) << QString{ };
			}
			if (run->State == core::tour_agent_run_state::answered) {
				lines << (run->Seconds ? tr("**%1** (answered in %2):").arg(agent_of(run->Id), agent_elapsed_words(*run->Seconds)) : tr("**%1:**").arg(agent_of(run->Id))) << QString{ } << reply_text(*run) << QString{ };
			} else {
				const auto ended = std::ranges::find(run->Entries, core::tour_transcript_kind::ended, &core::tour_transcript_entry::Kind);
				lines << tr("**%1** did not answer: %2").arg(agent_of(run->Id), ended == run->Entries.end() ? QString{ } : QString::fromStdString(ended->Text).section(QLatin1Char('\n'), 0, 0)) << QString{ };
			}
		}
		return lines.join(QLatin1Char('\n'));
	}

	void tour_side_chat::paintEvent(QPaintEvent* /*aEvent*/)
	{
		if (!isWindow()) {
			side_chat::paint_seam(*this);
		}
	}

	bool tour_side_chat::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		if (aEvent->type() == QEvent::Resize && aWatched == Transcript->viewport()) {
			QTimer::singleShot(0, this, [this] { fit(); });
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	auto tour_side_chat::name_of(const core::tour_thread& aThread) const -> QString
	{
		switch (aThread.Kind) {
		case core::tour_thread_kind::tour: return tr("Tour");
		case core::tour_thread_kind::stop: {
			if (Draft) {
				const auto found = std::ranges::find(Draft->Stops, aThread.Stop, &core::tour_stop::Id);
				if (found != Draft->Stops.end()) {
					return tr("Stop %1").arg(static_cast<std::size_t>(found - Draft->Stops.begin()) + 1U);
				}
			}
			return tr("Removed stop %1").arg(aThread.Stop.value());
		}
		}
		return { };
	}

	void tour_side_chat::refresh_header()
	{
		// What the row says is this chat's; what it holds is core's.
		const auto thread  = current();
		auto       facts   = core::chat_header_facts{ };
		facts.Kind         = !SessionsOffered ? core::chat_kind::local_draft_discussion : thread && thread->Kind == core::tour_thread_kind::stop ? core::chat_kind::tour_part
																																				 : core::chat_kind::whole_tour;
		facts.SessionName  = OpenSessionName;
		facts.RevisionRead = OpenSessionRevision;
		if (thread) {
			// A chat that keeps no sessions has no row of History to say what
			// it has cost.
			facts.Cost = SessionsOffered ? std::nullopt : core::tour_thread_cost(Session, *thread);
		}
		if (thread && thread->Kind == core::tour_thread_kind::tour) {
			facts.Subject = tr("The whole tour").toStdString();
		} else if (thread) {
			const core::tour_stop* stop = nullptr;
			if (Draft) {
				if (const auto found = std::ranges::find(Draft->Stops, thread->Stop, &core::tour_stop::Id); found != Draft->Stops.end()) {
					stop = &*found;
				}
			}
			auto place = QString{ };
			if (stop != nullptr) {
				if (const auto& source = stop->Mapped ? stop->Mapped : stop->Original; source) {
					place = source->First.Line == source->Last.Line ? tr("%1, line %2").arg(QString::fromStdString(source->Path)).arg(source->First.Line + 1U)
																	: tr("%1, lines %2 to %3").arg(QString::fromStdString(source->Path)).arg(source->First.Line + 1U).arg(source->Last.Line + 1U);
				} else if (stop->Destination) {
					// A stop the agent proposed, not yet confirmed in the code.
					place = tr("%1, lines %2 to %3").arg(QString::fromStdString(stop->Destination->Path)).arg(stop->Destination->FirstLine + 1U).arg(stop->Destination->LastLine + 1U);
				}
			}
			facts.Subject = (stop != nullptr ? tr("%1, %2").arg(name_of(*thread), QString::fromStdString(stop->Title)) : tr("A stop")).toStdString();
			facts.Place   = place.toStdString();
			facts.CanShow = true;
		}
		Header->present(core::plan_chat_header(facts));
	}

	void tour_side_chat::show_passage()
	{
		const auto held = passage();
		PassageRow->setVisible(held.has_value());
		if (!held) {
			PassageLine->clear();
			return;
		}
		// [[BPR-1995]]. The chip names what is attached; its tooltip has it all.
		const auto said = held->Field == core::tour_passage_field::stop_code ? passage_heading(*held)
																			 : QString{ open_quote } + QString::fromStdString(held->Text).simplified() + QString{ close_quote };
		PassageLine->setText(PassageLine->fontMetrics().elidedText(said, Qt::ElideRight, std::max(120, Field->width() - 60)));
		PassageLine->setToolTip(tr("Asking about %1").arg(QString::fromStdString(held->Text)));
		PassageLine->setAccessibleName(tr("Asking about %1").arg(said));
	}

	void tour_side_chat::set_check_dispatch(check_dispatch aDispatch)
	{
		Checks = std::move(aDispatch);
		refresh_actions();
	}

	void tour_side_chat::set_check_state(const bool aScratch, const bool aBuilds, const bool aAvailable, const QString& aWhy, const bool aRebuild)
	{
		const auto changed = aRebuild || aScratch != CheckScratch || aBuilds != CheckBuilds;
		CheckScratch       = aScratch;
		CheckBuilds        = aBuilds;
		CheckAvailable     = aAvailable;
		CheckWhy           = aWhy;
		if (aRebuild) {
			Transcript->remake_attachments("check");
		}
		if (changed) {
			present_thread();
		}
		refresh_actions();
	}

	void tour_side_chat::refresh_actions()
	{
		const auto thread = current();
		const auto blank  = Composer->toPlainText().trimmed().isEmpty();
		const auto active = thread && (!Draft || core::active_tour_thread(*Draft, *thread));
		HistoryMenu->refresh();
		if (thread && QueueShown != core::tour_part_name(*thread)) {
			show_queue();
		}
		// RFC-BPR-0126 ([[BPR-2154]]). While the tour agent runs, or this
		// part's queue holds messages, words typed here are a follow-up:
		// Send is Queue (or Steer, by the setting), and Stop stands beside it.
		// The one rule (`core::decide_chat_send`), the composer draws it.
		const auto controls  = core::decide_chat_send({ .Running = Mine, .FollowsUp = follow_up().has_value(), .Typed = !blank, .SteerFirst = FollowUpSteer });
		const auto following = controls.Send == core::chat_send_face::queue || controls.Send == core::chat_send_face::steer;
		const auto stopping  = controls.Send == core::chat_send_face::stop;
		const auto other     = QStringLiteral("%1Return").arg(QKeySequence{ Qt::ControlModifier }.toString(QKeySequence::NativeText));
		Field->present(controls);
		Send->setToolTip(!Ready          ? Why
							 : stopping  ? tr("Stop the tour agent; nothing in the tour changes.")
							 : following ? (FollowUpSteer ? tr("Steer (Return) · Queue (%1)").arg(other) : tr("Queue (Return) · Steer (%1)").arg(other))
										 : tr("Send (Return). Shift+Return starts a new line."));
		ContinueOld->setEnabled(active && HostIdle && !Running && !HistoryLoading && Ready);
		Send->setEnabled(active && !HistoryLoading && Ready && thread.has_value() && (stopping || (following && !RevisionChoiceRequired) || ((HostIdle || takes_feedback()) && !RevisionChoiceRequired && (!Running || takes_feedback()) && !blank)));
		const auto part = part_words();
		Transcript->set_resend_enabled(HostIdle && !HistoryLoading && Ready && !Running);
		const auto checks = Checks.Plan && thread && thread->Kind == core::tour_thread_kind::tour;
		PlanCheck->setVisible(checks);
		PlanCheck->setEnabled(checks && CheckAvailable && !Running && HostIdle);
		PlanCheck->setToolTip(!CheckAvailable ? CheckWhy
								  : blank     ? tr("The agent proposes the checks that matter most for this change. Nothing runs until you press Run.")
											  : tr("The agent plans a check of what you wrote. Nothing runs until you press Run."));
		if (checks) {
			wear_agent_accent(*PlanCheck, palette());
		}
		// Said only where a question can be asked: the line is about the
		// next one.
		const auto carries = active && Carried != tour_carried_answer::none;
		CarriedAnswer->setText(!carries ? QString{ } : Carried == tour_carried_answer::draft ? tr("Your next question starts from the draft's answer.")
																							 : tr("Your next question starts from the Enhance suggestion."));
		CarriedAnswer->setToolTip(!carries ? QString{ } : Carried == tour_carried_answer::draft ? tr("Draft Tour ran outside this chat. What it proposed goes to the agent with your next question here, so the chat goes on from it.")
																								: tr("Enhance ran outside this chat. Its suggestion goes to the agent with your next question here, so the chat goes on from it."));
		CarriedAnswer->setVisible(carries);
		Notice->setVisible(!SessionNotice->isHidden() || !RevisionStatus->isHidden() || !CarriedAnswer->isHidden());
		refresh_unchanged_line();
		// [[BPR-1930]]. The tour's chat is for feedback on its shape too.
		Composer->setPlaceholderText(!thread                                            ? QString{ }
										 : takes_feedback()                             ? tr("Feedback for the draft. It goes to the agent when the draft is ready")
										 : thread->Kind == core::tour_thread_kind::tour ? tr("Ask about the tour, or say how to change it: fewer stops, merge two, focus on a part")
																						: tr("Ask about %1").arg(part));
		Composer->setEnabled(thread.has_value());
		refresh_header();
		refresh_prompts();
	}

	void tour_side_chat::offer_selection(const core::tour_thread& aThread, std::optional<core::tour_passage> aSelected)
	{
		if (aSelected && !aSelected->Text.empty()) {
			Selection.emplace();
			Selection->Thread  = aThread;
			Selection->Passage = std::move(*aSelected);
		} else if (Selection && Selection->Thread.Kind == aThread.Kind) {
			// A stop's text is one editor for every stop: going to another
			// stop ends what was selected in the last.
			Selection.reset();
		}
		refresh_prompts();
	}

	auto tour_side_chat::prompt_part(const core::tour_thread& aThread) const -> core::prompt_tour_part
	{
		auto part = core::prompt_tour_part{ };
		switch (aThread.Kind) {
		case core::tour_thread_kind::tour: part.Place = core::prompt_place::tour_whole; break;
		case core::tour_thread_kind::stop:
			part.Place = core::prompt_place::tour_stop;
			part.Part  = name_of(aThread).toStdString();
			if (Draft) {
				const auto found = std::ranges::find(Draft->Stops, aThread.Stop, &core::tour_stop::Id);
				if (found != Draft->Stops.end()) {
					part.StopNumber = static_cast<std::uint32_t>(found - Draft->Stops.begin()) + 1U;
					part.StopTitle  = found->Title;
				}
			}
			break;
		}
		return part;
	}

	auto tour_side_chat::prompt_view() const -> std::optional<core::prompt_chat_view>
	{
		const auto thread = current();
		if (!thread) {
			return std::nullopt;
		}
		// [[BPR-1934]] v002. Explain the selected block leads while text is
		// selected in a part, whatever the chat holds already; for another
		// part than this chat's, it names the part. The rest are for an empty
		// chat, and go with its first turn.
		auto view      = core::prompt_chat_view{ };
		view.TourPart  = prompt_part(*thread);
		view.Place     = view.TourPart->Place;
		view.ChatEmpty = core::thread_runs(Session, *thread).empty() && !(thread->Kind == core::tour_thread_kind::tour && Session.Waiting);
		if (Selection) {
			view.Selection = prompt_part(Selection->Thread);
		}
		return view;
	}

	auto tour_side_chat::prompt_offers() const -> core::chat_prompts
	{
		const auto view = prompt_view();
		if (!view || !PromptSource) {
			return { };
		}
		return PromptSource(*view);
	}

	void tour_side_chat::refresh_prompts()
	{
		if (Suggestions == nullptr) {
			return;
		}
		// [[BPR-1934]]. The suggested prompts, only once the agent is set up
		// ([[BPR-1881]]); usable whenever a question could be sent.
		const auto offers = prompt_offers();
		auto       chips  = std::vector<agent_suggestion>{ };
		auto       more   = std::vector<agent_suggestion>{ };
		const auto shown  = [](const core::prompt_offer& aOffer) {
			return agent_suggestion{ .Key = QString::fromStdString(aOffer.Id), .Label = QString::fromStdString(aOffer.Label), .Expansion = QString::fromStdString(aOffer.Expansion.Text), .Finish = aOffer.finish() };
		};
		std::ranges::transform(offers.Chips, std::back_inserter(chips), shown);
		std::ranges::transform(offers.More, std::back_inserter(more), shown);
		const auto thread = current();
		const auto any    = !chips.empty();
		Suggestions->present(std::move(chips), std::move(more));
		Suggestions->lead()->setVisible(thread && core::thread_runs(Session, *thread).empty());
		Suggestions->setVisible(Ready && any);
		const auto block = prompt_block();
		Suggestions->set_usable(block == core::agent_suggestion_block::none, block == core::agent_suggestion_block::revision_choice ? tr("Choose New session in History or Continue old revision first.") : QString{ });
	}

	void tour_side_chat::send(const QString& aQuestion, const bool aUpdate, std::optional<core::tour_passage> aUpdatePassage)
	{
		const auto thread    = current();
		const auto following = aUpdate || aQuestion.isEmpty() ? std::nullopt : follow_up();
		if (!thread || aQuestion.isEmpty() || (!following && ((!HostIdle && !(takes_feedback() && !aUpdate)) || (Running && !(takes_feedback() && !aUpdate)))) || (following && RevisionChoiceRequired) || HistoryLoading
			|| (Draft && !core::active_tour_thread(*Draft, *thread)) || !Ready || !AskDispatch) {
			return;
		}
		auto ask = tour_ask{ .Thread = *thread, .Passage = aUpdate ? std::move(aUpdatePassage) : passage(), .Question = aQuestion, .Update = aUpdate, .SkillText = SendingSkillText, .Name = SendingName, .Steer = following };
		if (!SendingName.isEmpty()) {
			SentNames[aQuestion.toStdString()] = SendingName;
		}
		AskDispatch(ask);
		if (!aUpdate) {
			Composer->clear();
			Passages.erase(static_cast<std::size_t>(Current));
		}
		show_passage();
	}

	/// RFC-BPR-0126 ([[BPR-2154]]). The core decides queue against send, as
	/// for the Agent tab; the whole tour's feedback during its first draft
	/// keeps its own path (BPR-2181 AC11).
	auto tour_side_chat::follow_up() const -> std::optional<bool>
	{
		const auto thread = current();
		if (!thread || !QueueDispatch || takes_feedback()) {
			return std::nullopt;
		}
		const auto found = Queues.find(core::tour_part_name(*thread));
		const auto holds = found != Queues.end() && !found->second.Rows.empty();
		const auto said  = core::decide_follow_up({ .TurnShown = Running || !HostIdle, .QueueHolds = holds, .SteerByDefault = FollowUpSteer, .Opposite = Opposite });
		if (said == core::follow_up::send) {
			return std::nullopt;
		}
		return said == core::follow_up::steer;
	}

	void tour_side_chat::set_follow_up_steer(const bool aSteer)
	{
		FollowUpSteer = aSteer;
		refresh_actions();
	}

	void tour_side_chat::present_queue(const core::tour_thread& aThread, std::vector<agent_pending_row> aRows, const QString& aState, const bool aPaused)
	{
		auto& queue = Queues[core::tour_part_name(aThread)];
		if (queue.State == aState && queue.Paused == aPaused && std::ranges::equal(queue.Rows, aRows, [](const agent_pending_row& aOne, const agent_pending_row& aOther) {
				return aOne.Id == aOther.Id && aOne.Label == aOther.Label && aOne.Text == aOther.Text && aOne.Held == aOther.Held && aOne.Sending == aOther.Sending && aOne.Uncertain == aOther.Uncertain && aOne.Sent == aOther.Sent;
			})) {
			return;
		}
		queue = part_queue{ .Rows = std::move(aRows), .State = aState, .Paused = aPaused };
		if (const auto thread = current(); thread && *thread == aThread) {
			show_queue();
		}
		refresh_actions();
	}

	void tour_side_chat::set_queue_dispatch(std::function<agent_pending_dispatch(const core::tour_thread&)> aDispatch)
	{
		QueueDispatch = std::move(aDispatch);
		QueueShown.reset();
		Queues.clear();
		refresh_actions();
	}

	void tour_side_chat::show_queue()
	{
		const auto thread = current();
		QueueShown        = thread ? std::optional{ core::tour_part_name(*thread) } : std::nullopt;
		if (!thread || !QueueDispatch) {
			PendingList->present({ }, { }, false);
			return;
		}
		PendingList->set_dispatch(QueueDispatch(*thread));
		const auto found = Queues.find(*QueueShown);
		if (found == Queues.end()) {
			PendingList->present({ }, { }, false);
			return;
		}
		PendingList->present(found->second.Rows, found->second.State, found->second.Paused);
	}

	auto tour_side_chat::part_words() const -> QString
	{
		const auto thread = current();
		return !thread                                     ? QString{ }
			: thread->Kind == core::tour_thread_kind::stop ? tr("this stop")
														   : tr("the tour");
	}

	auto tour_side_chat::update_question() const -> QString
	{
		const auto thread = current();
		if (!thread) {
			return { };
		}
		return tr("Update %1 from our chat.").arg(thread->Kind == core::tour_thread_kind::stop ? tr("this stop's explanation") : part_words());
	}

	auto tour_side_chat::can_update() const -> bool
	{
		const auto thread   = current();
		const auto answered = thread && std::ranges::any_of(core::thread_runs(Session, *thread), [](const core::tour_agent_run* aRun) { return aRun->State == core::tour_agent_run_state::answered; });
		return thread && answered && (!Draft || core::active_tour_thread(*Draft, *thread)) && HostIdle && !RevisionChoiceRequired && !HistoryLoading && Ready && !Running && AskDispatch;
	}

	auto tour_side_chat::request_update() -> bool
	{
		if (!can_update()) {
			return false;
		}
		send(update_question(), true);
		return true;
	}

	auto tour_side_chat::enhance(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage) -> bool
	{
		// [[BPR-2138]], RFC-BPR-0101 (owner, 2026-11-12). Enhance is a fresh
		// run: the side chat is neither opened nor sent a turn, and no
		// conversation's history is needed, so neither its revision choice
		// nor its loading history holds it back. Its result is a proposal on
		// the part, accepted or dismissed there.
		if (!(Draft == nullptr || core::active_tour_thread(*Draft, aThread)) || !HostIdle || !Ready || Running || !AskDispatch) {
			return false;
		}
		const auto selection = aPassage && !aPassage->Text.empty();
		const auto part      = aThread.Kind == core::tour_thread_kind::stop ? tr("this stop") : tr("the tour");
		const auto question  = selection ? tr("Enhance the selected text of %1.").arg(part)
										 : tr("Enhance the writing of %1.").arg(aThread.Kind == core::tour_thread_kind::stop ? tr("this stop's explanation") : part);
		AskDispatch(tour_ask{ .Thread = aThread, .Passage = selection ? std::move(aPassage) : std::nullopt, .Question = question, .Update = true, .Fresh = true });
		return true;
	}

	/// [[BPR-1995]], RFC-BPR-0100 AC02. The link under the newest answer that
	/// changed nothing, in every thread: a link while the update can be
	/// asked for, the same words without one while it cannot.
	void tour_side_chat::refresh_unchanged_line()
	{
		if (UnchangedLine == nullptr) {
			return;
		}
		const auto part   = part_words();
		const auto usable = can_update();
		const auto action = tr("Update %1 from this chat").arg(part);
		UnchangedLine->setText(tr("Nothing in %1 changed. %2").arg(part, usable ? QStringLiteral("<a href=\"#update\">%1</a>").arg(action.toHtmlEscaped()) : action.toHtmlEscaped()));
		UnchangedLine->setToolTip(!Ready       ? Why
									  : usable ? tr("Asks the agent to change %1 as this chat agreed. Nothing changes until you apply what it proposes.").arg(part)
											   : tr("Available when the agent is idle."));
	}

	auto tour_side_chat::revision_words(const core::tour_agent_run_id aRun) const -> QStringList
	{
		const auto number = [this](const core::tour_stop_id aStop) -> QString {
			if (Draft) {
				if (const auto found = std::ranges::find(Draft->Stops, aStop, &core::tour_stop::Id); found != Draft->Stops.end()) {
					return QString::number(static_cast<std::size_t>(found - Draft->Stops.begin()) + 1U);
				}
			}
			return tr("a removed stop");
		};
		const auto quoted = [](const std::string& aText) { return open_quote + QString::fromStdString(aText) + close_quote; };
		auto       lines  = QStringList{ };
		if (!Proposal) {
			return lines;
		}
		for (const auto& item : Proposal->Items) {
			if (item.Run != aRun) {
				continue;
			}
			auto line = QString{ };
			switch (item.Part) {
			case core::tour_proposal_part::stop_text:   line = tr("Rewrite stop %1, %2").arg(number(item.StopId), quoted(item.Stop.Title)); break;
			case core::tour_proposal_part::stop:        line = tr("New stop %1 (%2, lines %3 to %4)").arg(quoted(item.Stop.Title), QString::fromStdString(item.Stop.Path)).arg(item.Stop.FirstLine).arg(item.Stop.LastLine); break;
			case core::tour_proposal_part::remove_stop: line = tr("Remove stop %1").arg(number(item.StopId)); break;
			case core::tour_proposal_part::merge_stops: {
				auto numbers = QStringList{ };
				for (const auto stop : item.Stops) {
					numbers << number(stop);
				}
				const auto last = numbers.isEmpty() ? QString{ } : numbers.takeLast();
				line            = tr("Merge stops %1 and %2 into %3").arg(numbers.join(QStringLiteral(", ")), last, quoted(item.Stop.Title));
				break;
			}
			case core::tour_proposal_part::split_stop: line = tr("Split stop %1 into %2").arg(number(item.StopId)).arg(item.Pieces.size()); break;
			case core::tour_proposal_part::reorder:    {
				auto numbers = QStringList{ };
				for (const auto stop : item.Stops) {
					numbers << number(stop);
				}
				line = tr("Move the stops into the order %1").arg(numbers.join(QStringLiteral(", ")));
				break;
			}
			}
			if (item.Decision == core::tour_proposal_decision::accepted) {
				line = tr("%1: accepted").arg(line);
			} else if (item.Decision == core::tour_proposal_decision::discarded) {
				line = item.Refusal.empty() ? tr("%1: discarded").arg(line) : tr("%1: refused, its target changed").arg(line);
			}
			lines << line;
		}
		return lines;
	}

	namespace {
		/// \brief The file a stop is on: its source, else its destination.
		[[nodiscard]] auto stop_path(const core::tour_stop& aStop) -> std::string
		{
			if (const auto& source = aStop.Mapped ? aStop.Mapped : aStop.Original; source) {
				return source->Path;
			}
			return aStop.Destination ? aStop.Destination->Path : std::string{ };
		}
	} // namespace

	auto tour_side_chat::code_context() const -> std::string
	{
		const auto thread = current();
		if (!thread || !Draft || thread->Kind != core::tour_thread_kind::stop) {
			return { };
		}
		const auto found = std::ranges::find(Draft->Stops, thread->Stop, &core::tour_stop::Id);
		return found != Draft->Stops.end() ? stop_path(*found) : std::string{ };
	}

	auto tour_side_chat::inline_selector() const -> std::string
	{
		const auto thread = current();
		if (!thread || !Draft) {
			return { };
		}
		const auto pathOf = stop_path;
		if (thread->Kind == core::tour_thread_kind::stop) {
			const auto found = std::ranges::find(Draft->Stops, thread->Stop, &core::tour_stop::Id);
			if (found != Draft->Stops.end()) {
				return pathOf(*found);
			}
		}
		// The language most of the tour's stops are in, the first such stop
		// breaking a tie, named as a file of that kind.
		auto counts = std::map<std::string, std::size_t>{ };
		auto first  = std::vector<std::string>{ };
		for (const auto& stop : Draft->Stops) {
			const auto path = pathOf(stop);
			const auto dot  = path.rfind('.');
			if (dot == std::string::npos || path.find('/', dot) != std::string::npos) {
				continue;
			}
			const auto kind = path.substr(dot);
			if (counts[kind]++ == 0U) {
				first.push_back(kind);
			}
		}
		auto best = std::string{ };
		for (const auto& kind : first) {
			if (best.empty() || counts[kind] > counts[best]) {
				best = kind;
			}
		}
		return best.empty() ? std::string{ } : "code" + best;
	}

	void tour_side_chat::set_reference_links(agent_reference_files_source aFiles, agent_reference_navigation aNavigate, agent_reference_chooser aChoose)
	{
		ReferenceFiles    = std::move(aFiles);
		ReferenceNavigate = std::move(aNavigate);
		ReferenceChoose   = std::move(aChoose);
		present_thread();
	}

	auto tour_side_chat::reader(QWidget* aParent, const std::string& aBody, const std::string& aIdentity, const std::string& aInlineSelector, const bool aFramed) -> markdown_body_view*
	{
		auto* const view = new markdown_body_view{ aParent };
		view->set_in_place(true);
		// [[BPR-1944]]. A reply's code is a box as wide as the chat, its long
		// lines scrolling inside it, with Wrap and Copy. A proposal keeps the
		// look its stop will have once published, as the stop reader shows it.
		view->set_code_block_tools(aFramed);
		view->setFocusPolicy(Qt::NoFocus);
		dress_code(*view, aInlineSelector);
		view->set_height_observer([this] { QTimer::singleShot(0, this, [this] { fit(); }); });
		view->set_body(aBody, aIdentity);
		return view;
	}

	void tour_side_chat::dress_code(markdown_body_view& aView, const std::string& aInlineSelector) const
	{
		// [[BPR-1914]] v004, owner look v003: "it seems like inline code isnt
		// being highlighted only blocks". Inline code reads as the part's
		// code does; with no language it stays plain monospace on its wash.
		aView.set_inline_code_selector(aInlineSelector);
		// [[BPR-2351]]. A stop's chat reads its code with the stop's file.
		aView.set_code_context(code_context());
		if (CodeFont != QFont{ }) {
			aView.set_code_font(CodeFont, TabStop);
		}
		if (CodeTheme) {
			aView.set_code_theme(*CodeTheme);
		}
		// [[BPR-1925]] v002, owner on v001: the chat's code was plain again.
		// Setting the body asks for its colours, so the dispatch is set first.
		if (CodeDispatch) {
			aView.set_code_syntax_dispatch(CodeDispatch);
		}
	}

	void tour_side_chat::dress_reply(markdown_body_view& aReader)
	{
		dress_code(aReader, inline_selector());
		aReader.set_link_dispatch([this](const std::string& aLink) { static_cast<void>(follow_agent_reference(this, QString::fromStdString(aLink), ReferenceNavigate, ReferenceChoose)); });
	}

	auto tour_side_chat::run_of(const std::string& aTurn) const -> const core::tour_agent_run*
	{
		constexpr auto prefix = std::string_view{ "tour-run:" };
		if (!aTurn.starts_with(prefix)) {
			return nullptr;
		}
		const auto id    = core::tour_agent_run_id{ std::stoull(aTurn.substr(prefix.size())) };
		const auto found = std::ranges::find(Session.Runs, id, &core::tour_agent_run::Id);
		return found == Session.Runs.end() ? nullptr : &*found;
	}

	auto tour_side_chat::reply_body(const core::chat_turn& aTurn) const -> std::string
	{
		// [[BPR-1998]]. The reply's references are links, by the Agent tab's
		// recogniser; Copy keeps the reply as written.
		if (!ReferenceFiles || !ReferenceNavigate) {
			return aTurn.Reply;
		}
		const auto thread = current();
		const auto stop   = thread && thread->Kind == core::tour_thread_kind::stop ? QString::fromStdString(inline_selector()) : QString{ };
		return agent_markdown_with_reference_links(QString::fromStdString(aTurn.Reply), ReferenceFiles(), stop).toStdString();
	}

	void tour_side_chat::dress_bubble(const core::chat_turn& aTurn, QWidget& aBubble, QVBoxLayout& aAbove, QLabel& aQuestion)
	{
		const auto* const run = run_of(aTurn.Id);
		if (run == nullptr) {
			return;
		}
		if (run->Passage) {
			const auto code   = run->Passage->Field == core::tour_passage_field::stop_code;
			auto       quoted = QString::fromStdString(run->Passage->Text);
			const auto named  = code && quoted.toStdString() == run->Passage->Path + ":" + std::to_string(run->Passage->FirstLine) + "-" + std::to_string(run->Passage->LastLine);
			aAbove.addWidget(quiet(small(words(passage_heading(*run->Passage), &aBubble))));
			if (!named) {
				if (code) {
					auto kept = quoted.split(QLatin1Char('\n'));
					if (kept.size() > 6) {
						kept = kept.mid(0, 6);
						kept << ellipsis;
					}
					auto* const lines = quiet(small(words(kept.join(QLatin1Char('\n')), &aBubble)));
					auto        face  = CodeFont != QFont{ } ? CodeFont : QFontDatabase::systemFont(QFontDatabase::FixedFont);
					face.setPointSizeF(lines->font().pointSizeF());
					lines->setFont(face);
					lines->setWordWrap(false);
					aAbove.addWidget(lines);
				} else {
					auto* const said = quiet(small(words(open_quote + quoted.simplified() + close_quote, &aBubble)));
					auto        face = said->font();
					face.setItalic(true);
					said->setFont(face);
					aAbove.addWidget(said);
				}
			}
		}
		// [[BPR-2235]], as the Agent tab ([[BPR-2219]] AC-h): a turn sent from
		// a prompt shows the agent glyph and the prompt's name; what was sent
		// is behind Show prompt.
		auto name = QString{ };
		if (const auto sent = SentNames.find(run->Question); sent != SentNames.end()) {
			name = sent->second;
		} else if (TurnNames) {
			name = TurnNames(run->Question);
		}
		if (name.isEmpty()) {
			return;
		}
		auto* const nameRow = new QHBoxLayout{ };
		nameRow->setContentsMargins(0, 0, 0, 0);
		nameRow->setSpacing(6);
		auto* const glyph = new QLabel{ &aBubble };
		glyph->setObjectName(QStringLiteral("tourSideChatPromptGlyph"));
		glyph->setPixmap(agent_glyph(palette()).pixmap(QSize{ 14, 14 }));
		glyph->setToolTip(tr("Sent from a prompt"));
		auto* const title = words(name, &aBubble);
		title->setObjectName(QStringLiteral("tourSideChatPromptName"));
		auto strong = title->font();
		strong.setWeight(QFont::DemiBold);
		title->setFont(strong);
		const auto  shown = PromptsShown.contains(run->Question);
		auto* const fold  = new quiet_push_button{ shown ? tr("Hide prompt") : tr("Show prompt"), &aBubble };
		fold->setObjectName(QStringLiteral("tourSideChatShowPrompt"));
		fold->setFlat(true);
		connect(fold, &QPushButton::clicked, this, [this, asked = QPointer<QLabel>{ &aQuestion }, fold, question = run->Question] {
			const auto open = !PromptsShown.contains(question);
			if (open) {
				PromptsShown.insert(question);
			} else {
				PromptsShown.erase(question);
			}
			if (asked) {
				asked->setVisible(open);
			}
			fold->setText(open ? tr("Hide prompt") : tr("Show prompt"));
		});
		nameRow->addWidget(glyph, 0, Qt::AlignVCenter);
		nameRow->addWidget(title, 1);
		nameRow->addWidget(fold, 0, Qt::AlignVCenter);
		aAbove.addLayout(nameRow);
		aQuestion.setVisible(shown);
		aBubble.setAccessibleName(tr("%1: %2").arg(name, QString::fromStdString(run->Question)));
	}

	void tour_side_chat::act(const std::string& aTurn, const chat_turn_gesture aGesture)
	{
		const auto* const run = run_of(aTurn);
		if (run == nullptr) {
			return;
		}
		if (aGesture == chat_turn_gesture::resend) {
			// [[BPR-2000]]. Resend starts a run.
			const auto again = tour_ask{ .Thread = run->Thread, .Passage = run->Passage, .Question = QString::fromStdString(run->Question), .Update = run->Update };
			if (HostIdle && !HistoryLoading && Ready && !Running && AskDispatch) {
				AskDispatch(again);
			}
			return;
		}
		if (!Running && DeleteDispatch) {
			DeleteDispatch(run->Id);
		}
	}

	auto tour_side_chat::thread_facts() const -> core::tour_thread_facts
	{
		auto facts     = core::tour_thread_facts{ };
		facts.Proposal = Proposal;
		facts.Running  = Running;
		for (const auto& [run, name] : RunAgents) {
			facts.Agents.emplace(run, name.toStdString());
		}
		for (const auto& [run, worked] : RunActivity) {
			auto activity     = core::chat_activity{ };
			activity.Activity = worked.first;
			activity.Seconds  = worked.second;
			facts.Activity.emplace(run, std::move(activity));
		}
		facts.Deletes      = static_cast<bool>(DeleteDispatch);
		facts.CheckScratch = CheckScratch;
		facts.CheckBuilds  = CheckBuilds;
		return facts;
	}

	auto tour_side_chat::make_attachment(const core::chat_turn& aTurn, const core::chat_attachment& aAttachment, QWidget* aHolder) -> QWidget*
	{
		if (const auto* const check = std::get_if<core::chat_tour_check>(&aAttachment.Payload)) {
			// RFC-BPR-0120. A check's plan card or report card.
			auto call = [turn = check->Run](const std::function<void(core::tour_agent_run_id)>& aDispatch) {
				return [aDispatch, turn] {
					if (aDispatch) {
						aDispatch(turn);
					}
				};
			};
			return new local_check_card{ check->Check, QString::fromStdString(check->Blocked), check->Latest, { .Scratch = check->Scratch, .Builds = check->Builds }, { .Run = call(Checks.Run), .Discard = call(Checks.Discard), .Open = call(Checks.Open), .Again = call(Checks.Again), .Grants = Checks.Grants }, aHolder };
		}
		if (const auto* const focus = std::get_if<core::chat_tour_focus>(&aAttachment.Payload)) {
			return make_focus_line(*focus, aHolder);
		}
		if (const auto* const revision = std::get_if<core::chat_tour_revision>(&aAttachment.Payload)) {
			return make_revision_card(*revision, aHolder);
		}
		if (const auto* const refused = std::get_if<core::chat_tour_refused>(&aAttachment.Payload)) {
			// [[BPR-1937]], owner 2026-11-08: asked for 8 stops, the refused
			// merges went unsaid and the earlier plan was applied. Each part of
			// this turn's revision that couldn't be proposed, and why.
			auto lines = QStringList{ };
			for (const auto& part : refused->Parts) {
				lines << tour_agent_panel::refused_words(part);
			}
			auto* const line = quiet(small(words(lines.join(QLatin1Char('\n')), aHolder)));
			line->setObjectName(QStringLiteral("tourSideChatRefused"));
			RefusedLine = line;
			return line;
		}
		if (std::holds_alternative<core::chat_tour_unchanged>(aAttachment.Payload)) {
			// [[BPR-1937]], [[BPR-1995]] RFC-BPR-0100 AC02. A change described
			// only in words changes nothing: under the newest answer that
			// proposed nothing, in every thread, the way to ask for the change
			// itself. It is the only place the update is offered.
			auto* const line = quiet(small(new QLabel{ aHolder }));
			line->setObjectName(QStringLiteral("tourSideChatUnchanged"));
			line->setWordWrap(true);
			line->setTextFormat(Qt::RichText);
			line->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
			connect(line, &QLabel::linkActivated, this, [this] { static_cast<void>(request_update()); });
			UnchangedLine = line;
			refresh_unchanged_line();
			return line;
		}
		if (const auto* const proposed = std::get_if<core::chat_tour_proposal>(&aAttachment.Payload)) {
			return make_proposal_card(aTurn, *proposed, aHolder);
		}
		return nullptr;
	}

	auto tour_side_chat::make_focus_line(const core::chat_tour_focus& aFocus, QWidget* aHolder) -> QWidget*
	{
		// [[BPR-1930]] follow-up, owner: "I dont really understand the focus
		// changes button. I clicked it and couldnt tell what it did then it
		// went away." A focus applies at once; one line says so, with Undo
		// while it is still the focus.
		const auto  focus = QString::fromStdString(aFocus.Focus).toHtmlEscaped();
		auto* const line  = quiet(small(new QLabel{ aHolder }));
		line->setObjectName(QStringLiteral("tourSideChatFocus"));
		line->setWordWrap(true);
		line->setTextFormat(Qt::RichText);
		line->setText(aFocus.Undone         ? tr("Focus set to %1%2%3, then undone.").arg(QString{ open_quote }, focus, QString{ close_quote })
						  : aFocus.Undoable ? tr("Focus set to %1%2%3 · <a href=\"#undo\">Undo</a>").arg(QString{ open_quote }, focus, QString{ close_quote })
											: tr("Focus set to %1%2%3.").arg(QString{ open_quote }, focus, QString{ close_quote }));
		line->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		line->setToolTip(tr("Later drafts and chat answers favour this part of the pull request. It shows as a chip under the tour's header."));
		connect(line, &QLabel::linkActivated, this, [this, turn = aFocus.Run] {
			if (FocusUndoDispatch) {
				FocusUndoDispatch(turn);
			}
		});
		FocusLine = line;
		return line;
	}

	auto tour_side_chat::make_revision_card(const core::chat_tour_revision& aRevision, QWidget* aHolder) -> QWidget*
	{
		// [[BPR-1995]], RFC-BPR-0100 AC04. The tour chat's revision waits in the
		// tour, marked grey, on the parts it changes, and here as one card that
		// lists them: Apply all, Review each in the tour, or Discard. Deciding
		// in either place settles the other, because the card is drawn from the
		// one proposal.
		auto pending  = std::vector<std::size_t>{ };
		auto accepted = std::size_t{ 0U };
		auto decided  = std::size_t{ 0U };
		for (std::size_t each = 0U; each < aRevision.Items.size(); ++each) {
			if (aRevision.Decisions[each] == core::tour_proposal_decision::pending) {
				pending.push_back(aRevision.Items[each]);
			} else {
				++decided;
				accepted += aRevision.Decisions[each] == core::tour_proposal_decision::accepted ? 1U : 0U;
			}
		}
		const auto  turn  = aRevision.Run;
		auto* const card  = new proposal_frame{ aHolder };
		auto* const lines = new QVBoxLayout{ card };
		card->setObjectName(QStringLiteral("tourSideChatRevision"));
		lines->setContentsMargins(12, 8, 8, 8);
		lines->setSpacing(4);
		auto* const title = small(words(tr("Proposed revision of the tour"), card));
		auto        heavy = title->font();
		heavy.setWeight(QFont::DemiBold);
		title->setFont(heavy);
		Accented.emplace_back(title);
		lines->addWidget(title);
		auto* const operations = small(words(revision_words(turn).join(QLatin1Char('\n')), card));
		operations->setObjectName(QStringLiteral("tourSideChatRevisionParts"));
		lines->addWidget(operations);
		auto* const state = quiet(small(words({ }, card)));
		state->setObjectName(QStringLiteral("tourSideChatRevisionState"));
		lines->addWidget(state);
		RevisionCard       = card;
		RevisionOperations = operations;
		RevisionState      = state;
		RevisionAcceptAll  = nullptr;
		RevisionReview     = nullptr;
		RevisionDiscard    = nullptr;
		if (pending.empty()) {
			state->setText(accepted == decided ? tr("Applied to the private draft.") : accepted == 0U ? tr("Discarded.")
																									  : tr("%1 of %2 parts applied.").arg(accepted).arg(decided));
			return card;
		}
		state->hide();
		if (decided > 0U) {
			state->setText(tr("%1 of %2 parts decided in the tour.").arg(decided).arg(decided + pending.size()));
			state->show();
		}
		// Three buttons in a row that wraps where the column is narrow; each
		// says what it does in its tooltip, not in words under the card.
		auto* const buttons = new detail::wrapping_row{ nullptr, 6, 6 };
		// RFC-BPR-0106 as amended on 2026-11-13: it accepts no stop, so it
		// does not say Accept.
		auto* const acceptAll = make_tour_action(tr("Apply all"), card);
		acceptAll->setObjectName(QStringLiteral("tourSideChatRevisionAcceptAll"));
		acceptAll->setToolTip(tr("Puts every part of this revision in the private draft. Nothing is published, and each new or changed stop still asks for your Accept."));
		auto* const review = make_tour_action(tr("Review each"), card);
		review->setObjectName(QStringLiteral("tourSideChatRevisionReview"));
		review->setToolTip(tr("Review each in the tour, where the parts wait marked grey: accept or discard them one by one there."));
		review->setAccessibleName(tr("Review each in the tour"));
		auto* const discard = make_tour_action(tr("Discard"), card);
		discard->setObjectName(QStringLiteral("tourSideChatRevisionDiscard"));
		discard->setToolTip(tr("Discards every part of this revision still waiting. The tour is unchanged."));
		for (auto* const button : { static_cast<QPushButton*>(acceptAll), static_cast<QPushButton*>(review), static_cast<QPushButton*>(discard) }) {
			button->setFocusPolicy(Qt::TabFocus);
			button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			buttons->addWidget(button);
		}
		lines->addLayout(buttons);
		RevisionAcceptAll = acceptAll;
		RevisionReview    = review;
		RevisionDiscard   = discard;
		// [[BPR-2000]]. Apply all and Discard apply; they start no run.
		const auto decide = [this, pending](const core::tour_proposal_decision aDecision) {
			if (!DecideDispatch || !Proposal) {
				return;
			}
			// Each part's text first: deciding one draws the chat again.
			auto decisions = std::vector<std::pair<std::size_t, QString>>{ };
			for (const auto index : pending) {
				if (index < Proposal->Items.size() && Proposal->Items[index].Decision == core::tour_proposal_decision::pending) {
					decisions.emplace_back(index, QString::fromStdString(Proposal->Items[index].Text));
				}
			}
			for (const auto& [index, text] : decisions) {
				DecideDispatch(index, aDecision, aDecision == core::tour_proposal_decision::accepted ? text : QString{ });
			}
		};
		connect(acceptAll, &QPushButton::clicked, this, [decide] { decide(core::tour_proposal_decision::accepted); });
		connect(discard, &QPushButton::clicked, this, [decide] { decide(core::tour_proposal_decision::discarded); });
		connect(review, &QPushButton::clicked, this, [this, turn] {
			if (ReviewDispatch) {
				ReviewDispatch(turn);
			}
		});
		return card;
	}

	auto tour_side_chat::make_proposal_card(const core::chat_turn& aTurn, const core::chat_tour_proposal& aProposed, QWidget* aHolder) -> QWidget*
	{
		// What it proposed, decided here.
		const auto  index = aProposed.Index;
		const auto& item  = aProposed.Item;
		auto* const card  = new proposal_frame{ aHolder };
		auto* const lines = new QVBoxLayout{ card };
		lines->setContentsMargins(12, 8, 8, 8);
		lines->setSpacing(4);
		auto heading = QString{ };
		switch (item.Part) {
		case core::tour_proposal_part::stop_text: heading = tr("Proposed explanation for %1").arg(QString::fromStdString(item.Stop.Title)); break;
		case core::tour_proposal_part::stop:      heading = tr("Proposed stop: %1 (%2, lines %3 to %4)").arg(QString::fromStdString(item.Stop.Title), QString::fromStdString(item.Stop.Path)).arg(item.Stop.FirstLine).arg(item.Stop.LastLine); break;
		// [[BPR-1930]]. Only the tour's chat proposes these, and they wait in
		// the tour, not here.
		case core::tour_proposal_part::remove_stop:
		case core::tour_proposal_part::merge_stops:
		case core::tour_proposal_part::split_stop:
		case core::tour_proposal_part::reorder:     heading = tr("A change to the tour"); break;
		}
		auto* const title = small(words(heading, card));
		auto        heavy = title->font();
		heavy.setWeight(QFont::DemiBold);
		title->setFont(heavy);
		Accented.emplace_back(title);
		lines->addWidget(title);
		if (item.Decision != core::tour_proposal_decision::pending) {
			lines->addWidget(quiet(small(words(!item.Refusal.empty() ? tr("Refused: the target changed since this question was sent.") : item.Decision == core::tour_proposal_decision::accepted ? tr("In the tour now.")
																																																 : tr("Discarded."),
											   card))));
			return card;
		}
		if (item.ReplacesAuthorText) {
			lines->addWidget(quiet(small(words(tr("Using it replaces what is there now."), card))));
		}
		const auto  source   = Edited.contains(index) ? Edited[index] : QString::fromStdString(item.Text);
		const auto  editing  = Editing.contains(index);
		const auto  identity = [turn = aTurn.Id, index](const QString& aSource) { return "tour-side-chat-proposal:" + turn + ":" + std::to_string(index) + ":" + std::to_string(qHash(aSource)); };
		auto* const shown    = reader(card, source.toStdString(), identity(source), inline_selector());
		shown->setObjectName(QStringLiteral("tourSideChatProposal"));
		shown->setVisible(!editing);
		ProposalReaders[index] = shown;
		lines->addWidget(shown);
		auto* const editor = new QPlainTextEdit{ card };
		editor->setObjectName(QStringLiteral("tourSideChatProposalText"));
		editor->setAccessibleName(heading);
		editor->setPlainText(source);
		editor->setTabChangesFocus(true);
		editor->setFixedHeight(std::clamp(static_cast<int>(source.size() / 40 + 2), 3, 12) * editor->fontMetrics().lineSpacing() + 14);
		editor->setVisible(editing);
		lines->addWidget(editor);
		connect(editor, &QPlainTextEdit::textChanged, this, [this, index, editor] { Edited[index] = editor->toPlainText(); });
		// The card's choices as a part's are in the tour, on one row: a pencil
		// and a trash, the comments' own glyphs, and at the right the one
		// worded action.
		auto* const buttons = new detail::wrapping_row{ nullptr, 6, 6 };
		buttons->set_last_trails(true);
		const auto tool = [card](const conversation_glyph aGlyph, const QString& aName) {
			auto* const made = new quiet_tool_button{ card };
			made->setObjectName(aName);
			made->set_tinted(true);
			made->setIcon(conversation_icon(aGlyph, card->palette()));
			made->setIconSize(QSize{ 16, 16 });
			made->setToolButtonStyle(Qt::ToolButtonIconOnly);
			made->setAutoRaise(true);
			made->setCursor(Qt::ArrowCursor);
			// No choice takes the keyboard on a press; Tab reaches each.
			made->setFocusPolicy(Qt::TabFocus);
			return made;
		};
		// The same words and look as the part's button in the tour: Accept
		// only on a stop's own text, which accepts the stop.
		auto* const accept = make_tour_action(tour_proposal_apply_words(item.Part), card);
		accept->setObjectName(QStringLiteral("tourSideChatAccept"));
		accept->setFocusPolicy(Qt::TabFocus);
		accept->setToolTip(item.Part == core::tour_proposal_part::stop ? tr("Adds this stop at the end of the tour.")
							   : tour_proposal_accepts_stop(item.Part) ? tr("Puts this text in the stop and accepts the stop.")
																	   : tr("Puts this text in the tour."));
		if (tour_proposal_accepts_stop(item.Part)) {
			wear_stop_accept(*accept);
		}
		// The pencil is down while the source shows, and says which way it
		// goes next. An icon only: its tooltip starts with its name, which is
		// its accessible name.
		auto* const edit     = tool(conversation_glyph::edit, QStringLiteral("tourSideChatEdit"));
		const auto  pencilBy = [edit](const bool aEditing) {
			edit->setChecked(aEditing);
			edit->setAccessibleName(aEditing ? tr("Show formatted") : tr("Edit text"));
			edit->setAccessibleDescription(aEditing ? tr("Shows this text as it will read.") : tr("Shows this text's Markdown source, which you can change before you apply it."));
			edit->setToolTip(edit->accessibleName() + QStringLiteral("\n") + edit->accessibleDescription());
		};
		edit->setCheckable(true);
		pencilBy(editing);
		auto* const discard = tool(conversation_glyph::trash, QStringLiteral("tourSideChatDiscard"));
		discard->setAccessibleName(tr("Discard"));
		discard->setAccessibleDescription(tr("Discards this suggestion. The tour is unchanged."));
		discard->setToolTip(tr("Discard") + QStringLiteral("\n") + discard->accessibleDescription());
		buttons->addWidget(edit);
		buttons->addWidget(discard);
		buttons->addWidget(accept);
		lines->addLayout(buttons);
		Accepts[index] = accept;
		// [[BPR-2000]]. Accept applies; it starts no run.
		connect(accept, &QPushButton::clicked, this, [this, index, editor, stop = item.Part == core::tour_proposal_part::stop_text ? item.StopId : core::tour_stop_id{ }] {
			if (DecideDispatch) {
				DecideDispatch(index, core::tour_proposal_decision::accepted, editor->toPlainText());
				// [[BPR-2014]]. Accept on a stop's own proposal accepts the stop.
				if (OwnAcceptDispatch && stop.value() != 0U) {
					OwnAcceptDispatch(stop, editor->toPlainText().toStdString());
				}
			}
		});
		connect(discard, &QToolButton::clicked, this, [this, index] {
			if (DecideDispatch) {
				DecideDispatch(index, core::tour_proposal_decision::discarded, { });
			}
		});
		// The pencil turns the card between its text as it reads and its
		// source, in place; what was written stays.
		connect(edit, &QToolButton::clicked, this, [this, index, shown, editor, pencilBy, identity] {
			const auto open = Editing.insert(index).second;
			if (!open) {
				Editing.erase(index);
				const auto text = editor->toPlainText();
				shown->set_body(text.toStdString(), identity(text));
			}
			shown->setVisible(!open);
			editor->setVisible(open);
			pencilBy(open);
			fit();
		});
		return card;
	}

	void tour_side_chat::present_thread()
	{
		Accented.erase(std::remove_if(Accented.begin(), Accented.end(), [](const QPointer<QWidget>& aWidget) { return aWidget.isNull(); }), Accented.end());
		refresh_header();
		show_passage();
		const auto thread    = current();
		const auto runs      = thread ? core::thread_runs(Session, *thread) : std::vector<const core::tour_agent_run*>{ };
		const auto wholeTour = thread && thread->Kind == core::tour_thread_kind::tour;
		const auto running   = std::ranges::find(runs, core::tour_agent_run_state::running, &core::tour_agent_run::State);
		Mine                 = running != runs.end();
		if (Mine && Live) {
			RunAgents[(*running)->Id.value()] = Live->Agent;
		}
		// [[BPR-1930]], [[BPR-1995]]. The tour's chat takes feedback on the
		// tour's shape; a request comes back as the change itself.
		Empty->setText(!thread         ? QString{ }
						   : wholeTour ? tr("Ask about the tour, or ask for a change: fewer stops, a merge, a new order, a focus.")
									   : tr("Ask about %1, or ask for a change to it.").arg(part_words()));
		Empty->setVisible(thread && runs.empty() && !(wholeTour && Session.Waiting));
		// A part's Accept and its text are found only while it waits in this
		// chat; a card the transcript is putting away is no longer it.
		const auto waits = [&](const std::size_t aIndex) { return !wholeTour && Proposal && aIndex < Proposal->Items.size() && Proposal->Items[aIndex].Decision == core::tour_proposal_decision::pending; };
		std::erase_if(Accepts, [&](const auto& aEntry) { return !waits(aEntry.first); });
		std::erase_if(ProposalReaders, [&](const auto& aEntry) { return !waits(aEntry.first); });
		// RFC-BPR-0130 stage 4. The one transcript changes only the turns that
		// changed.
		Transcript->present_live(live_status(), LiveClock);
		Transcript->set_resend_enabled(HostIdle && !HistoryLoading && Ready && !Running);
		Transcript->present(thread ? core::project_tour_thread(Session, *thread, thread_facts()) : core::chat_transcript{ });
		// A line or card the transcript put away (its turn gone, or no longer
		// holding it) is no longer the one shown.
		// A part just made is not shown yet; one put away was hidden on purpose.
		const auto gone = [body = Transcript->widget()](const QWidget* aPart) {
			for (const auto* each = aPart; each != nullptr && each != body; each = each->parentWidget()) {
				if (each->isHidden() && each->testAttribute(Qt::WA_WState_ExplicitShowHide)) {
					return true;
				}
			}
			return false;
		};
		if (gone(RevisionCard)) {
			RevisionCard       = nullptr;
			RevisionOperations = nullptr;
			RevisionState      = nullptr;
			RevisionAcceptAll  = nullptr;
			RevisionReview     = nullptr;
			RevisionDiscard    = nullptr;
		}
		for (auto* const line : { &RefusedLine, &UnchangedLine, &FocusLine }) {
			if (gone(line->data())) {
				*line = nullptr;
			}
		}
		DraftBlock->setVisible(wholeTour && Drafting);
		if (!DraftBlock->isHidden()) {
			DraftLine->present(live_status(), LiveClock);
		}
		// [[BPR-1934]], owner 2026-11-07: "Tour side chat should get
		// suggestions like the agent tab has." The Agent tab's own component,
		// the chat's last item, in the agent's accent: the prompts for this
		// kind of chat while it is empty, and Explain the selected block while
		// text is selected (v002).
		if (thread && PromptSource && Suggestions == nullptr) {
			Suggestions = new agent_suggestion_list{ tr("Or try:"), Transcript->widget() };
			Suggestions->setObjectName(QStringLiteral("tourSideChatSuggestions"));
			Suggestions->set_choose_dispatch([this](const QString& aPrompt, const bool aEditFirst) { choose_prompt(aPrompt, aEditFirst); });
			Accented.emplace_back(Suggestions.data());
			Transcript->trail()->addWidget(Suggestions);
		}
		if (Suggestions != nullptr && !thread) {
			Suggestions->hide();
		}
		// The author's own new question takes the chat to its end.
		if (runs.size() > Shown) {
			Transcript->pin();
		}
		Shown = runs.size();
		restyle(palette());
		refresh_actions();
		QTimer::singleShot(0, this, [this] { fit(); });
	}

	void tour_side_chat::fit()
	{
		// Each rendered text as tall as it reads at the chat's width; the
		// author's bubbles at most four fifths of it.
		Transcript->fit();
		show_passage();
	}
}
