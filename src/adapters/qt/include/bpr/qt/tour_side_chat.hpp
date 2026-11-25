#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_SIDE_CHAT_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_SIDE_CHAT_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/agent_turn.hpp"
#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/quick_ask.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/skill_references.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/qt/agent_pending_list.hpp"
#include "bpr/qt/agent_reference_links.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_session_menu.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/chat_history_menu.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include <QElapsedTimer>
#include <QFont>
#include <QIcon>
#include <QPointer>
#include <QToolButton>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

class QLabel;
class QMenu;
class QPainter;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QHBoxLayout;
class QAction;

class QVBoxLayout;

namespace bpr::qt {
	class skill_completer;
	/// \brief The fresh run whose answer a chat's next question starts from,
	/// as the host tells the chat (`core::fresh_answer_to_carry` decides):
	/// Draft Tour's first draft, for the chat about the tour as a whole, or an
	/// Enhance whose suggestion still waits, for its part's chat.
	enum class tour_carried_answer : std::uint8_t {
		none,
		draft,
		enhance,
	};
	/// \brief [[BPR-1914]] v003. One question for the tour agent from a side
	/// chat: the chat's part, the passage quoted with it, the words, and
	/// whether it asks for the part rewritten from the chat ("Update this
	/// section from our chat"). Every side chat's composer sends this, through
	/// one dispatch, so a reviewer's own `/skill` references ([[BPR-1916]])
	/// are resolved in one place.
	struct tour_ask
	{
		core::tour_thread                 Thread;
		std::optional<core::tour_passage> Passage;
		QString                           Question;
		bool                              Update = false;
		/// [[BPR-2138]]. Enhance: a fresh run whose result is a proposal on
		/// the part; nothing is sent into its side chat.
		bool Fresh = false;
		/// RFC-BPR-0125 (AC09). For a suggested prompt sent as offered, the
		/// only text its skill references are read from; none reads
		/// `Question`.
		std::optional<std::string> SkillText;
		/// [[BPR-2235]]. The prompt's name, for a prompt sent as offered whose
		/// text is not its name; empty for anything typed or edited.
		QString Name;
		/// RFC-BPR-0126 ([[BPR-2154]]). Set for a follow-up typed while the
		/// tour agent runs or this part's queue holds messages: queued on
		/// the part's own queue (false) or steered into its running turn
		/// (true). None: sent now.
		std::optional<bool> Steer;
	};

	/// \brief The side chat's glyph: a small speech bubble, in the colour of
	/// `aPalette`'s button text, for the quiet "Open side chat" affordances.
	[[nodiscard]] auto side_chat_icon(const QPalette& aPalette) -> QIcon;

	/// \brief [[BPR-1925]] v002. Draws a part's chat toggle in `aBox`: off, a
	/// speech bubble outlined in the agent accent with the chat's turns
	/// beside it; on, a pill filled with the accent and the bubble and turns
	/// in the ground's colour. The route draws its stops' toggles with it.
	void paint_chat_toggle(QPainter& aPainter, const QRectF& aBox, const QPalette& aPalette, std::size_t aTurns, bool aOn, bool aHovered);
	/// \brief The size `paint_chat_toggle` needs for `aTurns` in `aFont`.
	[[nodiscard]] auto chat_toggle_size(const QFont& aFont, std::size_t aTurns) -> QSize;

	/// \brief [[BPR-1925]] v002, owner on v001: "The mockup had the chat button
	/// on the right side of each of the sections and I prefer that, I also
	/// want the chat button to toggle it open and closed." A part's chat
	/// button, at the right of its header: pressed while that part's chat is
	/// in the column, with the chat's turns. It is a checkable tool button,
	/// so Space and Return work it and it shows the keyboard's focus; its
	/// accessible name says whether it opens or closes the chat.
	class part_chat_toggle final : public QToolButton
	{
	public:
		part_chat_toggle(const QString& aPart, QWidget* aParent = nullptr);
		/// \brief The chat's turns, and whether it is the one in the column.
		void               present(std::size_t aTurns, bool aOn);
		[[nodiscard]] auto turns() const noexcept -> std::size_t { return Turns; }
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override { return sizeHint(); }

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QString     Part;
		std::size_t Turns = 0U;
	};

	/// \brief [[BPR-1995]]. The side chat's composer field: the attached
	/// passage as a chip above the words, in one box drawn as a text field.
	class composer_field final : public QWidget
	{
	public:
		using QWidget::QWidget;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;

	private:
		bool Focused = false;
	};
	/// \brief [[BPR-1995]]. The passage a question quotes, as a chip.
	class passage_chip final : public QWidget
	{
	public:
		using QWidget::QWidget;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief [[BPR-1914]] v003, [[BPR-1925]]. Side chats with the tour
	/// agent: the Tour tab's second column, beside the tour.
	///
	/// \details [[BPR-1925]], owner 2026-11-07: "it's kind of weird that the
	/// sidechat is so decoupled from the tour when it's really about the
	/// tour." It no longer docks at the window's edge: the window places this
	/// column beside the tour, in the Tour tab, and it goes with that tab. The
	/// part a chat is about is marked in the tour, each part's glyph opens its
	/// chat, and there are no tabs: one chat shows at a time.
	///
	/// \details The owner rejected v002's inline threads (2026-11-06): "it's
	/// too in the way and noisy. I think I need something more like a side
	/// chat ... On each part I can pop open a side chat, ask some questions and
	/// then ... ask the agent to update the section based on our chat." So the
	/// tour stays as it was, and each part (the tour as a whole, a stop) has its own chat here, one tab each, opened from
	/// a small affordance on the part or its context menu, or from "+". The
	/// top says which part a chat is about and Show takes the tour (and a
	/// stop's code in the diff) there. The chat itself is plain: the author's
	/// words in bubbles on the right, the agent's replies as rendered Markdown
	/// on the left with their code highlighted ([[BPR-1919]]), Copy on each
	/// reply and Copy chat for all of it.
	/// [[BPR-1995]], RFC-BPR-0100: under an answer that changed nothing, the
	/// link "Update the part from this chat" asks for the part rewritten from
	/// the conversation; it is the only place the update is offered. A
	/// part's proposal is accepted, edited or discarded right in the chat; a
	/// whole-tour revision is a card listing its parts, with Apply all,
	/// Review each in the tour and Discard. Under the transcript sit only a
	/// notice when something is not as usual, the composer and the session
	/// line.
	///
	/// \details [[BPR-1941]], owner 2026-11-08: "bring the agent status
	/// updates from the agent tabs over to the tour tab's side chat". A turn
	/// ends with the Agent tab's own line (`agent_run_status_line`): while it
	/// runs, the agent, what it is doing and the time it has taken, live;
	/// then "Claude answered in 0:12." with Copy, or, for a turn that ended
	/// without an answer, "Stopped after 0:12." with Resend and Delete, as
	/// the core decides for the Agent tab's error turns. The whole tour's
	/// chat says a running Draft Tour the same way.
	///
	/// \details Presents `core::tour_agent_session` and `core::tour_proposal`
	/// and sends gestures; it decides and performs nothing.
	class tour_side_chat final : public QWidget
	{
	public:
		using ask_dispatch    = std::function<void(const tour_ask&)>;
		using decide_dispatch = std::function<void(std::size_t, core::tour_proposal_decision, const QString&)>;

		explicit tour_side_chat(QWidget* aParent = nullptr);
		/// [[BPR-2014]]. Told after Accept on one stop's own proposed text, never Apply all.
		void set_own_accept_dispatch(std::function<void(core::tour_stop_id, const std::string&)> aDispatch) { OwnAcceptDispatch = std::move(aDispatch); }
		/// \brief RFC-BPR-0101 AC12, without its button. What the open
		/// chat's next question starts from besides its own
		/// conversation, said in one quiet line on the question field until
		/// that question is asked. Ask more asked for this by a press, on
		/// each stop Draft Tour proposed and on an Enhance's suggestion in the
		/// chat it was already in; the owner, 2026-11-25, of the first: it
		/// "just opens the whole tour chat". The host carries the answer
		/// without being asked, and says so here.
		void               present_carried_answer(tour_carried_answer aCarried);
		[[nodiscard]] auto carried_answer_line() const noexcept -> QLabel* { return CarriedAnswer; }
		void               set_dispatch(ask_dispatch aAsk, std::function<void()> aStop, decide_dispatch aDecide);
		/// \brief RFC-BPR-0120. A local draft's checks: Plan a check… in the
		/// whole-change conversation (the composer's words, or none), and its
		/// plan and report cards' controls by turn. Unset, there is no check.
		struct check_dispatch
		{
			std::function<void(const QString&)>          Plan;
			std::function<void(core::tour_agent_run_id)> Run;
			std::function<void(core::tour_agent_run_id)> Discard;
			std::function<void(core::tour_agent_run_id)> Open;
			std::function<void(core::tour_agent_run_id)> Again;
			std::function<void(bool, bool)>              Grants;
		};
		void set_check_dispatch(check_dispatch aDispatch);
		/// \brief The draft's two grants for the chosen agent, and whether a
		/// check can be planned now (and why not).
		void set_check_state(bool aScratch, bool aBuilds, bool aAvailable, const QString& aWhy, bool aRebuild = false);
		/// \brief Show: brings the tour to the chat's part, and a stop's code
		/// into the diff.
		void set_reveal_dispatch(std::function<void(const core::tour_thread&)> aReveal) { RevealDispatch = std::move(aReveal); }
		/// \brief The column wants to be shown (true) or closed (false).
		void set_visibility_dispatch(std::function<void(bool)> aVisibility) { VisibilityDispatch = std::move(aVisibility); }
		/// \brief Opens (or brings forward) the chat about `aThread`, with
		/// `aPassage` quoted above its composer, and puts the keyboard there
		/// unless `aFocus` is false (the chat following the route).
		void open(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage = std::nullopt, bool aFocus = true);
		/// \brief Closes the chat about `aThread`; its turns stay in the
		/// session, and reopening it shows them again.
		void close(const core::tour_thread& aThread);
		/// \brief The tour as it is now names each part and quotes it.
		void present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal, std::shared_ptr<const core::review_tour> aDraft);
		/// \brief The tour as it is now, between sessions' presentations.
		void set_draft(std::shared_ptr<const core::review_tour> aDraft);
		/// \brief [[BPR-1930]]. `aDrafting`: the run is Draft the tour, and the
		/// tour's chat still takes feedback, which waits for its proposal.
		void set_running(bool aAny, bool aDrafting = false);
		/// \brief [[BPR-1941]]. What the running request is doing now, from
		/// the tour session's run events, as the Agent tab's turn says it;
		/// none once it ends. Its time is counted from the run's start here.
		void present_run_status(std::optional<agent_run_status> aStatus);
		/// \brief [[BPR-2005]]. What an ended run did, for its folded line.
		void record_run_activity(core::tour_agent_run_id aRun, core::agent_turn_activity aActivity, qint64 aSeconds);
		/// \brief The running turn's block, or an ended turn's folded block; null for none.
		[[nodiscard]] auto activity_view(std::size_t aTurn) const -> agent_activity_view*;
		/// \brief [[BPR-1941]]. Delete on a turn that ended without an answer.
		void set_delete_dispatch(std::function<void(core::tour_agent_run_id)> aDelete) { DeleteDispatch = std::move(aDelete); }
		void set_available(bool aReady, const QString& aWhy);
		/// rief [[BPR-2040]]. Observed host retirement, independent of the
		/// answer's Running state. Stop and queued draft feedback keep theirs.
		void set_host_idle(bool aIdle);
		/// \brief [[BPR-1934]], RFC-BPR-0125; [[BPR-2181]] (AC13, Prompts
		/// review B1). Where the open chat's suggested prompts are decided:
		/// handed what the chat shows, it answers the decided offers. The chat
		/// assembles no facts; with no source it offers none.
		using prompt_source = std::function<core::chat_prompts(const core::prompt_chat_view&)>;
		void set_prompt_source(prompt_source aSource);
		/// \brief [[BPR-2235]]. The name a turn's question was sent under, if
		/// one is kept, so a chat read back shows it as the Agent tab does.
		using turn_name_source = std::function<QString(const std::string& aAsked)>;
		void set_turn_name_source(turn_name_source aSource);
		/// \brief What the open chat shows that its prompts are decided on;
		/// none with no chat open.
		[[nodiscard]] auto prompt_view() const -> std::optional<core::prompt_chat_view>;
		/// \brief The decided offers shown again (the source's answer may
		/// have changed).
		void refresh_prompts();
		/// \brief [[BPR-1934]]. The open chat's suggested prompts, in the
		/// Agent tab's component, while the chat has no turns; otherwise null.
		/// Hidden while the agent is not set up ([[BPR-1881]]).
		[[nodiscard]] auto suggestions() const noexcept -> agent_suggestion_list* { return Suggestions; }
		/// \brief [[BPR-1934]] v002. The text selected now in `aThread`'s part,
		/// or none: while there is some, the chat offers "Explain the selected
		/// block", which asks about exactly that text, quoted, in that part's
		/// chat. The tour tells the chat as the selection changes.
		void offer_selection(const core::tour_thread& aThread, std::optional<core::tour_passage> aSelected);
		/// \brief [[BPR-1930]]. Feedback kept until the draft proposes, shown
		/// in the tour's chat while it waits.
		[[nodiscard]] auto waiting_feedback() const noexcept -> QWidget* { return WaitingBubble; }
		/// \brief [[BPR-1937]]. The newest whole-tour turn's refused parts, each
		/// with why, when it had any.
		[[nodiscard]] auto refused_line() const noexcept -> QLabel* { return RefusedLine; }
		/// \brief [[BPR-1937]]. Under the newest whole-tour answer that changed
		/// nothing: Update the tour from this chat, offered in place.
		[[nodiscard]] auto unchanged_line() const noexcept -> QLabel* { return UnchangedLine; }
		/// \brief [[BPR-1930]] follow-up. Undo on a turn's "Focus set to"
		/// line; the host restores the focus it replaced.
		void set_focus_undo_dispatch(std::function<void(core::tour_agent_run_id)> aUndo) { FocusUndoDispatch = std::move(aUndo); }
		/// \brief The newest turn's "Focus set to" line, if any.
		[[nodiscard]] auto focus_line() const noexcept -> QLabel* { return FocusLine; }
		/// \brief [[BPR-1919]]. Replies and proposals set their code as the
		/// diff's comments do.
		void set_code_style(const QFont& aFont, qreal aTabStopDistance, std::optional<markdown_code_theme> aTheme, std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		/// \brief [[BPR-1998]], RFC-BPR-0101. Which files the replies'
		/// references may name and where a link goes, as in the Agent tab; a
		/// stop's chat reads a bare "line 12" as its stop's file.
		void set_reference_links(agent_reference_files_source aFiles, agent_reference_navigation aNavigate, agent_reference_chooser aChoose = { });
		/// \brief The agent accent, from `aGround`'s window.
		void restyle(const QPalette& aGround);
		/// \brief [[BPR-1916]]. The skills `/name` completion offers in the
		/// composer, each with where it came from.
		void               present_skills(std::vector<core::skill_entry> aSkills);
		[[nodiscard]] auto skill_completion() const noexcept -> skill_completer* { return SkillCompletion; }

		/// \brief [[BPR-1925]]. Told when the chat shown, the column's state
		/// or the session changes, so the tour can mark the part and count the
		/// chats.
		void               set_change_observer(std::function<void()> aObserver) { ChangeObserver = std::move(aObserver); }
		[[nodiscard]] auto open_threads() const noexcept -> const std::vector<core::tour_thread>& { return Open; }
		[[nodiscard]] auto current() const -> std::optional<core::tour_thread>;
		/// \brief The parts whose chat was shown in this review, the shown one
		/// among them.
		[[nodiscard]] auto chat_count() const noexcept -> std::size_t { return Open.size(); }
		/// \brief The shown chat's place among `open_threads()`, or -1.
		[[nodiscard]] auto current_index() const noexcept -> int { return Current; }
		void               select(int aIndex);
		void               set_history_dispatch(std::function<void(const core::tour_thread&, std::optional<std::string>)> aDispatch) { HistoryDispatch = std::move(aDispatch); }
		/// \brief [[BPR-2377]]. Renames (`aName` set) or deletes a session of
		/// the current thread on this pull request.
		using session_edit_dispatch = std::function<void(const core::tour_thread&, const std::string& aId, std::optional<std::string> aName)>;
		void set_session_edit_dispatch(session_edit_dispatch aDispatch) { SessionEditDispatch = std::move(aDispatch); }
		/// \brief [[BPR-2377]]. History's session rows (pencil, trash, i):
		/// the shared `agent_session_menu`.
		auto session_rows() -> agent_session_menu&;
		/// \brief RFC-BPR-0130. The header's History menu, the Agent tab's
		/// own: New session first, the sessions, Delete all sessions…, the
		/// other parts' chats, Copy conversation, the revision read.
		[[nodiscard]] auto history_menu() const noexcept -> chat_history_menu* { return HistoryMenu; }
		/// \brief Whether this chat keeps sessions (a pull request's tour) or
		/// not (a local draft's discussion): the header then shows Copy
		/// conversation in History's place, as core decides.
		void set_sessions_offered(bool aOffered);
		/// \brief [[BPR-2377]]. What the last rename or deletion did.
		void present_session_edit(const QString& aStatus);
		void present_native_history(const application::native_agent_history_projection&);
		/// \brief The display name of the agent this chat asks, which its
		/// notice of a carried-over session and History's carry-over item
		/// name; empty while none is chosen.
		void set_agent_name(const QString& aName);
		/// \brief Projects the core's revision choice without submitting a question.
		void present_revision_continuation(std::optional<core::agent_continuation_offer> aOffer, core::git_commit_oid aObservedHead, bool aContinued);
		void set_continuation_dispatch(std::function<void(const core::tour_thread&)> aDispatch) { ContinuationDispatch = std::move(aDispatch); }
		/// \brief New session, History's first item: enabled exactly when
		/// core offers it, and greyed with its reason otherwise.
		[[nodiscard]] auto new_session_action() const noexcept -> QAction*;
		/// \brief History, in the header row; its `loading` property is true
		/// while the sessions are read.
		[[nodiscard]] auto history_button() const noexcept -> QToolButton* { return HistoryTool; }
		/// \brief [[BPR-1995]]. The notice above the composer: shown only for a
		/// transcript replay, native continuation unavailable or a moved head.
		[[nodiscard]] auto notice() const noexcept -> QWidget* { return Notice; }
		[[nodiscard]] auto composer_box() const noexcept -> QWidget* { return Field; }

		/// \brief [[BPR-1925]]. Whether the column is open (it shows while the
		/// Tour tab does), and the part it is about while it is.
		[[nodiscard]] auto column_open() const noexcept -> bool { return ColumnOpen; }
		[[nodiscard]] auto discussed() const -> std::optional<core::tour_thread>;
		/// \brief Opens or closes the column; closed, every chat is kept.
		void set_column_open(bool aOpen);
		/// \brief [[BPR-1925]]. Turns in `aThread`'s chat, for its glyph.
		[[nodiscard]] auto turns_about(const core::tour_thread& aThread) const -> std::size_t;
		/// \brief [[BPR-1925]]. How many parts have a chat, for the Tour tab's
		/// badge (`core::chat_threads`).
		[[nodiscard]] auto chat_parts() const -> std::size_t;
		[[nodiscard]] auto composer() const noexcept -> QPlainTextEdit* { return Composer; }
		[[nodiscard]] auto send_button() const noexcept -> QPushButton* { return Send; }
		/// \brief RFC-BPR-0126 ([[BPR-2154]]). Stop, beside Queue while this
		/// chat's run goes and the composer holds words.
		[[nodiscard]] auto stop_button() const noexcept -> QPushButton* { return StopRun; }
		/// \brief The follow-up setting: Queue (false, the default) or Steer;
		/// Cmd+Return (Ctrl+Return off macOS) is the other for one message.
		void set_follow_up_steer(bool aSteer);
		/// \brief A part's queue, as its session's host shows it. The chat
		/// shows the current part's pending list above its composer.
		void present_queue(const core::tour_thread& aThread, std::vector<agent_pending_row> aRows, const QString& aState, bool aPaused);
		/// \brief What a part's pending list's controls ask of its queue.
		void               set_queue_dispatch(std::function<agent_pending_dispatch(const core::tour_thread&)> aDispatch);
		[[nodiscard]] auto pending_list() const noexcept -> agent_pending_list* { return PendingList; }
		/// \brief [[BPR-1995]], RFC-BPR-0100 AC02. Whether the open chat's
		/// update could be asked for now, and asks for it: what the link under
		/// an answer that changed nothing does.
		[[nodiscard]] auto can_update() const -> bool;
		auto               request_update() -> bool;
		/// \brief [[BPR-1995]], RFC-BPR-0100 AC05. Enhance: opens `aThread`'s
		/// chat and sends its update turn in that part's session; with
		/// `aPassage`, the update of that passage alone. Answers whether it
		/// was sent.
		auto enhance(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage) -> bool;
		/// \brief [[BPR-1995]], RFC-BPR-0100 AC04. Review each in the tour on
		/// a revision card: the tour shows that run's first waiting part.
		void set_review_dispatch(std::function<void(core::tour_agent_run_id)> aReview) { ReviewDispatch = std::move(aReview); }
		/// \brief The newest whole-tour revision card shown, and its actions
		/// while any part of it waits.
		[[nodiscard]] auto revision_card() const noexcept -> QWidget* { return RevisionCard; }
		[[nodiscard]] auto revision_operations() const noexcept -> QLabel* { return RevisionOperations; }
		[[nodiscard]] auto revision_state() const noexcept -> QLabel* { return RevisionState; }
		[[nodiscard]] auto revision_accept_all() const noexcept -> QPushButton* { return RevisionAcceptAll; }
		[[nodiscard]] auto revision_review() const noexcept -> QPushButton* { return RevisionReview; }
		[[nodiscard]] auto revision_discard() const noexcept -> QPushButton* { return RevisionDiscard; }
		[[nodiscard]] auto close_button() const noexcept -> QToolButton* { return ClosePane; }
		[[nodiscard]] auto copy_chat_button() const noexcept -> QToolButton* { return CopyChat; }
		[[nodiscard]] auto show_button() const noexcept -> QPushButton* { return ShowPart; }
		[[nodiscard]] auto subject() const noexcept -> QLabel* { return Subject; }
		/// \brief [[BPR-1999]]. The open chat's session cost line.
		[[nodiscard]] auto session_cost_line() const noexcept -> QLabel* { return SessionCostLine; }
		[[nodiscard]] auto passage_line() const noexcept -> QLabel* { return PassageLine; }
		[[nodiscard]] auto passage() const -> std::optional<core::tour_passage>;
		/// \brief How many turns the open chat shows.
		[[nodiscard]] auto turn_count() const noexcept -> std::size_t { return Bubbles.size(); }
		/// \brief Turn `aTurn`'s bubble, its reply (none when it has no
		/// answer), its ending line ([[BPR-1941]]: live while it runs) and Copy.
		[[nodiscard]] auto bubble(std::size_t aTurn) const -> QWidget*;
		[[nodiscard]] auto reply(std::size_t aTurn) const -> markdown_body_view*;
		[[nodiscard]] auto status_line(std::size_t aTurn) const -> agent_run_status_line*;
		[[nodiscard]] auto copy_reply_button(std::size_t aTurn) const -> QToolButton*;
		/// \brief [[BPR-1941]]. Resend and Delete on a turn that ended without
		/// an answer, when the core offers them; otherwise none.
		[[nodiscard]] auto resend_button(std::size_t aTurn) const -> QPushButton*;
		[[nodiscard]] auto delete_button(std::size_t aTurn) const -> QPushButton*;
		/// \brief [[BPR-1941]]. The whole tour's chat's line while Draft Tour
		/// runs; otherwise none.
		[[nodiscard]] auto draft_status_line() const noexcept -> agent_run_status_line* { return DraftLine; }
		/// \brief Proposal item `aIndex`'s Accept and rendered text, while it is
		/// pending and shown in the open chat.
		[[nodiscard]] auto accept_button(std::size_t aIndex) const -> QPushButton*;
		[[nodiscard]] auto proposal_reader(std::size_t aIndex) const -> markdown_body_view*;
		/// \brief The open chat as plain text that reads as Markdown: what Copy
		/// chat puts on the clipboard.
		[[nodiscard]] auto chat_text() const -> QString;
		/// \brief A reply as written, what its Copy puts on the clipboard.
		[[nodiscard]] static auto reply_text(const core::tour_agent_run& aRun) -> QString;

	protected:
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;
		/// [[BPR-1925]]. The seam with the tour: a line of the agent's accent
		/// down the column's left edge, and the faintest wash beside it.
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		std::function<void(const core::tour_thread&, std::optional<std::string>)> HistoryDispatch;
		session_edit_dispatch                                                     SessionEditDispatch;
		chat_history_menu*                                                        HistoryMenu     = nullptr;
		quiet_tool_button*                                                        HistoryTool     = nullptr;
		bool                                                                      SessionsOffered = true;
		QString                                                                   AgentName;
		bool                                                                      CarriedOver    = false;
		bool                                                                      CannotContinue = false;
		void                                                                      present_session_notice();
		QString                                                                   ShowWords;
		std::function<void(const core::tour_thread&)>                             ContinuationDispatch;
		QToolButton*                                                              ContinueOld            = nullptr;
		QLabel*                                                                   RevisionStatus         = nullptr;
		bool                                                                      RevisionChoiceRequired = false;
		/// The open session reads a revision other than the one under review,
		/// whether or not Continue old revision was chosen: History's New
		/// session opens one at the head under review, so core offers it even
		/// in a chat nothing was asked in.
		bool RevisionStale = false;
		/// What the next question starts from, and the line that says so.
		tour_carried_answer Carried        = tour_carried_answer::none;
		QLabel*             CarriedAnswer  = nullptr;
		bool                HistoryLoading = false;
		void                rebuild();
		void                changed();
		/// [[BPR-1914]] v004. The file inline code in a chat is coloured as:
		/// a stop's own file, or the language most of the tour's stops are in.
		[[nodiscard]] auto inline_selector() const -> std::string;
		/// \brief [[BPR-2351]]. The file a stop's chat is about, which its
		/// code is anchored to; empty for the whole tour's chat.
		[[nodiscard]] auto code_context() const -> std::string;
		void               refresh_header();
		void               fit_header();
		void               copy_chat() const;
		void               refresh_actions();
		void               show_passage();
		void               send(const QString& aQuestion, bool aUpdate, std::optional<core::tour_passage> aUpdatePassage = std::nullopt);
		/// [[BPR-1934]]. A suggested prompt chosen: sent, or put in the
		/// composer to edit or finish, as the core decides for the Agent tab.
		void               choose_prompt(const QString& aPrompt, bool aEditFirst);
		[[nodiscard]] auto prompts_usable() const -> bool;
		/// Why no suggested prompt can be pressed now, as core decides: the
		/// revision choice first, which the greyed prompts then say.
		[[nodiscard]] auto prompt_block() const -> core::agent_suggestion_block;
		/// [[BPR-1934]], RFC-BPR-0125. What the open chat offers now, as its
		/// source decided it.
		[[nodiscard]] auto prompt_offers() const -> core::chat_prompts;
		[[nodiscard]] auto prompt_part(const core::tour_thread& aThread) const -> core::prompt_tour_part;
		void               fit();
		[[nodiscard]] auto name_of(const core::tour_thread& aThread) const -> QString;
		/// [[BPR-1944]]. `aFramed`: the agent's reply, whose code blocks are
		/// boxes with Wrap and Copy; a proposal reads as its stop will.
		[[nodiscard]] auto reader(QWidget* aParent, const std::string& aBody, const std::string& aIdentity, const std::string& aInlineSelector, bool aFramed = false) -> markdown_body_view*;
		[[nodiscard]] auto index_of(const core::tour_thread& aThread) const -> int;
		/// [[BPR-1941]]. The running request as its line says it, and the
		/// agent that ran `aRun`, as the live status named it.
		[[nodiscard]] auto live_status() const -> agent_run_status;
		[[nodiscard]] auto agent_of(core::tour_agent_run_id aRun) const -> QString;

		QLabel*                                                                     SessionCostLine = nullptr;
		agent_reference_files_source                                                ReferenceFiles;
		agent_reference_navigation                                                  ReferenceNavigate;
		agent_reference_chooser                                                     ReferenceChoose;
		ask_dispatch                                                                AskDispatch;
		std::function<void()>                                                       StopDispatch;
		decide_dispatch                                                             DecideDispatch;
		std::function<void(core::tour_stop_id, const std::string&)>                 OwnAcceptDispatch;
		std::function<void(const core::tour_thread&)>                               RevealDispatch;
		std::function<void(bool)>                                                   VisibilityDispatch;
		std::function<void()>                                                       ChangeObserver;
		check_dispatch                                                              Checks;
		bool                                                                        CheckScratch   = false;
		bool                                                                        CheckBuilds    = false;
		bool                                                                        CheckAvailable = false;
		QString                                                                     CheckWhy;
		QPushButton*                                                                PlanCheck  = nullptr;
		bool                                                                        ColumnOpen = false;
		core::tour_agent_session                                                    Session;
		std::optional<core::tour_proposal>                                          Proposal;
		std::shared_ptr<const core::review_tour>                                    Draft;
		std::vector<core::tour_thread>                                              Open;
		std::map<std::size_t, core::tour_passage>                                   Passages;
		std::map<std::size_t, QString>                                              Edited;
		std::set<std::size_t>                                                       Editing;
		bool                                                                        Running       = false;
		bool                                                                        Drafting      = false;
		QWidget*                                                                    WaitingBubble = nullptr;
		QLabel*                                                                     RefusedLine   = nullptr;
		QLabel*                                                                     UnchangedLine = nullptr;
		QLabel*                                                                     FocusLine     = nullptr;
		std::function<void(core::tour_agent_run_id)>                                FocusUndoDispatch;
		[[nodiscard]] auto                                                          takes_feedback() const -> bool;
		bool                                                                        Mine          = false;
		bool                                                                        Ready         = true;
		bool                                                                        HostIdle      = true;
		QLabel*                                                                     SessionNotice = nullptr;
		QString                                                                     Why;
		std::size_t                                                                 Shown = 0U;
		QFont                                                                       CodeFont;
		qreal                                                                       TabStop = 0.0;
		std::optional<markdown_code_theme>                                          CodeTheme;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		int                                                                         Current            = -1;
		QToolButton*                                                                CopyChat           = nullptr;
		QToolButton*                                                                ClosePane          = nullptr;
		QLabel*                                                                     Subject            = nullptr;
		QLabel*                                                                     Quote              = nullptr;
		QPushButton*                                                                ShowPart           = nullptr;
		QScrollArea*                                                                Scroll             = nullptr;
		QVBoxLayout*                                                                Turns              = nullptr;
		QWidget*                                                                    Notice             = nullptr;
		QWidget*                                                                    Field              = nullptr;
		QWidget*                                                                    RevisionCard       = nullptr;
		QLabel*                                                                     RevisionOperations = nullptr;
		QLabel*                                                                     RevisionState      = nullptr;
		QPushButton*                                                                RevisionAcceptAll  = nullptr;
		QPushButton*                                                                RevisionReview     = nullptr;
		QPushButton*                                                                RevisionDiscard    = nullptr;
		std::function<void(core::tour_agent_run_id)>                                ReviewDispatch;
		/// The update turn's words for the open chat's part.
		[[nodiscard]] auto update_question() const -> QString;
		[[nodiscard]] auto part_words() const -> QString;
		/// A revision's parts in plain words, one per line.
		[[nodiscard]] auto  revision_words(core::tour_agent_run_id aRun) const -> QStringList;
		void                refresh_unchanged_line();
		QWidget*            PassageRow   = nullptr;
		QLabel*             PassageLine  = nullptr;
		QToolButton*        ClearPassage = nullptr;
		QPlainTextEdit*     Composer     = nullptr;
		QPushButton*        Send         = nullptr;
		QPushButton*        StopRun      = nullptr;
		agent_pending_list* PendingList  = nullptr;
		struct part_queue
		{
			std::vector<agent_pending_row> Rows;
			QString                        State;
			bool                           Paused = false;
		};
		std::map<std::string, part_queue>                               Queues;
		std::optional<std::string>                                      QueueShown;
		std::function<agent_pending_dispatch(const core::tour_thread&)> QueueDispatch;
		bool                                                            FollowUpSteer = false;
		bool                                                            Opposite      = false;
		/// Whether a message typed now is a follow-up, and which.
		[[nodiscard]] auto    follow_up() const -> std::optional<bool>;
		void                  show_queue();
		skill_completer*      SkillCompletion = nullptr;
		std::vector<QWidget*> Bubbles;
		/// [[BPR-2154]]. The author's steered messages, after their turn's question.
		std::vector<QWidget*>                                                 SteeredBubbles;
		std::vector<markdown_body_view*>                                      Replies;
		std::vector<agent_run_status_line*>                                   Endings;
		std::vector<agent_activity_view*>                                     ActivityViews;
		std::map<std::uint64_t, std::pair<core::agent_turn_activity, qint64>> RunActivity;
		QPointer<agent_activity_view>                                         LiveActivity;
		std::vector<QToolButton*>                                             Copies;
		std::vector<QPushButton*>                                             Resends;
		std::vector<QPushButton*>                                             Deletes;
		/// [[BPR-1941]]. The running request: what it does, its clock, the
		/// lines saying it, and the agent each turn ran with.
		std::optional<agent_run_status>              Live;
		QElapsedTimer                                LiveClock;
		QPointer<agent_run_status_line>              LiveLine;
		agent_run_status_line*                       DraftLine = nullptr;
		std::map<std::uint64_t, QString>             RunAgents;
		std::function<void(core::tour_agent_run_id)> DeleteDispatch;
		std::vector<QPointer<markdown_body_view>>    Readers;
		std::map<std::size_t, QPushButton*>          Accepts;
		std::map<std::size_t, markdown_body_view*>   ProposalReaders;
		std::vector<QPointer<QWidget>>               Accented;
		prompt_source                                PromptSource;
		/// The offers shown now, chips then More, and while a suggested prompt
		/// is sent as offered, the only text its skills are read from.
		std::vector<core::prompt_offer> Offered;
		std::optional<std::string>      SendingSkillText;
		/// [[BPR-2235]]. The prompt's name while it is sent as offered, the
		/// names this chat sent questions under, the source of names kept
		/// across restarts, and the prompt turns shown whole.
		QString                         SendingName;
		std::map<std::string, QString>  SentNames;
		turn_name_source                TurnNames;
		std::set<std::string>           PromptsShown;
		QPointer<agent_suggestion_list> Suggestions;
		/// [[BPR-1934]] v002. The selected text, the part it is in, and the
		/// label its prompt has now.
		struct offered_selection
		{
			core::tour_thread  Thread;
			core::tour_passage Passage;
		};
		std::optional<offered_selection> Selection;
	};
}

#endif
